"""Read-only full-reload entry evidence from this player's metadata/PE.

Module RVAs come from a previous validated owner report and are rechecked
against this PE. No runtime call/hook is performed. Direct calls and names
identify investigation candidates; they do not prove a safe refresh contract.
"""
import argparse
import hashlib
import json
import mmap
from pathlib import Path
from inspect_il2cpp_type import inspect
from inspect_native_shapes import Image

CANDIDATES = [
    ("Beyond.Gameplay.View", "EntityRenderHelper", {"_InitRenderAndMaterial", "ReFindRenderersInChildren", "OnRelease", "InitAll"}),
    ("Beyond.Gameplay.View", "BaseModelComponent", {"SetModelId", "LoadMainModelSync", "RemoveMainPart"}),
    ("Beyond.Gameplay.View", "ModelComponent", {"ChangeMainModelSync"}),
    ("Beyond.Rendering", "EntityRenderHelperMaterialController", {"Init", "OnRelease"}),
    ("Beyond.Rendering", "EntityRenderCaptureMeshController", {"Init", "OnRelease", "Release", "CaptureMesh"}),
    ("Beyond.Resource.Runtime", "BundleResourceManager", {"_LoadAssetInternal", "_LoadSubAssetInternal", "UnloadUnusedAssets", "UnloadAllAssets"}),
    ("Beyond.Resource", "ResourceManager", {"Load", "LoadSubAsset", "UnloadUnusedAssets"}),
    ("Beyond.Resource", "FAssetProxyHandle", {"get_asset", "Release", "Dispose"}),
]

def sha256(path):
    with path.open("rb") as file:
        return hashlib.file_digest(file, "sha256").hexdigest().upper()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("metadata", type=Path)
    parser.add_argument("image", type=Path)
    parser.add_argument("module_evidence", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    modules = {m["assembly"]: int(m["rva"], 16) for m in json.loads(args.module_evidence.read_text(encoding="utf-8"))["modules"]}
    output = {"format": "EFF_FULL_RELOAD_STATIC_EVIDENCE", "version": 1,
              "metadata": str(args.metadata), "metadataSha256": sha256(args.metadata),
              "image": str(args.image), "imageSha256": sha256(args.image),
              "runtimeInvocation": False, "safeRefreshEntryVerified": False, "types": []}
    with args.metadata.open("rb") as meta_file, mmap.mmap(meta_file.fileno(), 0, access=mmap.ACCESS_READ) as metadata, args.image.open("rb") as pe_file, mmap.mmap(pe_file.fileno(), 0, access=mmap.ACCESS_READ) as raw:
        image = Image(args.image, raw)
        for namespace, name, selected in CANDIDATES:
            owners = inspect(metadata, None, None, name, namespace, 92)
            if len(owners) != 1:
                raise ValueError(f"Expected one owner for {namespace}.{name}")
            module = modules[owners[0]["assembly"]]
            owner = inspect(metadata, image, module, name, namespace, 92)[0]
            for method in owner["methods"]:
                if method["name"] not in selected or not method["rva"]:
                    continue
                rva = int(method["rva"], 16)
                function = image.containing(rva)
                if not function or function[0] != rva:
                    method["disassembly"] = {"available": False, "reason": "no exact PE function boundary"}
                    continue
                lo, hi = function
                calls, lines = [], []
                offset = image.offset(lo)
                for instruction in image.dis.disasm(raw[offset:offset + hi - lo], image.base + lo):
                    relative = instruction.address - image.base
                    lines.append(f"{relative:08X} {instruction.mnemonic} {instruction.op_str}")
                    if instruction.mnemonic == "call":
                        calls.append({"rva": hex(relative), "operand": instruction.op_str})
                method["disassembly"] = {"available": True, "range": [hex(lo), hex(hi)], "calls": calls, "instructions": lines, "completeCallGraph": False}
            output["types"].append(owner)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x", encoding="utf-8") as file:
        json.dump(output, file, ensure_ascii=False, indent=2)
        file.write("\n")
    for owner in output["types"]:
        selected = next(c[2] for c in CANDIDATES if c[0] == owner["namespace"] and c[1] == owner["name"])
        print(owner["namespace"] + "." + owner["name"], [(m["name"], m["rva"]) for m in owner["methods"] if m["name"] in selected])

if __name__ == "__main__":
    main()
