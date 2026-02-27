@echo off
setlocal enabledelayedexpansion
title Slowfiz Scanner - Build
color 0A

echo.
echo  ================================================
echo   Slowfiz Scanner - Auto Build Script
echo  ================================================
echo.

:: Sicherstellen dass wir im richtigen Ordner sind
cd /d "%~dp0"

:: Alten Build-Cache loeschen (verhindert Generator-Konflikte)
if exist "build" (
    echo  [INFO] Altes build\ Verzeichnis wird geloescht...
    rmdir /s /q "build"
)
mkdir build

:: Checks
if not exist "main.cpp"       ( echo  [ERROR] main.cpp fehlt!       & pause & exit /b 1 )
if not exist "CMakeLists.txt" ( echo  [ERROR] CMakeLists.txt fehlt! & pause & exit /b 1 )
if not exist "imgui\imgui.h"  ( echo  [ERROR] imgui\imgui.h fehlt!  & pause & exit /b 1 )

cmake --version >nul 2>&1
if errorlevel 1 ( echo  [ERROR] CMake nicht gefunden! & pause & exit /b 1 )

echo  [OK] Alle Dateien gefunden
echo.

cd build

:: Alle VS-Versionen der Reihe nach probieren
set BUILT=0

for %%G in (
    "Visual Studio 18 2026"
    "Visual Studio 17 2022"
    "Visual Studio 16 2019"
    "Visual Studio 15 2017"
) do (
    if !BUILT!==0 (
        echo  [TRY] Generator: %%~G
        cmake .. -G %%G -A x64 -Wno-dev >nul 2>&1
        if !errorlevel!==0 (
            echo  [OK]  Gefunden: %%~G
            set BUILT=1
        )
    )
)

if !BUILT!==0 (
    echo.
    echo  [ERROR] Kein Visual Studio Generator gefunden.
    echo  Bitte stelle sicher dass Visual Studio mit
    echo  "Desktop development with C++" installiert ist.
    cd ..
    pause
    exit /b 1
)

echo.
echo  [BUILD] Kompiliere Release... (1-3 Minuten)
cmake --build . --config Release

if errorlevel 1 (
    echo.
    echo  [ERROR] Kompilierung fehlgeschlagen!
    cd ..
    pause
    exit /b 1
)

cd ..

if exist "build\Release\SlowfizScanner.exe" (
    copy /Y "build\Release\SlowfizScanner.exe" "SlowfizScanner.exe" >nul
    echo.
    echo  ================================================
    echo   BUILD ERFOLGREICH!
    echo   SlowfizScanner.exe ist fertig!
    echo  ================================================
    echo.
    set /p OPEN="  Jetzt starten? (j/n): "
    if /i "!OPEN!"=="j" start "" "SlowfizScanner.exe"
) else (
    echo  [WARN] Suche EXE...
    for /r build %%f in (*.exe) do (
        echo  Gefunden: %%f
        copy /Y "%%f" "SlowfizScanner.exe" >nul
    )
)

echo.
pause