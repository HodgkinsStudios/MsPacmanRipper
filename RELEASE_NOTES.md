# MsPacmanRipper 1.0.0

Initial public release of MsPacmanRipper, a standalone C++17 / Code::Blocks / Ubuntu command-line disassembler for the canonical 13-device Ms. Pac-Man arcade ROM/PROM set.

## Highlights

- Validates all 13 expected ROM/PROM files before processing.
- Produces complete human-readable Z80 source in `program/mspacman.asm`.
- Exports structured graphics, color, palette, waveform, and sound-control source.
- Includes complete board reconstruction tooling and verification reports.
- Supports optional SjASMPlus reconstruction verification.
- Verified exact reconstruction: 13/13 physical files and 35,616/35,616 bytes.
- Public source package contains no Ms. Pac-Man ROM/PROM binaries and no generated ROM-derived disassembly tree.

## Requirements

Ubuntu/Linux, a C++17 compiler, Python 3, and `unzip`. SjASMPlus is optional unless immediate exact reconstruction verification is desired.

## Basic use

```bash
./build.sh
./bin/MsPacmanRipper /path/to/mspacman.zip /path/to/output
```

See `README.md` and `LEGAL.md` before use.
