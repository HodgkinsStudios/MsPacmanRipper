#!/usr/bin/env python3
# Created by Jacob Hodgkins
"""Fail-closed data-semantic verification exhaustive semantic DATA ownership verifier."""
from __future__ import annotations
import csv, subprocess, sys, tempfile
from pathlib import Path
from collections import Counter, defaultdict


def fail(msg: str) -> None:
    raise SystemExit("data-semantic verification DATA semantic audit: FAIL\n  " + msg)

def h(s: str) -> int:
    s=s.strip()
    return int(s[1:] if s.startswith('$') else s,16)

def rows(path: Path):
    with path.open(newline='', encoding='utf-8') as f:
        return list(csv.DictReader(f))

def logical_for_source(source: str, off: int) -> int:
    if source == 'pacman.6e': return off
    if source == 'pacman.6f': return 0x1000 + off
    if source == 'pacman.6h': return 0x2000 + off
    if source == 'pacman.6j': return 0x3000 + off
    if source == 'u5': return 0x8000 + off
    if source == 'u7': return 0x3000 + off
    if source == 'u6': return 0x9000 + off if off < 0x0800 else 0x8000 + off
    fail(f'unknown source in ownership.csv: {source}')

def daughter_source_for_logical(addr: int) -> str:
    if 0x3000 <= addr < 0x4000: return 'u7'
    if 0x8000 <= addr < 0x8800: return 'u5'
    if 0x8800 <= addr < 0x9800: return 'u6'
    fail(f'daughterboard evidence address ${addr:04X} escaped U5/U6/U7 logical domains')

def parse_hex_bytes(s: str) -> bytes:
    return bytes(int(x,16) for x in s.split())


def main() -> int:
    if len(sys.argv) not in (3,4):
        print(f'usage: {Path(sys.argv[0]).name} ROOT MSPACMAN_ROM [OWNERSHIP_CATALOG_OUT]', file=sys.stderr)
        return 2
    root=Path(sys.argv[1]).resolve(); rom=Path(sys.argv[2]).resolve()
    catalog_out=Path(sys.argv[3]).resolve() if len(sys.argv)==4 else None
    exe_candidates=[root/'bin'/'MsPacmanRipper.exe',root/'bin'/'MsPacmanRipper']
    exe=next((p for p in exe_candidates if p.is_file()),None)
    if exe is None: fail('bin/MsPacmanRipper(.exe) is missing; build before verification')
    parent_path=root/'evidence'/'pacman_semantic_data_reference.csv'
    target_path=root/'evidence'/'targeted_data_semantics.csv'
    if not parent_path.is_file() or not target_path.is_file(): fail('data-semantic verification evidence ledgers are missing')

    # Frozen parent semantic DATA authority: 257 non-overlapping COMPLETE spans / 4,783 bytes.
    parent_rows=rows(parent_path)
    if len(parent_rows) != 257: fail(f'expected 257 Pac-Man semantic DATA spans, got {len(parent_rows)}')
    parent_by_addr={}
    for r in parent_rows:
        if r.get('semantic_status') != 'COMPLETE': fail(f"parent span {r.get('span_id')} is not COMPLETE")
        a=h(r['start_address']); b=h(r['end_address_exclusive'])
        if b <= a or int(r['byte_count']) != b-a: fail(f"parent span {r['span_id']} has inconsistent bounds")
        for addr in range(a,b):
            if addr in parent_by_addr: fail(f'parent semantic DATA overlap at ${addr:04X}')
            parent_by_addr[addr]=r
    if len(parent_by_addr) != 4783: fail(f'expected 4,783 parent semantic DATA bytes, got {len(parent_by_addr)}')

    with tempfile.TemporaryDirectory(prefix='mspacman_data_semantics_') as td:
        td=Path(td); analysis=td/'analysis'; logical=td/'logical'
        subprocess.run([str(exe),'--internal-analyze',str(rom),str(analysis)],check=True,stdout=subprocess.DEVNULL)
        subprocess.run([str(exe),'--internal-export-logical',str(rom),str(logical)],check=True,stdout=subprocess.DEVNULL)
        enabled=(logical/'decoder_enabled_64k.bin').read_bytes(); disabled=(logical/'decoder_disabled_64k.bin').read_bytes()
        if len(enabled)!=65536 or len(disabled)!=65536: fail('logical 64K export size mismatch')

        ownership=rows(analysis/'ownership.csv')
        data_keys=[]; data_key_set=set(); base_keys=[]; daughter_keys=[]
        by_source=Counter()
        for r in ownership:
            if r['classification'] != 'DATA': continue
            source=r['source']; off=int(r['decoded_offset'],16); addr=logical_for_source(source,off)
            key=(source,addr)
            if key in data_key_set: fail(f'duplicate physical DATA key {key}')
            data_key_set.add(key); data_keys.append(key); by_source[source]+=1
            if source.startswith('pacman.'): base_keys.append(key)
            else: daughter_keys.append(key)
        if len(data_keys)!=12382: fail(f'expected 12,382 DATA bytes, got {len(data_keys)}')
        expected_sources={'pacman.6e':883,'pacman.6f':206,'pacman.6h':347,'pacman.6j':3347,'u5':1546,'u6':3570,'u7':2483}
        if {s:by_source[s] for s in expected_sources} != expected_sources:
            fail(f'DATA source partition changed: {dict(by_source)}')

        # Base board DATA must match the complete frozen Pac-Man semantic partition exactly.
        base_addr_set={a for s,a in base_keys}
        if base_addr_set != set(parent_by_addr):
            fail(f'base DATA/Pac-Man semantic partition mismatch missing={len(set(parent_by_addr)-base_addr_set)} extra={len(base_addr_set-set(parent_by_addr))}')

        evidence_by_key=defaultdict(list)
        # Existing specific structured evidence. Every range is daughterboard-enabled provenance.
        structured=rows(analysis/'structured_data_evidence.csv')
        if len(structured)!=166: fail(f'expected 166 structured-data evidence rows, got {len(structured)}')
        for idx,r in enumerate(structured):
            a=int(r['logical_start'],16); b=int(r['logical_end_exclusive'],16)
            source=daughter_source_for_logical(a)
            if daughter_source_for_logical(b-1) != source: fail(f'structured row {idx} crosses daughterboard source boundary')
            for addr in range(a,b):
                key=(source,addr)
                if key in data_key_set: evidence_by_key[key].append(('structured',f'STRUCT_{idx:03d}',r['kind'],r['proof']))

        # residual ownership closure residual evidence is semantically useful only for rows that certify DATA.
        residual=rows(analysis/'residual_closure_evidence.csv')
        for idx,r in enumerate(residual):
            if r['classification']!='DATA': continue
            a=int(r['logical_start'],16); b=int(r['logical_end_exclusive'],16)
            source=daughter_source_for_logical(a)
            if daughter_source_for_logical(b-1)!=source: fail(f'residual ownership closure DATA row {idx} crosses daughterboard source boundary')
            for addr in range(a,b):
                key=(source,addr)
                if key in data_key_set: evidence_by_key[key].append(('residual',f'RESIDUAL_{idx:03d}',r['kind'],r['proof']))

        # Targeted closure for the 71-byte pre-data-semantic verification semantic gap.
        targeted=rows(target_path)
        if len(targeted)!=17: fail(f'expected 17 targeted data-semantic verification semantic rows, got {len(targeted)}')
        target_ids=set()
        instructions={(int(r['address'],16),r['entry_decoder']):r for r in rows(analysis/'instructions.csv')}
        rst20=rows(analysis/'rst20_dispatch_tables.csv')
        patch_rows=rows(logical/'patch_map.csv')
        target_new_keys=set()
        for r in targeted:
            sid=r['semantic_id']
            if sid in target_ids: fail(f'duplicate targeted semantic_id {sid}')
            target_ids.add(sid)
            source=r['source']; a=int(r['logical_start'],16); b=int(r['logical_end_exclusive'],16)
            exp=parse_hex_bytes(r['expected_hex'])
            if len(exp)!=b-a: fail(f'{sid} expected_hex length does not match bounds')
            if source=='u5': actual=enabled[a:b]
            elif source in ('u6','u7'): actual=enabled[a:b]
            else: fail(f'{sid} has invalid target source {source}')
            if actual!=exp: fail(f'{sid} logical bytes changed at ${a:04X}-${b-1:04X}')
            for addr in range(a,b):
                key=(source,addr)
                if key not in data_key_set: fail(f'{sid} includes non-DATA physical byte {source}:${addr:04X}')
                evidence_by_key[key].append(('targeted',sid,r['semantic_label'],r['semantic_detail']+' '+r['proof']))
                target_new_keys.add(key)
            if sid=='TARGET_LOCAL_000':
                matches=[p for p in patch_rows if int(p['destination_start'],16)==0x23E0 and int(p['destination_end'],16)==0x23E7 and int(p['source_start'],16)==0x80E8 and int(p['source_end'],16)==0x80EF]
                if len(matches)!=1: fail('patched main-task-table tail alias $23E0-$23E7 -> U5 $80E8-$80EF is not uniquely present')
            elif sid=='TARGET_LOCAL_007':
                site=[x for x in rst20 if int(x['site'],16)==0x3E67]
                if len(site)!=17: fail(f'expected 17 RST20 entries for $3E67, got {len(site)}')
                site=sorted(site,key=lambda x:int(x['entry_index']))
                if int(site[0]['table_start'],16)!=0x3E68 or int(site[0]['table_end_exclusive'],16)!=0x3E8A:
                    fail('$3E67 RST20 table bounds changed')
                want=[0x045F,0x3E96,0x3E8B,0x000C,0x3EBD,0x3E9C,0x3483,0x3EA2,0x3488,0x3EAB,0x348D,0x3EB1,0x3492,0x3EC3,0x3EB7,0x3497,0x3EC9]
                got=[int(x['target'],16) for x in site]
                if got!=want: fail('$3E67 RST20 semantic target order changed')
            elif sid.startswith('TARGET_LOCAL_'):
                # Every other targeted row is exactly the two inline bytes after a reached RST $28.
                inst=instructions.get((a-1,'ENABLED'))
                if not inst or inst['mnemonic']!='RST' or inst['operands']!='$0028':
                    fail(f'{sid} is no longer immediately preceded by reached enabled RST $28')
                if exp[0]!=0x1C: fail(f'{sid} no longer targets patched main-task ID $1C')

        # Exact U7 semantic inheritance: only byte-identical data at the same logical address,
        # and only where the frozen Pac-Man parent has a COMPLETE DATA owner.
        exact_u7_inherited=set()
        for source,addr in daughter_keys:
            if source!='u7': continue
            if enabled[addr]==disabled[addr] and addr in parent_by_addr:
                exact_u7_inherited.add((source,addr))

        # Fail-closed unique assignment priority: targeted > structured > residual ownership closure residual > U7 exact parent.
        owner={}; category=Counter(); local_union=set()
        for key in daughter_keys:
            ev=evidence_by_key.get(key,[])
            for priority in ('targeted','structured','residual'):
                candidates=[e for e in ev if e[0]==priority]
                if candidates:
                    # Structured ranges can intentionally overlap; shortest/stable id wins ownership,
                    # while all evidence remains available. Targeted rows are non-overlapping by design.
                    chosen=sorted(candidates,key=lambda e:e[1])[0]
                    owner[key]=chosen; category[priority]+=1; local_union.add(key); break
            if key not in owner and key in exact_u7_inherited:
                pr=parent_by_addr[key[1]]
                owner[key]=('u7_inherited',pr['span_id'],pr['semantic_label'],pr['semantic_detail'])
                category['u7_inherited']+=1
        # Base owners are the complete Pac-Man reference.
        for key in base_keys:
            pr=parent_by_addr[key[1]]
            owner[key]=('base_inherited',pr['span_id'],pr['semantic_label'],pr['semantic_detail'])
            category['base_inherited']+=1

        missing=[k for k in data_keys if k not in owner]
        if missing:
            sample=', '.join(f'{s}:${a:04X}' for s,a in missing[:20])
            fail(f'{len(missing)} DATA bytes remain without semantic owner: {sample}')
        if len(owner)!=12382: fail(f'owner cardinality mismatch {len(owner)}')

        # Ensure the targeted semantic set is genuinely closing the previously identified semantic gap.
        # A target byte may overlap an exact U7 inherited byte, so the net new coverage is 71 bytes.
        pre_local=set(k for k in daughter_keys if any(e[0] in ('structured','residual') for e in evidence_by_key.get(k,[])))
        pre_covered=pre_local | exact_u7_inherited
        net_target=len(target_new_keys-pre_covered)
        if net_target!=71: fail(f'expected targeted semantics to close exactly 71 previously uncovered daughterboard DATA bytes, got {net_target}')

        # Emit a ROM-free semantic ownership catalog for review. It contains identifiers/addresses only.
        out=analysis/'data_semantic_ownership.csv'
        with out.open('w',newline='',encoding='utf-8') as f:
            w=csv.writer(f); w.writerow(['source','logical_address','owner_category','owner_id','semantic_label'])
            for source,addr in sorted(data_keys,key=lambda k:(k[0],k[1])):
                o=owner[(source,addr)]
                w.writerow([source,f'0x{addr:04X}',o[0],o[1],o[2]])
        if catalog_out is not None:
            catalog_out.parent.mkdir(parents=True,exist_ok=True)
            catalog_out.write_bytes(out.read_bytes())

        # Additional transparent metrics.
        daughter_total=len(daughter_keys)
        exact_u7=len(exact_u7_inherited)
        local_total=len(local_union)
        print('data-semantic verification independent exhaustive semantic DATA ownership audit: VERIFIED')
        print('  fixed DATA denominator: 12382/12382 semantic bytes')
        print('  frozen Pac-Man base DATA inheritance: 4783/4783')
        print(f'  daughterboard physical DATA: {daughter_total}/{daughter_total}')
        print(f'  local daughterboard semantic evidence union: {local_total}/{daughter_total}')
        print(f'  exact U7 Pac-Man semantic inheritance eligible: {exact_u7}/2483')
        print(f'  targeted data-semantic verification semantic closure: 71/71 previously uncovered bytes ({len(target_new_keys)} targeted physical bytes; one also exact-inherited)')
        print(f"  unique owner categories: base={category['base_inherited']} targeted={category['targeted']} structured={category['structured']} residual={category['residual']} u7_inherited={category['u7_inherited']}")
        print('  duplicated/inherited U7 DATA is not counted as a new Ms.-specific semantic family')
    return 0

if __name__=='__main__':
    raise SystemExit(main())
