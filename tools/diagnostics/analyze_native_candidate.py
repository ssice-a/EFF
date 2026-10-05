"""Summarize the scoped R1a native body witness, never full reload acceptance."""
import argparse
from collections import defaultdict
import json
from pathlib import Path
import re


def summarize(text):
    rows=defaultdict(list)
    for line in text.splitlines():
        match=re.search(r'\[(NATIVE-CANDIDATE-[A-Z-]+)\]\s*(.*)',line)
        if match:
            payload=match.group(2)
            fields={m.group(1):m.group(2) for m in re.finditer(r'(\w+)=(.*?)(?=\s+\w+=|$)',payload)}
            rows[match.group(1)].append(fields)
    errors=rows['NATIVE-CANDIDATE-REJECT']+rows['NATIVE-CANDIDATE-DRAIN-GAP']+rows['NATIVE-CANDIDATE-QUARANTINE']
    assets=rows['NATIVE-CANDIDATE-ASSET']
    distinct=bool(assets) and all(r.get('nativeIdentityDistinct')=='1' and
        r.get('nativeId') and r.get('originalNativeId') and r['nativeId']!=r['originalNativeId'] for r in assets)
    content=rows['NATIVE-CANDIDATE-CONTENT']
    parity=bool(content) and all(r.get('nativeArraysEqual')=='1' and len(r.get('baseDigest',''))==64 and
                               len(r.get('shapeDigest',''))==64 for r in content)
    passes=rows['NATIVE-CANDIDATE-PASS']
    scoped=bool(passes) and all(r.get('scope')=='dependency-free-body-Mesh' and
        r.get('nativeArraysEqual')=='1' and r.get('fullResourceReload')=='0' for r in passes)
    complete=bool(rows['NATIVE-CANDIDATE-READY'] and rows['NATIVE-CANDIDATE-PREPARED'] and
                  rows['NATIVE-CANDIDATE-BEGIN'] and rows['NATIVE-CANDIDATE-BUNDLE'])
    return dict(format='EFF_R1A_NATIVE_BODY_CANDIDATE_FINDINGS',version=1,
        ready=bool(rows['NATIVE-CANDIDATE-READY']),prepared=rows['NATIVE-CANDIDATE-PREPARED'],
        begin=rows['NATIVE-CANDIDATE-BEGIN'],bundleCompletions=rows['NATIVE-CANDIDATE-BUNDLE'],
        meshReturns=assets,content=content,cleanup=rows['NATIVE-CANDIDATE-CLEANUP'],
        completedWitnesses=passes,errors=errors,deduplicatedTriggers=rows['NATIVE-CANDIDATE-DEDUP'],
        nativeBodyWitnessVerified=complete and distinct and parity and scoped and not errors,
        distinctNativeMeshIdentities=distinct,nativeArrayAndShapeParity=parity,
        sharedDependencyLifetimeVerified=False,materialTexturePrefabCandidateVerified=False,
        existingInstanceResourceMigrationVerified=False,completeResourceHotReloadImplemented=False,
        visualAcceptanceReported=False,
        scope='Dependency-free body Mesh, independent native Bundle/Mesh and read-only parity; original Renderer retains original source.')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log',type=Path);parser.add_argument('output',type=Path)
    args=parser.parse_args()
    report=summarize(args.log.read_text(encoding='utf-8-sig',errors='replace'))
    report['log']=str(args.log.resolve())
    with args.output.open('x',encoding='utf8') as file:
        json.dump(report,file,ensure_ascii=False,indent=2);file.write('\n')
    print(json.dumps({k:report[k] for k in ('ready','nativeBodyWitnessVerified','distinctNativeMeshIdentities',
        'nativeArrayAndShapeParity','completeResourceHotReloadImplemented')},ensure_ascii=False))


if __name__=='__main__':main()
