"""Read-only lifecycle evidence; no runtime RVA invocation or private writes."""
import argparse
import hashlib
import json
import mmap
import struct
from pathlib import Path
from inspect_il2cpp_type import inspect
from inspect_native_shapes import Image
from map_il2cpp_method_owners import lookup

def control_flow(image, raw, entry, entries):
    todo, seen, result, calls = [entry], set(), [], set()
    while todo:
        address = todo.pop()
        if address in seen:
            continue
        if len(seen) > 8192:
            raise ValueError("Lifecycle CFG instruction limit")
        while address not in seen:
            seen.add(address)
            offset = image.offset(address)
            instruction = next(image.dis.disasm(raw[offset:offset + 15], address), None)
            if not instruction:
                raise ValueError("Undecodable lifecycle instruction")
            result.append({"rva": hex(address), "asm": instruction.mnemonic + " " + instruction.op_str})
            target = None
            if instruction.op_str.startswith("0x") and "," not in instruction.op_str:
                target = int(instruction.op_str, 16)
            if instruction.mnemonic == "call" and target:
                calls.add(target)
            if instruction.mnemonic in ("ret", "int3", "ud2"):
                break
            if instruction.mnemonic == "jmp":
                if target and target not in entries:
                    todo.append(target)
                elif target:
                    calls.add(target)
                break
            if instruction.mnemonic.startswith("j") and target:
                todo.append(target)
            address += instruction.size
    return sorted(result, key=lambda i: int(i["rva"], 16)), calls

SELECTED = {
    ("Beyond.Gameplay", "GameInstance"): {"get_instance"},
    ("Beyond.Gameplay", "EffectManager"): {"ChangeEntityFollowEffect"},
    ("Beyond.Gameplay.View", "BaseModelComponent"): {"LoadMainModelSync", "RemoveMainPart", "_LoadMainModelInternal", "OnMainPartLoadFinish", "FinishLoaded", "CreateMainPart", "GetModelGo", "get_modelPath", "get_entityRenderHelper"},
    ("Beyond.Gameplay.View", "ModelComponent"): {"ChangeMainModelSync"},
    ("Beyond.Gameplay.View", "BaseModelViewPart"): {"_LoadSync", "ReleaseModel", "StartSyncLoad", "SetupData", "_DoLoad"},
    ("Beyond.Gameplay.View", "BaseModelViewPartData"): set(),
    ("Beyond.Gameplay.View", "ComplexModelViewPart"): {"ReleaseModel", "PostDealLoadedModel"},
    ("Beyond.Gameplay.View", "ModelManager"): {"DiscardPooledObjects", "_TryDequeuePersistentIdle", "UnloadToPersistentPool", "LoadFromPersistentPool", "_RefillPersistentGroup", "_TrimPersistentGroup", "_OnGameObjectCreate", "Alloc", "Load", "Unload", "_DestroyPersistentModel"},
}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("metadata", type=Path)
    parser.add_argument("image", type=Path)
    parser.add_argument("modules", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    modules = {m["assembly"]: int(m["rva"], 16) for m in json.loads(args.modules.read_text("utf8"))["modules"]}
    result = {"format": "EFF_RESOURCE_RELOAD_LIFECYCLE_STATIC", "version": 1, "runtimeExecuted": False, "types": []}
    with args.image.open("rb") as f:
        result["assemblySha256"] = hashlib.file_digest(f, "sha256").hexdigest().upper()
    with args.metadata.open("rb") as f, mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ) as meta, args.image.open("rb") as g, mmap.mmap(g.fileno(), 0, access=mmap.ACCESS_READ) as raw:
        image = Image(args.image, raw)
        entries = set()
        for module in modules.values():
            _, count, pointers = struct.unpack_from("<QQQ", raw, image.offset(module))
            for pointer in struct.unpack_from("<" + "Q" * count, raw, image.offset(pointers - image.base)):
                if pointer:
                    entries.add(pointer - image.base)
        owners = []
        for (ns, name), selected in SELECTED.items():
            base = inspect(meta, None, None, name, ns, 92)
            if len(base) != 1:
                raise ValueError(f"Expected one metadata owner: {ns}.{name}")
            owner = inspect(meta, image, modules[base[0]["assembly"]], name, ns, 92)[0]
            entries.update(int(m["rva"], 16) for m in owner["methods"] if m["rva"])
            owner["methods"] = [m for m in owner["methods"] if m["name"] in selected]
            owners.append(owner)
        targets = set()
        for owner in owners:
            for method in owner["methods"]:
                if not method["rva"]:
                    continue
                rva = int(method["rva"], 16)
                boundary = image.containing(rva)
                # Method registration identifies the entry. Unwind fragments
                # alone do not establish whole control flow across cold tails.
                length = min(8192, boundary[1] - rva) if boundary else 4096
                method["entryInstructions"] = image.instructions(rva, length)
                method["unwindBoundary"] = [hex(x) for x in boundary] if boundary else None
                method["wholeControlFlowVerified"] = False
                cfg, calls = control_flow(image, raw, rva, entries)
                method["cfgInstructions"] = cfg
                method["directCallsAndTailCalls"] = [hex(c) for c in sorted(calls)]
                targets.update(calls)
            result["types"].append(owner)
        result["callOwners"] = lookup(meta, image, targets, 92, list(modules))
    with args.output.open("x", encoding="utf8") as f:
        json.dump(result, f, ensure_ascii=False, indent=2)
        f.write("\n")
    for t in result["types"]:
        print(t["namespace"] + "." + t["name"], [(m["name"], m["rva"]) for m in t["methods"]])

if __name__ == "__main__":
    main()
