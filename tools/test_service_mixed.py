"""Check the build platform and first observation in mixed-load evidence."""
import json
from pathlib import Path
import tempfile
import unittest

from check_service_mixed import check_build, check_windows, digest


class MixedEvidenceTest(unittest.TestCase):
    def test_build_and_run_platforms_must_agree(self):
        with tempfile.TemporaryDirectory() as name:
            binary = Path(name) / "probe"
            binary.write_bytes(b"probe")
            inputs = dict.fromkeys(("extlibs/xredis/tests/interop/redis_mixed.c",
                                    "single/xrt.h", "single/extlibs/xredis.h"), "0" * 64)
            report = dict(client_sha256=digest(binary), platform="Windows-10-10.0.19045-SP0", inputs=inputs)
            receipt = dict(binary_sha256=report["client_sha256"], platform="win32", inputs=inputs)
            check_build("xredis", binary, receipt, report, "client_sha256")
            receipt["platform"] = "Linux-6.18-x86_64"
            with self.assertRaisesRegex(ValueError, "build and run platform families differ"):
                check_build("xredis", binary, receipt, report, "client_sha256")

    def check_journal(self, first):
        claim = "native_client_72h"
        start = {"status": "running", claim: False, "seconds_requested": 8,
                 "resource_growth_limits": {"private": 64 * 1024 * 1024, "handles": 32}}
        report = {**start, "status": "passed"}
        client = {"private": 4096, "handles": 5}
        sample = {"rounds": 100, "seconds": 5, "samples": 100, "cpu_seconds": 0.1, "client": client,
                  "round_p50_us": 1, "round_p95_us": 2, "round_p99_us": 3, "round_p999_us": 4,
                  "observer_seconds": first, "clock": {"monotonic_elapsed_seconds": first,
                                                       "wall_elapsed_seconds": first, "offset_seconds": 0}}
        terminal = {"rounds": 100, "seconds": 8, "complete": True}
        summary = {**terminal, "windows": 1, "baseline": {"client": client},
                   "cpu_seconds": 0.1, "largest_tick_gap_seconds": 3}
        end = {**terminal, "observer_seconds": first + 3, "clock": {"monotonic_elapsed_seconds": first + 3,
                                                                   "wall_elapsed_seconds": first + 3,
                                                                   "offset_seconds": 0, "tick_gap_seconds": 3}}
        return check_windows(map(json.dumps, (start, sample, end)), report, summary, claim, ("client",), False)

    def test_reconciles_first_progress_at_startup_deadline(self):
        self.assertEqual(self.check_journal(25)["rounds"], 100)

    def test_rejects_unobserved_startup_interval(self):
        with self.assertRaisesRegex(ValueError, "initial journal observation gap"):
            self.check_journal(65)


if __name__ == "__main__":
    unittest.main()
