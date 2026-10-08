import csv
import io
from pathlib import Path
import struct
import random
import unittest
from itemslot import Document, Table, FormatError, ITEMS, import_csv, export_csv, weight

def sample(columns=24):
    doc = Document.new(columns)
    for role,table in enumerate((doc.player,doc.cpu)):
        # Each rank has a distinctive authored selection, including ranks 13–24.
        for c in range(columns): table.rows[(c+role)%19][c] = 200
    return doc

class CodecTests(unittest.TestCase):
    def test_all_rosters(self):
        for count in range(12,25):
            doc = sample(count)
            encoded = doc.encode()
            loaded = Document.parse(encoded)
            self.assertEqual(loaded.player.rows,doc.player.rows)
            self.assertEqual(loaded.cpu.rows,doc.cpu.rows)
            self.assertEqual(loaded.encode(),encoded)
            for c in range(count): self.assertEqual(loaded.player.rows[c%19][c],200)
            self.assertEqual(Document.parse(doc.encode(slt=True)).cpu.rows,doc.cpu.rows)

    def test_vanilla_prefix_preserved(self):
        doc = sample(12)
        raw = doc.encode(vanilla=True)
        loaded = Document.parse(raw); loaded.resize(24)
        extended = loaded.encode()
        self.assertEqual(extended[:len(raw)],raw)
        self.assertEqual(len(raw),1811)
        self.assertEqual(extended[len(raw):len(raw)+4],b'WISL')

    def test_csv_counts_roundtrip(self):
        for count in range(12,25):
            table = sample(count).player
            self.assertEqual(import_csv(export_csv(table)).rows,table.rows)
        self.assertEqual(weight('32.5'),65)

    def test_csv_errors(self):
        original = export_csv(sample().player)
        for invalid in (original.replace('Green Shell','Unknown'),original.replace('2nd','1st'),
                        original.splitlines()[0]+'\n', original+'Green Shell'+',0'*24+'\n',
                        original.replace('100','nan',1), original.replace('100','99.9',1),
                        original.replace('100','99',1)):
            with self.assertRaises(FormatError): import_csv(invalid)

    def test_binary_errors(self):
        data = sample().encode()
        for bad in (b'',data[:15],data[:-1],data+b'junk',bytes([99])+data[1:]):
            with self.assertRaises(FormatError): Document.parse(bad)
        pos=data.index(b'WISL')
        for offset in (4,7,8,9,10,12,16,17):
            bad=bytearray(data);bad[pos+offset]^=1
            with self.assertRaises(FormatError): Document.parse(bytes(bad))

    def test_csv_initializes_empty_other_role(self):
        doc=Document.new()
        table=import_csv(export_csv(sample().player))
        self.assertTrue(doc.import_race_csv(table,'player'))
        self.assertEqual(doc.cpu.rows,table.rows)
        self.assertEqual(Document.parse(doc.encode()).player.rows,table.rows)
        doc.player.rows[0][0]=0
        self.assertEqual(doc.cpu.rows[0][0],200) # no aliasing

    def test_csv_preserves_configured_other_role(self):
        doc=sample()
        cpu=doc.cpu.clone()
        self.assertFalse(doc.import_race_csv(import_csv(export_csv(sample(16).player)),'player'))
        cpu.resize(16)
        self.assertEqual(doc.cpu.rows,cpu.rows)
        doc.encode()

    def test_validation_names_role(self):
        doc=sample();doc.cpu.rows=[[0]*24 for _ in ITEMS]
        with self.assertRaisesRegex(FormatError,'CPU|Cpu'):
            doc.encode()

    def test_empty_race_rejected(self):
        with self.assertRaises(FormatError): Document.new().encode()

    def test_repeated_csv_edit_export_roundtrip(self):
        rng = random.Random(24)
        doc = Document.new()
        for attempt in range(20):
            for role in ('player', 'cpu'):
                rows = [[0]*24 for _ in ITEMS]
                for column in range(24):
                    for _ in range(200): rows[rng.randrange(19)][column] += 1
                doc.import_race_csv(import_csv(export_csv(Table(rows))), role)
                donor = next(i for i in range(19) if getattr(doc, role).rows[i][attempt%24])
                getattr(doc, role).rows[donor][attempt%24] -= 1
                getattr(doc, role).rows[(donor+1)%19][attempt%24] += 1
            expected = (doc.player.clone(), doc.cpu.clone())
            for slt in (False, True):
                out = doc.encode(slt=slt); loaded = Document.parse(out)
                self.assertEqual(loaded.player.rows, expected[0].rows)
                self.assertEqual(loaded.cpu.rows, expected[1].rows)
                self.assertEqual(loaded.encode(slt=slt), out)
            doc = Document.parse(doc.encode())

    def test_race_loading_report_file(self):
        path = Path('C:/Users/kafia/Downloads/PulsarModdings/Release/WiiLoaded/MK WiiLoaded/Assets/CommonAssets.d/ItemSlot24.bin')
        if not path.exists(): self.skipTest('Reported binary unavailable')
        raw = path.read_bytes();doc = Document.parse(raw)
        self.assertEqual(doc.player.columns, 24)
        doc.player.validate();doc.cpu.validate()
        self.assertEqual(raw, doc.encode())
        self.assertEqual(doc.player.sums(), [200] * 24)
        self.assertEqual(doc.cpu.sums(), [200] * 24)
        # This is a user-editable file: zero probabilities are valid; verify
        # its current data rather than requiring an older fixture's choices.
        reopened = Document.parse(doc.encode())
        self.assertEqual(reopened.player.rows, doc.player.rows)
        self.assertEqual(reopened.cpu.rows, doc.cpu.rows)

    def test_supplied_binary(self):
        p=Path('C:/Users/kafia/Downloads/PulsarModdings/mkwii/CommonAssets/ItemSlot.bin')
        if not p.exists(): self.skipTest('Supplied binary unavailable')
        data=p.read_bytes();doc=Document.parse(data)
        self.assertEqual(doc.encode(),data) # no-op open/save preserves GP/VS differences
        self.assertEqual([t.columns for t in doc.legacy],[12]*5+[16]+[3]*6)
        self.assertEqual(doc.player.sums(),[200]*12)
        doc.resize(24);out=doc.encode()
        self.assertEqual(out[:len(data)],data)
        self.assertEqual(Document.parse(out).encode(),out)

if __name__=='__main__': unittest.main()
