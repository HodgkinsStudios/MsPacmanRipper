# MsPacmanRipper

**Created by Jacob Hodgkins**  
**Version 1.2.0**

MsPacmanRipper is a standalone C++17 command-line tool for **macOS, Windows, and Ubuntu/Linux** that produces a complete structured disassembly of the supported canonical Ms. Pac-Man arcade ROM/PROM set. All three platforms use the same analyzer, ROM validation, daughterboard model, semantic pipeline, and output format.

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

## macOS

Version 1.2.0 adds first-class support for both **Apple Silicon (arm64)** and **Intel (x86_64)** Macs.

### Requirements

- macOS 11 or newer;
- Xcode Command Line Tools;
- Python 3;
- the system `/usr/bin/unzip`, used directly for ZIP input.

Install the Apple compiler tools if needed:

```bash
xcode-select --install
```

Python 3 can be supplied by any normal Python installation. The project does not require Homebrew or MacPorts libraries.

### Native build

```bash
chmod +x build_macos.sh run_macos.sh
./build_macos.sh
```

`build_macos.sh` uses Apple `clang++` through `xcrun`, the active macOS SDK, C++17, and a macOS 11 deployment target. By default it builds for the architecture of the current Mac.

The output is:

```text
bin/MsPacmanRipper
```

Run it with:

```bash
./run_macos.sh /path/to/mspacman.zip /path/to/output
```

or:

```bash
./bin/MsPacmanRipper /path/to/mspacman.zip /path/to/output
```

The build script also supports an explicit architecture when the installed Apple toolchain supports it:

```bash
MSPACMANRIPPER_MACOS_ARCHS=arm64 ./build_macos.sh
MSPACMANRIPPER_MACOS_ARCHS=x86_64 ./build_macos.sh
```

For ordinary local builds, leave this unset and use the native default.

### Code::Blocks on macOS

Open `MsPacmanRipper.cbp` and use the **Release macOS** target with a configured Clang compiler. The target builds a native executable and uses the macOS 11 deployment target.

### CMake on macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build --config Release
./build/bin/MsPacmanRipper /path/to/mspacman.zip /path/to/output
```

### Universal Intel + Apple Silicon package

The macOS GitHub Actions workflow builds the project **natively on an Apple Silicon runner and natively on an Intel runner**, verifies each architecture independently, and then combines the two verified Mach-O slices with `lipo`.

The final artifact is named:

```text
MsPacmanRipper-macOS-universal
```

It contains `MsPacmanRipper-macOS-universal.tar.gz`, whose runtime/source layout includes:

```text
MsPacmanRipper/
├── bin/MsPacmanRipper
├── scripts/
├── evidence/
├── src/
├── CMakeLists.txt
├── MsPacmanRipper.cbp
├── build_macos.sh
├── run_macos.sh
├── README.md
├── LICENSE
├── LEGAL.md
└── VERSION
```

The universal executable contains both `arm64` and `x86_64` slices. CI verifies its architecture list, code signature, packaged runtime, native ZIP loading, and that the native binaries do not link against Homebrew/MacPorts-style runtime paths.

The CI artifact is **ad-hoc signed**, not Apple Developer ID signed or notarized. A quarantined download can therefore require explicit approval in macOS Privacy & Security. Building from source on the Mac avoids distribution-signing requirements.

## Windows

### Requirements

- Windows 10 or Windows 11;
- Python 3 available as `python` or `python3`;
- for source builds: Code::Blocks + MinGW/GCC, another `g++` toolchain, Visual Studio Developer Command Prompt, or CMake;
- Windows `tar.exe`, used to stream ZIP members without a third-party ZIP library.

### Build from Command Prompt

```bat
build_windows.bat
```

Run:

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

The Windows workflow builds on `windows-latest`, checks the Python helpers, compiles the executable, smoke-tests ZIP loading, and publishes a runnable package with the required `scripts/` and `evidence/` runtime assets.

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

The executable is normally `build/bin/MsPacmanRipper` on macOS/Linux and `build\bin\MsPacmanRipper.exe` on Windows.

## Verification status

Version 1.2.0 has been verified against the current shared source:

- canonical files validated: **13/13**;
- canonical bytes validated: **35,616/35,616**;
- normal full-disassembly export: **VERIFIED**;
- normal export output files: **60**;
- primary `program/mspacman.asm`: **19,541 lines**;
- board byte-ownership rows: **35,616**;
- current source from the macOS universal package re-built on Linux and completed the canonical full export: **VERIFIED**;
- Apple Silicon direct Clang build: **VERIFIED**;
- Apple Silicon CMake build: **VERIFIED**;
- Apple Silicon ZIP-loader path: **VERIFIED**;
- Intel direct Clang build: **VERIFIED**;
- Intel CMake build: **VERIFIED**;
- Intel ZIP-loader path: **VERIFIED**;
- universal `arm64 + x86_64` Mach-O assembly and packaged-runtime test: **VERIFIED**;
- Windows CI still passes on the same 1.2.0 source: **VERIFIED**.

GitHub Actions intentionally does not contain or redistribute the copyrighted ROM/PROM set. Public CI therefore uses synthetic ZIP data to exercise platform-specific archive loading; the canonical 13-file full-export regression was performed separately with the supplied canonical set.

The established exact-reconstruction certification remains:

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

macOS/Linux:

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

macOS/Linux example:

```bash
SJASMPLUS=/path/to/sjasmplus ./bin/MsPacmanRipper /path/to/mspacman.zip /path/to/output
```

Windows Command Prompt example:

```bat
set "SJASMPLUS=C:\path\to\sjasmplus.exe"
bin\MsPacmanRipper.exe "C:\path\to\mspacman.zip" "C:\path\to\output"
```

## Repository layout

```text
MsPacmanRipper/
├── .github/workflows/     Windows and macOS build/package verification
├── evidence/              ROM-free semantic metadata used by the exporter
├── scripts/               structured export and reconstruction verification tools
├── src/                   C++17 source
├── CMakeLists.txt         cross-platform CMake build
├── MsPacmanRipper.cbp     Linux, Windows, and macOS Code::Blocks targets
├── VERSION                release version
├── build.sh               Ubuntu/Linux command-line build
├── run.sh                 Ubuntu/Linux build-if-needed launcher
├── build_windows.bat      Windows build
├── run_windows.bat        Windows launcher
├── build_macos.sh         native macOS Apple Clang build
└── run_macos.sh           macOS build-if-needed launcher
```

## Implementation notes

The public program has one normal job and one normal invocation: complete Ms. Pac-Man disassembly. Two `--internal-*` subprocess entry points are implementation plumbing used by the bundled structured exporter.

The ROM loader uses C++17 `std::filesystem` on all supported platforms. ZIP streaming uses `unzip` on Linux, the built-in `tar.exe` on Windows, and the system `/usr/bin/unzip` on macOS. Python helper scripts are launched through the active Python interpreter.

## License and third-party material

MsPacmanRipper source code is released under the MIT License. See `LICENSE`.

No game ROM/PROM data is distributed with the project. See `LEGAL.md` for the project disclaimer and the boundary between the MsPacmanRipper source license and third-party game material.

## Contributing and security

See `CONTRIBUTING.md` for contribution guidance and `SECURITY.md` for security reporting guidance.
