# MsPacmanRipper 1.2.0 Verification Report

**Created by Jacob Hodgkins**

## Canonical full-export regression

Verification used the exact source tree bundled inside the final macOS universal artifact.

- `--version` reporting `MsPacmanRipper 1.2.0`: **VERIFIED**.
- Canonical 13-member set identity: **13/13 VERIFIED**.
- Canonical total bytes: **35,616/35,616 VERIFIED**.
- Standalone full-disassembly export: **VERIFIED**.
- Normal export output files: **60**.
- Primary human-readable source `program/mspacman.asm`: **19,541 lines**.
- Board byte ownership rows: **35,616**.

The macOS port changes do not create a second analyzer or exporter. macOS, Windows, and Linux use the same core source and structured export pipeline.

## macOS Apple Silicon verification

GitHub Actions runner: native `arm64` macOS.

- Python helper scripts compile: **VERIFIED**.
- Apple `clang++` direct build through `xcrun`: **VERIFIED**.
- Active macOS SDK discovery: **VERIFIED**.
- Native architecture output: **arm64 VERIFIED**.
- `--version` and `--help`: **VERIFIED**.
- Ad-hoc code signature: **VERIFIED**.
- No Homebrew/MacPorts runtime-library path dependency: **VERIFIED**.
- Native CMake build: **VERIFIED**.
- ZIP streaming through `/usr/bin/unzip` reaches canonical validation: **VERIFIED**.
- Architecture slice artifact: **VERIFIED**.

## macOS Intel verification

GitHub Actions runner: native `x86_64` macOS.

- Python helper scripts compile: **VERIFIED**.
- Apple `clang++` direct build through `xcrun`: **VERIFIED**.
- Active macOS SDK discovery: **VERIFIED**.
- Native architecture output: **x86_64 VERIFIED**.
- `--version` and `--help`: **VERIFIED**.
- Ad-hoc code signature: **VERIFIED**.
- No Homebrew/MacPorts runtime-library path dependency: **VERIFIED**.
- Native CMake build: **VERIFIED**.
- ZIP streaming through `/usr/bin/unzip` reaches canonical validation: **VERIFIED**.
- Architecture slice artifact: **VERIFIED**.

## macOS universal package verification

The final universal binary is assembled with `lipo` from the two independently verified native slices.

- `arm64` slice present: **VERIFIED**.
- `x86_64` slice present: **VERIFIED**.
- Universal binary ad-hoc code signature: **VERIFIED**.
- Runnable `.tar.gz` package assembly: **VERIFIED**.
- Package extraction: **VERIFIED**.
- Packaged executable `--version`: **VERIFIED**.
- Packaged universal architecture check: **VERIFIED**.
- Final artifact: `MsPacmanRipper-macOS-universal`.

The artifact is not Developer ID signed or notarized; no Apple signing credential is stored in the public project.

## Windows regression

The Windows GitHub Actions workflow also passed on the same 1.2.0 source after the macOS port.

- Windows CMake/MSVC build: **VERIFIED**.
- Windows executable smoke test: **VERIFIED**.
- Windows ZIP-loader smoke test: **VERIFIED**.
- Windows runtime package smoke test: **VERIFIED**.

## Existing semantic/reconstruction certification

- Inherited Pac-Man semantic family markers: **381/381**.
- Ms. Pac-Man-specific semantic family markers: **63/63**.
- Program/daughterboard physical bytes represented: **26,624/26,624**.
- Z80 CODE bytes represented: **14,242/14,242**.
- Semantic DATA bytes represented: **12,382/12,382**.
- Complete physical board files reconstructable: **13/13**.
- Previously certified SjASMPlus exact reconstruction: **13/13 files, 35,616/35,616 bytes exact**.

## Repository and packaging

- Source tree contains no Ms. Pac-Man ROM/PROM binaries: **VERIFIED**.
- Public CI contains no copyrighted ROM/PROM payload.
- Public macOS ZIP tests use synthetic non-canonical data only to exercise the platform loader.
- macOS universal package contains project source/runtime assets and ROM-free evidence only.
