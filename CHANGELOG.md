# Changelog

All notable public changes to MsPacmanRipper are documented here.

## Unreleased

### Added

- Native Windows 10/11 support for ROM directories and ZIP input.
- `build_windows.bat` and `run_windows.bat` for command-line Windows builds and launches.
- A Code::Blocks `Release Windows` target that produces `bin/MsPacmanRipper.exe`.
- Cross-platform CMake configuration and Windows GitHub Actions build verification.

### Changed

- Replaced Unix-only directory traversal with C++17 `std::filesystem`.
- Added Windows-safe ZIP streaming through the built-in `tar.exe`.
- Made Python helper-script subprocesses and executable discovery work on Windows.

## 1.0.0 - 2026-09-04

### Added

- Standalone C++17 / Code::Blocks / Ubuntu Ms. Pac-Man full-ROM disassembler.
- Canonical 13-device ROM/PROM set validation using size, CRC32, and SHA-256 identity checks.
- Complete human-readable Z80 source generation.
- Structured graphics, palette, color lookup, waveform, and sound-control source export.
- Complete physical-board manifest and reconstruction material.
- Optional SjASMPlus reconstruction verification.
- Verified exact reconstruction of all 13 physical files and all 35,616 bytes.
- Source-only public packaging with no game ROM/PROM binaries or generated ROM-derived disassembly bundled.
