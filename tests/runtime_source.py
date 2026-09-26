"""Read the runtime translation unit with its local implementation modules.

The contract tests inspect the same declaration order that the C++ compiler
sees. Keep this deliberately narrow instead of recursively expanding every
production header and accidentally testing duplicate declarations.
"""

from pathlib import Path


def read_runtime_source(root: Path) -> str:
    src = root / "src"
    trace = (src / "il2cpp_trace.h").read_text(encoding="utf-8")
    for name in ("eiem_render_override.h", "eiem_render_executor.h", "eiem_mod_reconcile.h",
                 "eiem_model_registry.h", "eiem_world_ui_owner.h",
                 "eiem_skin_resolver.h", "eiem_assembly_binding.h"):
        marker = f'#include "{name}"'
        assert trace.count(marker) == 1, f"expected one {marker}"
        trace = trace.replace(marker, (src / name).read_text(encoding="utf-8"))
    return trace
