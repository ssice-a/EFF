"""Verify source-free TEX compilation against the actual game baseline in copies."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time

WORKSPACE = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(WORKSPACE / 'tools'))
sys.path.insert(0, str(WORKSPACE / 'tools/Blender/vendor'))
from nativepack.ini_package import ini_document, save_ini, texture_sections
from nativepack.ini_reader import read_package
from nativepack.resource_file import decode_resource, encode_resource, PIXELS
from nativepack.texture_fields import texture_blocks
from PIL import Image


def hashes(root):
    return {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in root.rglob('*') if p.is_file()}


def migrate(root):
    before = read_package(root)
    ini = ini_document(root)
    untouched = {s: dict(ini[s]) for s in ini.sections() if not s.startswith('Texture')}
    textures = []
    for texture in before['textures']:
        pixels = decode_resource((root/texture['file']).read_bytes(), PIXELS)['blocks']['pixels']
        textures.append(dict(texture, sourceAsset=texture['id'].removeprefix('Texture'),
                             pixelLabel=Path(texture['file']).stem, pixels=pixels))
    texture_sections(root, ini, textures)
    save_ini(root, ini)
    after = read_package(root)
    for old, new in zip(before['textures'], after['textures']):
        assert not new['sourcePath'] and not new['sourceAsset']
        assert old['file'] == new['file'] and old['sha256'] == new['sha256']
        for field in ('width', 'height', 'mipCount', 'colorSpace', 'filter', 'wrap', 'aniso', 'mipBias'):
            assert old[field] == new[field], (new['id'], field)
    assert before['materials'] == after['materials']
    assert untouched == {s: dict(ini[s]) for s in ini.sections() if not s.startswith('Texture')}
    return after


def invoke(command, output):
    start = time.perf_counter()
    result = subprocess.run([str(p) for p in command], capture_output=True,
                            encoding='utf-8', errors='replace', check=False)
    output.write_text(result.stdout + result.stderr, encoding='utf-8')
    if result.returncode:
        raise RuntimeError(f'{command[0].name} failed: {(result.stderr or result.stdout)[-1600:]}')
    return time.perf_counter() - start, result.stdout.strip()


def run(game, evidence):
    evidence.mkdir(parents=True, exist_ok=True)
    production = game/'plugin/mods/LZY'
    baseline = game/'plugin/resource-reload-baseline'
    compiler = WORKSPACE/'bin/resource-pack/eff_resource_pack.exe'
    inspector = WORKSPACE/'bin/resource-pack/inspect_payloads.exe'
    cache = evidence/'cache'
    original = hashes(production)
    root = evidence/'migrated/LZY'
    if not root.exists():
        shutil.copytree(production, root)
    package = migrate(root)
    migrated = hashes(root)
    assert all(migrated[p] == h for p, h in original.items() if p != 'mod.ini' and not p.endswith('.tex'))
    report = {'format': 'EFF_SOURCE_FREE_TEXTURE_VERIFICATION', 'tests': []}

    def compile_and_inspect(author, label):
        elapsed, _ = invoke([compiler, author, baseline, cache], evidence/(label+'-compile.log'))
        key, digest = (cache/'current.tsv').read_text().splitlines()[1].split('\t')
        generation = cache/'generations'/key
        _, checked = invoke([inspector, generation, author.parent, baseline], evidence/(label+'-native.txt'))
        report['tests'].append(dict(name=label, key=key, manifestSHA256=digest,
                                   compileSeconds=elapsed, native=checked))
        return key

    original_key = compile_and_inspect(root, 'source-free-original-size')
    resized = evidence/'resized/LZY'
    if not resized.exists():
        shutil.copytree(root, resized)
    texture = next(t for t in package['textures'] if t['id'] == 'TextureT_actor_lizhiyan_cloth_02_D')
    assert texture['width'] == 2048 and texture['height'] == 2048
    resource = decode_resource((resized/texture['file']).read_bytes(), PIXELS)
    mip = Image.frombytes('RGBA', (texture['width'], texture['height']),
                          resource['blocks']['pixels'][:texture['width']*texture['height']*4])
    mip = mip.resize((1024, 1024), Image.Resampling.BOX)
    pixels = bytearray()
    for _ in range(11):
        pixels.extend(mip.tobytes())
        mip = mip.resize((max(1, mip.width//2), max(1, mip.height//2)), Image.Resampling.BOX)
    changed = dict(texture, width=1024, height=1024, mipCount=11, pixels=bytes(pixels))
    (resized/texture['file']).write_bytes(encode_resource(PIXELS, '00'*16, texture_blocks(changed)))
    assert (resized/'mod.ini').read_bytes() == (root/'mod.ini').read_bytes()
    resized_package = read_package(resized)
    matched = [t for t in resized_package['textures'] if t['file'] == texture['file']]
    assert all((t['width'], t['height'], t['mipCount']) == (1024, 1024, 11) for t in matched)
    resized_key = compile_and_inspect(resized, 'same-ini-2k-to-1k')
    assert original_key != resized_key
    report['productionUnchanged'] = hashes(production) == original
    assert report['productionUnchanged']
    report['textures'] = len(package['textures'])
    report['success'] = True
    (evidence/'verification.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('game', type=Path)
    parser.add_argument('evidence', type=Path)
    args = parser.parse_args()
    run(args.game, args.evidence)
