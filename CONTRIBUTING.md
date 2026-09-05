# Contributing

Contributions that improve correctness, portability, documentation, validation, or maintainability are welcome.

## Development environment

MsPacmanRipper targets Ubuntu/Linux with a C++17 compiler. Install the normal build dependencies with:

```bash
sudo apt install build-essential unzip python3
```

Build with:

```bash
./build.sh
```

Before submitting a change, ensure the project builds without warnings under the flags used by `build.sh`.

## ROM/PROM data

Do not commit or attach copyrighted game ROM/PROM files, generated ROM-derived disassembly trees, reconstructed game binaries, or other third-party game assets to this repository or its issue tracker.

Changes affecting disassembly or reconstruction behavior should be verified locally using a lawfully obtained canonical set. SjASMPlus may be supplied through the `SJASMPLUS` environment variable for exact reconstruction verification.

## Scope

Keep changes focused on MsPacmanRipper's standalone purpose: validating the supported Ms. Pac-Man set and producing its complete structured disassembly and reconstruction material.
