"""Build ItemSlot Studio for the current operating system using PyInstaller."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
subprocess.run(
    [sys.executable, '-m', 'PyInstaller', '--noconfirm', '--clean', str(ROOT / 'ItemSlotStudio.spec')],
    cwd=ROOT,
    check=True,
)

if sys.platform == 'win32':
    result = ROOT / 'dist' / 'ItemSlotStudio.exe'
elif sys.platform == 'darwin':
    result = ROOT / 'dist' / 'ItemSlotStudio.app'
else:
    result = ROOT / 'dist' / 'ItemSlotStudio'
print(f'Built: {result}')
