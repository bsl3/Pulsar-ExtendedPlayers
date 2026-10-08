# ItemSlot Studio

Source-only copy of the redesigned editor. The Windows executable will be supplied separately through GitHub Releases. Generated build/dist files and the automatic executable-build workflow are not included. Nothing here is compiled into Code.pul.

Standalone ExtendedPlayers now loads `CommonAssets.szs/ItemSlot24.bin` for 13�24 racers. Export 24 positions and use native items only; remove custom rows or give them zero weight. The editor can describe custom items, but this standalone game module does not implement them.

ItemSlot Studio is a standalone desktop editor for WiiLoaded Mario Kart Wii ItemSlot tables. It is written in **Python 3 + Tkinter/ttk** and has no third-party runtime dependencies.

## UI

The editor uses a dark-blue desktop layout with a high-contrast probability table.

- **File → Import BIN…** opens ItemSlot BIN files (the dialog can also open SLT files).
- **File → Save BIN** saves back to the current BIN when one is open; otherwise it behaves like Save As.
- **File → Save BIN As…** always asks for a new BIN destination.
- **File → Save SLT…** writes the six-table SLT form.
- **Import CSV… / Export CSV…** stay in the main toolbar and operate on the selected Player or CPU tab.
- Use the **＋** button below a race table to add a custom item.
- Every custom item has an inline **−** control. Removing an item always asks for confirmation first.
- **Drag custom rows** to reorder them. Native IDs 0–18 cannot be removed or reordered. Reordering updates both Player and CPU data and is stored in the explicit WISL item directory when the BIN/SLT is saved.
- Double-click a probability cell to edit it. Values use 0.5% steps and race columns must total 100%.

The old **Export 1–12 BIN** UI has been removed. Normal BIN output is now handled through **Save BIN** / **Save BIN As…**.

## Supported data

- Native six-table SLT and twelve-table BIN prefixes.
- WISL v1/v2 extended race data from 12–24 positions.
- Native items 0–18 plus registered custom engine IDs.
- CSV import/export for Player and CPU race tables.
- Special item-box editing remains native-only.

`items.json` contains the default item registry. In a source checkout it is edited in place. In packaged builds, user changes are saved to a writable per-user configuration location:

- Windows: `%APPDATA%\ItemSlotStudio\items.json`
- macOS: `~/Library/Application Support/ItemSlotStudio/items.json`
- Linux: `$XDG_CONFIG_HOME/ItemSlotStudio/items.json` or `~/.config/ItemSlotStudio/items.json`

Adding metadata does **not** implement an item in-game. The engine ID still needs matching WiiLoaded C++ behavior.

## Run from source

On Windows, double-click **OpenItemSlotStudio.bat**. It opens the editor directly without building an executable. Python 3.10+ with Tkinter must be installed; no extra packages are needed.

Requirements:

- Python 3.10 or newer
- Tk/Tkinter (included with normal Windows/macOS Python installs; on Debian/Ubuntu install `python3-tk` if needed)

Run:

```bash
python ItemSlotStudio.pyw
```

or:

```bash
python editor.py
```

## Tests

Codec tests:

```bash
python -m unittest test_itemslot test_expansion test_configuration -v
```

`test_editor.py` exercises the Tk UI. On Linux without a desktop session, run GUI tests through Xvfb:

```bash
xvfb-run -a python -m unittest test_editor test_configuration -v
```

## Build a distributable

PyInstaller must build on the **same operating system as the target**. A Linux machine cannot directly produce a native Windows `.exe` or macOS `.app` with PyInstaller.

Install the build dependency:

```bash
python -m pip install -r requirements-build.txt
```

Then build for the current OS:

```bash
python scripts/build.py
```

Output:

| Platform | Output |
| --- | --- |
| Windows | `dist/ItemSlotStudio.exe` |
| macOS | `dist/ItemSlotStudio.app` |
| Linux | `dist/ItemSlotStudio` |

### Windows

Use a Windows Python environment and run the build command above. The PyInstaller spec creates a windowed `.exe` with `items.json` bundled inside it.

### macOS

Run the build on macOS. The result is `ItemSlotStudio.app`. The generated app is **unsigned**; public distribution should use an Apple Developer ID for signing/notarization.

### Linux

Install Tk first if necessary:

```bash
sudo apt install python3-tk
```

Then run the build command. The output is a standalone Linux executable.

## Automatic builds with GitHub Actions

`.github/workflows/build.yml` builds all three platforms on their native GitHub-hosted runners. It runs automatically for version tags such as `v1.0.0`, or manually from **Actions → Build desktop apps → Run workflow**.

The workflow produces three downloadable artifacts:

- `ItemSlotStudio-Windows`
- `ItemSlotStudio-macOS`
- `ItemSlotStudio-Linux`

This is the easiest way to create all three distributables from one GitHub repository without maintaining three local machines.

## Project layout

```text
ItemSlotStudio.pyw       App launcher
editor.py               Tkinter UI
itemslot.py              BIN/SLT/WISL codec and data model
items.json               Default item registry
ItemSlotStudio.spec      PyInstaller packaging configuration
requirements-build.txt   Build-only dependency
scripts/build.py         Cross-platform build entry point
.github/workflows/       Native Windows/macOS/Linux CI builds
test_*.py                Regression tests
```

## Notes

The editor validates encoded output by reopening the exact bytes before writing them. Custom item ordering uses explicit engine IDs, so moving one custom row does not renumber another engine ID.
