"""Compare every native object in a scoped no-shape diagnostic graph."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from nativepack.author_source import AuthorCore
from nativepack.type_tree import Schema,decode,encode,require
from verify_object_order_probe import object_rows

def verify(before,after,checker,core,output,runtime_shapes=False):
    before,after=Path(before),Path(after)
    require({p.name for p in before.glob('*.vfs')}=={p.name for p in after.glob('*.vfs')},'input graph coverage')
    parser=AuthorCore(core);changes=[];meshes=renderers=0
    for first in sorted(before.glob('*.vfs')):
        second=after/first.name
        if first.read_bytes()==second.read_bytes(): continue
        directories=[]
        for label,path in (('before',first),('after',second)):
            directory=output.parent/'no-shape-verification-members'/label/path.stem
            subprocess.run([str(Path(checker).resolve()),'--endfield-members',str(path.resolve()),str(directory.resolve())],check=True,capture_output=True)
            directories.append(directory)
        old_files,new_files=({p.name:p for p in directory.iterdir()} for directory in directories)
        require(old_files.keys()==new_files.keys(),'native member identity')
        objects=[]
        for name in old_files:
            old,new=old_files[name].read_bytes(),new_files[name].read_bytes()
            if old==new: continue
            old_start,old_end,old_rows=object_rows(old)
            new_start,new_end,new_rows=object_rows(new)
            require(old[:20]==new[:20] and old[40:48]==new[40:48],'legacy header')
            require(old[48:old_start]==new[48:new_start],'type metadata')
            old_meta=struct.unpack_from('>I',old,20)[0];new_meta=struct.unpack_from('>I',new,20)[0]
            require(old[old_end:48+old_meta]==new[new_end:48+new_meta],'external/remaining metadata')
            require([(r[0],r[3]) for r in old_rows]==[(r[0],r[3]) for r in new_rows],'object identity/order/type')
            schema=parser.schema(old)
            require(schema==parser.schema(new),'source schemas')
            old_offset=struct.unpack_from('>q',old,32)[0];new_offset=struct.unpack_from('>q',new,32)[0]
            compiled={}
            for a,b in zip(old_rows,new_rows):
                x=old[old_offset+a[1]:old_offset+a[1]+a[2]];y=new[new_offset+b[1]:new_offset+b[1]+b[2]]
                if x==y: continue
                row=schema['types'][a[3]];kind=row['classId']
                require(kind in (43,137),'control changed another class')
                if a[3] not in compiled: compiled[a[3]]=Schema.compile(row['nodes'])
                original=decode(compiled[a[3]],x);changed=decode(compiled[a[3]],y)
                require(encode(original)==x and encode(changed)==y,'whole-object roundtrip')
                fields=[f for f in original.data if encode(original[f])!=encode(changed[f])]
                if kind==43:
                    if runtime_shapes:
                        require(set(fields)=={'m_Name','m_IsReadable'},'runtime source changed outside name/readability')
                        require(encode(original['m_Shapes'])==encode(changed['m_Shapes']) and original['m_Shapes']['channels'].items(),
                                'runtime source lost authored shapes')
                        require(changed['m_IsReadable'].data==1 and '__EFF_SHAPE_' in changed['m_Name'].data,'runtime source marker/readability')
                    else:
                        require(fields==['m_Shapes'],'Mesh changed outside shapes')
                        require(original['m_Shapes']['channels'].items(),'control source is unshaped')
                        require(all(not changed['m_Shapes'][f].items() for f in ('vertices','shapes','channels','fullWeights')),'shape arrays not empty')
                    channels=changed['m_VertexData']['m_Channels'].items()
                    require(channels[1]['dimension'].data==3 and channels[2]['offset'].data==24 and channels[2]['dimension'].data==4,'float40 layout lost')
                    meshes+=1
                else:
                    require(not runtime_shapes,'runtime preparation modified a Renderer')
                    require(fields==['m_BlendShapeWeights'] and not changed['m_BlendShapeWeights'].items(),'Renderer changed outside shape weights')
                    renderers+=1
                objects.append(dict(member=name,pathId=a[0],classId=kind,fields=fields))
        changes.append(dict(file=first.name,objects=objects))
    require(meshes==4 and renderers==(0 if runtime_shapes else 218),'current authored shape scope')
    report=dict(format='EFF_RUNTIME_SHAPE_INPUT_VERIFICATION' if runtime_shapes else 'EFF_AFTERIMAGE_SHAPE_CONTROL_VERIFICATION',version=1,verified=True,changedInputs=len(changes),meshes=meshes,renderers=renderers,
                allOtherObjectFieldsPreserved=True,unrelatedNativeShapeObjectsPreserved=True,float40VertexBytesPreserved=True,nativeSubmission=False,changes=changes)
    output.write_text(json.dumps(report,indent=2)+'\n','utf-8')
    print(json.dumps({k:v for k,v in report.items() if k!='changes'}))

if __name__=='__main__':
    p=argparse.ArgumentParser()
    for name in ('before','after','checker','core','output'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--runtime-shapes',action='store_true')
    a=p.parse_args();verify(a.before,a.after,a.checker,a.core,a.output,a.runtime_shapes)
