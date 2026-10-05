"""Real CLI regression: watcher + Blender-style one-shot, stale/failed author.

Uses an isolated author/cache; never touches the installed Mod or Blender scene.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

def run(args):
    root=args.output.resolve();root.mkdir(parents=True,exist_ok=False)
    author=root/'LZY';shutil.copytree(args.author,author)
    cache=root/'cache';(cache/'generations').mkdir(parents=True)
    before=(args.cache/'current.tsv').read_bytes()
    key=before.decode().splitlines()[1].split('\t')[0]
    shutil.copytree(args.cache/'generations'/key,cache/'generations'/key)
    (cache/'current.tsv').write_bytes(before)
    command=[str(args.compiler.resolve()),str(author),str(args.baseline.resolve()),str(cache),str(args.assembly.resolve())]
    flags=getattr(subprocess,'CREATE_NO_WINDOW',0)
    def once():
        result=subprocess.run(command,capture_output=True,text=True,encoding='utf-8',errors='replace',creationflags=flags,timeout=120)
        assert result.returncode==0,(result.stdout,result.stderr)
        return result.stdout
    def wait_log(path,needle):
        end=time.monotonic()+100
        while time.monotonic()<end:
            if needle in path.read_text('utf-8',errors='replace'):return
            assert watcher.poll() is None,'watcher exited unexpectedly'
            time.sleep(.1)
        raise AssertionError('watcher did not reach '+needle)
    log=root/'watcher.txt'
    with log.open('w',encoding='utf-8') as stream:
        watcher=subprocess.Popen(command+['--watch-parent',str(os.getpid())],stdout=stream,stderr=subprocess.STDOUT,creationflags=flags)
        try:
            wait_log(log,'OFFLINE-REUSED')
            cached=once();assert (cache/'current.tsv').read_bytes()==before
            duplicate=subprocess.run(command+['--watch'],capture_output=True,text=True,creationflags=flags,timeout=5)
            assert duplicate.returncode==0 and 'ALREADY-ACTIVE' in duplicate.stdout
            # A label is author content, so the same compiler must publish a new
            # generation; the one-shot queues while the watcher is constructing it.
            ini=author/'mod.ini';original=ini.read_bytes()
            changed=original.replace(b'label=VaginaOpen',b'label=OfflineConcurrencyWitness')
            assert changed!=original
            ini.write_bytes(changed)
            wait_log(log,'OFFLINE-PROGRESS')
            rebuilt=once();after=(cache/'current.tsv').read_bytes()
            assert after!=before and watcher.poll() is None
            builds=log.read_text('utf-8',errors='replace').count('OFFLINE-PUBLISHED')
            (author/'state.ini').write_text('[State]\nvalue=1\n','utf-8');time.sleep(2)
            assert log.read_text('utf-8',errors='replace').count('OFFLINE-PUBLISHED')==builds
            # Invalid new author must not republish the old generation as new.
            ini.write_bytes(b'[Mod]\nformat=999\nid=LZY\n')
            failed=subprocess.run(command,capture_output=True,text=True,creationflags=flags,timeout=120)
            assert failed.returncode!=0 and (cache/'current.tsv').read_bytes()==after
            ini.write_bytes(changed)
            restored=once();assert (cache/'current.tsv').read_bytes()==after
            (root/'one-shot.txt').write_text(cached+rebuilt+restored,'utf-8')
        finally:
            watcher.terminate();watcher.wait(timeout=10)
    # A compiler attached to a parent must release its watch/build mutex on exit.
    parent=subprocess.Popen([sys.executable,'-c','import time; time.sleep(3)'],creationflags=flags)
    with (root/'parent-exit.txt').open('w') as stream:
        child=subprocess.Popen(command+['--watch-parent',str(parent.pid)],stdout=stream,stderr=subprocess.STDOUT,creationflags=flags)
        parent.wait(timeout=10);assert child.wait(timeout=10)==0
    report=dict(passed=True,cachedOneShotWhileWatcher=True,changedAuthorOneShotWhileWatcher=True,
                duplicateWatcherExcluded=True,stateOnlyIgnored=True,failedAuthorRetainsCurrent=True,
                parentExitStopsWatcher=True,originalKey=key,newKey=after.decode().splitlines()[1].split('\t')[0])
    (root/'result.json').write_text(json.dumps(report,indent=2)+'\n','utf-8');print(json.dumps(report))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('compiler','author','baseline','cache','assembly','output'):parser.add_argument('--'+name,type=Path,required=True)
    run(parser.parse_args())
