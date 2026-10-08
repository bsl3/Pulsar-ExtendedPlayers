"""WiiLoaded ItemSlot Studio — polished standalone Python/ttk desktop editor."""
from pathlib import Path
import ctypes
import sys
import tkinter as tk
from tkinter import ttk, filedialog, messagebox, simpledialog

from itemslot import (
    DEFINITION_FILE,
    NATIVE_IDS,
    save_definitions,
    item_name,
    Document,
    FormatError,
    ordinal,
    percent,
    weight,
    import_csv,
    export_csv,
)

# UI palette. The data grid stays light for readability; the application chrome
# around it uses dark/navy blues.
BG = '#0b1f33'
PANEL = '#102a43'
HEADER = '#123a5a'
HEADER_HOVER = '#194b72'
ACCENT = '#2f6f9f'
ACCENT_HOVER = '#3a82b8'
TEXT = '#f4f8fb'
MUTED = '#b8c7d6'
TABLE_BG = '#f7fafc'
TABLE_ALT = '#eef4f8'
TABLE_FG = '#142638'
TABLE_HEADER = '#2d5879'
TABLE_HEADER_HOVER = '#376a90'
SELECTED = '#3d6f93'
TOTAL_BG = '#dcebf3'
INVALID_BG = '#fff0ef'
INVALID_FG = '#a51f25'
BORDER = '#264963'


def _hex_to_colorref(value):
    """Convert #RRGGBB to the COLORREF byte order Windows DWM expects."""
    value = value.lstrip('#')
    r, g, b = int(value[0:2], 16), int(value[2:4], 16), int(value[4:6], 16)
    return r | (g << 8) | (b << 16)


def tint_windows_titlebar(root):
    """Best-effort Windows 11 title-bar tint; harmlessly no-ops elsewhere."""
    if sys.platform != 'win32':
        return
    try:
        root.update_idletasks()
        hwnd = ctypes.windll.user32.GetParent(root.winfo_id()) or root.winfo_id()
        dwm = ctypes.windll.dwmapi.DwmSetWindowAttribute
        dark = ctypes.c_int(1)
        caption = ctypes.c_int(_hex_to_colorref(HEADER))
        text = ctypes.c_int(_hex_to_colorref(TEXT))
        # Windows 10/11 dark title bar; Windows 11 caption/text color when available.
        dwm(hwnd, 20, ctypes.byref(dark), ctypes.sizeof(dark))
        dwm(hwnd, 35, ctypes.byref(caption), ctypes.sizeof(caption))
        dwm(hwnd, 36, ctypes.byref(text), ctypes.sizeof(text))
    except Exception:
        pass


class Grid(ttk.Frame):
    """Editable probability table with inline custom-item actions."""

    DRAG_THRESHOLD = 5

    def __init__(self, parent, app, kind):
        super().__init__(parent, style='App.TFrame')
        self.app = app
        self.kind = kind
        self.race_grid = kind in ('player', 'cpu')
        self.entry = None
        self.commit_pending = lambda: True
        self._drag_source = None
        self._drag_target = None
        self._drag_start_y = 0
        self._drag_moved = False

        shell = ttk.Frame(self, style='TableShell.TFrame', padding=1)
        shell.grid(row=0, column=0, sticky='nsew')
        shell.rowconfigure(0, weight=1)
        shell.columnconfigure(0, weight=1)

        self.tree = ttk.Treeview(shell, show='headings', height=20, selectmode='browse')
        self.tree.tag_configure('native', background=TABLE_BG, foreground=TABLE_FG)
        self.tree.tag_configure('custom', background=TABLE_ALT, foreground=TABLE_FG)
        self.tree.tag_configure('total', background=TOTAL_BG, foreground=TABLE_FG)
        self.tree.tag_configure('invalid', foreground=INVALID_FG, background=INVALID_BG)
        self.tree.grid(row=0, column=0, sticky='nsew')

        y = ttk.Scrollbar(shell, command=self.tree.yview, style='App.Vertical.TScrollbar')
        x = ttk.Scrollbar(shell, orient='horizontal', command=self.tree.xview, style='App.Horizontal.TScrollbar')
        self.tree.configure(yscrollcommand=y.set, xscrollcommand=x.set)
        y.grid(row=0, column=1, sticky='ns')
        x.grid(row=1, column=0, sticky='ew')

        self.rowconfigure(0, weight=1)
        self.columnconfigure(0, weight=1)

        if self.race_grid:
            footer = ttk.Frame(self, style='App.TFrame', padding=(0, 8, 0, 0))
            footer.grid(row=1, column=0, sticky='ew')
            ttk.Button(
                footer,
                text='＋',
                width=3,
                command=self.app.add_item,
                style='Accent.TButton',
            ).pack(side='left')
            ttk.Label(
                footer,
                text='Add custom item   •   Drag custom rows to reorder   •   Click − to remove',
                style='Muted.TLabel',
            ).pack(side='left', padx=(9, 0))

        self.tree.bind('<Double-1>', self.edit)
        self.tree.bind('<Return>', self.edit_selected)
        self.tree.bind('<ButtonPress-1>', self._on_press, add='+')
        self.tree.bind('<B1-Motion>', self._on_drag, add='+')
        self.tree.bind('<ButtonRelease-1>', self._on_release, add='+')
        self.tree.bind('<Motion>', self._on_motion, add='+')
        self.tree.bind('<Leave>', lambda _e: self.tree.configure(cursor=''))

    def table(self):
        return getattr(self.app.doc, self.kind)

    def _id_for_row(self, row):
        if not row or row == 'totals':
            return None
        try:
            index = int(row)
            return self.table().ids[index]
        except (ValueError, IndexError):
            return None

    def _is_custom_row(self, row):
        item_id = self._id_for_row(row)
        return item_id is not None and item_id not in NATIVE_IDS

    @property
    def position_column_start(self):
        # Treeview identifies display columns as #1, #2, ...
        # Race: #1 action, #2 item, #3 first position.
        # Special: #1 item, #2 first set.
        return 3 if self.race_grid else 2

    def refresh(self):
        if self.entry:
            self.entry.destroy()
            self.entry = None

        table = self.table()
        position_columns = [f'position_{c}' for c in range(table.columns)]
        columns = (['action', 'item'] if self.race_grid else ['item']) + position_columns
        self.tree.configure(columns=columns)

        if self.race_grid:
            self.tree.heading('action', text='')
            self.tree.column('action', width=34, minwidth=34, stretch=False, anchor='center')
        self.tree.heading('item', text='Item')
        self.tree.column('item', width=190, minwidth=160, stretch=False, anchor='w')

        for c, column in enumerate(position_columns):
            self.tree.heading(column, text=f'Set {c + 1}' if self.kind == 'special' else ordinal(c + 1))
            self.tree.column(column, width=66, minwidth=58, stretch=False, anchor='center')

        self.tree.delete(*self.tree.get_children())
        for i, (item_id, row) in enumerate(zip(table.ids, table.rows)):
            name = f'{item_name(item_id)} [{item_id}]'
            if self.race_grid:
                values = ['−' if item_id not in NATIVE_IDS else '', name] + [percent(v) for v in row]
            else:
                values = [name] + [percent(v) for v in row]
            tag = 'custom' if item_id not in NATIVE_IDS else 'native'
            self.tree.insert('', 'end', iid=str(i), values=values, tags=(tag,))
        self.totals()

    def totals(self):
        sums = self.table().sums()
        if self.tree.exists('totals'):
            self.tree.delete('totals')
        invalid = any(n != 200 and not (self.kind == 'special' and n == 0) for n in sums)
        prefix = ['', 'Total (%)'] if self.race_grid else ['Total (%)']
        self.tree.insert(
            '',
            'end',
            iid='totals',
            values=prefix + [percent(n) for n in sums],
            tags=('invalid' if invalid else 'total',),
        )
        self.app.status()

    def edit_selected(self, _event=None):
        selection = self.tree.selection()
        if selection:
            self.open_entry(selection[0], f'#{self.position_column_start}')

    def edit(self, event):
        self.open_entry(self.tree.identify_row(event.y), self.tree.identify_column(event.x))

    def open_entry(self, row, column):
        if not row or row == 'totals' or not column:
            return
        display_index = int(column[1:])
        if display_index < self.position_column_start:
            return
        if self.entry:
            self.entry.destroy()
        box = self.tree.bbox(row, column)
        if not box:
            return
        c = display_index - self.position_column_start
        try:
            row_index = int(row)
        except ValueError:
            return

        entry = self.entry = ttk.Entry(self.tree, justify='center', style='Cell.TEntry')
        entry.insert(0, percent(self.table().rows[row_index][c]))
        entry.place(x=box[0], y=box[1], width=box[2], height=box[3])
        entry.select_range(0, 'end')
        entry.focus_set()

        def cancel(_=None):
            if self.entry == entry:
                self.entry = None
                entry.destroy()

        def commit(_=None):
            if self.entry != entry:
                return True
            try:
                value = weight(entry.get())
            except FormatError as exc:
                self.app.note.set(str(exc))
                entry.focus_set()
                return False
            self.table().rows[row_index][c] = value
            current = list(self.tree.item(row, 'values'))
            current[self.position_column_start - 1 + c] = percent(value)
            self.tree.item(row, values=current)
            cancel()
            self.app.dirty = True
            self.totals()
            return True

        self.commit_pending = commit
        entry.bind('<Return>', commit)
        entry.bind('<FocusOut>', commit)
        entry.bind('<Escape>', cancel)

    def _on_motion(self, event):
        if not self.race_grid:
            return
        row = self.tree.identify_row(event.y)
        column = self.tree.identify_column(event.x)
        if column == '#1' and self._is_custom_row(row):
            self.tree.configure(cursor='hand2')
        elif self._is_custom_row(row):
            self.tree.configure(cursor='fleur')
        else:
            self.tree.configure(cursor='')

    def _on_press(self, event):
        if not self.race_grid:
            return
        row = self.tree.identify_row(event.y)
        column = self.tree.identify_column(event.x)
        self._drag_source = None
        self._drag_target = None
        self._drag_moved = False
        if column == '#1':
            return
        if self._is_custom_row(row):
            self._drag_source = self._id_for_row(row)
            self._drag_start_y = event.y

    def _on_drag(self, event):
        if self._drag_source is None:
            return
        if abs(event.y - self._drag_start_y) < self.DRAG_THRESHOLD:
            return
        self._drag_moved = True
        row = self.tree.identify_row(event.y)
        self._drag_target = self._id_for_row(row) if self._is_custom_row(row) else None
        self.tree.configure(cursor='fleur')

    def _on_release(self, event):
        if not self.race_grid:
            return
        row = self.tree.identify_row(event.y)
        column = self.tree.identify_column(event.x)

        # The small minus cell is an inline remove control for custom rows.
        if not self._drag_moved and column == '#1' and self._is_custom_row(row):
            item_id = self._id_for_row(row)
            self._clear_drag()
            self.app.remove_item(item_id)
            return

        if self._drag_moved and self._drag_source is not None and self._drag_target is not None:
            if self._drag_source != self._drag_target:
                target_row = self.tree.identify_row(event.y)
                box = self.tree.bbox(target_row)
                after = bool(box and event.y > box[1] + box[3] / 2)
                self.app.reorder_custom_item(self._drag_source, self._drag_target, after=after)
        self._clear_drag()

    def _clear_drag(self):
        self._drag_source = None
        self._drag_target = None
        self._drag_moved = False
        self.tree.configure(cursor='')


class Editor:
    def __init__(self, root):
        self.root = root
        self.doc = Document.new(expanded=True)
        self.path = None
        self.dirty = False

        root.title('ItemSlot Studio')
        root.geometry('1380x780')
        root.minsize(900, 620)
        root.configure(bg=BG)
        root.option_add('*tearOff', False)

        self._configure_styles()
        self._build_menu()

        self.note = tk.StringVar()
        self.count = tk.StringVar(value='24')

        header = ttk.Frame(root, style='Header.TFrame', padding=(14, 10))
        header.pack(fill='x')

        title_box = ttk.Frame(header, style='Header.TFrame')
        title_box.pack(side='left')
        ttk.Label(title_box, text='ItemSlot Studio', style='HeaderTitle.TLabel').pack(anchor='w')
        ttk.Label(title_box, text='WiiLoaded item probability editor', style='HeaderSub.TLabel').pack(anchor='w')

        controls = ttk.Frame(header, style='Header.TFrame')
        controls.pack(side='right')
        ttk.Button(controls, text='Import CSV…', command=self.csv_in, style='Header.TButton').pack(side='left', padx=(0, 6))
        ttk.Button(controls, text='Export CSV…', command=self.csv_out, style='Header.TButton').pack(side='left', padx=(0, 14))
        ttk.Label(controls, text='Race positions', style='Header.TLabel').pack(side='left', padx=(0, 6))
        counter = ttk.Combobox(
            controls,
            textvariable=self.count,
            values=list(range(12, 25)),
            state='readonly',
            width=4,
            style='Header.TCombobox',
        )
        counter.pack(side='left')
        counter.bind('<<ComboboxSelected>>', self.resize)

        ttk.Label(
            root,
            text='Probabilities (%)  •  Double-click a value to edit  •  0.5% steps  •  Each race column must total 100%',
            style='Help.TLabel',
            padding=(14, 8),
        ).pack(fill='x')

        self.tabs = ttk.Notebook(root, style='App.TNotebook')
        self.tabs.pack(fill='both', expand=True, padx=12, pady=(0, 8))
        self.grids = {}
        for key, label in [('player', 'Race — Player'), ('cpu', 'Race — CPU'), ('special', 'Special Item Boxes')]:
            grid = Grid(self.tabs, self, key)
            self.grids[key] = grid
            self.tabs.add(grid, text=label)

        self.status_label = ttk.Label(
            root,
            textvariable=self.note,
            style='Status.TLabel',
            padding=(14, 9),
            anchor='w',
        )
        self.status_label.pack(fill='x')

        root.protocol('WM_DELETE_WINDOW', self.close)
        self._bind_shortcuts()
        self.refresh()
        root.after(20, lambda: tint_windows_titlebar(root))

    def _configure_styles(self):
        style = ttk.Style(self.root)
        if 'clam' in style.theme_names():
            style.theme_use('clam')

        default_font = ('Segoe UI', 10)
        style.configure('App.TFrame', background=BG)
        style.configure('Header.TFrame', background=HEADER)
        style.configure('TableShell.TFrame', background=BORDER)
        style.configure('TLabel', font=default_font)

        style.configure('HeaderTitle.TLabel', background=HEADER, foreground=TEXT, font=('Segoe UI', 13, 'bold'))
        style.configure('HeaderSub.TLabel', background=HEADER, foreground=MUTED, font=('Segoe UI', 9))
        style.configure('Header.TLabel', background=HEADER, foreground=TEXT, font=default_font)
        style.configure('Help.TLabel', background=PANEL, foreground=MUTED, font=('Segoe UI', 9))
        style.configure('Muted.TLabel', background=BG, foreground=MUTED, font=('Segoe UI', 9))
        style.configure('Status.TLabel', background=PANEL, foreground=TEXT, font=('Segoe UI', 9))
        style.configure('StatusError.TLabel', background=PANEL, foreground='#ffb6b6', font=('Segoe UI', 9))

        style.configure(
            'Header.TButton',
            background=HEADER_HOVER,
            foreground=TEXT,
            borderwidth=0,
            focusthickness=1,
            focuscolor=ACCENT_HOVER,
            padding=(12, 7),
            font=default_font,
        )
        style.map(
            'Header.TButton',
            background=[('pressed', ACCENT), ('active', ACCENT_HOVER)],
            foreground=[('disabled', '#7f93a5')],
        )
        style.configure(
            'Accent.TButton',
            background=ACCENT,
            foreground=TEXT,
            borderwidth=0,
            padding=(8, 5),
            font=('Segoe UI', 11, 'bold'),
        )
        style.map('Accent.TButton', background=[('pressed', HEADER_HOVER), ('active', ACCENT_HOVER)])

        style.configure(
            'Header.TCombobox',
            fieldbackground=TEXT,
            background=HEADER_HOVER,
            foreground=TABLE_FG,
            arrowcolor=TABLE_FG,
            bordercolor=BORDER,
            lightcolor=BORDER,
            darkcolor=BORDER,
        )
        style.map('Header.TCombobox', fieldbackground=[('readonly', TEXT)], foreground=[('readonly', TABLE_FG)])

        style.configure(
            'Treeview',
            background=TABLE_BG,
            fieldbackground=TABLE_BG,
            foreground=TABLE_FG,
            borderwidth=0,
            rowheight=29,
            font=default_font,
        )
        style.map('Treeview', background=[('selected', SELECTED)], foreground=[('selected', TEXT)])
        style.configure(
            'Treeview.Heading',
            background=TABLE_HEADER,
            foreground=TEXT,
            relief='flat',
            borderwidth=1,
            font=('Segoe UI', 10, 'bold'),
            padding=(7, 7),
        )
        style.map('Treeview.Heading', background=[('active', TABLE_HEADER_HOVER)])
        style.configure('Cell.TEntry', fieldbackground='#ffffff', foreground=TABLE_FG, bordercolor=ACCENT)

        style.configure('App.TNotebook', background=BG, borderwidth=0, tabmargins=(0, 0, 0, 0))
        style.configure('App.TNotebook.Tab', background=PANEL, foreground=MUTED, padding=(14, 8), borderwidth=0)
        style.map(
            'App.TNotebook.Tab',
            background=[('selected', TABLE_HEADER), ('active', HEADER_HOVER)],
            foreground=[('selected', TEXT), ('active', TEXT)],
        )

        style.configure('App.Vertical.TScrollbar', background=PANEL, troughcolor=BG, bordercolor=BG, arrowcolor=TEXT)
        style.configure('App.Horizontal.TScrollbar', background=PANEL, troughcolor=BG, bordercolor=BG, arrowcolor=TEXT)

    def _build_menu(self):
        menu = tk.Menu(self.root)
        file_menu = tk.Menu(menu)
        file_menu.add_command(label='New', accelerator='Ctrl+N', command=self.new)
        file_menu.add_separator()
        file_menu.add_command(label='Import BIN…', accelerator='Ctrl+O', command=self.open)
        file_menu.add_separator()
        file_menu.add_command(label='Save BIN', accelerator='Ctrl+S', command=self.save_bin)
        file_menu.add_command(label='Save BIN As…', accelerator='Ctrl+Shift+S', command=lambda: self.save_bin(save_as=True))
        file_menu.add_command(label='Save SLT…', command=self.save_slt)
        file_menu.add_separator()
        file_menu.add_command(label='Exit', command=self.close)
        menu.add_cascade(label='File', menu=file_menu)
        self.root.configure(menu=menu)

    def _bind_shortcuts(self):
        self.root.bind_all('<Control-n>', lambda _e: self.new())
        self.root.bind_all('<Control-o>', lambda _e: self.open())
        self.root.bind_all('<Control-s>', lambda _e: self.save_bin())
        self.root.bind_all('<Control-Shift-S>', lambda _e: self.save_bin(save_as=True))

    def refresh(self):
        self.count.set(str(self.doc.player.columns))
        for grid in self.grids.values():
            grid.refresh()
        self.status()

    def status(self):
        problems = []
        for key in ('player', 'cpu', 'special'):
            for c, total in enumerate(getattr(self.doc, key).sums(), 1):
                if total != 200 and not (key == 'special' and total == 0):
                    problems.append(f'{key} {ordinal(c)}: {total / 2:g}%')
        if problems:
            message = 'Invalid totals — ' + ', '.join(problems[:6]) + (' …' if len(problems) > 6 else '')
            if hasattr(self, 'status_label'):
                self.status_label.configure(style='StatusError.TLabel')
        else:
            message = 'All configured columns total 100%. Empty special sets are disabled; do not reference them from KMP.'
            if hasattr(self, 'status_label'):
                self.status_label.configure(style='Status.TLabel')
        self.note.set(message)
        self.root.title('ItemSlot Studio — ' + (str(self.path) if self.path else 'New table') + (' *' if self.dirty else ''))

    def replace_ok(self):
        return not self.dirty or messagebox.askyesno(
            'Discard changes?',
            'Discard unsaved edits?',
            parent=self.root,
        )

    def new(self):
        if self.replace_ok():
            self.doc = Document.new(int(self.count.get()), expanded=True)
            self.path = None
            self.dirty = False
            self.refresh()

    def open(self):
        if not self.replace_ok():
            return
        path = filedialog.askopenfilename(
            title='Import ItemSlot BIN',
            filetypes=[('ItemSlot BIN', '*.bin'), ('ItemSlot SLT', '*.slt'), ('All files', '*')],
        )
        if not path:
            return
        try:
            doc = Document.parse(Path(path).read_bytes())
        except (OSError, FormatError) as exc:
            messagebox.showerror('Cannot open', str(exc), parent=self.root)
            return
        self.doc, self.path, self.dirty = doc, Path(path), False
        self.refresh()
        if doc.warnings:
            messagebox.showinfo('Import notes', '\n'.join(doc.warnings), parent=self.root)

    def add_item(self):
        if not self.commit_edits():
            return
        item_id = simpledialog.askinteger(
            'Add custom item',
            'Engine ID (must match the C++ registry):',
            parent=self.root,
            minvalue=0,
            maxvalue=255,
        )
        if item_id is None:
            return
        name = simpledialog.askstring('Add custom item', 'Item name:', parent=self.root)
        if name is None:
            return
        try:
            self.doc.add_item(item_id, name)
            save_definitions(DEFINITION_FILE)
        except (OSError, FormatError) as exc:
            messagebox.showerror('Cannot add item', str(exc), parent=self.root)
            return
        self.dirty = True
        self.refresh()
        self.note.set('Custom item added. Matching C++ behavior is still required for the game to support this engine ID.')

    def remove_item(self, item_id=None):
        if not self.commit_edits():
            return
        if item_id is None:
            grid = self.selected_grid()
            selection = grid.tree.selection()
            if grid.kind == 'special' or not selection or selection[0] == 'totals':
                messagebox.showinfo(
                    'Remove custom item',
                    'Select a custom item in Player or CPU first.',
                    parent=self.root,
                )
                return
            item_id = grid._id_for_row(selection[0])
        if item_id is None or item_id in NATIVE_IDS:
            messagebox.showinfo('Remove custom item', 'Native items cannot be removed.', parent=self.root)
            return

        name = item_name(item_id)
        if not messagebox.askyesno(
            'Remove custom item?',
            f'Are you sure you want to remove {name} [{item_id}]?\n\n'
            'It will be removed from both Player and CPU tables.',
            parent=self.root,
            icon='warning',
        ):
            return
        try:
            self.doc.remove_item(item_id)
            save_definitions(DEFINITION_FILE)
        except (OSError, FormatError) as exc:
            messagebox.showerror('Cannot remove item', str(exc), parent=self.root)
            return
        self.dirty = True
        self.refresh()
        self.note.set('Custom item removed. Rebalance any affected columns before saving.')

    def reorder_custom_item(self, source_id, target_id, *, after=False):
        if not self.commit_edits():
            return
        try:
            self.doc.reorder_custom_item(source_id, target_id, after=after)
        except FormatError as exc:
            messagebox.showerror('Cannot reorder item', str(exc), parent=self.root)
            return
        self.dirty = True
        self.refresh()
        self.note.set('Custom item order updated in both Player and CPU tables.')

    def resize(self, _=None):
        if not self.commit_edits():
            self.count.set(str(self.doc.player.columns))
            return
        self.doc.resize(int(self.count.get()))
        self.dirty = True
        self.refresh()
        self.note.set('Sampled the existing curve across the new count. Check your balance before saving.')

    def _encoded(self, *, slt=False):
        if not self.commit_edits():
            return None
        try:
            return self.doc.encode(slt=slt)
        except FormatError as exc:
            messagebox.showerror('Validation', str(exc), parent=self.root)
            return None

    def _write(self, path, data):
        try:
            Path(path).write_bytes(data)
        except OSError as exc:
            messagebox.showerror('Cannot save', str(exc), parent=self.root)
            return False
        self.path = Path(path)
        self.dirty = False
        self.status()
        return True

    def save_bin(self, save_as=False):
        data = self._encoded(slt=False)
        if data is None:
            return False
        path = None if save_as else self.path
        if path is None or Path(path).suffix.lower() != '.bin':
            path = filedialog.asksaveasfilename(
                title='Save ItemSlot BIN',
                initialfile='ItemSlot24.bin',
                defaultextension='.bin',
                filetypes=[('ItemSlot BIN', '*.bin')],
            )
        if not path:
            return False
        return self._write(path, data)

    def save_slt(self):
        data = self._encoded(slt=True)
        if data is None:
            return False
        initial = self.path.with_suffix('.slt').name if self.path else 'ItemSlotTable.slt'
        path = filedialog.asksaveasfilename(
            title='Save ItemSlot SLT',
            initialfile=initial,
            defaultextension='.slt',
            filetypes=[('ItemSlot SLT', '*.slt')],
        )
        if not path:
            return False
        return self._write(path, data)

    # Backward-compatible helper for callers that used the previous GUI method.
    def save(self, slt=False, original=False):
        if original:
            raise FormatError('The old Export 1–12 BIN UI path has been removed.')
        return self.save_slt() if slt else self.save_bin(save_as=True)

    def selected_grid(self):
        return self.grids[list(self.grids)[self.tabs.index('current')]]

    def commit_edits(self):
        return all(grid.commit_pending() for grid in self.grids.values())

    def csv_in(self):
        if not self.commit_edits():
            return
        grid = self.selected_grid()
        if grid.kind == 'special':
            messagebox.showinfo(
                'Race CSV',
                'Select Player or CPU. CSV position headers describe race placement, not KMP special-set IDs.',
                parent=self.root,
            )
            return
        path = filedialog.askopenfilename(title='Import race CSV', filetypes=[('CSV', '*.csv')])
        if not path:
            return
        try:
            table = import_csv(Path(path).read_text(encoding='utf-8-sig'))
        except (OSError, FormatError) as exc:
            messagebox.showerror('Cannot import CSV', str(exc), parent=self.root)
            return
        initialized = self.doc.import_race_csv(table, grid.kind)
        self.dirty = True
        self.refresh()
        if initialized:
            self.note.set('Imported CSV and initialized the previously empty companion race table.')

    def csv_out(self):
        if not self.commit_edits():
            return
        grid = self.selected_grid()
        if grid.kind == 'special':
            messagebox.showinfo('Race CSV', 'CSV export is available on Player and CPU tabs.', parent=self.root)
            return
        path = filedialog.asksaveasfilename(
            title='Export race CSV',
            initialfile=grid.kind + '.csv',
            defaultextension='.csv',
            filetypes=[('CSV', '*.csv')],
        )
        if path:
            try:
                Path(path).write_text(export_csv(grid.table()), encoding='utf-8', newline='')
            except OSError as exc:
                messagebox.showerror('Cannot export CSV', str(exc), parent=self.root)

    def close(self):
        if not self.commit_edits():
            return
        if self.replace_ok():
            self.root.destroy()


if __name__ == '__main__':
    root = tk.Tk()
    Editor(root)
    root.mainloop()
