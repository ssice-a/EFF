"""List the bones a mod Mesh carries that the game's own mesh for the same slot does not.

The game prefab records each source Renderer's bone count. A mod Mesh whose
bonePaths count differs cannot be skinned against that Renderer: Unity requires
one bindpose per bone slot, so a palette of the wrong length falls back to the
bind pose, which renders as a T-pose.

This compares a mod Mesh's bone paths against the game's own exported Mesh for
the same asset when one is available, and otherwise just reports the paths so
the extras can be identified by name.

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

    def skip_floats(self) -> None:
        count = self.i32()
        self.take(count * 4)


def read_paths(path: Path) -> list[str]:
    reader = Reader(path.read_bytes())
    if reader.take(8) != b"EFFMESH\x00":
        raise ValueError("not an EFFMESH container")
    version = reader.i32()
    reader.string()   # coordinate
    reader.string()   # source
    reader.string()   # name
    reader.i32()      # vertexCount
    for _ in range(4):
        reader.skip_floats()
    for _ in range(8):
        reader.skip_floats()
    reader.take(reader.i32() * 4)   # indices
    reader.take(reader.i32() * 24)  # submeshes
    reader.take(reader.i32() * 32)  # skin
    reader.take(reader.i32() * 64)  # bindposes
    reader.take(reader.i32() * 4)   # bone hashes
    if version < 3:
        return []
    return [reader.string() for _ in range(reader.i32())]


def tail(path: str) -> str:
    return path.rsplit("/", 1)[-1]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("meshes", nargs="+")
    parser.add_argument("--against", help="a mesh whose palette is the baseline")
    args = parser.parse_args()

    baseline: set[str] = set()
    if args.against:
        baseline = set(read_paths(Path(args.against)))
        print(f"baseline {Path(args.against).name}: {len(baseline)} bones")
        print()

    for item in args.meshes:
        path = Path(item)
        try:
            paths = read_paths(path)
        except ValueError as problem:
            print(f"{path.name}: {problem}", file=sys.stderr)
            continue
        print(f"{path.name}: {len(paths)} bones")
        if baseline:
            extra = [p for p in paths if p not in baseline]
            missing = [p for p in baseline if p not in set(paths)]
            print(f"    extra  ({len(extra)}):")
            for path_value in extra:
                print(f"      + {path_value}")
            if missing:
                print(f"    missing({len(missing)}):")
                for path_value in missing[:8]:
                    print(f"      - {path_value}")
        else:
            for path_value in paths:
                print(f"      {path_value}")
        print()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
