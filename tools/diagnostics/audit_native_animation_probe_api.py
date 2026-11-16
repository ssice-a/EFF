"""Audit the probe's real managed API candidates and native fallback evidence.

Reads the local game's metadata/PE files, never loads a DLL into the game.
Exact IL2CPP type signatures and registered icall ABIs remain runtime checks.
"""
import argparse
import json
import mmap
import re
from pathlib import Path

from inspect_il2cpp_type import inspect
from inspect_native_shapes import Image
from map_il2cpp_method_owners import lookup


def audit(game, source, strict_managed=False):
    text = source.read_text(encoding="utf-8-sig")
    owners = dict(re.findall(r'(\w+(?:\.\w+)?)=klass\("([^"]+)"\)', text))
    declarations = re.findall(
        r'(method|optional)\((klass\("[^"]+"\)|\w+(?:\.\w+)?),"([^"]+)",'
        r'\{([^}]*)\},"([^"]+)"(?:,(true|false))?\)', text)
    if len(declarations) < 40:
        raise ValueError("Probe declarations were not recognized")
    metadata_path = game / "Endfield_Data/il2cpp_data/Metadata/global-metadata.dat"
    fallback_names = {
        "get_isEmpty": "UnityEngine.MaterialPropertyBlock::get_isEmpty",
        "HasProperty": "UnityEngine.MaterialPropertyBlock::HasPropertyImpl",
        "GetColor": "UnityEngine.MaterialPropertyBlock::GetColorImpl_Injected",
    }
    with metadata_path.open("rb") as f, mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ) as metadata, \
            (game / "GameAssembly.dll").open("rb") as g, mmap.mmap(g.fileno(), 0, access=mmap.ACCESS_READ) as raw, \
            (game / "unityplayer.dll").open("rb") as u, mmap.mmap(u.fileno(), 0, access=mmap.ACCESS_READ) as unity:
        image = Image(game / "GameAssembly.dll", raw)
        types = {}
        for binding, owner, name, parameters, result, static in declarations:
            class_name = re.fullmatch(r'klass\("([^"]+)"\)', owner)
            class_name = class_name.group(1) if class_name else owners[owner]
            if class_name not in types:
                rows = inspect(metadata, None, None, class_name, "UnityEngine", 92)
                if len(rows) != 1:
                    raise ValueError(f"Ambiguous/missing UnityEngine.{class_name}")
                types[class_name] = rows[0]
        modules = lookup(metadata, image, set(), 92, sorted({x["assembly"] for x in types.values()}))["modules"]
        module_rvas = {m["assembly"]: int(m["rva"], 16) for m in modules}
        compiled = {name: inspect(metadata, image, module_rvas[row["assembly"]], name, "UnityEngine", 92)[0]
                    for name, row in types.items()}
        rows, missing = [], []
        for binding, owner, name, parameters, result, static in declarations:
            class_name = re.fullmatch(r'klass\("([^"]+)"\)', owner)
            class_name = class_name.group(1) if class_name else owners[owner]
            parameters = re.findall(r'"([^"]*)"', parameters)
            candidates = [m for m in compiled[class_name]["methods"] if m["name"] == name and
                          m["parameter_count"] == len(parameters) and
                          bool(int(m["flags"], 16) & 16) == (static == "true") and m["rva"]]
            fallback = None
            if not candidates and binding == "optional" and class_name == "MaterialPropertyBlock":
                expected = fallback_names.get(name)
                if expected and expected in text and unity.find(expected.encode("ascii") + b"\0") >= 0:
                    fallback = expected
            passed = bool(candidates or fallback and not strict_managed)
            row = dict(owner=class_name, method=name, requestedParameters=parameters, requestedReturn=result,
                       candidates=candidates, nativeFallbackStringEvidence=fallback, passed=passed)
            rows.append(row)
            if not passed:
                missing.append(f"{class_name}.{name}/{len(parameters)}")
    return dict(passed=not missing, strictManaged=strict_managed, declarations=len(rows), missing=missing,
                exactRuntimeTypeSignaturesVerified=False, registeredIcallAbiVerified=False,
                modules=modules, methods=rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("game", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--source", type=Path, default=Path(__file__).with_name("native_animation_probe.h"))
    parser.add_argument("--strict-managed", action="store_true", help="Reproduce the missing-wrapper failure without fallbacks")
    args = parser.parse_args()
    report = audit(args.game, args.source, args.strict_managed)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf8")
    print(f"{'PASS' if report['passed'] else 'FAIL'} native animation API candidates: {report['declarations']}; missing={report['missing']}")
    raise SystemExit(0 if report["passed"] else 1)


if __name__ == "__main__":
    main()
