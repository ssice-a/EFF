"""Verify the actual release binary excludes temporary probes and keeps lifecycle bindings."""
from pathlib import Path
import sys

path = Path(sys.argv[1])
data = path.read_bytes()
for marker in (
    b"[DEBUG-", b"[ENTRY-CAPTURE]", b"[ENTRY-SNAPSHOT]", b"[ENTRY-SNAPSHOT-SELECTION]",
    b"[ENTRY-COUNTERS]", b"[CAPTURE-ELIGIBILITY]", b"[RELOAD-LOAD-SUBMITTED]",
    b"resource-input-diagnostics", b"snapshot-ranges.tsv",
):
    assert marker not in data, f"Production DLL contains diagnostic marker: {marker!r}"
for marker in (
    b"[FULL-RELOAD-COMMIT]", b"[FULL-RELOAD-REJECT]", b"eff_log.txt",
    b"ForceExitAllPerformOnEntity", b"_PreloadSpIdlePerform", b"_PreloadSpDashPerform",
    b"ChangeAnimationConfig", b"UpdateAllWeaponMountPoint", b"ReloadWeapon",
    b"[SHAPE-CAPTURE-COMPLETION]",
):
    assert marker in data, f"Production DLL is missing functional binding: {marker!r}"
print(f"PASS production DLL: {path.name}, {len(data):,} bytes; probes excluded, lifecycle bindings retained")
