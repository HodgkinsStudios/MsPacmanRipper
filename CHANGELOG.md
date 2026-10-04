# Changelog

All notable public changes to MsPacmanRipper are documented here.

## Unreleased

## 1.0 - 2026-10-04

### Added

- Standalone C++17 Ms. Pac-Man ROM/PROM validation and full structured disassembly pipeline.
- Canonical 13-device ROM/PROM identity validation.
- Human-readable Z80 assembly generation.
- Structured graphics, palette, sound, semantic, and physical board-ownership exports.
- Optional SjASMPlus exact reconstruction verification.
- Native Ubuntu/Linux build and run helpers.
- Native Windows 10/11 build, ZIP loading, package workflow, CMake support, and Code::Blocks target.
- Native macOS Apple Silicon and Intel builds, CMake support, Code::Blocks target, and universal package.
- Multi-stage Docker image for distro-independent Linux use.
- Published `linux/amd64` and `linux/arm64` container variants.
- GHCR tags for `latest`, `1.0`, source commit SHA, and verified architecture slices.
- Portable multi-architecture OCI archive.
- Docker/Podman helper with host UID/GID and SELinux-aware bind mounts.
- Windows, macOS, Docker, and release-readiness GitHub Actions workflows.
- Release documentation, issue forms, security policy, contribution guide, and repository hygiene rules.

### Verified

- Canonical files recognized: **13/13**.
- Canonical bytes recognized: **35,616/35,616**.
- Complete physical files reconstructable: **13/13**.
- Exact reconstruction: **35,616/35,616 bytes**.
- Native Docker amd64 build/execution: **PASS**.
- Native Docker arm64 build/execution: **PASS**.
- Multi-architecture GHCR manifest: **PASS**.
- Portable OCI artifact: **PASS**.
- Windows build/package regression: **PASS**.
- macOS Apple Silicon, Intel, and universal package regression: **PASS**.
