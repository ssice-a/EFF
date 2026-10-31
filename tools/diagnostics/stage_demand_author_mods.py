"""Convert installed author resources to native inputs without searching consumer PFBs.

Development staging only. Original Mods are read in place and remain untouched;
each output has its own author directory. No compiled.bin or game index is read.
"""
import argparse
import configparser
import json
import subprocess
import time
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from query_endfield_index import Reader


def dependency_rows(index, wanted_cabs):
    """Read author-side identities once; never discover consumer Prefabs."""
    classes = {'Material': 21, 'Texture2D': 28, 'Shader': 48, 'Cubemap': 89,
               'Texture3D': 117, 'Texture2DArray': 187, 'CubemapArray': 188}
    rows = {}
    reader = Reader(index)
    try:
        if reader.string() != 'EFF_END_FIELD_INDEX' or reader.int32() != 3:
            raise ValueError('Unsupported author index')
        reader.string()
        source, assets = None, []
        while reader.tell() < reader.size:
            kind = reader.byte()
            if kind == 0:
                break
            if kind == 1:
                name, path, typ = reader.string(), reader.string(), reader.string()
                pid, bundle = reader.int64(), reader.string()
                if bundle != source:
                    source, assets = bundle, []
                if typ in classes and path:
                    assets.append((path, name, pid, classes[typ]))
            elif kind == 2:
                cab, bundle = reader.string(), reader.string()
                for _ in range(reader.int32()):
                    reader.string()
                if cab not in wanted_cabs:
                    continue
                if source != bundle:
                    raise ValueError(f'Dependency bundle has no indexed assets: {cab}')
                for path, name, pid, class_id in assets:
                    row = ['Dependency', path, name, cab, str(pid), str(class_id), '']
                    key = (cab, pid)
                    if key in rows and rows[key] != row:
                        raise ValueError(f'Ambiguous logical dependency: {key}')
                    rows[key] = row
            else:
                raise ValueError(f'Unknown index record: {kind}')
    finally:
        reader.close()
    return rows


def stage(mods, sources, compiler, output):
    templates = {}
    material_dependencies = {}
    texture_template = None
    manifests = sorted(path for path in sources.rglob('source/manifest.json')
                       if path.parent.parent.suffix.lower() == '.eff')
    for manifest_path in manifests:
        source = manifest_path.parent.parent
        manifest = json.loads(manifest_path.read_text('utf-8-sig'))
        files = {row['cab']: row for row in manifest['serializedFiles']}
        for asset in manifest['resources']:
            if asset['type'] not in ('Mesh', 'Material', 'Texture2D'):
                continue
            native = asset['native']
            row = files[native['cab']]
            snapshot = (source / row['snapshot']['file']).resolve()
            if not snapshot.is_relative_to(source.resolve()):
                raise ValueError('Template snapshot escapes EFFSOURCE')
            identity = (asset['logicalPath'], asset['name'])
            value = [asset['type'], *identity, native['cab'], str(native['pathId']),
                     str(snapshot), row['snapshot']['sha256']]
            templates.setdefault((asset['type'], *identity), value)
            if asset['type'] == 'Material':
                material_dependencies.setdefault(identity, {native['cab']} |
                    {external['cab'] for external in row.get('externals', [])})
            if texture_template is None and asset['type'] == 'Texture2D' and not row.get('externals'):
                texture_template = value
    output.mkdir(parents=True, exist_ok=True)
    wanted_cabs = set()
    for mod in mods.iterdir():
        if not mod.is_dir() or not (mod / 'mod.ini').is_file():
            continue
        ini = configparser.ConfigParser(interpolation=None, strict=False)
        ini.read(mod / 'mod.ini', encoding='utf-8-sig')
        for section in ini.sections():
            if section.startswith('Material'):
                wanted_cabs.update(material_dependencies.get(
                    (ini[section]['source.path'], ini[section]['source.asset']), ()))
    dependencies = dependency_rows(sources / 'index/endfield_assets.eidx', wanted_cabs)
    reports = []
    for mod in sorted(mods.iterdir()):
        if not mod.is_dir() or not (mod / 'mod.ini').is_file():
            continue
        ini = configparser.ConfigParser(interpolation=None, strict=False)
        ini.optionxform = str
        ini.read(mod / 'mod.ini', encoding='utf-8-sig')
        wanted = set()
        for name in ini.sections():
            row = ini[name]
            if name.startswith('Replace') and row.get('handling') != 'skip':
                wanted.add(('Mesh', row['target.path'], row['target.asset']))
            elif name.startswith('Material'):
                wanted.add(('Material', row['source.path'], row['source.asset']))
        missing = wanted - templates.keys()
        if missing:
            raise ValueError(f'{mod.name}: missing author templates: {sorted(missing)}')
        table = output / f'{mod.name}-templates.tsv'
        rows = [templates[key] for key in sorted(wanted)]
        cabs = set().union(*(material_dependencies[key[1:]] for key in wanted
                            if key[0] == 'Material'))
        rows += [row for key, row in sorted(dependencies.items()) if key[0] in cabs]
        if texture_template is not None:
            rows.append(texture_template)
        if any(any(char in cell for char in '\r\n\t') for row in rows for cell in row):
            raise ValueError('Template identity cannot be represented by TSV')
        table.write_text('EFF_AUTHOR_TEMPLATES/exact-assets+native-snapshots+logical-dependencies/tsv\n' +
                         ''.join('\t'.join(row) + '\n' for row in rows), encoding='utf-8')
        destination = output / mod.name
        if destination.exists():
            raise ValueError(f'Staged output already exists: {destination}')
        started = time.perf_counter()
        result = subprocess.run([str(compiler.resolve()), str(mod.resolve()), str(table.resolve()),
                                 str(destination.resolve())], capture_output=True, encoding='utf-8',
                                errors='replace', creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        report = dict(mod=mod.name, seconds=time.perf_counter()-started,
                      success=result.returncode == 0, compiler=result.stdout.strip(),
                      error=result.stderr.strip(), returnCode=result.returncode,
                      consumerPrefabSearch=False, completeFeatureMigration=False)
        reports.append(report)
        (output / 'report.json').write_text(json.dumps(reports, ensure_ascii=False, indent=2), encoding='utf-8')
        print(json.dumps(report, ensure_ascii=False), flush=True)
        if result.returncode:
            raise RuntimeError(f'{mod.name}: {report["error"]}')
    return reports


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('mods', 'sources', 'compiler', 'output'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    stage(args.mods, args.sources, args.compiler, args.output)
