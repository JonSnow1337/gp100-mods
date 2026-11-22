# Build the Windows version of GP-100 Firmware Builder. Run in PowerShell, from the repository root.
# Needs: Python 3.10+ for Windows (python.org), and the bundled firmware in src\gp100mods\data\firmware\ (see README).
# NOTE: written without being able to run it on Windows - if a step fails, the commands are plain pip / PyInstaller calls.
$ErrorActionPreference = "Stop"
python -m venv .venv-win
.\.venv-win\Scripts\python -m pip install --upgrade pip
.\.venv-win\Scripts\pip install -e ".[app,dev]"
.\.venv-win\Scripts\pip install pyinstaller
$env:QT_QPA_PLATFORM = "offscreen"
.\.venv-win\Scripts\python -m pytest -q
.\.venv-win\Scripts\pyinstaller --noconfirm --windowed --name GP100FirmwareBuilder `
    --collect-all imageio_ffmpeg --add-data "src\gp100mods\data;gp100mods\data" --paths src packaging\entry.py
$env:GP100_REQUIRE_FIRMWARE = "1"
.\dist\GP100FirmwareBuilder\GP100FirmwareBuilder.exe --selftest
Write-Host "`nDone: dist\GP100FirmwareBuilder\GP100FirmwareBuilder.exe (zip the whole dist\GP100FirmwareBuilder folder to share it)"
