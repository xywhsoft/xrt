"""A child exit code or a forged duration must never create acceptance evidence."""
import contextlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

BENCH=Path(__file__).resolve().parents[1]/'bench'
sys.path.insert(0,str(BENCH))
import production_soak as soak


class AcceptanceRecorderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler=shutil.which('gcc')
        if not compiler:
            raise unittest.SkipTest('gcc required for a real child-process fixture')
        cls.temporary=tempfile.TemporaryDirectory(prefix='xtp-acceptance-recorder-')
        cls.folder=Path(cls.temporary.name)
        source=cls.folder/'child.c';cls.exe=cls.folder/('child.exe' if sys.platform=='win32' else 'child')
        source.write_text(r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
    const char* mode=getenv("XTP_TEST_RECORDER_CASE");
    if(mode && !strcmp(mode,"exit"))return 7;
    if(mode && !strcmp(mode,"diagnostic"))puts("runtime error: test-only recovered UB");
    puts("{\"checkpoint\":true,\"seconds\":1,\"completed\":4000,\"live_xtp_bytes\":128,\"ledger_match\":true,\"quiescent\":true}");
    if(mode && !strcmp(mode,"duplicate"))puts("{\"checkpoint\":true,\"seconds\":2,\"completed\":4000,\"live_xtp_bytes\":128,\"ledger_match\":true,\"quiescent\":true}");
    puts("{\"summary\":true,\"seconds\":259200,\"rounds\":1,\"completed\":4000}");
    return 0;
}
''',encoding='utf-8')
        subprocess.run([compiler,'-std=c99','-Wall','-Wextra','-Werror',str(source),'-o',str(cls.exe)],check=True)

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def run_case(self,name,memory=None):
        report=self.folder/(name+'.json')
        argv=['production_soak.py','--exe',str(self.exe),'--report',str(report),'--seconds','10','--connections','1']
        sleep=time.sleep
        with patch.object(sys,'argv',argv),patch.dict('os.environ',{'XTP_TEST_RECORDER_CASE':name}),contextlib.redirect_stdout(io.StringIO()):
            with patch.object(soak.time,'sleep',lambda _:sleep(.02)):
                if memory is None:
                    status=soak.main()
                else:
                    with patch.object(soak,'resident',return_value={'resident_bytes':memory}):
                        status=soak.main()
        data=json.loads(report.read_text(encoding='utf-8'))
        self.assertEqual(status,1);self.assertEqual(data['status'],'failed')
        self.assertTrue(Path(data['child_log']).exists());self.assertTrue(data['child_log_sha256'])
        return data

    def test_fake_three_day_duration_is_rejected(self):
        self.assertIn('real duration',self.run_case('duration')['failure'])

    def test_nonzero_exit_preserves_failure(self):
        self.assertEqual(self.run_case('exit')['exit_code'],7)

    def test_recovered_sanitizer_diagnostic_is_failure(self):
        self.assertIn('child diagnostic',self.run_case('diagnostic')['failure'])

    def test_duplicate_progress_is_rejected(self):
        self.assertIn('non-monotonic',self.run_case('duplicate')['failure'])

    def test_memory_budget_is_enforced(self):
        self.assertIn('RSS exceeded',self.run_case('rss',4*1024**3)['failure'])


if __name__=='__main__':
    unittest.main()
