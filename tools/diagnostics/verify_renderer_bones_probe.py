"""Verify a bones-only consumer probe against current hierarchy and author skin data."""
from __future__ import annotations

import argparse
import copy
import json
from pathlib import Path
import struct
import sys

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from nativepack.ini_reader import read_package
from nativepack.serialized_file import SerializedFile
from nativepack.type_tree import decode, require
from verify_cpp_mesh_input import sha, verify_serialized
from verify_cpp_renderer_input import reference, pointer


def verify(args):
    package = read_package(args.mod)
    plan = json.loads((args.cache / 'consumer-bones.vfs.json').read_text())
    require(len(plan['parts']) == 1, 'this minimized probe must select one consumer')
    selected = plan['parts'][0]
    author = next(p for p in package['parts'] if p['partId'] == plan['part'])
    evidence = json.loads((args.run / 'current-consumer-schema.json').read_text())
    manifest = next(f for f in evidence['files'] if f['cab'] == selected['cab'])
    cab = manifest['cab']
    before = (args.run / 'independent-current-consumers-source/input-033' / (cab + '.bytes')).read_bytes()
    after = (args.cache / 'consumer-bones-members' / (cab + '.bytes')).read_bytes()
    native = SerializedFile(before, manifest, manifest)
    trees = {pid: native.tree(pid) for pid in native.order
             if native.objects[pid]['classId'] in (1, 4, 137)}
    parents, transforms, paths, roots, index = {}, {}, {}, {}, {}
    for tid, tree in trees.items():
        if native.objects[tid]['classId'] != 4:
            continue
        owner_cab, owner = reference(native, tree['m_GameObject'])
        parent_cab, parent = reference(native, tree['m_Father'])
        require(owner_cab == parent_cab == cab, 'bone hierarchy must be local')
        transforms[owner], parents[tid] = tid, parent

    def visit(tid, visiting=None):
        if tid in roots:
            return
        visiting = set() if visiting is None else visiting
        require(tid not in visiting, 'hierarchy cycle')
        visiting.add(tid)
        parent = parents[tid]
        if parent:
            visit(parent, visiting)
            name = trees[reference(native, trees[tid]['m_GameObject'])[1]]['m_Name'].data
            paths[tid] = (paths[parent] + '/' if paths[parent] else '') + name
            roots[tid] = roots[parent]
        else:
            paths[tid], roots[tid] = '', tid
        key = roots[tid], paths[tid]
        index[key] = tid if key not in index else None
        visiting.remove(tid)

    for tid in parents:
        visit(tid)
    rid = selected['renderer']
    original = trees[rid]
    tid = transforms[reference(native, original['m_GameObject'])[1]]
    root_bone = reference(native, original['m_RootBone'])[1]
    ancestors, node = set(), tid
    while node:
        ancestors.add(node)
        node = parents[node]
    palettes, anchor = [], root_bone
    while anchor:
        if anchor in ancestors:
            prefix = paths[anchor]
            expected_paths = [(prefix + '/' if prefix else '') + p for p in author['bonePaths']]
            bones = [index.get((roots[tid], p)) for p in expected_paths]
            if all(bones) and bones.count(root_bone) == 1:
                palettes.append(bones)
        anchor = parents[anchor]
    require(len(palettes) == 1, 'author paths need one unique hierarchy anchor')
    bones = palettes[0]
    require(bones == selected['bones'] and len(bones) == len(set(bones)),
            'C++ bones/order differ from independently resolved author paths')
    rebound = copy.deepcopy(original)
    rebound['m_Bones'].set_items([pointer(native, (cab, pid)) for pid in bones])
    native.write_tree(rid, rebound)
    verify_serialized(native, after)

    bundles = [pid for pid in native.order if native.objects[pid]['classId'] == 142]
    require(len(bundles) == 1, 'one original AssetBundle required')
    bundle = native.tree(bundles[0])
    preloads = [reference(native, p) for p in bundle['m_PreloadTable'].items()]
    covered = 0
    for field in ('m_Container', 'm_HashContainer'):
        for row in bundle[field].items():
            info = row['second'].plain()
            references = set(preloads[info['preloadIndex']:info['preloadIndex'] + info['preloadSize']])
            if (cab, rid) in references:
                require(all((cab, pid) in references for pid in bones), 'new bone missing from original preload')
                covered += 1
    require(covered, 'consumer absent from original preload intervals')

    mesh_cab, mesh_id = reference(native, original['m_Mesh'])
    baseline = json.loads((args.baseline / 'source/manifest.json').read_text())
    mesh_manifest = next(f for f in baseline['serializedFiles'] if f['cab'] == mesh_cab)
    schemas = json.loads(args.schema.read_text())
    mesh_schema = next(f for f in schemas['files'] if f['cab'] == mesh_cab)
    source_mesh = SerializedFile((args.baseline / mesh_manifest['snapshot']['file']).read_bytes(),
                                 mesh_manifest, mesh_schema)
    probe = (args.mesh_cache / 'first-part-only-members' / (mesh_cab + '.bytes')).read_bytes()
    count = struct.unpack_from('<I', probe, source_mesh.count_offset)[0]
    data_offset = struct.unpack_from('>Q', probe, 32)[0]
    rows = [struct.unpack_from('<qqIi', probe, source_mesh.table_start + i * 24) for i in range(count)]
    _, offset, size, kind = next(r for r in rows if r[0] == mesh_id)
    mesh = decode(source_mesh.schemas[kind], probe[data_offset + offset:data_offset + offset + size])
    bindposes = len(mesh['m_BindPose'].items())
    require(bindposes == len(bones) == len(author['bonePaths']), 'paired palette/bindpose count mismatch')
    vertex = mesh['m_VertexData']
    channels = [c.plain() for c in vertex['m_Channels'].items()]
    require(channels[12] == dict(stream=2, offset=0, format=4, dimension=4) and
            channels[13] == dict(stream=2, offset=8, format=6, dimension=4), 'unsupported author skin stream')
    widths = {0: 4, 4: 2, 6: 1}
    strides = {}
    for c in channels:
        dimension = c['dimension'] & 15
        if dimension:
            require(c['format'] in widths, 'unsupported channel format in skin audit')
            strides[c['stream']] = max(strides.get(c['stream'], 0), c['offset'] + dimension * widths[c['format']])
    vertices, start = vertex['m_VertexCount'].data, 0
    data = vertex['m_DataSize'].data
    for stream in range(2):
        start = (start + vertices * strides.get(stream, 0) + 15) & ~15
    require(strides[2] == 12 and start + vertices * 12 <= len(data), 'skin stream range')
    skin = np.frombuffer(data, dtype=[('weights', '<u2', (4,)), ('bones', 'u1', (4,))],
                         offset=start, count=vertices)
    require(np.all(skin['weights'].astype(np.uint32).sum(axis=1) == 65535), 'skin weights not normalized')
    require(np.all(skin['bones'] < bindposes), 'skin bone index out of range')
    original_count = len(original['m_Bones'].items())
    beyond = (skin['bones'] >= original_count) & (skin['weights'] > 0)
    return dict(format='EFF_RENDERER_BONES_PROBE_VERIFICATION', version=1, verified=True,
                renderer=rid, part=plan['part'], sourceObjectCount=len(native.order),
                changedObjects=1, addedObjects=0, originalRendererBones=original_count,
                pairedRendererBones=len(bones), bindPoseCount=bindposes, vertices=vertices,
                maxSkinBoneIndex=int(skin['bones'].max()),
                verticesUsingBoneBeyondOriginalPalette=int(np.any(beyond, axis=1).sum()),
                independentlyResolvedFullAuthorPaths=True, authorOrderPreserved=True,
                allBonesExistingAndAlreadyPreloaded=True, originalSkeletonBytesPreserved=True,
                allOtherRendererFieldsPreserved=True, allOtherObjectsPreserved=True,
                originalPreloadAndObjectOrderPreserved=True, skinWeightsNormalized=True,
                allSkinIndicesInPairedPalette=True,
                consumerOutputSha256=sha((args.cache / 'consumer-bones.vfs').read_bytes()),
                meshOutputSha256=sha((args.mesh_cache / 'first-part-only.vfs').read_bytes()),
                nativeAcceptanceVerified=False, visualReplacementVerified=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('mod', 'run', 'cache', 'mesh-cache', 'baseline', 'schema', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    require(not args.output.exists(), 'verification report exists')
    report = verify(args)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
