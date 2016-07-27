# BROADcast

**BROADcast** is a little Windows program which forces global UDP broadcast
packets on all network interfaces.

This allows proper functionality of applications which expect UDP broadcast
packets to be sent (and received) across all network interfaces available
in the system (not just the “default” one) and thus to be available
on all networks the system is connected to.

Lots of software developed to work in LAN environment depends on this feature,
such as chat applications, multiplayer video games, and other
collaborative software.

For a detailed example, using **BROADcast** can allow several people
to play *Warcraft III* (which advertises its game session using UDP broadcast)
over VPN (such as [OpenVPN](https://openvpn.net/)), making the game session
discoverable for all participants. (Remember to use TAP device.)

(Windows normally sends broadcast packets only on default network interface,
which is determined based on its metric in the routing table, which normally
is a lowest metric value assigned by the system. **BROADcast** allows to both
change the default network interface by modifying its metric and relay
broadcast packets to all other supported network interfaces as well.)

**Note:** BROADcast will only affect IPv4 Ethernet and Wi-Fi network interfaces.

(IPv6 employs multicast, which is different.)

## Detailed Function Explanation

  * BROADcast.exe /i “Interface” /m:
    Set the metric of all network interfaces except “Interface” to manual.
    The manual metric value is computed for each interface separately as
    current metric value + “Interface” metric. This turns the “Interface”
    into interface with the lowest metric and makes it a preferred route.

  * BROADcast.exe /i “Interface”:
    Opposite of the previous command: switch all network interfaces back
    to the automatic metric assigned by the operating system.

  * BROADcast.exe /b:
    Start the actual UDP broadcast relaying. The broadcast packets will be
    relayed to all network interfaces (except the preferred route).
    Use “Ctrl+C” to stop the program and exit cleanly.

  * BROADcast.exe /b /d:
    Display various diagnostic messages during the broadcast.

“Interface” is a network interface literal name which can be looked up (and changed)
in Network and Sharing Center section of Windows Control Panel.

## Usage Example

  1. BROADcast.exe /i “VPN” /m: make the “VPN” network interface preferred route.
  2. BROADcast.exe /b: start the broadcast.
  3. Start any application which relies on UDP broadcast packets.
  4. Once you are done with it, stop the BROADcast.exe (Ctrl+C).
  5. BROADcast.exe /i “VPN”: undo all network interface metric changes.
