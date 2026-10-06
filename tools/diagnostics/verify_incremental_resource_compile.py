"""Differential CLI regression with real Mesh/material/pixel author inputs.

All edits stay in an isolated author. Both compilers use the same input bytes;
their 68 startup and 68 namespaced outputs must be identical for every edit.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from nativepack.resource_file import decode_resource, encode_resource, MESH, MATERIAL, PIXELS


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def current(cache):
    return cache / 'generations' / (cache / 'current.tsv').read_text('utf8').splitlines()[1].split('\t')[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('compiler', 'reference', 'author', 'baseline', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--seed-geometry', type=Path)
    args = parser.parse_args()
    root = args.output.resolve()
    assert shutil.disk_usage(root.parent).free >= 16 * 1024**3, 'real-input regression needs 16 GiB of free temporary space'
    root.mkdir(parents=True, exist_ok=False)
    source = args.author.resolve(); author = root / source.name; shutil.copytree(source, author)
    protected = {p.relative_to(source).as_posix(): digest(p) for p in source.rglob('*') if p.is_file()}
    original = {p.relative_to(author): p.read_bytes() for p in author.rglob('*') if p.is_file()}
    fast, reference = root / 'cache', root / 'reference-cache'
    if args.seed_geometry:
        shutil.copytree(args.seed_geometry, reference / 'geometry')
    results = []

    def compile(exe, cache, label, expect_success=True):
        start = time.perf_counter()
        run = subprocess.run([str(exe.resolve()), str(author), str(args.baseline.resolve()),
                              str(cache)], capture_output=True,
                             text=True, encoding='utf8', errors='replace', timeout=180,
                             creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        elapsed = time.perf_counter() - start
        (root / (label + '.log')).write_text(run.stdout + run.stderr, 'utf8')
        assert (run.returncode == 0) == expect_success, run.stdout + run.stderr
        return elapsed, run.stdout

    def compare(label):
        fast_seconds, output = compile(args.compiler, fast, label + '-fast')
        ref_seconds, _ = compile(args.reference, reference, label + '-reference')
        a, b = current(fast), current(reference)
        assert a.name == b.name
        files = sorted(a.glob('input-*.vfs')) + sorted((a / 'startup').glob('input-*.vfs'))
        assert len(files) == 136
        for file in files:
            assert digest(file) == digest(b / file.relative_to(a)), str(file)
        assert (a / 'manifest.bin').read_bytes() == (b / 'manifest.bin').read_bytes(), 'compiled manifests differ'
        stats = dict((name, int(value)) for name, value in re.findall(r'(\w+(?:Hits|Misses))=(\d+)', output))
        results.append(dict(label=label, fastSeconds=fast_seconds, referenceSeconds=ref_seconds,
                            key=a.name, filesIdentical=136, caches=stats))
        print('PASS', label, 'fast=%.3fs' % fast_seconds, 'reference=%.3fs' % ref_seconds, stats, flush=True)
        for path, data in original.items():
            (author / path).write_bytes(data)

    def edit_resource(path, kind, edit):
        data = decode_resource(path.read_bytes(), kind); blocks = dict(data['blocks']); edit(blocks)
        path.write_bytes(encode_resource(kind, data['nativeTypeHash'], blocks))

    compare('baseline-cold')
    # Keep the recipes actually used for the original author. After the edits,
    # glob order can select a stage belonging only to a different Mesh/pixel
    # generation; corrupting that unused entry does not exercise rejection.
    baseline_appearances = sorted((fast / 'appearance').glob('*.vfs'))
    baseline_recipes = sorted((fast / 'namespace').glob('*.tsv'))
    tex = author / 'textures/body.tex'
    def pixels(blocks):
        changed = bytearray(blocks['pixels']); changed[0] ^= 0xFF; blocks['pixels'] = changed
    edit_resource(tex, PIXELS, pixels); compare('pixels')
    mat = author / 'materials/MaterialM_actor_lizhiyan_cloth_01_1.mat'
    edit_resource(mat, MATERIAL, lambda blocks: blocks.update(m_CustomRenderQueue=struct.pack('<i', 2001)))
    compare('material')
    ini = author / 'mod.ini'; raw = ini.read_bytes(); changed = raw.replace(b'label=VaginaOpen', b'label=CompilePerfWitness', 1)
    assert changed != raw; ini.write_bytes(changed); compare('controls')
    mesh = author / 'meshes/MeshS_actor_lizhiyan_body_01_lod0_0.mesh'
    def indices(blocks):
        data = bytearray(blocks['m_IndexBuffer']); count = struct.unpack_from('<i', data)[0]
        total = struct.unpack('<II', blocks['@geometry'])[1]; width = count // total
        assert width in (2, 4) and count == len(data) - 4
        data[4:4 + 2 * width] = data[4 + width:4 + 2 * width] + data[4:4 + width]; blocks['m_IndexBuffer'] = data
    edit_resource(mesh, MESH, indices); compare('mesh-indices')
    def vertex(blocks):
        data = bytearray(blocks['m_VertexData']); count = struct.unpack_from('<I', data)[0]
        channels = struct.unpack_from('<i', data, 4)[0]; assert channels == 14
        pixel_count_offset = 8 + 4 * channels
        size = struct.unpack_from('<i', data, pixel_count_offset)[0]
        start = pixel_count_offset + 4; assert size <= len(data) - start and count > 0
        value = struct.unpack_from('<f', data, start)[0]; struct.pack_into('<f', data, start, value + 0.001)
        blocks['m_VertexData'] = data
    edit_resource(mesh, MESH, vertex); compare('mesh-vertex')
    # All resources changed together must also observe their complete dependency graph.
    edit_resource(mesh, MESH, indices); edit_resource(tex, PIXELS, pixels)
    edit_resource(mat, MATERIAL, lambda blocks: blocks.update(m_CustomRenderQueue=struct.pack('<i', 2002)))
    compare('combined')
    before = (fast / 'current.tsv').read_bytes(); bad = bytearray(tex.read_bytes()); bad[-1] ^= 1; tex.write_bytes(bad)
    compile(args.compiler, fast, 'corrupt-author', False); assert (fast / 'current.tsv').read_bytes() == before
    for path, data in original.items(): (author / path).write_bytes(data)
    # Corrupted committed cache payloads/descriptors must never be accepted.
    (author / 'mod.ini').write_bytes(raw.replace(b'label=VaginaOpen', b'label=CacheCorruptionWitness', 1))
    cache_payload = baseline_appearances[0]; payload = cache_payload.read_bytes()
    broken = bytearray(payload); broken[-1] ^= 1; cache_payload.write_bytes(broken)
    compile(args.compiler, fast, 'corrupt-cache', False); assert (fast / 'current.tsv').read_bytes() == before
    cache_payload.write_bytes(payload)
    recipe = baseline_recipes[0]; descriptor = recipe.read_bytes()
    broken = bytearray(descriptor); broken[20] ^= 1; recipe.write_bytes(broken)
    compile(args.compiler, fast, 'corrupt-namespace-recipe', False); assert (fast / 'current.tsv').read_bytes() == before
    recipe.write_bytes(descriptor)
    assert protected == {p.relative_to(source).as_posix(): digest(p) for p in source.rglob('*') if p.is_file()}
    report = dict(passed=True, cases=results, originalAuthorUnchanged=True, corruptedAuthorRetainsCurrent=True,
                  corruptedCacheRetainsCurrent=True, corruptedNamespaceRecipeRetainsCurrent=True,
                  compilerSHA256=digest(args.compiler), referenceSHA256=digest(args.reference))
    (root / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', 'utf8')
    print('PASS differential resource compile; author/cache/namespace corruption retained current', flush=True)


if __name__ == '__main__':
    main()
