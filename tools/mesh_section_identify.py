"""Identify which exported mesh object each mod Mesh section came from.

The exported container records the Blender object's own name and the game asset
it replaces, but the mod sections are numbered. This prints, per section, the
recorded object name and a geometry fingerprint, and cross-references the
renderer hierarchy the runtime reported into the game's own LOD group, so a
part can be matched to the slot it actually occupies.

Read-only.
"""
from __future__ import annotations

import argparse
import re
import struct
import sys
from pathlib import Path

FIELD = re.compile(r"(\w+)=(\[[^\]]*\]|[^\s]+)")


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
        return self.take(length).decode("utf-8", errors="replace")

    def floats(self) -> list:
        count = self.i32()
        return list(struct.unpack(f"<{count}f", self.take(count * 4)))


def mesh_fingerprint(path: Path) -> dict:
    reader = Reader(path.read_bytes())
    if reader.take(8) != b"EFFMESH\x00":
        raise ValueError("not an EFFMESH container")
    version = reader.i32()
    reader.string()          # coordinate
    source = reader.string()
    name = reader.string()
    vertex_count = reader.i32()
    for _ in range(4):
        reader.floats()      # vertices, normals, tangents, colors
    for _ in range(8):
        reader.floats()      # uv channels
    return {
        "version": version,
        "source": source.rsplit("/", 1)[-1],
        "object": name,
        "verts": vertex_count,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mod_root")
    parser.add_argument("--log")
    args = parser.parse_args()

    root = Path(args.mod_root)
    mesh_dir = root / "meshes"
    print(f"{'section / mesh file':<50} {'recorded object name':<34} {'verts':>7}")
    print("-" * 95)
    for path in sorted(mesh_dir.glob("*.mesh")):
        try:
            info = mesh_fingerprint(path)
        except ValueError as problem:
            print(f"{path.name:<50} !! {problem}", file=sys.stderr)
            continue
        print(f"{path.stem:<50} {info['object']:<34} {info['verts']:>7}")

    # The runtime reports which renderer slot each source Mesh occupied. That
    # slot name is the game's own, so it names the body region rather than a
    # Mod section, and is what a Blender object name can be matched against.
    if args.log:
        print()
        print("renderer hierarchy reported by the runtime:")
        seen: set[str] = set()
        text = Path(args.log).read_text(encoding="utf-8", errors="replace")
        for line in text.splitlines():
            if "hierarchy=" not in line:
                continue
            values = {k: v for k, v in FIELD.findall(line)}
            hierarchy = values.get("hierarchy", "")
            name = values.get("rendererName") or values.get("name") or ""
            if not hierarchy or hierarchy in seen:
                continue
            seen.add(hierarchy)
            leaf = hierarchy.rsplit("/", 1)[-1]
            if "typhoea" not in hierarchy:
                continue
            print(f"  {leaf:<58} {name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
