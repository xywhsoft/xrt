"""Regression tests for the Redis comparison's peer command ledger."""

from argparse import Namespace
import unittest

from compare_hiredis import fixture_expected_commands


class RedisFixtureLedgerTest(unittest.TestCase):
    def test_mixed_counts_follow_admission_after_capacity_rejections(self):
        args = Namespace(connections=4, mixed=True)
        runs = [
            {"verified": True, "accounted": True,
             "metrics": {"accepted": 16, "successful": 16, "corrupt": 0,
                         "accepted_get": 12, "accepted_set": 4}},
            {"verified": False, "accounted": True,
             "metrics": {"accepted": 11, "successful": 11, "corrupt": 0,
                         "accepted_get": 8, "accepted_set": 3, "harness_capacity": 5}},
        ]
        self.assertEqual(fixture_expected_commands(args, runs), {"hello": 8, "ping": 8, "get": 20, "set": 9})

    def test_echo_counts_follow_admission(self):
        args = Namespace(connections=2, mixed=False)
        run = {"verified": False, "accounted": True,
               "metrics": {"accepted": 3, "successful": 3, "corrupt": 0, "harness_capacity": 1}}
        self.assertEqual(fixture_expected_commands(args, [run]), {"hello": 2, "ping": 2, "echo": 3})

    def test_unknown_wire_outcomes_leave_exact_counts_unverified(self):
        args = Namespace(connections=1, mixed=False)
        cases = ({"verified": False},
                 {"accounted": True, "metrics": {"accepted": 4, "successful": 3, "corrupt": 0}},
                 {"accounted": True, "metrics": {"accepted": 3, "successful": 3, "corrupt": 1}})
        for run in cases:
            with self.subTest(run=run):
                self.assertIsNone(fixture_expected_commands(args, [run]))


if __name__ == "__main__":
    unittest.main()
