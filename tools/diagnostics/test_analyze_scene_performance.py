import unittest

from analyze_scene_performance import analyze, markdown


def line(message, pid=20):
    return f'[12:00:00.000] [pid={pid} tid=2] {message}'


def stage(name='plugin-frame', calls=100, main=1, **extra):
    fields=dict(stage=name,changed=0,calls=calls,totalMs=main,maxMs=main,
                mainCalls=calls,mainTotalMs=main,mainMaxMs=main,p95UpperMs=2,
                p99UpperMs=4,mainP95UpperMs=1,mainP99UpperMs=2,bytes=0)
    fields.update(extra)
    return line('[DEBUG-loadperf] '+' '.join(f'{k}={v}' for k,v in fields.items()))


class PerformanceAnalysis(unittest.TestCase):
    def test_main_percentiles_and_overflow_are_not_session_percentiles(self):
        text='\n'.join([line('[BUILD] resource-directory-entry'),stage(),
            stage('material-frame',mainP99UpperMs=-1),stage('material-frame')])
        report=analyze(text)
        material=next(s for s in report['stages'] if s['stage']=='material-frame')
        self.assertTrue(material['mainP99ExceedsHistogram'])
        self.assertIsNone(material['largestWindowMainP99UpperMs'])
        self.assertEqual(material['mainMsPerPluginFrame'],.02)
        self.assertIn('不是整段精确 P95/P99',markdown(report))

    def test_only_full_uptime_windows_are_attributed_to_scenarios(self):
        rows=[line('[BUILD] resource-directory-entry',19),stage(calls=9999),line('[BUILD] resource-directory-entry')]
        for index in range(4):
            rows.extend([line(f'[DEBUG-loadperf] window=1 windowId={index+1} startTickMs={1000+index*5000} endTickMs={6000+index*5000} spanMs=5000'),
                stage(calls=100,main=index+1),
                line('[DEBUG-loadperf] operation=managed-invoke calls=200 mainCalls=190'),
                line(f'[DEBUG-loadperf] runtimeWorkload=1 effectInstances={index} sampleAgeMs=0'),
                line('[DEBUG-loadperf] process=1 privateBytes=100 wholeProcess=1')])
        markers=[dict(kind='begin',runId='a',processId=20,uptimeMs=2000,caseId='npc'),
                 dict(kind='end',runId='a',processId=20,uptimeMs=19000)]
        report=analyze('\n'.join(rows),markers=markers)
        case=report['scenarios'][0]
        self.assertEqual((case['fullWindows'],case['boundaryWindowsExcluded']),(2,2))
        self.assertEqual(case['stages'][0]['calls'],200)
        self.assertEqual(case['stages'][0]['mainTotalMs'],5)
        self.assertEqual(len(case['operationWindows']),2)
        self.assertEqual(case['wholeProcessPrivateBytesDelta'],0)

    def test_short_or_unfinished_case_does_not_invent_zero_cost(self):
        text='\n'.join([line('[BUILD] resource-directory-entry'),
            line('[DEBUG-loadperf] window=1 windowId=1 startTickMs=1000 endTickMs=6000 spanMs=5000'),stage()])
        report=analyze(text,markers=[dict(kind='begin',runId='x',processId=20,caseId='ui',uptimeMs=2000)])
        self.assertEqual(report['scenarios'][0]['status'],'no-full-windows')
        self.assertFalse(report['scenarios'][0]['complete'])
        self.assertEqual(report['scenarios'][0]['stages'],[])

    def test_worker_only_stage_does_not_report_main_thread_percentiles(self):
        text='\n'.join([line('[BUILD] resource-directory-entry'),stage('resource-prepare',mainCalls=0,mainTotalMs=0,mainMaxMs=0,mainP95UpperMs=0,mainP99UpperMs=0)])
        row=analyze(text)['stages'][0]
        self.assertIsNone(row['largestWindowMainP95UpperMs'])
        self.assertIsNone(row['largestWindowMainP99UpperMs'])

    def test_old_logs_and_pid_selection_remain_supported(self):
        text='\n'.join([line('[BUILD] resource-directory-entry',19),stage(calls=888),
            line('[BUILD] resource-directory-entry'),stage(calls=100,p95UpperMs=-1),
            line('[DEBUG-loadperf] runtimeWorkload=1 sampleAgeMs=20000')])
        result=analyze(text,19)
        self.assertEqual(result['pid'],19)
        self.assertEqual(result['stages'],[])
        current=analyze(text)
        self.assertIsNone(current['stages'][0]['largestWindowP95UpperMs'])
        self.assertTrue(current['warnings'])


if __name__=='__main__':unittest.main()
