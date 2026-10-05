"""Cross-check the DLL's C++ readers against independent offline resource readers."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from nativepack.ini_reader import read_package
from nativepack.type_tree import Schema, decode


def run(exe, *args):
    result = subprocess.run([str(exe), *map(str,args)], capture_output=True, encoding='utf-8')
    if result.returncode:
        raise ValueError(result.stderr.strip() or result.stdout.strip())
    return [line.split('\t') for line in result.stdout.splitlines()]


def verify(exe, mod, source, schema_path, bundles):
    package = read_package(mod)
    rows = run(exe, mod)
    blocks = {(r[1], r[2]): (int(r[3]), int(r[4]), r[5]) for r in rows if r[0] == 'BLOCK'}
    expected = {(relative, d['name']): (d['offset'], d['size'], d['sha256'])
                for relative, resource in package['_resources'].items() for d in resource['descriptors']}
    assert blocks == expected, 'C++/Python block directory/hash mismatch'
    targets = {(r[1],r[2],r[3]) for r in rows if r[0] == 'TARGET'}
    expected_targets = {(t['sourcePath'],t['sourceAsset'],r['id']) for r in package['rules'] for t in r['targets']}
    assert targets == expected_targets, 'C++ exact target identity mismatch'
    parts = {(r[1],r[2],r[3],r[4]=='1') for r in rows if r[0]=='PART'}
    assert parts == {(p['partId'],p['mesh'],p['material'],p['initialVisible']) for p in package['parts']}, 'C++ linkage/visibility differs'
    manifest=json.loads((source/'source/manifest.json').read_text(encoding='utf-8'))
    schema=json.loads(schema_path.read_text(encoding='utf-8'))
    schemas={f['cab']:f for f in schema['files']}
    identities=[]
    for f in manifest['serializedFiles']:
        data=(source/f['snapshot']['file']).read_bytes()
        types={t['index']:t for t in schemas[f['cab']]['types']}
        objects={o['pathId']:o for o in f['objects']}
        names={}
        for o in f['objects']:
            if o['classId'] not in (43,21,28,142):continue
            typ=types[o['typeIndex']]
            tree=decode(Schema.compile(typ['nodes']),data[o['offset']:o['offset']+o['size']])
            if 'm_Name' in tree.data:names[o['pathId']]=tree['m_Name'].data
        native=[]
        for o in f['objects']:
            if o['classId']!=142:continue
            tree=decode(Schema.compile(types[o['typeIndex']]['nodes']),data[o['offset']:o['offset']+o['size']])
            for pair in tree['m_Container'].items():
                path=pair['first'].data;ptr=pair['second']['asset'];file_id=ptr['m_FileID'].data;pid=ptr['m_PathID'].data
                if file_id:continue # separate dependency resolution, never infer asset by name
                target=objects[pid];typ=types[target['typeIndex']]
                native.append((path,names.get(pid,''),str(pid),str(target['classId']),typ['oldTypeHash']))
        actual=run(exe,'--native',source/f['snapshot']['file'])
        parsed=[(r[1],r[2],r[4],r[5],r[6]) for r in actual if r[0]=='ASSET' and r[3]=='<input>']
        assert sorted(parsed)==sorted(native), 'native AssetBundle identity differs: '+f['cab']
        identities.extend(parsed)
    bundle_count=0
    for file in sorted(bundles.rglob('*.ab')):
        run(exe,'--native',file);bundle_count+=1
    resolved={(r[0],r[1]) for r in identities}
    assert {(a,b) for a,b,_ in targets} <= resolved,'some Mod targets absent from native AssetBundle table'
    native_types = {(r[0], r[1]): r[4] for r in identities}
    by_part = {p['partId']: p for p in package['parts']}
    for rule in package['rules']:
        for target in rule['targets']:
            for part_id in rule['parts']:
                assert by_part[part_id]['nativeTypeHash'] == native_types[(target['sourcePath'], target['sourceAsset'])], 'native Mesh schema incompatibility'
    # Invalid packages must be rejected with a reason. Only fixtures are mutated.
    mutations={
        'unknown_section': lambda text:text+'\n[Typo]\nx=y\n',
        'unresolved_mesh': lambda text:text.replace('mesh=MeshS_actor_lizhiyan_body_01_lod0_0','mesh=MeshMissing',1),
        'mip_size': lambda text:text.replace('width=1024','width=1023',1),
        'path_escape': lambda text:text.replace('path=textures/body_','path=textures/../body_',1),
        'duplicate_target': lambda text:text+'\n[ReplaceDuplicate]\ntarget.path=assets/beyond/arts/entity/actor/girl/lizhiyan/models/s_actor_lizhiyan_body_01_lod0.asset\ntarget.asset=S_actor_lizhiyan_body_01_lod0\nhandling=skip\n',
    }
    rejected=[]
    with tempfile.TemporaryDirectory(prefix='eff-resource-entry-check-') as temp:
        root=Path(temp)/'LZY';root.mkdir()
        # Private copies keep corruption fixtures independent of published resources.
        for relative in package['_files']:
            if relative=='mod.ini':continue
            target=root/relative;target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copyfile(mod/relative,target)
        original=(mod/'mod.ini').read_text(encoding='utf-8-sig')
        for label,mutate in mutations.items():
            (root/'mod.ini').write_text(mutate(original),encoding='utf-8')
            result=subprocess.run([str(exe),str(root)],capture_output=True,encoding='utf-8')
            assert result.returncode and 'REJECT\t' in result.stderr, 'missing failure diagnosis: '+label
            rejected.append(dict(case=label,reason=result.stderr.strip()))
        # Kind/version and damaged hashes verified without touching deployed resources.
        sample=next(iter(package['_resources']))
        target=root/sample;target.unlink();raw=bytearray((mod/sample).read_bytes())
        raw[-1]^=1;target.write_bytes(raw);(root/'mod.ini').write_text(original,encoding='utf-8')
        result=subprocess.run([str(exe),str(root)],capture_output=True,encoding='utf-8')
        assert result.returncode and 'SHA256 mismatch' in result.stderr,'damaged block was accepted'
        rejected.append(dict(case='damaged_binary',reason=result.stderr.strip()))
    return dict(verified=True,gameLoadVerified=False,nativeReplacement=False,package=str(mod),
                resources=len(package['_resources']),blocks=len(blocks),targets=len(targets),
                serializedFiles=len(manifest['serializedFiles']),sourceAssetIdentities=len(identities),
                nativeBundles=bundle_count,rejectionCases=rejected)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--exe',type=Path,required=True);p.add_argument('--mod',type=Path,required=True)
    p.add_argument('--source',type=Path,required=True);p.add_argument('--schema',type=Path,required=True)
    p.add_argument('--bundles',type=Path,required=True);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
    try:result=verify(a.exe.resolve(),a.mod.resolve(),a.source.resolve(),a.schema.resolve(),a.bundles.resolve())
    except Exception as e:
        result=dict(verified=False,error=str(e));a.report.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');raise
    a.report.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(result,ensure_ascii=False,indent=2))
