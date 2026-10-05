"""Rename legacy TEX paths using image names read from the current Blender scene."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from nativepack.ini_package import ini_document, update_directory
from nativepack.ini_reader import read_package
from nativepack.resource_file import decode_resource, PIXELS


def hashes(root):
    return {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in root.rglob('*') if p.is_file()}


def prepare(production, evidence):
    names = json.loads((evidence/'source-images.json').read_text('utf-8'))
    destination = evidence/'prepared/LZY'
    shutil.copytree(production, destination)
    before = read_package(destination)
    original_hashes = hashes(destination)
    ini = ini_document(destination)
    controls = {s: dict(ini[s]) for s in ini.sections() if not s.startswith('Texture')}
    assert set(names) == {t['id'] for t in before['textures']}
    textures = [dict(t, sourceAsset=t['id'].removeprefix('Texture'), pixelLabel=names[t['id']]['name'],
                     pixels=decode_resource((destination/t['file']).read_bytes(), PIXELS)['blocks']['pixels'])
                for t in before['textures']]
    after = update_directory(destination, textures=textures, texture_only=True)
    for old, new in zip(before['textures'], after['textures']):
        assert new['file'] == 'textures/' + names[new['id']]['name'] + '.tex'
        assert original_hashes[old['file']] == hashlib.sha256((destination/new['file']).read_bytes()).hexdigest()
    updated = ini_document(destination)
    assert controls == {s: dict(updated[s]) for s in updated.sections() if not s.startswith('Texture')}
    current_hashes = hashes(destination)
    for relative, digest in original_hashes.items():
        if relative != 'mod.ini' and not relative.endswith('.tex'):
            assert current_hashes[relative] == digest
    report = dict(success=True, originalHashes=original_hashes,
                  paths=[dict(section=new['id'], before=old['file'], after=new['file'])
                         for old, new in zip(before['textures'], after['textures'])],
                  texFiles=sorted({t['file'] for t in after['textures']}),
                  textureBytesUnchanged=True, otherResourcesUnchanged=True)
    (evidence/'rename-verification.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k != 'originalHashes'},indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('production', type=Path)
    parser.add_argument('evidence', type=Path)
    args = parser.parse_args()
    prepare(args.production, args.evidence)
