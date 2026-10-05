"""Summarize decoded PFB/VFX configuration without claiming draw ownership."""
from __future__ import annotations

import argparse
import json
from pathlib import Path


def read(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def summarize(root: Path):
    prefabs, assets, failures = [], [], []
    for path in sorted(root.rglob("components.json")):
        report = read(path)
        resource = report["prefab"]
        selected = [c for c in report["components"] if c["inSelectedResourceFile"]]
        row = dict(resource=resource, source=report["source"], evidence=str(path.resolve()),
                   components=[], parsedRenderers=report["renderers"], meshFilters=report["meshFilters"])
        for component in selected:
            if not component["decoded"] or component["consumed"] != component["byteSize"]:
                failures.append(dict(resource=resource, identity=component["identity"], error=component["error"]))
                continue
            data = read(path.parent / (component["id"] + ".data.json"))
            fields = data.get("data", {})
            if not isinstance(fields, dict):
                fields = {}
            scalars = {k: v for k, v in fields.items() if not isinstance(v, dict)}
            # Keep timelines compact, retaining their actual serialized values.
            curves = {k: [{key: point[key] for key in ("time", "value")} for point in v["m_Curve"]]
                      for k, v in fields.items() if isinstance(v, dict) and "m_Curve" in v}
            row["components"].append(dict(
                identity=component["identity"], script=component["script"],
                ownerPath=component["ownerPath"], decodedBytes=component["consumed"],
                vfxNames=data.get("effectEntityRenderVfxCfg", {}).get("vfxNames", []),
                fields=scalars, curves=curves,
                references=[r for r in component["references"] if r["field"].startswith("$.data.")]))
        if resource.endswith(".prefab"):
            prefabs.append(row)
        else:
            assets.append(row)
    return dict(
        format=1, offlineEvidenceOnly=True, replacementImplemented=False,
        prefabCount=len(prefabs), vfxAssetCount=len(assets), prefabs=prefabs, vfxAssets=assets,
        failures=failures,
        limits=[
            "Resource request/instance completion is not proof of GPU drawing.",
            "Renderer masks are recorded; category bit meanings and native eligibility remain unverified here.",
            "The common capture template has null serialized Mesh/Material references; final runtime assignments are not established by this report.",
            "Renderer summaries omit ParticleSystemRenderer and must not establish absence of particle effects.",
            "Multi-submesh capture/material coverage with replacement native payloads remains unverified.",
        ])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    result = summarize(args.root)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"PFBs={result['prefabCount']} VFX assets={result['vfxAssetCount']} decode failures={len(result['failures'])}")
    if result["failures"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
