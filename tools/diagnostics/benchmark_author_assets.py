"""Measure source-independent Mesh/Material export on a read-only real Mod.

The template table is a temporary author input, not a player replacement
manifest. Generated assets are removed with the benchmark's own directory.
"""
import argparse
import configparser
import hashlib
import json
from pathlib import Path
import subprocess
import shutil
import tempfile
import time


def hashes(root):
    return {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in root.rglob('*') if p.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mod', type=Path)
    parser.add_argument('sources', type=Path)
    parser.add_argument('compiler', type=Path)
    parser.add_argument('--report', type=Path)
    parser.add_argument('--probe-sample', type=Path,
                        help='retain directory and first native texture for game acceptance')
    args = parser.parse_args()
    before = hashes(args.mod)
    ini = configparser.ConfigParser(interpolation=None, strict=False)
    ini.optionxform = str
    ini.read(args.mod/'mod.ini', encoding='utf-8-sig')
    wanted = {}
    for section in ini.sections():
        row = ini[section]
        if section.startswith('Replace') and row.get('handling') != 'skip':
            wanted[(row['target.path'], row['target.asset'])] = 'Mesh'
        if section.startswith('Material'):
            wanted[(row['source.path'], row['source.asset'])] = 'Material'
    found = {}
    texture_template = None
    for source in args.sources.glob('*.eff'):
        path = source/'source/manifest.json'
        if not path.is_file():
            continue
        manifest = json.loads(path.read_text('utf-8-sig'))
        files = {f['cab']: f for f in manifest['serializedFiles']}
        for asset in manifest['resources']:
            if texture_template is None and asset['type'] == 'Texture2D':
                native = asset['native']
                row = files[native['cab']]
                if not row.get('externals'):
                    texture_template = ['Texture2D',asset['logicalPath'],asset['name'],native['cab'],str(native['pathId']),str((source/row['snapshot']['file']).resolve()),row['snapshot']['sha256']]
            identity = (asset['logicalPath'], asset['name'])
            if identity not in wanted or asset['type'] != wanted[identity] or identity in found:
                continue
            native = asset['native']
            row = files[native['cab']]
            snapshot = (source/row['snapshot']['file']).resolve()
            if not snapshot.is_relative_to(source.resolve()):
                raise ValueError('Template snapshot escapes EFFSOURCE')
            found[identity] = [wanted[identity], *identity, native['cab'],
                               str(native['pathId']), str(snapshot), row['snapshot']['sha256']]
    if found.keys() != wanted.keys():
        raise ValueError('Missing exact author templates: '+str(wanted.keys()-found.keys()))
    with tempfile.TemporaryDirectory(prefix='eff-independent-author-') as folder:
        root = Path(folder)
        table = root/'author-templates.tsv'
        rows = ['EFF_AUTHOR_TEMPLATES/exact-assets+native-snapshots/tsv']
        for identity in sorted(found):
            row = found[identity]
            if any(any(c in value for c in '\r\n\t') for value in row):
                raise ValueError('Template identity cannot be represented by TSV')
            rows.append('\t'.join(row))
        if texture_template is not None:
            rows.append('\t'.join(texture_template))
        table.write_text('\n'.join(rows)+'\n', encoding='utf-8')
        started = time.perf_counter()
        p = subprocess.run([str(args.compiler.resolve()), str(args.mod.resolve()),
                            str(table), str(root/'assets')], capture_output=True,
                           encoding='utf-8', errors='replace',
                           creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        elapsed = time.perf_counter()-started
        if p.returncode:
            raise RuntimeError(p.stderr or p.stdout)
        output = hashes(root/'assets')
        report = dict(mod=args.mod.name, wallSeconds=elapsed,
                      compilerResult=p.stdout.strip(), outputFiles=len(output),
                      outputBytes=sum(p.stat().st_size for p in (root/'assets').rglob('*') if p.is_file()),
                      assetsDirectoryBytes=(root/'assets/assets.bin').stat().st_size,
                      outputSHA256=output, authorUnchanged=hashes(args.mod)==before,
                      texturesMigrated=True, physicsMigrated=False,
                      completeModExport=False, consumerPrefabSearch=False)
        assert report['authorUnchanged'], 'Author/Mod inputs changed'
        if args.probe_sample:
            sample = args.probe_sample.resolve()
            if sample.is_relative_to(args.mod.resolve()) or sample.is_relative_to(args.sources.resolve()):
                raise ValueError('Probe sample must stay outside authors and source')
            sample.mkdir(parents=True,exist_ok=True)
            shutil.copy2(root/'assets/assets.bin',sample/'assets.bin')
            textures = sorted((root/'assets/textures').glob('*.tex'))
            if not textures:
                raise ValueError('Probe sample requires one authored texture')
            (sample/'textures').mkdir(exist_ok=True)
            shutil.copy2(textures[0],sample/'textures'/textures[0].name)
            print('PROBE_TEXTURE_SAMPLE '+textures[0].name)
        if args.report:
            args.report.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
        print(json.dumps({k:v for k,v in report.items() if k!='outputSHA256'},ensure_ascii=False))


if __name__ == '__main__':
    main()
