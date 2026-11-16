"""Read-only Windows commit/process/NVIDIA sampling, independent of the game DLL.

Flushes JSONL every sample and preserves both logs when the observed game exits.
No game injection, process control, pagefile changes or Mod changes.
"""
import argparse
import ctypes as C
from ctypes import wintypes as W
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import time


class PerformanceInfo(C.Structure):
    _fields_ = [("cb", W.DWORD)] + [(n, C.c_size_t) for n in (
        "CommitTotal", "CommitLimit", "CommitPeak", "PhysicalTotal",
        "PhysicalAvailable", "SystemCache", "KernelTotal", "KernelPaged",
        "KernelNonpaged", "PageSize")] + [(n, W.DWORD) for n in (
        "HandleCount", "ProcessCount", "ThreadCount")]


class ProcessEntry(C.Structure):
    _fields_ = [("dwSize", W.DWORD), ("cntUsage", W.DWORD),
                ("pid", W.DWORD), ("heap", C.c_size_t),
                ("module", W.DWORD), ("threads", W.DWORD),
                ("parent", W.DWORD), ("priority", W.LONG),
                ("flags", W.DWORD), ("exe", W.WCHAR * 260)]


class ProcessMemory(C.Structure):
    _fields_ = [("cb", W.DWORD), ("PageFaultCount", W.DWORD)] + [
        (n, C.c_size_t) for n in (
            "PeakWorkingSetSize", "WorkingSetSize", "QuotaPeakPagedPoolUsage",
            "QuotaPagedPoolUsage", "QuotaPeakNonPagedPoolUsage",
            "QuotaNonPagedPoolUsage", "PagefileUsage", "PeakPagefileUsage",
            "PrivateUsage")]


class GpuMemory(C.Structure):
    _fields_ = [(n, C.c_uint64) for n in ("total", "free", "used")]


class Sampler:
    def __init__(self):
        self.k = C.WinDLL("kernel32", use_last_error=True)
        self.p = C.WinDLL("psapi", use_last_error=True)
        self.k.CreateToolhelp32Snapshot.argtypes = [W.DWORD, W.DWORD]
        self.k.CreateToolhelp32Snapshot.restype = W.HANDLE
        for name in ("Process32FirstW", "Process32NextW"):
            fn = getattr(self.k, name)
            fn.argtypes = [W.HANDLE, C.POINTER(ProcessEntry)]
            fn.restype = W.BOOL
        self.k.CloseHandle.argtypes = [W.HANDLE]
        self.k.OpenProcess.argtypes = [W.DWORD, W.BOOL, W.DWORD]
        self.k.OpenProcess.restype = W.HANDLE
        self.p.GetPerformanceInfo.argtypes = [C.POINTER(PerformanceInfo), W.DWORD]
        self.p.GetProcessMemoryInfo.argtypes = [W.HANDLE, C.POINTER(ProcessMemory), W.DWORD]
        self.nvml = None
        self.gpu = C.c_void_p()
        self.gpu_error = None
        try:
            nvml = C.CDLL(str(Path(os.environ['SystemRoot']) / 'System32' / 'nvml.dll'))
            nvml.nvmlDeviceGetHandleByIndex_v2.argtypes = [C.c_uint, C.POINTER(C.c_void_p)]
            nvml.nvmlDeviceGetMemoryInfo.argtypes = [C.c_void_p, C.POINTER(GpuMemory)]
            if nvml.nvmlInit_v2() or nvml.nvmlDeviceGetHandleByIndex_v2(0, C.byref(self.gpu)):
                raise RuntimeError("NVML initialization failed")
            self.nvml = nvml
        except (OSError, RuntimeError) as error:
            self.gpu_error = str(error)

    def sample(self):
        info = PerformanceInfo()
        info.cb = C.sizeof(info)
        if not self.p.GetPerformanceInfo(C.byref(info), info.cb):
            raise C.WinError(C.get_last_error())
        result = {"time": datetime.now(timezone.utc).isoformat(),
                  "monotonic": time.monotonic(),
                  "commitBytes": info.CommitTotal * info.PageSize,
                  "commitLimitBytes": info.CommitLimit * info.PageSize,
                  "commitPeakBytes": info.CommitPeak * info.PageSize,
                  "commitAvailableBytes": (info.CommitLimit - info.CommitTotal) * info.PageSize,
                  "physicalTotalBytes": info.PhysicalTotal * info.PageSize,
                  "physicalAvailableBytes": info.PhysicalAvailable * info.PageSize,
                  "kernelPagedBytes": info.KernelPaged * info.PageSize,
                  "kernelNonpagedBytes": info.KernelNonpaged * info.PageSize,
                  "processes": []}
        snapshot = self.k.CreateToolhelp32Snapshot(2, 0)
        if snapshot == C.c_void_p(-1).value:
            # The observed crash exhausted system commitment and this call
            # itself returned ERROR_COMMITMENT_LIMIT. Keep the system sample;
            # a failed process enumeration is not evidence the game exited.
            result["processSnapshotError"] = C.get_last_error()
            return result
        entry = ProcessEntry()
        entry.dwSize = C.sizeof(entry)
        try:
            more = self.k.Process32FirstW(snapshot, C.byref(entry))
            while more:
                if entry.exe.lower() in ("endfield.exe", "blender.exe"):
                    process = {"pid": entry.pid, "name": entry.exe}
                    handle = self.k.OpenProcess(0x1000, False, entry.pid)
                    if handle:
                        try:
                            memory = ProcessMemory()
                            memory.cb = C.sizeof(memory)
                            if self.p.GetProcessMemoryInfo(handle, C.byref(memory), memory.cb):
                                process.update(privateBytes=memory.PrivateUsage,
                                               workingSetBytes=memory.WorkingSetSize,
                                               peakPrivateBytes=memory.PeakPagefileUsage)
                        finally:
                            self.k.CloseHandle(handle)
                    result["processes"].append(process)
                more = self.k.Process32NextW(snapshot, C.byref(entry))
        finally:
            self.k.CloseHandle(snapshot)
        if self.nvml:
            memory = GpuMemory()
            code = self.nvml.nvmlDeviceGetMemoryInfo(self.gpu, C.byref(memory))
            if not code:
                result["gpu"] = {name + "Bytes": getattr(memory, name) for name in ("total", "free", "used")}
            else:
                result["gpuError"] = code
        return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--game", type=Path, default=Path(r"D:\Hypergryph Launcher\games\Endfield Game"))
    parser.add_argument("--seconds", type=float, default=1200)
    parser.add_argument("--interval", type=float, default=0.5)
    parser.add_argument("--once", action="store_true")
    args = parser.parse_args()
    if args.seconds <= 0 or args.interval < 0.1:
        parser.error("seconds must be positive and interval at least 0.1")
    args.output.mkdir(parents=True, exist_ok=False)
    sampler = Sampler()
    dll = args.game / "plugin/eff.dll"
    metadata = {"dllSha256": hashlib.sha256(dll.read_bytes()).hexdigest() if dll.exists() else None,
                "wholeProcessCounters": True, "changesGameFiles": False,
                "gpuError": sampler.gpu_error, "intervalSeconds": args.interval}
    (args.output / "session.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")
    started = time.monotonic()
    seen = set()
    try:
        with (args.output / "memory.jsonl").open("x", encoding="utf-8", buffering=1) as out:
            while True:
                tick = time.monotonic()
                row = sampler.sample()
                current = {p['pid'] for p in row['processes'] if p['name'].lower() == 'endfield.exe'}
                row['gameExited'] = bool(seen and not current and not row.get('processSnapshotError'))
                out.write(json.dumps(row) + '\n')
                seen.update(current)
                if args.once or row['gameExited'] or tick - started >= args.seconds:
                    break
                time.sleep(max(0, args.interval - (time.monotonic() - tick)))
    finally:
        for source, name in (
            (args.game / 'plugin/eff_log.txt', 'eff_log.txt'),
            (Path(os.environ['USERPROFILE']) / 'AppData/LocalLow/Hypergryph/Endfield/Player.log', 'Player.log')):
            if source.exists():
                try:
                    shutil.copyfile(source, args.output / name)
                except OSError as error:
                    (args.output / (name + '.copy-error.txt')).write_text(str(error), encoding='utf-8')
    print(args.output)


if __name__ == '__main__':
    main()
