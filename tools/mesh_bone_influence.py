"""Count, per bone, how many vertices carry a non-zero weight on it.

An earlier report claimed the skirt bones carry no vertices. That measured the
wrong thing: it counted vertices whose weights do not sum to one, which is a
different question. This counts per-bone influence directly, so a bone that has
topology flowing through it cannot be mistaken for an unused slot.

Read-only.
"""
from __future__ import annotations

import argparse
import struct
import sys
from collections import defaultdict
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

    def skip_floats(self) -> None:
        self.take(self.i32() * 4)


def load(path: Path):
    reader = Reader(path.read_bytes())
    if reader.take(8) != b"EFFMESH\x00":
        raise ValueError("not an EFFMESH container")
    version = reader.i32()
    reader.string()
    reader.string()
    reader.string()
    vertex_count = reader.i32()
    for _ in range(4):
        reader.skip_floats()
    for _ in range(8):
        reader.skip_floats()
    reader.take(reader.i32() * 4)
    reader.take(reader.i32() * 24)
    skin = [struct.unpack("<4f4i", reader.take(32))
            for _ in range(reader.i32())]
    reader.take(reader.i32() * 64)
    reader.take(reader.i32() * 4)
    paths = [reader.string() for _ in range(reader.i32())] if version >= 3 else []
    return vertex_count, skin, paths


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mesh")
    parser.add_argument("--match", help="only report bones whose path contains this")
    args = parser.parse_args()

    vertex_count, skin, paths = load(Path(args.mesh))
    influence: dict[int, int] = defaultdict(int)
    for entry in skin:
        weights, indices = entry[:4], entry[4:]
        for weight, index in zip(weights, indices):
            if weight != 0.0 and 0 <= index < len(paths):
                influence[index] += 1

    print(f"{Path(args.mesh).name}: {vertex_count} vertices, {len(paths)} bones, "
          f"{len(skin)} skin entries")
    referenced = sum(1 for index in influence if influence[index] > 0)
    print(f"bones with at least one weighted vertex: {referenced}")
    print(f"bones with no weighted vertex          : {len(paths) - referenced}")
    print()

    rows = [
        (index, paths[index], influence.get(index, 0))
        for index in range(len(paths))
        if influence.get(index, 0) > 0
    ]
    if args.match:
        rows = [row for row in rows if args.match in row[1]]
    rows.sort(key=lambda row: -row[2])
    for index, path, count in rows[:40]:
        short = "/".join(path.split("/")[-2:]) if path.count("/") >= 1 else path
        print(f"  {count:>6} verts  [{index:>3}] {short}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
