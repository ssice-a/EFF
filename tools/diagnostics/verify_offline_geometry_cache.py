"""Real-author differential fixtures; never edit installed Mod or Blender scene."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from nativepack.resource_file import decode_resource, encode_resource, MESH, MATERIAL, PIXELS
from nativepack.ini_reader import read_package


def current(cache):
    return cache / 'generations' / (cache / 'current.tsv').read_text('utf8').splitlines()[1].split('\t')[0]


def compare(a, b):
    files = sorted(a.glob('input-*.vfs')) + sorted((a / 'startup').glob('input-*.vfs'))
    assert len(files) == 136
    for file in files:
        other = b / file.relative_to(a)
        assert file.read_bytes() == other.read_bytes(), str(file)
    return len(files)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('game', type=Path)
    p.add_argument('evidence', type=Path)
    p.add_argument('compiler', type=Path)
    args = p.parse_args()
    evidence = args.evidence.resolve()
    warm = evidence / 'cache'
    cold = evidence / 'cache-cold'
    compared = compare(current(warm), current(cold))
    cache = evidence / 'cache-edits'
    assert not cache.exists()
    shutil.copytree(warm / 'geometry', cache / 'geometry')
    source = args.game / 'plugin/mods/LZY'
    author = evidence / 'author-edits/LZY'
    assert not author.exists()
    shutil.copytree(source, author)
    original = {f.relative_to(author): f.read_bytes() for f in author.rglob('*') if f.is_file()}
    results = []
    compiler = args.compiler.resolve()

    def run(label, geometry_reused):
        read_package(author)
        result = subprocess.run([str(compiler), str(author), str(args.game / 'plugin/resource-reload-baseline'),
                                 str(cache)], capture_output=True, text=True)
        (evidence / ('compile-' + label + '.log')).write_text(result.stdout + result.stderr, 'utf8')
        assert result.returncode == 0, result.stderr
        assert ('phase=geometry-reused' in result.stdout) == geometry_reused, result.stdout
        folder = current(cache)
        assert folder.name != current(warm).name
        changed = [f.name for f in folder.joinpath('startup').glob('*.vfs')
                   if f.read_bytes() != (current(warm) / 'startup' / f.name).read_bytes()]
        assert bool(changed) == (label != 'controls'), (label, changed)
        results.append(dict(label=label, key=folder.name, geometryReused=geometry_reused, changedInputs=changed))
        print('PASS', label, 'geometryReused=' + str(geometry_reused), 'changedInputs=' + str(len(changed)), flush=True)
        for path, data in original.items():
            (author / path).write_bytes(data)

    pixel = author / 'textures/body_010de4985b31.tex'
    decoded = decode_resource(pixel.read_bytes(), PIXELS)
    blocks = dict(decoded['blocks'])
    data = bytearray(blocks['pixels']); data[0] ^= 0xFF; blocks['pixels'] = data
    pixel.write_bytes(encode_resource(PIXELS, decoded['nativeTypeHash'], blocks))
    run('pixels', True)

    material = author / 'materials/MaterialM_actor_lizhiyan_cloth_01_1.mat'
    decoded = decode_resource(material.read_bytes(), MATERIAL)
    blocks = dict(decoded['blocks']); blocks['m_CustomRenderQueue'] = struct.pack('<i', 2001)
    material.write_bytes(encode_resource(MATERIAL, decoded['nativeTypeHash'], blocks))
    run('material', True)

    ini = author / 'mod.ini'
    before = ini.read_text('utf-8-sig')
    assert 'key=NUMPAD5' in before
    ini.write_text(before.replace('key=NUMPAD5', 'key=NUMPAD6', 1), 'utf8')
    run('controls', True)

    mesh = author / 'meshes/MeshS_actor_lizhiyan_body_01_lod0_0.mesh'
    decoded = decode_resource(mesh.read_bytes(), MESH)
    blocks = dict(decoded['blocks'])
    indices = struct.unpack('<II', blocks['@geometry'])[1]
    data = bytearray(blocks['m_IndexBuffer'])
    count = struct.unpack_from('<i', data)[0]
    assert count == len(data) - 4
    width = count // indices
    assert width in (2, 4)
    # Reverse one valid triangle, retaining counts, vertex values and all bones.
    data[4:4 + 2 * width] = data[4 + width:4 + 2 * width] + data[4:4 + width]
    blocks['m_IndexBuffer'] = data
    mesh.write_bytes(encode_resource(MESH, decoded['nativeTypeHash'], blocks))
    run('mesh', False)

    previous = (cache / 'current.tsv').read_bytes()
    bad = bytearray(pixel.read_bytes()); bad[-1] ^= 1; pixel.write_bytes(bad)
    result = subprocess.run([str(compiler), str(author), str(args.game / 'plugin/resource-reload-baseline'),
                             str(cache)], capture_output=True, text=True)
    assert result.returncode != 0 and (cache / 'current.tsv').read_bytes() == previous
    (evidence / 'compile-reject.log').write_text(result.stdout + result.stderr, 'utf8')
    for path, data in original.items():
        (author / path).write_bytes(data)
    report = dict(format='EFF_GEOMETRY_CACHE_DIFFERENTIAL', coldWarmBytesEqual=True, comparedFiles=compared,
                  changes=results, invalidAuthorRetainsPreviousCurrent=True, installedAuthorEdited=False, gameAcceptance=False)
    (evidence / 'geometry-cache-verification.json').write_text(json.dumps(report, indent=2) + '\n', 'utf8')
    print('PASS cold/warm 136 identical files; pixels/material/controls/mesh invalidation and failed-publication preservation')


if __name__ == '__main__':
    main()
