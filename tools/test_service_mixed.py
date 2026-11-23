"""Check the build platform and first observation in mixed-load evidence."""
import json
from pathlib import Path
import tempfile
import unittest

from check_service_mixed import check_build, check_windows, digest
from service_mixed_clock import CLOCK_BASIS, validate_clock


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

    def check_journal(self, first, wall_step=None):
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
        if wall_step is not None:
            start["clock_basis"] = report["clock_basis"] = CLOCK_BASIS
            for row in (sample, end):
                elapsed = row["observer_seconds"]
                row["clock"].update(wall_elapsed_seconds=elapsed + wall_step,
                                    offset_seconds=wall_step,
                                    boot_elapsed_seconds=elapsed, active_elapsed_seconds=elapsed)
        return check_windows(map(json.dumps, (start, sample, end)), report, summary, claim, ("client",), False)

    def test_reconciles_first_progress_at_startup_deadline(self):
        self.assertEqual(self.check_journal(25)["rounds"], 100)

    def test_rejects_unobserved_startup_interval(self):
        with self.assertRaisesRegex(ValueError, "initial journal observation gap"):
            self.check_journal(65)


    def test_utc_correction_preserves_observed_run(self):
        for step in (-40, 60):
            with self.subTest(step=step):
                self.assertTrue(self.check_journal(5, wall_step=step)["complete"])

    def test_clock_requires_awake_duration_and_coverage(self):
        previous = dict(monotonic_elapsed_seconds=5, wall_elapsed_seconds=5, offset_seconds=0,
                        boot_elapsed_seconds=5, active_elapsed_seconds=5)
        healthy = dict(monotonic_elapsed_seconds=10, wall_elapsed_seconds=70, offset_seconds=60,
                       boot_elapsed_seconds=10, active_elapsed_seconds=10, tick_gap_seconds=5)
        validate_clock(healthy, previous, duration=10)
        cases = [
            ("host suspend detected", {"boot_elapsed_seconds": 10.5}, None),
            ("active clock diverged", {"boot_elapsed_seconds": 5, "active_elapsed_seconds": 5}, None),
            ("journal observation gap", {"tick_gap_seconds": 6}, None),
            ("journal observation gap", dict(monotonic_elapsed_seconds=20, wall_elapsed_seconds=80,
                                             boot_elapsed_seconds=20, active_elapsed_seconds=20,
                                             tick_gap_seconds=15), None),
            ("incomplete awake duration", {}, 10.1),
        ]
        for reason, changed, duration in cases:
            with self.subTest(reason=reason, changed=changed):
                with self.assertRaisesRegex(ValueError, reason):
                    validate_clock(healthy | changed, previous, duration)


if __name__ == "__main__":
    unittest.main()
