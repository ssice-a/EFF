"""Validate all captured selected author parts; publish only incremental fields/rules.

Complete baseline files are written only to the separate offline validation
output. The deployable delta directory never contains an original PFB/bundle.
"""
from __future__ import annotations
import argparse
import copy
import json
from pathlib import Path
import sys
import time
import traceback
import numpy as np

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from nativepack.cloth06_experiment import Experiment,identity,pointer,sha,write_json
from nativepack.mesh import native_mesh,renderer_bounds
from nativepack.serialized_file import SerializedFile,unityfs
from nativepack.type_tree import encode,require
from nativepack.delta_apply import GEOMETRY_FIELDS
from nativepack.ini_package import export_directory
from eiem_format import read_mesh,safe_path


class SelectedExperiment(Experiment):
    def __init__(self,*args,mod_id):
        super().__init__(*args)
        self.mod_id=mod_id
        self.children=[];self.hidden_renderers=[];self.lod_updates=[]
        self.preload_expansions={};self.renderer_expansions={};self.delta_parts=[]

    def allocate(self,file,label):
        return file.allocate(self.mod_id+':'+label)

    def expand_preloads(self,file,expansions):
        bundles=[r for r in file.objects.values() if r['classId']==142]
        require(len(bundles)==1,'one native AssetBundle required')
        bid=bundles[0]['pathId'];tree=file.tree(bid)
        old=tree['m_PreloadTable'].items();new=[];offsets=[0]
        for p in old:
            new.append(p)
            new.extend(expansions.get((p['m_FileID'].data,p['m_PathID'].data),[]))
            offsets.append(len(new))
        if len(new)==len(old):return
        for field in ('m_Container','m_HashContainer'):
            for pair in tree[field].items():
                info=pair['second'];start=info['preloadIndex'].data;size=info['preloadSize'].data
                require(0<=start<=start+size<=len(old),'source preload range')
                info.set('preloadIndex',offsets[start]);info.set('preloadSize',offsets[start+size]-offsets[start])
        info=tree['m_MainAsset'];start=info['preloadIndex'].data;size=info['preloadSize'].data
        require(0<=start<=start+size<=len(old),'main asset preload range')
        info.set('preloadIndex',offsets[start]);info.set('preloadSize',offsets[start+size]-offsets[start])
        tree['m_PreloadTable'].set_items(new);file.write_tree(bid,tree)

    def build(self):
        require(len(self.capture['objects'])==14 and len(self.preflight['parts'])==13,'approved selection has changed')
        row_by_part={self.preflight['parts'][r['object']]['partId']:r for r in self.capture['objects'] if not r['hidden']}
        author_by_part={pid:read_mesh(safe_path(self.fixture,row['authorFile'])) for pid,row in row_by_part.items()}
        for name,spec in self.preflight['materials'].items():
            self.textures(self.source.resources[spec['sourceIdentity']])
        switches={s['variable']:s['default'] for s in self.capture['switches']}
        shape_defaults=lambda row:[next((c['default']*100. for c in row['shapeControls'] if c['shape']==name and c['enabled']),0.) for name,*_ in author_by_part[self.preflight['parts'][row['object']]['partId']]['blend_channels']]
        for rule in self.preflight['rules']:
            self.stage='selected-rule:'+rule['authorSourceAsset']
            for target in rule['targets']:
                resource=self.source.resolve('Mesh',target['sourcePath'],target['sourceAsset'])
                file=self.file_for(resource);source_id=resource['native']['pathId'];template=file.tree(source_id)
                mesh_ids=[]
                for i,pid in enumerate(rule['parts']):
                    row=row_by_part[pid];author=author_by_part[pid]
                    mid=source_id if i==0 else self.allocate(file,'mesh:'+target['sourceAsset']+':'+pid)
                    name=target['sourceAsset'] if i==0 else target['sourceAsset']+'__EFF_'+pid.split(':')[-1]
                    tree,precision=native_mesh(template,author,name)
                    if i==0:file.write_tree(mid,tree)
                    else:file.add_tree(mid,source_id,tree)
                    mesh_ids.append(identity(file.cab,mid))
                    if target['level']==0:
                        before=SerializedFile(file.original,file.manifest,self.schemas[file.cab]).tree(source_id)
                        # Geometry must explicitly override every dependent field, even when
                        # this LOD0 template happens to have the same default. Other LODs
                        # can have streaming/index/compression state with different values.
                        changed=list(GEOMETRY_FIELDS)
                        require(all(k in tree.data for k in changed),'required native geometry field absent')
                        self.delta_parts.append(dict(partId=pid,object=row['object'],authorFile=row['authorFile'],sourcePath=rule['authorSourcePath'],sourceAsset=rule['authorSourceAsset'],
                            meshIdentity=mesh_ids[-1],material=row['materials'][0]['material'],vertices=author['vertex_count'],indices=len(author['indices']),
                            bonePaths=author['bone_paths'],visibilityBindings=row['bindings'],shapeControls=row['shapeControls'],
                            shapeNames=[ch[0] for ch in author['blend_channels']],precision=precision,nativeTypeHash=file.manifest['types'][file.objects[mid]['typeIndex']]['oldTypeHash'],
                            fields=changed))
                if len(mesh_ids)>1:
                    self.preload_expansions.setdefault(file.cab,{})[(0,source_id)]=[pointer(0,int(mid.rsplit(':',1)[1])) for mid in mesh_ids[1:]]
                for plan in target['uses']:
                    use=next(u for u in self.source.manifest['rendererUses'] if u['identity']==plan['renderer'])
                    rfile=self.files[use['native']['cab']];rid=use['native']['pathId'];rt=rfile.tree(rid)
                    if rule['hidden']:
                        rt.set('m_Enabled',False);rfile.write_tree(rid,rt)
                        self.hidden_renderers.append(dict(renderer=use['identity'],sourceMesh=resource['identity']))
                        self.renderer_expansions[use['identity']]=[]
                        continue
                    owner=int(use['owner'].rsplit(':',1)[1]);transform=int(use['transform'].rsplit(':',1)[1])
                    parent=rfile.tree(transform);go=rfile.tree(owner);extra=[]
                    for i,(pid,mesh_id) in enumerate(zip(rule['parts'],mesh_ids)):
                        row=row_by_part[pid];author=author_by_part[pid];part=self.preflight['parts'][row['object']]
                        generated=next(g for g in plan['generatedRenderers'] if g['part']==pid)
                        bones=generated['bones'];root=use['rootBone']['identity'];root_slots=[j for j,b in enumerate(bones) if b==root]
                        require(len(root_slots)==1,'rootBone must have a unique author bindpose')
                        bounds=renderer_bounds(author,np.asarray(author['bindposes'][root_slots[0]]).reshape(4,4,order='F'))
                        material=self.preflight['materials'][generated['material']]['sourceIdentity']
                        enabled=all(switches[b['variable']] in b['visible'] for b in row['bindings'])
                        new_rid=rid if i==0 else self.allocate(rfile,'renderer:'+str(rid)+':'+pid)
                        if i:
                            gid=self.allocate(rfile,'go:'+str(rid)+':'+pid);tid=self.allocate(rfile,'transform:'+str(rid)+':'+pid)
                            childgo=copy.deepcopy(go);childgo.set('m_Name',go['m_Name'].data+'__EFF_'+pid.split(':')[-1]);childgo['m_Component'].set_items([dict(component=pointer(0,tid)),dict(component=pointer(0,new_rid))])
                            childtransform=copy.deepcopy(parent);childtransform.set('m_GameObject',pointer(0,gid));childtransform.set('m_Father',pointer(0,transform));childtransform['m_Children'].set_items([])
                            childtransform.set('m_LocalPosition',dict(x=0.,y=0.,z=0.));childtransform.set('m_LocalRotation',dict(x=0.,y=0.,z=0.,w=1.));childtransform.set('m_LocalScale',dict(x=1.,y=1.,z=1.))
                            if 'm_RootOrder' in childtransform.data:childtransform.set('m_RootOrder',len(parent['m_Children'].items()))
                            parent['m_Children'].set_items(parent['m_Children'].items()+[pointer(0,tid)])
                            rfile.add_tree(gid,owner,childgo);rfile.add_tree(tid,transform,childtransform)
                            extra.extend([pointer(0,gid),pointer(0,tid),pointer(0,new_rid),self.pp(rfile,mesh_id)])
                            self.children.append(dict(owner=identity(rfile.cab,gid),transform=identity(rfile.cab,tid),parent=identity(rfile.cab,transform),renderer=identity(rfile.cab,new_rid)))
                        else:gid=owner
                        renderer=copy.deepcopy(rt);renderer.set('m_GameObject',pointer(0,gid));renderer.set('m_Mesh',self.pp(rfile,mesh_id));renderer['m_Materials'].set_items([self.pp(rfile,material)])
                        renderer['m_Bones'].set_items([self.pp(rfile,b) for b in bones]);renderer['m_BlendShapeWeights'].set_items(shape_defaults(row));renderer.set('m_AABB',bounds);renderer.set('m_DirtyAABB',False);renderer.set('m_Enabled',enabled)
                        if i:rfile.add_tree(new_rid,rid,renderer)
                        else:rfile.write_tree(rid,renderer)
                        self.parts.append(dict(partId=pid,object=row['object'],authorFile=str(safe_path(self.fixture,row['authorFile'])),mesh=mesh_id,renderer=identity(rfile.cab,new_rid),
                            material=material,boneTransforms=bones,sourceRootBone=root,sourceSkinningRoot=use['skinningRoot']['identity'],bounds=bounds,
                            enabled=enabled,shapeWeights=shape_defaults(row),level=target['level']))
                    if extra:
                        rfile.write_tree(transform,parent)
                        self.preload_expansions.setdefault(rfile.cab,{})[(0,rid)]=extra
                    self.renderer_expansions[use['identity']]=[p['renderer'] for p in self.parts if p['level']==target['level'] and p['mesh'] in mesh_ids and p['renderer'].rsplit(':',1)[0]==rfile.cab.lower() and (p['renderer']==use['identity'] or any(ch['renderer']==p['renderer'] and ch['parent']==use['transform'] for ch in self.children))]
        self.stage='selected-LOD-preload-expansion'
        for file in self.files.values():
            for oid in list(file.order):
                if file.objects[oid]['classId']!=205:continue
                tree=file.tree(oid);changed=False
                for level,lod in enumerate(tree['m_LODs'].items()):
                    values=[]
                    for item in lod['renderers'].items():
                        key=identity(file.cab,item['renderer']['m_PathID'].data)
                        replacements=self.renderer_expansions.get(key)
                        if item['renderer']['m_FileID'].data==0 and replacements is not None:
                            for rid in replacements:
                                entry=copy.deepcopy(item);entry.set('renderer',pointer(0,int(rid.rsplit(':',1)[1])));values.append(entry)
                                if rid!=key:self.lod_updates.append(dict(identity=identity(file.cab,oid),level=level,sourceRenderer=key,addedRenderer=rid))
                            changed=changed or replacements!=[key]
                        else:values.append(item)
                    lod['renderers'].set_items(values)
                if changed:file.write_tree(oid,tree)
            if file.cab in self.preload_expansions:self.expand_preloads(file,self.preload_expansions[file.cab])
        self.stage='offline-reference-output'
        outputs=[];changes=[]
        for cab,file in self.files.items():
            for path_id in sorted(file.changed):
                obj=file.objects[path_id];fields=[]
                if path_id not in file.added:
                    before=SerializedFile(file.original,file.manifest,self.schemas[cab]).tree(path_id);after=file.tree(path_id)
                    fields=[k for k in before.data if encode(before[k])!=encode(after[k])]
                changes.append(dict(identity=identity(cab,path_id),classId=obj['classId'],added=path_id in file.added,changedFields=fields,payloadSha256=sha(obj['payload'])))
            members=[(cab,file.build())]
            if cab+'.resS' in self.streams:members.append((cab+'.resS',bytes(self.streams[cab+'.resS'])))
            data=unityfs(members,file.manifest['unityVersion']);relative='bundles/'+file.manifest['sourceBundle'];dest=safe_path(self.output,relative);dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data)
            outputs.append(dict(file=relative,sourceBundle=file.manifest['sourceBundle'],cab=cab,size=len(data),sha256=sha(data),objects=len(file.order),changed=bool(file.changed)))
        report=dict(format='EFF_NATIVE_WRITE_EXPERIMENT',version=1,experimentKind='selected-author',builtOffline=True,nativeLoadVerified=False,gameReady=False,
            modId=self.mod_id,sourcePackage=str(self.package),capture=str(self.capture_file),vfsFingerprint=self.capture['vfsFingerprint'],selectionObjects=14,authorParts=13,
            scope='selected body/cloth rules and their captured source PFB consumers; reference closure is validation-only',bundles=outputs,parts=self.parts,textures=self.texture_evidence,
            children=self.children,hiddenRenderers=self.hidden_renderers,lodUpdates=self.lod_updates,changedObjects=changes)
        write_json(self.output/'build-manifest.json',report)
        return report

    def delta(self,destination):
        require(not destination.exists(),'refusing to overwrite Mod directory');destination.mkdir(parents=True)
        return self.write_delta(destination)

    def write_delta(self,destination):
        self.stage="INI-directory-publication"
        records=[]
        for part in self.delta_parts:
            cab,mid=part['meshIdentity'].rsplit(':',1)
            file=next(f for f in self.files.values() if f.cab.lower()==cab);tree=file.tree(int(mid))
            record={k:part[k] for k in ('partId','object','material','vertices','indices','bonePaths',
                                      'visibilityBindings','shapeControls','shapeNames','nativeTypeHash')}
            record['fieldData']={name:encode(tree[name]) for name in part['fields']}
            records.append(record)
        textures=[]
        for index,evidence in enumerate(self.texture_evidence):
            native=self.source.resources[evidence['identity']];file=self.file_for(native);tree=file.tree(native['native']['pathId'])
            member=tree['m_StreamData']['path'].data.rsplit('/',1)[-1];offset=evidence['streamOffset']
            pixels=bytes(self.streams[member][offset:offset+evidence['streamSize']])
            settings=tree['m_TextureSettings']
            require(settings['m_WrapU'].data==settings['m_WrapV'].data==settings['m_WrapW'].data,
                    'export requires uniform texture wrap modes')
            textures.append(dict(id=f'texture:{index}',sourcePath=native['logicalPath'],sourceAsset=native['name'],
                pixels=pixels,pixelLabel=Path(evidence['authorPng']).stem,format='RGBA32',
                width=evidence['width'],height=evidence['height'],mipCount=evidence['mipCount'],colorSpace=evidence['colorSpace'],
                filter=settings['m_FilterMode'].data,wrap=settings['m_WrapU'].data,aniso=settings['m_Aniso'].data,
                mipBias=settings['m_MipBias'].data,sourceIdentity=evidence['identity']))
        materials=[]
        for name,mat in self.preflight['materials'].items():
            source=self.source.resources[mat['sourceIdentity']];file=self.file_for(source)
            properties={t['property']:t['pointer']['identity'] for t in source['details']['textures']}
            require(all(k.startswith('texture.') for k in mat['authorOverrides']),'unencoded material scalar edit: '+name)
            edits=[dict(property=k.removeprefix('texture.'),texture=next(t['id'] for t in textures if t['sourceIdentity']==properties[k.removeprefix('texture.')]))
                   for k in mat['authorOverrides']]
            materials.append(dict(id=name,sourcePath=source['logicalPath'],sourceAsset=source['name'],textureEdits=edits,
                nativeTypeHash=file.manifest['types'][file.objects[source['native']['pathId']]['typeIndex']]['oldTypeHash'],fieldData={}))
        rules=[dict(sourcePath=r['authorSourcePath'],sourceAsset=r['authorSourceAsset'],hidden=r['hidden'],parts=r['parts'],
                    targets=[dict(level=t['level'],sourcePath=t['sourcePath'],sourceAsset=t['sourceAsset']) for t in r['targets']])
               for r in self.preflight['rules']]
        return export_directory(destination,mod_id=self.mod_id,parts=records,materials=materials,
                                textures=textures,rules=rules,switches=self.capture['switches'])


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('capture','preflight','schema','output','delta-output'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--mod-id',required=True);args=parser.parse_args();out=args.output.resolve();require(not out.exists(),'validation output already exists');out.mkdir(parents=True)
    experiment=None;started=time.perf_counter()
    try:
        experiment=SelectedExperiment(args.capture.resolve(),args.preflight.resolve(),args.schema.resolve(),out,mod_id=args.mod_id)
        report=experiment.build();package=experiment.delta(args.delta_output.resolve());report['buildSeconds']=time.perf_counter()-started;write_json(out/'build-manifest.json',report)
        print(json.dumps(dict(builtOffline=True,nativeLoadVerified=False,selected=14,parts=13,renderers=len(report['parts']),textures=len(package['textures']),delta=str(args.delta_output),seconds=report['buildSeconds'])))
        return 0
    except Exception as error:
        evidence=dict(builtOffline=False,nativeLoadVerified=False,stage=experiment.stage if experiment else 'source-preparation',error=str(error),traceback=traceback.format_exc());write_json(out/'failure.json',evidence);print(json.dumps(evidence));return 1


if __name__=='__main__':raise SystemExit(main())
