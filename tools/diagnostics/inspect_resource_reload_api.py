"""Read-only compiled API presence audit; exact type names/icalls remain runtime-gated."""
import argparse
import json
import mmap
from pathlib import Path
import re
from inspect_il2cpp_type import inspect
from inspect_native_shapes import Image
from map_il2cpp_method_owners import lookup


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("game", type=Path)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    source = args.source.read_text("utf8")
    classes = {m[0]: (m[1], m[2]) for m in re.findall(r'(\w+)=FindClass\("([^"]*)","([^"]*)"', source)}
    wanted = re.findall(r'(Exact|Method)\((\w+),"([^"]*)",\{([^}]*)\},"([^"]*)",(true|false)\)', source)
    metadata = args.game / "Endfield_Data/il2cpp_data/Metadata/global-metadata.dat"
    with metadata.open("rb") as f, mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ) as meta, (args.game / "GameAssembly.dll").open("rb") as g, mmap.mmap(g.fileno(), 0, access=mmap.ACCESS_READ) as raw:
        image = Image(args.game / "GameAssembly.dll", raw)
        owners = {k: inspect(meta, None, None, v[1], v[0], 92)[0] for k, v in classes.items()}
        modules = lookup(meta, image, set(), 92, list({v["assembly"] for v in owners.values()}))["modules"]
        module_rvas = {m["assembly"]: int(m["rva"], 16) for m in modules}
        compiled = {k: inspect(meta, image, module_rvas[v["assembly"]], v["name"], v["namespace"], 92)[0] for k, v in owners.items()}
        rows, optional_missing = [], []
        for binding, owner, name, parameters, result, static in wanted:
            # Utility functions accept a caller-provided class; only audit
            # concrete FindClass bindings discovered in this source.
            if owner not in compiled:
                continue
            parameters = re.findall(r'"([^"]*)"', parameters)
            found = [m for m in compiled[owner]["methods"] if m["name"] == name and
                     m["parameter_count"] == len(parameters) and bool(int(m["flags"], 16) & 16) == (static == "true")]
            if not found or not any(m["rva"] for m in found):
                if binding == "Exact":
                    optional_missing.append(dict(owner=classes[owner], method=name, requestedParameters=parameters))
                    continue
                raise ValueError(f"Missing compiled candidate {classes[owner]} {name}/{len(parameters)}")
            rows.append(dict(owner=classes[owner], method=name, requestedReturn=result, requestedParameters=parameters, candidates=found))
    report = dict(format="EFF_FULL_RELOAD_API_STATIC_PRESENCE", version=1, compiledCandidatesPresent=True,
                  exactRuntimeTypeSignaturesVerified=False, registeredIcallBindingsVerified=False, methods=rows, optionalMissing=optional_missing, modules=modules)
    with args.output.open("x", encoding="utf8") as file:
        json.dump(report, file, indent=2)
        file.write("\n")
    print(f"PASS compiled API candidates={len(rows)} optionalMissing={len(optional_missing)}; exact managed type/icall signatures remain runtime-gated")


if __name__ == "__main__":
    main()
