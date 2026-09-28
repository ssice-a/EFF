"""Check every mod Mesh's bone paths against the Skeleton it is declared with.

A SkinnedMeshRenderer's palette is per-Renderer and may use any subset of the
skeleton, so the only requirement is that each of the Mesh's bone paths resolves
to a node the Skeleton declares. This reports that per Mesh, plus which bone
paths only exist as Mod-private nodes (declared source=false, so the game does
not have them and nothing animates them).

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
        self.take(self.i32() * 4)


def read_mesh(path: Path) -> list[str]:
    reader = Reader(path.read_bytes())
    if reader.take(8) != b"EFFMESH\x00":
        raise ValueError("not an EFFMESH container")
    version = reader.i32()
    reader.string()
    reader.string()
    reader.string()
    reader.i32()
    for _ in range(4):
        reader.skip_floats()
    for _ in range(8):
        reader.skip_floats()
    reader.take(reader.i32() * 4)
    reader.take(reader.i32() * 24)
    reader.take(reader.i32() * 32)
    reader.take(reader.i32() * 64)
    reader.take(reader.i32() * 4)
    if version < 3:
        return []
    return [reader.string() for _ in range(reader.i32())]


def read_skeleton(path: Path) -> dict[str, bool]:
    reader = Reader(path.read_bytes())
    if reader.take(8) != b"EFFSKEL\x00":
        raise ValueError("not an EFFSKEL container")
    version = reader.i32()
    reader.string()
    nodes: list[str] = []
    for _ in range(reader.i32()):
        nodes.append(reader.string())
        reader.take(4)
        reader.take(40)
    # paletteCount is asserted zero, then root is asserted -1, and only then
    # does the v2 flag array appear. Reading the count straight after the node
    # list lands on paletteCount and shifts every flag, which silently marks
    # game-owned bones as Mod-private.
    if reader.i32() != 0:
        raise ValueError("unexpected non-zero palette count")
    if reader.i32() != -1:
        raise ValueError("unexpected root index")
    if version != 2:
        return {name: True for name in nodes}
    flag_count = reader.i32()
    if flag_count != len(nodes):
        raise ValueError(f"flag count {flag_count} != node count {len(nodes)}")
    return {name: reader.take(1)[0] != 0 for name in nodes}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("skeleton")
    parser.add_argument("meshes", nargs="+")
    args = parser.parse_args()

    skeleton = read_skeleton(Path(args.skeleton))
    total = len(skeleton)
    game_bones = sum(1 for flag in skeleton.values() if flag)
    print(f"skeleton {Path(args.skeleton).name}: {total} nodes "
          f"({game_bones} source=true, {total - game_bones} source=false)")
    print()
    header = (f"{'mesh':<46} {'palette':>7} {'unresolved':>10} {'privateOnly':>11}")
    print(header)
    print("-" * len(header))
    worst: list[tuple[str, list[str]]] = []
    for item in args.meshes:
        path = Path(item)
        try:
            paths = read_mesh(path)
        except ValueError as problem:
            print(f"{path.name:<46} !! {problem}", file=sys.stderr)
            continue
        unresolved = [p for p in paths if p not in skeleton]
        private = [p for p in paths if skeleton.get(p) is False]
        print(f"{path.name:<46} {len(paths):>7} {len(unresolved):>10} {len(private):>11}")
        if unresolved or private:
            worst.append((path.name, unresolved or private))
    print()
    if worst:
        print("meshes whose palette includes bones the game does not have:")
        for name, paths in worst:
            print(f"  {name}: {len(paths)} path(s)")
            for value in paths[:4]:
                print(f"      {value}")
            if len(paths) > 4:
                print(f"      ... {len(paths) - 4} more")
    else:
        print("every mesh resolves entirely against game-owned bones")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
