"""Read actual independently decoded Mesh/Renderer bytes, not writer plans."""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import sys

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from nativepack.ini_reader import read_package
from nativepack.payload import read_payload
from nativepack.mesh import expand_shaped_frames
from nativepack.serialized_file import SerializedFile
from nativepack.type_tree import decode, encode, require
from verify_cpp_renderer_input import reference


def load_files(schema):
    result = {}
    for manifest in json.loads(schema.read_text(encoding='utf8'))['files']:
        data = Path(manifest['snapshot']['file']).read_bytes()
        result[manifest['cab']] = SerializedFile(data, manifest, manifest)
    return result


def hierarchy(file):
    trees = {pid: file.tree(pid) for pid in file.order
             if file.objects[pid]['classId'] in (1, 4)}
    parents, owners, paths, roots = {}, {}, {}, {}
    for tid, tree in trees.items():
        if file.objects[tid]['classId'] != 4:
            continue
        owner_cab, owner = reference(file, tree['m_GameObject'])
        parent_cab, parent = reference(file, tree['m_Father'])
        require(owner_cab == parent_cab == file.cab, 'nonlocal hierarchy')
        parents[tid], owners[owner] = parent, tid

    def visit(tid, stack=None):
        if tid in paths:
            return
        stack = set() if stack is None else stack
        require(tid not in stack, 'hierarchy cycle')
        stack.add(tid)
        parent = parents[tid]
        if parent:
            visit(parent, stack)
            name = trees[reference(file, trees[tid]['m_GameObject'])[1]]['m_Name'].data
            paths[tid] = paths[parent] + '/' + name
            roots[tid] = roots[parent]
        else:
            paths[tid], roots[tid] = '', tid
        stack.remove(tid)

    for tid in parents:
        visit(tid)
    return parents, owners, paths, roots


def audit_mesh(file, pid, author, package):
    mesh = file.tree(pid)
    # Native input fields are a separate author artifact; compare actual bytes.
    for field in author['nativeFields']:
        if field['name'] == 'm_VertexData':
            expected = decode(mesh[field['name']].schema, read_payload(package, field))
            channels = expected['m_Channels'].items()
            actual_channels = mesh[field['name']]['m_Channels'].items()
            for i in range(4, 12):
                if channels[i]['dimension'].data & 15:
                    channels[i].set('dimension', actual_channels[i]['dimension'].data)
            expected_mesh = copy.deepcopy(mesh)
            expected_mesh.data[field['name']] = expected
            expand_shaped_frames(expected_mesh)
            require(encode(expected_mesh[field['name']]) == encode(mesh[field['name']]),
                    'actual vertex bytes differ from native author frame-storage rule')
        else:
            require(encode(mesh[field['name']]) == read_payload(package, field),
                    'actual native author field differs: ' + field['name'])
    bindposes = len(mesh['m_BindPose'].items())
    require(bindposes == len(author['bonePaths']) == len(mesh['m_BoneNameHashes'].items()), 'palette counts')
    vertex = mesh['m_VertexData']
    channels = [c.plain() for c in vertex['m_Channels'].items()]
    require(channels[12] == dict(stream=2, offset=0, format=4, dimension=4)
            and channels[13] == dict(stream=2, offset=8, format=6, dimension=4), 'skin channels')
    strides = {}
    for c in channels:
        dim = c['dimension'] & 15
        if dim:
            width = {0: 4, 4: 2, 6: 1}[c['format']]
            strides[c['stream']] = max(strides.get(c['stream'], 0), c['offset'] + dim * width)
    count, data, start = vertex['m_VertexCount'].data, vertex['m_DataSize'].data, 0
    for stream in range(2):
        start = (start + count * strides.get(stream, 0) + 15) & ~15
    require(strides[2] == 12 and start + count * 12 <= len(data), 'skin stream bounds')
    skin = np.frombuffer(data, dtype=[('weights', '<u2', (4,)), ('bones', 'u1', (4,))], offset=start, count=count)
    require(np.all(skin['weights'].astype(np.uint32).sum(axis=1) == 65535), 'weight normalization')
    require(np.all(skin['bones'] < bindposes), 'out of range bone slots')
    positions = np.ndarray((count,3), dtype='<f4', buffer=data, strides=(strides[0],4))
    require(np.all(np.isfinite(positions)), 'nonfinite geometry')
    indices = np.frombuffer(bytes(mesh['m_IndexBuffer'].items()),
                            dtype='<u4' if mesh['m_IndexFormat'].data else '<u2')
    require(len(indices) % 3 == 0 and np.all(indices < count), 'triangle index range')
    return dict(cab=file.cab, mesh=pid, part=author['partId'], vertices=count,
                indices=len(indices), indexBits=32 if mesh['m_IndexFormat'].data else 16,
                bindposes=bindposes, maxBoneSlot=int(skin['bones'].max()),
                normalizedWeights=True, geometryIndicesInRange=True, authorBytesEqual=True)


def audit_rest_pose(file, mesh_file, item, parents, owners, worlds):
    """CPU skin actual deployed vertices with the unchanged serialized rest rig."""
    def world(pid):
        if pid in worlds:
            return worlds[pid]
        value = file.tree(pid)
        x, y, z, w = [value['m_LocalRotation'][k].data for k in 'xyzw']
        rotation = np.array([
            [1-2*y*y-2*z*z, 2*x*y-2*z*w, 2*x*z+2*y*w],
            [2*x*y+2*z*w, 1-2*x*x-2*z*z, 2*y*z-2*x*w],
            [2*x*z-2*y*w, 2*y*z+2*x*w, 1-2*x*x-2*y*y]])
        local = np.eye(4)
        local[:3, :3] = rotation @ np.diag([value['m_LocalScale'][k].data for k in 'xyz'])
        local[:3, 3] = [value['m_LocalPosition'][k].data for k in 'xyz']
        worlds[pid] = world(parents[pid]) @ local if parents[pid] else local
        return worlds[pid]

    renderer, mesh = file.tree(item['renderer']), mesh_file.tree(item['mesh'])
    owner = owners[reference(file, renderer['m_GameObject'])[1]]
    bones = [reference(file, b)[1] for b in renderer['m_Bones'].items()]
    poses = [np.array([[p[f'e{r}{c}'].data for c in range(4)] for r in range(4)])
             for p in mesh['m_BindPose'].items()]
    matrices = np.array([np.linalg.inv(world(owner)) @ world(b) @ p for b, p in zip(bones, poses)])
    vertex = mesh['m_VertexData']
    count, data = vertex['m_VertexCount'].data, vertex['m_DataSize'].data
    channels = [c.plain() for c in vertex['m_Channels'].items()]
    strides = {}
    for channel in channels:
        dim = channel['dimension'] & 15
        if dim:
            end = channel['offset'] + dim * {0: 4, 4: 2, 6: 1}[channel['format']]
            strides[channel['stream']] = max(strides.get(channel['stream'], 0), end)
    start = 0
    for stream in range(2):
        start = (start + count * strides.get(stream, 0) + 15) & ~15
    skin = np.frombuffer(data, dtype=[('weights', '<u2', (4,)), ('bones', 'u1', (4,))], offset=start, count=count)
    positions = np.ndarray((count,3), dtype='<f4', buffer=data, strides=(strides[0],4))
    homogeneous = np.column_stack((positions, np.ones(count)))
    deformed = np.zeros((count, 3))
    for influence in range(4):
        weight = skin['weights'][:, influence].astype(np.float64) / 65535
        matrix = matrices[skin['bones'][:, influence]]
        deformed += np.einsum('vij,vj->vi', matrix[:, :3], homogeneous) * weight[:, None]
    rigid = (homogeneous @ matrices[0].T)[:, :3]
    indices = np.frombuffer(bytes(mesh['m_IndexBuffer'].items()), dtype='<u4' if mesh['m_IndexFormat'].data else '<u2').reshape(-1, 3)

    def longest_edge(points):
        triangles = points[indices]
        return float(np.linalg.norm(triangles - np.roll(triangles, 1, axis=1), axis=2).max())

    return dict(restBindingSpread=float(np.max(np.abs(matrices - matrices[0]))),
                maxRestVertexDeviation=float(np.linalg.norm(deformed - rigid, axis=1).max()),
                longestRestEdge=longest_edge(deformed), longestRigidEdge=longest_edge(rigid),
                rendererToRootBind=matrices[0].tolist(), nativeAnimationReadback=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('schema', 'plan', 'mod', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--paired-schema', type=Path)
    args = parser.parse_args()
    require(not args.output.exists(), 'audit output exists')
    package = read_package(args.mod)
    parts = {p['partId']: p for p in package['parts']}
    files = load_files(args.schema)
    plan = json.loads(args.plan.read_text(encoding='utf8'))
    report = dict(format='EFF_DEPLOYED_SKINNING_AUDIT', version=1,
                  nativeRuntimeReadback=False, visualReplacementVerified=False,
                  meshes=[], renderers=[], meshInputsOutsideDecodedSet=[])
    audited_meshes = set()
    hierarchies = {}
    rest_worlds = {}
    for item in plan['parts']:
        file = files[item['cab']]
        renderer = file.tree(item['renderer'])
        mesh_ref = reference(file, renderer['m_Mesh'])
        require(mesh_ref == (item['meshCab'], item['mesh']), 'actual Mesh PPtr differs')
        bones = [reference(file, b) for b in renderer['m_Bones'].items()]
        require(bones == [(file.cab, b) for b in item['bones']], 'actual palette differs from plan')
        require(len(bones) == len(set(bones)), 'duplicate bones')
        if file.cab not in hierarchies:
            hierarchies[file.cab] = hierarchy(file)
        parents, owners, paths, roots = hierarchies[file.cab]
        author = parts[item['part']]
        owner = reference(file, renderer['m_GameObject'])[1]
        tid = owners[owner]
        root = reference(file, renderer['m_RootBone'])[1]
        ancestors, node = set(), tid
        while node:
            ancestors.add(node)
            node = parents[node]
        candidates, anchor = [], root
        while anchor:
            if anchor in ancestors:
                prefix = paths[anchor] + '/'
                actual_paths = [paths[b[1]] for b in bones]
                expected_paths = [prefix + path for path in author['bonePaths']]
                if actual_paths == expected_paths and all(roots[b[1]] == roots[tid] for b in bones):
                    candidates.append(anchor)
            anchor = parents[anchor]
        require(len(candidates) == 1 and sum(b[1] == root for b in bones) == 1,
                'actual per-slot paths differ from independently selected author anchor')
        rest = audit_rest_pose(file, files[mesh_ref[0]], item, parents, owners,
                               rest_worlds.setdefault(file.cab, {})) if mesh_ref[0] in files else None
        report['renderers'].append(dict(renderer=item['renderer'], part=item['part'],
            sourceRenderer=item['sourceRenderer'], boneSlots=len(bones), authorPathsEqual=True,
            meshPointerEqual=True, rootBoneSlot=[b[1] for b in bones].index(root),
            skinningRoot=reference(file, renderer['m_SkinningRoot'])[1],
            paletteSha256=hashlib.sha256(json.dumps(bones).encode()).hexdigest().upper(), restPose=rest))
        if mesh_ref not in audited_meshes:
            audited_meshes.add(mesh_ref)
            if mesh_ref[0] in files:
                report['meshes'].append(audit_mesh(files[mesh_ref[0]], mesh_ref[1], author, package))
            else:
                report['meshInputsOutsideDecodedSet'].append(list(mesh_ref))
    if args.paired_schema:
        paired = load_files(args.paired_schema)
        comparisons = []
        for cab, source in paired.items():
            if cab not in files:
                continue
            for pid in source.order:
                if source.objects[pid]['classId'] != 137 or pid not in files[cab].objects:
                    continue
                # Compare the accepted diagnostic Renderer, not unrelated ones.
                if not any(r['sourceRenderer'] == pid for r in report['renderers']):
                    continue
                before, after = source.tree(pid), files[cab].tree(pid)
                differences = [k for k in before.data if encode(before[k]) != encode(after[k])]
                comparisons.append(dict(renderer=pid, changedFields=differences))
        report['pairedRendererComparisons'] = comparisons
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(json.dumps(dict(meshes=len(report['meshes']), renderers=len(report['renderers']),
                         paletteCountsByPart={m['part']: m['bindposes'] for m in report['meshes']},
                         meshesUsing32BitIndices=sum(m['indexBits'] == 32 for m in report['meshes']),
                         missingMeshInputs=len(report['meshInputsOutsideDecodedSet']),
                         paired=report.get('pairedRendererComparisons'))))


if __name__ == '__main__':
    main()
