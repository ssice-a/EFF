"""Compare cold author compilation on one immutable real-Mod snapshot.

The source Mod is read-only. Temporary outputs are removed on completion.
With --interactive, each input line supplies another compiler to compare;
an empty line ends the benchmark. Each run starts without compiled.bin.
"""
import argparse
import ctypes
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import time


def compile_process(command):
    """Measure the actual compiler process, including its private-byte peak."""
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        encoding='utf-8', errors='replace',
        creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
    peaks = {}
    if __import__('os').name == 'nt':
        class Counters(ctypes.Structure):
            _fields_ = [('cb', ctypes.c_ulong), ('PageFaultCount', ctypes.c_ulong)] + [
                (name, ctypes.c_size_t) for name in ('PeakWorkingSetSize', 'WorkingSetSize',
                'QuotaPeakPagedPoolUsage', 'QuotaPagedPoolUsage',
                'QuotaPeakNonPagedPoolUsage', 'QuotaNonPagedPoolUsage',
                'PagefileUsage', 'PeakPagefileUsage', 'PrivateUsage')]
        query = ctypes.WinDLL('psapi').GetProcessMemoryInfo
        query.argtypes = [ctypes.c_void_p, ctypes.POINTER(Counters), ctypes.c_ulong]
        query.restype = ctypes.c_int
        def sample():
            counters = Counters()
            counters.cb = ctypes.sizeof(counters)
            if query(int(process._handle), ctypes.byref(counters), counters.cb):
                for label, field in (('peakWorkingSetBytes', 'PeakWorkingSetSize'),
                                     ('peakPrivateBytes', 'PeakPagefileUsage')):
                    peaks[label] = max(peaks.get(label, 0), getattr(counters, field))
    else:
        def sample():
            pass
    while True:
        sample()
        try:
            stdout, stderr = process.communicate(timeout=0.05)
            sample()
            return process.returncode, stdout, stderr, peaks
        except subprocess.TimeoutExpired:
            pass


def files(root):
    return {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in root.rglob('*') if p.is_file() and p.name != 'compiled.bin'}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('game', type=Path)
    parser.add_argument('mod', type=Path)
    parser.add_argument('compiler', type=Path)
    parser.add_argument('--interactive', action='store_true')
    parser.add_argument('--compare', type=Path, action='append', default=[],
                        help='additional compiler on the same immutable author snapshot')
    parser.add_argument('--report', type=Path,
                        help='save measurements and exact-output checks as JSON')
    args = parser.parse_args()
    runs = []
    with tempfile.TemporaryDirectory(prefix='eff-compile-benchmark-') as folder:
        root = Path(folder)
        snapshot = root / 'snapshot' / args.mod.name
        shutil.copytree(args.mod, snapshot,
                        ignore=shutil.ignore_patterns('compiled.bin', '*.tmp-*'))
        snapshot_hashes = files(snapshot)
        baseline = None

        def run(compiler):
            nonlocal baseline
            compiler_hash = hashlib.sha256(compiler.read_bytes()).hexdigest()
            destination = root / 'run' / args.mod.name
            if destination.exists():
                shutil.rmtree(destination)
            shutil.copytree(snapshot, destination, copy_function=lambda a, b:
                            __import__('os').link(a, b) or b)
            started = time.perf_counter()
            returncode, stdout, stderr, peaks = compile_process([
                str(compiler.resolve()), '--export-mod', str(destination), str(args.game.resolve())])
            elapsed = time.perf_counter() - started
            if returncode:
                raise RuntimeError(stderr or stdout)
            artifact = (destination / 'compiled.bin').read_bytes()
            assert files(snapshot) == snapshot_hashes, 'compiler changed shared author files'
            assert files(destination) == snapshot_hashes, 'compiler changed author inputs'
            if baseline is None:
                baseline = artifact
            assert artifact == baseline, 'optimized compiler changed compiled.bin bytes'
            costs = {row.split()[1]: float(row.split()[2])
                     for row in stdout.splitlines()
                     if row.startswith('COMPILED-MOD-COST ')}
            row = dict(compiler=str(compiler), compilerSHA256=compiler_hash,
                wallSeconds=elapsed,
                costsMs=costs, compiledBytes=len(artifact),
                compiledSHA256=hashlib.sha256(artifact).hexdigest(),
                exactArtifactMatch=True, authorUnchanged=True, **peaks)
            runs.append(row)
            if args.report:
                args.report.write_text(json.dumps(dict(runs=runs,
                    compilerCaches=False, windowsFileCacheCleared=False),
                    ensure_ascii=False, indent=2), encoding='utf-8')
            print(json.dumps(row), flush=True)

        run(args.compiler)
        for compiler in args.compare:
            run(compiler)
        if args.interactive:
            print('BENCHMARK_READY: send next compiler path or empty line to finish', flush=True)
            while True:
                try:
                    line = input().strip()
                except EOFError:
                    break
                if not line:
                    break
                run(Path(line))
                print('BENCHMARK_READY', flush=True)
    print('BENCHMARK_DONE: temporary outputs removed', flush=True)


if __name__ == '__main__':
    main()
