"""Custom entries are optional configuration, never renumbered engine IDs."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import tkinter as tk
import unittest
from unittest.mock import patch
import editor
from itemslot import CUSTOM_IDS, DEFINITIONS, Document, FormatError, save_definitions, import_csv, export_csv

class ConfigurationTests(unittest.TestCase):
    def setUp(self):
        self.names=dict(DEFINITIONS);self.active=set(CUSTOM_IDS)
    def tearDown(self):
        DEFINITIONS.clear();DEFINITIONS.update(self.names)
        CUSTOM_IDS.clear();CUSTOM_IDS.update(self.active)
    def filled(self, columns, item):
        doc=Document.new(columns,expanded=True)
        for table in (doc.player,doc.cpu): table.rows[table.ids.index(item)]=[200]*columns
        return doc
    def test_remove_readd_generic_ids_and_both_formats(self):
        # Test configured defaults and an arbitrary future entry via one mechanism.
        for removed, survivor in ((21,22),(22,21),(25,22)):
            for columns in (12,16,18,24):
                doc=self.filled(columns,survivor)
                doc.add_item(removed,DEFINITIONS.get(removed,'Future configuration item'))
                doc.remove_item(removed)
                for table in (doc.player,doc.cpu):
                    self.assertNotIn(removed,table.ids)
                    self.assertEqual(table.rows[table.ids.index(survivor)],[200]*columns)
                reopened=Document.parse(doc.encode())
                self.assertEqual(reopened.player,doc.player)
                self.assertNotIn(removed,Document.new(columns,expanded=True).player.ids)
                self.assertEqual(import_csv(export_csv(doc.player)),doc.player)
                if columns==12:
                    original=Document.parse(doc.encode(original=True))
                    self.assertNotIn(removed,original.player.ids)
                    self.assertEqual(original.player,doc.player)
                doc.add_item(removed,DEFINITIONS[removed])
                self.assertIn(removed,CUSTOM_IDS)
                self.assertEqual(doc.cpu.rows[doc.cpu.ids.index(removed)],[0]*columns)
    def test_removing_weight_requires_rebalance(self):
        doc=self.filled(24,21);doc.remove_item(21)
        with self.assertRaisesRegex(FormatError,'100%'):doc.encode()
        for table in (doc.player,doc.cpu):table.rows[table.ids.index(22)]=[200]*24
        self.assertEqual(Document.parse(doc.encode()).cpu,doc.cpu)
        with self.assertRaisesRegex(FormatError,'Native'):doc.remove_item(4)
    def test_existing_explicit_file_retains_disabled_nonzero_ids(self):
        raw=self.filled(24,21).encode()
        doc=Document.parse(raw);doc.remove_item(21)
        reopened=Document.parse(raw)
        self.assertIn(21,reopened.player.ids)
        self.assertEqual(reopened.player.rows[reopened.player.ids.index(21)],[200]*24)
        # Opening a file does not silently change new-file configuration.
        self.assertNotIn(21,CUSTOM_IDS)
    def test_enabled_flag_persists_and_names_are_retained(self):
        doc=self.filled(24,22);doc.remove_item(21)
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'items.json';save_definitions(path)
            entries={entry['id']:entry for entry in json.loads(path.read_text())}
            self.assertEqual(entries[21],{'id':21,'name':self.names[21],'enabled':False})
            self.assertTrue(entries[22]['enabled'])
            self.assertEqual([i for i in entries if i<19],list(range(19)))
            # Restart with an isolated registry; never overwrite repo metadata.
            codec=Path(__file__).with_name('itemslot.py')
            (Path(directory)/'itemslot.py').write_bytes(codec.read_bytes())
            subprocess.run([sys.executable,'-B','-c',
                'from itemslot import *; assert 21 not in Document.new(expanded=True).player.ids; '
                'assert 22 in Document.new(expanded=True).player.ids; '
                'd=Document.new(expanded=True); d.add_item(21,DEFINITIONS[21]); '
                'assert 21 in d.cpu.ids; save_definitions(); '
                'assert CUSTOM_IDS == {21,22,23,24,25,26,27,28,29}'],cwd=directory,check=True)
    def test_original_format_round_trip_and_reserved_holes(self):
        for item in (21,22,23):
            doc=self.filled(12,item)
            for _ in range(3):
                raw=doc.encode(original=True)
                self.assertNotIn(b'WISL',raw)
                self.assertEqual(raw[1:3],bytes((12,30)))
                self.assertFalse(any(raw[3+19*12:3+21*12]))
                doc=Document.parse(raw)
                self.assertEqual(doc.player.rows[doc.player.ids.index(item)],[200]*12)
            for slt in (True,False):self.assertEqual(Document.parse(doc.encode(original=True,slt=slt)).cpu,doc.cpu)
        with self.assertRaisesRegex(FormatError,'12 columns'):self.filled(24,21).encode(original=True)
    def test_actual_remove_button_does_not_renumber_or_reappear_on_open(self):
        root=tk.Tk();root.withdraw()
        try:
            app=editor.Editor(root);app.doc=self.filled(24,22);raw=app.doc.encode()
            app.refresh();app.grids['player'].tree.selection_set(str(app.doc.player.ids.index(21)))
            with tempfile.TemporaryDirectory() as folder, patch.object(editor,'DEFINITION_FILE',Path(folder)/'items.json'), patch.object(editor.messagebox,'askyesno',return_value=True):
                app.remove_item()
                self.assertNotIn(21,app.doc.cpu.ids)
                # Existing file contents stay intact even with custom defaults disabled.
                file=Path(folder)/'ItemSlot24.bin';file.write_bytes(raw)
                app.dirty=False
                with patch.object(editor.filedialog,'askopenfilename',return_value=str(file)):
                    app.open()
                self.assertIn(21,app.doc.player.ids)
                # A newly exported file with the custom ID removed stays removed.
                app.doc.remove_item(21);file.write_bytes(app.doc.encode());app.dirty=False
                with patch.object(editor.filedialog,'askopenfilename',return_value=str(file)):
                    app.open()
                self.assertNotIn(21,app.doc.player.ids)
                app.new();self.assertNotIn(21,app.doc.player.ids)
        finally:root.destroy()

if __name__=='__main__':unittest.main()
