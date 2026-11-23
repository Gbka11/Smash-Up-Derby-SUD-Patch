@echo off
title SUD Patch - setup
echo SUD Patch v1.0 - one-time setup
echo.

net session >nul 2>&1
if errorlevel 1 (
    echo Please run this file as Administrator:
    echo right click - "Run as administrator".
    echo.
    pause
    exit /b 1
)

cd /d "%~dp0"
if not exist "cars.exe" (
    echo cars.exe not found. Put this file in the Smash Up Derby folder.
    echo.
    pause
    exit /b 1
)

rem Windows caches "no manifest" for programs that already ran - refresh the exe date
copy /b cars.exe +,, >nul
if errorlevel 1 (echo [!!] could not refresh cars.exe) else (echo [OK] cars.exe refreshed - cars.exe.manifest will be used)

rem Firewall rules for multiplayer (needed on the host)
netsh advfirewall firewall delete rule name="Smash Up Derby (SUD Patch)" >nul 2>&1
netsh advfirewall firewall add rule name="Smash Up Derby (SUD Patch)" dir=in action=allow program="%~dp0cars.exe" profile=any >nul
netsh advfirewall firewall add rule name="Smash Up Derby (SUD Patch)" dir=in action=allow protocol=TCP localport=18005 profile=any >nul
netsh advfirewall firewall add rule name="Smash Up Derby (SUD Patch)" dir=in action=allow protocol=UDP localport=18023 profile=any >nul
if errorlevel 1 (echo [!!] firewall rules failed) else (echo [OK] firewall rules added: cars.exe, TCP 18005, UDP 18023)

echo.
echo Done. You can start the game.
pause
