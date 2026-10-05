"""Offline cloth_06 resource-load-input experiment. Never deploys to the game.

The source identity is resolved from exact path + asset. No runtime setters,
native-memory copy or alternate replacement route exists in this tool.
"""
from __future__ import annotations
import argparse
import copy
import hashlib
import json
from pathlib import Path
import shutil
import sys
import time
import traceback
import numpy as np
from PIL import Image

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'Blender'))
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'diagnostics'))
from eiem_format import read_mesh,parse_ini,safe_path
from validate_selected_mod_author import SourceCatalog
from nativepack.type_tree import require,encode,Value,from_data
from nativepack.serialized_file import SerializedFile,unityfs
from nativepack.mesh import native_mesh,aabb


def write_json(path,value):
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')


def sha(data):
    return hashlib.sha256(data).hexdigest().upper()


def identity(cab,path_id):
    return cab.lower()+':'+str(path_id)


def pointer(file_id,path_id):
    return dict(m_FileID=file_id,m_PathID=path_id)


class Experiment:
    def __init__(self,capture,preflight,schema,output):
        self.capture_file,self.output=capture,output
        self.fixture=capture.parent
        self.capture=json.loads(capture.read_text(encoding='utf-8'))
        self.preflight=json.loads(preflight.read_text(encoding='utf-8'))
        require(self.preflight['passed'] and not self.preflight['nativeLoadVerified'],'preflight contract')
        require(Path(self.preflight['capture']).resolve()==capture.resolve(),'preflight capture path')
        self.schema=json.loads(schema.read_text(encoding='utf-8'))
        require((self.schema['format'],self.schema['version'])==('EFF_NATIVE_SCHEMA',1),'native schema version')
        self.source=SourceCatalog(Path(self.capture['sourceBaseline']))
        self.package=self.source.package
        require(self.schema['vfsFingerprint']==self.capture['vfsFingerprint']==self.source.manifest['vfsFingerprint'],'source identity version differs')
        require(Path(self.schema['sourcePackage']).resolve()==self.package.resolve(),'schema source package')
        self.schemas={f['cab']:f for f in self.schema['files']}
        self.files={}
        for row in self.source.manifest['serializedFiles']:
            data=safe_path(self.package,row['snapshot']['file']).read_bytes()
            self.files[row['cab']]=SerializedFile(data,row,self.schemas[row['cab']])
            require(self.files[row['cab']].build()==data,'unmodified file roundtrip')
        self.streams={r['member']:bytearray(safe_path(self.package,r['snapshot']['file']).read_bytes()) for r in self.source.manifest['rawStreams']}
        self.original_stream_lengths={k:len(v) for k,v in self.streams.items()}
        for row in self.source.manifest['rawStreams']:
            data=self.streams[row['member']]
            require(len(data)==row['snapshot']['size'] and sha(data)==row['snapshot']['sha256'],'raw stream integrity')
        self.reference_scan_gaps=[]
        self.parts=[]
        self.texture_evidence=[]
        self.written_textures={}
        self.stage='source-loaded'

    def file_for(self,resource):
        return self.files[resource['native']['cab']]

    def pp(self,owner_file,target_identity):
        cab,path_id=target_identity.rsplit(':',1)
        if cab==owner_file.cab.lower():return pointer(0,int(path_id))
        matches=[r['fileId'] for r in owner_file.manifest['externals'] if r['path'].replace('\\','/').rsplit('/',1)[-1].lower()==cab]
        require(len(matches)==1,'required native external not uniquely registered: '+target_identity)
        return pointer(matches[0],int(path_id))

    def rebuild_preloads(self,file,additions):
        bundles=[r for r in file.objects.values() if r['classId']==142]
        require(len(bundles)==1,'source requires one native AssetBundle record')
        bundle_id=bundles[0]['pathId']
        value=file.tree(bundle_id)
        old=len(value['m_PreloadTable'].items())
        require(len(value['m_Container'].items())==1 and len(value['m_HashContainer'].items())==1,'experiment requires single resource entry')
        existing={(p['m_FileID'].data,p['m_PathID'].data) for p in value['m_PreloadTable'].items()}
        additions=[p for p in additions if (p['m_FileID'],p['m_PathID']) not in existing]
        if not additions:return
        for field in ('m_Container','m_HashContainer'):
            info=value[field].items()[0]['second']
            require(info['preloadIndex'].data==0 and info['preloadSize'].data==old,'source preload range')
            info.set('preloadSize',old+len(additions))
        main=value['m_MainAsset']
        if main['preloadSize'].data:
            require(main['preloadIndex'].data==0 and main['preloadSize'].data==old,'main asset preload range')
            main.set('preloadSize',old+len(additions))
        value['m_PreloadTable'].set_items(value['m_PreloadTable'].items()+additions)
        file.write_tree(bundle_id,value)

    def textures(self,material):
        self.stage='texture-write'
        name=next(name for name,m in self.preflight['materials'].items() if m['sourceIdentity']==material['identity'])
        overrides=self.preflight['materials'][name]['authorOverrides']
        if not overrides:return
        require(all(k.startswith('texture.') for k in overrides),'material expects only known texture deltas')
        properties={t['property']:t['pointer'] for t in material['details']['textures']}
        ini=parse_ini(self.fixture/'existing-author-export')
        for key,section in overrides.items():
            prop=key.removeprefix('texture.')
            native_identity=properties[prop]['identity']
            resource=self.source.resources[native_identity]
            file=self.file_for(resource)
            tree=file.tree(resource['native']['pathId'])
            require(not tree['m_PlatformBlob'].items() and not tree['image data'].data,'texture platform/inline source unsupported')
            require(tree['m_ImageCount'].data==1 and tree['m_TextureDimension'].data==2,'requires Texture2D')
            spec=ini[section]
            # This experiment preserves the source shader/material unchanged. Keep its
            # texture interpretation; legacy INI declarations can disagree with it.
            color_space=tree['m_ColorSpace'].data
            require(color_space==resource['details']['colorSpace'] and color_space in (0,1),'source texture color space')
            require(spec['linear'] in ('true','false'),'author texture linear declaration')
            declared_color_space=0 if spec['linear']=='true' else 1
            png=safe_path(self.fixture/'existing-author-export',spec['path'])
            declaration=(sha(png.read_bytes()),tuple(sorted(spec.items())))
            if native_identity in self.written_textures:
                require(self.written_textures[native_identity]==declaration,'conflicting author replacements for a shared native texture')
                continue
            self.written_textures[native_identity]=declaration
            with Image.open(png) as image:
                image=image.convert('RGBA')
                width,height=image.size
                expected_base_sha=sha(image.tobytes())
                raw=bytearray()
                mip=image
                mip_count=0
                while True:
                    raw.extend(mip.transpose(Image.Transpose.FLIP_TOP_BOTTOM).tobytes())
                    mip_count+=1
                    if spec['mipmaps']!='true' or mip.size==(1,1):break
                    mip=mip.resize((max(1,mip.width//2),max(1,mip.height//2)),Image.Resampling.BOX)
            member=tree['m_StreamData']['path'].data.rsplit('/',1)[-1]
            require(member in self.streams and member==file.cab+'.resS','native texture stream identity')
            stream=self.streams[member]
            stream.extend(bytes((-len(stream))%16))
            offset=len(stream)
            stream.extend(raw)
            for field,val in dict(m_Width=width,m_Height=height,m_CompleteImageSize=len(raw),m_MipsStripped=0,m_TextureFormat=4,
                                  m_MipCount=mip_count,m_IsPreProcessed=0,m_StreamingMipmaps=0,m_ColorSpace=color_space).items():
                tree.set(field,val)
            tree['m_StreamData'].set('offset',offset)
            tree['m_StreamData'].set('size',len(raw))
            settings=tree['m_TextureSettings']
            for field,val in dict(m_FilterMode=int(spec['filter']),m_Aniso=int(spec['aniso']),m_MipBias=float(spec['mip_bias']),
                                  m_WrapU=int(spec['wrap']),m_WrapV=int(spec['wrap']),m_WrapW=int(spec['wrap'])).items():settings.set(field,val)
            file.write_tree(resource['native']['pathId'],tree)
            self.texture_evidence.append(dict(property=prop,section=section,identity=native_identity,authorPng=str(png),width=width,height=height,
                                              mipCount=mip_count,format='RGBA32',colorSpace=color_space,sourceColorSpace=color_space,
                                              authorDeclaredColorSpace=declared_color_space,colorSpaceDeclarationMismatch=declared_color_space!=color_space,
                                              colorSpacePolicy='preserve-native-interpretation-for-unchanged-source-material',
                                              rgbaTopDownSha256=expected_base_sha,streamOffset=offset,streamSize=len(raw)))

    def build(self):
        path='assets/beyond/arts/entity/actor/girl/lizhiyan/models/s_actor_lizhiyan_cloth_06_lod0.asset'
        asset='S_actor_lizhiyan_cloth_06_lod0'
        resource=self.source.resolve('Mesh',path,asset)
        rows=[r for r in self.capture['objects'] if (r['sourcePath'],r['sourceAsset'])==(path,asset) and not r['hidden']]
        require(len(rows)==2,'cloth06 must contain the two captured parts')
        uses=self.source.uses[resource['identity']]
        require(len(uses)==1,'cloth06 experiment requires one LOD0 source usage')
        use=uses[0]
        mesh_file=self.file_for(resource)
        renderer_file=self.files[use['native']['cab']]
        material_names={r['materials'][0]['material'] for r in rows}
        require(len(material_names)==1,'cloth06 fixture material identity')
        mat_meta=self.preflight['materials'][next(iter(material_names))]
        material=self.source.resources[mat_meta['sourceIdentity']]
        self.textures(material)
        self.stage='mesh-write'
        template=mesh_file.tree(resource['native']['pathId'])
        mesh_ids=[]
        root_matrices=[]
        for i,row in enumerate(rows):
            part=self.preflight['parts'][row['object']]
            author_path=safe_path(self.fixture,row['authorFile'])
            require(sha(author_path.read_bytes())==row['sha256'],'capture fixture mutated')
            author=read_mesh(author_path)
            path_id=resource['native']['pathId'] if i==0 else mesh_file.allocate('cloth06:'+part['partId'])
            mesh_tree,precision=native_mesh(template,author,asset if i==0 else asset+'__EFF_part_'+str(i))
            if i==0:mesh_file.write_tree(path_id,mesh_tree)
            else:mesh_file.add_tree(path_id,resource['native']['pathId'],mesh_tree)
            mesh_id=identity(mesh_file.cab,path_id)
            mesh_ids.append(mesh_id)
            palette=part['palette']
            root_slots=[p['slot'] for p in palette if p['transform']==use['rootBone']['identity']]
            require(len(root_slots)==1,'rootBone has no unique author bind frame')
            root_bind=np.asarray(author['bindposes'][root_slots[0]]).reshape(4,4,order='F')
            positions=np.asarray(author['vertices']).reshape(-1,3)
            root_positions=(np.column_stack((positions,np.ones(len(positions))))@root_bind.T)[:,:3]
            root_matrices.append(aabb(root_positions))
            self.parts.append(dict(object=row['object'],partId=part['partId'],mesh=mesh_id,authorFile=str(author_path),precision=precision,
                                   vertices=author['vertex_count'],indices=len(author['indices']),bindposes=len(author['bindposes']),
                                   boneTransforms=[p['transform'] for p in palette],bounds=root_matrices[-1],
                                   material=material['identity'],visibilityBindings=row['bindings'],shapeControls=row['shapeControls']))
        self.rebuild_preloads(mesh_file,[pointer(0,int(mid.rsplit(':',1)[1])) for mid in mesh_ids])
        self.stage='renderer-assembly-write'
        renderer_id=use['native']['pathId']
        owner_id=int(use['owner'].rsplit(':',1)[1])
        transform_id=int(use['transform'].rsplit(':',1)[1])
        original_renderer=renderer_file.tree(renderer_id)
        original_go=renderer_file.tree(owner_id)
        original_transform=renderer_file.tree(transform_id)
        child_go_id=renderer_file.allocate('cloth06:child-go:'+self.parts[1]['partId'])
        child_transform_id=renderer_file.allocate('cloth06:child-transform:'+self.parts[1]['partId'])
        child_renderer_id=renderer_file.allocate('cloth06:child-renderer:'+self.parts[1]['partId'])
        renderer_ids=[renderer_id,child_renderer_id]
        for i,part in enumerate(self.parts):
            renderer=copy.deepcopy(original_renderer)
            renderer.set('m_GameObject',pointer(0,owner_id if i==0 else child_go_id))
            renderer.set('m_Mesh',self.pp(renderer_file,part['mesh']))
            renderer['m_Materials'].set_items([self.pp(renderer_file,material['identity'])])
            renderer['m_Bones'].set_items([self.pp(renderer_file,bone) for bone in part['boneTransforms']])
            renderer['m_BlendShapeWeights'].set_items([])
            renderer.set('m_AABB',part['bounds'])
            renderer.set('m_DirtyAABB',0)
            if i==0:renderer_file.write_tree(renderer_id,renderer)
            else:renderer_file.add_tree(child_renderer_id,renderer_id,renderer)
            part['renderer']=identity(renderer_file.cab,renderer_ids[i])
        child_go=copy.deepcopy(original_go)
        child_go.set('m_Name',original_go['m_Name'].data+'__EFF_part_1')
        child_go['m_Component'].set_items([dict(component=pointer(0,child_transform_id)),dict(component=pointer(0,child_renderer_id))])
        renderer_file.add_tree(child_go_id,owner_id,child_go)
        child_transform=copy.deepcopy(original_transform)
        child_transform.set('m_GameObject',pointer(0,child_go_id))
        child_transform.set('m_LocalRotation',dict(x=0.,y=0.,z=0.,w=1.))
        child_transform.set('m_LocalPosition',dict(x=0.,y=0.,z=0.))
        child_transform.set('m_LocalScale',dict(x=1.,y=1.,z=1.))
        child_transform['m_Children'].set_items([])
        child_transform.set('m_Father',pointer(0,transform_id))
        renderer_file.add_tree(child_transform_id,transform_id,child_transform)
        original_transform['m_Children'].set_items(original_transform['m_Children'].items()+[pointer(0,child_transform_id)])
        renderer_file.write_tree(transform_id,original_transform)
        self.stage='lod-and-preload-write'
        lod_hits=[]
        for object_id in list(renderer_file.order):
            if renderer_file.objects[object_id]['classId']!=205:continue
            tree=renderer_file.tree(object_id)
            changed=False
            for level,lod in enumerate(tree['m_LODs'].items()):
                values=lod['renderers'].items()
                matches=[v for v in values if v['renderer']['m_FileID'].data==0 and v['renderer']['m_PathID'].data==renderer_id]
                if matches:
                    require(len(matches)==1,'duplicate LOD source Renderer')
                    extra=copy.deepcopy(matches[0]);extra.set('renderer',pointer(0,child_renderer_id))
                    lod['renderers'].set_items(values+[extra])
                    lod_hits.append(dict(identity=identity(renderer_file.cab,object_id),level=level,sourceRenderer=identity(renderer_file.cab,renderer_id),addedRenderer=identity(renderer_file.cab,child_renderer_id)))
                    changed=True
            if changed:renderer_file.write_tree(object_id,tree)
        require(lod_hits,'source Renderer is not in a discovered LODGroup')
        self.rebuild_preloads(renderer_file,[pointer(0,child_go_id),pointer(0,child_transform_id),pointer(0,child_renderer_id),self.pp(renderer_file,mesh_ids[1])])
        self.stage='output-write'
        outputs=[]
        changed=[]
        for cab,file in self.files.items():
            for path_id in sorted(file.changed):
                row=file.objects[path_id]
                fields=[]
                if path_id not in file.added:
                    before=SerializedFile(file.original,file.manifest,self.schemas[cab]).tree(path_id)
                    after=file.tree(path_id)
                    fields=[k for k in before.data if encode(before[k])!=encode(after[k])]
                changed.append(dict(identity=identity(cab,path_id),classId=row['classId'],added=path_id in file.added,changedFields=fields,payloadSha256=sha(row['payload'])))
            data=file.build()
            members=[(cab,data)]
            if cab+'.resS' in self.streams:members.append((cab+'.resS',bytes(self.streams[cab+'.resS'])))
            bundle=unityfs(members,file.manifest['unityVersion'])
            relative='bundles/'+file.manifest['sourceBundle']
            dest=safe_path(self.output,relative)
            dest.parent.mkdir(parents=True,exist_ok=True)
            dest.write_bytes(bundle)
            outputs.append(dict(file=relative,sourceBundle=file.manifest['sourceBundle'],cab=cab,sha256=sha(bundle),size=len(bundle),
                                objects=len(file.order),changed=file.changed!=set()))
        report=dict(format='EFF_NATIVE_WRITE_EXPERIMENT',version=1,builtOffline=True,nativeLoadVerified=False,gameReady=False,
                    sourcePackage=str(self.package),capture=str(self.capture_file),vfsFingerprint=self.capture['vfsFingerprint'],
                    scope='cloth_06 LOD0 geometry/assembly in the captured source PFB; shared source cloth_02 texture payloads overridden',bundles=outputs,parts=self.parts,textures=self.texture_evidence,
                    originalSourceMesh=resource['identity'],sourceOwner=use['owner'],sourceTransform=use['transform'],
                    sourceRootBone=use['rootBone']['identity'],sourceSkinningRoot=use['skinningRoot']['identity'],
                    childOwner=identity(renderer_file.cab,child_go_id),childTransform=identity(renderer_file.cab,child_transform_id),
                    lodUpdates=lod_hits,changedObjects=changed,
                    preservation='Unmodified native object bytes, type/external metadata, and original stream prefixes must remain identical',
                    nativeMemoryCopy=False,runtimeMeshSetter=False,rawStreamBytes=sum(len(v) for v in self.streams.values()),
                    textureMips='RGBA32; exact base pixels, source texture color space, BOX-generated offline mips; compression and mip filtering optimization deferred',
                    unresolved=['Actual game bundle registration and resource loading','New child Renderer discovery by game VFX and custom controls',
                                'Only native LODGroup and AssetBundle preload references updated; custom/managed reference consumers need game validation',
                                'F10 native resource reinstantiation and control binding','UI/NPC/other LOD consumers not modified in this experiment'])
        write_json(self.output/'build-manifest.json',report)
        return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture',type=Path,required=True)
    parser.add_argument('--preflight',type=Path,required=True)
    parser.add_argument('--schema',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    output=args.output.resolve()
    require(not output.exists(),'refusing to overwrite existing experiment output')
    output.mkdir(parents=True)
    started=time.perf_counter()
    experiment=None
    try:
        experiment=Experiment(args.capture.resolve(),args.preflight.resolve(),args.schema.resolve(),output)
        report=experiment.build()
        report['buildSeconds']=time.perf_counter()-started
        write_json(output/'build-manifest.json',report)
        print(json.dumps(dict(builtOffline=True,nativeLoadVerified=False,parts=len(report['parts']),textures=len(report['textures']),
                             bundles=len(report['bundles']),changedObjects=len(report['changedObjects']),seconds=report['buildSeconds'],output=str(output)),ensure_ascii=False))
        return 0
    except Exception as error:
        report=dict(builtOffline=False,nativeLoadVerified=False,stage=experiment.stage if experiment else 'source-preparation',
                    error=type(error).__name__+': '+str(error),traceback=traceback.format_exc(),seconds=time.perf_counter()-started)
        write_json(output/'failure.json',report)
        print(json.dumps(report,ensure_ascii=False))
        return 1


if __name__=='__main__':raise SystemExit(main())
