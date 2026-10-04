# Changelog

All notable public changes to MsPacmanRipper are documented here.

## Unreleased

## 1.2.0 - 2026-10-04

### Added

- Native macOS support for Apple Silicon (`arm64`) and Intel (`x86_64`).
- `build_macos.sh` and `run_macos.sh`.
- A Code::Blocks **Release macOS** target.
- macOS GitHub Actions builds on native Apple Silicon and native Intel runners.
- Independent direct-Clang and CMake validation for both Mac architectures.
- A universal `arm64 + x86_64` CI package assembled from verified native slices.
- Ad-hoc code signing and signature validation for the Mac executable.
- macOS package/runtime smoke testing and architecture verification.

### Changed

- macOS ZIP input now uses the system `/usr/bin/unzip` explicitly.
- The macOS build script uses Apple `clang++` and SDK discovery through `xcrun`.
- The macOS build no longer trusts a generic inherited `CXX` environment variable; a custom compiler can be selected explicitly with `MSPACMANRIPPER_CXX`.
- Public documentation, release notes, source manifest, and verification report now cover macOS.

### Verified

- Apple Silicon direct build, CMake build, and ZIP loader: **PASS**.
- Intel direct build, CMake build, and ZIP loader: **PASS**.
- Universal Mach-O assembly, code signature, package extraction, and packaged runtime: **PASS**.
- Exact 1.2.0 package source with the supplied canonical set: **13/13 files, 35,616/35,616 bytes, 60 export files**.
- Windows CI remains green on 1.2.0.

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
