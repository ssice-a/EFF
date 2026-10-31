"""Verify the actual release binary excludes temporary probes and keeps lifecycle bindings."""
from pathlib import Path
import struct
import sys

def imports(data):
    """Read PE imports without requiring a developer-installed PE library."""
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    assert data[pe:pe+4] == b'PE\0\0', 'Invalid PE file'
    count = struct.unpack_from('<H', data, pe+6)[0]
    size = struct.unpack_from('<H', data, pe+20)[0]
    optional = pe+24
    assert struct.unpack_from('<H', data, optional)[0] == 0x20b, 'Expected x64 PE'
    section = optional+size

    def offset(rva):
        for i in range(count):
            virtual_size, address, raw_size, raw = struct.unpack_from('<IIII', data, section+40*i+8)
            if address <= rva < address+max(virtual_size, raw_size):
                return raw+rva-address
        raise AssertionError('Import RVA outside sections')

    directory = struct.unpack_from('<I', data, optional+112+8)[0]
    if not directory:
        return []
    cursor = offset(directory)
    result = []
    while any(struct.unpack_from('<IIIII', data, cursor)):
        name = offset(struct.unpack_from('<I', data, cursor+12)[0])
        result.append(data[name:data.index(b'\0', name)].decode('ascii').lower())
        cursor += 20
    return result

path = Path(sys.argv[1])
data = path.read_bytes()
load_perf = '--load-perf' in sys.argv[2:]
effects_debug = '--effects-debug' in sys.argv[2:]
checked = data.replace(b'[DEBUG-loadperf]', b'load-performance') if load_perf else data
if load_perf:
    assert b'[DEBUG-loadperf]' in data, 'Expected enabled performance measurements'
if effects_debug:
    assert b'[EFFECT-DEBUG-READY]' in data and b'EFF-Effects-' in data, 'Expected isolated effects debug channel'
else:
    assert b'[EFFECT-DEBUG-' not in data and b'EFF-Effects-' not in data, 'Production DLL includes opt-in effects debugger'
for dll in imports(data):
    assert not dll.startswith(('msvcp', 'vcruntime', 'api-ms-win-crt-')) and dll != 'ucrtbase.dll', \
        f'Production DLL depends on host C++ runtime: {dll}'
for proxy in ('d3dcompiler_47.dll', 'vulkan-1.dll'):
    sibling = path.with_name(proxy)
    if sibling.exists():
        for dll in imports(sibling.read_bytes()):
            assert not dll.startswith(('msvcp', 'vcruntime', 'api-ms-win-crt-')) and dll != 'ucrtbase.dll', \
                f'Proxy depends on host C++ runtime: {proxy}: {dll}'
for marker in (
    b"[DEBUG-", b"[ENTRY-CAPTURE]", b"[ENTRY-SNAPSHOT]", b"[ENTRY-SNAPSHOT-SELECTION]",
    b"[ENTRY-COUNTERS]", b"[CAPTURE-ELIGIBILITY]", b"[RELOAD-LOAD-SUBMITTED]",
    b"resource-input-diagnostics", b"snapshot-ranges.tsv", b"[ADAPT-PROBE-", b"[CATALOG-SURVEY-", b"[MESH-TARGET-", b"[MESH-NATIVE-", b"[DEMAND-PROBE-", b"[DEMAND-NATIVE-",
):
    assert marker not in checked, f"Production DLL contains diagnostic marker: {marker!r}"
for marker in (
    b"[FULL-RELOAD-COMMIT]", b"[FULL-RELOAD-REJECT]", b"eff_log.txt",
    b"ForceExitAllPerformOnEntity", b"_PreloadSpIdlePerform", b"_PreloadSpDashPerform",
    b"ChangeAnimationConfig", b"UpdateAllWeaponMountPoint", b"ReloadWeapon",
    b"[SHAPE-CAPTURE-COMPLETION]",
):
    assert marker in data, f"Production DLL is missing functional binding: {marker!r}"
print(f"PASS {'effects-debug' if effects_debug else 'performance' if load_perf else 'production'} DLL: {path.name}, {len(data):,} bytes; host C++ runtime independent, capture probes excluded, lifecycle bindings retained")
