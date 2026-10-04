# MsPacmanRipper

**Created by Jacob Hodgkins**  
**Version 1.1.0**

MsPacmanRipper is a standalone C++17 command-line tool for **Windows 10/11 and Ubuntu/Linux** that produces a complete structured disassembly of the supported canonical Ms. Pac-Man arcade ROM/PROM set. Windows and Linux use the same analyzer, ROM validation, daughterboard model, semantic pipeline, and output format.

## Features

Given the canonical 13-file `mspacman.zip` or a directory containing the same 13 files, MsPacmanRipper generates a complete disassembly tree containing:

- human-readable Z80 source in `program/mspacman.asm`;
- complete code/data ownership and semantic catalogs;
- structured character and sprite graphics source;
- palette and color lookup PROM source;
- waveform and sound timing/control PROM source;
- complete 13-device board manifest and byte ownership;
- reconstruction scripts and verification material capable of rebuilding all 13 physical ROM/PROM files exactly.

The public source package contains **no Ms. Pac-Man ROM/PROM data** and no pre-generated ROM-derived disassembly tree.

## Windows

### Requirements

- Windows 10 or Windows 11;
- Python 3 available as `python` or `python3`;
- for source builds: Code::Blocks + MinGW/GCC, another `g++` toolchain, Visual Studio Developer Command Prompt, or CMake;
- Windows `tar.exe`, which is used to stream ZIP members without adding a third-party ZIP library.

### Build from Command Prompt

```bat
build_windows.bat
```

This produces:

```text
bin\MsPacmanRipper.exe
```

### Run

```bat
run_windows.bat "C:\path\to\mspacman.zip" "C:\path\to\output"
```

or:

```bat
bin\MsPacmanRipper.exe "C:\path\to\mspacman.zip" "C:\path\to\output"
```

In Code::Blocks, open `MsPacmanRipper.cbp` and build the **Release Windows** target.

### CMake on Windows

```bat
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
build\bin\MsPacmanRipper.exe "C:\path\to\mspacman.zip" "C:\path\to\output"
```

The runtime now discovers the bundled export pipeline from nested build layouts such as `build\bin`, and nested verification uses the executable that actually launched the export rather than assuming `bin\MsPacmanRipper.exe`.

### GitHub Actions Windows package

The Windows workflow builds on `windows-latest`, checks all Python helper scripts, compiles the C++ executable, smoke-tests ZIP loading, and publishes a **MsPacmanRipper-Windows** artifact containing the runnable layout:

```text
MsPacmanRipper/
├── bin/MsPacmanRipper.exe
├── scripts/
├── evidence/
├── run_windows.bat
├── README.md
├── LICENSE
├── LEGAL.md
└── VERSION
```

Python 3 is still required at runtime because the structured export pipeline intentionally uses the bundled Python scripts.

## Ubuntu/Linux

Install the normal build/runtime requirements:

```bash
sudo apt install build-essential unzip python3
```

Build:

```bash
./build.sh
```

Run:

```bash
./run.sh /path/to/mspacman.zip /path/to/output
```

In Code::Blocks, open `MsPacmanRipper.cbp` and build the **Release** target.

## Cross-platform CMake

```text
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

On Windows the executable is normally `build\bin\MsPacmanRipper.exe`. On single-config Linux generators it is `build/bin/MsPacmanRipper`.

## Verification status

Version 1.1.0 was reviewed with the canonical 13-file set and the current shared source:

- canonical files validated: **13/13**;
- canonical bytes validated: **35,616/35,616**;
- normal full-disassembly export: **VERIFIED**;
- normal export output files: **60**;
- primary `program/mspacman.asm`: **19,541 lines**;
- board byte-ownership rows: **35,616**;
- normal `build.sh` export and CMake `build/bin` export: **byte-for-byte identical**;
- CMake executable tested from outside the repository with the normal root `bin` executable removed: **VERIFIED**;
- current `windows-latest` CMake/MSVC build: **VERIFIED**;
- Windows ZIP loader reaching canonical validation: **VERIFIED**;
- packaged Windows runtime layout: **VERIFIED by CI**.

GitHub Actions intentionally does not contain or redistribute the copyrighted ROM/PROM set. The canonical full-export test is therefore kept outside the public CI job, while Windows CI verifies the platform-specific compiler, Python, executable, ZIP-loader, and package paths.

The previously certified exact reconstruction remains:

- physical files reconstructed: **13/13**;
- physical bytes reconstructed: **35,616/35,616**;
- byte-exact comparison: **verified**;
- Z80 CODE bytes represented: **14,242/14,242**;
- semantic DATA bytes represented: **12,382/12,382**;
- program/daughterboard physical bytes represented: **26,624/26,624**.

See `TEST_REPORT.md` for the verification record.

## Canonical input

The supported set contains exactly these 13 members:

`pacman.6e`, `pacman.6f`, `pacman.6h`, `pacman.6j`, `u5`, `u6`, `u7`, `5e`, `5f`, `82s123.7f`, `82s126.4a`, `82s126.1m`, `82s126.3m`.

MsPacmanRipper validates file size, CRC32, and SHA-256 before disassembling and rejects an incomplete or non-canonical set.

## Help and version

Ubuntu/Linux:

```bash
./bin/MsPacmanRipper --help
./bin/MsPacmanRipper --version
```

Windows:

```bat
bin\MsPacmanRipper.exe --help
bin\MsPacmanRipper.exe --version
```

## Optional immediate reconstruction verification

SjASMPlus is optional for normal export. If the `SJASMPLUS` environment variable points to a SjASMPlus executable, the exporter performs an immediate exact 13-file reconstruction verification after export.

Linux example:

```bash
SJASMPLUS=/path/to/sjasmplus ./bin/MsPacmanRipper /path/to/mspacman.zip /path/to/output
```

Windows Command Prompt example:

```bat
set "SJASMPLUS=C:\path\to\sjasmplus.exe"
bin\MsPacmanRipper.exe "C:\path\to\mspacman.zip" "C:\path\to\output"
```

A successful verified export reports an exact `13/13 files` and `35616/35616 bytes` reconstruction.

## Repository layout

```text
MsPacmanRipper/
├── .github/workflows/     Windows build/package verification
├── evidence/              ROM-free semantic metadata used by the exporter
├── scripts/               structured export and reconstruction verification tools
├── src/                   C++17 source
├── CMakeLists.txt         cross-platform CMake build
├── MsPacmanRipper.cbp     Code::Blocks Linux and Windows targets
├── VERSION                release version
├── build.sh               Ubuntu/Linux command-line build
├── run.sh                 Ubuntu/Linux build-if-needed launcher
├── build_windows.bat      Windows MinGW/MSVC command-line build
└── run_windows.bat        Windows build-if-needed launcher
```

## Implementation notes

The public program has one normal job and one normal invocation: complete Ms. Pac-Man disassembly. Two `--internal-*` subprocess entry points are implementation plumbing used by the bundled structured exporter to invoke the C++ analyzer and daughterboard logical-image generator.

The ROM loader uses C++17 `std::filesystem` on both platforms. ZIP streaming uses `unzip` on Linux and Windows `tar.exe` on Windows. Python helper scripts are launched through the active Python interpreter so Windows does not depend on Unix executable-bit semantics.

## License and third-party material

MsPacmanRipper source code is released under the MIT License. See `LICENSE`.

No game ROM/PROM data is distributed with the project. See `LEGAL.md` for the project disclaimer and the boundary between the MsPacmanRipper source license and third-party game material.

## Contributing and security

See `CONTRIBUTING.md` for contribution guidance and `SECURITY.md` for security reporting guidance.
