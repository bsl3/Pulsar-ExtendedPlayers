"""Standalone MKW ItemSlot codec: native prefix plus WISL v1/v2 trailer.

No game, GUI or third-party dependencies. Values are integer half-percent weights.
The native game accepts arbitrary nonzero totals; editor exports require 100%.
"""
from dataclasses import dataclass, field
from decimal import Decimal, InvalidOperation
import csv
import io
import re
import struct
import json
import os
import sys
from pathlib import Path

# Metadata is extensible; every new ID still needs a matching runtime behavior.
# Source checkouts keep items.json next to this module. Packaged applications
# store user edits in a writable per-user config directory instead of trying to
# modify the application bundle / PyInstaller extraction directory.
BUNDLED_DEFINITION_FILE = Path(__file__).with_name('items.json')

def _default_definition_file():
    if not getattr(sys, 'frozen', False):
        return BUNDLED_DEFINITION_FILE
    if sys.platform == 'win32':
        base = Path(os.environ.get('APPDATA', Path.home() / 'AppData' / 'Roaming'))
    elif sys.platform == 'darwin':
        base = Path.home() / 'Library' / 'Application Support'
    else:
        base = Path(os.environ.get('XDG_CONFIG_HOME', Path.home() / '.config'))
    return base / 'ItemSlotStudio' / 'items.json'

DEFINITION_FILE = _default_definition_file()
DEFINITIONS = {}
CUSTOM_IDS = set() # enabled defaults; metadata retains disabled names/IDs
def register_item(id, name):
    if type(id) is not int or not 0 <= id <= 255 or id in (19,20):
        raise FormatError('Item ID must be 0–255, excluding reserved IDs 19/20.')
    if not isinstance(name,str) or not name.strip(): raise FormatError('Item needs a name.')
    if id in DEFINITIONS and DEFINITIONS[id] != name: raise FormatError('ID is already registered.')
    if any(n.casefold()==name.casefold() and i!=id for i,n in DEFINITIONS.items()):
        raise FormatError('Item name is already registered.')
    DEFINITIONS[id] = name
def item_name(id): return DEFINITIONS.get(id, f'Item ID {id}')
NATIVE_IDS = list(range(19))
HEADER = struct.Struct('>4sHBBBBHI')
MAGIC = b'WISL'

class FormatError(ValueError):
    pass
_definition_source = DEFINITION_FILE if DEFINITION_FILE.exists() else BUNDLED_DEFINITION_FILE
for definition in json.loads(_definition_source.read_text(encoding='utf-8')):
    register_item(definition['id'],definition['name'])
    if definition['id'] not in NATIVE_IDS and definition.get('enabled', True):
        CUSTOM_IDS.add(definition['id'])
if any(i not in DEFINITIONS for i in NATIVE_IDS): raise FormatError('Retain native IDs 0–18.')
ITEMS = tuple(item_name(i) for i in NATIVE_IDS) # vanilla CSV compatibility API

def save_definitions(path=DEFINITION_FILE):
    entries = [{'id':i, 'name':n, **({'enabled':i in CUSTOM_IDS} if i not in NATIVE_IDS else {})}
               for i,n in sorted(DEFINITIONS.items())]
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(entries,indent=2)+'\n',encoding='utf-8')

def configure_custom(id, enabled):
    if id in NATIVE_IDS or id not in DEFINITIONS:
        raise FormatError('Only registered custom items can be enabled/removed.')
    if enabled: CUSTOM_IDS.add(id)
    else: CUSTOM_IDS.discard(id)


def ordinal(n):
    return str(n) + ('th' if 10 <= n % 100 <= 20 else {1:'st', 2:'nd', 3:'rd'}.get(n % 10, 'th'))

def percent(weight):
    return str(Decimal(weight) / 2).rstrip('0').rstrip('.') if weight % 2 else str(weight // 2)

def weight(value):
    try:
        n = Decimal(str(value).strip())
    except InvalidOperation as exc:
        raise FormatError(f'Not a number: {value!r}') from exc
    if not n.is_finite() or n < 0 or n > 100 or n * 2 != (n * 2).to_integral_value():
        raise FormatError('Use 0–100%, in steps of 0.5%.')
    return int(n * 2)

@dataclass
class Table:
    rows: list[list[int]]
    ids: list[int] = field(default_factory=lambda: NATIVE_IDS[:])

    @property
    def columns(self):
        return len(self.rows[0]) if self.rows else 0

    def clone(self):
        return Table([r[:] for r in self.rows],self.ids[:])

    def sums(self):
        return [sum(r[c] for r in self.rows) for c in range(self.columns)]

    def validate(self, *, allow_empty=False):
        if len(self.rows) != len(self.ids) or not 1 <= len(self.ids) <= 254 or len(set(self.ids)) != len(self.ids) or any(type(i) is not int or not 0 <= i <= 255 or i in (19,20) for i in self.ids) or not 1 <= self.columns <= 24 or any(len(r) != self.columns for r in self.rows):
            raise FormatError('Expected unique non-reserved item IDs with equal column counts (1–24).')
        if any(type(v) is not int or v < 0 or v > 255 for r in self.rows for v in r):
            raise FormatError('Binary weights must be bytes (0–255).')
        invalid = [(i+1, total/2) for i,total in enumerate(self.sums()) if total != 200 and not (allow_empty and total == 0)]
        if invalid:
            raise FormatError('Columns must total 100%: ' + ', '.join(f'{ordinal(i)}={p:g}%' for i,p in invalid))

    def resize(self, columns):
        if not 1 <= columns <= 24:
            raise FormatError('Column count must be 1–24.')
        old = self.columns
        # Explicit authoring operation: sample the original curve across the new
        # field. Every column is subsequently independent; runtime never remaps.
        indices = [round(i * (old-1) / (columns-1)) if columns > 1 else 0 for i in range(columns)]
        self.rows = [[r[i] for i in indices] for r in self.rows]

    def encode(self):
        if len(self.rows) != len(self.ids) or not 1 <= len(self.ids) <= 254 or len(set(self.ids)) != len(self.ids) or any(type(i) is not int or not 0 <= i <= 255 or i in (19,20) for i in self.ids) or not 1 <= self.columns <= 24 or any(len(r) != self.columns for r in self.rows):
            raise FormatError('Malformed table dimensions.')
        try:
            return bytes((self.columns,len(self.rows))) + bytes(v for r in self.rows for v in r)
        except (ValueError, TypeError) as exc:
            raise FormatError('Weights must be byte integers.') from exc

def parse_table(data,offset,ids=None):
    if offset + 2 > len(data):
        raise FormatError('Truncated table header.')
    cols, rows = data[offset:offset+2]
    end = offset + 2 + cols * rows
    if not 1 <= cols <= 24 or rows != len(NATIVE_IDS if ids is None else ids) or end > len(data):
        raise FormatError(f'Invalid/truncated table at 0x{offset:x}.')
    cells = data[offset+2:end]
    return Table([list(cells[r*cols:(r+1)*cols]) for r in range(rows)],NATIVE_IDS[:] if ids is None else ids[:]),end

@dataclass
class Document:
    legacy: list[Table]
    player: Table
    cpu: Table
    warnings: list[str] = field(default_factory=list)

    @classmethod
    def parse(cls, data):
        if not data or data[0] not in (6, 12):
            raise FormatError('Expected 6-table SLT or 12-table BIN.')
        tables = []
        offset = 1
        for _ in range(data[0]):
            if offset+2 > len(data): raise FormatError('Truncated native header.')
            rows=data[offset+1]
            if rows < 19: raise FormatError('Native tables need IDs 0–18.')
            # Original expansion format: physical row index is the engine ID.
            if rows == 19:
                table, offset = parse_table(data, offset)
            else:
                cols=data[offset];end=offset+2+cols*rows
                if not 1<=cols<=24 or end>len(data): raise FormatError('Invalid native expansion table.')
                cells=data[offset+2:end]
                if any(cells[i*cols:(i+1)*cols].strip(b'\0') for i in (19,20) if i<rows):
                    raise FormatError('Nonzero reserved item row.')
                ids=NATIVE_IDS+[i for i in range(21,rows) if i in CUSTOM_IDS or any(cells[i*cols:(i+1)*cols])]
                table=Table([list(cells[i*cols:(i+1)*cols]) for i in ids],ids)
                offset=end
            tables.append(table)
        player, cpu = tables[2].clone(), tables[3].clone()
        warnings = []
        if tables[0].rows != player.rows or tables[1].rows != cpu.rows:
            warnings.append('GP and VS differ: VS initializes the unified race editor; the original prefix is retained.')
        for i,t in enumerate(tables):
            if i < 5 and t.columns != 12:
                warnings.append(f'Table {i} has {t.columns} native columns; exports normalize the prefix to 12.')
        if offset != len(data):
            if len(data)-offset < HEADER.size:
                raise FormatError('Truncated extension/trailing bytes.')
            magic, version, cols, rows, count, reserved, flags, size = HEADER.unpack_from(data, offset)
            offset += HEADER.size
            if (magic,count,reserved,flags) != (MAGIC,2,0,0) or version not in (1,2) or not 12 <= cols <= 24 or (version==1 and (rows!=19 or cols<13)):
                raise FormatError('Unknown or malformed WISL extension.')
            directory_size = rows*2 if version==2 else 0
            if not rows or rows>254 or size!=directory_size+2*(2+rows*cols) or offset+size!=len(data):
                raise FormatError('Invalid extension size.')
            ids = NATIVE_IDS[:]
            if version==2:
                ids=list(struct.unpack_from('>'+str(rows)+'H',data,offset));offset+=directory_size
                if len(set(ids))!=len(ids) or any(i>255 or i in (19,20) for i in ids):
                    raise FormatError('Duplicate/reserved/unsupported item IDs.')
            player,offset=parse_table(data,offset,ids)
            cpu,offset=parse_table(data,offset,ids)
            if player.columns != cols or cpu.columns != cols:
                raise FormatError('Extension table dimensions disagree.')
            player.validate(); cpu.validate()
        if player.columns != cpu.columns:
            raise FormatError('Player and CPU columns disagree.')
        # WISL compatibility prefix remains native-only; custom Player/CPU data
        # is retained separately, including when importing an original expansion.
        for index,t in enumerate(tables):
            if index >= 5 and any(any(row) for i,row in zip(t.ids,t.rows) if i not in NATIVE_IDS):
                raise FormatError('Custom special/battle rows are not editable by this race editor.')
        tables=[Table([dict(zip(t.ids,t.rows))[i][:] for i in NATIVE_IDS]) for t in tables]
        return cls(tables, player, cpu, warnings)

    @classmethod
    def new(cls,columns=24,*,expanded=False):
        def empty(c): return Table([[0]*c for _ in ITEMS])
        legacy = [empty(12) for _ in range(5)] + [empty(16)] + [empty(3) for _ in range(6)]
        # New documents intentionally have invalid totals until authored.
        ids=NATIVE_IDS+sorted(CUSTOM_IDS) if expanded else NATIVE_IDS[:]
        return cls(legacy,Table([[0]*columns for _ in ids],ids[:]),Table([[0]*columns for _ in ids],ids[:]))

    def add_item(self,id,name):
        register_item(id,name)
        if id not in NATIVE_IDS: configure_custom(id,True)
        for table in (self.player,self.cpu):
            if id not in table.ids: table.ids.append(id);table.rows.append([0]*table.columns)

    def remove_item(self,id):
        if id in NATIVE_IDS: raise FormatError('Native items cannot be removed.')
        # Explicit directories preserve engine IDs; remaining IDs never shift.
        if id in DEFINITIONS: configure_custom(id,False)
        for table in (self.player,self.cpu):
            if id in table.ids:
                row=table.ids.index(id); del table.ids[row]; del table.rows[row]

    def reorder_custom_item(self, source_id, target_id, *, after=False):
        if source_id in NATIVE_IDS or target_id in NATIVE_IDS:
            raise FormatError('Only custom items can be reordered.')
        if source_id == target_id:
            return
        for table in (self.player, self.cpu):
            custom_slots = [i for i, item_id in enumerate(table.ids) if item_id not in NATIVE_IDS]
            custom = [(table.ids[i], table.rows[i]) for i in custom_slots]
            custom_ids = [item_id for item_id, _ in custom]
            if source_id not in custom_ids or target_id not in custom_ids:
                raise FormatError('Custom item is not present in both race tables.')
            source_index = custom_ids.index(source_id)
            target_index = custom_ids.index(target_id)
            moving = custom.pop(source_index)
            if source_index < target_index:
                target_index -= 1
            if after:
                target_index += 1
            custom.insert(target_index, moving)
            for slot, (item_id, row) in zip(custom_slots, custom):
                table.ids[slot] = item_id
                table.rows[slot] = row

    @property
    def special(self): return self.legacy[5]

    def resize(self, columns):
        if not 12 <= columns <= 24:
            raise FormatError('Race tables support 12–24 positions.')
        self.player.resize(columns); self.cpu.resize(columns)

    def import_race_csv(self, table, role):
        if role not in ('player', 'cpu'):
            raise FormatError('Choose Player or CPU for a race CSV.')
        table.validate()
        other = 'cpu' if role == 'player' else 'player'
        initialize_other = not any(any(row) for row in getattr(self, other).rows)
        self.resize(table.columns)
        old_other=getattr(self,other)
        by_id=dict(zip(old_other.ids,old_other.rows))
        setattr(self,other,Table([by_id.get(i,[0]*table.columns) for i in table.ids],table.ids[:]))
        setattr(self,role,table.clone())
        if initialize_other:
            setattr(self, other, table.clone())
        return initialize_other

    def encode(self, *, slt=False, vanilla=False, original=False):
        if len(self.legacy) not in (6, 12):
            raise FormatError('Expected 6 or 12 native prefix tables.')
        for role in ('player', 'cpu', 'special'):
            try: getattr(self, role).validate(allow_empty=role=='special')
            except FormatError as exc: raise FormatError(f'{role.title()} table: {exc}') from exc
        if self.player.columns != self.cpu.columns or self.player.ids != self.cpu.ids or not 12 <= self.player.columns <= 24:
            raise FormatError('Player/CPU tables need the same 12–24 columns.')
        if original:
            if vanilla or self.player.columns!=12: raise FormatError('Original expansion export needs 12 columns.')
            base=[t.clone() for t in self.legacy]
            for index,table in ((0,self.player),(1,self.cpu),(2,self.player),(3,self.cpu),(4,self.player)):
                base[index]=table.clone()
            base=base[:6] if slt else base
            highest=max(i for t in base for i in t.ids)
            if highest>=255: raise FormatError('Original row-count byte cannot represent ID255; use WISL.')
            count=highest+1
            output=bytes([len(base)])
            for t in base:
                by_id=dict(zip(t.ids,t.rows))
                output+=bytes((t.columns,count))+bytes(v for i in range(count) for v in by_id.get(i,[0]*t.columns))
            reopened=type(self).parse(output)
            for role in ('player','cpu'):
                expected=dict(zip(getattr(self,role).ids,getattr(self,role).rows))
                actual=dict(zip(getattr(reopened,role).ids,getattr(reopened,role).rows))
                if any(actual.get(i)!=row for i,row in expected.items()) or any(any(row) for i,row in actual.items() if i not in expected):
                    raise FormatError('Original expansion export changed item-ID probabilities.')
            return output
        if self.special.ids!=NATIVE_IDS or any(t.ids!=NATIVE_IDS for t in self.legacy):
            raise FormatError('Native prefix/special tables must retain IDs0–18.')
        if vanilla and self.player.ids!=NATIVE_IDS: raise FormatError('Vanilla export cannot represent custom items.')
        base=[t.clone() for t in self.legacy]
        def native_bridge(table):
            by_id=dict(zip(table.ids,table.rows))
            rows=[by_id.get(i,[0]*table.columns)[:12] for i in NATIVE_IDS]
            # Bridge only: the trailer replaces these before the real lottery.
            # A vanilla prefix cannot represent custom IDs; aggregate their
            # weight into a safe native item without changing authored rows.
            for id,row in by_id.items():
                if id not in NATIVE_IDS:
                    for c in range(12): rows[4][c]+=row[c]
            return rows
        # Preserve the imported 12-player/battle balance. Editing the extended
        # tables cannot accidentally change vanilla races; explicit vanilla export
        # instead writes the edited first 12 columns to both GP and VS.
        edited = self.player.rows != self.legacy[2].rows or self.cpu.rows != self.legacy[3].rows
        if vanilla or (self.player.columns == 12 and edited):
            for index,table in ((0,self.player),(1,self.cpu),(2,self.player),(3,self.cpu),(4,self.player)):
                base[index]=Table(native_bridge(table))
        for index,t in enumerate(base[:5]):
            if t.columns != 12: t.resize(12)
            # Imported non-200 weights remain valid in the native prefix, but
            # race tables must not have empty columns (native RNG requires >0).
            if any(n == 0 for n in t.sums()):
                # For a new document, use the authored first twelve columns.
                role = self.cpu if index in (1,3) else self.player
                t.rows=native_bridge(role)
        if base[5].columns != 16 or any(t.columns != 3 for t in base[6:]):
            raise FormatError('Special boxes need 16 columns; battle tables need 3.')
        # Six-table files may use either suffix. Do not invent battle balance
        # when converting an imported SLT to the dedicated race-only asset.
        base = base[:6] if slt else base
        output = bytes([len(base)]) + b''.join(t.encode() for t in base)
        if not vanilla and (self.player.columns>12 or self.player.ids!=NATIVE_IDS):
            version=1 if self.player.ids==NATIVE_IDS else 2
            directory=b'' if version==1 else struct.pack('>'+str(len(self.player.ids))+'H',*self.player.ids)
            payload=directory+self.player.encode()+self.cpu.encode()
            output+=HEADER.pack(MAGIC,version,self.player.columns,len(self.player.ids),2,0,0,len(payload))+payload
        # Validate the exact bytes before any caller writes them. Reopening must
        # preserve every authored cell, not merely each column's total. Native
        # exports intentionally contain only the first twelve edited columns.
        reopened = type(self).parse(output)
        columns = 12 if vanilla else self.player.columns
        for role in ('player', 'cpu'):
            expected = [row[:columns] for row in getattr(self, role).rows]
            if getattr(reopened,role).rows!=expected or getattr(reopened,role).ids!=getattr(self,role).ids:
                raise FormatError(f'{role.title()} probabilities changed during export round-trip.')
        if reopened.special.rows != self.special.rows:
            raise FormatError('Special probabilities changed during export round-trip.')
        return output

def _item_key(name):
    return re.sub('[^a-z0-9]', '', name.casefold())

def import_csv(text):
    reader = csv.reader(io.StringIO(text.lstrip('\ufeff')))
    try: header = next(reader)
    except StopIteration: raise FormatError('Empty CSV.')
    if not header or header[0].strip().casefold() != 'item':
        raise FormatError('First column must be Item.')
    count = len(header)-1
    if not 12 <= count <= 24 or [h.strip().casefold() for h in header[1:]] != [ordinal(i).casefold() for i in range(1,count+1)]:
        raise FormatError('Use sequential 1st, 2nd, … position headers (12–24 columns).')
    known={_item_key(name):i for i,name in DEFINITIONS.items()}
    known.update({_item_key('Triple Mushrooms'):5, _item_key('Bob omb'):6, _item_key('Spiny Shell'):7})
    values = {}
    for line, cells in enumerate(reader, 2):
        if not cells or all(not c.strip() for c in cells): continue
        if len(cells) != len(header): raise FormatError(f'CSV line {line}: wrong column count.')
        key = _item_key(cells[0])
        if key not in known: raise FormatError(f'CSV line {line}: unknown item {cells[0]!r}.')
        index = known[key]
        if index in values: raise FormatError(f'Duplicate item: {item_name(index)}.')
        try: values[index] = [weight(v) for v in cells[1:]]
        except FormatError as exc: raise FormatError(f'CSV line {line}: {exc}') from exc
    missing = [ITEMS[i] for i in range(19) if i not in values]
    if missing: raise FormatError('Missing items: ' + ', '.join(missing))
    ids=sorted(values)
    table=Table([values[i] for i in ids],ids)
    table.validate()
    return table

def export_csv(table):
    out = io.StringIO(newline='')
    writer = csv.writer(out)
    writer.writerow(['Item']+[ordinal(c) for c in range(1,table.columns+1)])
    for id,row in zip(table.ids,table.rows): writer.writerow([item_name(id)]+[percent(v) for v in row])
    return out.getvalue()

if __name__ == '__main__':
    import argparse
    from pathlib import Path
    p = argparse.ArgumentParser(description='Inspect, validate or extend a native MKW ItemSlot file.')
    p.add_argument('file', type=Path)
    p.add_argument('--extend', type=int, choices=range(13,25))
    p.add_argument('--output', type=Path)
    args = p.parse_args()
    doc = Document.parse(args.file.read_bytes())
    if args.extend: doc.resize(args.extend)
    print(f'{len(doc.legacy)} native tables; {doc.player.columns} race columns')
    for w in doc.warnings: print('Note:', w)
    doc.player.validate(); doc.cpu.validate()
    if args.output:
        args.output.write_bytes(doc.encode(slt=args.output.suffix.lower()=='.slt'))
        print('Wrote', args.output)
