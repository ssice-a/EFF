"""Summarise EFFMESH bonePath sets so sibling meshes can be compared.

Read-only analysis helper. It parses the documented EFFMESH layout far enough to
reach the v3 bonePath table and reports, per mesh: vertex/submesh/bone counts,
the full bonePath list, and the exact branch each mesh hangs on. The goal is to
answer "how many independent bone chains does a merged mesh need", so the report
focuses on path prefixes rather than vertex data.

Usage:
    python tools/bone_path_stats.py <mesh> [<mesh> ...]
    python tools/bone_path_stats.py --dir <folder>
"""

from __future__ import annotations

import argparse
import os
import struct
import sys
from collections import Counter

MAGIC = b"EFFMESH\x00"


class Reader:
    def __init__(self, data: bytes) -> None:
        self.data = data
        self.pos = 0

    def take(self, count: int) -> bytes:
        end = self.pos + count
        if end > len(self.data):
            raise ValueError("unexpected end of file")
        chunk = self.data[self.pos : end]
        self.pos = end
        return chunk

    def i32(self) -> int:
        return struct.unpack("<i", self.take(4))[0]

    def u32(self) -> int:
        return struct.unpack("<I", self.take(4))[0]

    def f32(self) -> float:
        return struct.unpack("<f", self.take(4))[0]

    def string(self) -> str:
        length = 0
        shift = 0
        while True:
            byte = self.take(1)[0]
            length |= (byte & 0x7F) << shift
            if not byte & 0x80:
                break
            shift += 7
        return self.take(length).decode("utf-8", "replace")

    def floats(self) -> int:
        """Consume one length-prefixed float array and return its element count."""
        count = self.i32()
        if count < 0:
            raise ValueError(f"invalid float array length {count}")
        self.take(count * 4)
        return count


def read_mesh(path: str) -> dict:
    with open(path, "rb") as handle:
        reader = Reader(handle.read())
    if reader.take(8) != MAGIC:
        raise ValueError("not an EFFMESH file")
    version = reader.i32()
    if version not in (2, 3):
        raise ValueError(f"unsupported EFFMESH version {version}")
    coordinate_space = reader.string()
    source = reader.string()
    name = reader.string()

    vertex_count = reader.i32()
    components = [reader.floats() for _ in range(4)]  # vertices/normals/tangents/colors
    uvs = [reader.floats() for _ in range(8)]
    if not components[0]:
        raise ValueError("empty vertex array")
    declared = vertex_count if vertex_count > 0 else components[0] // 3
    if components[0] != declared * 3:
        raise ValueError(
            f"vertex array is {components[0]} floats, expected {declared * 3}"
        )
    vertex_count = declared

    index_count = reader.i32()
    reader.take(index_count * 4)

    submesh_count = reader.i32()
    submeshes = [reader.take(24) for _ in range(submesh_count)]

    skin_count = reader.i32()
    reader.take(skin_count * 32)
    bindpose_count = reader.i32()
    reader.take(bindpose_count * 64)

    bone_hash_count = reader.i32()
    reader.take(bone_hash_count * 4)

    bone_paths: list[str] = []
    if version >= 3:
        bone_path_count = reader.i32()
        bone_paths = [reader.string() for _ in range(bone_path_count)]

    return {
        "path": path,
        "name": name,
        "source": source,
        "coordinate_space": coordinate_space,
        "version": version,
        "vertices": vertex_count,
        "indices": index_count,
        "submeshes": submesh_count,
        "skin": skin_count,
        "bindposes": bindpose_count,
        "bone_hashes": bone_hash_count,
        "bone_paths": bone_paths,
    }


def parent(path: str) -> str:
    for separator in ("/", "|", "\\"):
        if separator in path:
            return path.rsplit(separator, 1)[0]
    return "<root>"


def summarise(meshes: list[dict]) -> None:
    for mesh in meshes:
        paths = mesh["bone_paths"]
        branches = Counter(parent(p) for p in paths)
        dominant = ", ".join(
            f"{branch} x{count}" for branch, count in branches.most_common(3)
        )
        print(f"\n=== {os.path.basename(mesh['path'])} ===")
        print(
            f"  vertices={mesh['vertices']} indices={mesh['indices']} "
            f"submeshes={mesh['submeshes']} skin={mesh['skin']} "
            f"bindposes={mesh['bindposes']} bonePaths={len(paths)}"
        )
        print(f"  top branches: {dominant}")

    if len(meshes) < 2:
        return

    print("\n=== pairwise bonePath comparison ===")
    all_paths = [set(m["bone_paths"]) for m in meshes]
    union = set().union(*all_paths)
    intersection = set.intersection(*all_paths) if all_paths else set()
    print(f"  union={len(union)} intersection={len(intersection)}")
    missing = sorted(union - intersection)
    if missing:
        print(f"  paths not shared by every mesh ({len(missing)}):")
        for path in missing[:40]:
            owners = [
                os.path.basename(meshes[i]["path"]).split(".")[0][-6:]
                for i, paths in enumerate(all_paths)
                if path in paths
            ]
            print(f"    {path}  <- {', '.join(owners)}")
        if len(missing) > 40:
            print(f"    ... {len(missing) - 40} more")
    else:
        print("  identical bonePath sets: merge needs ONE palette")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("meshes", nargs="*", help="EFFMESH files to inspect")
    parser.add_argument("--dir", help="inspect every *.mesh in this folder")
    args = parser.parse_args()

    targets = list(args.meshes)
    if args.dir:
        targets.extend(
            os.path.join(args.dir, entry)
            for entry in sorted(os.listdir(args.dir))
            if entry.lower().endswith(".mesh")
        )
    if not targets:
        parser.print_help()
        return 1

    meshes = []
    for target in targets:
        try:
            meshes.append(read_mesh(target))
        except Exception as error:  # noqa: BLE001 - report and continue
            print(f"skip {target}: {error}", file=sys.stderr)
    summarise(meshes)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
