"""Build one real standalone author Mesh from its exact source template.

Reads a Mod and EFFSOURCE without changing either. Only the explicitly named
output directory receives Mesh/probe files; no Prefab consumer search occurs.
"""
import argparse
import configparser
import hashlib
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mod', type=Path)
    parser.add_argument('source', type=Path)
    parser.add_argument('writer', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    ini = configparser.ConfigParser(interpolation=None, strict=False)
    ini.optionxform = str
    ini.read(args.mod/'mod.ini', encoding='utf-8-sig')
    source = json.loads((args.source/'source/manifest.json').read_text('utf-8-sig'))
    if source.get('format') != 'EFFSOURCE':
        raise ValueError('Requires an EFFSOURCE with native templates')
    selected = next((section for section in ini.sections()
                    if section.startswith('Replace') and
                    ini[section].get('target.path', '').endswith('.asset') and
                    ini[section].get('handling') != 'skip' and
                    ini[section].get('parts')), None)
    if selected is None:
        raise ValueError('No primary native Mesh target with author Parts')
    rule = ini[selected]
    part = rule['parts'].split(',')[0].strip()
    matches = [r for r in source['resources'] if r['type'] == 'Mesh' and
               r['logicalPath'] == rule['target.path'] and
               r['name'] == rule['target.asset']]
    if len(matches) != 1:
        raise ValueError('Source must contain exactly this path + asset')
    mesh = matches[0]
    row = next(r for r in source['serializedFiles']
               if r['cab'] == mesh['native']['cab'])
    template = (args.source/row['snapshot']['file']).resolve()
    if not template.is_relative_to(args.source.resolve()):
        raise ValueError('Template escapes EFFSOURCE')
    if hashlib.sha256(template.read_bytes()).hexdigest().upper() != row['snapshot']['sha256']:
        raise ValueError('Template content differs from EFFSOURCE')
    output = args.output.resolve()
    if output.is_relative_to(args.mod.resolve()) or output.is_relative_to(args.source.resolve()):
        raise ValueError('Diagnostic output must be separate from authors/source')
    output.mkdir(parents=True, exist_ok=True)
    destination = output/'mesh.ab'
    subprocess.run([str(args.writer.resolve()), str(args.mod.resolve()),
                    str(template), row['cab'], rule['target.path'],
                    rule['target.asset'], part, str(mesh['native']['pathId']),
                    str(destination)], check=True)
    print(json.dumps(dict(mod=ini['Mod']['id'], target=rule['target.asset'],
                          part=part, bundleBytes=destination.stat().st_size,
                          sourceUnchanged=True, consumerPrefabs=0)))


if __name__ == '__main__':
    main()
