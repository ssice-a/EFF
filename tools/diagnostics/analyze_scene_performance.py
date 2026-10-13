"""Summarize opt-in DLL timings; inclusive stages are never added together."""
import argparse
import json
import re
from pathlib import Path


def analyze(text, pid=None):
    rows=[]
    for line in text.splitlines():
        match=re.match(r'\[([^]]+)\] \[pid=(\d+) tid=(\d+)\] (.*)',line)
        if not match:
            continue
        stamp,process,thread,message=match.groups()
        rows.append((stamp,int(process),int(thread),message))
    if pid is None:
        sessions=[process for _,process,_,message in rows if '[BUILD] resource-directory-entry' in message]
        if not sessions:
            raise ValueError('No DLL build/session record in the log')
        pid=sessions[-1]
    stages={};stage_windows=[];frames=[];memory=[];lifetime=[];events=[]
    for stamp,process,thread,message in rows:
        if process!=pid:
            continue
        fields=dict(re.findall(r'(\w+)=([^\s]+)',message))
        if '[DEBUG-loadperf]' in message:
            fields.update(time=stamp,thread=thread)
            for key,value in list(fields.items()):
                if key in ('stage','time'):
                    continue
                try:fields[key]=float(value) if '.' in str(value) else int(value)
                except (TypeError,ValueError):pass
            if 'stage' in fields:
                stage_windows.append(fields)
                key=(fields['stage'],fields.get('changed',0))
                aggregate=stages.setdefault(key,dict(stage=key[0],changed=key[1],calls=0,totalMs=0,mainCalls=0,mainTotalMs=0,maxMs=0,mainMaxMs=0,bytes=0,largestWindowP95UpperMs=0,p95ExceedsHistogram=False))
                for name in ('calls','totalMs','mainCalls','mainTotalMs','bytes'):
                    aggregate[name]+=fields.get(name,0)
                for name in ('maxMs','mainMaxMs'):
                    aggregate[name]=max(aggregate[name],fields.get(name,0))
                p95=fields.get('p95UpperMs',0)
                if p95<0:
                    aggregate['p95ExceedsHistogram']=True
                    aggregate['largestWindowP95UpperMs']=None
                elif not aggregate['p95ExceedsHistogram']:
                    aggregate['largestWindowP95UpperMs']=max(aggregate['largestWindowP95UpperMs'],p95)
            elif fields.get('frameHookIntervals'):
                frames.append(fields)
            elif fields.get('process'):
                memory.append(fields)
            elif fields.get('nativeLifetime'):
                lifetime.append(fields)
        elif any(marker in message for marker in ('[ENTRY-STARTUP-COST]','[FULL-RELOAD-PREPARED]','[FULL-RELOAD-REQUEST]','[FULL-RELOAD-LOADED]','[FULL-RELOAD-REJECT]','[FULL-RELOAD-DRAINED]','[FULL-RELOAD-QUARANTINE]','[FULL-RELOAD-LATENCY]','[FULL-RELOAD-COMMIT]','[FULL-RELOAD-FRAME]','[ENTRY-RULE-SUBMIT]')):
            events.append(dict(time=stamp,thread=thread,message=message))
    for stage in stages.values():
        stage['averageUs']=stage['totalMs']*1000/stage['calls'] if stage['calls'] else 0
    return dict(pid=pid,stages=sorted(stages.values(),key=lambda s:s['mainMaxMs'],reverse=True),stageWindows=stage_windows,slowFrameWindows=sorted(frames,key=lambda s:s.get('maxMs',0),reverse=True)[:12],processSamples=memory,nativeLifetimeSamples=lifetime,events=events,
                scope='DLL measured wall time; inclusive/nested stages must not be added; async Unity work and GPU work are not measured; process memory is the whole game')


def main():
    parser=argparse.ArgumentParser();parser.add_argument('log',type=Path);parser.add_argument('--pid',type=int);parser.add_argument('--output',type=Path)
    args=parser.parse_args();result=analyze(args.log.read_text(encoding='utf-8',errors='replace'),args.pid)
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print(f"Session {result['pid']}: {len(result['stages'])} stage groups, {len(result['processSamples'])} process samples")
    for stage in result['stages'][:12]:
        print(f"{stage['stage']:18} hit={stage['changed']} calls={stage['calls']} mainTotalMs={stage['mainTotalMs']:.3f} mainMaxMs={stage['mainMaxMs']:.3f} allMaxMs={stage['maxMs']:.3f}")
    print(result['scope'])


if __name__=='__main__':main()
