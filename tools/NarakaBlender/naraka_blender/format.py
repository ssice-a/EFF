"""Read AnimeStudio's native EFFMESH v6 payload and Naraka Prefab manifest.

The geometry layout is shared with EFF; no FBX/OBJ conversion is involved.
This module has no Blender dependency.
"""
import hashlib
import json
import math
import struct
from pathlib import Path


class Reader:
    def __init__(self, data):
        self.data, self.pos = data, 0

    def take(self, count):
        if count < 0 or count > len(self.data) - self.pos:
            raise ValueError("Truncated geometry payload")
        result = self.data[self.pos:self.pos + count]
        self.pos += count
        return result

    def i32(self):
        return struct.unpack("<i", self.take(4))[0]

    def u32(self):
        return struct.unpack("<I", self.take(4))[0]

    def count(self):
        value = self.i32()
        if value < 0 or value > len(self.data):
            raise ValueError("Invalid geometry array length")
        return value

    def string(self):
        count = 0
        for shift in range(0, 35, 7):
            byte = self.take(1)[0]
            count |= (byte & 127) << shift
            if byte < 128:
                return self.take(count).decode("utf-8")
        raise ValueError("Invalid string length")

    def array(self, code, count=None):
        count = self.count() if count is None else count
        return list(struct.unpack("<%d%s" % (count, code), self.take(struct.calcsize(code) * count)))


def safe_path(root, relative):
    root = Path(root).resolve()
    path = (root / relative).resolve()
    if not path.is_relative_to(root):
        raise ValueError("Package path escapes its directory: " + relative)
    return path


def load_package(path, verify=True):
    path = Path(path)
    if path.is_dir():
        path /= "naraka.json"
    root = path.parent.resolve()
    data = json.loads(path.read_text(encoding="utf-8-sig"))
    if data.get("format") != "NARAKA_PREFAB_PACKAGE" or data.get("version") != 1:
        raise ValueError("Unsupported Naraka Prefab package")
    if data.get("coordinate") != "unity-y-up-left-handed":
        raise ValueError("Unknown coordinate space")
    if verify:
        checksums = json.loads((root / "checksums.json").read_text(encoding="utf-8-sig"))
        for name, expected in checksums.items():
            if hashlib.sha256(safe_path(root, name).read_bytes()).hexdigest().lower() != expected.lower():
                raise ValueError("Package checksum mismatch: " + name)
        for name in ["naraka.json"] + [r["mesh"] for r in data["renderers"]] + [p for t in data["textures"].values() for p in t.get("slices", [t["path"]])]:
            if name not in checksums:
                raise ValueError("Unverified package file: " + name)
    for index, node in enumerate(data["nodes"]):
        if node["id"] != index or node["parent"] < -1 or node["parent"] >= index:
            raise ValueError("Invalid hierarchy node/parent")
        if not all(math.isfinite(v) for field in ("position", "rotation", "scale") for v in node[field]):
            raise ValueError("Non-finite hierarchy transform")
    for renderer in data["renderers"]:
        if not 0 <= renderer["node"] < len(data["nodes"]):
            raise ValueError("Renderer has no hierarchy node")
        for reference in renderer["materialSlots"]:
            if reference is not None and reference not in data["materials"]:
                raise ValueError("Unresolved material identity")
    return root, data


def read_mesh(path):
    r = Reader(Path(path).read_bytes())
    if r.take(8) != b"EFFMESH\0" or r.i32() != 6:
        raise ValueError("Expected EFFMESH v6")
    coordinate, source, name = r.string(), r.string(), r.string()
    count = r.count()
    vertices, normals, tangents, colors = [r.array("f") for _ in range(4)]
    uvs = [r.array("f") for _ in range(8)]
    indices = r.array("I")
    submeshes = [struct.unpack("<i5I", r.take(24)) for _ in range(r.count())]
    skin = [struct.unpack("<4f4I", r.take(32)) for _ in range(r.count())]
    bindposes = [r.array("f", 16) for _ in range(r.count())]
    bone_hashes = r.array("I")
    bone_paths = [r.string() for _ in range(r.count())]
    index_paths = [r.string() for _ in range(r.count())]
    sources = [(r.string(), r.string(), r.u32()) for _ in range(r.count())]
    donors = [[(r.string(), r.string(), r.u32()) for _ in range(r.count())] for _ in range(r.count())]
    blend_vertices = [struct.unpack("<I9f", r.take(40)) for _ in range(r.count())]
    blend_frames = [(r.string(), r.u32(), r.u32(), *r.take(3)) for _ in range(r.count())]
    blend_channels = [(r.string(), r.u32(), r.u32(), r.u32()) for _ in range(r.count())]
    blend_weights = r.array("f")
    additional = [r.array("f", 3) for _ in range(r.count())]
    if coordinate != "unity-y-up-left-handed" or r.pos != len(r.data):
        raise ValueError("Invalid coordinate/trailing mesh data")
    if len(vertices) != count * 3 or not all(math.isfinite(v) for v in vertices + normals + tangents):
        raise ValueError("Invalid positions/normals/tangents")
    if any(i >= count for i in indices):
        raise ValueError("Mesh index exceeds vertex count")
    if skin and len(skin) != count:
        raise ValueError("Skin weight count mismatch")
    if any(not math.isfinite(w) or w < 0 for row in skin for w in row[:4]):
        raise ValueError("Invalid skin weight")
    if any(w > 0 and b >= len(bindposes) for row in skin for w, b in zip(row[:4], row[4:])):
        raise ValueError("Skin weight references missing bindpose")
    if len(blend_weights) != len(blend_frames):
        raise ValueError("BlendShape frame weights mismatch")
    for _, start, length, *_ in submeshes:
        if start + length > len(indices) or length % 3:
            raise ValueError("Invalid triangle submesh range")
    return locals()
