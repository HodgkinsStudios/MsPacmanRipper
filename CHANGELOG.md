# Changelog

All notable public changes to MsPacmanRipper are documented here.

## Unreleased

## 1.1.0 - 2026-10-04

### Added

- Native Windows 10/11 support for ROM directories and ZIP input.
- `build_windows.bat` and `run_windows.bat` for Windows command-line builds and launches.
- A Code::Blocks **Release Windows** target producing `bin/MsPacmanRipper.exe`.
- Cross-platform CMake configuration.
- Windows GitHub Actions build, Python, ZIP-loader, and package verification.
- A runnable Windows CI artifact containing the executable and required ROM-free runtime assets.

### Changed

- Replaced Unix-only directory traversal with C++17 `std::filesystem`.
- Added Windows-safe pipe handling and ZIP streaming through the built-in `tar.exe`.
- Made Python helper-script subprocesses portable to Windows.
- Made project-root discovery walk ancestor directories so nested CMake build layouts work.
- Made semantic verification use the active MsPacmanRipper executable instead of assuming a fixed `bin` path.
- Updated README, release notes, source manifest, and verification report for Windows support.

### Verified

- Canonical 13-file / 35,616-byte set validates with the current source.
- Normal full export produces 60 files and a 19,541-line primary assembly source.
- Normal and CMake full-export trees are byte-for-byte identical.
- Current `windows-latest` CMake/MSVC build and Windows ZIP-loader smoke test pass.

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
