# SUD Patch for Smash Up Derby

Unofficial patch for **Smash Up Derby** (City Interactive, 2003) that fixes its problems on Windows 10/11.
It is a single `d3d8.dll` proxy placed next to `cars.exe`. **No original game files are modified**, and every fix can be turned off in `SUD_Patch.ini`.

🇵🇱 Polish instructions: [README_PL.txt](README_PL.txt)

## Features

**Display**
- **Widescreen**: removes the 4:3 black bars in races; the image is not stretched and you see more of the track (Hor+).
- **Automatic resolution**: the game starts at your desktop resolution instead of 640×480.
- **White-screen guard**: fixes the white screen / broken menu after the game saves a wrong (DX7) graphics driver.

**Sound**
- The game no longer resets its volume in the Windows mixer to ~81% on every race.

**Multiplayer**
- LAN server search works on every network adapter (LAN, Wi-Fi, Radmin VPN, Hamachi…).
- Game data (UDP) is accepted on every adapter, so VPN connections no longer drop right after connecting.

**Windows**
- `cars.exe.manifest` stops the *"This program might not have installed correctly"* prompt.

## Installation

1. Download the latest release from **[Releases](../../releases)**.
2. Copy `d3d8.dll`, `SUD_Patch.ini`, `cars.exe.manifest` and `SUD_Patch_setup.bat` into the game folder (next to `cars.exe`).
3. Right click `SUD_Patch_setup.bat` → **Run as administrator** (once). It makes Windows use the manifest and adds firewall rules for multiplayer.
4. Start the game.

## Multiplayer

- Everybody needs the patch.
- Ports: **TCP 18005** (game server), **UDP 18023** (LAN search, multicast `225.226.227.228`), plus random UDP ports for game data.
- Over the Internet use a VPN (Radmin VPN, Hamachi) and pick the server from the LAN list. Router port forwarding is not enough because of the random UDP ports.
- The host must run `SUD_Patch_setup.bat` (firewall rules).

## Settings (`SUD_Patch.ini`)

| Section | Key | Default | Meaning |
|---|---|---|---|
| General | Log | 1 | write `SUD_Patch.log` |
| Display | Widescreen | 1 | remove the 4:3 bars in races |
| Display | Width / Height | 0 | 0 = desktop resolution, -1 = keep the game setting, or custom values |
| Display | FixRegistry | 1 | white-screen guard |
| Sound | FixVolume | 1 | game cannot change its mixer volume |
| Network | AllInterfaces | 1 | LAN search on all adapters |
| Network | BroadcastToo | 1 | also search with broadcast (helps with Radmin) |
| Network | BindAnyUDP | 1 | game UDP on all adapters (VPN connections) |

## Troubleshooting

- **Log**: `SUD_Patch.log` in the game folder, or (redirected by Windows) in `%LOCALAPPDATA%\VirtualStore\Program Files (x86)\Smash up Derby\`.
- **White screen even with the patch**:
  ```
  reg delete "HKCU\Software\Classes\VirtualStore\MACHINE\SOFTWARE\WOW6432Node\tatanka" /f
  ```
- **Something broke after adding the manifest**: delete `cars.exe.manifest`.

## Building

x86 Native Tools Command Prompt for Visual Studio:
```
cl /nologo /LD /O2 /MT /EHa src\sud_patch.cpp /Fe:d3d8.dll user32.lib
```

## Credits

- Patch: **Sponge (Gbka)**
- Multiplayer testing: **Grzelciu** and **Yoshi**, thanks for testing the multiplayer over Radmin VPN!

*Smash Up Derby © City Interactive / Lemon Interactive. This is an unofficial fan patch, not affiliated with the publishers.*
