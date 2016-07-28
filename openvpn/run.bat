@echo off
cd /d "%~dp0"

:: Variables
set CONFIG="server.ovpn"
set LOCAL=192.168.179.100
set SERVER=vpn.server.dns.address.or.ip.address
set PORT=1194
set INTERFACE="VPN"

:: Backup settings of installed VPN (if there is)
reg import "openvpn.reg"
reg export "HKLM\SOFTWARE\OpenVPN" "openvpn.reg" /y
reg import "openvpn-gui.reg"
reg export "HKLM\SOFTWARE\OpenVPN-GUI" "openvpn-gui.reg" /y

reg delete "HKLM\SOFTWARE\OpenVPN" /f
reg delete "HKLM\SOFTWARE\OpenVPN-GUI" /f

:: Import portable settings
reg import settings\openvpn.reg
reg import settings\openvpn-gui.reg

:: Set up portable paths
reg add "HKLM\SOFTWARE\OpenVPN" /ve /t REG_SZ /d "%CD%" /f
reg add "HKLM\SOFTWARE\OpenVPN" /v config_dir /t REG_SZ /d "%CD%\config" /f
reg add "HKLM\SOFTWARE\OpenVPN" /v exe_path /t REG_SZ /d "%CD%\bin\openvpn.exe" /f
reg add "HKLM\SOFTWARE\OpenVPN" /v log_dir /t REG_SZ /d "%CD%\log" /f

reg add "HKLM\SOFTWARE\OpenVPN-GUI" /v config_dir /t REG_SZ /d "%CD%\config" /f
reg add "HKLM\SOFTWARE\OpenVPN-GUI" /v exe_path /t REG_SZ /d "%CD%\bin\openvpn.exe" /f
reg add "HKLM\SOFTWARE\OpenVPN-GUI" /v log_dir /t REG_SZ /d "%CD%\log" /f

:: Modify variables in OpenVPN config
copy /y "%CD%\config\%CONFIG%.bak" "%CD%\config\%CONFIG%"
copy /y "%CD%\config\%CONFIG%" "%CD%\config\%CONFIG%.bak"

wscript adjust.js "%CD%\config\%CONFIG%" "$local$" "%LOCAL%"
wscript adjust.js "%CD%\config\%CONFIG%" "$server$" "%SERVER%"
wscript adjust.js "%CD%\config\%CONFIG%" "$port$" "%PORT%"
wscript adjust.js "%CD%\config\%CONFIG%" "$path$" "%CD:\=\\%"
wscript adjust.js "%CD%\config\%CONFIG%" "$interface$" %INTERFACE%

:: Modify variables in OpenVPN scripts
copy /y "%CD%\scripts\on.bat.bak" "%CD%\scripts\on.bat"
copy /y "%CD%\scripts\on.bat" "%CD%\scripts\on.bat.bak"
copy /y "%CD%\scripts\off.bat.bak" "%CD%\scripts\off.bat"
copy /y "%CD%\scripts\off.bat" "%CD%\scripts\off.bat.bak"

wscript adjust.js "%CD%\scripts\on.bat" "$interface$" %INTERFACE%
wscript adjust.js "%CD%\scripts\off.bat" "$interface$" %INTERFACE%

:: Start VPN with specified config
cd bin
openvpn-gui.exe --connect %CONFIG%
cd ..

:: Restore OpenVPN scripts
copy /y "%CD%\scripts\on.bat.bak" "%CD%\scripts\on.bat"
del /q /f "%CD%\scripts\on.bat.bak"

copy /y "%CD%\scripts\off.bat.bak" "%CD%\scripts\off.bat"
del /q /f "%CD%\scripts\off.bat.bak"

:: Restore OpenVPN config
copy /y "%CD%\config\%CONFIG%.bak" "%CD%\config\%CONFIG%"
del /q /f "%CD%\config\%CONFIG%.bak"

:: Restore OpenVPN settings
reg delete "HKLM\SOFTWARE\OpenVPN" /f
reg delete "HKLM\SOFTWARE\OpenVPN-GUI" /f

reg import "openvpn.reg"
del /q /f "openvpn.reg"

reg import "openvpn-gui.reg"
del /q /f "openvpn-gui.reg"

:: Remove the log
rd /s /q log
