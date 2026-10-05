"""Audit each part palette and serialized cloth skinning caches without mutation."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from nativepack.ini_reader import read_package
from nativepack.serialized_file import SerializedFile
from nativepack.type_tree import Value


FIELDS = {
    'sourceRenderers', 'skinningBones', 'customSkinningSetting',
    'skinBoneCount', 'skinRootBoneIndex', 'skinRootIndex',
    'skinBoneTransformIndices', 'skinBoneBindPoses', 'customSkinningBoneIndices',
    'renderer', 'skinRenderer', 'renderTransformIndex', 'transformList',
    'boneWeights', 'boneWeightArray', 'bonesPerVertexArray', 'isBoneCloth',
    'vertexCount', 'vertexDataCount', 'positions', 'localPositions',
}


def summarize(value):
    if isinstance(value, Value):
        return summarize(value.data)
    if isinstance(value, dict):
        return {key: summarize(item) for key, item in value.items()}
    if isinstance(value, bytes):
        return dict(bytes=len(value), sha256=hashlib.sha256(value).hexdigest().upper(),
                    head=value[:64].hex())
    if isinstance(value, list):
        if len(value) > 100:
            return dict(count=len(value), sample=[summarize(v) for v in value[:8]])
        return [summarize(v) for v in value]
    return value


def walk(value, path=''):
    name = value.schema.name
    path = path + '/' + name
    if name in FIELDS:
        yield dict(path=path, type=value.schema.type, value=summarize(value))
    if isinstance(value.data, dict):
        for child in value.data.values():
            yield from walk(child, path)
    elif isinstance(value.data, list):
        for index, child in enumerate(value.data):
            yield from walk(child, path + '/' + str(index))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('evidence', 'source', 'plan', 'mod', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError('skinning metadata audit exists')
    plan = json.loads(args.plan.read_text(encoding='utf8'))
    package = read_package(args.mod)
    parts = {p['partId']: p for p in package['parts']}
    cabs = {p['cab'] for p in plan['parts']}
    evidence = json.loads(args.evidence.read_text(encoding='utf8'))
    report = dict(format='EFF_RENDERER_SKINNING_METADATA_AUDIT', version=1,
                  nativeAcceptanceVerified=False, parts=[], components=[])
    for part in plan['parts']:
        palette = part['bones']
        author = parts[part['part']]
        if len(palette) != len(author['bonePaths']) or len(palette) != len(set(palette)):
            raise ValueError('invalid per-part palette')
        report['parts'].append(dict(part=part['part'], renderer=part['renderer'],
            sourceRenderer=part['sourceRenderer'], bones=len(palette),
            paletteSha256=hashlib.sha256(json.dumps(palette).encode()).hexdigest().upper(),
            authorPathsSha256=hashlib.sha256(json.dumps(author['bonePaths']).encode()).hexdigest().upper()))
    for manifest in evidence['files']:
        if manifest['cab'] not in cabs:
            continue
        path = args.source / (manifest['cab'] + '.bytes')
        file = SerializedFile(path.read_bytes(), manifest, manifest)
        for pid in file.order:
            row = file.objects[pid]
            if row['classId'] != 114:
                continue
            schema = file.schemas[row['typeIndex']]
            if not any(node['name'] in FIELDS for node in manifest['types'][row['typeIndex']]['nodes']):
                continue
            tree = file.tree(pid)
            fields = list(walk(tree))
            report['components'].append(dict(cab=file.cab, pathId=pid,
                typeHash=manifest['types'][row['typeIndex']]['oldTypeHash'],
                fields=fields))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(json.dumps(dict(parts=report['parts'], components=[
        dict(pathId=c['pathId'], typeHash=c['typeHash'], fields=len(c['fields']))
        for c in report['components']])))


if __name__ == '__main__':
    main()
