"""Verify authored material texture choices against native candidate descriptors.

This prepares read-only native loading expectations. It cannot prove native
shared Bundle lifetime, GPU pixels or current-instance hot reload.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from nativepack.ini_reader import read_package


def require(ok, message):
    if not ok:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest().upper()


def key(pointer):
    return pointer['member'], pointer['pathId']


def allocated_texture_id(cab, package, texture):
    label = ':'.join((cab, package['modId'], 'texture', texture['sourcePath'],
                      texture['sourceAsset'], texture['id']))
    return str(int.from_bytes(hashlib.sha256(label.encode()).digest()[:8], 'little', signed=True))


def verify(descriptors, package, graph, generation, shared_plan):
    require(descriptors['format'] == 'EFF_MATERIAL_TEXTURE_CANDIDATE_DESCRIPTORS' and
            descriptors['namespaceOnlyPayloadVerified'] and not descriptors['nativeSubmission'],
            'descriptor scope differs')
    require(descriptors['generationKey'] == generation['generationKey'], 'generation key differs')
    files = {r['file']: r for r in descriptors['inputs']}
    expected_files = {r['file']: r for r in generation['files']}
    for name, row in files.items():
        require(row['originalSha256'] == expected_files[name]['sourceSha256'] and
                row['candidateSha256'] == expected_files[name]['candidateSha256'],
                'descriptor bytes differ from verified generation: ' + name)
    all_objects = {key(o): o for r in files.values() for o in r['sourceObjects']}
    pristine = {key(o): o for r in files.values() for o in r['pristineObjects']}
    require(len(all_objects) == sum(len(r['sourceObjects']) for r in files.values()),
            'duplicate descriptor native object identity')
    textures = {}
    author_rows = []
    for texture in package['textures']:
        originals = [o for o in pristine.values() if o['classId'] == 28 and
                     texture['sourcePath'] in o['containerPaths'] and o['name'] == texture['sourceAsset']]
        require(len(originals) == 1, 'ambiguous or absent original texture: ' + texture['id'])
        original = originals[0]
        expected_key = (original['member'], allocated_texture_id(original['member'], package, texture))
        actual = all_objects.get(expected_key)
        require(actual and actual['classId'] == 28 and expected_key != key(original),
                'author texture clone not resolved: ' + texture['id'])
        require(all_objects[key(original)] == original, 'original texture was modified: ' + texture['id'])
        for field, declared in [('m_Width', texture['width']), ('m_Height', texture['height']),
                                ('m_TextureFormat', 4), ('m_MipCount', texture['mipCount']),
                                ('m_ColorSpace', texture['colorSpace'])]:
            require(actual[field] == declared, 'custom texture descriptor differs: ' + texture['id'] + '/' + field)
        require(actual['stream']['sha256'] == texture['sha256'] and
                actual['stream']['bytes'] == texture['size'], 'author pixel stream differs: ' + texture['id'])
        textures[texture['id']] = actual
        author_rows.append(dict(id=texture['id'], originalObject=key(original),
            customObject=key(actual), width=texture['width'], height=texture['height'],
            mipCount=texture['mipCount'], originalPreserved=True,
            authorPixelsSha256=texture['sha256'], candidateStreamPixelsMatched=True,
            nativeReadable=bool(actual['m_IsReadable']),
            validationScope='serialized pixel stream bytes; no native/GPU readback'))
    materials = []
    shared = {}
    selected_files = set(files)
    for material in package['materials']:
        matches = [o for o in all_objects.values() if o['classId'] == 21 and
                   material['sourcePath'] in o['containerPaths'] and o['name'] == material['sourceAsset']]
        require(len(matches) == 1, 'material source not unique: ' + material['id'])
        actual = matches[0]
        source = pristine[key(actual)]
        require(actual['shader'] == source['shader'], 'material shader identity changed')
        edits = {e['property']: e['texture'] for e in material['textureEdits']}
        props = {p['property']: p['texture'] for p in actual['textureProperties']}
        source_props = {p['property']: p['texture'] for p in source['textureProperties']}
        require(set(props) == set(source_props), 'material property names changed')
        rows = []
        for prop, ptr in props.items():
            if prop in edits:
                require(key(ptr) == key(textures[edits[prop]]), 'material custom texture mismatch: ' + material['id'] + '/' + prop)
                policy = 'author-custom-texture'
            else:
                require(ptr == source_props[prop], 'undeclared original texture reference changed: ' + material['id'] + '/' + prop)
                policy = 'original-texture' if ptr['pathId'] != '0' else 'original-null'
            rows.append(dict(property=prop, policy=policy, texture=ptr))
        for prop, ptr in [('shader', actual['shader'])] + list(props.items()):
            if ptr['pathId'] != '0' and key(ptr) not in all_objects:
                shared.setdefault(ptr['member'], []).append(dict(material=material['id'], property=prop, pathId=ptr['pathId']))
        raw_equal = actual['objectSha256'] == source['objectSha256']
        if not material['nativeFields'] and not edits:
            require(raw_equal and actual == source, 'unchanged original material payload differs')
        materials.append(dict(id=material['id'], object=key(actual), shader=actual['shader'],
            textureChoices=rows, wholeOriginalMaterialPreserved=raw_equal,
            customTextureBindings=len(edits), originalTextureBindings=sum(x['policy']=='original-texture' for x in rows)))
    # Include all serialized dependencies/preloads, not only visible shader properties.
    by_file = {r['input']: r for r in graph['serialized']}
    boundary = set()
    for name in selected_files:
        require(not by_file[name]['unparsedObjects'], 'opaque material/texture input')
        for ref in by_file[name]['referenceTargets']:
            require(not ref['broken'], 'broken material/texture preload reference')
            if not ref['insideGraph']:
                boundary.add(ref['member'])
    require(set(shared) == boundary, 'unaccounted shared preload dependency requires a wider plan')
    locator = {r['member']: r for r in shared_plan['sharedDependencyPlan']}
    dependencies = []
    for name in sorted(boundary):
        row = locator[name]
        require(row['policy'] == 'share-unchanged-original-dependency', 'unsupported shared dependency policy')
        dependencies.append(dict(member=name, locatorSource=row['locatorSource'], uses=shared[name],
            policy='retain-native-original-Bundle-and-referenced-Object-before-candidate-load',
            currentBytesVerified=False, nativeLifetimeVerified=False,
            transitiveSharedDependenciesVerified=False))
    return dict(format='EFF_R1B_MATERIAL_TEXTURE_LOADING_EXPECTATIONS', version=1,
        verified=True, generationKey=descriptors['generationKey'], inputs=sorted(files),
        nativeSubmission=False, namespaceOnlyPayloadVerified=True,
        customTextureObjects=author_rows, materials=materials,
        directSharedDependencyPlan=dependencies,
        proposedLoadOrder=['Verify and retain shared original dependencies',
                           'Load independent texture Bundles (original and custom objects)',
                           'Load independent material Bundles',
                           'Check each candidate Material shader and texture against these exact references',
                           'Observe coexistence; unload only owned candidate Bundles with false',
                           'Recheck retained source and candidate Objects'],
        originalCustomTextureSelectionsVerified=True, toushiOriginalMaterialVerified=any(
            m['id']=='MaterialM_actor_lizhiyan_cloth_01_1' and m['wholeOriginalMaterialPreserved'] for m in materials),
        sharedNativeDependenciesVerified=False, nativeTexturePixelsVerified=False,
        materialTexturePrefabCandidateVerified=False, existingInstanceResourceMigrationVerified=False,
        completeResourceHotReloadImplemented=False)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('descriptors', 'mod', 'graph', 'generation', 'shared-plan', 'output'):
        parser.add_argument('--'+name, type=Path, required=True)
    args=parser.parse_args()
    read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
    result=verify(read(args.descriptors), read_package(args.mod), read(args.graph),
                  read(args.generation), read(args.shared_plan))
    with args.output.open('x', encoding='utf-8') as file:
        json.dump(result,file,ensure_ascii=False,indent=2);file.write('\n')
    print(json.dumps(dict(verified=True,materials=len(result['materials']),
        customTextureObjects=len(result['customTextureObjects']),
        directSharedDependencies=len(result['directSharedDependencyPlan']),
        toushiOriginalMaterialVerified=result['toushiOriginalMaterialVerified'], nativeSubmission=False)))


if __name__=='__main__':main()
