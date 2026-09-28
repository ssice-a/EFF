"""Remove generated-object nodes from an EFFSKEL skeleton resource.

The physics skeleton export walks the authored physics hierarchy, which contains
objects the physics runtime creates rather than objects the game ships: capsule
collider GameObjects parented under real bones. Those are exported as source
nodes, so the resource claims the game already has them. It does not, so a
strict resolution fails on them and a permissive one fabricates a private bone
for each, and a fabricated bone under a weighted section is exactly what leaves
that section in its bind pose.

Colliders are not bones. A mesh's bonePath for `.../Bip001_L_Thigh` never equals
`.../Bip001_L_Thigh/Magica Capsule Collider (Bip001_L_Thigh)`, so no skin binding
depends on them; they exist only to give the physics data a collider root, which
is carried by the .physics file instead.

This rewrites the node list without them and keeps the two positional structures
consistent: parent indices are remapped, the source-flag array is rebuilt to the
new length, and the file is re-emitted in the same layout with the node count
updated. All removed nodes must be leaves: dropping an interior node would orphan
its subtree.

Usage:
    python tools/strip_skeleton_nodes.py SKELETON --match 'Magica Capsule Collider' \\
        --output SKELETON.stripped [--in-place] [--dry-run]
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from inspect_eiem_skeleton import MAGIC, read  # noqa: E402


def encode_string(value: str) -> bytes:
    data = value.encode("utf-8")
    length = len(data)
    out = bytearray()
    while length >= 0x80:
        out.append((length & 0x7F) | 0x80)
        length >>= 7
    out.append(length)
    out.extend(data)
    return bytes(out)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("skeleton")
    parser.add_argument("--match", required=True,
                        help="substring identifying nodes to remove")
    parser.add_argument("--output", help="write here instead of in place")
    parser.add_argument("--in-place", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    path = Path(args.skeleton)
    if not args.output and not args.in_place:
        print("error: pass --output or --in-place", file=sys.stderr)
        return 1

    data = read(path)
    nodes = data["nodes"]
    flags = data["flags"]

    children: dict[int, int] = {}
    for node in nodes:
        if node["parent"] >= 0:
            children[node["parent"]] = children.get(node["parent"], 0) + 1

    removed = [i for i, node in enumerate(nodes) if args.match in node["path"]]
    if not removed:
        print(f"nothing matching {args.match!r}; skeleton unchanged")
        return 0

    interior = [i for i in removed if children.get(i, 0) > 0]
    if interior:
        print(f"error: {len(interior)} matched node(s) have children; "
              f"refusing to orphan their subtrees: {interior[:5]}", file=sys.stderr)
        return 2

    removed_set = set(removed)
    kept = [i for i in range(len(nodes)) if i not in removed_set]
    new_index = {old: new for new, old in enumerate(kept)}

    print(f"nodes {len(nodes)} -> {len(kept)} (removing {len(removed)})")
    for index in removed[:5]:
        print(f"  remove [{index}] {nodes[index]['path'].rsplit('/', 1)[-1]}")
    if len(removed) > 5:
        print(f"  ... and {len(removed) - 5} more")

    out = bytearray()
    out += MAGIC
    out += struct.pack("<i", data["version"])
    out += encode_string(data["coordinate"])
    out += struct.pack("<i", len(kept))
    for index in kept:
        node = nodes[index]
        out += encode_string(node["path"])
        parent = node["parent"]
        # A removed parent is impossible here (removed nodes are leaves), so every
        # surviving parent is itself surviving and has a new index.
        new_parent = new_index[parent] if parent >= 0 else -1
        out += struct.pack("<i", new_parent)
        out += struct.pack("<3f", *node["position"])
        out += struct.pack("<4f", *node["rotation"])
        out += struct.pack("<3f", *node["scale"])
    out += struct.pack("<i", 0)          # palette count
    out += struct.pack("<i", data["root"])
    out += struct.pack("<i", len(kept))  # source flag count follows the node count
    out += bytes(flags[index] for index in kept)

    if args.dry_run:
        print(f"dry run: would write {len(out)} bytes")
        return 0

    target = Path(args.output) if args.output else path
    if target.exists() and not args.output:
        backup = target.with_suffix(target.suffix + ".bak")
        backup.write_bytes(path.read_bytes())
        print(f"backup: {backup}")
    target.write_bytes(bytes(out))
    print(f"wrote {target}: {len(out)} bytes")

    # Re-read to prove the rewrite is self-consistent.
    check = read(target)
    print(f"verify: nodes={len(check['nodes'])} flags={len(check['flags'])} "
          f"sourceCount={check['source_count']} trailing={check['trailing']}")
    remaining = [n for n in check["nodes"] if args.match in n["path"]]
    print(f"verify: {len(remaining)} node(s) still match {args.match!r}")
    if remaining or len(check["nodes"]) != len(kept) or \
            len(check["flags"]) != len(kept) or check["trailing"] != 0:
        print("error: verification failed", file=sys.stderr)
        return 3
    print("OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
