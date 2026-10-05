"""Verify the Blender output against preserved native schemas, not INI plans."""
import argparse
import copy
import json
from pathlib import Path
import sys
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from nativepack.author_source import AuthorCore,AuthorSource
from nativepack.ini_reader import read_package
from nativepack.mesh import expand_shaped_frames
from nativepack.type_tree import decode,encode,require

def verify(package_path,baseline,core):
    package=read_package(package_path);source=AuthorSource(baseline,AuthorCore(core));meshes=[]
    by_part={p['partId']:p for p in package['parts']}
    for rule in package['rules']:
        for target in rule['targets']:
            native=source.resolve('Mesh',target['sourcePath'],target['sourceAsset']);template=source.tree(native)
            for pid in rule['parts']:
                author=by_part[pid]
                tree=copy.deepcopy(template)
                for field in author['nativeFields']:
                    data=package['_resources'][field['file']]['blocks'][field['name']]
                    tree.data[field['name']]=decode(tree[field['name']].schema,data)
                expand_shaped_frames(tree)
                vertex=tree['m_VertexData'];count=vertex['m_VertexCount'].data
                channels=[c.plain() for c in vertex['m_Channels'].items()]
                widths=(4,2,1,1,2,2,1,1,2,2,4,4);strides=[0]*4
                for c in channels:
                    if c['dimension']&15: strides[c['stream']]=max(strides[c['stream']],c['offset']+(c['dimension']&15)*widths[c['format']])
                positions=np.ndarray((count,3),dtype='<f4',buffer=vertex['m_DataSize'].data,strides=(strides[0],4))
                require(np.all(np.isfinite(positions)),'nonfinite Blender positions')
                idx=np.frombuffer(bytes(tree['m_IndexBuffer'].items()),dtype='<u4' if tree['m_IndexFormat'].data else '<u2')
                require(len(idx)%3==0 and np.all(idx<count),'Blender index range')
                shapes=len(tree['m_Shapes']['channels'].items())
                require(strides[0]==(40 if shapes else 16),'Blender frame storage profile')
                require(len(tree['m_BindPose'].items())==len(author['bonePaths']),'Blender palette count')
                require(encode(decode(tree.schema,encode(tree)))==encode(tree),'independent whole Mesh roundtrip')
                meshes.append(dict(part=pid,target=target['sourceAsset'],vertices=count,indices=len(idx),shapeChannels=shapes,stride=strides[0]))
    return dict(format='EFF_BLENDER_NATIVE_EXPORT_VERIFICATION',version=1,verified=True,
                parts=len(package['parts']),materials=len(package['materials']),textures=len(package['textures']),
                rules=len(package['rules']),meshTargets=len(meshes),meshes=meshes,nativeSubmission=False)

if __name__=='__main__':
    parser=argparse.ArgumentParser();
    for name in ('mod','baseline','core','output'): parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();report=verify(args.mod,args.baseline,args.core)
    args.output.write_text(json.dumps(report,indent=2)+'\n','utf-8');print(json.dumps({k:v for k,v in report.items() if k!='meshes'}))
