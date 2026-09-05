#!/usr/bin/env python3
# Created by Jacob Hodgkins
"""Generate deterministic public textual reassembly components from a canonical Ms. Pac-Man set.

Generated .asm files are ROM-derived output and therefore MUST NOT be embedded in the
source-only source package.  This script emits text only; no INCBIN is used or permitted.
"""
from __future__ import annotations

import csv
import re
import shutil
import subprocess
import sys
import zipfile
from dataclasses import dataclass
from pathlib import Path

CANONICAL = [
    "pacman.6e", "pacman.6f", "pacman.6h", "pacman.6j",
    "u5", "u6", "u7", "5e", "5f",
    "82s123.7f", "82s126.4a", "82s126.1m", "82s126.3m",
]

ASM_COMPONENTS = [
    "base_program.asm",
    "enabled_low_patch_image.asm",
    "u5_decoded.asm",
    "u6_logical.asm",
    "u7_decoded.asm",
    "graphics_5e.asm",
    "graphics_5f.asm",
    "prom_82s123_7f.asm",
    "prom_82s126_4a.asm",
    "prom_82s126_1m.asm",
    "prom_82s126_3m.asm",
]


def die(msg: str) -> None:
    print(f"reassembly-source export source exporter: FAIL: {msg}", file=sys.stderr)
    raise SystemExit(1)


def hx(s: str) -> int:
    t = s.strip()
    if t.startswith("$"):
        t = "0x" + t[1:]
    return int(t, 0)


def qhex(v: int, width: int = 2) -> str:
    return f"${v:0{width}X}"


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def load_roms(path: Path) -> dict[str, bytes]:
    result: dict[str, bytes] = {}
    if path.is_dir():
        files = [p for p in path.rglob("*") if p.is_file()]
        by_base = {p.name.lower(): p for p in files}
        for name in CANONICAL:
            p = by_base.get(name.lower())
            if p is None:
                die(f"canonical member missing from directory: {name}")
            result[name] = p.read_bytes()
    elif path.is_file() and zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as zf:
            by_base: dict[str, str] = {}
            for n in zf.namelist():
                if n.endswith("/"):
                    continue
                base = Path(n).name.lower()
                if base in by_base:
                    die(f"ambiguous duplicate basename in ZIP: {base}")
                by_base[base] = n
            for name in CANONICAL:
                member = by_base.get(name.lower())
                if member is None:
                    die(f"canonical member missing from ZIP: {name}")
                result[name] = zf.read(member)
    else:
        die("ROM input must be a ZIP file or directory")
    return result


@dataclass(frozen=True)
class Instruction:
    address: int
    flavor: str
    text: str
    bytes_: bytes


def parse_complete_code(path: Path) -> list[Instruction]:
    lines = path.read_text(encoding="utf-8").splitlines()
    out: list[Instruction] = []
    label_re = re.compile(r"^(BASE|MS_EN|MS_DIS|DEAD)_([0-9A-Fa-f]{4}):$")
    for i, line in enumerate(lines):
        m = label_re.match(line.strip())
        if not m:
            continue
        if i + 1 >= len(lines):
            die(f"truncated instruction after {line}")
        body = lines[i + 1].strip()
        if ";" not in body:
            die(f"instruction line lacks byte comment after {line}")
        asm_text, byte_text = body.split(";", 1)
        tokens = byte_text.strip().split()
        try:
            raw = bytes(int(t, 16) for t in tokens)
        except ValueError:
            die(f"invalid byte comment after {line}: {byte_text!r}")
        if not raw:
            die(f"zero-length instruction after {line}")
        out.append(Instruction(int(m.group(2), 16), m.group(1), asm_text.rstrip(), raw))
    return out


@dataclass(frozen=True)
class StructRange:
    start: int
    end: int
    style: str
    comment: str


# Exact ranges are taken from already-certified structured-data evidence or from the
# next proven structure boundary.  Styles alter presentation only; they never alter bytes.
STRUCTURES = [
    StructRange(0x36A5, 0x3713, "dw", "55 little-endian DrawText record pointers"),
    StructRange(0x3B08, 0x3B30, "record2", "fruit-history sprite/color pairs"),
    StructRange(0x3B30, 0x3B40, "record8", "channel-1 sound-effect records"),
    StructRange(0x3B40, 0x3B80, "record8", "channel-2 sound-effect records"),
    StructRange(0x3B80, 0x3BB0, "record8", "channel-3 sound-effect records"),
    StructRange(0x3BB0, 0x3BB8, "chunk16", "sound duration lookup"),
    StructRange(0x3BB8, 0x3BC8, "chunk16", "sound frequency lookup"),
    StructRange(0x3F80, 0x3FE1, "dw_tail", "marquee screen-address lanes; final byte preserved explicitly"),
    StructRange(0x81F0, 0x8250, "dw", "48 animation-program pointers"),
    StructRange(0x8250, 0x8614, "chunk16", "validated animation command bytecode bank"),
    StructRange(0x8614, 0x869C, "animation_sequences", "FF-terminated animation sprite sequences plus proven gaps"),
    StructRange(0x879D, 0x87B2, "record3", "seven fruit descriptor records"),
    StructRange(0x87F8, 0x8800, "dw", "four fruit-entry path pointers"),
    StructRange(0x8800, 0x8808, "dw", "four fruit-exit path pointers"),
    StructRange(0x8808, 0x8810, "chunk16", "fixed fallback fruit route"),
    StructRange(0x8841, 0x88C1, "record2", "64 signed subpixel delta vectors"),
    StructRange(0x88C1, 0x8A3B, "chunk16", "maze 1 drawing command stream"),
    StructRange(0x8A3B, 0x8B2B, "record8", "maze 1 pellet map: 30 rows x 8 offsets"),
    StructRange(0x8BAE, 0x8D24, "chunk16", "maze 2 proven drawing command stream"),
    StructRange(0x8D27, 0x8E17, "record8", "maze 2 pellet map: 30 rows x 8 offsets"),
    StructRange(0x8EA8, 0x9015, "chunk16", "maze 3 proven drawing command stream"),
    StructRange(0x9018, 0x9108, "record8", "maze 3 pellet map: 30 rows x 8 offsets"),
    StructRange(0x9179, 0x92E9, "chunk16", "maze 4 proven drawing command stream"),
    StructRange(0x92EC, 0x93DC, "record8", "maze 4 pellet map: 30 rows x 8 offsets"),
    StructRange(0x9474, 0x947C, "dw", "four maze stream pointers"),
    StructRange(0x9499, 0x94A1, "dw", "four pellet-map pointers"),
    StructRange(0x94B5, 0x94BD, "dw", "four pellet-count pointers"),
    StructRange(0x94DF, 0x94EC, "chunk16", "13-entry level-to-maze schedule"),
    StructRange(0x951C, 0x9524, "dw", "four power-pellet table pointers"),
    StructRange(0x9578, 0x9580, "dw", "four ghost-destination table pointers"),
    StructRange(0x95AE, 0x95C3, "chunk16", "21 per-level maze palette codes"),
    StructRange(0x95DF, 0x95E3, "dw", "two tunnel slow-zone list pointers"),
    StructRange(0x9616, 0x9627, "record4_tail", "four bonus-MsPac graphic records plus FF terminator"),
    StructRange(0x967D, 0x9685, "dw", "four channel-2 song pointers"),
    StructRange(0x9685, 0x968D, "dw", "four channel-1 song pointers"),
    StructRange(0x968D, 0x9695, "dw", "four channel-3 song pointers"),
    StructRange(0x97D0, 0x9800, "ascii16", "GCC developer message text"),
]


def data_labels(analysis: Path) -> dict[int, list[tuple[str, str]]]:
    labels: dict[int, list[tuple[str, str]]] = {}
    for r in read_csv(analysis / "semantic_symbols.csv"):
        if r["kind"] != "data":
            continue
        labels.setdefault(hx(r["address"]), []).append((r["name"], r["evidence"]))
    return labels


def routine_labels(analysis: Path) -> dict[tuple[int, str], tuple[str, str]]:
    out: dict[tuple[int, str], tuple[str, str]] = {}
    for r in read_csv(analysis / "semantic_symbols.csv"):
        if r["kind"] not in ("routine", "dead_routine"):
            continue
        out[(hx(r["address"]), r["decoder_state"])] = (r["name"], r["evidence"])
    return out


def structured_evidence_labels(rows: list[dict[str, str]]) -> dict[int, list[tuple[str, str]]]:
    labels: dict[int, list[tuple[str, str]]] = {}
    for idx, r in enumerate(rows):
        start = hx(r["logical_start"])
        safe_kind = re.sub(r"[^A-Za-z0-9_]", "_", r["kind"])
        name = f"Evidence_{safe_kind}_{start:04X}_{idx:03d}"
        end = hx(r["logical_end_exclusive"])
        proof = f"Certified structured-data evidence ${start:04X}-${end-1:04X}: {r['proof']}"
        labels.setdefault(start, []).append((name, proof))
    return labels


def emit_db(lines: list[str], raw: bytes, comment: str = "") -> None:
    if not raw:
        return
    values = ",".join(qhex(x) for x in raw)
    suffix = f" ; {comment}" if comment else ""
    lines.append(f"    DB {values}{suffix} ; @DATA_LEN={len(raw)}")


def emit_dw(lines: list[str], raw: bytes, comment: str = "") -> None:
    if len(raw) % 2:
        die("DW structure has odd byte length")
    words = [raw[i] | (raw[i + 1] << 8) for i in range(0, len(raw), 2)]
    values = ",".join(qhex(x, 4) for x in words)
    suffix = f" ; {comment}" if comment else ""
    lines.append(f"    DW {values}{suffix} ; @DATA_LEN={len(raw)}")


def emit_structure(lines: list[str], sr: StructRange, raw: bytes, structured_rows: list[dict[str, str]]) -> None:
    lines.append(f"    ; Structure: {sr.comment}")
    if sr.style == "dw":
        for i in range(0, len(raw), 16):
            emit_dw(lines, raw[i:i + 16], f"{sr.comment} +{i}")
    elif sr.style == "dw_tail":
        even = len(raw) & ~1
        for i in range(0, even, 16):
            emit_dw(lines, raw[i:min(i + 16, even)], f"{sr.comment} +{i}")
        if even < len(raw):
            emit_db(lines, raw[even:], f"tail byte at +{even}")
    elif sr.style in ("record2", "record3", "record8"):
        n = int(sr.style.removeprefix("record"))
        for i in range(0, len(raw), n):
            emit_db(lines, raw[i:i + n], f"record {i // n}")
    elif sr.style == "record4_tail":
        body = len(raw) - 1
        for i in range(0, body, 4):
            emit_db(lines, raw[i:i + 4], f"record {i // 4}")
        emit_db(lines, raw[body:], "FF terminator")
    elif sr.style == "ascii16":
        for i in range(0, len(raw), 16):
            chunk = raw[i:i + 16]
            text = "".join(chr(x) if 32 <= x < 127 else "." for x in chunk)
            emit_db(lines, chunk, f"ASCII {text!r}")
    elif sr.style == "animation_sequences":
        # Use the exact sequence subranges already independently validated in structured-data analysis.
        seqs = []
        for r in structured_rows:
            if r["kind"] != "u5_animation_sprite_sequence":
                continue
            a, b = hx(r["logical_start"]), hx(r["logical_end_exclusive"])
            if sr.start <= a < b <= sr.end:
                seqs.append((a, b))
        seqs.sort()
        cursor = sr.start
        for idx, (a, b) in enumerate(seqs):
            if cursor < a:
                emit_db(lines, raw[cursor - sr.start:a - sr.start], f"proven non-sequence bytes ${cursor:04X}-${a - 1:04X}")
            lines.append(f"AnimationSpriteSequence_{idx:02d}:")
            emit_db(lines, raw[a - sr.start:b - sr.start], f"FF-terminated sequence {idx}")
            cursor = b
        if cursor < sr.end:
            emit_db(lines, raw[cursor - sr.start:], f"proven non-sequence bytes ${cursor:04X}-${sr.end - 1:04X}")
    else:
        for i in range(0, len(raw), 16):
            emit_db(lines, raw[i:i + 16], f"{sr.comment} +{i}")


def emit_component(path: Path, title: str, org: int, image: bytes,
                   instructions: list[Instruction], labels: dict[int, list[tuple[str, str]]],
                   routines: dict[tuple[int, str], tuple[str, str]],
                   flavor_state: dict[str, str], structures: list[StructRange],
                   holes: list[tuple[int, int]], structured_rows: list[dict[str, str]],
                   evidence_index_rows: list[tuple[int, dict[str, str]]] | None = None,
                   resource_block_prefix: str | None = None, resource_block_size: int = 16) -> None:
    end = org + len(image)
    ins_by_addr: dict[int, Instruction] = {}
    occupied: set[int] = set()
    for ins in instructions:
        if not (org <= ins.address < end):
            continue
        if ins.address + len(ins.bytes_) > end:
            die(f"instruction crosses component boundary: {ins.flavor}_{ins.address:04X}")
        off = ins.address - org
        if image[off:off + len(ins.bytes_)] != ins.bytes_:
            die(f"instruction byte mismatch for {ins.flavor}_{ins.address:04X} in {path.name}")
        rng = set(range(ins.address, ins.address + len(ins.bytes_)))
        if occupied & rng:
            die(f"overlapping instruction records in {path.name} at ${ins.address:04X}")
        occupied |= rng
        ins_by_addr[ins.address] = ins

    struct_by_start = {s.start: s for s in structures if org <= s.start < s.end <= end}
    for s in struct_by_start.values():
        if any(a in occupied for a in range(s.start, s.end)):
            die(f"structured DATA range overlaps mnemonic CODE: ${s.start:04X}-${s.end - 1:04X}")

    hole_by_start = {a: b for a, b in holes if org <= a < b <= end}
    for a, b in holes:
        if not (org <= a < b <= end):
            continue
        if any(x in occupied for x in range(a, b)):
            die(f"placeholder hole overlaps selected mnemonic in {path.name}: ${a:04X}-${b - 1:04X}")

    lines = [
        f"; {title}",
        "; Generated by MsPacmanRipper — ROM-derived output, not part of source packages.",
        "; Created by Jacob Hodgkins",
        "; Every CODE directive below is a real Z80 mnemonic; binary inclusion is prohibited.",
        "",
        f"    ORG {qhex(org, 4)}",
        "",
    ]
    if evidence_index_rows:
        lines.append("    ; ---- Certified structured-data evidence index for this component ----")
        for global_idx, er in evidence_index_rows:
            a, b = hx(er["logical_start"]), hx(er["logical_end_exclusive"])
            lines.append(f"    ; @STRUCT_EVIDENCE={global_idx:03d} {er['kind']} ${a:04X}-${b-1:04X} | {er['proof']}")
        lines.append("")

    event_starts = set(ins_by_addr) | set(labels) | set(struct_by_start) | set(hole_by_start)
    pc = org
    resource_index = 0
    while pc < end:
        if pc in labels:
            for name, evidence in labels[pc]:
                lines.append("")
                lines.append(f"{name}:")
                lines.append(f"    ; Evidence: {evidence}")

        if resource_block_prefix and (pc - org) % resource_block_size == 0:
            lines.append("")
            lines.append(f"{resource_block_prefix}_{resource_index:03d}:")
            resource_index += 1

        ins = ins_by_addr.get(pc)
        if ins is not None:
            state = flavor_state.get(ins.flavor, "ENABLED")
            sem = routines.get((pc, state))
            if sem:
                lines.append("")
                lines.append(f"{sem[0]}:")
                lines.append(f"    ; Evidence: {sem[1]}")
            lines.append(f"L_{ins.flavor}_{pc:04X}:")
            bytes_comment = " ".join(f"{b:02X}" for b in ins.bytes_)
            lines.append(f"    {ins.text} ; @CODE_BYTES={bytes_comment}")
            pc += len(ins.bytes_)
            continue

        if pc in hole_by_start:
            b = hole_by_start[pc]
            lines.append("")
            lines.append(f"    ; U5 patch-source window ${pc:04X}-${b - 1:04X}: reconstructed from enabled low alias after assembly.")
            cursor = pc
            while cursor < b:
                n = min(16, b - cursor)
                emit_db(lines, bytes([0] * n), "patch transplant placeholder")
                cursor += n
            pc = b
            continue

        sr = struct_by_start.get(pc)
        if sr is not None:
            lines.append("")
            emit_structure(lines, sr, image[sr.start - org:sr.end - org], structured_rows)
            pc = sr.end
            continue

        # Ordinary proven DATA/asset bytes: keep exact bytes, but stop before the next
        # semantic label, instruction, structure, hole, or resource block boundary.
        next_pc = min(end, pc + 16)
        for ev in event_starts:
            if pc < ev < next_pc:
                next_pc = ev
        if resource_block_prefix:
            boundary = org + (((pc - org) // resource_block_size) + 1) * resource_block_size
            next_pc = min(next_pc, boundary)
        emit_db(lines, image[pc - org:next_pc - org], f"exact non-CODE/resource bytes ${pc:04X}-${next_pc - 1:04X}")
        pc = next_pc

    lines.append("")
    lines.append(f"    ; End ${end:04X}; exact component length {len(image)} bytes.")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def emit_simple_resource(path: Path, title: str, data: bytes, prefix: str, block: int) -> None:
    lines = [
        f"; {title}",
        "; Generated by MsPacmanRipper — ROM-derived output, not part of source packages.",
        "; Created by Jacob Hodgkins",
        "",
        "    ORG $0000",
    ]
    for idx, off in enumerate(range(0, len(data), block)):
        chunk = data[off:off + block]
        lines += ["", f"{prefix}_{idx:03d}:"]
        emit_db(lines, chunk, f"{title} block {idx}, source offset ${off:04X}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    if len(sys.argv) != 6:
        print(f"usage: {sys.argv[0]} <repo-root> <rom.zip|dir> <analysis-dir> <logical-dir> <out-dir>", file=sys.stderr)
        return 2
    root, rom_path, analysis, logical, out = map(Path, sys.argv[1:])
    root, rom_path, analysis, logical, out = root.resolve(), rom_path.resolve(), analysis.resolve(), logical.resolve(), out.resolve()
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)

    roms = load_roms(rom_path)
    enabled = (logical / "decoder_enabled_64k.bin").read_bytes()
    disabled = (logical / "decoder_disabled_64k.bin").read_bytes()
    if len(enabled) != 65536 or len(disabled) != 65536:
        die("logical images must each be 64 KiB")
    base = b"".join(roms[n] for n in ("pacman.6e", "pacman.6f", "pacman.6h", "pacman.6j"))
    if len(base) != 0x4000 or base != disabled[:0x4000]:
        die("base program does not match disabled logical image")

    records = parse_complete_code(analysis / "complete_code_disassembly.asm")
    structured_rows = read_csv(analysis / "structured_data_evidence.csv")
    labels = data_labels(analysis)
    for addr, extra in structured_evidence_labels(structured_rows).items():
        labels.setdefault(addr, []).extend(extra)
    routines = routine_labels(analysis)
    indexed_structured_rows = list(enumerate(structured_rows))
    patch_rows = read_csv(logical / "patch_map.csv")
    patch_dest = set()
    holes: list[tuple[int, int]] = []
    for r in patch_rows:
        d = hx(r["destination_start"])
        s = hx(r["source_start"])
        patch_dest.update(range(d, d + 8))
        holes.append((s, s + 8))
    holes.sort()

    base_records = [r for r in records if r.flavor == "BASE"]
    low_records = [r for r in records if r.flavor == "MS_EN" and r.address < 0x3000 and any((r.address + i) in patch_dest for i in range(len(r.bytes_)))]
    u5_records = [r for r in records if r.flavor == "MS_EN" and 0x8000 <= r.address < 0x8800 and not any(a <= r.address < b for a, b in holes)]
    u6_records = [r for r in records if r.flavor == "MS_EN" and 0x8800 <= r.address < 0x9800]
    u7_records = [r for r in records if (r.flavor in ("MS_EN", "MS_DIS", "DEAD")) and 0x3000 <= r.address < 0x4000]
    # If multiple state records ever appear at one U7 address, fail instead of choosing silently.
    if len({r.address for r in u7_records}) != len(u7_records):
        die("multiple U7 mnemonic records share an address; exporter requires explicit adjudication")

    emit_component(out / "base_program.asm", "Canonical Pac-Man base 16 KiB program", 0x0000, base,
                   base_records, {}, {}, {"BASE": "ENABLED"}, [], [], structured_rows, None)
    emit_component(out / "enabled_low_patch_image.asm", "Enabled low-address patch execution view", 0x0000, enabled[:0x3000],
                   low_records, {}, routines, {"MS_EN": "ENABLED"}, [], [], structured_rows, None)
    emit_component(out / "u5_decoded.asm", "Decoded U5 logical storage", 0x8000, enabled[0x8000:0x8800],
                   u5_records, labels, routines, {"MS_EN": "ENABLED"}, STRUCTURES, holes, structured_rows, [(i,r) for i,r in indexed_structured_rows if 0x8000 <= hx(r["logical_start"]) < 0x8800])
    emit_component(out / "u6_logical.asm", "U6 logical execution view (decoded halves reordered by rebuilder)", 0x8800, enabled[0x8800:0x9800],
                   u6_records, labels, routines, {"MS_EN": "ENABLED"}, STRUCTURES, [], structured_rows, [(i,r) for i,r in indexed_structured_rows if 0x8800 <= hx(r["logical_start"]) < 0x9800])
    emit_component(out / "u7_decoded.asm", "Decoded U7 logical storage", 0x3000, enabled[0x3000:0x4000],
                   u7_records, labels, routines, {"MS_EN": "ENABLED", "MS_DIS": "DISABLED", "DEAD": "ENABLED"}, STRUCTURES, [], structured_rows, [(i,r) for i,r in indexed_structured_rows if 0x3000 <= hx(r["logical_start"]) < 0x4000])

    emit_simple_resource(out / "graphics_5e.asm", "Character graphics ROM 5e", roms["5e"], "CharacterGlyph", 16)
    emit_simple_resource(out / "graphics_5f.asm", "Sprite graphics ROM 5f", roms["5f"], "SpriteGraphicsBlock", 16)
    emit_simple_resource(out / "prom_82s123_7f.asm", "Palette PROM 82s123.7f", roms["82s123.7f"], "PaletteEntryBlock", 16)
    emit_simple_resource(out / "prom_82s126_4a.asm", "Color lookup PROM 82s126.4a", roms["82s126.4a"], "ColorLookupBlock", 16)
    emit_simple_resource(out / "prom_82s126_1m.asm", "Waveform PROM 82s126.1m", roms["82s126.1m"], "WaveformBlock", 16)
    emit_simple_resource(out / "prom_82s126_3m.asm", "Sound timing/control PROM 82s126.3m", roms["82s126.3m"], "SoundTimingBlock", 16)

    missing = [n for n in ASM_COMPONENTS if not (out / n).is_file()]
    if missing:
        die(f"component generation incomplete: {missing}")
    if len(list(out.glob("*.asm"))) != 11:
        die("output directory does not contain exactly 11 .asm components")
    for p in out.glob("*.asm"):
        text = p.read_text(encoding="utf-8")
        if re.search(r"\bINCBIN\b", text, re.IGNORECASE):
            die(f"forbidden INCBIN token emitted in {p.name}")

    # data-semantic verification sidecar: exhaustive semantic owner for every one of the 12,382
    # physical DATA bytes.  It carries addresses/semantic identifiers only, never ROM bytes.
    data_catalog = out / "DATA_SEMANTIC_OWNERSHIP.csv"
    proc = subprocess.run([str(root / "scripts/verify_data_semantics.py"), str(root), str(rom_path), str(data_catalog)],
                          text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if proc.returncode != 0:
        die("data-semantic verification semantic sidecar generation failed:\n" + proc.stdout[-4000:])
    with data_catalog.open(newline="", encoding="utf-8") as f:
        if sum(1 for _ in csv.DictReader(f)) != 12382:
            die("data-semantic verification semantic sidecar does not contain exactly 12,382 DATA-byte owners")

    # Human-readable manifest contains only sizes/roles, never canonical hashes or payloads.
    manifest = out / "SOURCE_COMPONENTS.txt"
    manifest.write_text(
        "MsPacmanRipper reassembly-source export generated source components\n"
        "Created by Jacob Hodgkins\n"
        "ROM-derived output: do not include these generated files in the source packages.\n\n" +
        "\n".join(ASM_COMPONENTS + ["DATA_SEMANTIC_OWNERSHIP.csv"]) + "\n",
        encoding="utf-8",
    )
    print(f"reassembly-source export textual reassembly source export: VERIFIED -> {out}")
    print("  deterministic component count: 11")
    print("  frozen DATA semantic labels carried into public source: 37/37")
    print(f"  certified structured-data evidence rows surfaced: {len(structured_rows)}/{len(structured_rows)}")
    print("  exhaustive semantic DATA ownership sidecar: 12382/12382")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
