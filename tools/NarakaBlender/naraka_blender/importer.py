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


def make_mesh(renderer, payload, collection, material_map, renderer_property="naraka_renderer"):
    vertices = [convert(payload["vertices"][i:i + 3]) for i in range(0, len(payload["vertices"]), 3)]
    faces, slots = [], []
    submesh_slots = renderer.get("submeshSlots", list(range(len(renderer["materialSlots"]))))
    mapping = {sub: slot for slot, sub in enumerate(submesh_slots)}
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
    mesh = bpy.data.meshes.new(renderer.get("meshName", renderer.get("sourceName", renderer["name"])))
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(renderer["name"], mesh)
    collection.objects.link(obj)
    obj[renderer_property] = renderer.get("id", renderer.get("source"))
    source_property = "naraka_mesh_source" if renderer_property == "naraka_renderer" else "naraka_effect_mesh_source"
    obj[source_property] = renderer.get("meshSource", renderer.get("source", renderer.get("id")))
    obj["naraka_resource_kind"] = "mesh-renderer" if renderer_property == "naraka_renderer" else (
        "effect-mesh-renderer" if renderer_property == "naraka_effect_renderer" else "effect-mesh-asset")
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


def make_billboard_preview(system, collection, material_map, world, node_path):
    size = max(0.05, float(system.get("previewSize", 0.12)))
    name = "%s %s source proxy" % (system["name"], system["renderModeName"])
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata([(-size, 0, -size), (size, 0, -size), (size, 0, size), (-size, 0, size)], [], [(0, 1, 2, 3)])
    mesh.update()
    uv = mesh.uv_layers.new(name="UV0")
    uv.data.foreach_set("uv", [0, 0, 1, 0, 1, 1, 0, 1])
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    obj.matrix_world = world[system["node"]]
    obj["naraka_resource_kind"] = "billboard-preview"
    obj["naraka_effect_prefab"] = system["prefab"]
    obj["naraka_particle_system"] = system["source"]
    obj["naraka_source_mesh"] = "Unity built-in billboard quad; no authored Mesh reference"
    obj["naraka_preview_only"] = True
    obj["naraka_render_mode"] = system["renderModeName"]
    obj["naraka_source_node_path"] = node_path
    obj["naraka_particle_materials_json"] = json.dumps(system.get("materials", []))
    obj["naraka_particle_textures_json"] = json.dumps(system.get("textures", []))
    material_ids = system.get("materials", [])
    for material_id in material_ids:
        material = material_map.get(material_id)
        if material is not None:
            mesh.materials.append(material)
    if mesh.materials:
        for polygon in mesh.polygons:
            polygon.material_index = 0
    return obj


def _texture_inventory(manifest, identity):
    source = manifest["textures"].get(identity)
    if source is None:
        return {"id": identity, "name": identity, "missing": True}
    return {"id": identity, "name": source["name"], "logicalPath": source.get("logicalPath"),
        "packagePath": source.get("path"), "width": source.get("width"), "height": source.get("height")}


def _material_inventory(manifest, identity):
    source = manifest["materials"].get(identity)
    if source is None:
        return {"id": identity, "name": identity, "missing": True}
    bindings = []
    for property_name, binding in source.get("textures", {}).items():
        texture_id = binding.get("source") or binding.get("texture")
        if texture_id:
            bindings.append({"property": property_name, "texture": _texture_inventory(manifest, texture_id),
                "scale": binding.get("scale"), "offset": binding.get("offset")})
    return {"id": identity, "name": source["name"], "shader": source.get("shader"),
        "logicalPath": source.get("logicalPath"), "textures": bindings}


def _mesh_inventory(row, payload, manifest):
    return {"id": row.get("id", row.get("source")), "name": row["name"],
        "sourceName": row.get("meshName", row.get("sourceName")), "logicalPath": row.get("logicalPath"),
        "packagePath": row.get("mesh"), "vertices": payload["count"],
        "triangles": len(payload["indices"]) // 3, "submeshes": len(payload["submeshes"]),
        "uses": row.get("uses", []),
        "materials": [_material_inventory(manifest, identity) for identity in row.get("materialSlots", []) if identity]}


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
    effect_renderers = manifest.get("effectRenderers", [])
    effect_renderer_payloads = [read_mesh(safe_path(root, r["mesh"])) for r in effect_renderers]
    effect_meshes = manifest.get("effectMeshes", [])
    effect_mesh_payloads = [read_mesh(safe_path(root, r["mesh"])) for r in effect_meshes]
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
        effects_collection = bpy.data.collections.new(collection.name + " Effects")
        effects_collection["naraka_collection_role"] = "EFFECTS"
        collection.children.link(effects_collection)
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
        effect_collections = {}
        for effect in manifest.get("effects", []):
            name = Path(effect["prefab"].replace("\\", "/")).stem
            child = bpy.data.collections.new(collection.name + " FX " + name)
            child["naraka_collection_role"] = "EFFECT_PREFAB"
            child["naraka_effect_prefab"] = effect["prefab"]
            child["naraka_embedded_effect"] = bool(effect.get("embedded", False))
            effects_collection.children.link(child)
            effect_collections[effect["prefab"]] = child
            component_path = effect.get("components")
            if component_path:
                component_text = bpy.data.texts.new(child.name + " source components.json")
                component_text.write(safe_path(root, component_path).read_text(encoding="utf-8-sig"))
                child["naraka_source_components"] = component_text.name

        particles_by_prefab = {}
        for particle in manifest.get("particleSystems", []):
            particles_by_prefab.setdefault(particle["prefab"], []).append(particle)
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

        effect_prepared_by_prefab = {}
        for renderer, payload in zip(effect_renderers, effect_renderer_payloads):
            prefab = renderer["effectPrefab"]
            obj = make_mesh(renderer, payload, effect_collections[prefab], materials, "naraka_effect_renderer")
            node = manifest["nodes"][renderer["node"]]
            obj.matrix_world = world[renderer["node"]]
            obj["naraka_effect_prefab"] = prefab
            obj["naraka_source_node"] = node["source"]
            obj["naraka_source_node_path"] = node["path"]
            obj["naraka_source_enabled"] = bool(visible[renderer["node"]] and renderer["enabled"])
            obj.hide_render = not obj["naraka_source_enabled"]
            obj.hide_set(obj.hide_render)
            effect_prepared_by_prefab.setdefault(prefab, []).append((renderer, payload, obj))

        effect_mesh_objects = {}
        for row, payload in zip(effect_meshes, effect_mesh_payloads):
            prefab = row["effectPrefab"]
            obj = make_mesh(row, payload, effect_collections[prefab], materials, "naraka_effect_mesh")
            node = manifest["nodes"][row["node"]]
            obj.matrix_world = world[row["node"]]
            obj["naraka_effect_prefab"] = prefab
            obj["naraka_source_node"] = node["source"]
            obj["naraka_source_node_path"] = node["path"]
            obj["naraka_mesh_uses"] = row.get("uses", [])
            obj["naraka_source_enabled"] = bool(visible[row["node"]])
            obj.hide_render = not obj["naraka_source_enabled"]
            obj.hide_set(obj.hide_render)
            effect_mesh_objects[(prefab, row.get("id", row.get("source")))] = obj

        for particle in manifest.get("particleSystems", []):
            prefab = particle["prefab"]
            for mesh_id in particle.get("meshes", []):
                obj = effect_mesh_objects.get((prefab, mesh_id))
                if obj is not None:
                    users = json.loads(obj.get("naraka_particle_systems_json", "[]"))
                    if particle["source"] not in users:
                        users.append(particle["source"])
                    obj["naraka_particle_systems_json"] = json.dumps(users)
            if particle.get("renderModeName") in ("Billboard", "Stretch", "HorizontalBillboard", "VerticalBillboard"):
                node = manifest["nodes"][particle["node"]]
                obj = make_billboard_preview(particle, effect_collections[prefab], materials, world, node["path"])
                obj["naraka_source_node"] = node["source"]
                active = bool(visible[particle["node"]])
                obj["naraka_source_enabled"] = active
                obj.hide_render = not active
                obj.hide_set(obj.hide_render)
            elif particle.get("renderModeName") != "Mesh":
                report.append("Particle system %s uses %s mode; source resources are listed, but no preview geometry was generated." % (
                    particle["name"], particle.get("renderModeName", "Unknown")))

        for prefab, effect_prepared in effect_prepared_by_prefab.items():
            if not any(payload["skin"] and payload["bindposes"] for _, payload, _ in effect_prepared):
                continue
            skeleton_collection = bpy.data.collections.new(effect_collections[prefab].name + " Skeletons")
            skeleton_collection["naraka_collection_role"] = "EFFECT_SKELETONS"
            effect_collections[prefab].children.link(skeleton_collection)
            effect_rig = make_rig(skeleton_collection, effect_prepared, world, manifest["nodes"], report)
            if effect_rig:
                for renderer, _, obj in effect_prepared:
                    if not renderer.get("skinned"):
                        continue
                    obj.parent = effect_rig
                    obj.matrix_parent_inverse = world[renderer["node"]]
                    obj.matrix_basis = Matrix.Identity(4)

        mesh_payload_by_effect_identity = {
            (row["effectPrefab"], row["id"]): payload for row, payload in zip(effect_meshes, effect_mesh_payloads)}
        renderer_payload_by_effect_identity = {
            (row["effectPrefab"], row["id"]): payload for row, payload in zip(effect_renderers, effect_renderer_payloads)}
        mesh_rows_by_effect_identity = {
            (row["effectPrefab"], row.get("id", row.get("source"))): row for row in effect_meshes}
        mesh_reference_rows = {}
        for row in effect_meshes:
            mesh_reference_rows[(row["effectPrefab"], row["id"])] = row
        for row in effect_renderers:
            mesh_reference_rows[(row["effectPrefab"], row["meshSource"])] = row
        effect_inventory = []
        for effect in manifest.get("effects", []):
            prefab = effect["prefab"]
            renderer_inventory = [_mesh_inventory(row, renderer_payload_by_effect_identity[(prefab, row["id"])], manifest)
                for row in effect.get("renderers", [])]
            asset_rows = [mesh_rows_by_effect_identity[(prefab, identity)] for identity in effect.get("meshes", [])
                if (prefab, identity) in mesh_rows_by_effect_identity]
            mesh_asset_inventory = [_mesh_inventory(row, mesh_payload_by_effect_identity[(prefab, row["id"])], manifest)
                for row in asset_rows]
            particles = []
            for particle in particles_by_prefab.get(prefab, []):
                node = manifest["nodes"][particle["node"]]
                mesh_refs = []
                for identity in particle.get("meshes", []):
                    row = mesh_reference_rows.get((prefab, identity))
                    payload = (mesh_payload_by_effect_identity.get((prefab, identity)) or
                        renderer_payload_by_effect_identity.get((prefab, identity)))
                    if row is not None and payload is not None:
                        mesh_refs.append(_mesh_inventory(row, payload, manifest))
                    else:
                        mesh_refs.append({"id": identity, "name": identity, "missing": True})
                particles.append({"id": particle["source"], "name": particle["name"],
                    "node": node["name"], "nodePath": node["path"], "renderMode": particle["renderModeName"],
                    "sourceGeometry": ({"kind": "Unity built-in particle quad", "vertices": 4, "triangles": 2,
                        "authoredMesh": False} if particle["renderModeName"] in
                        ("Billboard", "Stretch", "HorizontalBillboard", "VerticalBillboard") else
                        {"kind": "particle mesh reference", "authoredMesh": True}),
                    "customMeshReferences": mesh_refs,
                    "materials": [_material_inventory(manifest, identity) for identity in particle.get("materials", [])],
                    "textures": [_texture_inventory(manifest, identity) for identity in particle.get("textures", [])]})
            effect_inventory.append({
                "prefab": prefab,
                "mounts": effect.get("mounts", []),
                "meshRenderers": renderer_inventory,
                "meshAssets": mesh_asset_inventory,
                "materials": [_material_inventory(manifest, identity) for identity in effect.get("materials", [])],
                "textures": [_texture_inventory(manifest, identity) for identity in effect.get("textures", [])],
                "particleSystems": particles,
                "components": effect.get("components"),
            })
            effect_collection = effect_collections[prefab]
            effect_collection["naraka_mesh_renderer_count"] = len(renderer_inventory)
            effect_collection["naraka_mesh_asset_count"] = len(mesh_asset_inventory)
            effect_collection["naraka_particle_system_count"] = len(particles)
        inventory_text = bpy.data.texts.new(collection.name + " effects inventory.json")
        inventory_text.write(json.dumps(effect_inventory, ensure_ascii=False, indent=2))
        effects_collection["naraka_effect_inventory"] = inventory_text.name

        created_objects = set(bpy.data.objects) - before["objects"]
        summary = {"format": "NARAKA_IMPORT_REPORT", "package": str(root), "collection": collection.name,
            "meshes": len(prepared), "materials": len(materials), "textures": len(builder.images),
            "sourceTextures": len(manifest["textures"]), "bones": len(rig.data.bones) if rig else 0,
            "effects": len(manifest.get("effects", [])), "effectMeshRenderers": len(effect_renderers),
            "effectMeshAssets": len(effect_meshes), "particleSystems": len(manifest.get("particleSystems", [])),
            "billboardPreviews": sum(o.get("naraka_resource_kind") == "billboard-preview" for o in created_objects),
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
