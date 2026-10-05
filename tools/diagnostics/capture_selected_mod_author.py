"""Capture a real selected Blender Mod as an offline native-packer input fixture.

Executed inside the already-open Blender through MCP. It preserves the existing
selection contract, does not save the .blend, and never deploys to the game.
Editable v6 files are inspection inputs, not native game resources.
"""
from pathlib import Path
import hashlib
import json
import sys
import time
import traceback

import bpy


def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def capture(output, baseline_package):
    output = Path(output).resolve()
    baseline_package = Path(baseline_package).resolve()
    if output.exists():
        raise FileExistsError("Refusing to overwrite a previous capture: " + str(output))
    if bpy.context.mode != "OBJECT":
        raise ValueError("Capture requires Object mode")
    addon = next(m for name, m in sys.modules.items() if name.endswith("eiem_blender_addon"))
    objects, rigs = addon.selected_eiem_resources()
    objects = list(objects)
    plan = addon.plan_switch_export(objects)
    selected_before = {o.as_pointer() for o in bpy.context.selected_objects}
    active_before = bpy.context.view_layer.objects.active
    levels = addon.lod_levels_for_export(objects, all_levels=True)
    source = json.loads((baseline_package / "source/manifest.json").read_text(encoding="utf-8"))
    if source.get("format") != "EFFSOURCE" or source.get("version") != 1:
        raise ValueError("Unsupported source baseline")
    output.mkdir(parents=True)
    (output / "parts").mkdir()
    errors = []
    rows = []
    materials = {}
    started = time.perf_counter()
    for index, obj in enumerate(objects):
        row = dict(object=obj.name, mesh=obj.data.name,
                   sourcePath=str(obj.data.get("eiem_target_path", "") or obj.data.get("eiem_source", "")),
                   sourceAsset=str(obj.data.get("eiem_target_asset", "") or obj.data.get("eiem_asset", "")),
                   authorPackage=str(obj.get("eiem_author_package", "")),
                   prefab=str(obj.get("eiem_prefab_path", "")),
                   vertices=len(obj.data.vertices), polygons=len(obj.data.polygons),
                   hidden=obj in plan["hidden"], bindings=plan["bindings"].get(obj, []),
                   rig=obj.find_armature().name if obj.find_armature() else None,
                   materials=[], shapeControls=[], shapeNames=[], modifiers=[m.type for m in obj.modifiers])
        if not row["hidden"]:
            for slot, mat in enumerate(obj.data.materials):
                if mat is None:
                    row["materials"].append(dict(slot=slot, material=None))
                    continue
                row["materials"].append(dict(slot=slot, material=mat.name))
                # Preserve editable property payload verbatim. Exclude large native
                # baseline copies, which remain separately in the source package.
                if mat.name not in materials:
                    materials[mat.name] = {str(k): v if isinstance(v, (str, bool, int, float)) else str(v)
                                           for k, v in mat.items() if str(k).startswith("eiem_")}
            keys = obj.data.shape_keys
            row["shapeNames"] = [k.name for k in keys.key_blocks if k != keys.reference_key] if keys else []
            for control in obj.data.eiem_shape_controls:
                row["shapeControls"].append({k: getattr(control, k) for k in
                    ("shape", "enabled", "identity", "automatic", "label", "default", "minimum", "maximum",
                     "hotkey_increase", "hotkey_decrease", "hotkey_speed") if hasattr(control, k)})
            file = "parts/%03d.mesh" % index
            t0 = time.perf_counter()
            try:
                addon.write_mesh(output / file, obj)
                row["authorFile"] = file
                row["sha256"] = hashlib.sha256((output / file).read_bytes()).hexdigest().upper()
                row["exportSeconds"] = time.perf_counter() - t0
            except Exception as error:
                row["error"] = type(error).__name__ + ": " + str(error)
                errors.append(dict(stage="author-part", object=obj.name, error=row["error"], traceback=traceback.format_exc()))
        rows.append(row)
    report = dict(format="EFF_AUTHOR_VALIDATION", version=1, nativePack=False,
                  blendFile=bpy.data.filepath, blenderVersion=bpy.app.version_string,
                  addonFile=addon.__file__, addonVersion=addon.bl_info.get("version"),
                  selectionContract="selected_eiem_resources", sourceBaseline=str(baseline_package),
                  vfsFingerprint=source["vfsFingerprint"], lodLevels=levels,
                  sourceGroups=[[o.name for o in group] for group in plan["sources"]],
                  switches=[dict(name=group.name, stateCount=len(states), default=default,
                                 key=key, variable=variable)
                            for group, states, default, key, variable in plan["groups"]],
                  objects=rows, materials=materials, errors=errors,
                  authorPartCaptureSeconds=time.perf_counter() - started)
    write_json(output / "capture.json", report)
    # Use the existing export solely to collect its material/texture/control
    # dependency closure for comparison; it is not a native output/fallback route.
    if not errors:
        try:
            t0 = time.perf_counter()
            report["existingAuthorExport"] = addon.export_package(
                output / "existing-author-export", mesh_objects=objects, armatures=rigs,
                physics_objects=[], lod_levels=levels, include_switches=True)
            report["existingAuthorExportSeconds"] = time.perf_counter() - t0
        except Exception as error:
            errors.append(dict(stage="existing-author-export", error=str(error), traceback=traceback.format_exc()))
    report["selectionPreserved"] = selected_before == {o.as_pointer() for o in bpy.context.selected_objects}
    report["activePreserved"] = active_before == bpy.context.view_layer.objects.active
    if not report["selectionPreserved"] or not report["activePreserved"]:
        errors.append(dict(stage="scene-contract", error="Export changed selection or active object"))
    write_json(output / "capture.json", report)
    print(json.dumps(dict(output=str(output), selected=len(rows), hidden=sum(r["hidden"] for r in rows),
                         partFiles=sum("authorFile" in r for r in rows), sourceGroups=len(plan["sources"]),
                         errors=errors, seconds=time.perf_counter() - started), ensure_ascii=False))
    return report
