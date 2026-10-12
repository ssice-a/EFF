"""Compare first compilation in isolated Mod copies; never edit installed Mods."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

parser=argparse.ArgumentParser()
parser.add_argument('mod',type=Path)
parser.add_argument('game',type=Path)
parser.add_argument('before',type=Path)
parser.add_argument('after',type=Path)
parser.add_argument('--report',type=Path)
args=parser.parse_args()
rows=[]
with tempfile.TemporaryDirectory(prefix='eff-cold-compile-') as folder:
    target=Path(folder)/args.mod.name
    shutil.copytree(args.mod,target)
    for label,compiler in [('before',args.before),('after',args.after)]:
        (target/'compiled.bin').unlink(missing_ok=True)
        started=time.perf_counter()
        result=subprocess.run([str(compiler),'--export-mod',str(target),str(args.game)],
            capture_output=True,text=True,encoding='utf-8',errors='replace',
            creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
        if result.returncode:
            raise RuntimeError(result.stdout+result.stderr)
        costs={line.split()[1]:float(line.split()[2]) for line in result.stdout.splitlines()
               if line.startswith('COMPILED-MOD-COST ')}
        rows.append(dict(label=label,seconds=time.perf_counter()-started,costsMs=costs,
            compiledSha256=hashlib.sha256((target/'compiled.bin').read_bytes()).hexdigest()))
    assert rows[0]['compiledSha256']==rows[1]['compiledSha256'],'first compile output differs'
    compiled_bytes=(target/'compiled.bin').stat().st_size
report=dict(runs=rows,compiledExactlyEqual=True,compiledBytes=compiled_bytes)
if args.report:
    args.report.write_text(json.dumps(report,ensure_ascii=False,indent=2),'utf-8')
print(json.dumps(report,ensure_ascii=False))
