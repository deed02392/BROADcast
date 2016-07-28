@echo off
cd /d "%~dp0"

:: Use BROADcast to make $interface$ a preferred route (lowest metric)
..\..\broadcast.exe /i "$interface$" /m

:: Start BROADcast
wscript.exe "%~dp0on.js"
