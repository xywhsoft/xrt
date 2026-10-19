"""Observer exit races and ctypes cache growth must not invalidate a soak."""
from types import SimpleNamespace
from pathlib import Path
import tempfile
import ctypes
import os
import unittest
from unittest.mock import patch
import test_xrudp_native_soak as tool


class SoakMonitor(unittest.TestCase):
    def test_compiler_never_replaces_an_existing_acceptance_binary(self):
        with tempfile.TemporaryDirectory() as raw,patch.object(tool.subprocess,'run') as compiler:
            binary=Path(raw)/'native'; binary.write_bytes(b'running acceptance')
            with self.assertRaises(FileExistsError): tool.compile_new(binary,['compiler'])
            self.assertEqual(binary.read_bytes(),b'running acceptance'); compiler.assert_not_called()

    def test_concurrent_build_ownership_is_exclusive(self):
        with tempfile.TemporaryDirectory() as raw,patch.object(tool.subprocess,'run') as compiler:
            binary=Path(raw)/'native'; binary.with_name('native.building').touch()
            with self.assertRaises(FileExistsError): tool.compile_new(binary,['compiler'])
            compiler.assert_not_called(); self.assertTrue(binary.with_name('native.building').exists())

    def test_future_gate_requires_real_work_and_complete_quota_release(self):
        raw={'read':10,'written':10,'messages':1,'games':1,'backpressure':1,'rekeys':1}
        tool.verify_final_sample(raw,False)
        with self.assertRaises(AssertionError): tool.verify_final_sample(raw,True)
        good={**raw,'future_mode':True,'future_submitted':20,'future_completed':20,
              'future_pending':0,'future_retained':0,'future_bytes':0}
        tool.verify_final_sample(good,True)
        for name in ('future_pending','future_retained','future_bytes'):
            with self.subTest(name=name),self.assertRaises(AssertionError):
                tool.verify_final_sample({**good,name:1},True)
        with self.assertRaises(AssertionError):
            tool.verify_final_sample({**good,'future_completed':19},True)

    def test_exit_resource_race_is_bounded_and_can_recover(self):
        child=SimpleNamespace(pid=123,poll=lambda:None)
        with patch.object(tool,'resources',side_effect=[ProcessLookupError(),{'private':1}]),\
                patch.object(tool.time,'monotonic',side_effect=[10,10.1]):
            value,pending=tool.observe_resources(child,None)
            self.assertIsNone(value); self.assertEqual(pending,10)
            value,pending=tool.observe_resources(child,pending)
            self.assertEqual(value,{'private':1}); self.assertIsNone(pending)

    def test_persistent_resource_failure_is_not_ignored(self):
        child=SimpleNamespace(pid=123,poll=lambda:None)
        with patch.object(tool,'resources',side_effect=ProcessLookupError()),\
                patch.object(tool.time,'monotonic',return_value=12.1):
            with self.assertRaises(ProcessLookupError): tool.observe_resources(child,10)

    def test_authoritative_exit_can_finish_output_drain(self):
        child=SimpleNamespace(pid=123,poll=lambda:0)
        with patch.object(tool,'resources',side_effect=KeyError('VmRSS')):
            self.assertEqual(tool.observe_resources(child,None),(None,None))

    @unittest.skipUnless(os.name=='nt','Windows process metrics')
    def test_repeated_samples_do_not_create_unbounded_ctypes_types(self):
        tool.resources(os.getpid()); before=len(ctypes._pointer_type_cache)
        for _ in range(500):
            value=tool.resources(os.getpid()); self.assertGreater(value['private'],0)
        self.assertEqual(len(ctypes._pointer_type_cache),before)


if __name__=='__main__': unittest.main()
