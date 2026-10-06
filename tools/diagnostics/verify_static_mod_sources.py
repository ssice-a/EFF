"""Use actual author data to compile static sources without a running game."""
import argparse
import shutil
import subprocess
import tempfile
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('game', 'compiler', 'source_tool', 'index'):
        parser.add_argument('--' + name.replace('_', '-'), type=Path, required=True)
    parser.add_argument('--previous-compiler', type=Path,
                        help='Optional old runtime-discovery compiler for red/green comparison')
    args = parser.parse_args()
    from verify_mod_set_reload import split_author
    import configparser
    flags = getattr(subprocess, 'CREATE_NO_WINDOW', 0)

    def run(command):
        result = subprocess.run([str(value) for value in command], capture_output=True,
                                text=True, encoding='utf-8', errors='replace', creationflags=flags)
        if result.returncode:
            raise RuntimeError(result.stdout[-2000:] + result.stderr[-4000:])
        print('\n'.join(result.stdout.splitlines()[-2:]), flush=True)
        return result.stdout

    with tempfile.TemporaryDirectory(prefix='eff-static-source-', dir='E:/') as folder:
        root = Path(folder)
        mods = root / 'plugin/mods'
        author = mods / 'LZY'
        shutil.copytree(args.game / 'plugin/mods/LZY', author)
        (author / 'source-inputs.bin').unlink(missing_ok=True)
        if args.previous_compiler:
            old = run([args.previous_compiler, '--mods', mods,
                       root / 'plugin/resource-reload-baseline', root / 'plugin/resource-reload-cache'])
            assert 'OFFLINE-SOURCE-LOOKUP-PENDING' in old
            print('RED previous compiler requires runtime lookup and produces no candidate.', flush=True)
        run([args.source_tool, '--prepare-static-sources', args.game, args.index, author, args.compiler])
        # Sources resolve relative to the real installed game's directory;
        # the original author files and production cache are never overwritten.
        baseline = args.game / 'plugin' / ('static-source-test-' + root.name)
        cache = root / 'plugin/static-cache'
        try:
            command = [args.compiler, '--mods', mods, baseline, cache]
            run(command)
            before = (cache / 'current.tsv').read_bytes()
            generation = cache / 'generations' / before.decode().splitlines()[1].split('\t')[0]
            assert (generation / 'manifest.bin').stat().st_size < 1024 * 1024
            assert not list(generation.rglob('*.vfs')) and not (generation / 'startup').exists()
            assert (author / 'source-inputs.bin').stat().st_size < 1024 * 1024
            print('GREEN static sources produced small rules with no complete Prefab/resource files.', flush=True)
            assert 'OFFLINE-REUSED' in run(command)
            assert (cache / 'current.tsv').read_bytes() == before
            assert 'STATIC-SOURCES-REUSED' in run([args.source_tool, '--prepare-static-sources', args.game, args.index, author, args.compiler])
            ini = configparser.ConfigParser(interpolation=None, delimiters=('=',))
            ini.optionxform = str
            ini.read(author / 'mod.ini', encoding='utf-8-sig')
            rules = [section for section in ini.sections() if section.startswith('Replace')]
            assert len(rules) > 1
            split_author(author, mods / 'A', rules[:1], 'A')
            split_author(author, mods / 'B', rules[1:], 'B')
            shutil.rmtree(author)
            for member in ('A', 'B'):
                run([args.source_tool, '--prepare-static-sources', args.game, args.index, mods / member, args.compiler])
            run(command)
            print('PASS two static Mod descriptors compose shared CAB/Prefab sources.', flush=True)
            shutil.rmtree(mods / 'B')
            run(command)
            shutil.rmtree(mods / 'A')
            run(command)
            print('PASS remove member and empty set compile from retained pristine sources.', flush=True)
        finally:
            resolved = baseline.resolve()
            assert resolved.parent == (args.game / 'plugin').resolve()
            assert resolved.name.startswith('static-source-test-eff-static-source-')
            if resolved.exists():
                shutil.rmtree(resolved)


if __name__ == '__main__':
    main()
