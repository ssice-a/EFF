"""Select observed effect PFBs and write a small native-format diagnostic index.

This also normalizes the legacy EIEM index header to the current EFF header;
records and identities retain their meaning. No game/Mod resource is modified.
"""
from __future__ import annotations
import argparse
from collections import defaultdict, deque
import json
from pathlib import Path
import re
from build_prefab_mesh_candidates import records


def string(handle, value: str):
    data = value.encode('utf-8')
    length = len(data)
    while length >= 128:
        handle.write(bytes([(length & 127) | 128]))
        length >>= 7
    handle.write(bytes([length]))
    handle.write(data)


def build(index: Path, events: Path, seeds: Path, output: Path, pattern: str):
    runtime = json.loads(events.read_text(encoding='utf-8-sig'))
    wanted = {p['path'].lower() for p in runtime['paths']
              if p['path'].lower().endswith(('.prefab', '.pfb', '.asset')) and re.search(pattern, p['path'], re.I)}
    cabs = {}
    prefab_records = defaultdict(list)
    for kind, row in records(index):
        if kind == 'header':
            header = row
        elif kind == 'cab':
            cabs[row['name'].lower()] = row
        elif kind == 'asset' and row['container'].lower() in wanted:
            prefab_records[row['container'].lower()].append(row)
    missing = wanted - prefab_records.keys()
    if missing:
        raise ValueError(f'Observed PFB paths absent from index: {sorted(missing)}')
    sources = {r['source'] for rows in prefab_records.values() for r in rows}
    by_source = defaultdict(set)
    for key, row in cabs.items():
        by_source[row['source']].add(key)
    closure = set()
    queue = deque(c for source in sources for c in by_source[source])
    unresolved = set()
    while queue:
        cab = queue.popleft()
        if cab in closure:
            continue
        if cab not in cabs:
            unresolved.add(cab)
            continue
        closure.add(cab)
        queue.extend(d.replace('\\', '/').rsplit('/', 1)[-1].lower() for d in cabs[cab]['dependencies'])
    closure_sources = {cabs[c]['source'] for c in closure}
    # Retain every CAB in a selected source bundle, just as the full index does.
    selected_cabs = {c for s in closure_sources for c in by_source[s]}
    output.mkdir(parents=True, exist_ok=True)
    small_index = output / 'effect-resources.eidx'
    asset_count = 0
    with small_index.open('wb') as handle:
        string(handle, 'EFF_END_FIELD_INDEX')
        handle.write((3).to_bytes(4, 'little', signed=True))
        string(handle, header['fingerprint'])
        for kind, row in records(index):
            if kind != 'asset' or row['source'] not in closure_sources:
                continue
            handle.write(bytes([1]))
            for field in ('name', 'container', 'type'):
                string(handle, row[field])
            handle.write(int(row['pathID']).to_bytes(8, 'little', signed=True))
            string(handle, row['source'])
            asset_count += 1
        for cab in sorted(selected_cabs):
            row = cabs[cab]
            handle.write(bytes([2]))
            string(handle, row['name'])
            string(handle, row['source'])
            handle.write(len(row['dependencies']).to_bytes(4, 'little', signed=True))
            for dependency in row['dependencies']:
                string(handle, dependency)
        handle.write(bytes([0]))
        handle.write(asset_count.to_bytes(8, 'little', signed=True))
        handle.write(len(selected_cabs).to_bytes(8, 'little', signed=True))
    seed_manifest = json.loads(seeds.read_text(encoding='utf-8-sig'))
    manifest = dict(format=1, offlineEvidenceOnly=True, replacementRulesGenerated=False,
        fingerprint=header['fingerprint'], targets=seed_manifest['targets'],
        resources=[dict(source=rows[0]['source'], container=key, records=rows)
                   for key, rows in sorted(prefab_records.items())],
        candidates=[dict(source=rows[0]['source'], container=key, records=rows,
                        candidateReason='observed-runtime-PFB-path-not-draw-proof')
                    for key, rows in sorted(prefab_records.items()) if key.endswith(('.prefab', '.pfb'))],
        bundles=[dict(source=s, cabs=[cabs[c]['name'] for c in sorted(by_source[s])]) for s in sorted(sources)],
        unresolvedExternalCabs=sorted(unresolved), smallIndex=str(small_index.resolve()),
        limits=['Runtime-requested paths are not proof of visible effects.',
                'Only the matching observed effects and their declared dependency closure are selected.'])
    (output / 'effect-candidates.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f"Observed resources={len(wanted)}; closure assets={asset_count}, cabs={len(selected_cabs)}, "
          f"unresolved externals={len(unresolved)}; index={small_index}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('index', type=Path)
    parser.add_argument('events', type=Path)
    parser.add_argument('seeds', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--pattern', required=True)
    args = parser.parse_args()
    build(args.index, args.events, args.seeds, args.output, args.pattern)


if __name__ == '__main__':
    main()
