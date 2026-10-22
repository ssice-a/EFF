"""Summarize opt-in DLL timings; inclusive stages are never added together."""
import argparse
import json
import re
from pathlib import Path


def aggregate_stages(rows):
    stages={}
    percentiles={'p95UpperMs':('largestWindowP95UpperMs','p95ExceedsHistogram'),
                 'p99UpperMs':('largestWindowP99UpperMs','p99ExceedsHistogram'),
                 'mainP95UpperMs':('largestWindowMainP95UpperMs','mainP95ExceedsHistogram'),
                 'mainP99UpperMs':('largestWindowMainP99UpperMs','mainP99ExceedsHistogram')}
    for fields in rows:
        key=(fields['stage'],fields.get('changed',0))
        aggregate=stages.setdefault(key,dict(stage=key[0],changed=key[1],calls=0,totalMs=0,mainCalls=0,mainTotalMs=0,maxMs=0,mainMaxMs=0,bytes=0))
        for name in ('calls','totalMs','mainCalls','mainTotalMs','bytes'):
            aggregate[name]+=fields.get(name,0)
        for name in ('maxMs','mainMaxMs'):
            aggregate[name]=max(aggregate[name],fields.get(name,0))
        for source,(target,overflow) in percentiles.items():
            aggregate.setdefault(target,None);aggregate.setdefault(overflow,False)
            if source.startswith('main') and not fields.get('mainCalls',0):continue
            if source not in fields:continue
            if fields[source]<0:aggregate[overflow]=True;aggregate[target]=None
            elif not aggregate[overflow]:aggregate[target]=max(aggregate[target] or 0,fields[source])
    frames=sum(s['mainCalls'] for s in stages.values() if s['stage']=='plugin-frame')
    for stage in stages.values():
        stage['averageUs']=stage['totalMs']*1000/stage['calls'] if stage['calls'] else None
        stage['mainAverageUs']=stage['mainTotalMs']*1000/stage['mainCalls'] if stage['mainCalls'] else None
        stage['mainMsPerPluginFrame']=stage['mainTotalMs']/frames if frames else None
    return sorted(stages.values(),key=lambda s:s['mainMaxMs'],reverse=True)


def scenario_reports(markers,pid,windows,stages,operations,frames,workloads,memory):
    pending={};intervals=[]
    for row in markers:
        if row.get('processId') not in (0,pid):continue
        if row.get('kind')=='begin':pending[row['runId']]=row
        elif row.get('kind')=='end' and row.get('runId') in pending:
            intervals.append((pending.pop(row['runId']),row))
    intervals.extend((row,None) for row in pending.values())
    result=[]
    for begin,end in intervals:
        start=begin['uptimeMs'];stop=end['uptimeMs'] if end else max((w['endTickMs'] for w in windows),default=start)
        included={w['windowId'] for w in windows if start<=w['startTickMs'] and w['endTickMs']<=stop}
        overlap=[w for w in windows if w['startTickMs']<stop and w['endTickMs']>start and w['windowId'] not in included]
        select=lambda rows:[r for r in rows if r.get('windowId') in included]
        samples=[s for s in select(workloads) if start<=s.get('sampleTickMs',start)<=stop];process=select(memory)
        stage_rows=select(stages);counts=select(operations)
        result.append(dict(caseId=begin.get('caseId','custom'),runId=begin['runId'],name=begin.get('name',''),notes=begin.get('notes',''),
            startTickMs=start,endTickMs=stop,complete=end is not None and not end.get('aborted',False),
            status='measured' if included else 'no-full-windows',fullWindows=len(included),boundaryWindowsExcluded=len(overlap),
            measuredMs=sum(w['spanMs'] for w in windows if w['windowId'] in included),
            stages=aggregate_stages(stage_rows),operationWindows=counts,workloadSamples=samples,processSamples=process,
            maxFrameHookIntervalMs=max((f.get('maxMs',0) for f in select(frames)),default=None),
            wholeProcessPrivateBytesDelta=process[-1]['privateBytes']-process[0]['privateBytes'] if len(process)>1 else None))
    return result


def analyze(text, pid=None, markers=()):
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
    stage_windows=[];frames=[];memory=[];lifetime=[];events=[];windows=[];operations=[];workloads=[];unity_frames=[];window={}
    for stamp,process,thread,message in rows:
        if process!=pid:
            continue
        fields=dict(re.findall(r'(\w+)=([^\s]+)',message))
        if '[DEBUG-loadperf]' in message:
            fields.update(time=stamp,thread=thread)
            for key,value in list(fields.items()):
                if key in ('stage','operation','time'):
                    continue
                try:fields[key]=float(value) if '.' in str(value) else int(value)
                except (TypeError,ValueError):pass
            if fields.get('window'):
                window={key:fields[key] for key in ('windowId','startTickMs','endTickMs','spanMs')}
                windows.append(fields);continue
            fields.update(window)
            if 'stage' in fields:
                stage_windows.append(fields)
            elif 'operation' in fields:operations.append(fields)
            elif fields.get('runtimeWorkload'):workloads.append(fields)
            elif fields.get('unityDeltaFrames'):unity_frames.append(fields)
            elif fields.get('frameHookIntervals'):
                frames.append(fields)
            elif fields.get('process'):
                memory.append(fields)
            elif fields.get('nativeLifetime'):
                lifetime.append(fields)
        elif any(marker in message for marker in ('[ENTRY-STARTUP-COST]','[FULL-RELOAD-PREPARED]','[FULL-RELOAD-REQUEST]','[FULL-RELOAD-LOADED]','[FULL-RELOAD-REJECT]','[FULL-RELOAD-DRAINED]','[FULL-RELOAD-QUARANTINE]','[FULL-RELOAD-LATENCY]','[FULL-RELOAD-COMMIT]','[FULL-RELOAD-FRAME]','[ENTRY-RULE-SUBMIT]','[FULL-RELOAD-UI-','[EFFECT-ATTACHMENT-','[EFFECT-RELOAD','[MATERIAL-ANIMATION-GAP]','[STARTUP-DEPENDENCY-KEYS]')):
            events.append(dict(time=stamp,thread=thread,message=message))
    warnings=[]
    if not stage_windows:warnings.append('No measurement stages: this may be a production DLL log, not a performance test.')
    if markers and not windows:warnings.append('Legacy log has no uptime windows; scenario attribution is unavailable.')
    if any(s.get('sampleAgeMs',0)>10000 for s in workloads):warnings.append('Some runtime workload samples are older than 10 seconds; model hooks may have paused.')
    return dict(pid=pid,stages=aggregate_stages(stage_windows),stageWindows=stage_windows,measurementWindows=windows,
                frameWindows=frames,unityDeltaWindows=unity_frames,operationWindows=operations,runtimeWorkloadSamples=workloads,
                scenarios=scenario_reports(markers,pid,windows,stage_windows,operations,frames,workloads,memory),warnings=warnings,
                slowFrameWindows=sorted(frames,key=lambda s:s.get('maxMs',0),reverse=True)[:12],processSamples=memory,nativeLifetimeSamples=lifetime,events=events,
                scope='DLL measured wall time; inclusive/nested stages must not be added; async Unity work and GPU work are not measured; process memory is the whole game')


def markdown(result):
    lines=['# DLL 性能测试报告', '', f"进程：{result['pid']}。", '', result['scope'], '',
           '分位值是直方图桶上界；跨窗口展示最差窗口上界，不是整段精确 P95/P99。`null` 表示缺少数据或超出直方图范围。', '']
    for warning in result['warnings']:lines.extend(['- '+warning,''])
    def table(stages):
        rows=['| 阶段 | 类别 | 次数 | 主线程总 ms | 主线程最大 ms | 摊到每插件帧 ms | 最差窗口主线程 P99 上界 ms |',
              '| --- | --- | ---: | ---: | ---: | ---: | ---: |']
        number=lambda v:'—' if v is None else f'{v:.4f}'
        for s in stages[:20]:rows.append(f"| {s['stage']} | {s['changed']} | {s['calls']} | {s['mainTotalMs']:.3f} | {s['mainMaxMs']:.3f} | {number(s['mainMsPerPluginFrame'])} | {number(s['largestWindowMainP99UpperMs'])} |")
        return rows
    lines.extend(['## 整个会话','',*table(result['stages']),''])
    for case in result['scenarios']:
        name=str(case['name'] or case['caseId']).replace('\n',' ')
        lines.extend([f'## {name}', '', f"状态：{case['status']}；完整窗口 {case['fullWindows']}，边界窗口排除 {case['boundaryWindowsExcluded']}；测量覆盖 {case['measuredMs']/1000:.1f} 秒。",''])
        if not case['complete']:lines.extend(['场景未记录结束，结果仅作中途观察。',''])
        lines.extend(table(case['stages']));lines.append('')
        if case['workloadSamples']:
            for key in ('mods','parts','uiModels','shapeBindings','materialBindings','activeAnimations','effectInstances','effectGenerations','effectRetired','effectQuarantined','retainedGenerations'):
                values=[s[key] for s in case['workloadSamples'] if key in s]
                if values:lines.append(f'- {key}: {values[0]} → {values[-1]}，范围 {min(values)}～{max(values)}')
            lines.append('')
    return '\n'.join(lines)+'\n'


def main():
    parser=argparse.ArgumentParser();parser.add_argument('log',type=Path);parser.add_argument('--pid',type=int);parser.add_argument('--output',type=Path);parser.add_argument('--markers',type=Path);parser.add_argument('--markdown',type=Path)
    args=parser.parse_args();markers=[json.loads(line) for line in args.markers.read_text(encoding='utf-8-sig').splitlines() if line.strip()] if args.markers else []
    result=analyze(args.log.read_text(encoding='utf-8-sig',errors='replace'),args.pid,markers)
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    if args.markdown:
        args.markdown.parent.mkdir(parents=True,exist_ok=True);args.markdown.write_text(markdown(result),encoding='utf-8')
    print(f"Session {result['pid']}: {len(result['stages'])} stage groups, {len(result['processSamples'])} process samples")
    for stage in result['stages'][:12]:
        print(f"{stage['stage']:18} hit={stage['changed']} calls={stage['calls']} mainTotalMs={stage['mainTotalMs']:.3f} mainMaxMs={stage['mainMaxMs']:.3f} allMaxMs={stage['maxMs']:.3f}")
    print(result['scope'])
    for case in result['scenarios']:print(f"{case['caseId']}: {case['status']}, {case['fullWindows']} full windows, {case['boundaryWindowsExcluded']} boundary windows excluded")


if __name__=='__main__':main()
