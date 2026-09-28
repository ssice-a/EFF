"""Compare the skin structure of several mod meshes side by side.

Written to test whether a failure that is reported per-family is actually
structural: if one source asset's meshes differ from another's in bone count,
weight normalisation, or which bones they reference, then "sometimes this mesh
lies down" may really be "this family is always broken and the other is not".

Reports per mesh: bone path count, distinct bones any vertex actually uses,
whether weights sum to one, how many vertices carry no weight at all, and the
bone counts that only exist as Mod-private nodes (the ones the game does not
have, so nothing animates them).

Read-only.
"""
from __future__ import annotations

import argparse
import struct
import sys
from collections import Counter
from pathlib import Path


class Reader:
    def __init__(self, blob: bytes) -> None:
        self.blob = blob
        self.at = 0

    def take(self, count: int) -> bytes:
        if self.at + count > len(self.blob):
            raise ValueError(f"truncated at {self.at}: wanted {count}")
        chunk = self.blob[self.at : self.at + count]
        self.at += count
        return chunk

    def i32(self) -> int:
        return struct.unpack("<i", self.take(4))[0]

    def string(self) -> str:
        length = 0
        for shift in range(0, 29, 7):
            byte = self.take(1)[0]
            length |= (byte & 0x7F) << shift
            if not byte & 0x80:
                break
        return self.take(length).decode("utf-8")

    def skip_floats(self) -> int:
        count = self.i32()
        self.take(count * 4)
        return count


def read_mesh(path: Path):
    reader = Reader(path.read_bytes())
    if reader.take(8) != b"EFFMESH\x00":
        raise ValueError("not an EFFMESH container")
    version = reader.i32()
    reader.string()  # coordinate
    source = reader.string()
    name = reader.string()
    vertex_count = reader.i32()
    reader.skip_floats()  # vertices
    reader.skip_floats()  # normals
    reader.skip_floats()  # tangents
    reader.skip_floats()  # colors
    for _ in range(8):
        reader.skip_floats()
    reader.take(reader.i32() * 4)   # indices
    reader.take(reader.i32() * 24)  # submeshes
    skin_count = reader.i32()
    skin = [struct.unpack("<4f4i", reader.take(32)) for _ in range(skin_count)]
    pose_count = reader.i32()
    reader.take(pose_count * 64)
    hash_count = reader.i32()
    reader.take(hash_count * 4)
    paths = [reader.string() for _ in range(reader.i32())] if version >= 3 else []
    return {
        "name": name,
        "source": source,
        "vertexCount": vertex_count,
        "skin": skin,
        "paths": paths,
        "poses": pose_count,
    }


def report(path: Path) -> dict:
    mesh = read_mesh(path)
    paths = mesh["paths"]
    used: Counter = Counter()
    unweighted = 0
    bad_sum = 0
    sum_buckets: Counter = Counter()
    for entry in mesh["skin"]:
        weights, indices = entry[:4], entry[4:]
        total = 0.0
        for weight, index in zip(weights, indices):
            if weight == 0.0:
                continue
            total += weight
            if 0 <= index < len(paths):
                used[index] += 1
        if total == 0.0:
            unweighted += 1
        else:
            if abs(total - 1.0) > 1e-3:
                bad_sum += 1
            sum_buckets[round(total, 3)] += 1
    skirt = [i for i in used if "maid_skirt" in paths[i]]
    return {
        "file": path.name,
        "name": mesh["name"],
        "source": mesh["source"].rsplit("/", 1)[-1],
        "verts": mesh["vertexCount"],
        "bones": len(paths),
        "poses": mesh["poses"],
        "usedBones": len(used),
        "unweightedVerts": unweighted,
        "weightsNotOne": bad_sum,
        "sumValues": sum_buckets.most_common(4),
        "skirtBonesUsed": len(skirt),
        "skirtVerts": sum(used[i] for i in skirt),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("meshes", nargs="+")
    args = parser.parse_args()

    rows = []
    for item in args.meshes:
        path = Path(item)
        try:
            rows.append(report(path))
        except ValueError as problem:
            print(f"{path.name}: {problem}", file=sys.stderr)

    header = (f"{'mesh':<46} {'verts':>7} {'bones':>6} {'used':>5} "
              f"{'noW':>6} {'w!=1':>6} {'skirtB':>7} {'skirtV':>7}")
    print(header)
    print("-" * len(header))
    for row in rows:
        print(f"{row['file']:<46} {row['verts']:>7} {row['bones']:>6} "
              f"{row['usedBones']:>5} {row['unweightedVerts']:>6} "
              f"{row['weightsNotOne']:>6} {row['skirtBonesUsed']:>7} "
              f"{row['skirtVerts']:>7}")
    print()
    for row in rows:
        print(f"{row['file']}: source={row['source']} weightSums={row['sumValues']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
