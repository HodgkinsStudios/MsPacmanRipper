# MsPacmanRipper 1.2.0

**Created by Jacob Hodgkins**

Version 1.2.0 is the macOS portability release. MsPacmanRipper now supports macOS, Windows, and Ubuntu/Linux from the same C++17 and Python export pipeline.

## macOS port

- Added native Apple Silicon (`arm64`) support.
- Added native Intel Mac (`x86_64`) support.
- Added `build_macos.sh` and `run_macos.sh`.
- Added a Code::Blocks **Release macOS** target using Clang.
- Added explicit macOS ZIP loading through the system `/usr/bin/unzip`.
- The native build uses Apple `clang++` from `xcrun` and the active macOS SDK.
- Added native macOS CMake verification on both CPU architectures.
- Added native ZIP-loader smoke tests on both CPU architectures.
- Added checks preventing accidental Homebrew/MacPorts runtime-library dependencies.
- Added ad-hoc code signing and signature verification.
- Added a CI-produced universal `arm64 + x86_64` Mach-O binary assembled from separately verified native slices.
- Added a runnable universal `.tar.gz` package containing the binary, required exporter/evidence runtime assets, source, and macOS build scripts.

The CI artifact is ad-hoc signed rather than Developer ID signed/notarized because the public repository does not contain Apple signing credentials.

## Verification

The final macOS workflow passed on both Apple Silicon and Intel macOS runners:

- direct Apple Clang build: **PASS on arm64 and x86_64**;
- CMake build: **PASS on arm64 and x86_64**;
- Python helper validation: **PASS on both**;
- ZIP input reaches canonical validation: **PASS on both**;
- architecture checks: **PASS**;
- code-signature checks: **PASS**;
- universal binary assembly: **PASS**;
- packaged-runtime smoke test: **PASS**.

The exact 1.2.0 source bundled in the resulting Mac artifact was then rebuilt and tested with the supplied canonical set:

- **13/13** canonical files;
- **35,616/35,616** canonical bytes;
- **60** normal export files;
- **19,541** lines in `program/mspacman.asm`;
- **35,616** rows in `manifest/board_byte_ownership.csv`.

The Windows workflow also remains green on the same 1.2.0 source.

## Existing reconstruction certification

The established round-trip certification remains 13/13 physical files and 35,616/35,616 bytes exact when SjASMPlus verification is requested.

See `README.md`, `TEST_REPORT.md`, and `LEGAL.md`.
