"""Reject a diagnostic DLL whose observation boundary was optimized away."""
from pathlib import Path
import hashlib
import sys

path = Path(sys.argv[1])
data = path.read_bytes()
format3_runtime = '--format3-runtime' in sys.argv[2:]
acceptance = '--native-acceptance' in sys.argv[2:] or format3_runtime
for marker in (b'[DEMAND-NATIVE-READY]', b'[DEMAND-NATIVE-PREPARED]', b'[DEMAND-NATIVE-SUBMIT]', b'[DEMAND-NATIVE-HELPER-SUBMIT]', b'[DEMAND-NATIVE-READBACK]'):
    assert (marker in data) == acceptance, f'Incorrect replacement/observation build: {marker!r}'
assert b'[DEMAND-NATIVE-FALLBACK]' in data, 'Missing safe fallback diagnostics'
startup_effect_gate = b'!defined(EIEM_MESH_TARGET_PROBE) && (!defined(EIEM_DEMAND_LOADING_PROBE) || defined(EIEM_DEMAND_NATIVE_ACCEPTANCE))'
assert startup_effect_gate in Path('src/eiem_resource_entry.cpp').read_bytes(), 'Incorrect demand-build effect attachment startup gate'
runtime_armed = b'demandReplacementArmed=DemandLoadingProbe::enabled.load()&&DemandLoadingProbe::sharedInputsEnabled.load()'
assert runtime_armed in Path('src/eiem_resource_entry.cpp').read_bytes(), 'Demand replacement readiness is absent from startup status'
probe = Path('src/eiem_entry_demand_loading_probe.h').read_bytes()
prepare = probe[probe.index(b'void PrepareReplacement('):probe.index(b'bool SubmitPrepared(')]
empty_rewrite = prepare.index(b'if(rewrites.empty())')
empty_state_commit = prepare.find(b'commitScope();', empty_rewrite)
empty_publish = prepare.find(b'publish({});', empty_rewrite)
assert 0 <= empty_state_commit < empty_publish, 'No-op target input must commit scope identities before publishing passthrough'
for marker in (
    b"mode=format3-runtime" if format3_runtime else b"mode=demand-native-acceptance" if acceptance else b"mode=demand-loading-observation",
    b"[DEMAND-PROBE-READY]",
    b"[DEMAND-TIMING-BUDGET]",
    b"[DEMAND-TIMING-STATS]",
    b"[DEMAND-TIMING-GAP]",
    b"[DEMAND-TIMING]",
    b"[DEMAND-PROBE-DECLARATIONS]",
    b"[DEMAND-PROBE-VFS-IDENTITY]",
    b"[DEMAND-PROBE-VFS-BEGIN]",
    b"[DEMAND-PROBE-INPUT]",
    b"[DEMAND-PROBE-ASSET-COMPLETE]",
):
    assert marker in data, f"Missing request boundary: {marker!r}"
assert (b"[DEMAND-PROBE-MESH-IDENTITY]" in data) == acceptance, \
    "Mesh identity boundary must be present only in native acceptance builds"
for marker in (b"[MESH-NATIVE-", b"[MESH-TARGET-", b"[ENTRY-STARTUP-COST]"):
    assert marker not in data, f"Unrelated eager/experimental backend retained: {marker!r}"
print(f"PASS creation-before {'native acceptance' if acceptance else 'observation'} DLL: {len(data):,} bytes; SHA256 {hashlib.sha256(data).hexdigest().upper()}")
