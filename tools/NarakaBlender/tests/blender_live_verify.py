"""Read-only checks on an already imported package in the user's Blender.

Execute via MCP with verify_import(collection). Does not clear or save a scene.
"""
import json

import bpy

from naraka_blender.format import load_package, read_mesh, safe_path
from naraka_blender.importer import local_matrix
from naraka_blender.layout import node_size


def verify_import(collection):
    root, manifest = load_package(collection["naraka_package"])
    meshes = {obj["naraka_renderer"]: obj for obj in collection.all_objects if obj.type == "MESH"}
    rigs = [obj for obj in collection.all_objects if obj.type == "ARMATURE"]
    assert len(meshes) == len(manifest["renderers"])
    assert all(obj.type in ("MESH", "ARMATURE") for obj in collection.all_objects)
    world = []
    for node in manifest["nodes"]:
        matrix = local_matrix(node)
        world.append(world[node["parent"]] @ matrix if node["parent"] >= 0 else matrix)
    rest_error, transform_error = 0.0, 0.0
    bpy.context.view_layer.update()
    for renderer in manifest["renderers"]:
        obj = meshes[renderer["id"]]
        payload = read_mesh(safe_path(root, renderer["mesh"]))
        assert len(obj.data.vertices) == payload["count"]
        assert len(obj.material_slots) == len(renderer["materialSlots"])
        transform_error = max(transform_error, max(abs(obj.matrix_world[r][c]-world[renderer["node"]][r][c]) for r in range(4) for c in range(4)))
        evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
        geometry = evaluated.to_mesh()
        try:
            rest_error = max(rest_error, max(((a.co-b.co).length for a, b in zip(obj.data.vertices, geometry.vertices)), default=0))
        finally:
            evaluated.to_mesh_clear()
    assert transform_error < 1e-6, transform_error
    assert rest_error < 1e-5, rest_error

    def rect(node):
        position, parent = node.location.copy(), node.parent
        while parent:
            position += parent.location
            parent = parent.parent
        factor = node.dimensions.x/node.width if node.dimensions.x > 0 else 1
        height = node.dimensions.y/factor if node.dimensions.y > 0 else node_size(node)[1]
        return position.x, position.y, position.x+node.width, position.y-height

    materials = {slot.material for obj in meshes.values() for slot in obj.material_slots if slot.material}
    measured, overlapping = 0, []
    for material in materials:
        nodes = [node for node in material.node_tree.nodes if node.type != "FRAME"]
        measured += sum(node.dimensions.x > 0 for node in nodes)
        for index, a in enumerate(nodes):
            ax, ay, ar, ab = rect(a)
            for b in nodes[index+1:]:
                bx, by, br, bb = rect(b)
                if min(ar,br) > max(ax,bx)+0.1 and min(ay,by) > max(ab,bb)+0.1:
                    overlapping.append([material.name, a.label, b.label])
    assert not overlapping, overlapping
    dynamic = [m for m in materials if m.get("naraka_dynamic_material")]
    frame = bpy.context.scene.frame_current
    try:
        bpy.context.scene.frame_set(1)
        times1 = [next(n for n in m.node_tree.nodes if n.type == "VALUE").outputs[0].default_value for m in dynamic]
        bpy.context.scene.frame_set(37)
        times2 = [next(n for n in m.node_tree.nodes if n.type == "VALUE").outputs[0].default_value for m in dynamic]
        assert all(b > a for a,b in zip(times1,times2))
    finally:
        bpy.context.scene.frame_set(frame)
    return {"blender": bpy.app.version_string, "meshes":len(meshes), "armatures":len(rigs),
        "empties":0, "materials":len(materials), "maxRestSkinError":rest_error,
        "maxSourceTransformError":transform_error, "uiMeasuredNodes":measured,
        "overlappingNodes":overlapping, "dynamicMaterials":len(dynamic),
        "sourceHierarchyInMetadata":len(manifest["nodes"])}
