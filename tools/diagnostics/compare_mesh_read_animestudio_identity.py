"""Offline evidence only: compare observed Mesh keys with AnimeStudio's index.

This does not generate replacement rules or a prefab-consumer dependency table.
The plugin must obtain the file identity from the live resource-loading chain.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from query_endfield_index import ASSET, CAB, END, Reader


def compare(index: Path, observations: Path) -> dict:
    live = json.loads(observations.read_text(encoding="utf-8-sig"))
    targets = live["targets"]
    wanted_names = {t["name"] for t in targets}
    wanted_keys = {int(t["objectKey"]) for t in targets}
    matches: list[dict] = []
    cab_sources: dict[str, str] = {}
    asset_count = cab_count = 0
    complete = False
    reader = Reader(index)
    try:
        magic = reader.string()
        if magic not in ("EFF_END_FIELD_INDEX", "EIEM_END_FIELD_INDEX"):
            raise ValueError(f"Unsupported AnimeStudio index magic: {magic!r}")
        version = reader.int32()
        if version != 3:
            raise ValueError(f"Unsupported index version: {version}")
        fingerprint = reader.string()
        while reader.tell() < reader.size:
            kind = reader.byte()
            if kind == ASSET:
                name, container, type_name = reader.string(), reader.string(), reader.string()
                path_id, source = reader.int64(), reader.string()
                asset_count += 1
                if name in wanted_names or path_id in wanted_keys:
                    matches.append(dict(name=name, pathID=str(path_id), container=container,
                                        type=type_name, source=source))
            elif kind == CAB:
                name, source, count = reader.string(), reader.string(), reader.int32()
                if count < 0 or count > 1_000_000:
                    raise ValueError(f"Invalid dependency count: {count}")
                for _ in range(count):
                    reader.string()
                cab_sources[name] = source
                cab_count += 1
            elif kind == END:
                stored_assets, stored_cabs = reader.int64(), reader.int64()
                if (stored_assets, stored_cabs) != (asset_count, cab_count):
                    raise ValueError("Index record counts do not match footer")
                if reader.tell() != reader.size:
                    raise ValueError("Unexpected bytes after index footer")
                complete = True
                break
            else:
                raise ValueError(f"Unknown record {kind} at {reader.tell()}")
        if not complete:
            raise ValueError("Incomplete index: missing footer")
    finally:
        reader.close()

    rows = []
    for target in targets:
        candidates = [a for a in matches if a["name"] == target["name"] and a["type"] == "Mesh"]
        exact = [a for a in candidates if a["pathID"] == target["objectKey"]]
        for asset in candidates:
            asset["serializedFileCandidates"] = sorted(
                name for name, source in cab_sources.items() if source == asset["source"]
            )
        rows.append(dict(name=target["name"], owner=target["owner"],
                         objectKey=target["objectKey"], recordOffset=target["recordOffset"],
                         recordLength=target["recordLength"],
                         nameCandidateCount=len(candidates), keyMatchCount=len(exact),
                         matches=exact, nameCandidates=candidates))
    return dict(index=str(index.resolve()), observations=str(observations.resolve()),
                offlineEvidenceOnly=True, runtimeFileIdentityProven=False,
                magic=magic, version=version, vfsFingerprint=fingerprint,
                complete=complete, assetRecords=asset_count, cabRecords=cab_count,
                targetCount=len(rows), matchedTargets=sum(bool(r["matches"]) for r in rows),
                targets=rows)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("index", type=Path)
    parser.add_argument("observations", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    report = compare(args.index, args.observations)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Matched {report['matchedTargets']}/{report['targetCount']} target keys to AnimeStudio Mesh PathIDs")
    for row in report["targets"]:
        print(f"{row['name']} keyMatches={row['keyMatchCount']} nameCandidates={row['nameCandidateCount']}")
        for asset in row["matches"]:
            print(f"  {asset['source']} | {asset['container']}")


if __name__ == "__main__":
    main()
