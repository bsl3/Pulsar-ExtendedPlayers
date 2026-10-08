"""Exercise actual Tk registration UI without showing a window or editing metadata."""
import json, tempfile, tkinter as tk, unittest
from pathlib import Path
from unittest.mock import patch
import editor
from itemslot import DEFINITIONS, CUSTOM_IDS
class EditorRegistrationTests(unittest.TestCase):
 def test_add_item_button_registers_metadata_and_both_roles(self):
  root=tk.Tk();root.withdraw();snapshot=dict(DEFINITIONS);active=set(CUSTOM_IDS)
  try:
   app=editor.Editor(root)
   def widgets(parent):
    for widget in parent.winfo_children():
     yield widget
     yield from widgets(widget)
   button=next(w for w in widgets(root) if isinstance(w,editor.ttk.Button) and w.cget('text')=='＋')
   with tempfile.TemporaryDirectory() as folder, patch.object(editor,'DEFINITION_FILE',Path(folder)/'items.json'), patch.object(editor.simpledialog,'askinteger',return_value=30), patch.object(editor.simpledialog,'askstring',return_value='Future item'):
    button.invoke()
    for role in (app.doc.player,app.doc.cpu):
     self.assertIn(30,role.ids);self.assertEqual(role.rows[role.ids.index(30)],[0]*24)
    self.assertTrue(app.dirty)
    self.assertIn({'id':30,'name':'Future item','enabled':True},json.loads(editor.DEFINITION_FILE.read_text()))
  finally:
   DEFINITIONS.clear();DEFINITIONS.update(snapshot);CUSTOM_IDS.clear();CUSTOM_IDS.update(active);root.destroy()
if __name__=='__main__':unittest.main()
