"""Compile/prepare 0/1/N author sets from isolated copies of real resources.

The LZY baseline supplies shared CAB/PFB evidence, not a second character's
game acceptance. Distinct character control ownership is tested by the native
and control fixtures. This test splits nonoverlapping targets of that baseline.
"""
import argparse
import configparser
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def split_author(source, destination, chosen, identity):
    original = configparser.ConfigParser(interpolation=None, delimiters=('=',))
    original.optionxform = str
    original.read(source / 'mod.ini', encoding='utf-8-sig')
    keep = set(chosen)
    parts = {p.strip() for rule in chosen for p in original[rule].get('parts', '').split(',') if p.strip()}
    keep.update(parts)
    materials = {original[p]['material'] for p in parts}
    keep.update(materials)
    keep.update(original[p]['mesh'] for p in parts)
    keep.update(value for material in materials for key, value in original[material].items() if key.startswith('texture.'))
    variables = {value for p in parts for key, value in original[p].items() if key.startswith('shape.')}
    keep.update(s for s in original.sections() if s.startswith('ShapeControl') and original[s]['variable'] in variables)
    # Retain switch constants/bindings: they are isolated by original Mod ID.
    keep.update(s for s in original.sections() if s.startswith('Key') and
                (not s.startswith('KeyShape') or any(v in original[s] for v in variables)))
    keep.update(('Mod', 'Constants'))
    output = configparser.ConfigParser(interpolation=None, delimiters=('=',))
    output.optionxform = str
    destination.mkdir(parents=True)
    for section in original.sections():
        if section not in keep:
            continue
        output[section] = dict(original[section])
        if section == 'Mod':
            output[section]['id'] = identity
        if section == 'Constants':
            for name in list(output[section]):
                if '$shape_' in name and name.removeprefix('persist ') not in variables:
                    del output[section][name]
        if 'path' in original[section]:
            relative = Path(original[section]['path'])
            target = destination / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source / relative, target)
    with (destination / 'mod.ini').open('w', encoding='utf-8') as stream:
        output.write(stream, space_around_delimiters=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('compiler', 'loader', 'author', 'baseline', 'game'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--temp-root', type=Path, default=Path('E:/'))
    args = parser.parse_args()
    flags = getattr(subprocess, 'CREATE_NO_WINDOW', 0)
    with tempfile.TemporaryDirectory(prefix='eff-mod-set-', dir=args.temp_root) as folder:
        game = Path(folder)
        mods = game / 'plugin/mods'
        mods.mkdir(parents=True)
        baseline = game / 'plugin/resource-reload-baseline'
        shutil.copytree(args.baseline, baseline)
        assert not (game / 'GameAssembly.dll').exists()
        # Loader only validates source ranges while preparing a nonempty set.
        # Copy referenced physical files by range so their offsets remain exact.
        for line in (baseline / 'source-ranges.tsv').read_text('utf-8-sig').splitlines()[1:]:
            _, source, offset, size, digest = line.split('\t')
            source = Path(source)
            relative = source.relative_to(args.game)
            target = game / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            with source.open('rb') as stream:
                stream.seek(int(offset)); data = stream.read(int(size))
            assert hashlib.sha256(data).hexdigest().upper() == digest
            mode = 'r+b' if target.exists() else 'w+b'
            with target.open(mode) as stream:
                stream.seek(int(offset)); stream.write(data)
        rows = (baseline / 'source-ranges.tsv').read_text('utf-8-sig')
        rows = rows.replace(str(args.game), str(game))
        (baseline / 'source-ranges.tsv').write_text(rows, 'utf-8')
        cache = game / 'plugin/resource-reload-cache'
        tools = game / 'plugin/tools'; tools.mkdir()
        shutil.copyfile(args.compiler, tools / 'eff_resource_pack.exe')
        (game / '.eff-isolated-author-test').touch()
        env = dict(os.environ, TEMP=folder, TMP=folder)
        command = [str(tools / 'eff_resource_pack.exe'), '--mods', str(mods), str(baseline), str(cache)]

        def compile(label, success=True):
            result = subprocess.run(command, cwd=folder, env=env, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=180, creationflags=flags)
            assert (result.returncode == 0) == success, label + '\n' + result.stdout + result.stderr
            print(label, 'PASS', result.stdout.splitlines()[-1:] or result.stderr[-200:], flush=True)
            return (cache / 'current.tsv').read_bytes()

        def prepare(label, lifecycle=False):
            command = [str(args.loader.resolve()), str(game)] + (['--mod-set-lifecycle'] if lifecycle else [])
            result = subprocess.run(command, cwd=folder, env=env, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=120, creationflags=flags)
            assert result.returncode == 0, label + '\n' + result.stdout + result.stderr
            print(label, result.stdout.strip(), flush=True)

        def remove_author(name):
            path = (mods / name).resolve()
            assert path.parent == mods.resolve() and path.is_relative_to(game.resolve())
            shutil.rmtree(path)

        empty = compile('0 Mods'); prepare('0 Mods DLL preparation')
        mods.rmdir()
        assert compile('Missing mods directory -> pristine') == empty
        assert not mods.exists()
        prepare('missing mods directory DLL preparation')
        mods.mkdir()
        pristine = cache / 'generations' / empty.decode().splitlines()[1].split('\t')[0]
        assert len((pristine / 'loading-inputs.tsv').read_text('utf-8').splitlines()) == 1
        for source in baseline.glob('input-*.vfs'):
            assert source.read_bytes() == (pristine / 'startup' / source.name).read_bytes()
        split_author(args.author, mods / 'CharacterA', ['ReplaceS_actor_lizhiyan_body_01_lod0'], 'CharacterA')
        one = compile('0 -> 1 Mod'); prepare('1 Mod DLL preparation')
        assert compile('No GameAssembly file required, same candidate reused') == one
        prepare('DLL disk preparation without GameAssembly file')
        split_author(args.author, mods / 'CharacterB', ['ReplaceS_actor_lizhiyan_cloth_03_lod0', 'ReplaceS_actor_lizhiyan_cloth_06_lod0'], 'CharacterB')
        two = compile('1 -> 2 Mods shared CAB/PFB'); prepare('2 Mods DLL preparation / F10 membership lifecycle', True)
        assert len({empty, one, two}) == 3
        before = (cache / 'current.tsv').read_bytes()
        shutil.copytree(mods / 'CharacterA', mods / 'Conflict')
        ini = mods / 'Conflict/mod.ini'
        ini.write_text(ini.read_text('utf-8').replace('id=CharacterA', 'id=Conflict'), 'utf-8')
        assert compile('Conflicting target rejected', False) == before
        remove_author('Conflict')
        remove_author('CharacterA')
        compile('Delete selected author'); prepare('remaining author DLL preparation')
        remove_author('CharacterB')
        assert compile('Delete last author -> pristine') == empty
        prepare('empty set DLL preparation')
        split_author(args.author, mods / 'CharacterA', ['ReplaceS_actor_lizhiyan_body_01_lod0'], 'CharacterA')
        assert compile('Readd after empty') == one
        prepare('readded author DLL preparation')
        assert not (game / 'GameAssembly.dll').exists()
        print('PASS isolated real-input Mod membership, shared native graph, pristine startup bytes, zero substitutions, failed publication retention, no GameAssembly file; game/Blender untouched', flush=True)


if __name__ == '__main__':
    main()
