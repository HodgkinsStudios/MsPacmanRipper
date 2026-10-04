# Changelog

All notable public changes to MsPacmanRipper are documented here.

## Unreleased

## 1.3.0 - 2026-10-04

### Added

- Multi-stage Docker image for distro-independent Linux use.
- Published `linux/amd64` and `linux/arm64` container variants.
- GHCR multi-architecture tags for `latest`, `1.3.0`, and source commit SHA.
- Portable multi-architecture OCI archive artifact.
- `run_docker.sh` Docker/Podman helper.
- SELinux-aware bind-mount handling.
- Native GitHub Actions container testing on x86-64 and ARM64 Linux runners.
- Host UID/GID helper/output-ownership verification.

### Changed

- Container runtime sets `HOME=/tmp` for clean host-user execution.
- Docker build context excludes archives, ROM-style binary files, build output, and repository metadata.
- Documentation now treats containers as the supported distro-independent path for other Linux distributions.

### Verified

- Native amd64 container build/execution: **PASS**.
- Native arm64 container build/execution: **PASS**.
- ZIP-loader validation path on both architectures: **PASS**.
- Host-user helper on both architectures: **PASS**.
- Multi-architecture GHCR manifest: **PASS**.
- Portable OCI artifact: **PASS**.
- Packaged amd64 OCI root filesystem with supplied canonical set: **13/13 files, 35,616/35,616 bytes, 60 output files**.
- Windows regression: **PASS**.
- macOS regression: **PASS**.

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

## 1.1.0 - 2026-10-04

### Added

- Native Windows 10/11 support for ROM directories and ZIP input.
- `build_windows.bat` and `run_windows.bat`.
- Code::Blocks **Release Windows** target.
- Cross-platform CMake configuration.
- Windows GitHub Actions verification and runnable package.

## 1.0.0 - 2026-09-04

### Added

- Standalone C++17 / Code::Blocks / Ubuntu Ms. Pac-Man full-ROM disassembler.
- Canonical 13-device ROM/PROM validation.
- Complete human-readable Z80 source generation.
- Structured graphics, palette, sound, semantic, and board ownership exports.
- Optional SjASMPlus reconstruction verification.
- Verified exact reconstruction of all 13 physical files and all 35,616 bytes.
