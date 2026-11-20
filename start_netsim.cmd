@echo off
setlocal EnableExtensions
chcp 65001 >nul
cd /d "%~dp0"

rem ---------------------------------------------------------------------------
rem NetSim: one-file Windows launcher.
rem - first run: builds x64\Release\netsim.exe if it is missing;
rem - then starts the CustomTkinter GUI;
rem - all arguments are forwarded to run_gui.py;
rem - --rebuild forces a rebuild (the option itself is not forwarded).
rem ---------------------------------------------------------------------------

set "PYTHON=python"
where python >nul 2>&1
if errorlevel 1 (
    where py >nul 2>&1
    if errorlevel 1 (
        echo [NetSim] Python 3 not found in PATH.
        echo Install Python 3.11+ and run: python -m pip install -r requirements.txt
        pause
        exit /b 1
    )
    set "PYTHON=py -3"
)

set "BUILD=0"
set "ARGS=%*"
if /I "%~1"=="--rebuild" (
    set "BUILD=1"
    for /f "tokens=1,*" %%A in ("%*") do set "ARGS=%%B"
)

if not exist "x64\Release\netsim.exe" set "BUILD=1"

if "%BUILD%"=="1" (
    echo [NetSim] Building C++ core...
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1"
    if errorlevel 1 (
        echo [NetSim] Build failed. See the messages above.
        pause
        exit /b 1
    )
)

set "PYTHONUTF8=1"
%PYTHON% run_gui.py %ARGS%
set "CODE=%ERRORLEVEL%"

if not "%CODE%"=="0" (
    echo [NetSim] GUI exited with code %CODE%.
    if /I not "%ARGS%"=="--selftest" pause
)
exit /b %CODE%
