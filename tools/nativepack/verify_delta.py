"""Reconstruct format-2 INI resources against an independently verified reference.

Consumes only the published Mod and current source templates for reconstruction;
reference snapshots are consulted separately to verify the result.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import sys
import traceback
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'diagnostics'))
from nativepack.serialized_file import SerializedFile
from nativepack.type_tree import encode,require
from nativepack.delta_apply import apply_mesh_fields
from nativepack.texture_fields import apply_texture_fields,TEXTURE_FIELDS
from nativepack.ini_reader import read_package
from nativepack.payload import read_payload
from nativepack.resource_file import digest
from validate_selected_mod_author import SourceCatalog
from eiem_format import safe_path,read_mesh


def verify(package_path,native_report,schema_path):
    package=read_package(package_path)
    verified=json.loads(native_report.read_text(encoding='utf-8'))
    require(verified['verified'] and verified['offlineOnly'] and not verified['nativeLoadVerified'],'native reference verification required')
    build=json.loads((Path(verified['experiment'])/'build-manifest.json').read_text(encoding='utf-8'))
    require(len(package['parts'])==13 and sum(r['hidden'] for r in package['rules'])==1,'approved selected/hidden coverage')
    source=SourceCatalog(Path(build['sourcePackage']))
    schema=json.loads(schema_path.read_text(encoding='utf-8'));schemas={f['cab']:f for f in schema['files']}
    files={f['cab']:SerializedFile(safe_path(source.package,f['snapshot']['file']).read_bytes(),f,schemas[f['cab']]) for f in source.manifest['serializedFiles']}
    field_count=0;reconstructed=[]
    by_part={p['partId']:p for p in package['parts']}
    by_material={m['id']:m for m in package['materials']}
    author_by_object={}
    for part in build['parts']:
        if part['object'] not in author_by_object:author_by_object[part['object']]=read_mesh(Path(part['authorFile']))
    for rule in package['rules']:
        if rule['hidden']:
            require(not rule['parts'],'hidden rule contains geometry')
            for target in rule['targets']:source.resolve('Mesh',target['sourcePath'],target['sourceAsset'])
            continue
        for target in rule['targets']:
            resource=source.resolve('Mesh',target['sourcePath'],target['sourceAsset']);file=files[resource['native']['cab']]
            template=file.tree(resource['native']['pathId'])
            source_type=file.manifest['types'][file.objects[resource['native']['pathId']]['typeIndex']]['oldTypeHash']
            for pid in rule['parts']:
                part=by_part[pid];require(part['nativeTypeHash']==source_type,'target native type hash incompatibility')
                tree=apply_mesh_fields(template,part,lambda row:read_payload(package,row));field_count+=len(part['nativeFields'])
                expected=next(p for p in build['parts'] if p['object']==part['object'] and p['level']==target['level'])
                change=next(c for c in build['changedObjects'] if c['identity']==expected['mesh'])
                # Object naming is not geometry. Match the independently built
                # reference name while checking every native geometry byte.
                suffix=expected['partId'].split(':')[-1]
                name=target['sourceAsset'] if pid==rule['parts'][0] else target['sourceAsset']+'__EFF_'+suffix
                tree.set('m_Name',name)
                require(digest(encode(tree))==change['payloadSha256'],'INI mesh reconstruction differs: '+part['object']+' / LOD '+str(target['level']))
                require(part['initialVisible']==expected['enabled'],'initial visibility differs from native reference')
                author=author_by_object[part['object']]
                require(part['bonePaths']==author['bone_paths'] and part['vertices']==author['vertex_count'] and
                        part['indices']==len(author['indices']) and part['shapeNames']==[c[0] for c in author['blend_channels']],
                        'mesh binding/geometry directory differs from author')
                material=by_material[part['material']]
                require(source.resolve('Material',material['sourcePath'],material['sourceAsset'])['identity']==expected['material'],
                        'part material binding differs from native reference')
                weights=[package['states'][part['shapeBindings'][shape]]*100 if shape in part['shapeBindings'] else 0 for shape in part['shapeNames']]
                require(weights==expected['shapeWeights'],'initial shape weights differ from native reference')
                reconstructed.append(dict(partId=pid,sourcePath=target['sourcePath'],sourceAsset=target['sourceAsset']))
    for texture in package['textures']:
        data=read_payload(package,texture)
        resource=source.resolve('Texture2D',texture['sourcePath'],texture['sourceAsset']);file=files[resource['native']['cab']];tree=file.tree(resource['native']['pathId'])
        tree=apply_texture_fields(tree,texture,len(data));field_count+=len(TEXTURE_FIELDS)
        expected=next(t for t in build['textures'] if t['identity']==resource['identity'])
        require(texture['width']==expected['width'] and texture['height']==expected['height'] and texture['mipCount']==expected['mipCount'] and
                texture['colorSpace']==expected['colorSpace'] and len(data)==expected['streamSize'],'texture metadata correspondence')
        from PIL import Image
        with Image.open(expected['authorPng']) as image:
            raw=image.convert('RGBA').transpose(Image.Transpose.FLIP_TOP_BOTTOM).tobytes()
            require(data[:len(raw)]==raw,'INI texture base pixels differ')
        tree['m_StreamData'].set('offset',expected['streamOffset']);tree['m_StreamData'].set('size',expected['streamSize'])
        change=next(c for c in build['changedObjects'] if c['identity']==resource['identity'])
        require(digest(encode(tree))==change['payloadSha256'],'INI texture field reconstruction differs')
    texture_by_id={t['id']:t for t in package['textures']}
    for material in package['materials']:
        native=source.resolve('Material',material['sourcePath'],material['sourceAsset']);file=files[native['native']['cab']]
        require(material['nativeTypeHash']==file.manifest['types'][file.objects[native['native']['pathId']]['typeIndex']]['oldTypeHash'],
                'material native type hash incompatibility')
        require(not material['nativeFields'],'this reference has texture edits only')
        native_textures={t['property']:t['pointer']['identity'] for t in native['details']['textures']}
        for edit in material['textureEdits']:
            texture=texture_by_id[edit['texture']]
            require(edit['property'] in native_textures and
                    source.resolve('Texture2D',texture['sourcePath'],texture['sourceAsset'])['identity']==native_textures[edit['property']],
                    'material texture property binding differs from reference')
    ini=package_path/'mod.ini'
    return dict(format='EFF_NATIVE_MOD_DIRECTORY_VERIFICATION',version=2,verified=True,offlineOnly=True,nativeLoadVerified=False,gameReady=False,
        package=str(package_path.resolve()),selectedObjects=14,geometryParts=len(package['parts']),hiddenSourceGroups=1,
        targetMeshReconstructions=len(reconstructed),textureTargets=len(package['textures']),
        uniqueTexturePayloads=len({t['file'] for t in package['textures']}),textureDeclarations='INI',runtimeImageDecoding=False,boneSlots=sum(len(p['bonePaths']) for p in package['parts']),
        sourceObjectsShipped=0,wholePrefabsShipped=0,wholeBundlesShipped=0,payloadFiles=len(package['_files']),
        payloadBytes=sum((package_path/name).stat().st_size for name in package['_files']),iniBytes=ini.stat().st_size,
        iniLines=len(ini.read_text(encoding='utf-8').splitlines()),stateVariables=len(package['states']),keyBindings=len(package['keyBindings']),
        fieldApplications=field_count,nativeReference=str(native_report.resolve()),meshes=reconstructed)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('package','native-report','schema','output'):parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();require(not args.output.exists(),'evidence already exists');args.output.parent.mkdir(parents=True,exist_ok=True)
    try:result=verify(args.package,args.native_report,args.schema)
    except Exception as error:result=dict(verified=False,nativeLoadVerified=False,stage='INI-directory-reconstruction',error=str(error),traceback=traceback.format_exc())
    args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:result[k] for k in ('verified','nativeLoadVerified','error','payloadBytes','targetMeshReconstructions','uniqueTexturePayloads','iniLines') if k in result}));return 0 if result['verified'] else 1


if __name__=='__main__':raise SystemExit(main())
