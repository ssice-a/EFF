"""Merge several EIEM meshes into one multi-submesh mesh.

This is the offline half of the "sibling meshes share one Renderer" design. The
game collects a Renderer list once per model instance and never rescans it, so
every extra Renderer EIEM has to register is an extra chance to be missed. A
merged mesh collapses N sibling parts into one Mesh with N submeshes, so the
game's own Renderer can carry all of them and only needs one material slot per
part.

Merge rules:
  * Vertices are concatenated unchanged. No welding, no reordering, so every
    input vertex keeps its identity and only gains a constant offset.
  * The joint palette is the ordered union of the input ``bonePaths``. The
    longest palette seeds the order so the common case copies nothing.
  * Each input part becomes exactly one submesh, and therefore one material
    slot. A part that already carried several submeshes is flattened into one;
    per-part material variation is expressed by the key-switch material map, not
    by sub-submesh material slots.
  * Blend shapes are carried through only when every input part declares the
    same channel names in the same order. A mismatched set is a hard error
    rather than a silently wrong face.

Usage:
    python tools/merge_eiem_mesh.py OUTPUT.mesh PART.mesh PART.mesh [...]
    python tools/merge_eiem_mesh.py OUTPUT.mesh --parts LIST.txt
    python tools/merge_eiem_mesh.py OUTPUT.mesh --parts LIST.txt --sections A B C

``--parts`` reads one mesh path per line; blank lines and ``#`` comments are
ignored. ``--sections`` optionally names the mod.ini material section that each
part should use, which is echoed into the generated mod.ini fragment.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import struct
import sys
from pathlib import Path

MAGIC = b"EFFMESH\x00"
COORDINATE_SPACE = "unity-y-up-left-handed"
UV_CHANNELS = 8


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

    def u8(self) -> int:
        return self.take(1)[0]

    def i32(self) -> int:
        return struct.unpack("<i", self.take(4))[0]

    def u32(self) -> int:
        return struct.unpack("<I", self.take(4))[0]

    def f32(self) -> float:
        return struct.unpack("<f", self.take(4))[0]

    def floats(self) -> list[float]:
        count = self.i32()
        if count < 0:
            raise ValueError(f"invalid float array length {count}")
        return list(struct.unpack("<" + "f" * count, self.take(count * 4))) if count else []

    def u32s(self) -> list[int]:
        count = self.i32()
        if count < 0:
            raise ValueError(f"invalid integer array length {count}")
        return list(struct.unpack("<" + "I" * count, self.take(count * 4))) if count else []

    def string(self) -> str:
        length = 0
        shift = 0
        while True:
            byte = self.u8()
            length |= (byte & 0x7F) << shift
            if not byte & 0x80:
                break
            shift += 7
        return self.take(length).decode("utf-8", "replace")


class Writer:
    def __init__(self) -> None:
        self.data = bytearray()

    def raw(self, data: bytes) -> None:
        self.data.extend(data)

    def i32(self, value: int) -> None:
        self.raw(struct.pack("<i", int(value)))

    def u32(self, value: int) -> None:
        self.raw(struct.pack("<I", int(value)))

    def f32(self, value: float) -> None:
        self.raw(struct.pack("<f", float(value)))

    def floats(self, values: list[float]) -> None:
        self.i32(len(values))
        if values:
            self.raw(struct.pack("<" + "f" * len(values), *values))

    def u32s(self, values: list[int]) -> None:
        self.i32(len(values))
        if values:
            self.raw(struct.pack("<" + "I" * len(values), *values))

    def string(self, value: str) -> None:
        data = str(value).encode("utf-8")
        length = len(data)
        while length >= 0x80:
            self.raw(bytes(((length & 0x7F) | 0x80,)))
            length >>= 7
        self.raw(bytes((length,)))
        self.raw(data)


class Mesh:
    """One parsed EFFMESH file, in the exact field order the runtime reads."""

    def __init__(self, path: Path) -> None:
        self.path = path
        reader = Reader(path.read_bytes())
        if reader.take(8) != MAGIC:
            raise ValueError("not an EFFMESH file")
        self.version = reader.i32()
        if self.version not in (2, 3):
            raise ValueError(f"unsupported EFFMESH version {self.version}")
        self.coordinate_space = reader.string()
        self.source = reader.string()
        self.name = reader.string()
        self.vertex_count = reader.i32()
        # Every float channel is length-prefixed, so the on-disk vertex count is
        # redundant. Cross-check it instead of trusting it.
        self.vertices = reader.floats()
        self.normals = reader.floats()
        self.tangents = reader.floats()
        self.colors = reader.floats()
        self.uvs = [reader.floats() for _ in range(UV_CHANNELS)]
        self.indices = reader.u32s()
        submesh_count = reader.i32()
        self.submeshes = [
            (
                reader.i32(),
                reader.u32(),
                reader.u32(),
                reader.u32(),
                reader.u32(),
                reader.u32(),
            )
            for _ in range(max(0, submesh_count))
        ]
        skin_count = reader.i32()
        self.skin = [
            (
                [reader.f32() for _ in range(4)],
                [reader.u32() for _ in range(4)],
            )
            for _ in range(max(0, skin_count))
        ]
        bind_count = reader.i32()
        self.bindposes = [
            list(struct.unpack("<16f", reader.take(64))) for _ in range(max(0, bind_count))
        ]
        self.bone_hashes = reader.u32s()
        self.bone_paths = [reader.string() for _ in range(max(0, reader.i32()))] if self.version >= 3 else []
        blend_vertex_count = reader.i32()
        self.blend_vertices = [
            (
                reader.u32(),
                [reader.f32() for _ in range(3)],
                [reader.f32() for _ in range(3)],
                [reader.f32() for _ in range(3)],
            )
            for _ in range(max(0, blend_vertex_count))
        ]
        blend_frame_count = reader.i32()
        self.blend_frames = [
            (reader.string(), reader.u32(), reader.u32(), reader.u8(), reader.u8(), reader.u8())
            for _ in range(max(0, blend_frame_count))
        ]
        blend_channel_count = reader.i32()
        self.blend_channels = [
            (reader.string(), reader.u32(), reader.u32(), reader.u32())
            for _ in range(max(0, blend_channel_count))
        ]
        self.blend_weights = reader.floats()
        self.additional_vertices = [reader.f32() for _ in range(max(0, reader.i32()) * 3)]
        if reader.pos != len(reader.data):
            raise ValueError("unexpected trailing EFFMESH data")
        self.validate()

    def uv_dimensions(self) -> list[int]:
        """Floats per vertex for each UV channel.

        The channel length is authoritative; the header vertex count is only a
        cross-check. Assuming a fixed stride silently truncates a channel such
        as a 4-component uv2, so the stride is derived here and validated.
        """
        dimensions = []
        for values in self.uvs:
            if not values:
                dimensions.append(0)
                continue
            if len(values) % self.vertex_count:
                raise ValueError(
                    f"UV channel holds {len(values)} floats, which is not a "
                    f"whole number of {self.vertex_count}-vertex entries"
                )
            stride = len(values) // self.vertex_count
            if stride > 4:
                raise ValueError(f"UV channel declares {stride} components per vertex")
            dimensions.append(stride)
        return dimensions

    def validate(self) -> None:
        if self.coordinate_space != COORDINATE_SPACE:
            raise ValueError(
                f"coordinate space {self.coordinate_space!r} is not {COORDINATE_SPACE!r}"
            )
        if len(self.vertices) != self.vertex_count * 3:
            raise ValueError(
                f"vertex array holds {len(self.vertices) // 3} vertices, header says {self.vertex_count}"
            )
        self.uv_dimensions()
        if self.bone_paths and len(self.bone_paths) != len(self.bindposes):
            raise ValueError("bonePaths and bindposes disagree")
        palette = len(self.bindposes) if self.bindposes else len(self.bone_paths)
        used = max((max(bones) for _weights, bones in self.skin), default=-1)
        if used >= palette:
            raise ValueError(
                f"BoneWeight joint {used} is outside the {palette}-joint palette"
            )
        for index in self.indices:
            if index >= self.vertex_count:
                raise ValueError(
                    f"index {index} is outside the {self.vertex_count}-vertex buffer"
                )
        for submesh in self.submeshes:
            _topology, start, count, _base, _first, _vertices = submesh
            if start + count > len(self.indices):
                raise ValueError("submesh index range runs past the index buffer")


def sanitise(name: str) -> str:
    cleaned = re.sub(r"[^0-9A-Za-z_]+", "_", name).strip("_")
    return cleaned or "Part"


def union_palette(meshes: list[Mesh]) -> list[str]:
    ordered: list[str] = []
    seen: set[str] = set()
    # Seed with the longest palette so the dominant part needs no remap at all.
    for mesh in sorted(meshes, key=lambda item: len(item.bone_paths), reverse=True):
        for path in mesh.bone_paths:
            if path not in seen:
                seen.add(path)
                ordered.append(path)
    return ordered


def derive_name(meshes: list[Mesh], explicit: str | None) -> str:
    if explicit:
        return explicit
    stems = [mesh.name or mesh.path.stem for mesh in meshes]
    prefix = os.path.commonprefix(stems).rstrip("_")
    return f"{prefix}_MERGED" if prefix else f"{stems[0]}_MERGED"


def merge(meshes: list[Mesh], name: str, allow_uv_padding: bool = False) -> tuple[bytes, dict]:
    if not meshes:
        raise ValueError("nothing to merge")

    palette = union_palette(meshes)
    palette_index = {path: index for index, path in enumerate(palette)}
    uv_dimensions = [mesh.uv_dimensions() for mesh in meshes]
    # One merged channel cannot hold two different component counts. Narrowing a
    # channel would truncate real data, so the default is to refuse. Widening is
    # safe but must be asked for explicitly, because padding a channel that a
    # shader actually samples changes what the extra parts read.
    uv_mismatches = []
    for channel in range(UV_CHANNELS):
        widths = sorted({dimensions[channel] for dimensions in uv_dimensions})
        if len(widths) > 1:
            uv_mismatches.append((channel, widths))
    if uv_mismatches and not allow_uv_padding:
        detail = ", ".join(f"uv{channel}={widths}" for channel, widths in uv_mismatches)
        raise ValueError(
            f"{detail}; the merged channel needs one width. Pass --pad-uv to widen "
            "each channel to the widest part and zero-fill the rest."
        )
    merged_uv_dimensions = [
        max((dimensions[channel] for dimensions in uv_dimensions), default=0)
        for channel in range(UV_CHANNELS)
    ]
    report: dict = {
        "name": name,
        "parts": [],
        "palette": len(palette),
        "palette_union": len({path for mesh in meshes for path in mesh.bone_paths}),
        "uv_dimensions": merged_uv_dimensions,
        "uv_padded_channels": [channel for channel, _ in uv_mismatches],
    }

    # Blend shapes are all-or-nothing: a part with channels cannot be merged
    # into one without, because the runtime rebuilds the channel table wholesale.
    channel_sets = [tuple(channel[0] for channel in mesh.blend_channels) for mesh in meshes]
    with_shapes = [index for index, channels in enumerate(channel_sets) if channels]
    if with_shapes:
        first = channel_sets[with_shapes[0]]
        for index in with_shapes[1:]:
            if channel_sets[index] != first:
                raise ValueError(
                    "blend shape channels differ between %s and %s; "
                    "remove the morphs or make the channel sets identical"
                    % (meshes[with_shapes[0]].path.name, meshes[index].path.name)
                )

    writer = Writer()
    writer.raw(MAGIC)
    writer.i32(3)
    writer.string(meshes[0].coordinate_space)
    sources = []
    for mesh in meshes:
        if mesh.source and mesh.source not in sources:
            sources.append(mesh.source)
    writer.string(",".join(sources))
    writer.string(name)

    vertices: list[float] = []
    normals: list[float] = []
    tangents: list[float] = []
    colors: list[float] = []
    uvs: list[list[float]] = [[] for _ in range(UV_CHANNELS)]
    indices: list[int] = []
    skin: list[tuple[list[float], list[int]]] = []
    blend_vertices: list[tuple[int, list[float], list[float], list[float]]] = []
    blend_frames: list[tuple[str, int, int, int, int, int]] = []
    blend_channels: list[tuple[str, int, int, int]] = []
    blend_weights: list[float] = []
    submeshes: list[tuple[int, int, int, int, int, int]] = []

    vertex_offset = 0
    for position, mesh in enumerate(meshes):
        part_count = mesh.vertex_count
        # --- geometry, verbatim, at a constant offset -------------------------
        vertices.extend(mesh.vertices)
        normals.extend(mesh.normals)
        tangents.extend(mesh.tangents)
        colors.extend(mesh.colors)
        for channel, values in enumerate(mesh.uvs):
            # A merged channel is a contiguous block per part at the common
            # width, so a narrower part is padded per vertex, not appended as a
            # flat tail. Appending flat would shift every following part.
            width = merged_uv_dimensions[channel]
            part_width = uv_dimensions[position][channel]
            if width == part_width:
                uvs[channel].extend(values)
                continue
            padded = [0.0] * (part_count * width)
            for vertex in range(part_count):
                source = vertex * part_width
                target = vertex * width
                padded[target : target + part_width] = values[
                    source : source + part_width
                ]
            uvs[channel].extend(padded)

        # --- one submesh per part --------------------------------------------
        triangles: list[int] = []
        for _topology, start, count, _base, _first, _vertices in mesh.submeshes:
            triangles.extend(mesh.indices[start : start + count])
        index_start = len(indices)
        indices.extend(index + vertex_offset for index in triangles)
        submeshes.append((0, index_start, len(triangles), 0, vertex_offset, part_count))

        # --- skinning remapped into the union palette -------------------------
        remap = [palette_index[path] for path in mesh.bone_paths] if mesh.bone_paths else None
        for weights, bones in mesh.skin:
            skin.append((list(weights), [remap[bone] for bone in bones] if remap else list(bones)))

        # --- blend shapes, offsets only --------------------------------------
        if mesh.blend_channels:
            frame_base = len(blend_frames)
            for frame_name, first, count, has_normals, has_tangents, has_additional in mesh.blend_frames:
                blend_frames.append(
                    (
                        frame_name,
                        first + vertex_offset,
                        count,
                        has_normals,
                        has_tangents,
                        has_additional,
                    )
                )
            for channel_name, name_hash, first, count in mesh.blend_channels:
                blend_channels.append((channel_name, name_hash, frame_base + first, count))
            blend_weights.extend(mesh.blend_weights)
            for index, vertex, normal, tangent in mesh.blend_vertices:
                blend_vertices.append((index + vertex_offset, list(vertex), list(normal), list(tangent)))

        report["parts"].append(
            {
                "index": position,
                "file": mesh.path.name,
                "name": mesh.name or mesh.path.stem,
                "vertices": part_count,
                "triangles": len(triangles) // 3,
                "palette": len(mesh.bone_paths),
                "palette_remapped": bool(remap) and remap != list(range(len(remap))),
                "blend_channels": len(mesh.blend_channels),
                "source_submeshes": len(mesh.submeshes),
            }
        )
        vertex_offset += part_count

    writer.i32(vertex_offset)
    writer.floats(vertices)
    writer.floats(normals)
    writer.floats(tangents)
    writer.floats(colors)
    for values in uvs:
        writer.floats(values)
    writer.u32s(indices)
    writer.i32(len(submeshes))
    for topology, start, count, base, first, count_vertices in submeshes:
        writer.i32(topology)
        writer.u32(start)
        writer.u32(count)
        writer.u32(base)
        writer.u32(first)
        writer.u32(count_vertices)
    writer.i32(len(skin))
    for weights, bones in skin:
        for value in weights:
            writer.f32(value)
        for value in bones:
            writer.u32(value)
    # The merged bind poses are the union palette in its declared order. Parts
    # exported from one skeleton agree to float precision, so a shared bone is
    # taken from the first part that declares it and the spread is recorded.
    # A materially different matrix means the parts do not share one skin, which
    # is a hard error rather than a silent first-wins.
    bindpose_tolerance = 1e-4
    bindpose_by_path: dict[str, list[float]] = {}
    bindpose_spread = 0.0
    for mesh in meshes:
        for path, matrix in zip(mesh.bone_paths, mesh.bindposes):
            existing = bindpose_by_path.get(path)
            if existing is None:
                bindpose_by_path[path] = matrix
                continue
            worst = max(abs(a - b) for a, b in zip(existing, matrix))
            bindpose_spread = max(bindpose_spread, worst)
            if worst > bindpose_tolerance:
                raise ValueError(
                    "bind pose for %s differs by %.6g between parts (tolerance "
                    "%.0e); these parts do not share one skin"
                    % (path, worst, bindpose_tolerance)
                )
    report["bindpose_spread"] = bindpose_spread
    writer.i32(len(palette))
    for path in palette:
        matrix = bindpose_by_path.get(path)
        if matrix is None:
            raise ValueError(f"palette bone {path} has no bind pose in any part")
        for value in list(matrix)[:16]:
            writer.f32(value)
    # boneHashes are serialized in palette order. Take each bone's hash from the
    # first part that declares it, so the table stays aligned with bonePaths.
    hash_by_path: dict[str, int] = {}
    for mesh in meshes:
        for offset, path in enumerate(mesh.bone_paths):
            if offset < len(mesh.bone_hashes):
                hash_by_path.setdefault(path, mesh.bone_hashes[offset])
    bone_hashes = [hash_by_path[path] for path in palette if path in hash_by_path]
    writer.i32(len(bone_hashes))
    for value in bone_hashes:
        writer.u32(value)
    writer.i32(len(palette))
    for path in palette:
        writer.string(path)
    writer.i32(len(blend_vertices))
    for index, vertex, normal, tangent in blend_vertices:
        writer.u32(index)
        for value in vertex + normal + tangent:
            writer.f32(value)
    writer.i32(len(blend_frames))
    for frame_name, first, count, has_normals, has_tangents, has_additional in blend_frames:
        writer.string(frame_name)
        writer.u32(first)
        writer.u32(count)
        writer.raw(bytes((has_normals, has_tangents, has_additional)))
    writer.i32(len(blend_channels))
    for channel_name, name_hash, first, count in blend_channels:
        writer.string(channel_name)
        writer.u32(name_hash)
        writer.u32(first)
        writer.u32(count)
    writer.floats(blend_weights)
    writer.i32(0)  # additional vertices

    report["vertices"] = vertex_offset
    report["indices"] = len(indices)
    report["submeshes"] = len(submeshes)
    report["bone_hashes"] = len(bone_hashes)
    return bytes(writer.data), report


def read_part_list(path: Path) -> list[str]:
    parts = []
    for line in path.read_text(encoding="utf-8").splitlines():
        stripped = line.split("#", 1)[0].strip()
        if stripped:
            parts.append(stripped)
    return parts


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", help="merged EFFMESH file to write")
    parser.add_argument(
        "parts",
        nargs="*",
        help="part mesh files, in material-slot order (submesh 0, 1, 2, ...)",
    )
    parser.add_argument("--parts", dest="parts_file", help="file listing one mesh path per line")
    parser.add_argument("--name", help="override the merged mesh name")
    parser.add_argument(
        "--sections",
        nargs="*",
        help="mod.ini material section per part, in the same order",
    )
    parser.add_argument("--report", help="write the merge report as JSON to this path")
    parser.add_argument(
        "--pad-uv",
        action="store_true",
        help=(
            "widen every UV channel to the widest part and zero-fill the rest; "
            "required when parts disagree, e.g. uv2 is 2 floats in one group and "
            "4 in another"
        ),
    )
    args = parser.parse_args()

    targets = list(args.parts)
    if args.parts_file:
        targets.extend(read_part_list(Path(args.parts_file)))
    if not targets:
        parser.print_help()
        return 1

    meshes = []
    for target in targets:
        path = Path(target)
        try:
            meshes.append(Mesh(path))
        except Exception as error:  # noqa: BLE001 - report the offending part
            print(f"error: {path}: {error}", file=sys.stderr)
            return 2

    sections = list(args.sections or [])
    if sections and len(sections) != len(meshes):
        print(
            f"error: --sections has {len(sections)} entries for {len(meshes)} parts",
            file=sys.stderr,
        )
        return 2

    name = derive_name(meshes, args.name)
    try:
        payload, report = merge(meshes, name, allow_uv_padding=args.pad_uv)
    except ValueError as error:
        print(f"error: {error}", file=sys.stderr)
        return 3

    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(payload)

    report["output"] = str(output)
    report["bytes"] = len(payload)
    if sections:
        report["material_sections"] = sections

    print(f"wrote {output}")
    print(
        f"  parts={report['submeshes']} vertices={report['vertices']} "
        f"indices={report['indices']} palette={report['palette']}"
    )
    for part in report["parts"]:
        print(
            f"  [{part['index']}] {part['file']}: {part['vertices']}v "
            f"{part['triangles']}t palette={part['palette']}"
            + (" (remapped)" if part["palette_remapped"] else "")
            + (f" shapes={part['blend_channels']}" if part["blend_channels"] else "")
        )
    if sections:
        print("\nmod.ini fragment:")
        print(f"[{sanitise(name)}]")
        print(f"name={name}")
        for index, section in enumerate(sections):
            print(f"submesh.{index}={index}")
        for index, section in enumerate(sections):
            print(f"material.{index}={section}")
    if args.report:
        Path(args.report).write_text(
            json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8"
        )
        print(f"\nreport: {args.report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
