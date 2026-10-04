@echo off
rem Created by Jacob Hodgkins
setlocal
cd /d "%~dp0"

if not exist bin\MsPacmanRipper.exe (
    call build_windows.bat
    if errorlevel 1 exit /b %errorlevel%
)

bin\MsPacmanRipper.exe %*
