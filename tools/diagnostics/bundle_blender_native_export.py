"""Assemble the installed Blender package from canonical sources and core.

Does not publish a release or modify the game. Geometry modules have one source
of truth under tools/nativepack; bundled copies are build artifacts.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

ROOT=Path(__file__).resolve().parents[2]
MODULES=('author_source','serialized_file','type_tree','mesh','delta_apply',
         'resource_file','ini_package','ini_reader','texture_fields','state_rules','payload')

def bundle(destination):
    destination=Path(destination).resolve()
    if destination.exists(): raise FileExistsError('Blender bundle exists: '+str(destination))
    destination.mkdir(parents=True)
    for path in (ROOT/'tools/Blender').glob('*.py'):
        if path.name=='__init__.py' or path.name.startswith('eiem_'): shutil.copy2(path,destination/path.name)
    core=destination/'nativepack';core.mkdir()
    (core/'__init__.py').write_text('',encoding='utf-8')
    for name in MODULES: shutil.copy2(ROOT/'tools/nativepack'/(name+'.py'),core/(name+'.py'))
    shutil.copy2(ROOT/'bin/author-core/eff_author_core.dll',core/'eff_author_core.dll')
    shutil.copy2(ROOT/'bin/resource-pack/eff_resource_pack.exe',core/'eff_resource_pack.exe')
    vendor=ROOT/'bin/author-core/vendor'
    if not (vendor/'PIL').is_dir(): raise FileNotFoundError('Install Pillow for the target Blender Python into bin/author-core/vendor')
    shutil.copytree(vendor,destination/'vendor',ignore=shutil.ignore_patterns('__pycache__','*.pyc'))
    return dict(format='EFF_BLENDER_NATIVE_EXPORT_BUNDLE',version=1,path=str(destination),
                sourceGeometryCore='tools/nativepack',sourceParser='src/eiem_native_input.h',
                files={p.relative_to(destination).as_posix():hashlib.sha256(p.read_bytes()).hexdigest().upper()
                       for p in destination.rglob('*') if p.is_file()})

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();report=bundle(args.output)
    args.output.with_suffix('.bundle.json').write_text(json.dumps(report,indent=2)+'\n','utf-8')
    print(json.dumps(dict(path=report['path'],files=len(report['files']))))
