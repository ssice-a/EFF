"""Independently verify Renderer expansion from current decoded input references.

AnimeStudio must decode the source and rebuilt VFS inputs first. No game
submission or dynamic Renderer registration claim is made by this check.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import struct
import sys

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from nativepack.ini_reader import read_package
from nativepack.delta_apply import apply_mesh_fields
from nativepack.mesh import renderer_bounds
from nativepack.payload import read_payload
from nativepack.serialized_file import SerializedFile
from nativepack.type_tree import decode, require
from validate_selected_mod_author import SourceCatalog
from verify_cpp_mesh_input import sha, verify_serialized


def allocate(cab, mod, label):
    return int.from_bytes(hashlib.sha256((cab + ':' + mod + ':' + label).encode()).digest()[:8],
                          'little', signed=True)


def pointer(file, reference):
    cab, pid = reference
    names = [e['path'].replace('\\', '/').rsplit('/', 1)[-1] for e in file.manifest['externals']]
    return dict(m_FileID=0 if cab == file.cab else names.index(cab) + 1, m_PathID=pid)


def reference(file, value):
    fid, pid = value['m_FileID'].data, value['m_PathID'].data
    return ((file.cab if not fid else file.manifest['externals'][fid - 1]['path'].replace('\\', '/').rsplit('/', 1)[-1]), pid)


def author_bounds(mesh, root_slot):
    count = mesh['m_VertexData']['m_VertexCount'].data
    data = mesh['m_VertexData']['m_DataSize'].data
    positions = np.frombuffer(data[:count * 16], dtype='<f4').reshape(count, 4)[:, :3].astype(np.float64)
    shapes = mesh['m_Shapes']
    vector = lambda v: [v[k].data for k in ('x', 'y', 'z')]
    author = dict(vertices=positions.reshape(-1).tolist(),
                  blend_channels=[(c['name'].data, c['nameHash'].data, c['frameIndex'].data, c['frameCount'].data)
                                  for c in shapes['channels'].items()],
                  blend_frames=[('', f['firstVertex'].data, f['vertexCount'].data)
                                for f in shapes['shapes'].items()],
                  blend_vertices=[(v['index'].data, vector(v['vertex'])) for v in shapes['vertices'].items()],
                  blend_weights=[v.data for v in shapes['fullWeights'].items()])
    bind = mesh['m_BindPose'].items()[root_slot]
    matrix = np.asarray([[bind[f'e{r}{c}'].data for c in range(4)] for r in range(4)])
    return renderer_bounds(author, matrix)


def expand_preloads(file, expansions):
    ids = [pid for pid in file.order if file.objects[pid]['classId'] == 142]
    require(len(ids) == 1, 'one source AssetBundle required')
    tree = file.tree(ids[0])
    old = tree['m_PreloadTable'].items()
    new, offsets, seen = [], [0], set()
    for item in old:
        new.append(item)
        cab, pid = reference(file, item)
        if cab == file.cab and pid in expansions:
            seen.add(pid)
            new.extend(pointer(file, dep) for dep in sorted(expansions[pid]))
        offsets.append(len(new))
    require(seen == set(expansions), 'Renderer absent from original preload table')
    if len(new) == len(old):
        return 0
    def update(info):
        start, count = info['preloadIndex'].data, info['preloadSize'].data
        require(0 <= start <= start + count <= len(old), 'original preload interval')
        info.set('preloadIndex', offsets[start])
        info.set('preloadSize', offsets[start + count] - offsets[start])
    for name in ('m_Container', 'm_HashContainer'):
        for pair in tree[name].items():
            update(pair['second'])
    update(tree['m_MainAsset'])
    tree['m_PreloadTable'].set_items(new)
    file.write_tree(ids[0], tree)
    return len(new) - len(old)


def verify(args):
    package = read_package(args.mod)
    catalog = SourceCatalog(args.baseline)
    manifests = {f['cab']: f for f in catalog.manifest['serializedFiles']}
    schemas = {f['cab']: f for f in json.loads(args.schema.read_text(encoding='utf8'))['files']}
    if args.current_evidence:
        current = json.loads(args.current_evidence.read_text(encoding='utf8'))
        for file in current['files']:
            manifests[file['cab']] = file
            schemas[file['cab']] = file
    mesh_targets = {}
    for rule in package['rules']:
        for target in rule['targets']:
            asset = catalog.resolve('Mesh', target['sourcePath'], target['sourceAsset'])
            mesh_targets[(asset['native']['cab'], asset['native']['pathId'])] = (rule, target, asset)
    parts = {p['partId']: p for p in package['parts']}
    materials = {m['id']: catalog.resolve('Material', m['sourcePath'], m['sourceAsset']) for m in package['materials']}
    native_meshes = {}
    for key, (rule, target, asset) in mesh_targets.items():
        if rule['hidden']:
            continue
        cab, pid = key
        data = (args.baseline / manifests[cab]['snapshot']['file']).read_bytes()
        file = SerializedFile(data, manifests[cab], schemas[cab])
        for part_id in rule['parts']:
            native_meshes[(key, part_id)] = apply_mesh_fields(file.tree(pid), parts[part_id],
                                                            lambda row: read_payload(package, row))
    report = dict(format='EFF_CPP_RENDERER_INPUT_VERIFICATION', version=1, verified=False,
                  nativeSubmission=False, gameWorkerObserved=False,
                  dynamicRegistrationVerified=False, inputs=[])
    for source_path in sorted((args.input_directory or args.run / 'renderer-current-inputs').glob('*.vfs')):
        plan = json.loads(Path(str((args.rebuilt_directory or args.run / 'renderer-rebuilt-inputs') / source_path.name) + '.json').read_text())
        before_dir = (args.source_decoded or args.run / 'independent-renderer-source') / source_path.stem
        after_dir = (args.rebuilt_decoded or args.run / 'independent-renderer-rebuilt') / source_path.stem
        before = {p.stem: p.read_bytes() for p in before_dir.glob('*.bytes')}
        after = {p.stem: p.read_bytes() for p in after_dir.glob('*.bytes')}
        require(before.keys() == after.keys(), 'Renderer member names changed')
        expected_parts = []
        counters = dict(matches=0, skipped=0, changedObjects=0, addedObjects=0, lodGroups=0, preloadReferences=0,
                        preservedObjects=0, bones=0)
        for cab, data in before.items():
            require(cab in manifests and sha(data) == manifests[cab]['snapshot']['sha256'],
                    'current Renderer fixture differs from independently verified baseline')
            file = SerializedFile(data, manifests[cab], schemas[cab])
            trees = {pid: file.tree(pid) for pid in file.order if file.objects[pid]['classId'] in (1, 4, 137)}
            transforms, parents, paths, roots, index = {}, {}, {}, {}, {}
            for tid, tree in trees.items():
                if file.objects[tid]['classId'] != 4:
                    continue
                owner_cab, owner = reference(file, tree['m_GameObject'])
                parent_cab, parent = reference(file, tree['m_Father'])
                require(owner_cab == parent_cab == cab, 'fixture hierarchy must be local')
                transforms[owner], parents[tid] = tid, parent
            def visit(tid, visiting=None):
                if tid in roots:
                    return
                visiting = set() if visiting is None else visiting
                require(tid not in visiting, 'source hierarchy cycle')
                visiting.add(tid)
                parent = parents[tid]
                if parent:
                    visit(parent, visiting)
                    name = trees[reference(file, trees[tid]['m_GameObject'])[1]]['m_Name'].data
                    paths[tid] = (paths[parent] + '/' if paths[parent] else '') + name
                    roots[tid] = roots[parent]
                else:
                    paths[tid], roots[tid] = '', tid
                key = (roots[tid], paths[tid])
                index[key] = tid if key not in index else None
                visiting.remove(tid)
            for tid in parents:
                visit(tid)
            expansions, preloads = {}, {}
            for rid in list(file.order):
                if file.objects[rid]['classId'] != 137:
                    continue
                rt = trees[rid]
                key = reference(file, rt['m_Mesh'])
                if key not in mesh_targets:
                    continue
                rule, target, asset = mesh_targets[key]
                counters['matches'] += 1
                if rule['hidden']:
                    hidden = copy.deepcopy(rt)
                    hidden.set('m_Enabled', False)
                    file.write_tree(rid, hidden)
                    expansions[rid] = []
                    counters['skipped'] += 1
                    continue
                owner = reference(file, rt['m_GameObject'])[1]
                tid = transforms[owner]
                root_bone = reference(file, rt['m_RootBone'])[1]
                dependencies, generated = set(), []
                for i, part_id in enumerate(rule['parts']):
                    part = parts[part_id]
                    owner_ancestors, node = set(), tid
                    while node:
                        owner_ancestors.add(node)
                        node = parents[node]
                    palettes, anchor = [], root_bone
                    while anchor:
                        if anchor in owner_ancestors:
                            prefix = paths[anchor]
                            bone_paths = [(prefix + '/' if prefix else '') + path for path in part['bonePaths']]
                            palette = [index.get((roots[tid], path)) for path in bone_paths]
                            if all(palette) and palette.count(root_bone) == 1:
                                palettes.append(palette)
                        anchor = parents[anchor]
                    require(len(palettes) == 1, 'independent unique character hierarchy anchor')
                    bones = palettes[0]
                    gid, new_tid, new_rid = owner, tid, rid
                    if i:
                        new_rid = allocate(cab, package['modId'], f'renderer:{rid}:{part_id}')
                        gid = allocate(cab, package['modId'], f'go:{rid}:{part_id}')
                        new_tid = allocate(cab, package['modId'], f'transform:{rid}:{part_id}')
                        go = copy.deepcopy(trees[owner])
                        go.set('m_Name', go['m_Name'].data + '__EFF_' + package['modId'] + '_' + part_id)
                        go['m_Component'].set_items([dict(component=pointer(file, (cab, p))) for p in (new_tid, new_rid)])
                        transform = copy.deepcopy(trees[tid])
                        transform.set('m_GameObject', pointer(file, (cab, gid)))
                        transform.set('m_Father', pointer(file, (cab, tid)))
                        transform['m_Children'].set_items([])
                        transform.set('m_LocalPosition', dict(x=0., y=0., z=0.))
                        transform.set('m_LocalRotation', dict(x=0., y=0., z=0., w=1.))
                        transform.set('m_LocalScale', dict(x=1., y=1., z=1.))
                        if 'm_RootOrder' in transform.data:
                            transform.set('m_RootOrder', len(trees[tid]['m_Children'].items()))
                        trees[tid]['m_Children'].set_items(trees[tid]['m_Children'].items() + [pointer(file, (cab, new_tid))])
                        file.write_tree(tid, trees[tid])
                        file.add_tree(gid, owner, go)
                        file.add_tree(new_tid, tid, transform)
                        dependencies.update((cab, p) for p in (gid, new_tid, new_rid))
                    mesh_cab, original_mid = key
                    mid = original_mid if i == 0 else allocate(mesh_cab, package['modId'],
                        'mesh:' + target['sourcePath'] + ':' + target['sourceAsset'] + ':' + part_id)
                    mat = materials[part['material']]
                    mat_ref = (mat['native']['cab'], mat['native']['pathId'])
                    renderer = copy.deepcopy(rt)
                    renderer.set('m_GameObject', pointer(file, (cab, gid)))
                    renderer.set('m_Mesh', pointer(file, (mesh_cab, mid)))
                    renderer['m_Materials'].set_items([pointer(file, mat_ref)])
                    renderer['m_Bones'].set_items([pointer(file, (cab, p)) for p in bones])
                    renderer['m_BlendShapeWeights'].set_items([package['states'][part['shapeBindings'][shape]] * 100.
                        if shape in part['shapeBindings'] else 0. for shape in part['shapeNames']])
                    renderer.set('m_Enabled', bool(part['initialVisible']))
                    renderer.set('m_DirtyAABB', False)
                    renderer.set('m_AABB', author_bounds(native_meshes[(key, part_id)], bones.index(root_bone)))
                    if i:
                        file.add_tree(new_rid, rid, renderer)
                    else:
                        file.write_tree(rid, renderer)
                    generated.append(new_rid)
                    dependencies.update((cab, p) for p in bones)
                    dependencies.update(((mesh_cab, mid), mat_ref))
                    counters['bones'] += len(bones)
                    expected_parts.append(dict(cab=cab, rule=rule['id'], part=part_id, sourceRenderer=rid,
                        renderer=new_rid, owner=gid, transform=new_tid, parent=tid, meshCab=mesh_cab, mesh=mid,
                        materialCab=mat_ref[0], material=mat_ref[1], bones=bones))
                expansions[rid], preloads[rid] = generated, dependencies
            for pid in list(file.order):
                if file.objects[pid]['classId'] != 205:
                    continue
                tree, changed = file.tree(pid), False
                for lod in tree['m_LODs'].items():
                    entries = []
                    for item in lod['renderers'].items():
                        ref_cab, renderer = reference(file, item['renderer'])
                        if ref_cab != cab or renderer not in expansions:
                            entries.append(item)
                        else:
                            for new_rid in expansions[renderer]:
                                entry = copy.deepcopy(item)
                                entry.set('renderer', pointer(file, (cab, new_rid)))
                                entries.append(entry)
                            changed |= expansions[renderer] != [renderer]
                    lod['renderers'].set_items(entries)
                if changed:
                    file.write_tree(pid, tree)
                    counters['lodGroups'] += 1
            counters['preloadReferences'] += expand_preloads(file, preloads)
            verify_serialized(file, after[cab])
            counters['changedObjects'] += len(file.changed - file.added)
            counters['addedObjects'] += len(file.added)
            counters['preservedObjects'] += len(manifests[cab]['objects']) - len(file.changed - file.added)
        sort_key = lambda p: (p['cab'], p['sourceRenderer'], p['part'])
        require(sorted(expected_parts, key=sort_key) == sorted(plan['parts'], key=sort_key), 'Renderer plan differs')
        for key in ('matches', 'skipped', 'changedObjects', 'addedObjects', 'lodGroups', 'preloadReferences'):
            require(counters[key] == plan[key], 'Renderer counter differs: ' + key)
        report['inputs'].append(dict(file=source_path.name, parts=len(expected_parts), **counters))
    for key in ('parts', 'matches', 'skipped', 'changedObjects', 'addedObjects', 'lodGroups', 'preloadReferences',
                'preservedObjects', 'bones'):
        report[key] = sum(r[key] for r in report['inputs'])
    report['verified'] = True
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('mod', 'baseline', 'schema', 'run', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    for name in ('current-evidence', 'input-directory', 'rebuilt-directory', 'source-decoded', 'rebuilt-decoded'):
        parser.add_argument('--' + name, type=Path)
    args = parser.parse_args()
    require(not args.output.exists(), 'Renderer verification report exists')
    report = verify(args)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(json.dumps({k: v for k, v in report.items() if k != 'inputs'}))


if __name__ == '__main__':
    main()
