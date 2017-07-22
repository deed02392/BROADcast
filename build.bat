@echo off
cd /d "%~dp0"

:: Build the executable
gcc.exe -O2 -mconsole -municode broadcast.c -o BROADcast.exe -lws2_32 -lIphlpapi
