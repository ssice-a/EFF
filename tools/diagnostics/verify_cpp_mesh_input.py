"""Compare C++ Mesh-input rebuilding with the pre-existing Python writer.

Inputs must first be independently decoded with AnimeStudio. Baseline bytes
must equal the actual captured members; source consumer lists are not used.
No game submission or Renderer/dependency-closure claim is made here.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from nativepack.delta_apply import apply_mesh_fields
from nativepack.ini_reader import read_package
from nativepack.payload import read_payload
from nativepack.selected_experiment import SelectedExperiment
from nativepack.serialized_file import SerializedFile
from nativepack.type_tree import require
from validate_selected_mod_author import SourceCatalog


def sha(data):
    return hashlib.sha256(data).hexdigest().upper()


def verify_serialized(native, after):
    """Independently read rows; added row order is not a resource identity."""
    before = native.original
    metadata_size, file_size, data_offset = struct.unpack_from('>Iqq', after, 20)
    require(file_size == len(after) and 48 + metadata_size <= data_offset <= len(after)
            and data_offset % 16 == 0, 'rebuilt extended header/ranges')
    require(before[:20] == after[:20] and before[40:48] == after[40:48],
            'legacy/reserved header fields changed')
    require(before[48:native.count_offset] == after[48:native.count_offset],
            'native type metadata changed')
    count = struct.unpack_from('<i', after, native.count_offset)[0]
    require(count == len(native.order), 'independent rebuilt object count')
    table_end = native.table_start + count * 24
    require(table_end <= 48 + metadata_size, 'rebuilt object table range')
    require(before[native.count_offset+4:native.table_start] ==
            after[native.count_offset+4:native.table_start], 'object table padding changed')
    require(before[native.table_end:48+native.metadata_size] ==
            after[table_end:48+metadata_size], 'external/ref-type/user metadata changed')
    rows = {}
    ranges = []
    order = []
    for i in range(count):
        path_id, offset, size, type_index = struct.unpack_from(
            '<qqIi', after, native.table_start + i * 24)
        require(path_id not in rows and path_id in native.objects and
                0 <= offset <= len(after) - data_offset and offset % 8 == 0 and
                size <= len(after) - data_offset - offset, 'rebuilt object identity/range')
        expected = native.objects[path_id]
        payload = after[data_offset+offset:data_offset+offset+size]
        require(type_index == expected['typeIndex'] and payload == expected['payload'],
                'independent object bytes/type differ: ' + native.cab + ':' + str(path_id))
        rows[path_id] = (offset, size)
        order.append(path_id)
        ranges.append((data_offset + offset, size))
    require(order == sorted(native.order), 'rebuilt object table is not in signed PathID order')
    end = data_offset
    for offset, size in sorted(ranges):
        require(offset >= end and not any(after[end:offset]), 'overlap/nonzero object alignment')
        end = offset + size
    require(end == len(after) and not any(after[48+metadata_size:data_offset]),
            'unreferenced data/nonzero metadata alignment')


def verify(args):
    package = read_package(args.mod)
    catalog = SourceCatalog(args.baseline)
    schema = {f['cab']: f for f in json.loads(args.schema.read_text(encoding='utf8'))['files']}
    manifests = {f['cab']: f for f in catalog.manifest['serializedFiles']}
    parts = {p['partId']: p for p in package['parts']}
    report = {'format': 'EFF_CPP_MESH_INPUT_VERIFICATION', 'version': 1,
              'verified': False, 'nativeSubmission': False, 'rendererExpansion': False,
              'dependencyClosureVerified': False, 'inputs': []}
    for captured in sorted(args.run.glob('input-*.vfs')):
        rebuilt = args.run / 'mesh-rebuilt-inputs' / captured.name
        cpp = json.loads(Path(str(rebuilt) + '.json').read_text())
        before_dir = args.run / 'independent-decoded' / captured.stem
        after_dir = args.run / 'independent-mesh-rebuilt-decoded' / captured.stem
        before_members = {p.name: p for p in before_dir.glob('*.bytes')}
        after_members = {p.name: p for p in after_dir.glob('*.bytes')}
        require(set(before_members) == set(after_members), 'member identity set changed')
        matches = skipped = changed = added = preserved = count = 0
        for name, before_path in before_members.items():
            before, after = before_path.read_bytes(), after_members[name].read_bytes()
            cab = before_path.stem
            if cab not in manifests:
                require(before == after, 'unselected input member changed: ' + name)
                continue
            manifest = manifests[cab]
            require(sha(before) == manifest['snapshot']['sha256'],
                    'baseline differs from actual captured member: ' + cab)
            native = SerializedFile(before, manifest, schema[cab])
            expansions = {}
            expected_parts = []
            for rule in package['rules']:
                for target in rule['targets']:
                    resource = catalog.resolve('Mesh', target['sourcePath'], target['sourceAsset'])
                    if resource['native']['cab'] != cab:
                        continue
                    matches += 1
                    if rule['hidden']:
                        skipped += 1
                        continue
                    source_id = resource['native']['pathId']
                    template = native.tree(source_id)
                    for i, pid in enumerate(rule['parts']):
                        part = parts[pid]
                        tree = apply_mesh_fields(template, part, lambda row: read_payload(package, row))
                        label = ':'.join((cab, package['modId'], 'mesh', target['sourcePath'],
                                          target['sourceAsset'], pid))
                        new_id = source_id if i == 0 else int.from_bytes(
                            hashlib.sha256(label.encode()).digest()[:8], 'little', signed=True)
                        name_value = target['sourceAsset'] if i == 0 else (
                            target['sourceAsset'] + '__EFF_' + package['modId'] + '_' + pid)
                        tree.set('m_Name', name_value)
                        if i == 0:
                            native.write_tree(new_id, tree)
                        else:
                            native.add_tree(new_id, source_id, tree)
                            expansions.setdefault((0, source_id), []).append(
                                {'m_FileID': 0, 'm_PathID': new_id})
                        expected_parts.append((target['sourcePath'], target['sourceAsset'], pid,
                                               source_id, new_id, part['vertices'], part['indices']))
            if expansions:
                # This reference routine predates the C++ implementation.
                SelectedExperiment.expand_preloads(None, native, expansions)
            verify_serialized(native, after)
            observed = [(p['path'], p['asset'], p['part'], p['sourceId'], p['id'],
                         p['vertices'], p['indices']) for p in cpp['parts'] if p['cab'] == cab]
            require(sorted(observed) == sorted(expected_parts), 'Mesh plan differs: ' + cab)
            changed += len(native.changed - native.added)
            added += len(native.added)
            preserved += len(native.order) - len(native.changed)
            count += len(expected_parts)
        require((matches, skipped, changed, added, count) ==
                (cpp['matches'], cpp['skipped'], cpp['changedObjects'], cpp['addedObjects'],
                 len(cpp['parts'])), 'C++ plan counters differ: ' + captured.name)
        if not changed:
            require(captured.read_bytes() == rebuilt.read_bytes(), 'no-op input bytes changed')
        report['inputs'].append({'file': captured.name, 'matches': matches, 'skipped': skipped,
                                  'parts': count, 'changedObjects': changed, 'addedObjects': added,
                                  'preservedObjects': preserved, 'rebuiltBytes': rebuilt.stat().st_size,
                                  'sha256': sha(rebuilt.read_bytes())})
    report['verified'] = True
    for key in ('matches', 'skipped', 'parts', 'changedObjects', 'addedObjects', 'preservedObjects'):
        report[key] = sum(row[key] for row in report['inputs'])
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('mod', 'baseline', 'schema', 'run', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    require(not args.output.exists(), 'verification report already exists')
    report = verify(args)
    args.output.write_text(json.dumps(report, indent=2, ensure_ascii=False) + '\n', encoding='utf8')
    print(json.dumps({k: v for k, v in report.items() if k != 'inputs'}))


if __name__ == '__main__':
    main()
