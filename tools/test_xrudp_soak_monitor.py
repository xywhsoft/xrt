"""Observer exit races and ctypes cache growth must not invalidate a soak."""
from types import SimpleNamespace
import ctypes
import os
import unittest
from unittest.mock import patch
import test_xrudp_native_soak as tool


class SoakMonitor(unittest.TestCase):
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
