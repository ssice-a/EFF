"""Export observed Mesh provider vtable and its constructor/file access chain."""
import pathlib

import ida_auto
import ida_bytes
import ida_funcs
import ida_hexrays
import ida_lines
import ida_pro
import idautils
import idc

output = pathlib.Path(__file__).resolve().parents[2] / "diagnostics" / "mesh-resource-read-owner-20261002" / "UnityPlayer.mesh-provider.ida.txt"
ida_auto.auto_wait()
ida_hexrays.init_hexrays_plugin()
base = 0x180000000
vtable = base + 0x18678D0
targets = [base + rva for rva in (0x1BF4A0, 0x0AF980, 0x0AEBB0, 0x072C20,
                                 0x0B3E70, 0xD73BC0)]
with output.open("w", encoding="utf-8") as report:
    report.write("OBSERVED PROVIDER VTABLE +18678D0\n")
    for slot in range(8):
        ea = ida_bytes.get_qword(vtable + slot * 8)
        report.write(f"  SLOT {slot} +{ea-base:X} {idc.get_name(ea)}\n")
        if slot != 3:
            targets.append(ea)
    report.write("VTABLE REFERENCES\n")
    for ref in idautils.XrefsTo(vtable):
        fn = ida_funcs.get_func(ref.frm)
        report.write(f"  +{ref.frm-base:X} {ida_lines.tag_remove(idc.generate_disasm_line(ref.frm, 0) or '')}\n")
        if fn:
            targets.append(fn.start_ea)
    seen = set()
    for ea in targets:
        fn = ida_funcs.get_func(ea)
        if not fn or fn.start_ea in seen:
            continue
        seen.add(fn.start_ea)
        report.write(f"\nTARGET +{fn.start_ea-base:X} FUNCTION {idc.get_func_name(fn.start_ea)} SIZE {fn.end_ea-fn.start_ea:X}\nCALLERS\n")
        for ref in idautils.XrefsTo(fn.start_ea):
            caller = ida_funcs.get_func(ref.frm)
            report.write(f"  +{ref.frm-base:X} caller={idc.get_func_name(caller.start_ea) if caller else 'data'} {ida_lines.tag_remove(idc.generate_disasm_line(ref.frm, 0) or '')}\n")
        report.write("PSEUDOCODE\n")
        try:
            report.write(str(ida_hexrays.decompile(fn.start_ea)) + "\n")
        except Exception as exc:
            report.write(f"DECOMPILE FAILED {exc}\n")
        report.write("ASSEMBLY\n")
        for insn in idautils.FuncItems(fn.start_ea):
            report.write(f"  +{insn-base:X} {ida_lines.tag_remove(idc.generate_disasm_line(insn, 0) or '')}\n")
ida_pro.qexit(0)
