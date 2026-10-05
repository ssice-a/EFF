"""Exercise converter renames through the production F10 disk worker."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def hashes(root):
    return {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in root.rglob('*') if p.is_file()}


def immutable_copy(source, target):
    try:
        os.link(source, target)
    except OSError:
        shutil.copy2(source, target)
    return str(target)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('evidence', type=Path)
    args = parser.parse_args()
    args.game = args.game.resolve()
    args.evidence = args.evidence.resolve()
    workspace = Path(__file__).resolve().parents[2]
    author = args.game / 'plugin/mods/LZY'
    before = hashes(author)
    args.evidence.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='renamed-author-', dir=args.evidence) as folder:
        game = Path(folder)
        plugin = game / 'plugin'
        shutil.copytree(author, plugin / 'mods/LZY')
        shutil.copytree(args.game / 'plugin/resource-reload-baseline', plugin / 'resource-reload-baseline',
                        copy_function=immutable_copy)
        immutable_copy(args.game / 'GameAssembly.dll', game / 'GameAssembly.dll')
        shutil.copy2(args.game / 'plugin/eff.ini', plugin / 'eff.ini')
        compiler = workspace / 'bin/resource-pack/eff_resource_pack.exe'
        (plugin / 'tools').mkdir()
        shutil.copy2(compiler, plugin / 'tools/eff_resource_pack.exe')
        cache = plugin / 'resource-reload-cache'
        current = (args.game / 'plugin/resource-reload-cache/current.tsv').read_text('utf8')
        key = current.splitlines()[1].split('\t')[0]
        seed = args.game / 'plugin/resource-reload-cache'
        shutil.copytree(seed / 'generations' / key, cache / 'generations' / key,
                        copy_function=immutable_copy)
        shutil.copytree(seed / 'geometry', cache / 'geometry', copy_function=immutable_copy)
        (cache / 'current.tsv').write_text(current, 'utf8')
        (game / '.eff-isolated-author-test').write_text('isolated converter/author mutation test', 'ascii')
        png = game / 'RELOAD_FIRST.png'
        shutil.copy2(workspace / 'diagnostics/blender-mcp-export-fixed-20260929_095149/textures/body.png', png)
        proc = subprocess.run([str(workspace / 'bin/resource-directory-entry/offline_loader_smoke.exe'),
                               str(game), str(workspace / 'bin/texture-convert/eff_texture_convert.exe'), str(png)],
                              capture_output=True, encoding='utf8', errors='replace', timeout=240)
        (args.evidence / 'rename-reload-worker.txt').write_text(proc.stdout + proc.stderr, 'utf8')
        assert proc.returncode == 0, proc.stdout + proc.stderr
        assert proc.stdout.count('PASS renamed texture F10') == 2, proc.stdout
        generation = cache / 'generations' / (cache / 'current.tsv').read_text('utf8').splitlines()[1].split('\t')[0]
        check = subprocess.run([str(workspace / 'bin/resource-pack/inspect_payloads.exe'), str(generation),
                                str(plugin / 'mods')], capture_output=True, encoding='utf8', errors='replace', timeout=30)
        (args.evidence / 'rename-reload-payloads.txt').write_text(check.stdout + check.stderr, 'utf8')
        assert check.returncode == 0, check.stdout + check.stderr
    assert hashes(author) == before, 'installed author changed'
    report = dict(passed=True, oldTexturePathRemoved=True, preparedAndManifestPathsPassed=True,
                  productionF10ExternalCompilerInvoked=True, nativePayloadsPassed=True,
                  installedAuthorUnchanged=True, gameAcceptance=False)
    (args.evidence / 'rename-reload-verification.json').write_text(json.dumps(report, indent=2) + '\n', 'utf8')
    print('PASS converter rename/removal -> production F10 compiler -> new candidate; native payloads match; installed Mod unchanged')


if __name__ == '__main__':
    main()
