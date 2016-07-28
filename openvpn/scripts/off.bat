@echo off
cd /d "%~dp0"

:: Stop BROADcast
taskkill.exe /im "broadcast.exe" /f

:: Use BROADcast to revert network interface metric adjustments
..\..\broadcast.exe /i "$interface$" /a
