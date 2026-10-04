# MsPacmanRipper

**Created by Jacob Hodgkins**  
**Version 1.0.0**

MsPacmanRipper is a standalone C++17 command-line tool for **Windows and Ubuntu/Linux** that produces a complete structured disassembly of the supported canonical Ms. Pac-Man arcade ROM/PROM set. The same core source is used on both platforms, with Code::Blocks/GCC, command-line build scripts, and a CMake build available.

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

## Platform support

### Windows

The Windows port supports both ZIP input and an extracted ROM directory.

Requirements:

- Windows 10 or Windows 11;
- Python 3 available as `python` or `python3`;
- either Code::Blocks with MinGW/GCC, a `g++` toolchain on `PATH`, or a Visual Studio Developer Command Prompt;
- the built-in Windows `tar.exe` used to read ZIP input.

Build from Command Prompt:

```bat
build_windows.bat
```

Run:

```bat
run_windows.bat "C:\path\to\mspacman.zip" "C:\path\to\output"
```

or directly:

```bat
bin\MsPacmanRipper.exe "C:\path\to\mspacman.zip" "C:\path\to\output"
```

In Code::Blocks, open `MsPacmanRipper.cbp` and build the **Release Windows** target.

### Ubuntu/Linux

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

### CMake

A cross-platform CMake build is also available:

```text
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

On Windows the executable is `build\bin\MsPacmanRipper.exe`. On single-config Linux generators it is `build/bin/MsPacmanRipper`.

## Verified reconstruction

Release 1.0.0 was verified against the supported canonical set:

- physical files reconstructed: **13/13**;
- physical bytes reconstructed: **35,616/35,616**;
- byte-exact comparison: **verified**;
- Z80 CODE bytes represented: **14,242/14,242**;
- semantic DATA bytes represented: **12,382/12,382**;
- program/daughterboard physical bytes represented: **26,624/26,624**.

See `TEST_REPORT.md` for the release verification record.

The Windows port is also compiled on GitHub Actions using the Windows runner. The platform changes keep the canonical validation and structured-export pipeline shared with Linux rather than maintaining a separate Windows implementation.

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

A successful verified export reports an exact `13/13 files` and `35616/35616 bytes` reconstruction. Immediate verification adds local rebuild/verification artifacts to the selected output directory; these are not part of the public source package.

## Repository layout

```text
MsPacmanRipper/
├── .github/workflows/     Windows build verification
├── evidence/              ROM-free semantic metadata used by the exporter
├── scripts/               structured export and reconstruction verification tools
├── src/                   C++17 source
├── CMakeLists.txt         cross-platform CMake build
├── MsPacmanRipper.cbp     Code::Blocks project with Linux and Windows targets
├── VERSION                release version
├── build.sh               Ubuntu/Linux command-line build
├── run.sh                 Ubuntu/Linux build-if-needed launcher
├── build_windows.bat      Windows MinGW/MSVC command-line build
└── run_windows.bat        Windows build-if-needed launcher
```

## Implementation note

The public program has one normal job and one normal invocation: complete Ms. Pac-Man disassembly. Two `--internal-*` subprocess entry points are implementation plumbing used by the bundled structured exporter to invoke the C++ analyzer and daughterboard logical-image generator.

The ROM loader uses `std::filesystem` on both platforms. ZIP streaming uses `unzip` on Linux and the built-in `tar.exe` on Windows. Python helper scripts are launched through the active Python interpreter so the export pipeline works without Unix executable-bit semantics.

## Output stability

The exporter uses stable descriptive filenames for analysis, semantic evidence, reconstruction material, and verification reports. Generated source remains fully reconstructable and is validated against the complete supported 13-device board set.

## License and third-party material

MsPacmanRipper source code is released under the MIT License. See `LICENSE`.

No game ROM/PROM data is distributed with the project. See `LEGAL.md` for the project disclaimer and the boundary between the MsPacmanRipper source license and third-party game material.

## Contributing and security

See `CONTRIBUTING.md` for contribution guidance and `SECURITY.md` for security reporting guidance.
