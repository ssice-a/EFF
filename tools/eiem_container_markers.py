"""Read the authoring markers of every EIEM container in a mod.

The container records the coordinate space it was authored in as metadata. The
runtime does not interpret it, but a mismatch means the vertices were written in
a different basis than the skeleton and bindposes, which orients the mesh
wrongly regardless of whether it has any skin weights at all.

Also reports the source asset each mesh claims, so a mesh can be traced back to
the game asset it replaces.

Read-only.
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
        return self.take(length).decode("utf-8", errors="replace")


def markers(path: Path) -> dict:
    blob = path.read_bytes()
    reader = Reader(blob)
    magic = reader.take(8)
    if magic == b"EFFMESH\x00":
        version = reader.i32()
        return {
            "kind": "mesh",
            "version": version,
            "coordinate": reader.string(),
            "source": reader.string(),
            "name": reader.string(),
            "vertexCount": reader.i32(),
        }
    if magic == b"EFFSKEL\x00":
        version = reader.i32()
        return {
            "kind": "skeleton",
            "version": version,
            "coordinate": reader.string(),
            "nodes": reader.i32(),
        }
    if magic.startswith(b"EFFPHYS"):
        return {"kind": "physics", "magic": magic.decode("ascii", "replace")}
    return {"kind": "unknown", "magic": repr(magic)}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("roots", nargs="+")
    args = parser.parse_args()

    files: list[Path] = []
    for item in args.roots:
        root = Path(item)
        if root.is_dir():
            files.extend(sorted(p for p in root.rglob("*") if p.is_file()))
        else:
            files.append(root)

    coordinates: dict[str, int] = {}
    rows = []
    for path in files:
        if path.suffix.lower() not in (".mesh", ".skeleton", ".physics"):
            continue
        try:
            info = markers(path)
        except ValueError as problem:
            print(f"{path.name}: {problem}", file=sys.stderr)
            continue
        if info["kind"] == "mesh":
            coordinates[info["coordinate"]] = coordinates.get(info["coordinate"], 0) + 1
        rows.append((path, info))

    print(f"{'file':<50} {'kind':<9} {'ver':>4} {'coordinate':<26} {'source/name'}")
    print("-" * 120)
    for path, info in rows:
        coordinate = info.get("coordinate", info.get("magic", ""))
        detail = info.get("source") or info.get("name") or ""
        if len(detail) > 44:
            detail = "..." + detail[-41:]
        print(f"{path.name:<50} {info['kind']:<9} {info.get('version', 0):>4} "
              f"{coordinate:<26} {detail}")

    print()
    print("coordinate markers seen on meshes:")
    for coordinate, count in sorted(coordinates.items(), key=lambda kv: -kv[1]):
        flag = "" if coordinate == "unity-y-up-left-handed" else "   <-- NOT the native basis"
        print(f"  {count:>4}  {coordinate!r}{flag}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
