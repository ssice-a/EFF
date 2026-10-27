"""Run in an isolated Blender: --background --factory-startup --python ... -- <package> <output>."""
import json
import math
import hashlib
import shutil
import sys
import tempfile
from pathlib import Path

import bpy
from mathutils import Vector

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from naraka_blender import register, unregister
from naraka_blender.importer import import_package, local_matrix
from naraka_blender.format import load_package, read_mesh, safe_path
from naraka_blender.layout import arrange_material, node_size

arguments = sys.argv[sys.argv.index("--") + 1:]
package, output = Path(arguments[0]).resolve(), Path(arguments[1]).resolve()
output.mkdir(parents=True, exist_ok=True)
register()
unregister()
register()
root, manifest = load_package(package)
existing = set(bpy.data.objects)
summary = import_package(package)
created = set(bpy.data.objects) - existing
all_meshes = [o for o in created if o.type == "MESH"]
meshes = [o for o in all_meshes if "naraka_renderer" in o]
effect_renderers = [o for o in created if "naraka_effect_renderer" in o]
effect_meshes = [o for o in created if "naraka_effect_mesh" in o]
billboard_previews = [o for o in created if o.get("naraka_resource_kind") == "billboard-preview"]
assert len(meshes) == len(manifest["renderers"])
assert len(effect_renderers) == len(manifest.get("effectRenderers", []))
assert len(effect_meshes) == len(manifest.get("effectMeshes", []))
assert len(billboard_previews) == sum(p["renderModeName"] in ("Billboard", "Stretch", "HorizontalBillboard", "VerticalBillboard")
    for p in manifest.get("particleSystems", []))
assert all(o.type in ("MESH", "ARMATURE") for o in created)
package_collection = bpy.data.collections[summary["collection"]]
roles = {c.get("naraka_collection_role") for c in package_collection.children}
assert roles == {"MESHES", "SKELETONS", "EFFECTS"}, roles
effects_collection = next(c for c in package_collection.children if c.get("naraka_collection_role") == "EFFECTS")
effect_collections = [c for c in effects_collection.children if c.get("naraka_collection_role") == "EFFECT_PREFAB"]
assert len(effect_collections) == len(manifest.get("effects", []))
assert all(any(c in obj.users_collection for c in effect_collections) for obj in effect_renderers + effect_meshes + billboard_previews)
assert summary["effectMeshRenderers"] == len(effect_renderers)
assert summary["effectMeshAssets"] == len(effect_meshes)
assert summary["billboardPreviews"] == len(billboard_previews)
inventory_text = bpy.data.texts[effects_collection["naraka_effect_inventory"]]
effect_inventory = json.loads(inventory_text.as_string())
assert len(effect_inventory) == len(manifest.get("effects", []))
assert all("logicalPath" in texture for effect in effect_inventory for texture in effect["textures"])
assert all("sourceGeometry" in particle for effect in effect_inventory for particle in effect["particleSystems"])
for obj in billboard_previews:
    assert obj["naraka_preview_only"] is True
    assert "built-in billboard quad" in obj["naraka_source_mesh"]
    assert len(obj.data.vertices) == 4 and len(obj.data.polygons) == 1
    assert len(obj.data.uv_layers) == 1
    assert all(slot.material for slot in obj.material_slots)
world = []
for node in manifest["nodes"]:
    local = local_matrix(node)
    world.append(world[node["parent"]] @ local if node["parent"] >= 0 else local)
by_identity = {o["naraka_renderer"]: o for o in meshes}
bpy.context.view_layer.update()
max_rest_error = 0.0
for renderer in manifest["renderers"]:
    obj = by_identity[renderer["id"]]
    assert max(abs(obj.matrix_world[r][c] - world[renderer["node"]][r][c]) for r in range(4) for c in range(4)) < 1e-6
    assert obj.users_collection[0].get("naraka_collection_role") == "LOD"
    payload = read_mesh(safe_path(root, renderer["mesh"]))
    assert len(obj.data.vertices) == payload["count"]
    assert len(obj.material_slots) == len(renderer["materialSlots"])
    assert len(obj.data.polygons) == len(payload["indices"]) // 3
    for slot, reference in zip(obj.material_slots, renderer["materialSlots"]):
        assert slot.material.get("naraka_source") == reference if reference else slot.material is None
    assert all(math.isfinite(v) for vertex in obj.data.vertices for v in vertex.co)
    # The reconstructed bindpose skeleton must leave the imported rest geometry unchanged.
    if obj.modifiers:
        assert obj.parent == obj.modifiers[0].object
        evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
        checked = evaluated.to_mesh()
        try:
            for a, b in zip(obj.data.vertices, checked.vertices):
                max_rest_error = max(max_rest_error, (a.co - b.co).length)
        finally:
            evaluated.to_mesh_clear()
for renderer in manifest.get("effectRenderers", []):
    obj = next(o for o in effect_renderers if o["naraka_effect_renderer"] == renderer["id"])
    assert obj["naraka_resource_kind"] == "effect-mesh-renderer"
    assert max(abs(obj.matrix_world[r][c] - world[renderer["node"]][r][c]) for r in range(4) for c in range(4)) < 1e-6
    assert obj.users_collection[0].get("naraka_collection_role") == "EFFECT_PREFAB"
for source_mesh in manifest.get("effectMeshes", []):
    obj = next(o for o in effect_meshes if o["naraka_effect_mesh"] == source_mesh["id"])
    assert obj["naraka_resource_kind"] == "effect-mesh-asset"
    assert max(abs(obj.matrix_world[r][c] - world[source_mesh["node"]][r][c]) for r in range(4) for c in range(4)) < 1e-6
    assert obj.users_collection[0].get("naraka_collection_role") == "EFFECT_PREFAB"
assert max_rest_error < 0.001, max_rest_error
summary["maxRestSkinError"] = max_rest_error
assert all(image.has_data and all(v > 0 for v in image.size) for image in bpy.data.images if image.get("naraka_source"))
def recursive_nodes(tree, seen=None):
    seen=set() if seen is None else seen
    if tree in seen:return []
    seen.add(tree)
    result=list(tree.nodes)
    for n in tree.nodes:
        if n.type=='GROUP':result+=recursive_nodes(n.node_tree,seen)
    return result

for material in bpy.data.materials:
    if material.get("naraka_source"):
        assert material.use_nodes
        assert any(n.type in ("BSDF_PRINCIPLED", "EMISSION") for n in recursive_nodes(material.node_tree))
        assert next(n for n in material.node_tree.nodes if n.type == "OUTPUT_MATERIAL").inputs["Surface"].is_linked
        assert any(n.type == "TEX_IMAGE" and n.image for n in recursive_nodes(material.node_tree))
        assert json.loads(material["naraka_conversion_status_json"])["surface"] == "approximate"
        tree = material.node_tree
        before_links = [(l.from_node.as_pointer(), l.from_socket.identifier, l.to_node.as_pointer(), l.to_socket.identifier) for l in tree.links]
        arrange_material(material)
        assert before_links == [(l.from_node.as_pointer(), l.from_socket.identifier, l.to_node.as_pointer(), l.to_socket.identifier) for l in tree.links]
        nodes = [n for n in tree.nodes if n.type != "FRAME"]
        def bounds(n):
            x, y = n.location
            parent = n.parent
            while parent:
                x += parent.location.x
                y += parent.location.y
                parent = parent.parent
            width, height = node_size(n)
            return x, y, x + width, y - height
        for i, a in enumerate(nodes):
            ax, ay, ar, ab = bounds(a)
            for b in nodes[i + 1:]:
                bx, by, br, bb = bounds(b)
                assert ar <= bx or br <= ax or ab >= by or bb >= ay, (material.name, a.label, b.label)

for effect in manifest.get("effects", []):
    for identity in effect.get("materials", []):
        source = manifest["materials"][identity]
        expected = {binding.get("source") or binding.get("texture") for binding in source.get("textures", {}).values()
            if binding.get("source") or binding.get("texture")}
        material = next(m for m in bpy.data.materials if m.get("naraka_source") == identity)
        actual = {node.image.get("naraka_source") for node in recursive_nodes(material.node_tree)
            if node.type == "TEX_IMAGE" and node.image}
        assert expected <= actual, (source["name"], expected - actual)
        if source["shader"].lower().endswith("fx_pyro_uber.shadergraph"):
            assert "Main texture RGB preview" in material["naraka_material_preview_only"]
            source_alpha = [node for node in material.node_tree.nodes if node.type == "TEX_IMAGE"
                and node.get("naraka_binding") in {"_MainTextureTransmittance", "_MaskTexture"}]
            assert len(source_alpha) == 2 and all(not node.outputs["Alpha"].is_linked for node in source_alpha)
            status = json.loads(material["naraka_conversion_status_json"])
            assert {"_MainTextureTransmittance", "_MaskTexture"} <= set(status["retainedTextureBindings"])

# A rig object transform moves each mesh once; eliminating Empty parents must
# not remove group movement or apply a second skin-space transformation.
rigs = [o for o in created if o.type == "ARMATURE"]
if rigs:
    rig = rigs[0]
    before_matrices = {obj: obj.matrix_world.copy() for obj in meshes}
    original = rig.matrix_world.copy()
    rig.location.x += 0.25
    bpy.context.view_layer.update()
    for obj in meshes:
        assert abs(obj.matrix_world.translation.x - before_matrices[obj].translation.x - 0.25) < 1e-6
    rig.matrix_world = original
    bpy.context.view_layer.update()

dynamic = [m for m in bpy.data.materials if m.get("naraka_dynamic_material")]
if any("periodic2" in m["shader"] and m["floats"].get("_MFX_EMISSIVE_PERIODIC_2", 0) for m in manifest["materials"].values()):
    assert dynamic, "Expected the sample's periodic material conversion"
bpy.context.scene.frame_set(1)
times1 = [next(n for n in m.node_tree.nodes if n.type == "VALUE").outputs[0].default_value for m in dynamic]
bpy.context.scene.frame_set(37)
times2 = [next(n for n in m.node_tree.nodes if n.type == "VALUE").outputs[0].default_value for m in dynamic]
assert all(b > a for a, b in zip(times1, times2)), (times1, times2)
summary["dynamicMaterials"] = len(dynamic)

# Invalid packages fail before scene changes; late material failures roll back
# everything created by that import and preserve the original package objects.
with tempfile.TemporaryDirectory(prefix="naraka-import-test-") as temporary:
    fixture = Path(temporary)
    shutil.copytree(root, fixture / "package", dirs_exist_ok=True)
    fixture /= "package"
    manifest_path = fixture / "naraka.json"
    manifest_path.write_text("broken", encoding="utf-8")
    counts = {kind: len(getattr(bpy.data, kind)) for kind in ("objects", "meshes", "materials", "collections", "images")}
    try:
        import_package(manifest_path)
        raise AssertionError("Corrupted manifest accepted")
    except (ValueError, json.JSONDecodeError):
        pass
    assert counts == {kind: len(getattr(bpy.data, kind)) for kind in counts}
    modified = json.loads((root / "naraka.json").read_text(encoding="utf-8-sig"))
    first_material = next(iter(modified["materials"].values()))
    first_binding = next(v for v in first_material["textures"].values() if v["texture"])
    first_binding["texture"] = "missing-texture-identity"
    manifest_path.write_text(json.dumps(modified), encoding="utf-8")
    receipts = json.loads((fixture / "checksums.json").read_text(encoding="utf-8-sig"))
    receipts["naraka.json"] = hashlib.sha256(manifest_path.read_bytes()).hexdigest()
    (fixture / "checksums.json").write_text(json.dumps(receipts), encoding="utf-8")
    try:
        import_package(manifest_path)
        raise AssertionError("Missing texture identity accepted")
    except KeyError:
        pass
    assert counts == {kind: len(getattr(bpy.data, kind)) for kind in counts}

# Reimport creates an independent subtree and must not modify the first import.
before_materials = {o: tuple(s.material for s in o.material_slots) for o in meshes}
before_second = {kind: set(getattr(bpy.data, kind)) for kind in ("objects", "meshes", "armatures", "materials", "node_groups", "images", "collections", "texts")}
second = import_package(package)
assert all(tuple(s.material for s in o.material_slots) == mats for o, mats in before_materials.items())
for kind in ("objects", "meshes", "armatures", "materials", "node_groups", "images", "collections", "texts"):
    block = getattr(bpy.data, kind)
    for item in set(block) - before_second[kind]:
        block.remove(item, do_unlink=True)

# A nested rotated/nonuniform hierarchy can contain shear. Flattening it to
# Mesh/Skeleton resources must keep the exact source world-space geometry.
with tempfile.TemporaryDirectory(prefix="naraka-transform-test-") as temporary:
    fixture = Path(temporary) / "package"
    shutil.copytree(root, fixture)
    transformed = json.loads((fixture / "naraka.json").read_text(encoding="utf-8-sig"))
    transformed["nodes"][0]["position"] = [0.3, -0.2, 0.4]
    transformed["nodes"][0]["rotation"] = [0, math.sin(0.23), 0, math.cos(0.23)]
    transformed["nodes"][0]["scale"] = [1.2, 0.8, 1.1]
    expected_world = []
    for node in transformed["nodes"]:
        local = local_matrix(node)
        expected_world.append(expected_world[node["parent"]] @ local if node["parent"] >= 0 else local)
    manifest_path = fixture / "naraka.json"
    manifest_path.write_text(json.dumps(transformed), encoding="utf-8")
    receipts = json.loads((fixture / "checksums.json").read_text(encoding="utf-8-sig"))
    receipts["naraka.json"] = hashlib.sha256(manifest_path.read_bytes()).hexdigest()
    (fixture / "checksums.json").write_text(json.dumps(receipts), encoding="utf-8")
    before_transformed = {kind: set(getattr(bpy.data, kind)) for kind in ("objects", "meshes", "armatures", "materials", "node_groups", "images", "collections", "texts")}
    import_package(manifest_path)
    bpy.context.view_layer.update()
    actual = {o["naraka_renderer"]:o for o in set(bpy.data.objects)-before_transformed["objects"]
        if o.type == "MESH" and "naraka_renderer" in o}
    for renderer in transformed["renderers"]:
        obj = actual[renderer["id"]]
        assert max(abs(obj.matrix_world[r][c] - expected_world[renderer["node"]][r][c]) for r in range(4) for c in range(4)) < 1e-6
        if obj.modifiers:
            evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
            geometry = evaluated.to_mesh()
            try:
                assert max(((a.co-b.co).length for a,b in zip(obj.data.vertices,geometry.vertices)), default=0) < 1e-5
            finally:
                evaluated.to_mesh_clear()
    for kind in before_transformed:
        block = getattr(bpy.data, kind)
        for item in set(block)-before_transformed[kind]:
            block.remove(item, do_unlink=True)

if "--checks-only" in arguments:
    (output / "verification.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")
    print("NARAKA_IMPORT_PASS " + json.dumps({k: v for k, v in summary.items() if k not in ("warnings", "materialAdapters", "materialStatus")}, ensure_ascii=False))
    unregister()
    raise SystemExit(0)

for obj in existing:
    bpy.data.objects.remove(obj, do_unlink=True)
visible = [obj for obj in meshes if not obj.hide_render]
points = [obj.matrix_world @ Vector(corner) for obj in visible for corner in obj.bound_box]
low = Vector(tuple(min(v[i] for v in points) for i in range(3)))
high = Vector(tuple(max(v[i] for v in points) for i in range(3)))
center = (low + high) * 0.5
height = max(high - low)
summary["bounds"] = [list(low), list(high)]
summary["visibleMeshes"] = len(visible)
scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.samples = 16
scene.render.resolution_x, scene.render.resolution_y, scene.render.resolution_percentage = 800, 1000, 100
scene.world.color = (0.12, 0.12, 0.12)
camera_data = bpy.data.cameras.new("Naraka preview camera")
camera = bpy.data.objects.new(camera_data.name, camera_data)
scene.collection.objects.link(camera)
camera.location = center + Vector((height * 1.2, -height * 2.8, height * 0.45))
camera.rotation_euler = (center - camera.location).to_track_quat("-Z", "Y").to_euler()
camera_data.type = "ORTHO"
camera_data.ortho_scale = height * 1.3
scene.camera = camera
for name, location, energy, color in (
    ("Key", (2, -3, 4), 400, (1, 0.9, 0.8)),
    ("Fill", (-3, -1, 2), 200, (0.65, 0.8, 1)),
    ("Rim", (0, 2, 3), 500, (0.65, 0.8, 1)),
):
    data = bpy.data.lights.new(name, "AREA")
    obj = bpy.data.objects.new(name, data)
    scene.collection.objects.link(obj)
    obj.location = center + Vector(location) * height / 2
    obj.rotation_euler = (center - obj.location).to_track_quat("-Z", "Y").to_euler()
    data.energy, data.color, data.shape, data.size = energy, color, "DISK", height * 2
scene.render.image_settings.file_format = "PNG"
scene.render.filepath = str(output / "preview.png")
scene.frame_set(1)
bpy.ops.wm.save_as_mainfile(filepath=str(output / "sample.blend"))
bpy.ops.render.render(write_still=True)
if "--two-frames" in arguments:
    scene.frame_set(37)
    scene.render.filepath = str(output / "preview-frame37.png")
    bpy.ops.render.render(write_still=True)
    assert hashlib.sha256((output / "preview.png").read_bytes()).digest() != hashlib.sha256((output / "preview-frame37.png").read_bytes()).digest()
(output / "verification.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")
print("NARAKA_IMPORT_PASS " + json.dumps({k: v for k, v in summary.items() if k not in ("warnings", "materialAdapters", "materialStatus")}, ensure_ascii=False))
