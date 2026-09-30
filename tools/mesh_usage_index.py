"""Index Mesh consumers from AnimeStudio postmodel structure exports.

AnimeStudio's Endfield postmodel export writes a human-readable
``*.prefab.structure.txt`` beside the EFF package.  The structure contains
the serialized Renderer -> Mesh reference and is therefore a better source for
static ownership than runtime object names.

This tool joins those references with Mesh ``target.path``/``target.asset``
metadata from an EFF ``mod.ini`` and writes a small JSON report.  It does not
read Unity objects, compute geometry fingerprints, or modify the mod.
"""

from __future__ import annotations

import argparse
import json
import re
from collections import defaultdict
from pathlib import Path


RENDERER_RE = re.compile(
    r"\[(?P<renderer>[^]]*Renderer)\]\s+"
    r"Mesh=(?P<mesh>.*?)\s+\[(?P<mesh_path>[^]]+)\]\s+"
    r"SubMeshes=(?P<submeshes>\d+)\s+Materials=\[(?P<materials>.*)\]"
)
GAME_OBJECT_RE = re.compile(
    r"^- (?P<name>.*?) \[GameObject\] Path=(?P<path>.*?) Root="
)


def logical_path(value: str) -> str:
    return value.replace("\\", "/").strip("/").casefold()


def display_path(value: str) -> str:
    return value.replace("\\", "/").strip("/")


def read_mesh_targets(mod_ini: Path) -> list[dict[str, str]]:
    targets: list[dict[str, str]] = []
    section = ""
    values: dict[str, str] = {}
    def finish() -> None:
        if not section.casefold().startswith("mesh"):
            return
        target_path = values.get("target.path", "").strip()
        target_asset = values.get("target.asset", "").strip()
        if target_path or target_asset:
            targets.append({
                "resource": section,
                "targetPath": display_path(target_path),
                "targetAsset": target_asset,
            })

    for raw in mod_ini.read_text(encoding="utf-8-sig", errors="replace").splitlines():
        line = raw.strip()
        if not line or line.startswith(";") or line.startswith("#"):
            continue
        if line.startswith("[") and line.endswith("]"):
            finish()
            section = line[1:-1].strip()
            values = {}
            continue
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        key = key.strip().casefold()
        if key in {"target.path", "target.asset"}:
            values[key] = value.strip()
    finish()
    return targets


def read_structure(path: Path, workspace: Path) -> list[dict[str, object]]:
    current: dict[str, str] | None = None
    result: list[dict[str, object]] = []
    for line_number, line in enumerate(
        path.read_text(encoding="utf-8", errors="replace").splitlines(), 1
    ):
        game_object = GAME_OBJECT_RE.match(line)
        if game_object:
            current = {
                "name": game_object.group("name"),
                "path": game_object.group("path"),
            }
        renderer = RENDERER_RE.search(line)
        if not renderer:
            continue
        result.append(
            {
                "prefab": display_path(str(path.relative_to(workspace))),
                "line": line_number,
                "renderer": renderer.group("renderer"),
                "mesh": renderer.group("mesh").strip(),
                "meshPath": display_path(renderer.group("mesh_path")),
                "subMeshes": int(renderer.group("submeshes")),
                "materials": renderer.group("materials").strip(),
                "gameObject": current["name"] if current else "",
                "gameObjectPath": current["path"] if current else "",
            }
        )
    return result


def build_index(workspace: Path, mod_ini: Path) -> dict[str, object]:
    targets = read_mesh_targets(mod_ini)
    uses = [
        item
        for structure in sorted(workspace.rglob("*.prefab.structure.txt"))
        for item in read_structure(structure, workspace)
    ]

    by_path: defaultdict[str, list[dict[str, object]]] = defaultdict(list)
    by_asset: defaultdict[str, list[dict[str, object]]] = defaultdict(list)
    for item in uses:
        by_path[logical_path(str(item["meshPath"]))].append(item)
        by_asset[str(item["mesh"]).casefold()].append(item)

    joined: list[dict[str, object]] = []
    for target in targets:
        path_key = logical_path(target["targetPath"])
        asset_key = target["targetAsset"].casefold()
        direct = list(by_path.get(path_key, [])) if path_key else []
        # Asset-name matches are reported separately. A same-named FBX subasset
        # is not treated as the same resource when a target path is available.
        same_name = list(by_asset.get(asset_key, [])) if asset_key else []
        joined.append(
            {
                **target,
                "directUses": direct,
                "sameNameUses": same_name,
                "sameNameDifferentPath": [
                    item for item in same_name if logical_path(str(item["meshPath"])) != path_key
                ],
            }
        )

    return {
        "format": 1,
        "workspace": str(workspace),
        "modIni": str(mod_ini),
        "structureFiles": len(list(workspace.rglob("*.prefab.structure.txt"))),
        "rendererReferences": len(uses),
        "targets": joined,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("workspace", type=Path)
    parser.add_argument("mod_ini", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    report = build_index(args.workspace.resolve(), args.mod_ini.resolve())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(
        f"structureFiles={report['structureFiles']} "
        f"rendererReferences={report['rendererReferences']} "
        f"targets={len(report['targets'])} output={args.output}"
    )
    for target in report["targets"]:
        print(
            f"{target['resource']}: direct={len(target['directUses'])} "
            f"sameName={len(target['sameNameUses'])} "
            f"differentPath={len(target['sameNameDifferentPath'])}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
