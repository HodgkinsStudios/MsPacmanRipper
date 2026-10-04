# Contributing to MsPacmanRipper

Thank you for helping improve MsPacmanRipper. Contributions focused on correctness, portability, verification, documentation, build reliability, and maintainability are welcome.

## Before you start

Please keep the project boundary clear:

- do not commit or attach copyrighted Ms. Pac-Man/Pac-Man ROM or PROM data;
- do not commit generated ROM-derived disassembly trees;
- do not commit reconstructed game binaries;
- do not paste ROM bytes into issues, pull requests, logs, or screenshots.

Changes that affect disassembly/reconstruction behavior should be verified locally with a lawfully obtained compatible set.

## Supported development paths

### Ubuntu/Linux

```bash
sudo apt install build-essential unzip python3 cmake
./build.sh
```

### Windows

Use `build_windows.bat`, CMake, or the Code::Blocks **Release Windows** target.

### macOS

Use `build_macos.sh`, CMake, or the Code::Blocks **Release macOS** target.

### Docker

```bash
docker build -t mspacripper:dev .
```

## Code quality

C++ changes should remain compatible with C++17 and compile cleanly with the warning levels already enforced by the project.

For Linux, the strict local build is:

```bash
./build.sh
```

The release-readiness workflow also checks:

- version consistency;
- shell syntax;
- Python syntax;
- warnings-as-errors Linux compilation;
- `--help` and `--version`;
- accidental tracking of ROM/archive/binary payloads.

## Pull-request checklist

Before submitting:

1. Keep the change focused and explain the reason for it.
2. Build the affected platform locally when practical.
3. Run `--help` and `--version` after build-system changes.
4. Verify ZIP/directory loading after ROM-loader changes.
5. Update README/release documentation when behavior or supported platforms change.
6. Do not include local build products, IDE state, ROM data, or generated game-derived output.
7. Confirm GitHub Actions is green.

## Versioned releases

Do not change release tags or published version numbers casually. Release preparation is documented in [RELEASING.md](RELEASING.md).

## Reporting problems

Use the repository issue forms for reproducible bugs and feature requests. Security-sensitive issues should follow [SECURITY.md](SECURITY.md).
