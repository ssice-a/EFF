"""Preflight a captured Blender selection against an EFFSOURCE baseline.

This writes evidence and an assembly plan only. EFFMESH fixtures and the old
author dependency export are not native resources or an alternative load path.
No Blender scene, source package, Mod deployment, or game file is modified.
"""
from __future__ import annotations

import argparse
from collections import defaultdict
import hashlib
import json
import math
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "Blender"))
from eiem_format import parse_ini, read_flat_properties, read_mesh, safe_path


def check(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


class SourceCatalog:
    def __init__(self, package):
        self.package = package
        self.manifest = json.loads((package / "source/manifest.json").read_text(encoding="utf-8"))
        check((self.manifest["format"], self.manifest["version"]) == ("EFFSOURCE", 1), "source format")
        self.resources = {r["identity"]: r for r in self.manifest["resources"]}
        check(len(self.resources) == len(self.manifest["resources"]), "duplicate resource identities")
        self.logical = defaultdict(list)
        self.uses = defaultdict(list)
        self.nodes = {n["transform"]: n for n in self.manifest["hierarchy"]}
        self.native = {}
        self.meshes = {}
        for file in self.manifest["serializedFiles"]:
            for obj in file["objects"]:
                identity = file["cab"].lower() + ":" + str(obj["pathId"])
                check(identity not in self.native, "duplicate native identity: " + identity)
                self.native[identity] = obj
        for resource in self.resources.values():
            self.logical[(resource["type"], resource["logicalPath"], resource["name"])].append(resource)
            native = self.native.get(resource["identity"])
            check(native and all(native[k] == resource["native"][k] for k in ("offset", "size", "classId")),
                  "native resource range: " + resource["identity"])
        for use in self.manifest["rendererUses"]:
            if use["mesh"] and not use["mesh"]["isNull"]:
                self.pointer(use["mesh"], 43)
                self.uses[use["mesh"]["identity"]].append(use)

    def resolve(self, kind, path, asset):
        values = self.logical[(kind, path, asset)]
        check(len(values) == 1, f"{kind} exact logical path + asset has {len(values)} matches: {path} / {asset}")
        return values[0]

    def pointer(self, pointer, class_id):
        check(pointer and not pointer["isNull"] and pointer["resolved"], "unresolved native pointer")
        obj = self.native.get(pointer["identity"])
        check(obj and obj["classId"] == class_id, "native pointer class: " + str(pointer["identity"]))
        return pointer["identity"]

    def mesh(self, resource):
        key = resource["identity"]
        if key not in self.meshes:
            self.meshes[key] = read_mesh(safe_path(self.package, resource["authorFile"]))
        return self.meshes[key]

    def palette(self, mesh):
        result = []
        for slot, candidates in enumerate(mesh["bone_source_candidates"]):
            check(candidates, f"bone {slot} has no source donor (new bones are deferred)")
            transform_ids = set()
            donor_evidence = []
            for path, asset, donor_slot in candidates:
                resource = self.resolve("Mesh", path, asset)
                check(donor_slot < len(self.mesh(resource)["bindposes"]), f"donor bindpose slot: {asset}/{donor_slot}")
                uses = self.uses[resource["identity"]]
                check(uses, "donor has no source Renderer")
                for use in uses:
                    check(donor_slot < len(use["bones"]), f"donor Renderer slot: {asset}/{donor_slot}")
                    transform = self.pointer(use["bones"][donor_slot], 4)
                    node = self.nodes.get(transform)
                    check(node and node["path"] == mesh["bone_paths"][slot], f"donor bone path mismatch at {slot}")
                    transform_ids.add(transform)
                    donor_evidence.append(dict(mesh=resource["identity"], renderer=use["identity"], slot=donor_slot))
            check(len(transform_ids) == 1, f"ambiguous donor Transform at palette slot {slot}")
            check(tuple(mesh["bone_sources"][slot]) == tuple(candidates[0]), f"first donor mismatch at {slot}")
            result.append(dict(slot=slot, path=mesh["bone_paths"][slot], transform=next(iter(transform_ids)),
                               donors=donor_evidence))
        return result

    def calibrated_bindposes(self, resource, author_mesh):
        """Compare author matrices to the current, independently exported source.

        Donor meshes can have different bind frames. Establish each conversion
        through common native palette paths, checking every shared anchor.
        This never treats a donor's raw matrix as interchangeable by name.
        """
        import numpy as np

        def matrix(values):
            return np.asarray(values, dtype=np.float64).reshape((4, 4), order="F")

        original = self.mesh(resource)
        expected = {path: matrix(values) for path, values in zip(original["bone_paths"], original["bindposes"])}
        donors = {self.resolve("Mesh", path, asset)["identity"]
                  for slot in author_mesh["bone_source_candidates"] for path, asset, _index in slot}
        pending = {identity: self.mesh(self.resources[identity]) for identity in sorted(donors)}
        while pending:
            progress = False
            for identity, mesh in list(pending.items()):
                raw = {path: matrix(values) for path, values in zip(mesh["bone_paths"], mesh["bindposes"])}
                common = sorted(raw.keys() & expected.keys())
                if not common:
                    continue
                anchor = common[0]
                basis = np.linalg.inv(raw[anchor]) @ expected[anchor]
                converted = {path: value @ basis for path, value in raw.items()}
                check(all(float(np.max(np.abs(converted[path] - expected[path]))) <= 1e-4 for path in common),
                      "current native donor bind frames disagree: " + identity)
                expected.update({path: value for path, value in converted.items() if path not in expected})
                del pending[identity]
                progress = True
            if not progress:
                break
        difference = 0.0
        for path, values in zip(author_mesh["bone_paths"], author_mesh["bindposes"]):
            check(path in expected, "no native bind-frame bridge for bone: " + path)
            delta = float(np.max(np.abs(matrix(values) - expected[path])))
            check(delta <= 1e-4, "author/native calibrated bindpose differs: " + path)
            difference = max(difference, delta)
        return difference


def audit_geometry(mesh, row):
    count = mesh["vertex_count"]
    check(count > 0 and mesh["coordinate"] == "unity-y-up-left-handed", "vertex count/coordinate")
    check(mesh["source"] == row["sourcePath"] and mesh["name"] == row["sourceAsset"], "fixture source differs from capture")
    dimensions = {"vertices": 3, "normals": 3, "tangents": 4, "colors": 4}
    for key, dimension in dimensions.items():
        values = mesh[key]
        check((not values and key == "colors") or len(values) == count * dimension, key + " layout")
        check(all(math.isfinite(v) for v in values), key + " non-finite")
    uv_dimensions = []
    for values in mesh["uvs"]:
        dimension = len(values) // count
        check(not values or len(values) % count == 0 and 2 <= dimension <= 4, "UV layout")
        check(all(math.isfinite(v) for v in values), "UV non-finite")
        uv_dimensions.append(dimension)
    check(mesh["index_count"] == len(mesh["indices"]) and mesh["index_count"] > 0, "index count")
    check(len(mesh["submeshes"]) == 1, "fixture must have one material part")
    topology, start, size, base, first, vertices = mesh["submeshes"][0]
    check((topology, start, size, base, first, vertices) == (0, 0, len(mesh["indices"]), 0, 0, count), "triangle submesh range")
    check(size % 3 == 0 and all(i < count for i in mesh["indices"]), "triangle indices outside vertex buffer")
    check(len(mesh["skin"]) == count, "skin layout")
    palette_count = len(mesh["bindposes"])
    check(all(len(mesh[k]) == palette_count for k in
              ("bone_paths", "bone_hashes", "bone_index_paths", "bone_sources", "bone_source_candidates")), "palette layout")
    check(len(set(mesh["bone_paths"])) == palette_count, "duplicate palette paths")
    check(all(len(matrix) == 16 and all(math.isfinite(v) for v in matrix) for matrix in mesh["bindposes"]), "bindpose layout/finite")
    used_bones, max_error = set(), 0.0
    for weights, indices in mesh["skin"]:
        check(len(weights) == 4 and len(indices) == 4 and all(math.isfinite(w) and w >= 0 for w in weights), "skin weights")
        error = abs(sum(weights) - 1)
        max_error = max(max_error, error)
        check(error <= 1e-5 and all(b < palette_count for b in indices), "skin normalization/bone range")
        used_bones.update(b for w, b in zip(weights, indices) if w > 0)
    for index, position, normal, tangent in mesh["blend_vertices"]:
        check(index < count and all(math.isfinite(v) for v in (*position, *normal, *tangent)), "shape delta range/finite")
    for name, start, size, *_flags in mesh["blend_frames"]:
        check(start + size <= len(mesh["blend_vertices"]), "shape frame range: " + name)
        indices = [v[0] for v in mesh["blend_vertices"][start:start+size]]
        check(len(set(indices)) == len(indices), "duplicate shape vertex: " + name)
    channels = []
    for name, channel_hash, start, size in mesh["blend_channels"]:
        check(size > 0 and start + size <= len(mesh["blend_frames"]), "shape channel range: " + name)
        check(all(f[0] == name for f in mesh["blend_frames"][start:start+size]), "shape channel/frame name")
        channels.append(dict(name=name, hash=channel_hash, frameWeights=mesh["blend_weights"][start:start+size]))
    check(len({c["name"] for c in channels}) == len(channels), "duplicate shape channels")
    check({c["name"] for c in channels} == set(row["shapeNames"]), "shape channels differ from author selection")
    check(all(math.isfinite(w) and w > 0 for w in mesh["blend_weights"]), "shape frame weight")
    check(not mesh["additional"] or len(mesh["additional"]) == count, "additional vertex layout")
    check(all(math.isfinite(v) for vector in mesh["additional"] for v in vector), "additional vertex non-finite")
    return dict(authorVertices=row["vertices"], outputVertices=count, indices=len(mesh["indices"]), triangles=len(mesh["indices"]) // 3,
                uvDimensions=uv_dimensions, requires32BitIndices=max(mesh["indices"]) > 65535,
                paletteSlots=palette_count, positiveWeightedSlots=len(used_bones), maxWeightSumError=max_error,
                shapes=channels)


def audit_materials(capture, fixture, source, ini):
    from PIL import Image

    materials, textures = {}, {}
    author_export = fixture / "existing-author-export"
    for name, props in capture["materials"].items():
        path, asset = props["eiem_target_path"], props["eiem_target_asset"]
        resource = source.resolve("Material", path, asset)
        details = resource["details"]
        shader = source.pointer(details["shader"], 48)
        native_textures = {}
        for tex in details["textures"]:
            if not tex["pointer"]["isNull"]:
                native_textures[tex["property"]] = source.pointer(tex["pointer"], 28)
        section = props["eiem_section"]
        overrides = {}
        if ini.has_section(section):
            declaration = dict(ini.items(section))
            check((declaration["target.path"], declaration["target.asset"]) == (path, asset), "Material declaration identity")
            payload = read_flat_properties(safe_path(author_export, declaration["path"]))
            check((payload["format"], payload["version"], payload["overrides"]) == ("EFFMAT", "1", "true"), "Material author delta format")
            check((payload["source"], payload["name"]) == (path, asset), "Material author delta identity")
            overrides = {k: v for k, v in payload.items() if k not in ("format", "version", "overrides", "source", "name")}
            for key, value in overrides.items():
                if not key.startswith("texture."):
                    continue
                prop = key[len("texture."):]
                check(prop in {t["property"] for t in details["textures"]}, "authored texture property absent from source Material")
                check(ini.has_section(value) and value.startswith("Texture"), "texture delta dependency absent")
                texture_path = safe_path(author_export, ini[value]["path"])
                image_path = Path(props["eiem_" + key])
                with Image.open(texture_path) as a, Image.open(image_path) as b:
                    a.load()
                    b.load()
                    check(a.size == b.size and a.convert("RGBA").tobytes() == b.convert("RGBA").tobytes(), "exported texture pixels changed")
                    dimensions = list(a.size)
                linear = ini[value].get("linear")
                check(linear in ("true", "false"), "texture color-space declaration")
                if prop == "_BumpMap":
                    check(linear == "true", "normal-map texture must be linear")
                textures[value] = dict(file=str(texture_path), authorFile=str(image_path), sha256=digest(texture_path),
                                       dimensions=dimensions, linear=linear == "true", nativeEncoded=False)
        materials[name] = dict(sourcePath=path, sourceAsset=asset, sourceIdentity=resource["identity"],
                               shader=shader, nativeTextures=native_textures, authorOverrides=overrides,
                               sourceNativeBytesPreserved=True)
    declared = {s for s in ini.sections() if s.startswith("Texture")}
    check(declared == textures.keys(), "unused/missing texture dependency")
    return materials, textures


def build_plan(capture, source, parts):
    by_name = {r["object"]: r for r in capture["objects"]}
    rules = []
    for group in capture["sourceGroups"]:
        rows = [by_name[name] for name in group]
        keys = {(r["sourcePath"], r["sourceAsset"]) for r in rows}
        check(len(keys) == 1, "source group has mixed identities")
        path, asset = next(iter(keys))
        resource = source.resolve("Mesh", path, asset)
        source_uses = source.uses[resource["identity"]]
        check(len(source_uses) == 1, "LOD author fixture requires one unambiguous source assembly context")
        owner = source_uses[0]["ownerPath"]
        check("/lod0/" in owner and owner.endswith("_lod0"), "unexpected LOD0 owner structure")
        targets = []
        # Resolve the current export's all-LOD contract through exact hierarchy
        # use -> Mesh PPtr. Asset names alone never establish resource identity.
        for level in capture["lodLevels"]:
            expected_owner = owner.replace("/lod0/", f"/lod{level}/").removesuffix("_lod0") + f"_lod{level}"
            contexts = [u for u in source.manifest["rendererUses"] if u["ownerPath"] == expected_owner]
            check(len(contexts) <= 1, "ambiguous LOD assembly owner")
            if not contexts:
                continue  # This source family genuinely has no such LOD.
            identity = source.pointer(contexts[0]["mesh"], 43)
            target = source.resources[identity]
            check(target["name"] == asset.removesuffix("_lod0") + f"_lod{level}", "LOD asset/context inconsistency")
            uses = []
            for use in source.uses[identity]:
                source.pointer(use["rootBone"], 4)
                source.pointer(use["skinningRoot"], 4)
                source.pointer(dict(isNull=False, resolved=True, identity=use["transform"]), 4)
                original_materials = [source.pointer(s["pointer"], 21) for s in use["materials"]]
                generated = []
                for row in rows:
                    if row["hidden"]:
                        continue
                    part = parts[row["object"]]
                    generated.append(dict(part=part["partId"], meshAuthorFile=row["authorFile"],
                                          material=row["materials"][0]["material"],
                                          bones=[b["transform"] for b in part["palette"]],
                                          visibilityBindings=row["bindings"], shapeControls=row["shapeControls"]))
                uses.append(dict(renderer=use["identity"], ownerPath=use["ownerPath"],
                                 originalMaterialRoles=original_materials, rootBone=use["rootBone"]["identity"],
                                 skinningRoot=use["skinningRoot"]["identity"], transform=use["transform"],
                                 generatedRenderers=generated, originalBounds=use["bounds"],
                                 boundsMustBeRebuiltFromAuthorMesh=True, preserveAssemblyTransform=True))
            targets.append(dict(level=level, sourcePath=target["logicalPath"], sourceAsset=target["name"],
                                sourceIdentity=identity, uses=uses))
        rules.append(dict(authorSourcePath=path, authorSourceAsset=asset, sourceIdentity=resource["identity"],
                          hidden=all(r["hidden"] for r in rows), parts=[parts[r["object"]]["partId"] for r in rows if not r["hidden"]],
                          targets=targets))
    return rules


def audit(capture_path):
    fixture = capture_path.parent
    capture = json.loads(capture_path.read_text(encoding="utf-8"))
    check((capture["format"], capture["version"], capture["nativePack"]) == ("EFF_AUTHOR_VALIDATION", 1, False), "capture format")
    check(not capture["errors"] and capture["selectionPreserved"] and capture["activePreserved"], "capture failed/changed selection")
    source = SourceCatalog(Path(capture["sourceBaseline"]))
    check(source.manifest["vfsFingerprint"] == capture["vfsFingerprint"], "source VFS mismatch")
    check(capture["selectionContract"] == "selected_eiem_resources", "selection contract changed")
    objects = capture["objects"]
    check(len({r["object"] for r in objects}) == len(objects), "duplicate author objects")
    grouped = [name for group in capture["sourceGroups"] for name in group]
    check(sorted(grouped) == sorted(r["object"] for r in objects), "source groups differ from selection")
    switches = {s["variable"]: s for s in capture["switches"]}
    check(len(switches) == len(capture["switches"]), "duplicate switch variables")
    ini = parse_ini(fixture / "existing-author-export")
    materials, textures = audit_materials(capture, fixture, source, ini)
    parts = {}
    for row in objects:
        resource = source.resolve("Mesh", row["sourcePath"], row["sourceAsset"])
        if row["hidden"]:
            check("authorFile" not in row, "hidden object exported geometry")
            continue
        check(len(row["materials"]) == 1 and row["materials"][0]["slot"] == 0 and
              row["materials"][0]["material"] in materials, "part material layout")
        file = safe_path(fixture, row["authorFile"])
        check(digest(file) == row["sha256"], "fixture hash mismatch")
        mesh = read_mesh(file)
        geometry = audit_geometry(mesh, row)
        original = source.mesh(resource)
        slots = len(original["bindposes"])
        check(mesh["bone_paths"][:slots] == original["bone_paths"], "original bone slots changed")
        check(mesh["bindposes"][:slots] == original["bindposes"], "original bindpose values changed")
        palette = source.palette(mesh)
        calibrated_difference = source.calibrated_bindposes(resource, mesh)
        for binding in row["bindings"]:
            switch = switches.get(binding["variable"])
            check(switch and binding["values"] == list(range(switch["stateCount"])) and
                  set(binding["visible"]) <= set(binding["values"]), "visibility switch binding")
        for control in row["shapeControls"]:
            check(control["shape"] in row["shapeNames"] and control["identity"], "shape control identity")
            check(control["minimum"] <= control["default"] <= control["maximum"], "shape control default/range")
        # Snapshot ID is diagnostic only. Production needs a persisted author
        # part ID, so renaming objects does not change runtime control targets.
        part_id = "fixture:" + hashlib.sha256(row["object"].encode("utf-8")).hexdigest()[:20]
        parts[row["object"]] = dict(partId=part_id, authorFile=row["authorFile"], sourceIdentity=resource["identity"],
                                    material=row["materials"][0]["material"], geometry=geometry,
                                    originalPaletteSlots=slots, addedPaletteSlots=len(palette)-slots,
                                    originalBindposeMaxDifference=0.0,
                                    calibratedBindposeMaxDifference=calibrated_difference, palette=palette)
    rules = build_plan(capture, source, parts)
    targets = [target for rule in rules for target in rule["targets"]]
    uses = [use for target in targets for use in target["uses"]]
    report = dict(format="EFF_AUTHOR_PREFLIGHT", version=1, passed=True, nativePack=False, nativeLoadVerified=False,
                  capture=str(capture_path), sourceBaseline=str(source.package), vfsFingerprint=capture["vfsFingerprint"],
                  coverageScope="This source PFB dependency closure only; not a permanent consumer whitelist",
                  productionPartIdsImplemented=False,
                  summary=dict(selectedObjects=len(objects), hiddenObjects=sum(r["hidden"] for r in objects),
                               singleMaterialParts=len(parts), sourceGroups=len(rules), baseMaterials=len(materials),
                               materialDeltaFiles=sum(bool(m["authorOverrides"]) for m in materials.values()),
                               textures=len(textures), switches=len(switches),
                               shapeChannels=sum(len(p["geometry"]["shapes"]) for p in parts.values()),
                               outputVertices=sum(p["geometry"]["outputVertices"] for p in parts.values()),
                               outputIndices=sum(p["geometry"]["indices"] for p in parts.values()),
                               targetMeshResources=len(targets), sourceRendererUses=len(uses),
                               plannedRendererInstances=sum(len(u["generatedRenderers"]) for u in uses),
                               plannedRemovedUses=sum(not u["generatedRenderers"] for u in uses)),
                  materials=materials, textures=textures, parts=parts, switches=capture["switches"], rules=rules,
                  unresolved=["Game-native Mesh/Material/Texture serialization and registration",
                              "Serialized Renderer/LOD/control-reference expansion and native discovery of added Renderers",
                              "Effect Material inheritance per part and VFX capture of added Renderers",
                              "Native refresh/reinstantiation for F10, plus shape/visibility controller binding",
                              "Persisted author part IDs and Blender import/use of EFFSOURCE metadata"],
                  skinContract="Existing strongest normalized four influences; no new reduction introduced",
                  errors=[])
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    check(not args.output.exists(), "refusing to overwrite earlier evidence")
    try:
        report = audit(args.capture.resolve())
    except Exception as error:
        report = dict(format="EFF_AUTHOR_PREFLIGHT", version=1, passed=False, nativePack=False,
                      nativeLoadVerified=False, capture=str(args.capture), errors=[type(error).__name__ + ": " + str(error)])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({k: report[k] for k in ("passed", "nativePack", "nativeLoadVerified", "errors")} |
                     {"summary": report.get("summary"), "report": str(args.output)}, ensure_ascii=False))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
