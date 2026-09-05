# MsPacmanRipper 1.0.0 Release Verification Report

**Created by Jacob Hodgkins**

Test platform: Ubuntu/Linux, GCC C++17.

## Release verification

- Release build with `-O2 -Wall -Wextra -Wpedantic -Werror`: **VERIFIED**.
- `--help` command: **VERIFIED**.
- `--version` command reporting `MsPacmanRipper 1.0.0`: **VERIFIED**.
- Canonical 13-member `mspacman.zip` validation: **VERIFIED**.
- Standalone complete full-disassembly export: **VERIFIED**.
- Normal export output files: **60**.
- Immediate SjASMPlus verification export files: **79**, including local reconstruction verification artifacts.
- Primary human-readable source: `program/mspacman.asm`, **19,541 lines**.
- Inherited Pac-Man semantic family markers: **381/381**.
- Ms. Pac-Man-specific semantic family markers: **63/63**.
- Program/daughterboard physical bytes represented: **26,624/26,624**.
- Z80 CODE bytes represented: **14,242/14,242**.
- Semantic DATA bytes represented: **12,382/12,382**.
- Complete physical board files reconstructable: **13/13**.
- SjASMPlus v1.24.0 assembly and complete-board reconstruction test: **VERIFIED**.
- Exact physical reconstruction: **13/13 files, 35,616/35,616 bytes exact**.

## Repository verification

- Source tree contains no Ms. Pac-Man ROM/PROM binaries: **VERIFIED**.
- Source tree contains no generated ROM-derived disassembly tree: **VERIFIED**.
- Source tree contains no compiler output: **VERIFIED**.
- Release terminology audit: **VERIFIED**.
- GitHub workflow performs a clean Ubuntu build and command-line metadata checks without ROM data.

## Packaging

The public release package is source-only. ROM/PROM files and generated game-derived outputs are intentionally excluded.
