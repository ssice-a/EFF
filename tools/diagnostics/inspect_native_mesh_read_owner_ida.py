"""Read-only IDA export for the serialized Mesh transaction's owner/key."""
import pathlib

import ida_auto
import ida_funcs
import ida_hexrays
import ida_pro
import ida_lines
import idautils
import idc

output = pathlib.Path(__file__).resolve().parents[2] / "diagnostics" / "mesh-resource-read-owner-20261002" / "UnityPlayer.reader-owner.ida.txt"
output.parent.mkdir(parents=True, exist_ok=True)
ida_auto.auto_wait()
ida_hexrays.init_hexrays_plugin()
base = 0x180000000
targets = [0x133800, 0x347BC0, 0x133E80, 0x1340F0, 0x132D40, 0x1326C0, 0x133720, 0x133710, 0x127B080]
with output.open("w", encoding="utf-8") as report:
    for rva in targets:
        fn = ida_funcs.get_func(base + rva)
        if not fn:
            report.write(f"MISSING +{rva:X}\n")
            continue
        report.write(f"\nTARGET +{rva:X} FUNCTION {idc.get_func_name(fn.start_ea)} SIZE {fn.end_ea-fn.start_ea:X}\n")
        report.write("CALLERS\n")
        for ref in idautils.CodeRefsTo(fn.start_ea, False):
            report.write(f"  +{ref-base:X} {ida_lines.tag_remove(idc.generate_disasm_line(ref, 0) or '')}\n")
        report.write("PSEUDOCODE\n")
        try:
            report.write(str(ida_hexrays.decompile(fn.start_ea)) + "\n")
        except Exception as exc:
            report.write(f"DECOMPILE FAILED {exc}\n")
        report.write("ASSEMBLY\n")
        for ea in idautils.FuncItems(fn.start_ea):
            report.write(f"  +{ea-base:X} {ida_lines.tag_remove(idc.generate_disasm_line(ea, 0) or '')}\n")
ida_pro.qexit(0)
