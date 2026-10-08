import random
import struct
import unittest
from itemslot import Document, FormatError, DEFINITIONS, import_csv, export_csv, register_item, CUSTOM_IDS, save_definitions


class ExpansionTests(unittest.TestCase):
    def test_explicit_ids_all_columns(self):
        rng = random.Random(221)
        for count in range(12, 25):
            doc = Document.new(count, expanded=True)
            self.assertEqual(doc.player.ids, list(range(19)) + sorted(CUSTOM_IDS))
            for table in (doc.player, doc.cpu):
                for c in range(count):
                    for _ in range(200): table.rows[rng.randrange(len(table.ids))][c] += 1
            for _ in range(3):
                raw = doc.encode()
                reopened = Document.parse(raw)
                for role in ('player', 'cpu'):
                    self.assertEqual(getattr(doc, role), getattr(reopened, role))
                self.assertEqual(import_csv(export_csv(doc.player)), doc.player)
                doc = reopened

    def test_custom_only_safe_prefix(self):
        for id in sorted(CUSTOM_IDS):
            doc = Document.new(expanded=True)
            for table in (doc.player, doc.cpu): table.rows[table.ids.index(id)] = [200] * 24
            raw = doc.encode()
            reopened = Document.parse(raw)
            self.assertEqual(reopened.player, doc.player)
            self.assertTrue(all(t.sums() == [200] * 12 for t in reopened.legacy[:5]))
            self.assertEqual(raw[raw.index(b'WISL') + 5], 2)

    def test_sparse_reordered_directory(self):
        doc = Document.new(expanded=True)
        for table in (doc.player, doc.cpu):
            table.ids.reverse(); table.rows.reverse()
            table.rows[table.ids.index(22)] = [200] * 24
        self.assertEqual(Document.parse(doc.encode()).player, doc.player)

    def test_reorder_custom_item_updates_both_roles_without_moving_native_slots(self):
        doc = Document.new(expanded=True)
        native_slots = [i for i, item_id in enumerate(doc.player.ids) if item_id < 19]
        native_ids = [doc.player.ids[i] for i in native_slots]
        before = [i for i in doc.player.ids if i >= 21]
        doc.reorder_custom_item(29, 21, after=False)
        expected = [29] + [i for i in before if i != 29]
        self.assertEqual([i for i in doc.player.ids if i >= 21], expected)
        self.assertEqual(doc.player.ids, doc.cpu.ids)
        self.assertEqual([doc.player.ids[i] for i in native_slots], native_ids)

    def test_future_definition_and_reserved_ids(self):
        original = dict(DEFINITIONS); active=set(CUSTOM_IDS)
        try:
            doc = Document.new(expanded=True)
            doc.add_item(30, 'Future Test Item')
            for table in (doc.player, doc.cpu): table.rows[-1] = [200] * 24
            self.assertEqual(Document.parse(doc.encode()).cpu, doc.cpu)
            self.assertEqual(import_csv(export_csv(doc.player)), doc.player)
            for id in (19, 20, -1, 256):
                with self.assertRaises(FormatError): register_item(id, 'invalid')
            with self.assertRaises(FormatError): register_item(21, 'different Boo')
            with self.assertRaises(FormatError): register_item(27, 'Boo')
        finally:
            DEFINITIONS.clear(); DEFINITIONS.update(original); CUSTOM_IDS.clear(); CUSTOM_IDS.update(active)

    def test_bad_directory_and_payload(self):
        doc = Document.new(expanded=True)
        for table in (doc.player, doc.cpu): table.rows[-1] = [200] * 24
        raw = doc.encode(); start = raw.index(b'WISL')
        for id in (19, 20, 256, 0):
            bad = bytearray(raw)
            struct.pack_into('>H', bad, start + 16 + 19 * 2, id)
            with self.assertRaises(FormatError): Document.parse(bad)
        for size in range(start + 1, len(raw)):
            with self.assertRaises(FormatError): Document.parse(raw[:size])

    def test_vanilla_export_rejects_lossy_custom_rows(self):
        doc = Document.new(expanded=True)
        for table in (doc.player, doc.cpu): table.rows[-1] = [200] * 24
        with self.assertRaisesRegex(FormatError, 'custom'): doc.encode(vanilla=True)


if __name__ == '__main__': unittest.main()
