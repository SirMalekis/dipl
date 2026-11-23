@echo off
setlocal EnableExtensions
chcp 65001 >nul
cd /d "%~dp0"

rem One-click showcase: 5 presets + CSV + combined plot + GUI screenshots.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\run_showcase.ps1" %*
set "CODE=%ERRORLEVEL%"

if not "%CODE%"=="0" (
    echo [NetSim] Showcase failed with code %CODE%.
    pause
    exit /b %CODE%
)
if "%~1"=="" pause
exit /b 0
