@echo off
setlocal
cd /d "%~dp0"
where py >nul 2>&1
if not errorlevel 1 (
    py -3 -c "import sys, tkinter; assert sys.version_info >= (3, 10)" >nul 2>&1
    if not errorlevel 1 (
        py -3 ItemSlotStudio.pyw
        if errorlevel 1 pause
        exit /b
    )
)
where python >nul 2>&1
if not errorlevel 1 (
    python -c "import sys, tkinter; assert sys.version_info >= (3, 10)" >nul 2>&1
    if not errorlevel 1 (
        python ItemSlotStudio.pyw
        if errorlevel 1 pause
        exit /b
    )
)
echo ItemSlot Studio needs Python 3.10 or newer with Tkinter.
echo Install Python from https://www.python.org/downloads/windows/
echo Enable the Python launcher or Add Python to PATH, then try again.
pause
exit /b 1
