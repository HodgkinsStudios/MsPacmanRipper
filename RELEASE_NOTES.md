# MsPacmanRipper 1.0

**Created by Jacob Hodgkins**

Version 1.0 is the first release-ready cross-platform version of MsPacmanRipper. It combines the complete canonical Ms. Pac-Man ROM/PROM validation and structured disassembly pipeline with native Windows and macOS support plus a Docker/OCI distribution path for Linux systems.

## Core disassembly pipeline

- Validates the canonical 13-file Ms. Pac-Man ROM/PROM set before processing.
- Produces complete human-readable Z80 source.
- Exports structured graphics, palette, sound, semantic, and board-ownership data.
- Produces reconstruction material for all 13 physical devices.
- Supports optional exact reconstruction verification through SjASMPlus.
- Keeps ROM/PROM data and generated ROM-derived output out of the public source tree and release artifacts.

## Windows

- Native Windows 10/11 support.
- ZIP and directory input.
- `build_windows.bat` and `run_windows.bat`.
- Cross-platform CMake support.
- Code::Blocks **Release Windows** target.
- GitHub Actions build, ZIP-loader smoke test, package assembly, packaged-runtime test, and artifact upload.

## macOS

- Native Apple Silicon (`arm64`) and Intel (`x86_64`) support.
- `build_macos.sh` and `run_macos.sh`.
- Code::Blocks **Release macOS** target.
- Native CI builds on both Mac architectures.
- Universal `arm64 + x86_64` package assembly.
- Ad-hoc code signing and signature verification.

## Linux / Docker / OCI

- Native Ubuntu/Linux GCC/CMake build path.
- Multi-stage Docker image for distro-independent Linux use.
- Published `linux/amd64` and `linux/arm64` image variants.
- Docker/Podman helper with host UID/GID output ownership.
- SELinux-aware bind-mount handling.
- Debian Bookworm Slim runtime with Python 3 and `unzip`.
- Build tools remain outside the runtime image.
- Portable multi-architecture OCI archive artifact.

Published container tags:

```text
ghcr.io/hodgkinsstudios/mspacmanripper:1.0
ghcr.io/hodgkinsstudios/mspacmanripper:latest
```

## Release engineering

- Windows, macOS, Docker, and release-readiness GitHub Actions workflows.
- Native Docker verification on amd64 and arm64 runners.
- Multi-architecture manifest assembled from the exact native image slices tested by CI.
- Release-readiness checks for version consistency, repository hygiene, shell/Python syntax, strict Linux compilation, and CLI smoke tests.
- Cross-platform line-ending policy through `.gitattributes`.
- Build output, local ROM/archive data, IDE state, and generated artifacts excluded through repository ignore rules.
- Maintainer release checklist in `RELEASING.md`.

## Verification

The established verification record includes:

- canonical files: **13/13**;
- canonical bytes: **35,616/35,616**;
- complete physical files reconstructable: **13/13**;
- exact reconstruction: **35,616/35,616 bytes**;
- Docker amd64 and arm64 execution: **PASS**;
- Windows regression/package path: **PASS**;
- Apple Silicon, Intel, and universal macOS package paths: **PASS**.

See `TEST_REPORT.md` for the detailed verification record.
