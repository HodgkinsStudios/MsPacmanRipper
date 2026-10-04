# MsPacmanRipper 1.3.0

**Created by Jacob Hodgkins**

Version 1.3.0 is the container/Linux-distribution release. It adds a tested Docker/OCI distribution path for running the existing MsPacmanRipper pipeline consistently across Docker-capable Linux distributions.

## Docker / OCI support

- Added a multi-stage `Dockerfile`.
- Runtime image uses Debian Bookworm Slim with Python 3 and `unzip`.
- Compiler, CMake, and build tools remain outside the runtime stage.
- Added `.dockerignore` rules that exclude local archives, ROM-style binary files, build output, and repository metadata from the Docker build context.
- Added `run_docker.sh` for easy ZIP/directory input and output mounting.
- Helper mounts ROM input read-only and output read/write.
- Helper runs the container using the invoking Linux user's UID/GID.
- Helper detects SELinux and adds `:Z` bind-mount labeling where appropriate.
- Helper supports Docker or Podman.
- Container runtime uses `HOME=/tmp` to support non-root/host-UID execution cleanly.

## Published architectures

GitHub Actions builds and executes the image natively on:

- `linux/amd64`;
- `linux/arm64`.

The workflow then publishes a single multi-architecture image:

```text
ghcr.io/hodgkinsstudios/mspacmanripper:1.3.0
ghcr.io/hodgkinsstudios/mspacmanripper:latest
```

It also uploads a portable multi-architecture OCI archive artifact named:

```text
MsPacmanRipper-linux-multiarch-oci
```

## Verification

Docker CI verifies on both native CPU architectures:

- image build: **PASS**;
- reported architecture: **PASS**;
- `--version`: **PASS**;
- `--help`: **PASS**;
- ZIP input reaches canonical validation: **PASS**;
- host-UID/GID helper execution: **PASS**;
- helper output directory remains owned by the host user: **PASS**.

Publishing verification:

- GHCR multi-architecture publish: **PASS**;
- manifest contains `linux/amd64`: **PASS**;
- manifest contains `linux/arm64`: **PASS**;
- portable OCI archive generation/upload: **PASS**.

The packaged amd64 OCI root filesystem was also extracted locally and run with the supplied canonical Ms. Pac-Man set without uploading those ROMs to public CI:

- `MsPacmanRipper 1.3.0`: **PASS**;
- canonical files: **13/13**;
- canonical bytes: **35,616/35,616**;
- full export: **PASS**;
- output files: **60**;
- `program/mspacman.asm`: **19,541 lines**;
- board byte ownership: **35,616 rows**.

Windows and macOS regression workflows remain green on 1.3.0.

## Existing reconstruction certification

The established exact reconstruction certification remains 13/13 physical files and 35,616/35,616 bytes exact when SjASMPlus verification is requested.
