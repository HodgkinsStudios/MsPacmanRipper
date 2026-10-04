# MsPacmanRipper

**Created by Jacob Hodgkins**  
**Version 1.3.0**

MsPacmanRipper is a standalone C++17 command-line tool for **macOS, Windows, Ubuntu/Linux, and containerized Linux environments** that produces a complete structured disassembly of the supported canonical Ms. Pac-Man arcade ROM/PROM set.

The native builds and Docker image all use the same analyzer, ROM validation, daughterboard model, semantic pipeline, and output format.

## Features

Given the canonical 13-file `mspacman.zip` or a directory containing the same 13 files, MsPacmanRipper generates a complete disassembly tree containing:

- human-readable Z80 source in `program/mspacman.asm`;
- complete code/data ownership and semantic catalogs;
- structured character and sprite graphics source;
- palette and color lookup PROM source;
- waveform and sound timing/control PROM source;
- complete 13-device board manifest and byte ownership;
- reconstruction scripts and verification material capable of rebuilding all 13 physical ROM/PROM files exactly.

The public source, release artifacts, and container image contain **no Ms. Pac-Man ROM/PROM data** and no pre-generated ROM-derived disassembly tree.

# Docker / other Linux distributions

Version 1.3.0 adds a distro-independent Linux distribution path through Docker-compatible containers.

The published image uses a Debian Bookworm Slim runtime internally, so the host distribution does not need the exact compiler, Python, or `unzip` versions used by MsPacmanRipper. A working Docker-compatible container engine is the main host requirement.

Supported published Linux architectures:

- `linux/amd64` — normal Intel/AMD 64-bit PCs;
- `linux/arm64` — 64-bit ARM Linux systems.

The same image tag automatically selects the correct architecture.

## Pull the image

```bash
docker pull ghcr.io/hodgkinsstudios/mspacmanripper:1.3.0
```

The moving tag is:

```bash
docker pull ghcr.io/hodgkinsstudios/mspacmanripper:latest
```

The 1.3.0 multi-architecture manifest is published by GitHub Actions and contains both `linux/amd64` and `linux/arm64`.

If your GHCR client requests authentication because of registry/package visibility settings, authenticate to `ghcr.io` first and repeat the pull.

## Easiest Linux usage

The repository includes `run_docker.sh`. It accepts either the ZIP file or a directory containing the 13 canonical files:

```bash
chmod +x run_docker.sh
./run_docker.sh /path/to/mspacman.zip /path/to/output
```

or:

```bash
./run_docker.sh /path/to/extracted-rom-directory /path/to/output
```

The helper:

- automatically uses `ghcr.io/hodgkinsstudios/mspacmanripper:latest` unless overridden;
- mounts ROM input read-only;
- mounts only the selected output directory read/write;
- runs the container as the current host UID/GID so generated files are not left owned by root;
- automatically adds an SELinux `:Z` mount label on SELinux hosts such as Fedora/RHEL when appropriate;
- can use Podman instead of Docker.

Use Podman:

```bash
CONTAINER_ENGINE=podman ./run_docker.sh /path/to/mspacman.zip /path/to/output
```

Pin a particular image:

```bash
MSPACMANRIPPER_IMAGE=ghcr.io/hodgkinsstudios/mspacmanripper:1.3.0 \
  ./run_docker.sh /path/to/mspacman.zip /path/to/output
```

## Direct Docker invocation

```bash
mkdir -p output

docker run --rm \
  --user "$(id -u):$(id -g)" \
  -v "$PWD/mspacman.zip:/input/mspacman.zip:ro" \
  -v "$PWD/output:/output" \
  ghcr.io/hodgkinsstudios/mspacmanripper:1.3.0 \
  /input/mspacman.zip /output
```

On an SELinux-enforcing host, use `:Z` on the bind mounts or use `run_docker.sh`, which handles that automatically.

## Build the image locally

No registry access is required if you want to build it yourself:

```bash
docker build \
  --build-arg MSPACMANRIPPER_VERSION=1.3.0 \
  -t mspacripper:local .
```

Then:

```bash
MSPACMANRIPPER_IMAGE=mspacripper:local \
  ./run_docker.sh /path/to/mspacman.zip /path/to/output
```

The Dockerfile is multi-stage. CMake/GCC/build tools stay in the build stage; the runtime stage contains the MsPacmanRipper executable plus Python 3, `unzip`, `scripts/`, `evidence/`, and the project documentation required by the normal export pipeline.

## Portable OCI archive

Every successful main-branch Docker workflow also uploads:

```text
MsPacmanRipper-linux-multiarch-oci
```

This contains a multi-architecture OCI image archive with the amd64 and arm64 image variants. It is useful for offline transfer or OCI-compatible tooling when pulling from GHCR is undesirable.

# macOS

Version 1.2.0 added first-class support for both **Apple Silicon (arm64)** and **Intel (x86_64)** Macs.

## macOS requirements

- macOS 11 or newer;
- Xcode Command Line Tools;
- Python 3;
- system `/usr/bin/unzip`.

Install the compiler tools if needed:

```bash
xcode-select --install
```

## Native macOS build

```bash
chmod +x build_macos.sh run_macos.sh
./build_macos.sh
```

Run:

```bash
./run_macos.sh /path/to/mspacman.zip /path/to/output
```

`build_macos.sh` uses Apple `clang++` through `xcrun`, the active macOS SDK, C++17, and a macOS 11 deployment target.

## Code::Blocks on macOS

Open `MsPacmanRipper.cbp` and use the **Release macOS** target with a configured Clang compiler.

## CMake on macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build --config Release
./build/bin/MsPacmanRipper /path/to/mspacman.zip /path/to/output
```

## Universal Intel + Apple Silicon package

The macOS GitHub Actions workflow builds the project natively on both Apple Silicon and Intel, verifies each architecture independently, and combines the verified slices with `lipo`.

The final CI artifact is:

```text
MsPacmanRipper-macOS-universal
```

The universal executable is ad-hoc signed rather than Developer ID signed/notarized.

# Windows

## Requirements

- Windows 10 or Windows 11;
- Python 3 available as `python` or `python3`;
- for source builds: Code::Blocks + MinGW/GCC, another `g++` toolchain, Visual Studio Developer Command Prompt, or CMake;
- Windows `tar.exe`.

## Build and run

```bat
build_windows.bat
run_windows.bat "C:\path\to\mspacman.zip" "C:\path\to\output"
```

In Code::Blocks, open `MsPacmanRipper.cbp` and build the **Release Windows** target.

## CMake on Windows

```bat
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
build\bin\MsPacmanRipper.exe "C:\path\to\mspacman.zip" "C:\path\to\output"
```

# Native Ubuntu/Linux

Install the normal build/runtime requirements:

```bash
sudo apt install build-essential unzip python3
```

Build and run:

```bash
./build.sh
./run.sh /path/to/mspacman.zip /path/to/output
```

For Fedora, Arch, openSUSE, Alpine, and other Linux distributions, the Docker path is the supported distribution-independent option if you do not want to adapt the native package names/toolchain.

# Cross-platform CMake

```text
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The executable is normally `build/bin/MsPacmanRipper` on macOS/Linux and `build\bin\MsPacmanRipper.exe` on Windows.

# Verification status

Version 1.3.0 verification includes:

- canonical files validated: **13/13**;
- canonical bytes validated: **35,616/35,616**;
- full canonical export from the packaged Docker amd64 root filesystem: **VERIFIED**;
- Docker canonical output files: **60**;
- Docker canonical `program/mspacman.asm`: **19,541 lines**;
- Docker canonical board byte-ownership rows: **35,616**;
- native `linux/amd64` Docker build and execution: **VERIFIED**;
- native `linux/arm64` Docker build and execution: **VERIFIED**;
- Docker ZIP-loader validation path on both architectures: **VERIFIED**;
- host-UID/GID `run_docker.sh` helper on both architectures: **VERIFIED**;
- published multi-architecture GHCR manifest: **VERIFIED**;
- portable multi-architecture OCI artifact: **VERIFIED**;
- Windows CI regression on 1.3.0: **VERIFIED**;
- macOS Apple Silicon/Intel/universal regression: **VERIFIED**.

The established exact-reconstruction certification remains:

- physical files reconstructed: **13/13**;
- physical bytes reconstructed: **35,616/35,616**;
- byte-exact comparison: **verified**;
- Z80 CODE bytes represented: **14,242/14,242**;
- semantic DATA bytes represented: **12,382/12,382**;
- program/daughterboard physical bytes represented: **26,624/26,624**.

Public GitHub Actions does not contain or redistribute copyrighted Ms. Pac-Man ROM/PROM data. Public container CI therefore uses synthetic non-canonical ZIP data to test archive loading, while the complete canonical regression is performed separately.

See `TEST_REPORT.md` for the detailed verification record.

# Canonical input

The supported set contains exactly these 13 members:

`pacman.6e`, `pacman.6f`, `pacman.6h`, `pacman.6j`, `u5`, `u6`, `u7`, `5e`, `5f`, `82s123.7f`, `82s126.4a`, `82s126.1m`, `82s126.3m`.

MsPacmanRipper validates file size, CRC32, and SHA-256 before disassembling and rejects an incomplete or non-canonical set.

# Help and version

macOS/Linux:

```bash
./bin/MsPacmanRipper --help
./bin/MsPacmanRipper --version
```

Windows:

```bat
bin\MsPacmanRipper.exe --help
bin\MsPacmanRipper.exe --version
```

Docker:

```bash
docker run --rm ghcr.io/hodgkinsstudios/mspacmanripper:1.3.0 --help
docker run --rm ghcr.io/hodgkinsstudios/mspacmanripper:1.3.0 --version
```

# Optional immediate reconstruction verification

SjASMPlus is optional for normal native export. If `SJASMPLUS` points to a SjASMPlus executable, the exporter performs the exact reconstruction verification.

For the container image, SjASMPlus is not bundled in the standard runtime image.

# Repository layout

```text
MsPacmanRipper/
├── .github/workflows/     Windows, macOS, and Docker verification/publishing
├── evidence/              ROM-free semantic metadata used by the exporter
├── scripts/               structured export and reconstruction verification tools
├── src/                   C++17 source
├── Dockerfile             multi-stage Linux container image
├── .dockerignore          ROM/build-output-safe container build context exclusions
├── run_docker.sh          Docker/Podman launcher with UID/GID + SELinux handling
├── CMakeLists.txt         cross-platform CMake build
├── MsPacmanRipper.cbp     Linux, Windows, and macOS Code::Blocks targets
├── VERSION                release version
├── build.sh               Ubuntu/Linux build
├── run.sh                 Ubuntu/Linux launcher
├── build_windows.bat      Windows build
├── run_windows.bat        Windows launcher
├── build_macos.sh         native macOS Apple Clang build
└── run_macos.sh           macOS launcher
```

# Implementation notes

The ROM loader uses C++17 `std::filesystem` on all supported platforms. ZIP streaming uses `unzip` on native/container Linux, the built-in `tar.exe` on Windows, and system `/usr/bin/unzip` on macOS.

The container runtime sets `HOME=/tmp`, disables Python bytecode generation, and keeps ROM input outside the image through read-only bind mounts.

# License and third-party material

MsPacmanRipper source code is released under the MIT License. See `LICENSE`.

No game ROM/PROM data is distributed with the project, the Docker image, or CI artifacts. See `LEGAL.md`.

# Contributing and security

See `CONTRIBUTING.md` and `SECURITY.md`.
