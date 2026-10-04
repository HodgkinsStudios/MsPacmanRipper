@echo off
rem Created by Jacob Hodgkins
setlocal
cd /d "%~dp0"

if not exist bin mkdir bin

set "SOURCES=src\main.cpp src\crypto\Hash.cpp src\rom\RomSet.cpp src\mspacman\Daughterboard.cpp src\disasm\Z80Disassembler.cpp src\analysis\StatefulAnalyzer.cpp src\semantic\SemanticCatalog.cpp src\semantic\PacmanSemanticFamilies.cpp"

if defined CXX (
    "%CXX%" -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -Isrc %SOURCES% -o bin\MsPacmanRipper.exe
    if errorlevel 1 exit /b %errorlevel%
    goto :built
)

where g++ >nul 2>&1
if not errorlevel 1 (
    g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -Isrc %SOURCES% -o bin\MsPacmanRipper.exe
    if errorlevel 1 exit /b %errorlevel%
    goto :built
)

where cl >nul 2>&1
if not errorlevel 1 (
    cl /nologo /std:c++17 /O2 /EHsc /W4 /Isrc %SOURCES% /Fe:bin\MsPacmanRipper.exe
    if errorlevel 1 exit /b %errorlevel%
    del /q *.obj >nul 2>&1
    goto :built
)

echo ERROR: No supported C++ compiler was found.
echo Install Code::Blocks with MinGW/GCC, put g++ on PATH, or run from a Visual Studio Developer Command Prompt.
exit /b 1

:built
echo Built: %CD%\bin\MsPacmanRipper.exe
