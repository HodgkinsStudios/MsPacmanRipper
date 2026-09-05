#!/usr/bin/env python3
# Created by Jacob Hodgkins
"""Fail-closed semantic-family denominator verifier.

This verifier intentionally does not trust a percentage exported by the C++ program.  It
recomputes the daughterboard CODE decomposition from independent reference data, checks the
63-family ROM-free semantic ledger against the current semantic catalog, and checks the
43 modified Pac-Man verdict ledger against the independently generated behavioral semantic analysis impact map.
"""
from __future__ import annotations

import csv
import os
import sys
from collections import Counter
from pathlib import Path


def die(msg: str) -> None:
    print(f"semantic-family audit: FAIL: {msg}", file=sys.stderr)
    raise SystemExit(1)


def read_csv(path: Path) -> list[dict[str, str]]:
    if not path.is_file():
        die(f"missing required CSV: {path}")
    with path.open(newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def hx(text: str) -> int:
    s = text.strip()
    if s.startswith("$"):
        s = "0x" + s[1:]
    return int(s, 0)


def main() -> int:
    if len(sys.argv) != 4:
        print(f"usage: {sys.argv[0]} <repo-root> <analysis-dir> <logical-dir>", file=sys.stderr)
        return 2

    root = Path(sys.argv[1]).resolve()
    analysis = Path(sys.argv[2]).resolve()
    logical = Path(sys.argv[3]).resolve()

    sem = read_csv(analysis / "semantic_symbols.csv")
    families = read_csv(root / "evidence/mspacman_semantic_families_reference.csv")
    verdicts = read_csv(root / "evidence/modified_family_verdicts_reference.csv")
    ownership = read_csv(analysis / "ownership.csv")
    segments = read_csv(analysis / "daughterboard_code_segments.csv")
    patches = read_csv(analysis / "patch_coverage.csv")
    transfer = read_csv(analysis / "pacman_semantic_family_transfer.csv")
    impacts = read_csv(analysis / "pacman_modified_family_impact.csv")
    instructions = read_csv(analysis / "instructions.csv")

    enabled_path = logical / "decoder_enabled_64k.bin"
    disabled_path = logical / "decoder_disabled_64k.bin"
    if not enabled_path.is_file() or not disabled_path.is_file():
        die("logical export is missing decoder_enabled_64k.bin / decoder_disabled_64k.bin")
    enabled = enabled_path.read_bytes()
    disabled = disabled_path.read_bytes()
    if len(enabled) != 65536 or len(disabled) != 65536:
        die("logical image size is not exactly 64 KiB")

    # Current semantic catalog must freeze exactly the 63 Ms.-specific routine families
    # plus the previously proven 37 DATA semantic symbols.
    routine_rows = [r for r in sem if r["kind"] in ("routine", "dead_routine")]
    data_rows = [r for r in sem if r["kind"] == "data"]
    if len(sem) != 100 or len(routine_rows) != 63 or len(data_rows) != 37:
        die(f"semantic catalog expected 100 = 63 routine/dead + 37 DATA, got {len(sem)} = {len(routine_rows)} + {len(data_rows)}")
    if len(families) != 63:
        die(f"frozen Ms.-specific family ledger expected 63 rows, got {len(families)}")

    sem_by_key = {(hx(r["address"]), r["decoder_state"], r["name"]): r for r in sem}
    if len(sem_by_key) != len(sem):
        die("semantic catalog contains duplicate address/state/name rows")

    family_ids = [r["family_id"] for r in families]
    family_names = [r["name"] for r in families]
    family_entries = [(hx(r["entry_address"]), r["entry_decoder"]) for r in families]
    if len(set(family_ids)) != 63 or len(set(family_names)) != 63 or len(set(family_entries)) != 63:
        die("family ledger IDs, names, or entry identities are not unique")

    live = dead = ledger_bytes = 0
    instruction_keys = {(hx(r["address"]), r["entry_decoder"]) for r in instructions}
    dead_bounds = {
        0x367F: (0x367F, 0x3696),
        0x39E0: (0x39E0, 0x39F2),
        0x3A00: (0x3A00, 0x3A06),
    }
    complete_asm = (analysis / "complete_code_disassembly.asm").read_text(encoding="utf-8")

    for row in families:
        addr = hx(row["entry_address"])
        state = row["entry_decoder"]
        kind = row["kind"]
        key = (addr, state, row["name"])
        current = sem_by_key.get(key)
        if current is None:
            die(f"frozen family seed missing from current catalog: {row['family_id']} {row['name']}")
        if current["kind"] != kind or current["evidence"] != row["evidence"]:
            die(f"semantic identity/evidence drift for {row['family_id']} {row['name']}")
        owned = int(row["owned_extension_code_bytes"])
        if owned <= 0:
            die(f"family has zero/negative owned extension CODE bytes: {row['family_id']}")
        ledger_bytes += owned
        if kind == "dead_routine":
            dead += 1
            if addr not in dead_bounds or (dead_bounds[addr][1] - dead_bounds[addr][0]) != owned:
                die(f"dead-family bound/byte ledger mismatch at ${addr:04X}")
            if f"{row['name']}:" not in complete_asm:
                die(f"dead-family label missing from complete CODE disassembly: {row['name']}")
        elif kind == "routine":
            live += 1
            if (addr, state) not in instruction_keys:
                die(f"live semantic family entry is not a reached instruction root: {row['name']} ${addr:04X} {state}")
        else:
            die(f"unexpected family kind {kind!r}")

    if (live, dead, ledger_bytes) != (60, 3, 1714):
        die(f"family ledger expected 60 live / 3 dead / 1714 extension CODE bytes, got {live}/{dead}/{ledger_bytes}")

    # Recompute physical daughterboard CODE counts from ownership.csv and independently
    # cross-check the frozen 64-segment ledger.
    own_counts = Counter()
    for r in ownership:
        if r["source"] in ("u5", "u6", "u7") and r["classification"] == "CODE":
            own_counts[r["source"]] += 1
    seg_counts = Counter()
    for r in segments:
        seg_counts[r["source"]] += int(r["code_bytes"])
    if len(segments) != 64:
        die(f"daughterboard segment denominator drifted: expected 64, got {len(segments)}")
    expected_db = {"u5": 502, "u6": 526, "u7": 1613}
    if dict(own_counts) != expected_db or dict(seg_counts) != expected_db:
        die(f"daughterboard CODE source counts drifted: ownership={dict(own_counts)} segments={dict(seg_counts)}")
    if sum(own_counts.values()) != 2641:
        die("daughterboard CODE total is not 2641")

    patch_code = sum(int(r["code_bytes"]) for r in patches)
    patch_inline = sum(int(r["inline_data_bytes"]) for r in patches)
    patch_alias_other = sum(int(r["unknown_bytes"]) for r in patches)
    if len(patches) != 40 or patch_code != 178 or patch_code + patch_inline + patch_alias_other != 320:
        die(f"patch-window accounting drifted: patches={len(patches)} code={patch_code} inline={patch_inline} other={patch_alias_other}")

    # The Pac-Man semantic transfer ledger identifies U7 bytes that retain Pac-Man family
    # ownership even though the physical byte lives in daughterboard U7.  This is the key
    # guard against double-counting duplicated Pac-Man code as new Ms.-specific semantics.
    if len(transfer) != 381:
        die(f"Pac-Man semantic-family denominator drifted: expected 381, got {len(transfer)}")
    tcounts = Counter(r["transfer_status"] for r in transfer)
    if tcounts != Counter({"BYTE_EQUIVALENT_UNPATCHED": 338, "PATCH_OVERLAY_INTERSECTS": 42, "ENABLED_BYTES_DIFFER": 1}):
        die(f"semantic transfer analysis transfer status decomposition drifted: {dict(tcounts)}")
    equivalent_u7 = sum(int(r["u7_bytes"]) for r in transfer if r["transfer_status"] == "BYTE_EQUIVALENT_UNPATCHED")
    changed_rows = [r for r in transfer if r["transfer_status"] == "ENABLED_BYTES_DIFFER"]
    if len(changed_rows) != 1:
        die("expected exactly one enabled-view-different Pac-Man family")
    changed_u7_owned = int(changed_rows[0]["u7_bytes"])
    changed_u7_bytes = int(changed_rows[0]["enabled_byte_differences"])
    if (equivalent_u7, changed_u7_owned, changed_u7_bytes) != (559, 190, 2):
        die(f"U7 Pac-Man ownership drifted: equivalent={equivalent_u7} changed-family-owned={changed_u7_owned} changed-bytes={changed_u7_bytes}")
    pacman_owned_u7 = equivalent_u7 + changed_u7_owned
    duplicated_pacman_u7 = pacman_owned_u7 - changed_u7_bytes
    u7_extension = own_counts["u7"] - pacman_owned_u7
    u5_extension = own_counts["u5"] - patch_code
    u6_extension = own_counts["u6"]
    extension_total = u5_extension + u6_extension + u7_extension
    if (duplicated_pacman_u7, u5_extension, u6_extension, u7_extension, extension_total) != (747, 324, 526, 864, 1714):
        die("daughterboard extension/duplicate decomposition does not match the frozen 1714+180+747 partition")

    # Explicitly verify the changed enabled-view U7 bytes are still exactly $322B/$322C.
    changed_addrs = [a for a in range(0x3000, 0x4000) if enabled[a] != disabled[a]]
    # This list includes Ms.-specific extension, so identify the one changed Pac-Man family
    # from its frozen source PCs rather than pretending all U7 differences are Pac-Man changes.
    fam_ref = read_csv(root / "evidence/pacman_semantic_families_reference.csv")
    boundary_rows = read_csv(root / "evidence/pacman_certified_instruction_boundaries.csv")
    lengths = {hx(r["pc"]): int(r["length"]) for r in boundary_rows}
    changed_family_id = changed_rows[0]["family_id"]
    fr = next((r for r in fam_ref if r["family_id"] == changed_family_id), None)
    if fr is None:
        die("changed Pac-Man family missing from family reference")
    changed_family_bytes = set()
    for token in fr["source_pcs"].split():
        pc = hx(token)
        for i in range(lengths[pc]):
            if 0x3000 <= pc + i < 0x4000 and enabled[pc + i] != disabled[pc + i]:
                changed_family_bytes.add(pc + i)
    if changed_family_bytes != {0x322B, 0x322C}:
        die(f"changed Pac-Man U7 bytes drifted: {sorted(hex(x) for x in changed_family_bytes)}")

    # Revalidate all 43 modified-family verdicts against the current independent impact map.
    if len(impacts) != 43 or len(verdicts) != 43:
        die(f"modified-family denominator drifted: impacts={len(impacts)} verdicts={len(verdicts)}")
    verdict_by_id = {r["family_id"]: r for r in verdicts}
    if len(verdict_by_id) != 43:
        die("duplicate family IDs in modified-family verdict ledger")
    verdict_counts = Counter()
    for impact in impacts:
        v = verdict_by_id.get(impact["family_id"])
        if v is None:
            die(f"missing verdict for modified family {impact['family_id']}")
        if v["family_name"] != impact["family_name"] or hx(v["entry_pc"]) != hx(impact["entry_pc"]):
            die(f"verdict identity drift for {impact['family_id']}")
        if v["patch_destinations"] != impact["patch_destinations"] or v["verified_redirects"] != impact["verified_redirects"] or v["semantic_targets"] != impact["semantic_redirect_targets"]:
            die(f"verdict/impact evidence mismatch for {impact['family_id']}")
        if impact["impact_kind"] == "PATCHED_WITH_VERIFIED_REDIRECT":
            expected = "PATCH_REDIRECTS_TO_MS_SPECIFIC_ROUTINE"
        elif impact["impact_kind"] == "PATCHED_INLINE_OR_DATA_ONLY":
            expected = "PATCHED_INLINE_OR_DATA_OVERLAY"
        elif impact["impact_kind"] == "U7_CHANGED_BYTES":
            expected = "PACMAN_FAMILY_PRESERVED_WITH_U7_ENABLED_VIEW_CHANGE"
        else:
            die(f"unknown modified-family impact kind {impact['impact_kind']}")
        if v["verdict"] != expected:
            die(f"wrong semantic verdict for {impact['family_id']}: expected {expected}, got {v['verdict']}")
        verdict_counts[v["verdict"]] += 1
    expected_vcounts = Counter({
        "PATCH_REDIRECTS_TO_MS_SPECIFIC_ROUTINE": 25,
        "PATCHED_INLINE_OR_DATA_OVERLAY": 17,
        "PACMAN_FAMILY_PRESERVED_WITH_U7_ENABLED_VIEW_CHANGE": 1,
    })
    if verdict_counts != expected_vcounts:
        die(f"modified-family verdict decomposition drifted: {dict(verdict_counts)}")

    # Emit validated ledgers alongside the fresh analysis so downstream verification stages consume
    # only artifacts that survived this run.
    for src_name, dst_name in (
        ("mspacman_semantic_families_reference.csv", "mspacman_semantic_families.csv"),
        ("modified_family_verdicts_reference.csv", "pacman_modified_family_verdicts.csv"),
    ):
        (analysis / dst_name).write_bytes((root / "evidence" / src_name).read_bytes())

    status = analysis / "SEMANTIC_FAMILY_STATUS.txt"
    status.write_text(
        "MsPacmanRipper semantic-family audit: VERIFIED\n"
        "Ms.-specific semantic families: 63/63\n"
        "Live / bounded dead families: 60 / 3\n"
        "Ms.-specific extension CODE bytes: 1714/1714\n"
        "Modified Pac-Man physical CODE bytes: 180/180\n"
        "Duplicated U7 Pac-Man CODE excluded from Ms.-specific denominator: 747\n"
        "Human-semantic CODE-byte surface: 1894/1894\n"
        "Modified Pac-Man family verdicts: 43/43\n"
        "Semantic catalog: 100 = 63 routine/dead-routine + 37 DATA\n",
        encoding="utf-8",
    )

    print("semantic-family verification independent semantic denominator audit: VERIFIED")
    print("  Ms.-specific semantic families: 63/63 (60 live / 3 bounded dead)")
    print("  daughterboard extension CODE: 1714 = U5 324 + U6 526 + U7 864")
    print("  modified Pac-Man physical CODE: 180 = U5 patch 178 + U7 enabled-view 2")
    print("  duplicated U7 Pac-Man CODE excluded from new-family denominator: 747")
    print("  semantic CODE-byte surface: 1894/1894")
    print("  modified Pac-Man family verdicts: 43/43 = 25 redirect + 17 inline/data + 1 U7-preserved")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
