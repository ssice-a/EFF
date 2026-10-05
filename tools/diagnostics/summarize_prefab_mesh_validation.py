"""Summarize exact offline PFB references; do not infer runtime effect use."""
from __future__ import annotations
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path


def summarize(references: Path, candidates: Path, author_audit: Path) -> dict:
    refs = json.loads(references.read_text(encoding="utf-8-sig"))
    manifest = json.loads(candidates.read_text(encoding="utf-8-sig"))
    authors = json.loads(author_audit.read_text(encoding="utf-8-sig"))
    names = {f"{t['cab'].lower()}:{t['pathID']}": t['name'] for t in manifest['targets']}
    uses_by_prefab = defaultdict(list)
    for row in refs['exactMeshUses']:
        for prefab in row['prefabs']:
            uses_by_prefab[prefab].append(row)
    prefab_rows = []
    for prefab, rows in sorted(uses_by_prefab.items()):
        prefab_rows.append(dict(container=prefab, exactReferenceCount=len(rows),
            meshes=sorted({names[r['mesh']['identity']] for r in rows}),
            materialSlotCounts=dict(Counter(len(r['details']['materials']) for r in rows if r['details'])),
            rendererBlendShapeWeightCounts=dict(Counter(r['details']['blendShapeWeights'] for r in rows if r['details'])),
            runtimeActionUseProven=False))
    plans = [dict(name=a['name'], meshFile=a['file'], vertices=a['vertices'],
                  submeshes=a['submeshes'], bindposes=a['bindposes'],
                  newShapeChannels=a['blendChannels'], newShapeFrames=a['blendFrames'],
                  requiredChecks=['native Mesh serialization/read acceptance',
                      'Renderer material array and per-effect shader semantics',
                      'bones palette, weight indices and bindpose agreement',
                      'Renderer weight array, channel mapping and custom GPU morph support',
                      'new Material/Texture object identity and dependency registration']) for a in authors]
    return dict(format=1, offlineEvidenceOnly=True, replacementFilesWritten=False,
        runtimeActionUseProven=False, candidatePrefabCount=manifest['candidatePrefabCount'],
        parsedSourceFileCount=refs['loadedFileCount'], exactMeshReferenceCount=refs['exactMeshUseCount'],
        resolvedPrefabCount=len(prefab_rows), prefabReferences=prefab_rows,
        unresolvedPrefabRoots=[r for r in refs['prefabRoots'] if r['rootCount'] != 1],
        exactReferencesWithoutResolvedPrefab=sum(not r['prefabs'] for r in refs['exactMeshUses']),
        unknownComponents=refs['unknownComponents'], failures=refs['failures'],
        authorChangeRequirements=plans,
        runtimeEvidenceNeeded=['resource requests before/during sprint and attack',
            'PFB instance completion and persistent-pool returns, including cached reuse',
            'effect Mesh/material assignment if no serialized reference exists'],
        limits=['Missing load events cannot prove a cached effect was unused.',
            'No PFB is identified as an afterimage solely from its name or a dependency.',
            'Unresolved PFB roots are gaps, not negative Mesh-reference evidence.',
            'Material texture edits must update PPtrs and dependency registration, not only paths.',
            'Added shape data is serialized; weights are controlled per runtime Renderer.',
            'Submesh toggle support still needs a validated display control that never edits native geometry.'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('references', type=Path)
    parser.add_argument('candidates', type=Path)
    parser.add_argument('author_audit', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    report = summarize(args.references, args.candidates, args.author_audit)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f"Resolved {report['resolvedPrefabCount']} PFBs; {len(report['unresolvedPrefabRoots'])} roots remain unresolved; "
          'runtime sprint/attack association pending.')


if __name__ == '__main__':
    main()
