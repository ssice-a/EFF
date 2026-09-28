"""Report a .skeleton's nodes, their tree, and their source flags.

Read-only. Used to decide what a skeleton resource actually declares before
editing one, since the source-flag array is positional and silently wrong if the
node list and the flag list are changed independently.
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

MAGIC = b"EFFSKEL\x00"


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


def read(path: Path) -> dict:
    reader = Reader(path.read_bytes())
    magic = reader.take(8)
    if magic != MAGIC:
        raise ValueError(f"not an EFFSKEL file: {magic!r}")
    version = reader.i32()
    coordinate = reader.string()
    count = reader.i32()
    if not 0 < count <= 200000:
        raise ValueError(f"implausible node count {count}")
    nodes = []
    for _ in range(count):
        node_path = reader.string()
        parent = reader.i32()
        position = struct.unpack("<3f", reader.take(12))
        rotation = struct.unpack("<4f", reader.take(16))
        scale = struct.unpack("<3f", reader.take(12))
        nodes.append({
            "path": node_path,
            "parent": parent,
            "position": position,
            "rotation": rotation,
            "scale": scale,
        })
    palette_count = reader.i32()
    palette = [reader.i32() for _ in range(max(0, palette_count))]
    root = reader.i32()
    source_count = reader.i32()
    flags = list(reader.take(max(0, source_count)))
    trailing = len(reader.data) - reader.pos
    return {
        "version": version,
        "coordinate": coordinate,
        "nodes": nodes,
        "palette": palette,
        "root": root,
        "source_count": source_count,
        "flags": flags,
        "trailing": trailing,
    }


def parent_ok(nodes: list[dict]) -> list[str]:
    problems = []
    for index, node in enumerate(nodes):
        parent = node["parent"]
        if parent < -1 or parent >= len(nodes):
            problems.append(f"[{index}] parent {parent} is out of range")
        elif parent == index:
            problems.append(f"[{index}] parent points at itself")
    return problems


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("skeleton")
    parser.add_argument("--match", help="only show nodes whose path contains this")
    parser.add_argument("--tree", action="store_true", help="show parent chain")
    args = parser.parse_args()

    path = Path(args.skeleton)
    data = read(path)
    nodes, flags = data["nodes"], data["flags"]

    print(f"file        {path}")
    print(f"version     {data['version']}")
    print(f"coordinate  {data['coordinate']}")
    print(f"nodes       {len(nodes)}")
    print(f"palette     {len(data['palette'])}")
    print(f"root        {data['root']}")
    print(f"sourceCount {data['source_count']}  (flags {len(flags)})")
    print(f"trailing    {data['trailing']} bytes")
    if data["source_count"] != len(nodes):
        print("WARNING: sourceCount does not equal node count")
    if len(flags) != len(nodes):
        print("WARNING: flag array length does not equal node count")
    problems = parent_ok(nodes)
    for problem in problems[:10]:
        print("PROBLEM " + problem)

    children: dict[int, int] = {}
    for node in nodes:
        if node["parent"] >= 0:
            children[node["parent"]] = children.get(node["parent"], 0) + 1

    source_true = sum(1 for flag in flags if flag)
    print(f"source=true {source_true}   source=false {len(flags) - source_true}")

    if args.match:
        hits = [i for i, node in enumerate(nodes) if args.match in node["path"]]
        print(f"\n--- {len(hits)} node(s) matching {args.match!r} ---")
        for index in hits:
            node = nodes[index]
            flag = flags[index] if index < len(flags) else -1
            leaf = node["path"].rsplit("/", 1)[-1]
            parent_path = (
                nodes[node["parent"]]["path"] if 0 <= node["parent"] < len(nodes) else "<root>"
            )
            print(f"[{index:4d}] source={flag} children={children.get(index, 0)}")
            print(f"        leaf   {leaf}")
            if args.tree:
                print(f"        parent {parent_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
