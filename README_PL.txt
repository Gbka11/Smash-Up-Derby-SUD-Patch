==============================================================
  SUD Patch v1.0 - nieoficjalny patch do Smash Up Derby
  (City Interactive, 2003)
  Autor: Sponge (Gbka)
==============================================================

Nieoficjalny patch naprawiający problemy gry na nowych Windowsach
(10/11). To jeden plik d3d8.dll wrzucany obok cars.exe - żaden
oryginalny plik gry nie jest zmieniany. Każdą poprawkę można
wyłączyć w SUD_Patch.ini.


CO NAPRAWIA
-----------
Obraz
 - Widescreen: koniec z czarnymi pasami 4:3 w wyścigach, obraz nie jest
   rozciągnięty i widać więcej trasy (Hor+).
 - Automatyczna rozdzielczość: gra startuje w rozdzielczości pulpitu,
   a nie w domyślnym 640x480.
 - Ochrona przed białym ekranem: naprawia biały ekran / zepsute menu,
   gdy gra zapisze sobie zły sterownik grafiki (DX7).

Dźwięk
 - Gra przestaje ustawiać swoją głośność w mikserze Windows na ~81%
   przy każdym wyścigu. Ustawiasz raz w mikserze i tak zostaje.

Multiplayer
 - Wyszukiwanie serwerów LAN działa na każdej karcie sieciowej
   (LAN, Wi-Fi, Radmin VPN, Hamachi...).
 - Dane gry (UDP) są odbierane na każdej karcie - połączenie przez VPN
   nie zrywa się zaraz po połączeniu.

Windows
 - cars.exe.manifest wyłącza komunikat "Ten program może nie być
   zainstalowany poprawnie".


INSTALACJA
----------
1. Skopiuj pliki do folderu gry (obok cars.exe), np.
   C:\Program Files (x86)\Smash up Derby\
     d3d8.dll
     SUD_Patch.ini
     cars.exe.manifest
     SUD_Patch_setup.bat
2. Prawy przycisk na SUD_Patch_setup.bat -> "Uruchom jako administrator"
   (jednorazowo). Windows zacznie używać manifestu i zostaną dodane
   reguły zapory dla multiplayera.
3. Uruchom grę.


MULTIPLAYER
-----------
 - Patch muszą mieć wszyscy gracze.
 - Porty: TCP 18005 (serwer gry), UDP 18023 (wyszukiwanie w LAN),
   do tego losowe porty UDP na dane gry (przydziela je Windows).
 - Przez internet używajcie VPN typu Radmin VPN lub Hamachi i wybierajcie
   serwer z listy LAN. Samo przekierowanie portów na routerze nie
   wystarczy przez losowe porty UDP.
 - Host musi uruchomić SUD_Patch_setup.bat (reguły zapory). Windows często
   oznacza sieć VPN jako "publiczną" i blokuje ruch przychodzący.


USTAWIENIA (SUD_Patch.ini)
--------------------------
[General]  Log           1 = zapisuj SUD_Patch.log
[Display]  Widescreen    1 = usuń pasy 4:3 w wyścigach
           Width/Height  0 = rozdzielczość pulpitu, -1 = ustawienie gry,
                         albo własne wartości, np. 2560 / 1440
           FixRegistry   1 = ochrona przed białym ekranem
[Sound]    FixVolume     1 = gra nie zmienia głośności w mikserze
[Network]  AllInterfaces 1 = wyszukiwanie LAN na wszystkich kartach
           BroadcastToo  1 = szukaj też broadcastem (pomaga przy Radminie)
           BindAnyUDP    1 = UDP gry na wszystkich kartach (połączenia VPN)


PROBLEMY
--------
 - Log: SUD_Patch.log w folderze gry. Jeśli go tam nie ma, Windows
   przekierował go do:
   %LOCALAPPDATA%\VirtualStore\Program Files (x86)\Smash up Derby\
 - Biały ekran mimo patcha: wpisz w cmd
     reg delete "HKCU\Software\Classes\VirtualStore\MACHINE\SOFTWARE\WOW6432Node\tatanka" /f
 - Coś się zepsuło po dodaniu manifestu: usuń cars.exe.manifest.


ODINSTALOWANIE
--------------
Usuń d3d8.dll, SUD_Patch.ini, SUD_Patch.log i cars.exe.manifest.
Opcjonalnie usuń reguły zapory (cmd jako administrator):
  netsh advfirewall firewall delete rule name="Smash Up Derby (SUD Patch)"


PODZIĘKOWANIA
-------------
Patch:              Sponge (Gbka)
Testy multiplayer:  Grzelciu i Yoshi - dzięki za testy multiplayera
                    przez Radmin VPN!

Smash Up Derby (c) City Interactive / Lemon Interactive.
To nieoficjalny patch fanowski, niezwiązany z wydawcami.
Kod źródłowy: src\sud_patch.cpp
