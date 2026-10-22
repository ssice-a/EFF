"""Create editable native Blender geometry, preserving Renderer instances."""
import json
import math
import re
from pathlib import Path

import bpy
from mathutils import Matrix, Quaternion, Vector

from .format import load_package, read_mesh, safe_path
from .materials import MaterialBuilder


# Convert the complete Unity hierarchy and Mesh together. Naraka's character
# buffers can be Z-up locally; the Prefab supplies their -90-degree rotation.
# Blender front looks along +Y: characters must face -Y, with +Z up.
# Rotate the former conversion by 180 degrees around Z for the entire rig.
BASIS = Matrix(((-1, 0, 0, 0), (0, 0, -1, 0), (0, 1, 0, 0), (0, 0, 0, 1)))
BASIS_INVERSE = BASIS.inverted()


def convert(value):
    return (-value[0], -value[2], value[1])


def local_matrix(node):
    q = node["rotation"]
    value = Matrix.LocRotScale(Vector(node["position"]), Quaternion((q[3], q[0], q[1], q[2])), Vector(node["scale"]))
    return BASIS @ value @ BASIS_INVERSE


def bind_matrix(values):
    return BASIS @ Matrix([[values[row + col * 4] for col in range(4)] for row in range(4)]) @ BASIS_INVERSE


def shape_keys(obj, payload, weights):
    if not payload["blend_frames"]:
        return
    obj.shape_key_add(name="Basis")
    for channel_index, (name, _, first, count) in enumerate(payload["blend_channels"]):
        initial = weights[channel_index] if channel_index < len(weights) else 0.0
        for local_index in range(count):
            index = first + local_index
            _, start, length, *_ = payload["blend_frames"][index]
            frame_weight = payload["blend_weights"][index]
            key = obj.shape_key_add(name=name if count == 1 else "%s@%g" % (name, frame_weight))
            coords = [c for vertex in obj.data.vertices for c in vertex.co]
            for record in payload["blend_vertices"][start:start + length]:
                vertex, *values = record
                delta = convert(values[:3])
                for axis in range(3):
                    coords[vertex * 3 + axis] += delta[axis]
            key.data.foreach_set("co", coords)
            if count == 1:
                key.value = initial / frame_weight if frame_weight else 0
            else:
                # Keep every source frame; a single channel control interpolates
                # its neighboring frame deltas in Unity's percentage units.
                prop = "naraka_shape_%d" % channel_index
                obj[prop] = initial
                driver = key.driver_add("value").driver
                variable = driver.variables.new()
                variable.name, variable.type = "w", "SINGLE_PROP"
                variable.targets[0].id = obj
                variable.targets[0].data_path = '["%s"]' % prop
                previous = payload["blend_weights"][index - 1] if local_index else 0
                driver.expression = "min(1,max(0,(w-(%r))/(%r)))" % (previous, frame_weight - previous)
                if local_index < count - 1:
                    following = payload["blend_weights"][index + 1]
                    driver.expression += "*min(1,max(0,((%r)-w)/(%r)))" % (following, following - frame_weight)


def make_mesh(renderer, payload, collection, material_map):
    vertices = [convert(payload["vertices"][i:i + 3]) for i in range(0, len(payload["vertices"]), 3)]
    faces, slots = [], []
    mapping = {sub: slot for slot, sub in enumerate(renderer["submeshSlots"])}
    for sub_index, (_, start, length, base, *_rest) in enumerate(payload["submeshes"]):
        for i in range(start, start + length, 3):
            indices = payload["indices"][i:i + 3]
            # Source indices are normalized triangles; baseVertex remains part
            # of the Unity submesh addressing contract.
            indices = [v + base for v in indices]
            if any(v >= len(vertices) for v in indices):
                raise ValueError("Submesh base vertex exceeds positions")
            faces.append((indices[0], indices[2], indices[1]))
            slots.append(mapping.get(sub_index, min(sub_index, max(0, len(renderer["materialSlots"]) - 1))))
    mesh = bpy.data.meshes.new(renderer["meshName"])
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(renderer["name"], mesh)
    collection.objects.link(obj)
    obj["naraka_renderer"] = renderer["id"]
    obj["naraka_mesh_source"] = renderer["meshSource"]
    for reference in renderer["materialSlots"]:
        mesh.materials.append(material_map.get(reference))
    for polygon, slot in zip(mesh.polygons, slots):
        polygon.material_index = slot
        polygon.use_smooth = True
    loops = [loop.vertex_index for loop in mesh.loops]
    if len(payload["normals"]) == len(vertices) * 3:
        normals = [convert(payload["normals"][i:i + 3]) for i in range(0, len(payload["normals"]), 3)]
        mesh.normals_split_custom_set([normals[index] for index in loops])
    if len(payload["tangents"]) == len(vertices) * 4:
        tangents = mesh.attributes.new("Naraka tangent", "FLOAT_VECTOR", "POINT")
        tangents.data.foreach_set("vector", [v for i in range(len(vertices)) for v in convert(payload["tangents"][i * 4:i * 4 + 3])])
        signs = mesh.attributes.new("Naraka tangent sign", "FLOAT", "POINT")
        signs.data.foreach_set("value", payload["tangents"][3::4])
    dimensions = []
    for index, uv in enumerate(payload["uvs"]):
        dim = len(uv) // len(vertices) if uv else 0
        dimensions.append(dim)
        if not dim:
            continue
        if len(uv) != len(vertices) * dim or not 1 <= dim <= 4:
            raise ValueError("Invalid UV channel")
        layer = mesh.uv_layers.new(name="UV%d" % index)
        layer.data.foreach_set("uv", [value for vertex in loops for value in (uv[vertex * dim], uv[vertex * dim + 1] if dim > 1 else 0)])
        if dim > 2:
            attr = mesh.attributes.new("UV%d_ZW" % index, "FLOAT_VECTOR", "POINT")
            attr.data.foreach_set("vector", [value for vertex in range(len(vertices)) for value in
                (uv[vertex * dim + 2], uv[vertex * dim + 3] if dim > 3 else 0, 0)])
    mesh["naraka_uv_dimensions"] = dimensions
    if len(payload["colors"]) in (len(vertices) * 3, len(vertices) * 4):
        dim = len(payload["colors"]) // len(vertices)
        attr = mesh.color_attributes.new(name="Color", type="FLOAT_COLOR", domain="POINT")
        attr.data.foreach_set("color", [v for i in range(len(vertices)) for v in
            (*payload["colors"][i * dim:i * dim + 3], payload["colors"][i * dim + 3] if dim == 4 else 1)])
    shape_keys(obj, payload, renderer.get("blendShapeWeights", []))
    return obj


def make_rig(collection, prepared, world, source_nodes, report):
    definitions, palettes, source_bones = {}, {}, {}
    for renderer, payload, obj in prepared:
        if not payload["skin"] or not payload["bindposes"]:
            continue
        palette = []
        for index, values in enumerate(payload["bindposes"]):
            rest = world[renderer["node"]] @ bind_matrix(values).inverted()
            hashed = payload["bone_hashes"][index] if index < len(payload["bone_hashes"]) else index
            source_node = renderer["bones"][index] if index < len(renderer["bones"]) else None
            base = source_nodes[source_node]["name"] if source_node is not None else "bone_%08x" % hashed
            name = base
            # Share bones only when their actual rest frames agree, not merely
            # their names/hashes. Independent bindposes remain independent.
            suffix = 1
            while name in definitions and max(abs(rest[r][c] - definitions[name][r][c]) for r in range(4) for c in range(4)) > 0.0002:
                name = base + "_%d" % suffix
                suffix += 1
            definitions.setdefault(name, rest)
            if source_node is not None:
                source_bones.setdefault(source_node, name)
            palette.append(name)
        palettes[obj] = palette
    if not definitions:
        return None
    data = bpy.data.armatures.new(collection.name + " skeleton")
    rig = bpy.data.objects.new(data.name, data)
    collection.objects.link(rig)
    rig["naraka_rest_source"] = "inverse-bindposes"
    bpy.context.view_layer.objects.active = rig
    rig.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    try:
        for name, matrix in definitions.items():
            bone = data.edit_bones.new(name)
            bone.head = matrix.translation
            direction = matrix.to_3x3() @ Vector((0, 0.04, 0))
            bone.tail = bone.head + (direction if direction.length > 0.00001 else Vector((0, 0.04, 0)))
            bone.align_roll(matrix.to_3x3() @ Vector((0, 0, 1)))
            bone["naraka_bind_rest"] = [matrix[row][column] for row in range(4) for column in range(4)]
        for source_node, name in source_bones.items():
            parent = source_nodes[source_node]["parent"]
            while parent >= 0 and parent not in source_bones:
                parent = source_nodes[parent]["parent"]
            if parent in source_bones and source_bones[parent] != name:
                data.edit_bones[name].parent = data.edit_bones[source_bones[parent]]
    finally:
        bpy.ops.object.mode_set(mode="OBJECT")
        rig.select_set(False)
    rig.show_in_front = True
    for renderer, payload, obj in prepared:
        if obj not in palettes:
            continue
        names = palettes[obj]
        groups = [obj.vertex_groups.new(name=name) for name in names]
        for vertex, row in enumerate(payload["skin"]):
            for weight, index in zip(row[:4], row[4:]):
                if weight > 0:
                    groups[index].add([vertex], weight, "ADD")
        modifier = obj.modifiers.new("Naraka skin", "ARMATURE")
        modifier.object = rig
        obj["naraka_bindposes"] = json.dumps(payload["bindposes"])
        obj["naraka_bone_hashes"] = json.dumps(payload["bone_hashes"])
        obj["naraka_bone_nodes"] = renderer["bones"]
    resolved = sum(bool(renderer["bones"]) for renderer, _, obj in prepared if obj in palettes)
    report.append("Rest matrices are reconstructed from bindposes; %d/%d skinned renderer palettes use original source bone names/parents. Unresolved palettes retain hashes. Runtime animation is not imported." % (resolved, len(palettes)))
    return rig


def import_package(path, highest_lod=True):
    root, manifest = load_package(path)
    # Validate every binary before touching the user's scene.
    payloads = [read_mesh(safe_path(root, r["mesh"])) for r in manifest["renderers"]]
    previous_active = bpy.context.view_layer.objects.active
    previous_selection = list(bpy.context.selected_objects)
    if bpy.context.object and bpy.context.object.mode != "OBJECT":
        raise ValueError("Switch Blender to Object Mode before importing")
    before = {kind: set(getattr(bpy.data, kind)) for kind in ("objects", "meshes", "armatures", "materials", "node_groups", "images", "collections", "texts")}
    report = list(manifest.get("warnings", []))
    try:
        for obj in previous_selection:
            obj.select_set(False)
        collection = bpy.data.collections.new(manifest["name"])
        bpy.context.scene.collection.children.link(collection)
        collection["naraka_package"] = str(root)
        collection["naraka_collection_role"] = "PACKAGE"
        collection["naraka_basis_version"] = 2
        mesh_collection = bpy.data.collections.new(collection.name + " Meshes")
        mesh_collection["naraka_collection_role"] = "MESHES"
        collection.children.link(mesh_collection)
        skeleton_collection = bpy.data.collections.new(collection.name + " Skeletons")
        skeleton_collection["naraka_collection_role"] = "SKELETONS"
        collection.children.link(skeleton_collection)
        builder = MaterialBuilder(root, manifest, report)
        materials = {key: builder.build(value) for key, value in manifest["materials"].items()}
        world, visible = [], []
        for node in manifest["nodes"]:
            local = local_matrix(node)
            world.append(world[node["parent"]] @ local if node["parent"] >= 0 else local)
            visible.append(node["active"] and (visible[node["parent"]] if node["parent"] >= 0 else True))
        # Source Transforms are import metadata, not Blender Empty objects.
        # Keep cloth helpers, effect anchors and source TRS available without
        # cluttering the editable mesh/skeleton resource collections.
        source_text = bpy.data.texts.new(collection.name + " source hierarchy.json")
        source_text.write(json.dumps(manifest["nodes"], ensure_ascii=False, indent=2))
        collection["naraka_source_hierarchy"] = source_text.name
        hidden_lods, lod_levels = set(), {}
        for group in manifest.get("lodGroups", []):
            if group["levels"]:
                first = set(group["levels"][0]["renderers"])
                hidden_lods.update(value for level in group["levels"][1:] for value in level["renderers"] if value not in first)
                for level, source in enumerate(group["levels"]):
                    for reference in source["renderers"]:
                        lod_levels[reference] = min(level, lod_levels.get(reference, level))
        inferred_lods = not manifest.get("lodGroups")
        prepared, lod_collections = [], {}
        for renderer, payload in zip(manifest["renderers"], payloads):
            lod_match = re.search(r"_L([1-9]\d*)$", renderer["name"], re.IGNORECASE)
            lod = int(lod_match[1]) if inferred_lods and lod_match else lod_levels.get(renderer["id"], 0)
            if lod not in lod_collections:
                child = bpy.data.collections.new(collection.name + " LOD%d" % lod)
                child["naraka_collection_role"] = "LOD"
                child["naraka_lod"] = lod
                mesh_collection.children.link(child)
                lod_collections[lod] = child
            obj = make_mesh(renderer, payload, lod_collections[lod], materials)
            node = manifest["nodes"][renderer["node"]]
            obj.matrix_world = world[renderer["node"]]
            obj["naraka_node"] = node["source"]
            obj["naraka_node_path"] = node["path"]
            obj["naraka_active"] = node["active"]
            obj["naraka_lod"] = lod
            lower_lod = renderer["id"] in hidden_lods or (inferred_lods and lod_match is not None)
            obj["naraka_lower_lod"] = bool(lower_lod)
            enabled = visible[renderer["node"]] and renderer["enabled"]
            obj["naraka_source_enabled"] = enabled
            obj.hide_render = not enabled or (highest_lod and lower_lod)
            obj.hide_set(obj.hide_render)
            prepared.append((renderer, payload, obj))
        if inferred_lods:
            report.append("No native LODGroup in this Prefab. Lower-detail visibility uses the explicit _L<number> naming convention; all meshes are retained.")
        bpy.context.view_layer.update()
        rig = make_rig(skeleton_collection, prepared, world, manifest["nodes"], report)
        if rig:
            for renderer, _, obj in prepared:
                obj.parent = rig
                # A composed source hierarchy may contain shear from rotated
                # nonuniform scales. Object TRS cannot represent that matrix;
                # the full parent-inverse matrix can, without baking meshes,
                # normals or shape keys. Rig motion still applies exactly once.
                obj.matrix_parent_inverse = world[renderer["node"]]
                obj.matrix_basis = Matrix.Identity(4)
        bpy.context.view_layer.update()
        summary = {"format": "NARAKA_IMPORT_REPORT", "package": str(root), "collection": collection.name,
            "meshes": len(prepared), "materials": len(materials), "textures": len(builder.images),
            "sourceTextures": len(manifest["textures"]), "bones": len(rig.data.bones) if rig else 0,
            "empties": 0, "sourceNodes": len(manifest["nodes"]),
            "warnings": list(dict.fromkeys(report)), "materialAdapters": builder.adapters,
            "materialStatus": builder.statuses}
        text = bpy.data.texts.new(collection.name + " import report.json")
        text.write(json.dumps(summary, ensure_ascii=False, indent=2))
        collection["naraka_import_report"] = text.name
        return summary
    except Exception:
        # A failed import must not leave a half-imported costume in the scene.
        if bpy.context.object and bpy.context.object.mode != "OBJECT":
            bpy.ops.object.mode_set(mode="OBJECT")
        for kind in ("objects", "meshes", "armatures", "materials", "node_groups", "images", "collections", "texts"):
            block = getattr(bpy.data, kind)
            for item in set(block) - before[kind]:
                block.remove(item, do_unlink=True)
        raise
    finally:
        for obj in previous_selection:
            if obj.name in bpy.data.objects:
                obj.select_set(True)
        if previous_active and previous_active.name in bpy.data.objects:
            bpy.context.view_layer.objects.active = previous_active
