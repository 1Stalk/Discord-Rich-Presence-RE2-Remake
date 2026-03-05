@echo off
setlocal EnableDelayedExpansion
title Building DiscordPresenceRE2R...

echo ================================================
echo  Building DiscordPresenceRE2R.dll
echo  REFramework Plugin for Resident Evil 2 Remake
echo ================================================
echo.

:: -----------------------------------------------
:: Find Visual Studio vcvars64.bat
:: -----------------------------------------------
set "VCVARS="
for %%Y in (2022 2019 2017) do (
    for %%E in (BuildTools Community Professional Enterprise) do (
        if exist "C:\Program Files (x86)\Microsoft Visual Studio\%%Y\%%E\VC\Auxiliary\Build\vcvars64.bat" (
            set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\%%Y\%%E\VC\Auxiliary\Build\vcvars64.bat"
            echo Found: Visual Studio %%Y %%E
            goto :found_vs
        )
        if exist "C:\Program Files\Microsoft Visual Studio\%%Y\%%E\VC\Auxiliary\Build\vcvars64.bat" (
            set "VCVARS=C:\Program Files\Microsoft Visual Studio\%%Y\%%E\VC\Auxiliary\Build\vcvars64.bat"
            echo Found: Visual Studio %%Y %%E
            goto :found_vs
        )
    )
)
echo ERROR: Visual Studio not found!
pause
exit /b 1

:found_vs
echo Setting up 64-bit compiler environment...
call "%VCVARS%" x64 > nul 2>&1

:: -----------------------------------------------
:: Build into our own reframework/plugins/
:: -----------------------------------------------
set "SRC=%~dp0src\main.cpp"
set "OUT=%~dp0DiscordPresenceRE2R.dll"
set "PLUGINS_DIR=%~dp0reframework\plugins"

echo Compiling...
echo.

cl.exe ^
    /LD ^
    /O2 ^
    /GL ^
    /GS- ^
    /std:c++17 ^
    /EHsc ^
    /DWIN32_LEAN_AND_MEAN ^
    /DNOMINMAX ^
    /W3 ^
    "%SRC%" ^
    /Fe:"%OUT%" ^
    /link ^
    /DLL ^
    /LTCG ^
    /OPT:REF ^
    /OPT:ICF ^
    kernel32.lib

if errorlevel 1 (
    echo.
    echo ERROR: Compilation failed!
    pause
    exit /b 1
)

if not exist "%PLUGINS_DIR%" mkdir "%PLUGINS_DIR%"
move /Y "%OUT%" "%PLUGINS_DIR%\DiscordPresenceRE2R.dll" > nul

:: Clean up intermediate files
del /Q "%~dp0*.obj" "%~dp0*.exp" "%~dp0*.lib" 2>nul

echo.
echo ================================================
echo  SUCCESS!
echo  DLL: %PLUGINS_DIR%\DiscordPresenceRE2R.dll
echo ================================================
echo.
pause
