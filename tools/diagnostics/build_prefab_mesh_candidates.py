"""Build offline PFB candidates from exact Mesh identities and CAB dependencies.

Dependency reachability is a candidate filter, never proof that a PFB uses a
Mesh. A second step must resolve actual component PPtrs in the candidate files.
No game files, replacement rules, or authoring files are modified.
"""
from __future__ import annotations

import argparse
from collections import defaultdict, deque
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from query_endfield_index import ASSET, CAB, END, Reader


def records(path: Path):
    reader = Reader(path)
    assets = cabs = 0
    try:
        magic, version, fingerprint = reader.string(), reader.int32(), reader.string()
        if magic not in ("EFF_END_FIELD_INDEX", "EIEM_END_FIELD_INDEX") or version != 3:
            raise ValueError(f"Unsupported index {magic!r}, version {version}")
        yield "header", dict(magic=magic, version=version, fingerprint=fingerprint)
        while reader.tell() < reader.size:
            kind = reader.byte()
            if kind == ASSET:
                name, container, type_name = reader.string(), reader.string(), reader.string()
                path_id, source = reader.int64(), reader.string()
                assets += 1
                yield "asset", dict(name=name, container=container, type=type_name,
                                    pathID=str(path_id), source=source)
            elif kind == CAB:
                name, source, count = reader.string(), reader.string(), reader.int32()
                if not 0 <= count <= 1_000_000:
                    raise ValueError(f"Invalid dependency count {count}")
                cabs += 1
                yield "cab", dict(name=name, source=source,
                                  dependencies=[reader.string() for _ in range(count)])
            elif kind == END:
                if (reader.int64(), reader.int64()) != (assets, cabs) or reader.tell() != reader.size:
                    raise ValueError("Index footer/count mismatch")
                yield "end", dict(assets=assets, cabs=cabs)
                return
            else:
                raise ValueError(f"Unknown index record {kind}")
        raise ValueError("Incomplete index")
    finally:
        reader.close()


def build(index: Path, observations: Path) -> dict:
    live = json.loads(observations.read_text(encoding="utf-8-sig"))
    targets = [dict(cab=t["serializedFile"], pathID=t["objectKey"], name=t["name"],
                    container=t["matchedAssetPath"]) for t in live["targets"]]
    cabs = {}
    prefabs = []
    for kind, row in records(index):
        if kind == "header":
            header = row
        elif kind == "cab":
            key = row["name"].lower()
            if key in cabs and cabs[key] != row:
                raise ValueError(f"Conflicting CAB identity {key}")
            cabs[key] = row
        elif kind == "asset" and row["container"].lower().endswith((".prefab", ".pfb")):
            prefabs.append(row)
        elif kind == "end":
            counts = row

    reverse = defaultdict(set)
    source_cabs = defaultdict(set)
    for cab, row in cabs.items():
        source_cabs[row["source"]].add(cab)
        for dependency in row["dependencies"]:
            reverse[dependency.replace("\\", "/").rsplit("/", 1)[-1].lower()].add(cab)
    reached = {t["cab"].lower(): [t["cab"].lower()] for t in targets}
    for cab in reached:
        if cab not in cabs:
            raise ValueError(f"Observed CAB missing from index: {cab}")
    queue = deque(reached)
    while queue:
        dependency = queue.popleft()
        for consumer in sorted(reverse[dependency]):
            if consumer not in reached:
                reached[consumer] = [consumer, *reached[dependency]]
                queue.append(consumer)
    groups = {}
    for prefab in prefabs:
        paths = [reached[c] for c in source_cabs[prefab["source"]] if c in reached]
        if not paths:
            continue
        key = (prefab["source"], prefab["container"])
        group = groups.setdefault(key, dict(source=key[0], container=key[1], records=[],
                                            candidateReason="CAB-dependency-reachability",
                                            dependencyWitness=min(paths, key=len),
                                            directMeshUseProven=False))
        group["records"].append(prefab)
    candidates = sorted(groups.values(), key=lambda r: (r["container"], r["source"]))
    sources = {r["source"] for r in candidates}
    return dict(format=1, offlineEvidenceOnly=True, replacementRulesGenerated=False,
                index=str(index.resolve()), observations=str(observations.resolve()),
                **header, **counts, targets=targets,
                reachedCabCount=len(reached), candidateBundleCount=len(sources),
                candidatePrefabCount=len(candidates), candidates=candidates,
                bundles=[dict(source=s, cabs=sorted(source_cabs[s])) for s in sorted(sources)],
                limits=["Dependencies only select candidates; actual PPtrs must be parsed.",
                        "Runtime-assigned Mesh references need runtime evidence.",
                        "This proves nothing about sprint or attack use without action evidence.",
                        "Index fingerprint must match the current game VFS before extraction."])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("index", type=Path)
    parser.add_argument("observations", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    report = build(args.index, args.observations)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"{len(report['targets'])} exact target identities; {report['candidatePrefabCount']} PFB candidates "
          f"in {report['candidateBundleCount']} bundles. Actual Mesh references not yet proven.")


if __name__ == "__main__":
    main()
