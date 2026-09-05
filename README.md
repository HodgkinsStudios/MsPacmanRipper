# MsPacmanRipper

**Created by Jacob Hodgkins**  
**Version 1.0.0**

MsPacmanRipper is a standalone C++17 / Code::Blocks / Ubuntu command-line tool that produces a complete structured disassembly of the supported canonical Ms. Pac-Man arcade ROM/PROM set.

## Features

Given the canonical 13-file `mspacman.zip` or a directory containing the same 13 files, MsPacmanRipper generates a complete disassembly tree containing:

- human-readable Z80 source in `program/mspacman.asm`;
- complete code/data ownership and semantic catalogs;
- structured character and sprite graphics source;
- palette and color lookup PROM source;
- waveform and sound timing/control PROM source;
- complete 13-device board manifest and byte ownership;
- reconstruction scripts and verification material capable of rebuilding all 13 physical ROM/PROM files exactly.

The public source package contains **no Ms. Pac-Man ROM/PROM data** and no pre-generated ROM-derived disassembly tree.

## Verified reconstruction

Release 1.0.0 was verified against the supported canonical set:

- physical files reconstructed: **13/13**;
- physical bytes reconstructed: **35,616/35,616**;
- byte-exact comparison: **verified**;
- Z80 CODE bytes represented: **14,242/14,242**;
- semantic DATA bytes represented: **12,382/12,382**;
- program/daughterboard physical bytes represented: **26,624/26,624**.

See `TEST_REPORT.md` for the release verification record.

## Canonical input

The supported set contains exactly these 13 members:

`pacman.6e`, `pacman.6f`, `pacman.6h`, `pacman.6j`, `u5`, `u6`, `u7`, `5e`, `5f`, `82s123.7f`, `82s126.4a`, `82s126.1m`, `82s126.3m`.

MsPacmanRipper validates file size, CRC32, and SHA-256 before disassembling and rejects an incomplete or non-canonical set.

## Ubuntu dependencies

```bash
sudo apt install build-essential unzip python3
```

SjASMPlus is optional for normal export. If `SJASMPLUS` points to a SjASMPlus executable, the exporter also performs immediate exact 13-file reconstruction verification.

## Build

```bash
./build.sh
```

Alternatively, open `MsPacmanRipper.cbp` in Code::Blocks and build the `Release` target.

## Use

```bash
./bin/MsPacmanRipper /path/to/mspacman.zip /path/to/output
```

or:

```bash
./run.sh /path/to/mspacman.zip /path/to/output
```

The output directory is recreated by the exporter on each run.

### Help and version

```bash
./bin/MsPacmanRipper --help
./bin/MsPacmanRipper --version
```

## Optional immediate reconstruction verification

```bash
SJASMPLUS=/path/to/sjasmplus ./bin/MsPacmanRipper /path/to/mspacman.zip /path/to/output
```

A successful verified export reports an exact `13/13 files` and `35616/35616 bytes` reconstruction. Immediate verification adds local rebuild/verification artifacts to the selected output directory; these are not part of the public source package.

## Repository layout

```text
MsPacmanRipper/
├── .github/workflows/   GitHub build verification
├── evidence/            ROM-free semantic metadata used by the exporter
├── scripts/             structured export and reconstruction verification tools
├── src/                 C++17 source
├── MsPacmanRipper.cbp   Code::Blocks project
├── VERSION              release version
├── build.sh             Ubuntu command-line build
└── run.sh               build-if-needed launcher
```

## Implementation note

The public program has one normal job and one normal invocation: complete Ms. Pac-Man disassembly. Two `--internal-*` subprocess entry points are implementation plumbing used by the bundled structured exporter to invoke the C++ analyzer and daughterboard logical-image generator.

## Output stability

The exporter uses stable descriptive filenames for analysis, semantic evidence, reconstruction material, and verification reports. Generated source remains fully reconstructable and is validated against the complete supported 13-device board set.

## License and third-party material

MsPacmanRipper source code is released under the MIT License. See `LICENSE`.

No game ROM/PROM data is distributed with the project. See `LEGAL.md` for the project disclaimer and the boundary between the MsPacmanRipper source license and third-party game material.

## Contributing and security

See `CONTRIBUTING.md` for contribution guidance and `SECURITY.md` for security reporting guidance.
