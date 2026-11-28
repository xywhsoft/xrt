"""Check the build platform and first observation in mixed-load evidence."""
import json
from pathlib import Path
import tempfile
import unittest

from unittest.mock import patch

from check_service_mixed import check_build, check_report, check_running, check_windows, digest
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

    def test_mqtt_tls_evidence_matches_transport_and_identity(self):
        with tempfile.TemporaryDirectory() as name:
            binary = Path(name) / "probe"
            binary.write_bytes(b"probe")
            inputs = dict.fromkeys(("extlibs/xmqtt/tests/interop/mqtt_mixed.c",
                                    "single/xrt.h", "single/extlibs/xmqtt.h"), "0" * 64)
            report = dict(binary_sha256=digest(binary), platform="Linux-x86_64", inputs=inputs.copy())
            receipt = dict(binary_sha256=report["binary_sha256"], platform=report["platform"], inputs=inputs.copy())
            check_build("xmqtt", binary, receipt, report, "binary_sha256")
            report["transport"] = "tls13"
            with self.assertRaisesRegex(ValueError, "MQTT build and run transports differ"):
                check_build("xmqtt", binary, receipt, report, "binary_sha256")
            receipt["transport"] = "tls13"
            report["tls"] = dict(hostname="localhost", cipher="TLS_AES_128_GCM_SHA256",
                                 leaf_sha256="0" * 64, ca_sha256="1" * 64)
            with self.assertRaisesRegex(ValueError, "missing build input fingerprint"):
                check_build("xmqtt", binary, receipt, report, "binary_sha256")
            helper = "extlibs/xmqtt/examples/server/app_tls.h"
            report["inputs"][helper] = receipt["inputs"][helper] = "2" * 64
            check_build("xmqtt", binary, receipt, report, "binary_sha256")
            report["tls"]["leaf_sha256"] = "short"
            with self.assertRaisesRegex(ValueError, "invalid MQTT TLS identity evidence"):
                check_build("xmqtt", binary, receipt, report, "binary_sha256")

    def journal(self, first, wall_step=None):
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
        return (start, sample, end), report, summary

    def check_journal(self, first, wall_step=None):
        rows, report, summary = self.journal(first, wall_step)
        return check_windows(map(json.dumps, rows), report, summary, "native_client_72h", ("client",), False)

    def test_native_acceptance_requires_awake_clock_evidence(self):
        with tempfile.TemporaryDirectory() as name, patch("check_service_mixed.RELEASE_SECONDS", 8):
            folder = Path(name)
            binary = folder / "probe"
            binary.write_bytes(b"probe")
            inputs = dict.fromkeys(("extlibs/xredis/tests/interop/redis_mixed.c",
                                    "single/xrt.h", "single/extlibs/xredis.h"), "0" * 64)
            receipt = dict(binary_sha256=digest(binary), platform="win32", inputs=inputs, commit="0" * 40)
            counters = dict(set=900, get=900, publish=100, subscribe=1, hello=4)
            identity = dict(run_id="fixture", process_id=123, connected_clients=1)
            common = dict(client_sha256=receipt["binary_sha256"], platform="Windows-10-10.0.19045-SP0",
                          inputs=inputs, wsl=False, server_owned=False,
                          server_before=dict(identity, counts=dict.fromkeys(counters, 0)),
                          server_after=dict(identity, counts=counters), command_delta=counters)
            report_path = folder / "redis.json"
            for wall_step in (None, 0):
                with self.subTest(clock_basis=wall_step):
                    rows, report, summary = self.journal(5, wall_step)
                    rows[0].update(common)
                    report.update(common, workload=summary, native_client_72h=True)
                    report_path.write_text(json.dumps(report), encoding="utf-8")
                    report_path.with_suffix(".jsonl").write_text(
                        "\n".join(map(json.dumps, rows)), encoding="utf-8")
                    evidence = check_report("xredis", binary, report_path, receipt)
                    self.assertIs(evidence["native_72h"], wall_step is not None)

    def test_native_replay_requires_broker_sample_in_last_window(self):
        (start, sample, end), report, summary = self.journal(5, wall_step=0)
        start["native_72h"] = False
        report["native_72h"] = True
        sample.update(replays=7, shared0=150, shared1=150)
        for rank in (50, 95, 99, 999):
            sample[f"p{rank}_us"] = sample.pop(f"round_p{rank}_us")
        end.update(replays=7)
        summary.update(replays=7, shared0=150, shared1=150)
        roles = ("client", "broker")
        with self.assertRaisesRegex(ValueError, "missing process resource sample"):
            check_windows(map(json.dumps, (start, sample, end)), report, summary, "native_72h", roles, True)

        sample["broker"] = sample.pop("client")
        summary["baseline"] = {"broker": sample["broker"]}
        result = check_windows(map(json.dumps, (start, sample, end)), report, summary, "native_72h", roles, True)
        self.assertTrue(result["complete"])

    def test_running_replay_checks_complete_lines_without_a_final_report(self):
        with tempfile.TemporaryDirectory() as name:
            folder = Path(name)
            binary = folder / "probe"
            binary.write_bytes(b"probe")
            inputs = dict.fromkeys(("extlibs/xredis/tests/interop/redis_mixed.c",
                                    "single/xrt.h", "single/extlibs/xredis.h"), "0" * 64)
            receipt = dict(binary_sha256=digest(binary), platform="win32", inputs=inputs, commit="0" * 40)
            (start, sample, terminal), _, _ = self.journal(5, wall_step=0)
            start.update(client_sha256=receipt["binary_sha256"], platform="Windows-10-10.0.19045-SP0",
                         inputs=inputs, wsl=False, server_owned=False)
            journal = folder / "redis.jsonl"
            prefix = b"".join(json.dumps(row).encode() + b"\n" for row in (start, sample))
            journal.write_bytes(prefix + b'{"partial":')
            result = check_running("xredis", binary, journal, receipt)
            self.assertFalse(result["terminal_seen"])
            self.assertEqual((result["rounds"], result["windows"], result["awake_seconds"]), (100, 1, 5))
            self.assertEqual(result["latest_sampled_roles"], ["client"])
            journal.write_bytes(prefix + json.dumps(terminal).encode() + b"\n")
            self.assertTrue(check_running("xredis", binary, journal, receipt)["terminal_seen"])
            journal.write_bytes(prefix + b'{"partial":\n')
            with self.assertRaises(json.JSONDecodeError):
                check_running("xredis", binary, journal, receipt)

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
