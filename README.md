# BROADcast

(Go to [releases](https://github.com/garnetius/BROADcast/releases) page to download
the latest Windows build.)

**BROADcast** is a tiny Windows program which forces global IPv4 UDP broadcast
packets on all network interfaces (Ethernet & Wi-Fi).

*Administrator privileges are required to run this utility.
See the **OpenVPN** section below.*

This allows proper functionality of applications which expect UDP broadcast
packets to be sent (and received) across all network interfaces available
in the system (not just the “default” one) and thus to be available
on all networks the system is connected to.

Lots of software developed to work in a LAN environment depends on this feature,
such as older chat applications, multiplayer video games, and other
collaborative software.

For example, using **BROADcast** makes it possible to use
[Vypress Chat](http://www.vypress.com/lan_chat/) (LAN chat application)
over encrypted VPN (such as [OpenVPN](https://openvpn.net/))
or just play some [Warcraft III](http://us.blizzard.com/en-us/games/war3/)
(which advertises its game session over LAN using UDP broadcast packets),
allowing the game session to be discovered by all participants.
(Remember to use the “TAP” device.)

Windows normally sends broadcast packets only on default network interface,
which is determined based on its metric in the routing table, which normally
is a lowest metric value assigned by the system. **BROADcast** allows to both
change the default network interface by modifying its metric and relay
broadcast packets to all other supported network interfaces as well.

## Examples

  * `BROADcast.exe /i "Interface" /m`:
    Set the metric of all network interfaces except “Interface” to manual.
    The manual metric value is computed for each interface separately as
    current metric value + “Interface” metric. This turns the “Interface”
    into interface with the lowest metric and makes it the preferred route.

  * `BROADcast.exe /i "Interface"`:
    Opposite of the previous command: switch all network interfaces back
    to the automatic metric assigned by the operating system.

  * `BROADcast.exe /b`:
    Start the actual UDP broadcast relaying. The broadcast packets will be
    relayed to all network interfaces (except the preferred route).
    Use “Ctrl+C” to stop the program and exit gracefully.

  * `BROADcast.exe /b /d`:
    Display various diagnostic messages during the broadcast.

“Interface” is the network interface friendly name which can be looked up (and changed)
in the Network and Sharing Center section of the Windows Control Panel.

You can find this list quickly by opening a Run prompt (WinKey+X -> Run or Start -> Run...)
and entering:
  `%windir%\explorer.exe shell:::{992CFFA0-F557-101A-88EC-00DD010CCC48}`

## Usage

  1. `BROADcast.exe /i "VPN" /m`: make the “VPN” network interface preferred route.
  2. `BROADcast.exe /b`: start the broadcast.
  3. Start any application which relies on UDP broadcast packets.
  4. Once you are done with it, stop the BROADcast.exe (Ctrl+C).
  5. `BROADcast.exe /i "VPN"`: undo all network interface metric changes.

## OpenVPN

Repository contains an example OpenVPN configuration and scripts
for running BROADcast after starting a OpenVPN server:

  * `clients`: client OpenVPN configuration.
  * `config`: server OpenVPN configuration.
  * `scripts`: OpenVPN start / stop scripts that run BROADcast.
  * `settings`: OpenVPN registry settings.
  * `static`: static IP client configuration.
  * `tap`: install / uninstall the TAP Windows driver.
  * `adjust.js`: script adjusting the OpenVPN config variables.
  * `run.bat` and `run.js`: scripts for starting the OpenVPN GUI.

Note that just like OpenVPN BROADcast requires administrator privileges to run.

## Building

You can use [tdm-gcc](http://tdm-gcc.tdragon.net/), [MinGW-w64](http://mingw-w64.org/),
or [Microsoft Visual Studio 2013+](https://www.visualstudio.com/) to build **BROADcast** yourself.
