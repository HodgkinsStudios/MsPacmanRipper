# MsPacmanRipper

[![Windows build](https://github.com/HodgkinsStudios/MsPacmanRipper/actions/workflows/windows.yml/badge.svg)](https://github.com/HodgkinsStudios/MsPacmanRipper/actions/workflows/windows.yml)
[![macOS build](https://github.com/HodgkinsStudios/MsPacmanRipper/actions/workflows/macos.yml/badge.svg)](https://github.com/HodgkinsStudios/MsPacmanRipper/actions/workflows/macos.yml)
[![Docker build](https://github.com/HodgkinsStudios/MsPacmanRipper/actions/workflows/docker.yml/badge.svg)](https://github.com/HodgkinsStudios/MsPacmanRipper/actions/workflows/docker.yml)
[![Release readiness](https://github.com/HodgkinsStudios/MsPacmanRipper/actions/workflows/release-readiness.yml/badge.svg)](https://github.com/HodgkinsStudios/MsPacmanRipper/actions/workflows/release-readiness.yml)

**Created by Jacob Hodgkins · Version 1.3.0 · MIT licensed**

MsPacmanRipper is a cross-platform C++17 command-line tool that validates the supported canonical Ms. Pac-Man arcade ROM/PROM set and produces a complete, structured disassembly package: Z80 source, semantic catalogs, graphics and PROM data, physical-device ownership maps, and reconstruction material.

The repository, CI artifacts, and container images contain **no Ms. Pac-Man ROM/PROM data**. You supply your own lawfully obtained compatible set.

## Platform support

| Platform | Supported form | Architectures | CI verified |
| --- | --- | --- | --- |
| Windows 10/11 | Native executable / CMake / Code::Blocks | x86-64 | Yes |
| macOS 11+ | Native / CMake / universal package | Apple Silicon + Intel | Yes |
| Ubuntu/Linux | Native GCC/CMake build | x86-64 | Yes |
| Other Linux distributions | Docker or Podman | linux/amd64 + linux/arm64 | Yes |

The Docker image is the recommended distribution-independent Linux option for Fedora, RHEL-family, Arch, openSUSE, Debian derivatives, and other Docker/Podman-capable systems.

## Quick start

### Docker / Podman

Pull the current release image:

```bash
docker pull ghcr.io/hodgkinsstudios/mspacmanripper:1.3.0
```

Then use the repository helper:

```bash
chmod +x run_docker.sh
./run_docker.sh /path/to/mspacman.zip /path/to/output
```

For Podman:

```bash
CONTAINER_ENGINE=podman ./run_docker.sh /path/to/mspacman.zip /path/to/output
```

The helper mounts ROM input read-only, writes only to the selected output directory, runs with the host UID/GID, and handles SELinux bind-mount relabeling when needed.

### Windows

Requirements: Windows 10/11 and Python 3.

```bat
build_windows.bat
run_windows.bat "C:\path\to\mspacman.zip" "C:\path\to\output"
```

CMake is also supported:

```bat
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
build\bin\MsPacmanRipper.exe "C:\path\to\mspacman.zip" "C:\path\to\output"
```

### macOS

Requirements: macOS 11+, Python 3, and Xcode Command Line Tools.

```bash
xcode-select --install
chmod +x build_macos.sh run_macos.sh
./build_macos.sh
./run_macos.sh /path/to/mspacman.zip /path/to/output
```

The CI package is a universal `arm64 + x86_64` Mach-O. It is ad-hoc signed, not Developer ID notarized.

### Ubuntu/Linux

```bash
sudo apt install build-essential unzip python3
./build.sh
./run.sh /path/to/mspacman.zip /path/to/output
```

## Input

MsPacmanRipper accepts either:

- a ZIP archive containing the canonical files; or
- a directory containing the canonical files.

The supported set contains exactly:

```text
pacman.6e
pacman.6f
pacman.6h
pacman.6j
u5
u6
u7
5e
5f
82s123.7f
82s126.4a
82s126.1m
82s126.3m
```

Before disassembly, each member is checked against the expected size, CRC32, and SHA-256 identity. Incomplete or non-canonical input is rejected.

## Output

A successful run creates a structured output tree. Key files include:

```text
output/
├── program/
│   └── mspacman.asm
├── manifest/
│   └── board_byte_ownership.csv
├── graphics/
├── palette/
├── sound/
├── semantic/
└── reconstruction/
```

The complete export includes:

- human-readable Z80 assembly;
- code/data ownership and semantic catalogs;
- character and sprite graphics source;
- palette and color lookup PROM source;
- waveform and sound timing/control PROM source;
- a complete 13-device board manifest;
- byte ownership for the full physical set;
- reconstruction source and optional exact round-trip verification.

## Docker image

Published image:

```text
ghcr.io/hodgkinsstudios/mspacmanripper:1.3.0
ghcr.io/hodgkinsstudios/mspacmanripper:latest
```

Supported platforms:

```text
linux/amd64
linux/arm64
```

Direct invocation:

```bash
mkdir -p output

docker run --rm \
  --user "$(id -u):$(id -g)" \
  -v "$PWD/mspacman.zip:/input/mspacman.zip:ro" \
  -v "$PWD/output:/output" \
  ghcr.io/hodgkinsstudios/mspacmanripper:1.3.0 \
  /input/mspacman.zip /output
```

Every successful Docker workflow also creates a portable `MsPacmanRipper-linux-multiarch-oci` Actions artifact.

## Build systems

The project intentionally supports several workflows:

- `build.sh` — native GCC Linux build;
- `build_windows.bat` — Windows command-line build;
- `build_macos.sh` — Apple Clang native macOS build;
- `CMakeLists.txt` — cross-platform CMake build;
- `MsPacmanRipper.cbp` — Code::Blocks project;
- `Dockerfile` — reproducible Linux container build.

Cross-platform CMake:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Optional exact reconstruction verification

SjASMPlus is not required for normal disassembly.

If `SJASMPLUS` points to a SjASMPlus executable, the export pipeline can perform immediate exact reconstruction verification:

```bash
SJASMPLUS=/path/to/sjasmplus ./bin/MsPacmanRipper /path/to/mspacman.zip /path/to/output
```

SjASMPlus is not bundled in the standard Docker runtime image.

## Repository layout

```text
MsPacmanRipper/
├── .github/               CI and issue templates
├── evidence/              ROM-free semantic reference data
├── scripts/               export and verification pipeline
├── src/                   C++17 implementation
├── Dockerfile
├── CMakeLists.txt
├── MsPacmanRipper.cbp
├── build.sh
├── build_windows.bat
├── build_macos.sh
├── run.sh
├── run_windows.bat
├── run_macos.sh
├── run_docker.sh
├── TEST_REPORT.md
├── RELEASE_NOTES.md
├── CHANGELOG.md
├── CONTRIBUTING.md
├── SECURITY.md
├── LEGAL.md
└── LICENSE
```

## Release process

The repository includes [RELEASING.md](RELEASING.md) with the release checklist, version consistency requirements, CI gates, tag convention, and artifact procedure.

Release tags use the form `vMAJOR.MINOR.PATCH`, and tag builds must match the value in `VERSION`.

## Contributing

Contributions that improve correctness, portability, validation, documentation, or maintainability are welcome. Read [CONTRIBUTING.md](CONTRIBUTING.md) before opening a pull request.

Do **not** attach ROM/PROM files, generated ROM-derived output, or reconstructed game binaries to issues or pull requests.

## Security

See [SECURITY.md](SECURITY.md) for supported versions and private-reporting guidance.

## Legal

MsPacmanRipper is an independent project and is not affiliated with or endorsed by the owners of Ms. Pac-Man, Pac-Man, or related trademarks/copyrights.

The MIT License applies to this project's original software. It does not grant rights to third-party game data, artwork, music, trademarks, or generated ROM-derived material. See [LEGAL.md](LEGAL.md).

## License

MsPacmanRipper is released under the [MIT License](LICENSE).
