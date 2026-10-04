# MsPacmanRipper 1.0 Verification Report

**Created by Jacob Hodgkins**

## Canonical full-export regression

The canonical 13-file test set was kept local and was not uploaded to public GitHub Actions.

The exact amd64 runtime root filesystem from the generated multi-architecture OCI artifact was extracted and executed locally.

Results:

- `MsPacmanRipper 1.0`: **VERIFIED**.
- Canonical set identity: **13/13 VERIFIED**.
- Canonical bytes: **35,616/35,616 VERIFIED**.
- Full disassembly export: **VERIFIED**.
- Output files: **60**.
- `program/mspacman.asm`: **19,541 lines**.
- `manifest/board_byte_ownership.csv`: **35,616 data rows**.

This verifies the executable and runtime filesystem actually packaged by the container build, including its bundled Python/export pipeline and Linux ZIP handling.

## Docker linux/amd64 verification

GitHub Actions runner: native x86-64 Ubuntu Linux.

- Docker Buildx setup: **VERIFIED**.
- Native amd64 image build: **VERIFIED**.
- Container architecture reports `x86_64`: **VERIFIED**.
- `--version`: **VERIFIED**.
- `--help`: **VERIFIED**.
- Synthetic ZIP input reaches canonical validation: **VERIFIED**.
- `run_docker.sh` host-user execution: **VERIFIED**.
- Helper output directory host ownership: **VERIFIED**.

## Docker linux/arm64 verification

GitHub Actions runner: native ARM64 Ubuntu Linux.

- Docker Buildx setup: **VERIFIED**.
- Native arm64 image build: **VERIFIED**.
- Container architecture reports `aarch64`: **VERIFIED**.
- `--version`: **VERIFIED**.
- `--help`: **VERIFIED**.
- Synthetic ZIP input reaches canonical validation: **VERIFIED**.
- `run_docker.sh` host-user execution: **VERIFIED**.
- Helper output directory host ownership: **VERIFIED**.

## Multi-architecture publishing

The workflow publishes:

- `ghcr.io/hodgkinsstudios/mspacmanripper:latest`;
- `ghcr.io/hodgkinsstudios/mspacmanripper:1.0`;
- a source-SHA tag.

The published OCI index is verified to include both `linux/amd64` and `linux/arm64`.

A portable multi-architecture OCI archive is also produced and uploaded as the `MsPacmanRipper-linux-multiarch-oci` Actions artifact.

The public pipeline contains no Ms. Pac-Man ROM/PROM files.

## Container security/packaging boundary

- ROM files are not copied into the Docker image.
- `.dockerignore` excludes archive and ROM-style binary inputs from the build context.
- Runtime image includes only the executable, ROM-free `scripts/` and `evidence/`, documentation, Python 3, `unzip`, and runtime dependencies.
- Build tools stay in the build stage.
- `run_docker.sh` mounts the selected input read-only.
- `run_docker.sh` uses the host UID/GID for output ownership.
- SELinux bind-mount relabeling is handled by the helper when detected.
- Container `HOME` is `/tmp` for host-user execution.

## Windows regression

The Windows GitHub Actions workflow passes on 1.0:

- CMake/MSVC build: **VERIFIED**.
- executable smoke test: **VERIFIED**.
- ZIP-loader test: **VERIFIED**.
- runnable package test: **VERIFIED**.

## macOS regression

The established macOS pipeline covers:

- native Apple Silicon;
- native Intel;
- CMake builds on both;
- ZIP-loader tests;
- universal `arm64 + x86_64` package assembly;
- packaged-runtime verification.

## Existing semantic/reconstruction certification

- Inherited Pac-Man semantic family markers: **381/381**.
- Ms. Pac-Man-specific semantic family markers: **63/63**.
- Program/daughterboard physical bytes represented: **26,624/26,624**.
- Z80 CODE bytes represented: **14,242/14,242**.
- Semantic DATA bytes represented: **12,382/12,382**.
- Complete physical board files reconstructable: **13/13**.
- Previously certified SjASMPlus exact reconstruction: **13/13 files, 35,616/35,616 bytes exact**.
