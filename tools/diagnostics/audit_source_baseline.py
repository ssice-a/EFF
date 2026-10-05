"""Validate EFF source assembly coverage and compare fresh author files to an old export."""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "Blender"))
from eiem_format import read_mesh, read_skeleton


class AuditBytes:
    def __init__(self, data):
        self.data = data

    def read_bytes(self):
        return self.data


def check(value, message):
    if not value:
        raise ValueError(message)


def audit(package: Path, old: Path, references: Path):
    manifest = json.loads((package / "source/manifest.json").read_text(encoding="utf-8"))
    check(manifest["format"] == "EFFSOURCE" and manifest["version"] == 1, "source version")
    resources = {r["identity"]: r for r in manifest["resources"]}
    nodes = {n["identity"]: n for n in manifest["hierarchy"]}
    uses = {u["identity"]: u for u in manifest["rendererUses"]}
    check(len(resources) == len(manifest["resources"]), "duplicate author identities")
    check(len(uses) == len(manifest["rendererUses"]), "duplicate Renderer identities")
    table = {}
    for file in manifest["serializedFiles"]:
        for obj in file["objects"]:
            identity = f"{file['cab'].lower()}:{obj['pathId']}"
            check(identity not in table, "duplicate native identity")
            table[identity] = obj

    def native(identity, info):
        check(identity in table, f"native object absent: {identity}")
        obj = table[identity]
        check((info["offset"], info["size"], info["classId"]) ==
              (obj["offset"], obj["size"], obj["classId"]), f"native range mismatch: {identity}")

    for resource in resources.values():
        native(resource["identity"], resource["native"])
        check((package / resource["authorFile"]).is_file(), "author file missing")
    for node in nodes.values():
        native(node["identity"], node["native"])
        native(node["transform"], node["transformNative"])
    for use in uses.values():
        native(use["identity"], use["native"])
        check(use["owner"] in nodes, "Renderer owner outside hierarchy")
        check(use["identity"] in {p["identity"] for p in nodes[use["owner"]]["components"]}, "Renderer component missing")
        if use["mesh"] and not use["mesh"]["isNull"]:
            check(use["mesh"]["identity"] in resources, "Renderer Mesh not exported")
        for slot in use["materials"]:
            if not slot["pointer"]["isNull"]:
                check(slot["pointer"]["identity"] in resources, "Renderer Material not exported")
        for bone in use["bones"]:
            check(bone["resolved"] and bone["identity"] in table, "bone unresolved")

    refs = json.loads(references.read_text(encoding="utf-8-sig"))
    check(manifest["vfsFingerprint"] == refs["Fingerprint"], "reference VFS differs")
    expected = [r for r in refs["exactMeshUses"] if manifest["prefab"] in r["prefabs"]]
    for row in expected:
        check(row["component"] in uses, "previously proven Renderer missing")
        use = uses[row["component"]]
        check(use["mesh"]["identity"] == row["mesh"]["identity"], "Mesh association changed")
        check(use["owner"] == row["ownerIdentity"], "Renderer owner changed")
        check([s["pointer"]["identity"] for s in use["materials"]] ==
              [p["identity"] for p in row["details"]["materials"]], "Material association changed")
        check(len(use["bones"]) == row["details"]["bones"], "bone count changed")
    target_ids = {r["mesh"]["identity"] for r in expected}
    check(len([u for u in uses.values() if u["mesh"] and u["mesh"]["identity"] in target_ids]) == len(expected), "target coverage differs")

    comparisons = {}
    for directory, suffix, tags in (
        ("meshes", ".mesh", (b"EIEMESH\0", b"EFFMESH\0")),
        ("skeletons", ".skeleton", (b"EIESKEL\0", b"EFFSKEL\0")),
        ("textures", ".png", None),
    ):
        old_files = {p.name: p for p in (old / directory).glob("*" + suffix)}
        new_files = {p.name: p for p in (package / directory).glob("*" + suffix)}
        check(old_files.keys() == new_files.keys(), f"{directory} file set changed")
        differences = []
        for name, file in new_files.items():
            before, after = old_files[name].read_bytes(), file.read_bytes()
            if tags:
                check(before[:8] in tags and after[:8] == tags[1], "unexpected author magic")
                if before[8:] == after[8:]:
                    continue
                read = read_mesh if suffix == ".mesh" else read_skeleton
                old_data = read(AuditBytes(tags[1] + before[8:]))
                new_data = read(AuditBytes(after))
                check(old_data.keys() == new_data.keys(), "author layout changed")
                for key in old_data:
                    if key in ("path", "reader") or old_data[key] == new_data[key]:
                        continue
                    # Compressed tangent normalization can vary by the last float bits.
                    # All other arrays (including positions/indices/skin/bindposes) must be exact.
                    check(key == "tangents", f"author field changed: {name}/{key}")
                    check(len(old_data[key]) == len(new_data[key]), "tangent dimensions changed")
                    delta = max(abs(a - b) for a, b in zip(old_data[key], new_data[key]))
                    check(delta <= 2e-7, f"tangent difference exceeds roundoff: {name}/{delta}")
                    differences.append(dict(file=name, field=key, maxAbsDiff=delta))
            else:
                check(before == after, f"author payload changed: {name}")
        comparisons[directory] = dict(files=len(new_files), allFieldsExactExceptTangents=not differences or suffix == ".mesh",
            differingTangentFiles=len(differences), maxTangentDifference=max((d["maxAbsDiff"] for d in differences), default=0),
            legacyMagicChanged=tags is not None)

    sizes = []
    for row in manifest["serializedFiles"] + manifest["rawStreams"]:
        snapshot = row["snapshot"]
        path = package / snapshot["file"]
        check(path.stat().st_size == snapshot["size"], "snapshot size mismatch")
        with path.open("rb") as stream:
            check(hashlib.file_digest(stream, "sha256").hexdigest().upper() == snapshot["sha256"], "snapshot hash mismatch")
        sizes.append(snapshot["size"])
    check(all(e["inSnapshot"] for f in manifest["serializedFiles"] for e in f["externals"]), "external closure missing")
    return dict(format=1, verified=True, offlineOnly=True, package=str(package.resolve()),
        authorResourceCounts=dict(Counter(r["type"] for r in resources.values())),
        rendererUses=len(uses), distinctUsedMeshes=len({u["mesh"]["identity"] for u in uses.values() if u["mesh"]}),
        targetCoverage=dict(rendererUses=len(expected), distinctMeshes=len(target_ids), identicalToPreviousEvidence=True),
        authorComparison=comparisons, nodes=len(nodes), serializedFiles=len(manifest["serializedFiles"]),
        rawStreams=len(manifest["rawStreams"]), rawBytes=sum(sizes),
        allNativeRangesVerified=True, allExternalFilesIncluded=True, runtimeLoadingProven=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path)
    parser.add_argument("old", type=Path)
    parser.add_argument("references", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    check(not args.output.exists(), "refusing to overwrite evidence")
    report = audit(args.package, args.old, args.references)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
