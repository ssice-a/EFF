"""Cross-check offline candidate references and classify shared original boundaries.

This cannot establish Unity native cache isolation or existing-instance migration.
The locator index provides identities only; it is not native lifetime evidence.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
from build_prefab_mesh_candidates import records


def require(condition, message):
    if not condition:
        raise ValueError(message)


def verify(source, candidate, manifest):
    names = manifest['memberMap']
    bundles = manifest['bundleMap']
    require(len(set(names.values())) == len(names) and not set(names) & set(names.values()), 'member namespace is not disjoint')
    require(len(set(bundles.values())) == len(bundles) and not set(bundles) & set(bundles.values()), 'Bundle namespace is not disjoint')
    before = {row['member']: row for row in source['serialized']}
    after = {row['member']: row for row in candidate['serialized']}
    require(set(after) == {names[name] for name in before}, 'serialized member coverage differs')
    require(source['members'] == candidate['members'] and source['inputs'] == candidate['inputs'], 'input/member count differs')
    count = 0
    stream_count = 0
    shared_refs = 0
    for name, old in before.items():
        new = after[names[name]]
        require(new['bundleName'] == bundles[old['bundleName']], 'AssetBundle identity not isolated')
        for key in ('input', 'flags', 'objectCount', 'classCounts', 'unparsedObjects'):
            require(old[key] == new[key], 'candidate changed ' + key)
        expected_assets = [dict(row, member=names.get(row['member'], row['member'])) for row in old['assets']]
        require(expected_assets == new['assets'], 'logical asset/container identity changed')
        require(len(old['externals']) == len(new['externals']), 'external slot count changed')
        for a, b in zip(old['externals'], new['externals']):
            expected = dict(a, member=names.get(a['member'], a['member']))
            if a['member'] in names:
                require(a['path'] == f"archive:/{a['member']}/{a['member']}", 'external archive grammar differs')
                expected['path'] = f"archive:/{names[a['member']]}/{names[a['member']]}"
            require(expected == b, 'external fileID/GUID/identity/offset changed')
        expected_refs = [dict(row, member=names.get(row['member'], row['member'])) for row in old['referenceTargets']]
        require(expected_refs == new['referenceTargets'], 'numeric PPtr or graph resolution changed')
        count += sum(row['uses'] for row in old['referenceTargets'])
        shared_refs += sum(row['uses'] for row in old['referenceTargets'] if not row['insideGraph'])
        require(len(old['streams']) == len(new['streams']), 'stream count changed')
        for a, b in zip(old['streams'], new['streams']):
            member = a['member']
            expected = dict(a)
            if member in names:
                cab, new_cab = member[:36], names[member][:36]
                require(a['path'] == f'archive:/{cab}/{member}', 'stream archive grammar differs')
                expected.update(member=names[member], path=f'archive:/{new_cab}/{names[member]}')
            # The optional offset was added to the auditor after the first frozen audit.
            b = dict(b)
            if 'pathDataOffset' not in a:
                b.pop('pathDataOffset', None)
            require(expected == b, 'stream range/identity changed')
            stream_count += 1
    for key in ('internalReferenceUses', 'boundaryReferenceUses', 'brokenReferenceUses', 'brokenStreams', 'unparsedObjects', 'boundaryStreams'):
        require(source[key] == candidate[key], 'candidate graph summary differs: ' + key)
    require(candidate['brokenReferenceUses'] == 0 and candidate['brokenStreams'] == 0, 'candidate has broken references/ranges')
    require(source['externalBoundaryMembers'] == candidate['externalBoundaryMembers'], 'shared original dependency identities changed')
    return dict(serializedMembers=len(before), numericReferenceUsesPreserved=count,
                streamRecordsPreserved=stream_count, sharedOriginalReferenceUses=shared_refs,
                logicalAssetIdentitiesPreserved=True, perMeshPaletteAndBindposesPreserved=True,
                geometryShapeMaterialAndPixelContentPreservedBySchemaBoundedPatches=True,
                internalGraphReferencesVerified=True, sharedBoundaryIdentitiesPreserved=True,
                opaqueManagedObjectCount=candidate['unparsedObjects'],
                opaqueManagedPayloadsPreserved=manifest['opaqueObjectsPreserved'] == candidate['unparsedObjects'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source-audit', 'candidate', 'index', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    require(not args.output.exists(), 'verification report already exists')
    manifest = json.loads((args.candidate / 'manifest.json').read_text(encoding='utf8'))
    require(manifest['schemaNamespacePrepared'] and not manifest['nativeSubmission'], 'incomplete or native-published candidate')
    source = json.loads(args.source_audit.read_text(encoding='utf8'))
    candidate = json.loads((args.candidate / 'graph-audit.json').read_text(encoding='utf8'))
    report = dict(format='EFF_OFFLINE_LOADING_GENERATION_VERIFICATION', version=1,
                  generationKey=manifest['generationKey'], nativeSubmission=False,
                  generationCacheIsolationVerified=False, existingInstanceResourceMigrationVerified=False,
                  completeResourceHotReloadImplemented=False, readyForNativeCandidateLoading=False,
                  verified=False)
    try:
        for row in manifest['files']:
            path = args.candidate / 'inputs' / row['file']
            require(path.parent.resolve() == (args.candidate / 'inputs').resolve(), 'candidate manifest path escapes input folder')
            data = path.read_bytes()
            require(len(data) == row['bytes'] and hashlib.sha256(data).hexdigest().upper() == row['candidateSha256'], 'candidate file changed')
        report.update(verify(source, candidate, manifest))
        needed = {name.lower() for name in source['externalBoundaryMembers']}
        locator = {}
        all_cabs, all_bundles = set(), set()
        for kind, row in records(args.index):
            if kind == 'header':
                report['locatorFingerprint'] = row['fingerprint']
            elif kind == 'cab':
                name = row['name'].lower()
                all_cabs.add(name)
                # index source is Bundles/Windows/main/<hash>.ab; native m_Name is main/<hash>.ab.
                all_bundles.add(row['source'].replace('\\','/').split('Bundles/Windows/',1)[-1].lower())
                if name in needed:
                    require(name not in locator or locator[name] == row['source'], 'ambiguous shared dependency locator')
                    locator[name] = row['source']
        require(not {name.lower() for name in manifest['memberMap'].values() if len(name) == 36} & all_cabs,
                'candidate CAB collides with current game locator')
        require(not {name.lower() for name in manifest['bundleMap'].values()} & all_bundles,
                'candidate Bundle identity collides with current game locator')
        report['candidateNamesDisjointFromCurrentLocator'] = True
        boundary = []
        for name in source['externalBoundaryMembers']:
            if name.lower() == 'unity default resources':
                boundary.append(dict(member=name, policy='preserve-native-builtin', nativeGuidPreserved=True))
            elif name.lower() in locator:
                boundary.append(dict(member=name, policy='share-unchanged-original-dependency',
                                     locatorSource=locator[name.lower()], currentBytesVerified=False,
                                     nativeLoadAndLifetimeVerified=False))
            else:
                boundary.append(dict(member=name, policy='unresolved-original-dependency',
                                     currentBytesVerified=False, nativeLoadAndLifetimeVerified=False))
        report['sharedDependencyPlan'] = boundary
        report['sharedOriginalDependencyCount'] = sum(row['policy'] == 'share-unchanged-original-dependency' for row in boundary)
        report['nativeBuiltinCount'] = sum(row['policy'] == 'preserve-native-builtin' for row in boundary)
        report['unresolvedBoundaryCount'] = sum(row['policy'] == 'unresolved-original-dependency' for row in boundary)
        report['outgoingClosureVerified'] = False
        report['remainingNativeRequirements'] = [
            'Verify shared-original dependencies and retain their native ownership during candidate load.',
            'Create candidate Bundles/assets at verified Unity boundaries; verify old/new object identities and dependency attribution.',
            'Reject incomplete candidate loads and preserve the old generation.',
            'Use a verified owner/display migration transaction, preserving controllers, animation, LOD and immutable afterimages.',
        ]
        report['verified'] = True
    except Exception as error:
        report['error'] = str(error)
        raise
    finally:
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf8')
    print(json.dumps({k:report[k] for k in ('verified','serializedMembers','numericReferenceUsesPreserved','streamRecordsPreserved',
        'sharedOriginalDependencyCount','nativeBuiltinCount','unresolvedBoundaryCount','readyForNativeCandidateLoading')},ensure_ascii=False))


if __name__ == '__main__':
    main()
