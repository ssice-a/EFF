"""Build the Typhoea fixture that binds visible mesh vertices to one new bone.

This is an offline diagnostic package builder.  It does not load the DLL or
write into the game directory.  The source package is copied to a new output
directory and the copied Mesh, Skeleton, Physics and INI are changed together.
"""

import argparse
import copy
import hashlib
import importlib.util
import json
import math
import shutil
import struct
import zlib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PHYSICS_MODULE = ROOT / "tools" / "Blender" / "eiem_physics_document.py"
SPEC = importlib.util.spec_from_file_location("eiem_physics_document", PHYSICS_MODULE)
physics_document = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(physics_document)

MESH_MAGIC = b"EIEMESH\0"
SKELETON_MAGIC = b"EIESKEL\0"
COORDINATE = "unity-y-up-left-handed"
MESH_RELATIVE = "meshes/MeshS_actor_typhoea_body_01_lod0_0.mesh"
OLD_PHYSICS_RELATIVE = "physics/typhoea-left-index-v67.physics"
OLD_SKELETON_RELATIVE = "physics/skeletons/typhoea-left-index-v67.skeleton"
PHYSICS_RELATIVE = "physics/typhoea-left-index-v70.physics"
SKELETON_RELATIVE = "physics/skeletons/typhoea-left-index-v70.skeleton"
PHYSICS_SKELETON_RELATIVE = "skeletons/typhoea-left-index-v70.skeleton"

FINGER0_SUFFIX = "/Bip001_L_Finger0"
FINGER01_SUFFIX = FINGER0_SUFFIX + "/Bip001_L_Finger01"
FINGER02_SUFFIX = FINGER01_SUFFIX + "/Bip001_L_Finger02"
NEW_BONE_NAME = "EIEM_PhysicsTip"
NEW_LOCAL_POSITION = (-0.015, 0.0, 0.0)
IDENTITY_ROTATION = (0.0, 0.0, 0.0, 1.0)
IDENTITY_SCALE = (1.0, 1.0, 1.0)


class Reader:
    def __init__(self, data):
        self.data = data
        self.pos = 0

    def take(self, size):
        if size < 0 or self.pos + size > len(self.data):
            raise ValueError("truncated resource")
        value = self.data[self.pos:self.pos + size]
        self.pos += size
        return value

    def i32(self):
        return struct.unpack("<i", self.take(4))[0]

    def u32(self):
        return struct.unpack("<I", self.take(4))[0]

    def f32(self):
        return struct.unpack("<f", self.take(4))[0]

    def u8(self):
        return self.take(1)[0]

    def string(self):
        length = 0
        shift = 0
        while True:
            byte = self.u8()
            length |= (byte & 0x7F) << shift
            if not byte & 0x80:
                break
            shift += 7
            if shift > 28:
                raise ValueError("invalid string length")
        return self.take(length).decode("utf-8", "strict")

    def floats(self):
        count = self.i32()
        if count < 0:
            raise ValueError("invalid float array")
        return list(struct.unpack("<" + "f" * count, self.take(count * 4))) if count else []


class Writer:
    def __init__(self):
        self.data = bytearray()

    def raw(self, value):
        self.data.extend(value)

    def i32(self, value):
        self.raw(struct.pack("<i", value))

    def u32(self, value):
        self.raw(struct.pack("<I", value))

    def f32(self, value):
        self.raw(struct.pack("<f", value))

    def u8(self, value):
        self.raw(bytes((value,)))

    def string(self, value):
        data = value.encode("utf-8")
        length = len(data)
        while length >= 0x80:
            self.u8((length & 0x7F) | 0x80)
            length >>= 7
        self.u8(length)
        self.raw(data)

    def floats(self, values):
        self.i32(len(values))
        if values:
            self.raw(struct.pack("<" + "f" * len(values), *values))


def read_mesh(path):
    reader = Reader(Path(path).read_bytes())
    if reader.take(8) != MESH_MAGIC or reader.i32() != 3:
        raise ValueError("fixture requires EIEM Mesh v3")
    mesh = {
        "version": 3,
        "coordinate": reader.string(),
        "source": reader.string(),
        "name": reader.string(),
        "vertex_count": reader.i32(),
        "arrays": [reader.floats() for _ in range(4)],
        "uvs": [reader.floats() for _ in range(8)],
    }
    mesh["indices"] = [reader.u32() for _ in range(reader.i32())]
    mesh["submeshes"] = [
        (reader.i32(), reader.u32(), reader.u32(), reader.u32(), reader.u32(), reader.u32())
        for _ in range(reader.i32())
    ]
    mesh["skin"] = [
        ([reader.f32() for _ in range(4)], [reader.u32() for _ in range(4)])
        for _ in range(reader.i32())
    ]
    mesh["bindposes"] = [tuple(reader.f32() for _ in range(16)) for _ in range(reader.i32())]
    mesh["bone_hashes"] = [reader.u32() for _ in range(reader.i32())]
    mesh["bone_paths"] = [reader.string() for _ in range(reader.i32())]
    mesh["blend_vertices"] = [
        (reader.u32(), tuple(reader.f32() for _ in range(9)))
        for _ in range(reader.i32())
    ]
    mesh["blend_frames"] = [
        (reader.string(), reader.u32(), reader.u32(), reader.u8(), reader.u8(), reader.u8())
        for _ in range(reader.i32())
    ]
    mesh["blend_channels"] = [
        (reader.string(), reader.u32(), reader.u32(), reader.u32())
        for _ in range(reader.i32())
    ]
    mesh["blend_weights"] = reader.floats()
    mesh["additional"] = [tuple(reader.f32() for _ in range(3)) for _ in range(reader.i32())]
    if reader.pos != len(reader.data):
        raise ValueError("unexpected trailing Mesh data")
    validate_mesh(mesh)
    return mesh


def validate_mesh(mesh):
    count = mesh["vertex_count"]
    if mesh["coordinate"] != COORDINATE or count <= 0:
        raise ValueError("unexpected Mesh coordinate or vertex count")
    expected = (3, 3, 4, 4)
    for values, width in zip(mesh["arrays"], expected):
        if values and len(values) != count * width:
            raise ValueError("invalid Mesh vertex channel")
    for values in mesh["uvs"]:
        if values and len(values) not in (count * 2, count * 3, count * 4):
            raise ValueError("invalid Mesh UV channel")
    if len(mesh["skin"]) != count:
        raise ValueError("fixture requires one skin record per vertex")
    palette = len(mesh["bindposes"])
    if not palette or len(mesh["bone_hashes"]) != palette or len(mesh["bone_paths"]) != palette:
        raise ValueError("incomplete Mesh bone identity")
    for weights, bones in mesh["skin"]:
        if len(weights) != 4 or len(bones) != 4 or any(index >= palette for index in bones):
            raise ValueError("invalid Mesh skin record")


def write_mesh(path, mesh):
    validate_mesh(mesh)
    writer = Writer()
    writer.raw(MESH_MAGIC); writer.i32(3)
    writer.string(mesh["coordinate"]); writer.string(mesh["source"]); writer.string(mesh["name"])
    writer.i32(mesh["vertex_count"])
    for values in mesh["arrays"]: writer.floats(values)
    for values in mesh["uvs"]: writer.floats(values)
    writer.i32(len(mesh["indices"]))
    for value in mesh["indices"]: writer.u32(value)
    writer.i32(len(mesh["submeshes"]))
    for topology, index_start, index_count, base, first, count in mesh["submeshes"]:
        writer.i32(topology); writer.u32(index_start); writer.u32(index_count)
        writer.u32(base); writer.u32(first); writer.u32(count)
    writer.i32(len(mesh["skin"]))
    for weights, bones in mesh["skin"]:
        for value in weights: writer.f32(value)
        for value in bones: writer.u32(value)
    writer.i32(len(mesh["bindposes"]))
    for matrix in mesh["bindposes"]:
        for value in matrix: writer.f32(value)
    writer.i32(len(mesh["bone_hashes"]))
    for value in mesh["bone_hashes"]: writer.u32(value)
    writer.i32(len(mesh["bone_paths"]))
    for value in mesh["bone_paths"]: writer.string(value)
    writer.i32(len(mesh["blend_vertices"]))
    for index, values in mesh["blend_vertices"]:
        writer.u32(index)
        for value in values: writer.f32(value)
    writer.i32(len(mesh["blend_frames"]))
    for name, name_hash, start, has_vertices, has_normals, has_tangents in mesh["blend_frames"]:
        writer.string(name); writer.u32(name_hash); writer.u32(start)
        writer.u8(has_vertices); writer.u8(has_normals); writer.u8(has_tangents)
    writer.i32(len(mesh["blend_channels"]))
    for name, name_hash, frame_index, frame_count in mesh["blend_channels"]:
        writer.string(name); writer.u32(name_hash); writer.u32(frame_index); writer.u32(frame_count)
    writer.floats(mesh["blend_weights"])
    writer.i32(len(mesh["additional"]))
    for values in mesh["additional"]:
        for value in values: writer.f32(value)
    Path(path).write_bytes(writer.data)


def stored_to_matrix(values):
    return [[values[column * 4 + row] for column in range(4)] for row in range(4)]


def matrix_to_stored(matrix):
    return tuple(matrix[row][column] for column in range(4) for row in range(4))


def multiply(left, right):
    return [[sum(left[row][k] * right[k][column] for k in range(4))
             for column in range(4)] for row in range(4)]


def inverse_translation(position):
    x, y, z = position
    return [[1.0, 0.0, 0.0, -x], [0.0, 1.0, 0.0, -y],
            [0.0, 0.0, 1.0, -z], [0.0, 0.0, 0.0, 1.0]]


def unique_suffix(paths, suffix):
    matches = [path for path in paths if path.endswith(suffix)]
    if len(matches) != 1:
        raise ValueError("expected one Mesh bone ending with " + suffix)
    return matches[0]


def modify_mesh(source):
    mesh = copy.deepcopy(source)
    finger0 = unique_suffix(mesh["bone_paths"], FINGER0_SUFFIX)
    finger01 = unique_suffix(mesh["bone_paths"], FINGER01_SUFFIX)
    finger02 = unique_suffix(mesh["bone_paths"], FINGER02_SUFFIX)
    old_slot = mesh["bone_paths"].index(finger02)
    new_path = finger02 + "/" + NEW_BONE_NAME
    if new_path in mesh["bone_paths"]:
        raise ValueError("new fixture bone already exists")
    new_slot = len(mesh["bone_paths"])
    new_bind = multiply(inverse_translation(NEW_LOCAL_POSITION),
                        stored_to_matrix(mesh["bindposes"][old_slot]))
    mesh["bone_paths"].append(new_path)
    mesh["bone_hashes"].append(zlib.crc32(new_path.encode("utf-8")) & 0xFFFFFFFF)
    mesh["bindposes"].append(matrix_to_stored(new_bind))
    remapped_vertices = set()
    remapped_influences = 0
    for vertex, (weights, bones) in enumerate(mesh["skin"]):
        for influence, (weight, bone) in enumerate(zip(weights, bones)):
            if weight > 0.0 and bone == old_slot:
                bones[influence] = new_slot
                remapped_vertices.add(vertex)
                remapped_influences += 1
    if not remapped_influences:
        raise ValueError("Finger02 has no weighted Mesh vertices")
    validate_mesh(mesh)
    return mesh, {
        "finger0": finger0, "finger01": finger01, "finger02": finger02,
        "newPath": new_path, "oldSlot": old_slot, "newSlot": new_slot,
        "remappedVertices": len(remapped_vertices),
        "remappedInfluences": remapped_influences,
    }


def hierarchy(paths, new_path):
    ordered = []
    seen = set()
    for path in paths:
        parts = path.split("/") if path else [""]
        for count in range(1, len(parts) + 1):
            prefix = "/".join(parts[:count])
            if prefix not in seen:
                seen.add(prefix)
                ordered.append(prefix)
    source_paths = set(ordered)
    if new_path in source_paths:
        raise ValueError("new Skeleton path collides with source hierarchy")
    ordered.append(new_path)
    nodes = []
    indices = {}
    for path in ordered:
        parent_path = path.rsplit("/", 1)[0] if "/" in path else None
        parent = indices[parent_path] if parent_path is not None else -1
        is_new = path == new_path
        nodes.append({
            "path": path, "parent": parent,
            "position": NEW_LOCAL_POSITION if is_new else (0.0, 0.0, 0.0),
            "rotation": IDENTITY_ROTATION, "scale": IDENTITY_SCALE,
            "source": not is_new,
        })
        indices[path] = len(nodes) - 1
    return nodes


def skeleton_bytes(nodes):
    writer = Writer(); writer.raw(SKELETON_MAGIC); writer.i32(2)
    writer.string(COORDINATE); writer.i32(len(nodes))
    for node in nodes:
        writer.string(node["path"]); writer.i32(node["parent"])
        for value in node["position"]: writer.f32(value)
        for value in node["rotation"]: writer.f32(value)
        for value in node["scale"]: writer.f32(value)
    writer.i32(0); writer.i32(-1)
    writer.i32(len(nodes))
    for node in nodes: writer.u8(1 if node["source"] else 0)
    return bytes(writer.data)


def identity(label):
    return hashlib.sha256(label.encode("utf-8")).hexdigest()[:32]


def physics_bytes(details):
    document = {
        "version": 5, "purpose": "authoring", "coordinate": COORDINATE,
        "backend": "BeyondDynamicBone", "id": identity("v70-typhoea-new-bone-resource"),
        "skeleton": PHYSICS_SKELETON_RELATIVE,
        "groups": [{
            "id": identity("v70-typhoea-new-bone-group"),
            "name": "v70 Typhoea visible new-bone diagnostic",
            "nodes": [
                {"bone": details["finger0"], "role": "FIXED"},
                {"bone": details["finger01"], "role": "MOVE"},
                {"bone": details["finger02"], "role": "MOVE"},
                {"bone": details["newPath"], "role": "MOVE"},
            ],
            "parameters": {
                "gravity": 10.0, "stablizationTimeAfterReset": 0.1,
                "gravityFalloff": 0.0, "blendWeight": 1.0,
                "animationPoseRatio": 1.0,
            },
            "radius": physics_document.default_radius(),
            "nativeParameters": [],
            "colliders": [],
        }],
        "colliders": [],
    }
    return physics_document.encode(document)


def modify_ini(text):
    old_section = "[PhysicsTyphoeaLeftIndexV67]\npath=" + OLD_PHYSICS_RELATIVE
    replacement = ("[SkeletonTyphoeaNewBoneV70]\npath=" + SKELETON_RELATIVE +
                   "\n\n[PhysicsTyphoeaNewBoneV70]\npath=" + PHYSICS_RELATIVE)
    if text.count(old_section) != 1:
        raise ValueError("expected one v67 Physics resource in mod.ini")
    text = text.replace(old_section, replacement)
    old_action = "physics=PhysicsTyphoeaLeftIndexV67"
    new_action = "skeleton=SkeletonTyphoeaNewBoneV70\nphysics=PhysicsTyphoeaNewBoneV70"
    if text.count(old_action) != 1:
        raise ValueError("expected one v67 Physics Render action in mod.ini")
    return text.replace(old_action, new_action)


def unchanged_mesh_fields(source, target):
    changed = {"skin", "bindposes", "bone_hashes", "bone_paths"}
    return all(source[key] == target[key] for key in source if key not in changed)


def build(source_mod, output):
    source_mod = Path(source_mod).resolve()
    output = Path(output).resolve()
    if output.exists():
        raise ValueError("output already exists: " + str(output))
    mesh_source_path = source_mod / MESH_RELATIVE
    ini_source_path = source_mod / "mod.ini"
    if not mesh_source_path.is_file() or not ini_source_path.is_file():
        raise ValueError("source mod is missing the Typhoea Mesh or mod.ini")
    source_mesh = read_mesh(mesh_source_path)
    target_mesh, details = modify_mesh(source_mesh)
    nodes = hierarchy(source_mesh["bone_paths"], details["newPath"])
    if not set(source_mesh["bone_paths"]).issubset({node["path"] for node in nodes}):
        raise AssertionError("Skeleton does not cover the source Mesh palette")

    shutil.copytree(source_mod, output)
    write_mesh(output / MESH_RELATIVE, target_mesh)
    (output / SKELETON_RELATIVE).parent.mkdir(parents=True, exist_ok=True)
    (output / SKELETON_RELATIVE).write_bytes(skeleton_bytes(nodes))
    (output / PHYSICS_RELATIVE).write_bytes(physics_bytes(details))
    for relative in (OLD_PHYSICS_RELATIVE, OLD_SKELETON_RELATIVE):
        stale = output / relative
        if stale.exists(): stale.unlink()
    ini = ini_source_path.read_text(encoding="utf-8-sig")
    (output / "mod.ini").write_text(modify_ini(ini), encoding="utf-8", newline="\n")

    decoded_target = read_mesh(output / MESH_RELATIVE)
    decoded_physics = physics_document.decode((output / PHYSICS_RELATIVE).read_bytes())
    new_bind = stored_to_matrix(decoded_target["bindposes"][details["newSlot"]])
    expected_bind = multiply(inverse_translation(NEW_LOCAL_POSITION),
                             stored_to_matrix(source_mesh["bindposes"][details["oldSlot"]]))
    bind_error = max(abs(new_bind[row][column] - expected_bind[row][column])
                     for row in range(4) for column in range(4))
    validation = {
        "fixture": "v70-new-bone-visible-mesh",
        "sourceMod": str(source_mod), "output": str(output),
        "sourceMeshSha256": hashlib.sha256(mesh_source_path.read_bytes()).hexdigest().upper(),
        "targetMeshSha256": hashlib.sha256((output / MESH_RELATIVE).read_bytes()).hexdigest().upper(),
        "vertexCount": source_mesh["vertex_count"],
        "sourcePalette": len(source_mesh["bone_paths"]),
        "targetPalette": len(decoded_target["bone_paths"]),
        "skeletonNodes": len(nodes), "sourceSkeletonNodes": sum(n["source"] for n in nodes),
        "newSkeletonNodes": sum(not n["source"] for n in nodes),
        "physicsNodes": len(decoded_physics["groups"][0]["nodes"]),
        "physicsRoles": [node["role"] for node in decoded_physics["groups"][0]["nodes"]],
        "unchangedMeshFields": unchanged_mesh_fields(source_mesh, decoded_target),
        "newBindposeMaxError": bind_error,
        **details,
    }
    if (validation["targetPalette"] != validation["sourcePalette"] + 1 or
            validation["newSkeletonNodes"] != 1 or
            validation["physicsRoles"] != ["FIXED", "MOVE", "MOVE", "MOVE"] or
            not validation["unchangedMeshFields"] or bind_error > 1e-6):
        raise AssertionError("generated fixture failed validation: " + json.dumps(validation))
    (output.parent / "validation.json").write_text(
        json.dumps(validation, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return validation


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source_mod", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    print(json.dumps(build(args.source_mod, args.output), ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
