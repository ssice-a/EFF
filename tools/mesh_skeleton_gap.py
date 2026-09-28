"""Report whether a mod Mesh's skin names bones the replacement Skeleton lacks.

A SkinnedMeshRenderer whose bones array does not cover the mesh's own bone
paths leaves those vertices weighted to nothing, and the mesh collapses -- which
is what a character "lying on the ground" looks like. This walks both containers
with the runtime's own layout so the comparison is between the paths the runtime
would actually resolve, not a text scan of the file.

Mesh    (EFFMESH v2/v3): magic, version, coordinate, source, name, vertexCount,
        vertices/normals/tangents/colors as count+float payload, 8 UV channels,
        indices, submeshes, skin, bindposes, boneHashes, [v3] bonePaths,
        blend shapes.
Skeleton (EFFSKEL v1/v2): magic, version, coordinate, nodes (LEB128 path,
        parent, position, rotation, scale), paletteCount(0), root(-1),
        [v2] source flags.
Strings use a LEB128 length prefix. Read-only.
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
            raise ValueError(f"truncated at {self.at}: wanted {count} bytes")
        chunk = self.blob[self.at : self.at + count]
        self.at += count
        return chunk

    def value(self, fmt: str):
        size = struct.calcsize(fmt)
        return struct.unpack(fmt, self.take(size))[0]

    def count(self) -> int:
        return self.value("<i")

    def string(self) -> str:
        length = 0
        for shift in range(0, 29, 7):
            byte = self.take(1)[0]
            length |= (byte & 0x7F) << shift
            if not byte & 0x80:
                break
        else:
            raise ValueError("bad LEB128 length")
        if length > 4 * 1024 * 1024:
            raise ValueError(f"absurd string length {length}")
        return self.take(length).decode("utf-8")

    def float_array(self) -> int:
        """Read count + raw floats; return how many floats there were."""
        count = self.count()
        if count < 0:
            raise ValueError("negative float count")
        self.take(count * 4)
        return count

    def at_end(self) -> bool:
        return self.at == len(self.blob)


def read_mesh(blob: bytes) -> tuple[str, str, list[str], int]:
    reader = Reader(blob)
    if reader.take(8) != b"EFFMESH\x00":
        raise ValueError("not an EFFMESH container")
    version = reader.count()
    if version not in (2, 3):
        raise ValueError(f"unsupported mesh version {version}")
    coordinate = reader.string()
    source = reader.string()
    name = reader.string()
    vertex_count = reader.count()
    if not 0 <= vertex_count <= 10_000_000:
        raise ValueError(f"bad vertexCount {vertex_count}")
    reader.float_array()  # vertices   (3 per vertex)
    reader.float_array()  # normals
    reader.float_array()  # tangents
    reader.float_array()  # colors
    for _ in range(8):
        reader.float_array()  # uv channels
    reader.take(reader.count() * 4)  # indices
    for _ in range(reader.count()):
        reader.take(24)  # submesh: 6 x int32
    for _ in range(reader.count()):
        reader.take(32)  # bone weight: 4 x float + 4 x int32
    pose_count = reader.count()
    reader.take(pose_count * 64)  # bindposes
    reader.take(reader.count() * 4)  # bone hashes
    bone_paths: list[str] = []
    if version >= 3:
        bone_paths = [reader.string() for _ in range(reader.count())]
    return coordinate, source or name, bone_paths, pose_count


def read_skeleton(blob: bytes) -> list[tuple[str, bool]]:
    reader = Reader(blob)
    if reader.take(8) != b"EFFSKEL\x00":
        raise ValueError("not an EFFSKEL container")
    version = reader.count()
    if version not in (1, 2):
        raise ValueError(f"unsupported skeleton version {version}")
    if reader.string() != "unity-y-up-left-handed":
        raise ValueError("unexpected coordinate space")
    nodes: list[tuple[str, bool]] = []
    for _ in range(reader.count()):
        path = reader.string()
        reader.take(4)  # parent index
        reader.take(40)  # position(12) + rotation(16) + scale(12)
        nodes.append((path, True))
    if reader.count() != 0:
        raise ValueError("unexpected non-zero palette count")
    if reader.value("<i") != -1:
        raise ValueError("unexpected root index")
    if version == 2:
        flags = reader.count()
        if flags != len(nodes):
            raise ValueError(f"source flag count {flags} != node count {len(nodes)}")
        nodes = [
            (path, reader.take(1)[0] != 0) for path, _source in nodes
        ]
    return nodes


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mesh")
    parser.add_argument("skeleton")
    parser.add_argument("--list-missing", type=int, default=15)
    args = parser.parse_args()

    mesh_blob = Path(args.mesh).read_bytes()
    skel_blob = Path(args.skeleton).read_bytes()
    try:
        _coord, source, bone_paths, pose_count = read_mesh(mesh_blob)
        nodes = read_skeleton(skel_blob)
    except ValueError as problem:
        print(f"parse failed: {problem}", file=sys.stderr)
        return 2

    present = {path for path, _source in nodes}
    missing = [path for path in bone_paths if path not in present]
    skirt_missing = [path for path in missing if "maid_skirt" in path]
    skirt_nodes = [path for path, _ in nodes if "maid_skirt" in path]

    print(f"mesh      {Path(args.mesh).name}")
    print(f"  source          {source or '<none>'}")
    print(f"  bonePaths       {len(bone_paths)}")
    print(f"  bindPoses       {pose_count}")
    print(f"skeleton  {Path(args.skeleton).name}")
    print(f"  nodes           {len(nodes)}")
    print(f"  nodes+maid_skirt{len(skirt_nodes):>4}")
    print(f"  mesh->skeleton bone paths NOT present : {len(missing)}")
    print(f"    of which maid_skirt                 : {len(skirt_missing)}")
    if missing:
        print("  VERDICT: skin references bones the skeleton does not declare;")
        print("           those vertices resolve to no bone and collapse.")
    else:
        print("  VERDICT: every skin bone path resolves in the skeleton.")
    for path in missing[: args.list_missing]:
        print(f"    missing: {path}")
    if len(missing) > args.list_missing:
        print(f"    ... {len(missing) - args.list_missing} more")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
