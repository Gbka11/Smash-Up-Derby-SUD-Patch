==============================================================
  SUD Patch v1.0 - unofficial patch for Smash Up Derby
  (City Interactive, 2003)
  Author: Sponge (Gbka)
==============================================================

An unofficial patch that fixes problems the game has on modern
Windows (10/11). It is a single d3d8.dll placed next to cars.exe -
no original game files are modified. Every fix can be turned off
in SUD_Patch.ini.


FEATURES
--------
Display
 - Widescreen: no more 4:3 black bars in races, the image is not
   stretched and you see more of the track (Hor+).
 - Automatic resolution: the game starts at your desktop resolution
   instead of the default 640x480.
 - White-screen guard: fixes the white screen / broken menu after the
   game saves a wrong (DX7) graphics driver.

Sound
 - The game no longer resets its volume in the Windows mixer to ~81%
   on every race. Set it once in the mixer and it stays.

Multiplayer
 - LAN server search works on every network adapter (LAN, Wi-Fi,
   Radmin VPN, Hamachi...).
 - Game data (UDP) is accepted on every adapter - connections over a
   VPN no longer drop right after connecting.

Windows
 - cars.exe.manifest stops the "This program might not have installed
   correctly" prompt.


INSTALLATION
------------
1. Copy these files into the game folder (next to cars.exe), e.g.
   C:\Program Files (x86)\Smash up Derby\
     d3d8.dll
     SUD_Patch.ini
     cars.exe.manifest
     SUD_Patch_setup.bat
2. Right click SUD_Patch_setup.bat -> "Run as administrator" (once).
   It makes Windows read the manifest and adds firewall rules for
   multiplayer.
3. Start the game.


MULTIPLAYER
-----------
 - Everybody needs the patch installed.
 - Ports: TCP 18005 (game server), UDP 18023 (LAN search),
   plus random UDP ports for game data (assigned by Windows).
 - Over the Internet use a VPN like Radmin VPN or Hamachi and pick the
   server from the LAN list. Port forwarding on a router is not enough
   because of the random UDP ports.
 - The host must run SUD_Patch_setup.bat (firewall rules). Windows often
   marks VPN networks as "public", where incoming traffic is blocked.


SETTINGS (SUD_Patch.ini)
------------------------
[General]  Log           1 = write SUD_Patch.log
[Display]  Widescreen    1 = remove the 4:3 bars in races
           Width/Height  0 = desktop resolution, -1 = keep the game setting,
                         or your own values, e.g. 2560 / 1440
           FixRegistry   1 = white-screen guard
[Sound]    FixVolume     1 = game cannot change its mixer volume
[Network]  AllInterfaces 1 = LAN search on all adapters
           BroadcastToo  1 = also search with broadcast (helps Radmin)
           BindAnyUDP    1 = game UDP on all adapters (VPN connections)


TROUBLESHOOTING
---------------
 - Log file: SUD_Patch.log in the game folder. If it is not there,
   Windows redirected it to:
   %LOCALAPPDATA%\VirtualStore\Program Files (x86)\Smash up Derby\
 - White screen even with the patch: run in cmd
     reg delete "HKCU\Software\Classes\VirtualStore\MACHINE\SOFTWARE\WOW6432Node\tatanka" /f
 - Something broke after adding the manifest: delete cars.exe.manifest.


UNINSTALL
---------
Delete d3d8.dll, SUD_Patch.ini, SUD_Patch.log and cars.exe.manifest.
Optional - remove the firewall rules (cmd as administrator):
  netsh advfirewall firewall delete rule name="Smash Up Derby (SUD Patch)"


CREDITS
-------
Patch:              Sponge (Gbka)
Multiplayer tests:  Grzelciu and Yoshi - thanks for testing the
                    multiplayer over Radmin VPN!

SUPPORT
-------
If the patch helped you, you can buy me a coffee:
  https://ko-fi.com/gbka1

Smash Up Derby (c) City Interactive / Lemon Interactive.
This is an unofficial fan patch, not affiliated with the publishers.
Source code: src\sud_patch.cpp
