"""Read-only audit of an existing author package, not a native pack builder.

The legacy magic adaptation exists only in this evidence tool's memory; the
original package is never rewritten. Full current-reader consumption determines
whether payloads actually share the current author layout.
"""
from __future__ import annotations

import argparse
import configparser
from collections import Counter, defaultdict
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "Blender"))
from eiem_format import read_mesh, read_skeleton


class AuditBytes:
    def __init__(self, data: bytes):
        self.data = data

    def read_bytes(self):
        return self.data


def inspect(package: Path, references: Path | None):
    files = sorted(p for p in package.rglob("*") if p.is_file())
    ini = configparser.ConfigParser(interpolation=None, strict=True)
    ini.read(package / "mod.ini", encoding="utf-8-sig")
    meshes, skeletons, failures = [], [], []
    for path in files:
        if path.suffix not in (".mesh", ".skeleton"):
            continue
        raw = path.read_bytes()
        magic = raw[:8]
        expected = b"EFFMESH\0" if path.suffix == ".mesh" else b"EFFSKEL\0"
        legacy = b"EIEMESH\0" if path.suffix == ".mesh" else b"EIESKEL\0"
        audit_raw = expected + raw[8:] if magic == legacy else raw
        try:
            if path.suffix == ".mesh":
                m = read_mesh(AuditBytes(audit_raw))
                count = m["vertex_count"]
                if len(m["vertices"]) != count * 3:
                    raise ValueError("position dimension/count mismatch")
                if any(index >= count for index in m["indices"]):
                    raise ValueError("index outside vertex range")
                if m["skin"] and len(m["skin"]) != count:
                    raise ValueError("skin vertex count mismatch")
                if any(bone >= len(m["bindposes"]) for weights, bones in m["skin"]
                       for weight, bone in zip(weights, bones) if weight > 0):
                    raise ValueError("weighted bone index outside palette")
                if any(start + index_count > len(m["indices"])
                       for _, start, index_count, *_ in m["submeshes"]):
                    raise ValueError("submesh index range outside payload")
                meshes.append(dict(file=path.relative_to(package).as_posix(),
                    magic=magic.decode("ascii").rstrip("\0"), version=m["version"],
                    legacyTagOnly=magic == legacy, fullyConsumed=True, source=m["source"],
                    name=m["name"], vertices=count, submeshes=len(m["submeshes"]),
                    boneSlots=len(m["bindposes"]), candidateSlots=len(m["bone_source_candidates"]),
                    shapeChannels=len(m["blend_channels"])))
            else:
                s = read_skeleton(AuditBytes(audit_raw))
                skeletons.append(dict(file=path.relative_to(package).as_posix(),
                    magic=magic.decode("ascii").rstrip("\0"), version=s["version"],
                    legacyTagOnly=magic == legacy, fullyConsumed=True, nodes=len(s["nodes"])))
        except Exception as ex:
            failures.append(dict(file=path.relative_to(package).as_posix(), error=str(ex)))

    materials = []
    for path in sorted((package / "materials").glob("*.mat")):
        props = dict(line.split("=", 1) for line in path.read_text(encoding="utf-8-sig").splitlines()
                     if "=" in line and not line.lstrip().startswith(";"))
        materials.append(dict(file=path.relative_to(package).as_posix(),
            source=props.get("source"), shaderName=props.get("shader"),
            nativeShaderPointerStored=any(k in props for k in ("shader.fileID", "shader.pathID", "shader.cab")),
            textureProperties=sum(k.startswith("texture.") for k in props)))

    native_manifest = package / "physics" / "components.json"
    physics = json.loads(native_manifest.read_text(encoding="utf-8-sig")) if native_manifest.is_file() else {}
    association = None
    if references:
        r = json.loads(references.read_text(encoding="utf-8-sig"))
        prefab = physics.get("prefab")
        if prefab is None:
            prefab = next((ini[s].get("path") for s in ini.sections() if s.startswith("Prefab")), None)
        rows = [row for row in r["exactMeshUses"] if prefab in row["prefabs"]]
        by_mesh = defaultdict(list)
        for row in rows:
            by_mesh[row["mesh"]["identity"]].append(row)
        association = dict(prefab=prefab, scopedToReferenceReportTargets=True,
            targetRendererReferences=len(rows), distinctTargetMeshes=len(by_mesh),
            repeatedUses=[dict(mesh=k, uses=len(v),
                owners=[row["owner"] for row in v],
                materialIdentities=[[p["identity"] for p in row["details"]["materials"]] for row in v])
                for k, v in by_mesh.items() if len(v) > 1])
    return dict(format=1, readOnly=True, nativePackingProven=False, package=str(package.resolve()),
        fileCounts=dict(Counter(p.relative_to(package).parts[0] for p in files)),
        meshes=meshes, skeletons=skeletons, materials=materials, failures=failures,
        sourceBinaryScope=dict(componentPurpose=physics.get("purpose"),
            rawComponentCount=len(physics.get("components", [])),
            fullNativePrefabStored=False, closureBundles=len(physics.get("closure", []))),
        referenceComparison=association,
        limits=["Only legacy magic is adapted in memory; no file migration is performed.",
            "Matching the current author parser is not native SerializedFile compatibility.",
            "Reference counts cover the diagnostic target set, not every Mesh in the package."])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--references", type=Path)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError("Refusing to overwrite evidence output")
    report = inspect(args.package, args.references)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(dict(meshes=len(report["meshes"]), skeletons=len(report["skeletons"]),
        failures=report["failures"], referenceComparison=report["referenceComparison"]), ensure_ascii=False))
    return bool(report["failures"])


if __name__ == "__main__":
    sys.exit(main())
