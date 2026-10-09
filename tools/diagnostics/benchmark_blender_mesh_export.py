"""Compare the open Blender selection without saving or publishing its Mod.

Call compare_current(game, mod, compilers) through Blender's Python console/MCP.
Each run has no compiled.bin and clears only the disposable geometry cache.
"""
import hashlib
import os
from pathlib import Path
import shutil
import tempfile


def hashes(root):
    return {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in root.rglob('*') if p.is_file()}


def compare_current(game, mod, compilers):
    import bpy
    from EIEM_Blender import eiem_blender_addon as addon
    from EIEM_Blender import eiem_native_export as exporter
    from EIEM_Blender import eiem_offline_reload as offline
    mod = Path(mod).resolve()
    compilers = [Path(p).resolve() for p in compilers]
    before = hashes(mod)
    objects = list(bpy.context.selected_objects)
    active = bpy.context.view_layer.objects.active
    mode = active.mode if active else 'OBJECT'
    filepath = bpy.data.filepath
    previous_game = os.environ.get('EFF_RELOAD_GAME')
    real_context = offline.reload_context
    reports = []
    expected = None
    try:
        if mode != 'OBJECT':
            bpy.ops.object.mode_set(mode='OBJECT')
        os.environ['EFF_RELOAD_GAME'] = str(Path(game).resolve())
        with tempfile.TemporaryDirectory(prefix='eff-blender-mesh-benchmark-') as folder:
            root = Path(folder)
            snapshot = root / 'snapshot'
            shutil.copytree(mod, snapshot, ignore=shutil.ignore_patterns('compiled.bin'))
            for index, compiler in enumerate(compilers):
                destination = root / str(index) / mod.name
                shutil.copytree(snapshot, destination)
                def context(destination, compiler=compiler):
                    current = real_context(destination)
                    return (compiler, *current[1:])
                offline.reload_context = context
                exporter._NATIVE_SNAPSHOT_CACHE.clear()
                stats = exporter.export_native_package(addon, destination, objects,
                                                       resource_scope='MESH')
                current = hashes(destination)
                if expected is None:
                    expected = current
                assert current == expected, 'compiler or author output differs'
                reports.append(dict(compiler=str(compiler), meshes=stats['meshes'],
                    materials=stats['materials'], textures=stats['textures'],
                    seconds=stats['exportSeconds'], stagesMs=stats['exportCostsMs'],
                    compilerCostsMs=stats['offlineCompilerCostsMs'],
                    compiledBytes=(destination/'compiled.bin').stat().st_size,
                    exactOutputMatch=True))
        assert hashes(mod) == before, 'production Mod changed during benchmark'
        return reports
    finally:
        offline.reload_context = real_context
        if previous_game is None:
            os.environ.pop('EFF_RELOAD_GAME', None)
        else:
            os.environ['EFF_RELOAD_GAME'] = previous_game
        bpy.context.view_layer.objects.active = active
        if mode != 'OBJECT':
            bpy.ops.object.mode_set(mode=mode)
        assert active is None or active.mode == mode, 'object mode changed'
        assert bpy.context.view_layer.objects.active == active, 'active object changed'
        assert set(bpy.context.selected_objects) == set(objects), 'selection changed'
        assert bpy.data.filepath == filepath, 'project changed'
