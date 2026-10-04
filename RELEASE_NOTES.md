# MsPacmanRipper 1.1.0

**Created by Jacob Hodgkins**

Version 1.1.0 is the Windows portability release. MsPacmanRipper now supports Windows 10/11 and Ubuntu/Linux from the same C++17 and Python export pipeline.

## Windows port

- Added native Windows ROM-directory and ZIP input support.
- Replaced Unix-only directory traversal with C++17 `std::filesystem`.
- Added Windows ZIP streaming through the built-in `tar.exe`.
- Added Windows-safe `_popen`/binary-pipe handling.
- Added `build_windows.bat` and `run_windows.bat`.
- Added a Code::Blocks **Release Windows** target.
- Added cross-platform CMake configuration.
- Added `windows-latest` GitHub Actions compilation and smoke tests.
- Added a runnable Windows CI artifact containing the executable plus required `scripts/` and `evidence/` runtime assets.
- Updated helper-script launching so Python files work without Unix executable-bit semantics.
- Updated semantic verification so it follows the executable that actually launched the export.
- Updated project-root discovery so nested CMake layouts such as `build/bin` work even when launched outside the repository root.

## Verification

The canonical 13-file set was validated and fully exported with the current shared source:

- 13/13 canonical files and 35,616/35,616 bytes validated;
- 60 normal export files produced;
- `program/mspacman.asm` contains 19,541 lines;
- `manifest/board_byte_ownership.csv` contains 35,616 byte-owner rows;
- normal and CMake exports were byte-for-byte identical;
- the CMake executable was exercised from outside the repository with the root `bin` executable absent;
- the current Windows CI build, Python checks, executable smoke test, ZIP-loader smoke test, and packaged-runtime smoke test pass.

The project does not upload or redistribute Ms. Pac-Man ROM/PROM data in GitHub Actions.

## Existing reconstruction certification

The established round-trip certification remains 13/13 physical files and 35,616/35,616 bytes exact when SjASMPlus verification is requested.

See `README.md`, `TEST_REPORT.md`, and `LEGAL.md`.
