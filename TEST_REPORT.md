# MsPacmanRipper 1.1.0 Verification Report

**Created by Jacob Hodgkins**

## Canonical full-export verification

Verification platform: Ubuntu/Linux, GCC C++17, plus a separate CMake build using the same source.

- Release build with `-O2 -Wall -Wextra -Wpedantic -Werror`: **VERIFIED**.
- `--help`: **VERIFIED**.
- `--version` reporting `MsPacmanRipper 1.1.0`: **VERIFIED after version update**.
- Canonical 13-member set identity: **13/13 VERIFIED**.
- Canonical total bytes: **35,616/35,616 VERIFIED**.
- Standalone full-disassembly export: **VERIFIED**.
- Normal export output files: **60**.
- Primary human-readable source `program/mspacman.asm`: **19,541 lines**.
- Board byte ownership rows: **35,616**.
- CMake `build/bin` executable full export: **VERIFIED**.
- CMake executable launched from outside the repository with root `bin` removed: **VERIFIED**.
- Normal-build and CMake export trees: **byte-for-byte identical**.

## Windows verification

Verification platform: GitHub Actions `windows-latest`.

- Python helper scripts compile with Python 3: **VERIFIED**.
- CMake configure: **VERIFIED**.
- Windows C++ build: **VERIFIED**.
- `MsPacmanRipper.exe --version`: **VERIFIED**.
- Windows ZIP streaming through `tar.exe`: **VERIFIED**.
- ZIP input reaches canonical validation rather than failing in the platform loader: **VERIFIED**.
- Runnable package containing `bin/MsPacmanRipper.exe`, `scripts/`, `evidence/`, and launch/docs files: **VERIFIED**.
- Packaged executable smoke test: **VERIFIED**.

GitHub Actions intentionally contains no copyrighted Ms. Pac-Man ROM/PROM payload, so the canonical ROM set is not uploaded to the public Windows CI job.

## Existing semantic/reconstruction certification

- Inherited Pac-Man semantic family markers: **381/381**.
- Ms. Pac-Man-specific semantic family markers: **63/63**.
- Program/daughterboard physical bytes represented: **26,624/26,624**.
- Z80 CODE bytes represented: **14,242/14,242**.
- Semantic DATA bytes represented: **12,382/12,382**.
- Complete physical board files reconstructable: **13/13**.
- Previously certified SjASMPlus v1.24.0 exact reconstruction: **13/13 files, 35,616/35,616 bytes exact**.

## Repository verification

- Source tree contains no Ms. Pac-Man ROM/PROM binaries: **VERIFIED**.
- Source tree contains no generated ROM-derived disassembly tree: **VERIFIED**.
- Windows and Linux share the same core source and export pipeline.
- Public CI uses synthetic ZIP data only for Windows loader smoke testing.

## Packaging

The public repository remains source-only. The Windows CI artifact packages only project-owned executable/runtime files and ROM-free metadata; game ROM/PROM files and generated game-derived output remain excluded.
