@echo off
cd /d "%~dp0"

set TAP="C:\Program Files\TAP-Windows"
%TAP%\bin\devcon.exe install %TAP%\driver\OemWin2k.inf tap0901

pause
