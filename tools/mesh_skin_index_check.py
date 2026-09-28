"""Compare a Mesh's bone paths, bone hashes and skin bone indices for consistency.

The runtime builds SkinnedMeshRenderer.bones by walking the Mesh's own bone
paths in order and resolving each against the Skeleton. Skinning is then
indexed by BoneWeight.boneIndex*, which the mesh author wrote against whatever
order it had in mind. If that order is not the bonePaths order, every weight
lands on the wrong bone -- and because the wrong bones still exist it does not
fail loudly, it just deforms the mesh.

This checks, per bone slot:
  * whether the path/hash pairs agree with the skin indices actually used
  * the highest bone index any vertex references
  * whether that index range fits inside bonePaths
Read-only; prints the paths involved when a mismatch is found.
"""
from __future__ import annotations

import argparse
import struct
import sys
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
    coordinate = reader.string()
    source = reader.string()
    name = reader.string()
    vertex_count = reader.i32()
    reader.skip_floats()  # vertices
    reader.skip_floats()  # normals
    reader.skip_floats()  # tangents
    reader.skip_floats()  # colors
    for _ in range(8):
        reader.skip_floats()
    reader.take(reader.i32() * 4)          # indices
    reader.take(reader.i32() * 24)         # submeshes
    skin_count = reader.i32()
    skin = [struct.unpack("<4f4i", reader.take(32)) for _ in range(skin_count)]
    pose_count = reader.i32()
    reader.take(pose_count * 64)           # bindposes
    hash_count = reader.i32()
    hashes = [struct.unpack("<I", reader.take(4))[0] for _ in range(hash_count)]
    paths = [reader.string() for _ in range(reader.i32())] if version >= 3 else []
    return {"name": name, "source": source, "coordinate": coordinate,
            "vertexCount": vertex_count, "skin": skin, "poses": pose_count,
            "hashes": hashes, "paths": paths}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mesh")
    parser.add_argument("--list", type=int, default=12)
    args = parser.parse_args()

    try:
        mesh = read_mesh(Path(args.mesh))
    except ValueError as problem:
        print(f"parse failed: {problem}", file=sys.stderr)
        return 2

    paths = mesh["paths"]
    hashes = mesh["hashes"]
    print(f"mesh        {Path(args.mesh).name}")
    print(f"  vertexCount {mesh['vertexCount']}")
    print(f"  bonePaths   {len(paths)}")
    print(f"  boneHashes  {len(hashes)}")
    print(f"  bindPoses   {mesh['poses']}")
    print(f"  skin        {len(mesh['skin'])}")
    if len(paths) != len(hashes):
        print("  MISMATCH: bonePaths and boneHashes differ in length")
    if len(paths) != mesh["poses"]:
        print("  MISMATCH: bonePaths and bindPoses differ in length")

    used: set[int] = set()
    for entry in mesh["skin"]:
        weights, indices = entry[:4], entry[4:]
        for weight, index in zip(weights, indices):
            if weight != 0.0:
                used.add(index)
    maximum = max(used) if used else -1
    print(f"  bone indices actually used : {len(used)} distinct, max={maximum}")
    over = sorted(i for i in used if i < 0 or i >= len(paths))
    if over:
        print(f"  OUT OF RANGE indices: {len(over)} -> {over[:20]}")
    else:
        print("  every used bone index is within bonePaths")

    unused = [i for i in range(len(paths)) if i not in used]
    print(f"  bonePaths never referenced : {len(unused)}")
    for index in unused[: args.list]:
        print(f"    [{index}] {paths[index]}")
    if len(unused) > args.list:
        print(f"    ... {len(unused) - args.list} more")

    print()
    print("  first bone slots (index -> path):")
    for index in range(min(args.list, len(paths))):
        mark = "" if index in used else "   <-- unused by any weight"
        print(f"    [{index:>3}] {paths[index]}{mark}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
