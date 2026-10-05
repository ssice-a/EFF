"""Select current loading inputs from logical Mod declarations and indexed dependencies.

The index only locates candidates. The C++ writer must re-resolve all target
identities from the actual input bytes. No prefab name whitelist is used.
"""
from __future__ import annotations
import argparse
from collections import defaultdict, deque
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from nativepack.ini_reader import read_package
from nativepack.type_tree import require
from build_prefab_mesh_candidates import records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mod', type=Path, required=True)
    parser.add_argument('--index', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    require(not args.output.exists(), 'current graph selection already exists')
    mod = read_package(args.mod)
    targets = {('Mesh', t['sourcePath'], t['sourceAsset']) for r in mod['rules'] for t in r['targets']}
    resources = targets | {(kind, r['sourcePath'], r['sourceAsset'])
                           for kind, values in [('Material', mod['materials']), ('Texture2D', mod['textures'])]
                           for r in values}
    located, cabs, source_cabs = defaultdict(set), {}, defaultdict(set)
    for kind, row in records(args.index):
        if kind == 'header':
            header = row
        elif kind == 'asset':
            key = (row['type'], row['container'], row['name'])
            if key in resources:
                located[key].add(row['source'])
        elif kind == 'cab':
            name = row['name'].lower()
            require(name not in cabs or cabs[name] == row, 'ambiguous index CAB locator')
            cabs[name] = row
            source_cabs[row['source']].add(name)
    require(all(len(located[r]) == 1 for r in resources), 'missing/ambiguous logical resource index locator')
    reverse = defaultdict(set)
    for cab, row in cabs.items():
        for dep in row['dependencies']:
            reverse[dep.replace('\\', '/').rsplit('/', 1)[-1].lower()].add(cab)
    reached = set(c for target in targets for c in source_cabs[next(iter(located[target]))])
    queue = deque(reached)
    while queue:
        for consumer in reverse[queue.popleft()]:
            if consumer not in reached:
                reached.add(consumer)
                queue.append(consumer)
    selected = {next(iter(located[r])) for r in resources}
    consumers = {cabs[c]['source'] for c in reached} - selected
    report = dict(format='EFF_CURRENT_INPUT_CANDIDATES', version=1, modId=mod['modId'],
                  locatorIndex=str(args.index.resolve()), vfsFingerprint=header['fingerprint'],
                  resourceIdentities=len(resources), meshIdentities=len(targets),
                  sourceInputs=sorted(selected), consumerInputs=sorted(consumers),
                  sources=sorted(selected | consumers),
                  identityMatching='actual input logical path + asset; index is only a candidate locator',
                  prefabWhitelist=False, nativeSubmission=False)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(json.dumps({k: report[k] for k in ('resourceIdentities', 'meshIdentities', 'prefabWhitelist')} |
                     dict(sourceInputs=len(selected), consumerInputs=len(consumers))))


if __name__ == '__main__':
    main()
