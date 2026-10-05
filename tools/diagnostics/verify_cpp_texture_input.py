"""Independent Python object/stream comparison after AnimeStudio decoding."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from nativepack.ini_reader import read_package
from nativepack.payload import read_payload
from nativepack.serialized_file import SerializedFile
from nativepack.texture_fields import apply_texture_fields
from nativepack.type_tree import require
from validate_selected_mod_author import SourceCatalog
from verify_cpp_mesh_input import sha, verify_serialized


def verify(args):
    package = read_package(args.mod)
    catalog = SourceCatalog(args.baseline)
    manifests = {f['cab']: f for f in catalog.manifest['serializedFiles']}
    schemas = {f['cab']: f for f in json.loads(args.schema.read_text(encoding='utf8'))['files']}
    texture_by_identity = {}
    for texture in package['textures']:
        asset = catalog.resolve('Texture2D', texture['sourcePath'], texture['sourceAsset'])
        texture_by_identity[asset['identity']] = texture
    materials = {}
    for material in package['materials']:
        asset = catalog.resolve('Material', material['sourcePath'], material['sourceAsset'])
        materials[asset['identity']] = (material, asset)
    report = {'format': 'EFF_CPP_TEXTURE_INPUT_VERIFICATION', 'version': 1,
              'verified': False, 'nativeSubmission': False, 'gameWorkerObserved': False,
              'rendererExpansion': False, 'dependencyClosureVerified': False, 'inputs': []}
    for input_path in sorted((args.run / 'material-texture-current-inputs').glob('*.vfs')):
        cpp_dir = args.run / 'material-texture-rebuilt-inputs'
        plan = json.loads(Path(str(cpp_dir / input_path.name) + '.json').read_text())
        source = args.run / 'independent-material-texture-source' / input_path.stem
        rebuilt = args.run / 'independent-material-texture-rebuilt' / input_path.stem
        before_members = {p.stem: p.read_bytes() for p in source.glob('*.bytes')}
        after_members = {p.stem: p.read_bytes() for p in rebuilt.glob('*.bytes')}
        require(before_members.keys() == after_members.keys(), 'Texture input member identities changed')
        streams = {name: bytearray(data) for name, data in before_members.items() if name.endswith('.resS')}
        texture_results = []
        material_count = binding_count = preserved = 0
        pixel_bytes = 0
        for cab, data in before_members.items():
            if cab.endswith('.resS'):
                continue
            manifest = manifests[cab]
            require(sha(data) == manifest['snapshot']['sha256'], 'current source differs from verified baseline')
            native = SerializedFile(data, manifest, schemas[cab])
            bundle_ids = [p for p in native.order if native.objects[p]['classId'] == 142]
            require(len(bundle_ids) == 1, 'one current AssetBundle required')
            entries = native.tree(bundle_ids[0])['m_Container'].items()
            seen = set()
            for pair in entries:
                path = pair['first'].data
                pointer = pair['second']['asset']
                require(pointer['m_FileID'].data == 0, 'verification fixture expects local named assets')
                pid = pointer['m_PathID'].data
                identity = cab.lower() + ':' + str(pid)
                if identity not in texture_by_identity and identity not in materials:
                    continue
                require(pid not in seen, 'ambiguous selected object in source input')
                seen.add(pid)
                tree = native.tree(pid)
                if identity in materials:
                    material, asset = materials[identity]
                    require(path == material['sourcePath'] and tree['m_Name'].data == material['sourceAsset'],
                            'Material source identity differs')
                    material_count += 1
                    require(not material['nativeFields'], 'current verification expects pointer-only material declarations')
                    properties = {row['property']: row['pointer']['identity'] for row in asset['details']['textures']}
                    for edit in material['textureEdits']:
                        selected = next(t for t in package['textures'] if t['id'] == edit['texture'])
                        target = catalog.resolve('Texture2D', selected['sourcePath'], selected['sourceAsset'])
                        require(properties.get(edit['property']) == target['identity'], 'Material pointer target differs')
                        binding_count += 1
                    continue
                texture = texture_by_identity[identity]
                require(path == texture['sourcePath'] and tree['m_Name'].data == texture['sourceAsset'],
                        'Texture source identity differs')
                pixels = read_payload(package, texture)
                changed = apply_texture_fields(tree, texture, len(pixels))
                stream = changed['m_StreamData']['path'].data.rsplit('/', 1)[-1]
                require(stream == cab + '.resS' and stream in streams, 'Texture member identity')
                offset = (len(streams[stream]) + 15) & ~15
                streams[stream].extend(bytes(offset - len(streams[stream])))
                streams[stream].extend(pixels)
                changed['m_StreamData'].set('offset', offset)
                changed['m_StreamData'].set('size', len(pixels))
                native.write_tree(pid, changed)
                pixel_bytes += len(pixels)
                texture_results.append({'path': path, 'asset': texture['sourceAsset'], 'cab': cab,
                                        'stream': stream, 'id': pid, 'offset': offset, 'bytes': len(pixels)})
            verify_serialized(native, after_members[cab])
            preserved += len(native.order) - len(native.changed)
        for member, expected in streams.items():
            require(bytes(expected) == after_members[member], 'independent raw stream bytes differ: ' + member)
        key = lambda r: (r['cab'], r['id'])
        require(sorted(texture_results, key=key) == sorted(plan['textures'], key=key), 'Texture stream plan differs')
        require((material_count, binding_count, 0) ==
                (plan['materialMatches'], plan['textureBindings'], plan['changedMaterials']),
                'Material plan counters differ')
        if not texture_results:
            require(input_path.read_bytes() == (cpp_dir / input_path.name).read_bytes(),
                    'unchanged Material input bytes changed')
        report['inputs'].append({'file': input_path.name, 'materials': material_count,
                                  'textures': len(texture_results), 'textureBindings': binding_count,
                                  'pixelBytes': pixel_bytes, 'preservedObjects': preserved,
                                  'preservedStreamBytes': sum(len(v) for k, v in before_members.items()
                                                              if k.endswith('.resS'))})
    for key in ('materials', 'textures', 'textureBindings', 'pixelBytes', 'preservedObjects', 'preservedStreamBytes'):
        report[key] = sum(row[key] for row in report['inputs'])
    require(report['materials'] == 5 and report['textures'] == 11 and report['textureBindings'] == 14,
            'current LZY coverage differs')
    report['verified'] = True
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
