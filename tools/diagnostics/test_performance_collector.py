import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import unittest


class PerformanceCollector(unittest.TestCase):
    def test_snapshot_retains_source_and_existing_output(self):
        script=Path(__file__).with_name('Start-EFFPerformanceTest.ps1')
        with tempfile.TemporaryDirectory(prefix='EFF-perf-collector-') as temporary:
            root=Path(temporary);game=root/'game';plugin=game/'plugin';plugin.mkdir(parents=True)
            log=b'[12:00:00.000] [pid=123 tid=456] [BUILD] resource-directory-entry\n'
            (plugin/'eff_log.txt').write_bytes(log);(plugin/'eff.dll').write_bytes(b'fixture-dll')
            before={p.name:hashlib.sha256(p.read_bytes()).digest() for p in plugin.iterdir()}
            args=['powershell','-NoProfile','-ExecutionPolicy','Bypass','-File',str(script),
                  '-GameDirectory',str(game),'-OutputDirectory',str(root/'report'),'-SnapshotOnly']
            result=subprocess.run(args,capture_output=True,text=True,errors='replace')
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            report=root/'report';metadata=json.loads((report/'session.json').read_text('utf-8-sig'))
            self.assertFalse(metadata['changesGameFiles'])
            self.assertEqual(next(report.glob('*.log')).read_bytes(),log)
            events=[json.loads(s) for s in (report/'markers.jsonl').read_text('utf-8-sig').splitlines()]
            self.assertGreater(events[0]['uptimeMs'],0)
            self.assertEqual(before,{p.name:hashlib.sha256(p.read_bytes()).digest() for p in plugin.iterdir()})
            manifest={p.name:p.read_bytes() for p in report.iterdir()}
            result=subprocess.run(args,capture_output=True,text=True,errors='replace')
            self.assertNotEqual(result.returncode,0,'existing report was overwritten')
            self.assertEqual(manifest,{p.name:p.read_bytes() for p in report.iterdir()})


if __name__=='__main__':unittest.main()
