"""Check original/custom texture separation against actual rebuilt input bytes.

The C++ utility only unwraps the VFS containers. Python independently reads
object tables, native fields, texture pointers, pixels and preload intervals.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from nativepack.author_source import AuthorCore, AuthorSource
from nativepack.ini_reader import read_package
from nativepack.payload import read_payload
from nativepack.texture_fields import apply_texture_fields
from nativepack.type_tree import Schema, decode, encode, require
from verify_object_order_probe import object_rows


def clone_id(cab, package, texture):
    label = ':'.join((cab, package['modId'], 'texture', texture['sourcePath'],
                      texture['sourceAsset'], texture['id']))
    return struct.unpack('<q', hashlib.sha256(label.encode()).digest()[:8])[0]


def native(data, core):
    first, last, rows = object_rows(data)
    offset = struct.unpack_from('>Q', data, 32)[0]
    types = core.schema(data)['types']
    schemas = {t['index']: Schema.compile(t['nodes']) for t in types if t['nodes']}
    objects = {pid: (types[kind]['classId'], kind, data[offset+start:offset+start+size])
               for pid, start, size, kind in rows}
    pos = last
    scripts = struct.unpack_from('<I', data, pos)[0]; pos += 4 + scripts*12
    count = struct.unpack_from('<I', data, pos)[0]; pos += 4
    externals = []
    for _ in range(count):
        pos = data.index(b'\0', pos) + 1 + 20
        end = data.index(b'\0', pos)
        externals.append(data[pos:end].decode().replace('\\', '/').rsplit('/', 1)[-1])
        pos = end + 1
    return objects, schemas, externals


def reference(pointer, cab, externals):
    file, path = pointer['m_FileID'].data, pointer['m_PathID'].data
    require(0 <= file <= len(externals), 'native texture FileID range')
    return (externals[file-1] if file else cab, path)


def inspect(args):
    package = read_package(args.mod)
    core = AuthorCore(args.core)
    source = AuthorSource(args.baseline, core)
    snapshots = json.loads((args.snapshots/'snapshots.json').read_text())['inputs']
    resources = {r['identity']: r for r in source.manifest['resources']}
    selected_materials, expected_textures, expansion = {}, {}, {}
    for m in package['materials']:
        r = source.resolve('Material', m['sourcePath'], m['sourceAsset'])
        selected_materials[(r['native']['cab'], r['native']['pathId'])] = m
    for t in package['textures']:
        r = source.resolve('Texture2D', t['sourcePath'], t['sourceAsset'])
        ref = (r['native']['cab'], r['native']['pathId'])
        cid = clone_id(ref[0], package, t)
        expected_textures[(ref[0], cid)] = (ref, t)
        expansion.setdefault(ref, []).append((ref[0], cid))
    pixels = {t['id']: read_payload(package, t) for t in package['textures']}
    by_texture = {t['id']: t for t in package['textures']}
    result = dict(format='EFF_TEXTURE_ISOLATION_VERIFICATION', version=1,
                  verified=False, runtimeVisualAcceptance=False, originalTexturesPreserved=0,
                  originalMaterialsPreserved=0, customTextures=0, bindings=0,
                  preloadIntervals=0, clonePreloadReferences=0, materials=[], textures=[])
    actual_clones, preload_clones = set(), set()
    for number, row in enumerate(snapshots, 1):
        original = args.snapshots/row['file']
        rebuilt = args.rebuilt/original.name
        if not rebuilt.is_file() and args.allow_unchanged_sources:
            rebuilt = original
        require(rebuilt.is_file(), 'rebuilt input absent: '+original.name)
        directories = []
        for label, path in (('source', original), ('rebuilt', rebuilt)):
            directory = args.work/label/original.stem
            if not directory.exists():
                subprocess.run([str(args.checker.resolve()), '--endfield-members',
                    str(path.resolve()), str(directory.resolve())], check=True, capture_output=True)
            directories.append(directory)
        before = {p.name.removesuffix('.bytes'): p for p in directories[0].glob('*.bytes')}
        after = {p.name.removesuffix('.bytes'): p for p in directories[1].glob('*.bytes')}
        require(before.keys() == after.keys(), 'native member identity differs')
        for cab, path in before.items():
            old, new = path.read_bytes(), after[cab].read_bytes()
            if old[8:12] != b'\x00\x00\x00\x16':
                require(new.startswith(old), 'original stream prefix modified: '+cab)
                continue
            objects, schemas, externals = native(old, core)
            current, current_schemas, current_externals = native(new, core)
            require(externals == current_externals, 'texture external registration changed')
            for pid, (kind, type_id, payload) in objects.items():
                if kind == 28:
                    require(current[pid][2] == payload, 'original texture was globally overwritten: '+cab+':'+str(pid))
                    result['originalTexturesPreserved'] += 1
                if kind == 21:
                    material = selected_materials.get((cab, pid))
                    original_tree = decode(schemas[type_id], payload)
                    actual = decode(current_schemas[current[pid][1]], current[pid][2])
                    if material is None or not material['textureEdits']:
                        require(current[pid][2] == payload, 'unedited material changed')
                        result['originalMaterialsPreserved'] += 1
                    if material is None: continue
                    expected = copy.deepcopy(original_tree)
                    for field in material['nativeFields']:
                        block = package['_resources'][field['file']]['blocks'][field['name']]
                        expected.data[field['name']] = decode(expected[field['name']].schema, block)
                    slots = {p['first'].data: p['second'] for p in expected['m_SavedProperties']['m_TexEnvs'].items()}
                    for edit in material['textureEdits']:
                        t = by_texture[edit['texture']]
                        r = source.resolve('Texture2D', t['sourcePath'], t['sourceAsset'])
                        pointer = slots[edit['property']]['m_Texture']
                        require(reference(pointer,cab,externals) == (r['native']['cab'],r['native']['pathId']), 'wrong native texture template')
                        pointer.set('m_PathID', clone_id(r['native']['cab'], package, t))
                        result['bindings'] += 1
                    require(encode(expected) == encode(actual), 'material native fields/texture bindings differ: '+material['id'])
                    result['materials'].append(dict(id=material['id'], edits=material['textureEdits'],
                        inheritsOriginal=not material['textureEdits']))
                if kind == 142:
                    old_tree = decode(schemas[type_id], payload)
                    tree = decode(current_schemas[current[pid][1]], current[pid][2])
                    old_preloads = [reference(p,cab,externals) for p in old_tree['m_PreloadTable'].items()]
                    preloads = [reference(p,cab,externals) for p in tree['m_PreloadTable'].items()]
                    preload_clones.update(p for p in preloads if p in expected_textures)
                    result['clonePreloadReferences'] += sum(p in expected_textures for p in preloads)
                    infos = [(r['second'], n, i) for n in ('m_Container','m_HashContainer')
                             for i,r in enumerate(tree[n].items())] + [(tree['m_MainAsset'], 'm_MainAsset', 0)]
                    for info, name, index in infos:
                        first, count = info['preloadIndex'].data, info['preloadSize'].data
                        require(0 <= first <= len(preloads) and 0 <= count <= len(preloads)-first, 'invalid texture preload interval')
                        old_info = old_tree[name] if name == 'm_MainAsset' else old_tree[name].items()[index]['second']
                        a,b = old_info['preloadIndex'].data, old_info['preloadSize'].data
                        require(0 <= a <= len(old_preloads) and 0 <= b <= len(old_preloads)-a, 'invalid source preload interval')
                        interval = set(preloads[first:first+count])
                        required = {clone for ref in old_preloads[a:a+b] for clone in expansion.get(ref,())}
                        require(required <= interval, 'custom texture missing from owning preload interval')
                        result['preloadIntervals'] += 1
            for ref, (original_ref, texture) in expected_textures.items():
                if ref[0] != cab: continue
                require(ref[1] in current and ref[1] not in objects, 'custom texture clone absent/colliding')
                actual_clones.add(ref)
                _, type_id, payload = objects[original_ref[1]]
                original_tree = decode(schemas[type_id], payload)
                actual = decode(current_schemas[current[ref[1]][1]],current[ref[1]][2])
                expected = apply_texture_fields(original_tree,texture,len(pixels[texture['id']]))
                expected.set('m_Name',texture['sourceAsset']+'__EFF_'+package['modId']+'_'+texture['id'])
                stream = actual['m_StreamData']
                name = stream['path'].data.replace('\\','/').rsplit('/',1)[-1]
                offset,size = stream['offset'].data,stream['size'].data
                data = after[name].read_bytes()
                require(offset >= before[name].stat().st_size and offset%16==0 and size==len(pixels[texture['id']]), 'custom texture stream range')
                require(data[offset:offset+size] == pixels[texture['id']], 'custom texture pixel/mip mismatch')
                expected['m_StreamData'].set('offset',offset); expected['m_StreamData'].set('size',size)
                require(encode(expected) == encode(actual),'custom texture native fields differ')
                result['customTextures'] += 1
                result['textures'].append(dict(id=texture['id'],cab=cab,sourceId=original_ref[1],cloneId=ref[1],
                    width=texture['width'],height=texture['height'],linear=texture['colorSpace']==0,
                    mipCount=texture['mipCount'],pixelSha256=hashlib.sha256(pixels[texture['id']]).hexdigest().upper()))
        if number%10==0: print('verified inputs',number,flush=True)
    require(actual_clones == set(expected_textures) and actual_clones <= preload_clones, 'texture graph clone/reference coverage')
    require(len(result['materials']) == len(package['materials']), 'material coverage')
    result['inputs'] = len(snapshots); result['verified'] = True
    return result


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('mod','baseline','core','checker','snapshots','rebuilt','work','output'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--allow-unchanged-sources',action='store_true')
    args=parser.parse_args()
    try:
        report=inspect(args)
    except Exception as error:
        args.output.write_text(json.dumps(dict(verified=False,error=str(error)),indent=2)+'\n','utf-8')
        raise
    args.output.write_text(json.dumps(report,indent=2)+'\n','utf-8')
    print(json.dumps({k:v for k,v in report.items() if k not in ('materials','textures')}))
