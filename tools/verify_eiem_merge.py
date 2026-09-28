"""Prove an EFFMESH merge is lossless by slicing each part back out.

The merge tool writes vertices verbatim at a constant offset, so the inverse
operation is exact: part P occupies ``[offset, offset + vertexCount)`` and its
submesh indexes that range. This script re-derives every part from the merged
file and compares it field by field against the original part file. Any
mismatch means the merge corrupted geometry, skin weights, or the joint palette.

Usage:
    python tools/verify_eiem_merge.py MERGED.mesh PART.mesh PART.mesh [...]
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from merge_eiem_mesh import Mesh  # noqa: E402


def compare(
    merged: Mesh, part: Mesh, offset: int, submesh_index: int, uv_padded: bool = False
) -> list[str]:
    problems: list[str] = []
    span = part.vertex_count

    def expect(label: str, actual, expected) -> None:
        if actual == expected:
            return
        # Report sizes rather than dumping two multi-megabyte float lists.
        if isinstance(actual, list) and isinstance(expected, list):
            problems.append(
                f"{label}: merged has {len(actual)} values, part has {len(expected)}"
            )
        else:
            problems.append(f"{label}: merged has {actual!r}, part has {expected!r}")

    def same(label: str, actual: list, expected: list) -> None:
        if len(actual) != len(expected):
            problems.append(
                f"{label}: merged has {len(actual)} values, part has {len(expected)}"
            )
            return
        for offset_in_values, (left, right) in enumerate(zip(actual, expected)):
            if left == right:
                continue
            problems.append(
                f"{label}: first difference at element {offset_in_values} "
                f"({left!r} vs {right!r})"
            )
            return

    # --- geometry, channel by channel ---------------------------------------
    for label, stride in (("vertices", 3), ("normals", 3), ("tangents", 4), ("colors", 4)):
        merged_values = getattr(merged, label)[offset * stride : (offset + span) * stride]
        same(label, merged_values, getattr(part, label))
    # UV channels are not fixed at two components; compare each channel at its
    # own declared width. When the merge widened a channel, the extra components
    # must be exactly zero -- that is the contract -- so they are checked rather
    # than skipped, and the part's own values must survive unchanged.
    part_uv = part.uv_dimensions()
    merged_uv = merged.uv_dimensions()
    for channel in range(8):
        width = merged_uv[channel]
        part_width = part_uv[channel]
        if width == 0 and part_width == 0:
            continue
        merged_values = merged.uvs[channel][offset * width : (offset + span) * width]
        same(f"uv{channel} width", [len(merged_values)], [span * width])
        expected = list(part.uvs[channel])
        if width == part_width:
            same(f"uv{channel}", merged_values, expected)
            continue
        if width < part_width:
            problems.append(
                f"uv{channel}: merged is {width} components, part needs {part_width}"
            )
            continue
        # Merged is wider: check the real components and the zero padding.
        trimmed: list[float] = []
        padding: list[float] = []
        for vertex in range(span):
            start = vertex * width
            trimmed.extend(merged_values[start : start + part_width])
            padding.extend(merged_values[start + part_width : start + width])
        same(f"uv{channel} values", trimmed, expected)
        if not uv_padded:
            problems.append(
                f"uv{channel}: merged is wider ({width} vs {part_width}) but the "
                "merge did not declare UV padding"
            )
        elif any(value != 0.0 for value in padding):
            non_zero = next(value for value in padding if value != 0.0)
            problems.append(f"uv{channel} padding is not zero (found {non_zero!r})")

    # --- indices, re-based back to part-local vertex numbers -----------------
    topology, start, count, _base, first, vertices = merged.submeshes[submesh_index]
    expect(f"submesh{submesh_index}.firstVertex", first, offset)
    expect(f"submesh{submesh_index}.vertexCount", vertices, span)
    expect(f"submesh{submesh_index}.topology", topology, 0)
    merged_indices = merged.indices[start : start + count]
    expect(f"submesh{submesh_index}.indexCount", count, len(part.indices))
    same(f"submesh{submesh_index}.indices", [value - offset for value in merged_indices], part.indices)

    # --- skin weights, mapping the merged joint index back to the part's ----
    # ``palette_index`` is part slot -> merged slot, which is the direction the
    # merge writes. Validation needs the opposite direction, so invert it.
    palette_index = {path: index for index, path in enumerate(merged.bone_paths)}
    inverse = {
        palette_index[path]: index for index, path in enumerate(part.bone_paths)
    }
    merged_skin = merged.skin[offset : offset + span]
    expect("skin length", len(merged_skin), len(part.skin))
    for position, ((merged_weights, merged_bones), (weights, bones)) in enumerate(
        zip(merged_skin, part.skin)
    ):
        if merged_weights != weights:
            problems.append(f"skin[{position}] weights differ")
            break
        # A merged joint indexes the merged palette, which is at least as large
        # as the part's own. Every joint the part actually uses must map back to
        # a slot the part declares.
        outside = [bone for bone in merged_bones if bone >= len(merged.bone_paths)]
        if outside:
            problems.append(
                f"skin[{position}] joint {outside[0]} is outside the merged "
                f"{len(merged.bone_paths)}-joint palette"
            )
            break
        unmapped = [bone for bone in merged_bones if bone not in inverse]
        if unmapped:
            problems.append(
                f"skin[{position}] joint {unmapped[0]} "
                f"({merged.bone_paths[unmapped[0]]}) is not in the part's palette"
            )
            break
        if [inverse[bone] for bone in merged_bones] != bones:
            problems.append(f"skin[{position}] joints differ")
            break

    # --- skeleton assets, remapped through the part's own palette -----------
    # Parts exported from one skeleton agree on bind poses only to float
    # precision, so this checks the same tolerance the merge enforces instead of
    # demanding bit equality.
    for index, path in enumerate(part.bone_paths):
        merged_index = palette_index[path]
        worst = max(
            abs(a - b)
            for a, b in zip(merged.bindposes[merged_index], part.bindposes[index])
        )
        if worst > 1e-4:
            problems.append(f"bindpose[{path}] differs by {worst:.6g}")
            break
    return problems


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("merged", help="merged EFFMESH file")
    parser.add_argument("parts", nargs="+", help="original part files in submesh order")
    parser.add_argument(
        "--pad-uv",
        action="store_true",
        help="the merged file was written with widened, zero-filled UV channels",
    )
    args = parser.parse_args()

    merged = Mesh(Path(args.merged))
    if len(merged.submeshes) != len(args.parts):
        print(
            f"error: merged mesh has {len(merged.submeshes)} submeshes "
            f"for {len(args.parts)} parts",
            file=sys.stderr,
        )
        return 2

    offset = 0
    failed = 0
    for position, target in enumerate(args.parts):
        part = Mesh(Path(target))
        problems = compare(merged, part, offset, position, args.pad_uv)
        status = "OK" if not problems else f"FAIL ({len(problems)})"
        print(f"[{position}] {part.path.name}: {status}")
        for problem in problems[:10]:
            print(f"      {problem}")
        if problems:
            failed += 1
        offset += part.vertex_count

    if offset != merged.vertex_count:
        print(
            f"error: parts total {offset} vertices, merged declares {merged.vertex_count}",
            file=sys.stderr,
        )
        failed += 1

    palette_paths = [path for part in args.parts for path in Mesh(Path(part)).bone_paths]
    missing = sorted(set(palette_paths) - set(merged.bone_paths))
    if missing:
        print(f"error: merged palette is missing {len(missing)} bones", file=sys.stderr)
        failed += 1

    print()
    if failed:
        print(f"{failed} part(s) FAILED")
        return 1
    print(f"lossless: {len(args.parts)} parts, {merged.vertex_count} vertices, "
          f"{len(merged.bone_paths)} bones")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
