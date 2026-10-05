"""Summarize resource/instance evidence without inferring draw or action use."""
from __future__ import annotations
from collections import Counter, defaultdict
import argparse
import json
from pathlib import Path
import re

FIELDS = re.compile(r'(\w+)=(?:"([^"]*)"|(\S+))')


def analyze(path: Path) -> dict:
    events = []
    install = []
    captures = []
    for number, line in enumerate(path.read_text(encoding='utf-8-sig', errors='replace').splitlines(), 1):
        if '[PFB-RESOURCE-OBSERVATION]' in line:
            install.append(dict(line=number, text=line))
        if '[PFB-RESOURCE-EVENT]' in line:
            row = {key: quoted if quoted else plain for key, quoted, plain in FIELDS.findall(line)}
            row['line'] = number
            events.append(row)
        elif 'p_common_capture_mesh' in line:
            captures.append(dict(line=number, text=line))
    groups = defaultdict(list)
    for event in events:
        groups[event.get('path', '?').lower()].append(event)
    paths = []
    for key, rows in groups.items():
        paths.append(dict(path=rows[0].get('path'), count=len(rows),
            eventKinds=dict(Counter(r['event'] for r in rows)),
            firstTick=min(int(r['tick']) for r in rows), lastTick=max(int(r['tick']) for r in rows),
            firstLine=rows[0]['line'], lastLine=rows[-1]['line'],
            instances=[r for r in rows if 'instance' in r['event'] or 'pool' in r['event']]))
    sequences = [int(r['seq']) for r in events]
    observed = set(sequences)
    missing = sorted(set(range(min(sequences), max(sequences) + 1)) - observed) if sequences else []
    selected = [r for r in paths if any(s in r['path'].lower() for s in
        ('lizhiyan', 'canying', 'capture_mesh', 'dissolv', 'disslov', 'dash', 'sprint', 'dodge', 'evade'))]
    return dict(log=str(path.resolve()), eventCount=len(events),
        eventKinds=dict(Counter(r['event'] for r in events)), sequenceGaps=missing,
        unresolvedEvents=sum(r.get('pathStatus') != 'observed' for r in events),
        install=install, uniquePathCount=len(paths), paths=sorted(paths, key=lambda p: p['firstLine']),
        selectedPaths=sorted(selected, key=lambda p: p['firstLine']), captureMeshAssignmentLines=captures,
        runtimeDrawUseProven=False,
        limits=['A request event is not a draw event.', 'A preload may occur long before an attack.',
            'No event does not prove no use of a cached instance.',
            'Proxy log names need verification before treating them as full logical resource paths.'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    report = analyze(args.log)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f"events={report['eventCount']} unresolved={report['unresolvedEvents']} "
          f"sequenceGaps={len(report['sequenceGaps'])}; kinds={report['eventKinds']}")
    for row in report['selectedPaths']:
        if row['path'].lower().endswith(('.prefab', '.pfb', '.mat')):
            print(f"{row['firstLine']}-{row['lastLine']} ticks={row['firstTick']}..{row['lastTick']} "
                  f"n={row['count']} instances={len(row['instances'])} {row['path']}")


if __name__ == '__main__':
    main()
