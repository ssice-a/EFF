"""Create a deliberately reduced EIEM mesh for runtime replacement tests."""

import struct
import sys
from pathlib import Path


MAGIC = b"EFFMESH\0"


class Reader:
    def __init__(self, data):
        self.data = data
        self.pos = 0

    def take(self, n):
        if self.pos + n > len(self.data):
            raise ValueError("truncated EIEM mesh")
        value = self.data[self.pos:self.pos + n]
        self.pos += n
        return value

    def i32(self):
        return struct.unpack("<i", self.take(4))[0]

    def u32(self):
        return struct.unpack("<I", self.take(4))[0]

    def f32(self):
        return struct.unpack("<f", self.take(4))[0]

    def string(self):
        length = 0
        shift = 0
        while True:
            byte = self.take(1)[0]
            length |= (byte & 0x7f) << shift
            if not byte & 0x80:
                break
            shift += 7
        return self.take(length).decode("utf-8", "replace")

    def floats(self):
        count = self.i32()
        if count < 0:
            raise ValueError("invalid float array")
        return list(struct.unpack("<" + "f" * count, self.take(count * 4))) if count else []


class Writer:
    def __init__(self):
        self.data = bytearray()

    def raw(self, data):
        self.data.extend(data)

    def i32(self, value):
        self.raw(struct.pack("<i", int(value)))

    def u32(self, value):
        self.raw(struct.pack("<I", int(value)))

    def f32(self, value):
        self.raw(struct.pack("<f", float(value)))

    def string(self, value):
        data = str(value).encode("utf-8")
        length = len(data)
        while length >= 0x80:
            self.raw(bytes(((length & 0x7f) | 0x80,)))
            length >>= 7
        self.raw(bytes((length,)))
        self.raw(data)

    def floats(self, values):
        self.i32(len(values))
        if values:
            self.raw(struct.pack("<" + "f" * len(values), *values))


def read_mesh(path):
    reader = Reader(path.read_bytes())
    if reader.take(8) != MAGIC:
        raise ValueError("not an EIEM mesh")
    version = reader.i32()
    coordinate = reader.string()
    source = reader.string()
    name = reader.string()
    vertex_count = reader.i32()
    arrays = [reader.floats() for _ in range(4)]
    uvs = [reader.floats() for _ in range(8)]
    index_count = reader.i32()
    indices = [reader.u32() for _ in range(index_count)]
    submesh_count = reader.i32()
    submeshes = [tuple((reader.i32(), reader.u32(), reader.u32(), reader.u32(),
                        reader.u32(), reader.u32())) for _ in range(submesh_count)]
    skin_count = reader.i32()
    skin = [(tuple(reader.f32() for _ in range(4)), tuple(reader.u32() for _ in range(4)))
            for _ in range(skin_count)]
    bind_count = reader.i32()
    bindposes = [tuple(reader.f32() for _ in range(16)) for _ in range(bind_count)]
    bone_hash_count = reader.i32()
    bone_hashes = [reader.u32() for _ in range(bone_hash_count)]
    # Preserve blend-shape bytes structurally; their vertex indices are filtered below.
    blend_vertex_count = reader.i32()
    blend_vertices = [(reader.u32(), tuple(reader.f32() for _ in range(9)))
                      for _ in range(blend_vertex_count)]
    blend_frame_count = reader.i32()
    blend_frames = []
    for _ in range(blend_frame_count):
        blend_frames.append((reader.string(), reader.u32(), reader.u32(),
                             reader.take(1)[0], reader.take(1)[0], reader.take(1)[0]))
    blend_channel_count = reader.i32()
    blend_channels = [(reader.string(), reader.u32(), reader.u32(), reader.u32())
                      for _ in range(blend_channel_count)]
    blend_weights = reader.floats()
    additional_count = reader.i32()
    additional = [tuple(reader.f32() for _ in range(3)) for _ in range(additional_count)]
    return locals()


def write_mesh(path, mesh, keep):
    vertices, normals, tangents, colors = mesh["arrays"]
    def trim(values, components):
        return values[:keep * components] if values else []

    filtered_indices = []
    submeshes = []
    for topology, start, count, _base, _first, _count in mesh["submeshes"]:
        original = mesh["indices"][start:start + count]
        selected = []
        if topology == 0:
            for i in range(0, len(original) - 2, 3):
                tri = original[i:i + 3]
                if all(index < keep for index in tri):
                    selected.extend(tri)
        else:
            selected = [index for index in original if index < keep]
        index_start = len(filtered_indices)
        filtered_indices.extend(selected)
        submeshes.append((topology, index_start, len(selected), 0, 0, keep))

    writer = Writer()
    writer.raw(MAGIC)
    writer.i32(2)
    writer.string(mesh["coordinate"])
    writer.string(mesh["source"])
    writer.string(mesh["name"] + "_half")
    writer.i32(keep)
    writer.floats(trim(vertices, 3)); writer.floats(trim(normals, 3))
    writer.floats(trim(tangents, 4)); writer.floats(trim(colors, 4))
    for uv in mesh["uvs"]:
        dimensions = len(uv) // mesh["vertex_count"] if mesh["vertex_count"] else 0
        writer.floats(trim(uv, dimensions) if dimensions else [])
    writer.i32(len(filtered_indices))
    for index in filtered_indices:
        writer.u32(index)
    writer.i32(len(submeshes))
    for values in submeshes:
        writer.i32(values[0]); writer.u32(values[1]); writer.u32(values[2])
        writer.u32(values[3]); writer.u32(values[4]); writer.u32(values[5])
    writer.i32(min(len(mesh["skin"]), keep))
    for weights, bones in mesh["skin"][:keep]:
        for value in weights: writer.f32(value)
        for value in bones: writer.u32(value)
    writer.i32(len(mesh["bindposes"]))
    for source_values in mesh["bindposes"]:
        # Version 1 test files stored AnimeStudio's raw serialized order.
        # Version 2 stores the managed Unity Matrix4x4 field order used by
        # Mesh.bindposes.  New v2 sources are already canonical.
        values = ([source_values[row * 4 + column]
                   for column in range(4) for row in range(4)]
                  if mesh["version"] == 1 else source_values)
        for value in values: writer.f32(value)
    writer.i32(len(mesh["bone_hashes"]))
    for value in mesh["bone_hashes"]: writer.u32(value)
    # Blend-shape deltas reference the original vertex index space. Drop the
    # optional morph data in this visual test rather than leaving stale frame
    # ranges that could point past the reduced vertex buffer.
    writer.i32(0); writer.i32(0); writer.i32(0); writer.floats([]); writer.i32(0)
    path.write_bytes(writer.data)


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: trim_eiem_mesh.py input.mesh output.mesh")
    source, target = map(Path, sys.argv[1:])
    mesh = read_mesh(source)
    mesh["arrays"] = mesh["arrays"]
    keep = max(1, mesh["vertex_count"] // 2)
    write_mesh(target, mesh, keep)
    print(f"wrote {target}: vertices {mesh['vertex_count']} -> {keep}")


if __name__ == "__main__":
    main()
