@echo off
cd /d "%~dp0"

set TAP="C:\Program Files\TAP-Windows"
%TAP%\bin\devcon.exe remove tap0901

pause
