@echo off
cd /d "%~dp0"

:: Variables
set CONFIG=server.ovpn
set LOCAL=192.168.179.100
set SERVER=vpn.server.host
set PORT=1194
set INTERFACE=VPN

:: Modify variables in the OpenVPN config
copy /y "%CD%\config\%CONFIG%.var" "%USERPROFILE%\OpenVPN\config\%CONFIG%"

wscript adjust.js "%USERPROFILE%\OpenVPN\config\%CONFIG%" "$local$" "%LOCAL%"
wscript adjust.js "%USERPROFILE%\OpenVPN\config\%CONFIG%" "$server$" "%SERVER%"
wscript adjust.js "%USERPROFILE%\OpenVPN\config\%CONFIG%" "$port$" "%PORT%"
wscript adjust.js "%USERPROFILE%\OpenVPN\config\%CONFIG%" "$path$" "%CD:\=\\%"
wscript adjust.js "%USERPROFILE%\OpenVPN\config\%CONFIG%" "$interface$" "%INTERFACE%"

:: Modify variables in the OpenVPN scripts
copy /y "%CD%\scripts\on.bat.var" "%CD%\scripts\on.bat"
wscript adjust.js "%CD%\scripts\on.bat" "$interface$" "%INTERFACE%"

copy /y "%CD%\scripts\off.bat.var" "%CD%\scripts\off.bat"
wscript adjust.js "%CD%\scripts\off.bat" "$interface$" "%INTERFACE%"

:: Start VPN with the specified config
openvpn-gui.exe --connect "%CONFIG%"

:: Remove junk and logs
rmdir /s /q scripts\%%SystemDrive%%
rmdir /s /q "%USERPROFILE%\OpenVPN\log"
