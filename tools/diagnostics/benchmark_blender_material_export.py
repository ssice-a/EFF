"""Measure a real parameter-only export in a disposable copy of a Mod.

Run with Blender --background <saved.blend> --python-exit-code 1 --python
this_file -- <addon-directory> <mod-directory> <game-directory> [--profile].
The saved project and installed Mod are never written.
"""
import argparse
import cProfile
import difflib
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import pstats
import shutil
import subprocess
import sys
import tempfile

import bpy


def hashes(root):
    return {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in root.rglob('*') if p.is_file()}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('addon', type=Path)
    parser.add_argument('mod', type=Path)
    parser.add_argument('game', type=Path)
    parser.add_argument('--profile', action='store_true')
    parser.add_argument('--report', type=Path)
    parser.add_argument('--compare-report', type=Path)
    parser.add_argument('--compiler', type=Path)
    parser.add_argument('--verifier', type=Path)
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    name = 'EFFMaterialBenchmark'
    spec = importlib.util.spec_from_file_location(name, args.addon / '__init__.py',
                                                  submodule_search_locations=[str(args.addon)])
    package = importlib.util.module_from_spec(spec)
    sys.modules[name] = package
    spec.loader.exec_module(package)
    package.register()
    addon = package.eiem_blender_addon
    exporter = addon.native_export
    if args.compiler:
        from EFFMaterialBenchmark import eiem_offline_reload as offline
        original_context = offline.reload_context
        def context(destination):
            current = original_context(destination)
            return (args.compiler.resolve(), *current[1:])
        offline.reload_context = context
    modules, _ = exporter.pack_modules()
    ini = modules['ini_package'].ini_document(args.mod)
    candidates = []
    for obj in bpy.data.objects:
        if obj.type != 'MESH' or not obj.data.get('eiem_source'):
            continue
        for mat in obj.data.materials:
            if mat and exporter.material_id(mat) in ini:
                keys = sorted(k for k in mat.keys() if k.startswith('eiem_float.')
                              and 'emissionbrightness' not in k.lower())
                if keys:
                    candidates.append((obj.name, mat.name, keys[0], obj, mat))
    if not candidates:
        raise RuntimeError('Saved scene has no material matching the published Mod')
    _, _, key, obj, mat = sorted(candidates, key=lambda row: row[:3])[0]
    original = mat[key]
    mat[key] = format(float(original) + 0.125, '.9g')
    writes = []
    original_write = addon.write_texture
    def write_texture(path, image):
        writes.append(image.name)
        return original_write(path, image)
    addon.write_texture = write_texture
    os.environ['EFF_RELOAD_GAME'] = str(args.game.resolve())
    source_before = hashes(args.mod)
    try:
        with tempfile.TemporaryDirectory(prefix='eff-material-benchmark-') as temporary:
            destination = Path(temporary) / args.mod.name
            shutil.copytree(args.mod, destination)
            profiler = cProfile.Profile() if args.profile else None
            if profiler:
                profiler.enable()
            result = exporter.export_native_package(addon, destination, [obj],
                         source_baseline='', resource_scope='MATERIALS')
            if profiler:
                profiler.disable()
            report = dict(result, object=obj.name, material=mat.name, parameter=key,
                          original=str(original), edited=str(mat[key]), textureConversions=writes,
                          outputHashes=hashes(destination), productionUnchanged=hashes(args.mod) == source_before)
            report['iniDiff']=list(difflib.unified_diff((args.mod/'mod.ini').read_text('utf-8-sig').splitlines(),
                         (destination/'mod.ini').read_text('utf-8-sig').splitlines()))
            if profiler:
                stats = pstats.Stats(profiler)
                report['profile'] = [dict(file=k[0], line=k[1], function=k[2], calls=v[1],
                                          ownSeconds=v[2], cumulativeSeconds=v[3])
                    for k, v in sorted(stats.stats.items(), key=lambda item: -item[1][3])[:18]]
            if args.compare_report:
                prior = json.loads(args.compare_report.read_text('utf-8'))
                current_author={k:v for k,v in report['outputHashes'].items() if k != 'compiled.bin'}
                prior_author={k:v for k,v in prior['outputHashes'].items() if k != 'compiled.bin'}
                differences=[k for k in current_author.keys()|prior_author.keys() if current_author.get(k)!=prior_author.get(k)]
                assert not differences, 'parameter export changed author bytes: '+', '.join(sorted(differences))
                report['exactAuthorMatch'] = True
            assert report['productionUnchanged'], 'benchmark changed installed Mod'
            if args.verifier:
                check = subprocess.run([str(args.verifier),str(args.game),str(args.mod),str(destination),
                                        Path(result['offlineReloadCache']).name],
                    capture_output=True,text=True,encoding='utf-8',errors='replace',
                    creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
                if check.returncode:
                    raise AssertionError(check.stdout + check.stderr)
                report['nativeVerification'] = check.stdout.strip()
            if args.report:
                args.report.parent.mkdir(parents=True, exist_ok=True)
                args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2), 'utf-8')
            print('MATERIAL-BENCHMARK ' + json.dumps({k:v for k,v in report.items()
                  if k not in ('outputHashes', 'profile')}, ensure_ascii=False))
            if profiler:
                print('MATERIAL-PROFILE ' + json.dumps(report['profile'], ensure_ascii=False))
    finally:
        mat[key] = original
        addon.write_texture = original_write
        package.unregister()


main()
