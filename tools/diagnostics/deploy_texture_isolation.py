"""Install a verified selected package and its native inputs while game is stopped."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[2]
EVIDENCE=ROOT/'diagnostics/texture-selection-audit-20261004'
PLUGIN=Path(r'D:\Hypergryph Launcher\games\Endfield Game\plugin')
GAME=PLUGIN.parent
CHECKER=ROOT/'bin/resource-directory-entry/resource_directory_check.exe'

def sha(path):
    digest=hashlib.sha256()
    with path.open('rb') as file:
        for chunk in iter(lambda:file.read(1024*1024),b''):digest.update(chunk)
    return digest.hexdigest().upper()

def stopped():
    command="Get-CimInstance Win32_Process | Where-Object {$_.Name -match '^Endfield.*\\.exe$'} | Select-Object -ExpandProperty ProcessId"
    check=subprocess.run(['powershell','-NoProfile','-Command',command],capture_output=True,text=True,check=True)
    if check.stdout.strip():raise RuntimeError('Endfield is running: '+check.stdout.strip())

def inside(path, root):
    path=path.resolve();root=root.resolve()
    if path==root or not path.is_relative_to(root):raise ValueError('Unsafe deployment target: '+str(path))
    return path

def files(root):
    return {p.relative_to(root).as_posix():sha(p) for p in root.rglob('*') if p.is_file()}

def run():
    for name in ('texture-verification.json','mesh-verification.json','image-pixel-verification.json'):
        if not json.loads((EVIDENCE/name).read_text('utf-8'))['verified']:
            raise RuntimeError('Verification failed: '+name)
    stopped()
    old_dll=PLUGIN/'eff.dll';table=PLUGIN/'resource-input-substitutions.tsv';mod=PLUGIN/'mods/LZY'
    if sha(old_dll)!='0F582D182617BD5FD30584599CB11D3ACD92D81C6514A2D462982A6ABB92ABE0':
        raise RuntimeError('Installed DLL changed since the v18 audit')
    if sha(table)!='448FA914902AF23FEC427A7349574E7F4FC1DE07B0EFE041AB30020CB1337B65':
        raise RuntimeError('Installed input table changed since the v18 audit')
    rollback=inside(PLUGIN/'resource-input-diagnostics/rollback/v19-texture-isolation',PLUGIN)
    candidate=inside(PLUGIN/'resource-export-staging/v19-texture-isolation',PLUGIN)
    staged_mod=inside(PLUGIN/'resource-export-staging/v19-selected-package/LZY',PLUGIN)
    for path in (rollback,candidate,staged_mod):
        if path.exists():raise FileExistsError(path)
    rollback.mkdir(parents=True)
    shutil.copy2(old_dll,rollback/'eff-v18.dll');shutil.copy2(table,rollback/'table-v18.tsv')
    old_files=files(mod);proxy_hash=sha(GAME/'vulkan-1.dll')
    snapshots=json.loads((ROOT/'diagnostics/resource-directory-entry-20261003/restart-v6/current-input-graph/snapshots.json').read_text())['inputs']
    graph=EVIDENCE/'native-full-graph';candidate.mkdir(parents=True)
    package=EVIDENCE/'selected-export/LZY'
    digest=json.loads(subprocess.check_output([str(CHECKER),'--package-digest',str(package)],text=True))['packageDigest']
    lines=['\t'.join(('EFF_LOADING_INPUT_SUBSTITUTIONS','1','LZY',digest))]
    for row in snapshots:
        path=graph/Path(row['file']).name
        output_hash=sha(path)
        if output_hash==row['sha256']:continue
        target=candidate/path.name;shutil.copy2(path,target)
        lines.append('\t'.join(map(str,(row['physicalPath'],row['offset'],row['bytes'],row['sha256'],target,target.stat().st_size,output_hash))))
    staged_mod.parent.mkdir(parents=True,exist_ok=True);shutil.copytree(package,staged_mod)
    pending=PLUGIN/'resource-input-diagnostics/v19-texture-isolation-pending.tsv'
    pending.write_text('\n'.join(lines)+'\n','utf-8')
    validation=subprocess.check_output([str(CHECKER),'--check-loading-table',str(staged_mod),str(pending),str(GAME)],text=True)
    (EVIDENCE/'installed-table-validation.txt').write_text(validation,'utf-8')
    # Verify both resolved rename endpoints before moving the directory.
    source=inside(mod,PLUGIN/'mods');backup=inside(rollback/'LZY-v18',rollback)
    destination=inside(mod,PLUGIN/'mods');stage=inside(staged_mod,PLUGIN/'resource-export-staging')
    if files(source)!=old_files:raise RuntimeError('Author package changed during preparation')
    stopped()
    source.rename(backup)
    try:
        stage.rename(destination)
        shutil.copy2(ROOT/'bin/resource-directory-entry/eff.dll',old_dll)
        pending.replace(table)
        if files(destination)!=files(package):raise RuntimeError('Installed package file hashes differ')
        validation=subprocess.check_output([str(CHECKER),'--check-loading-table',str(destination),str(table),str(GAME)],text=True)
        if sha(GAME/'vulkan-1.dll')!=proxy_hash:raise RuntimeError('Ordinary Vulkan proxy changed')
    except Exception:
        failed=inside(rollback/'LZY-failed',rollback)
        if destination.exists():destination.rename(failed)
        backup.rename(destination)
        shutil.copy2(rollback/'eff-v18.dll',old_dll);shutil.copy2(rollback/'table-v18.tsv',table)
        raise
    report=dict(format='EFF_TEXTURE_ISOLATION_DEPLOYMENT',version=1,installed=True,
        gameStoppedAtDeployment=True,runtimeVisualAcceptance=False,
        build='resource-directory-entry-v19-texture-isolation',dll=str(old_dll),dllSha256=sha(old_dll),
        mod=str(destination),authorFiles=files(destination),packageDigest=digest,
        selectedObjects=14,exportedMeshes=12,materials=5,textures=11,
        table=str(table),tableSha256=sha(table),substitutions=len(lines)-1,
        sourceNativeShapesPreserved=True,ordinaryProxySha256=proxy_hash,
        runtimeShapesMarkerPresent=(PLUGIN/'resource-input-diagnostics/runtime-shapes.enabled').is_file(),
        rollback=str(rollback),toushiOriginalTextureSlots=7)
    (EVIDENCE/'deployment.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n','utf-8')
    (EVIDENCE/'installed-table-validation.txt').write_text(validation,'utf-8')
    print(json.dumps({k:v for k,v in report.items() if k!='authorFiles'},ensure_ascii=False))

if __name__=='__main__':run()
