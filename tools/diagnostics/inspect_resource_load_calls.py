"""Read-only disassembly of the VFS load methods observed in entry logs."""
import argparse
import json
from pathlib import Path

import capstone
import pefile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dll", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    pe = pefile.PE(str(args.dll), fast_load=True)
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXCEPTION"]])
    image = pe.get_memory_mapped_image()
    base = pe.OPTIONAL_HEADER.ImageBase
    disassembler = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    disassembler.detail = True
    methods = {
        "VFS.sync": 0xEF2320,
        "VFS.syncPos": 0xEF1140,
        "VFS.async": 0xEEFC40,
        "VFS.asyncPos": 0xEEFFB0,
        "VFS.syncCallee": 0x2D7B090,
        "VFS.asyncCallee": 0x2D7C870,
        "VFS.syncLeaf": 0x2D7B710,
        "VFS.asyncLeaf": 0x2D7CEF0,
        "VFS.syncMode1": 0x5BBAE9C,
        "VFS.asyncMode1": 0x5BBAC38,
    }
    result = {"dll": str(args.dll), "methods": {}}
    for name, rva in methods.items():
        end = next((entry.struct.EndAddress for entry in pe.DIRECTORY_ENTRY_EXCEPTION
                    if entry.struct.BeginAddress == rva), rva + 0x1200)
        lines, calls, strings = [], [], []
        for ins in disassembler.disasm(image[rva:end], base + rva):
            offset = ins.address - base
            lines.append(f"{offset:08X} {ins.mnemonic} {ins.op_str}")
            if ins.mnemonic == "call":
                calls.append({"rva": hex(offset), "operand": ins.op_str})
            for operand in ins.operands:
                if operand.type == capstone.CS_OP_MEM and operand.mem.base == capstone.x86.X86_REG_RIP:
                    target = offset + ins.size + operand.mem.disp
                    value = image[target:target + 200].split(b"\0", 1)[0]
                    if len(value) > 8 and all(32 <= byte < 127 for byte in value):
                        strings.append(value.decode("ascii"))
        result["methods"][name] = {"rva": hex(rva), "calls": calls,
                                   "strings": strings, "instructions": lines}
        print(name, json.dumps({"callCount": len(calls), "strings": strings,
                               "tailCalls": calls[-7:]}))
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
