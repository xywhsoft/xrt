"""Verify APP-owned cron checkpoint replay across separate processes."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile


def run(program, checkpoint, *args):
    return subprocess.run(
        [str(program.resolve()), args[0], str(checkpoint), *args[1:]],
        capture_output=True, text=True, timeout=10,
    )


def verify(program):
    with tempfile.TemporaryDirectory() as work:
        checkpoint = Path(work) / "cron checkpoint.txt"
        assert run(program, checkpoint, "seed", "1000").returncode == 0
        interrupted = run(program, checkpoint, "replay", "5000", "4", "3000")
        assert interrupted.returncode == 1 and interrupted.stdout.splitlines() == ["ran 2000"], interrupted
        assert checkpoint.read_text() == "2000\n"
        limited = run(program, checkpoint, "replay", "5000", "2")
        assert limited.returncode == 0 and limited.stdout.splitlines() == [
            "ran 3000", "ran 4000", "last=4000 ran=2 pending=1",
        ], limited
        final = run(program, checkpoint, "replay", "5000", "2")
        assert final.returncode == 0 and final.stdout.splitlines() == [
            "ran 5000", "last=5000 ran=1 pending=0",
        ], final
        for horizon in ("5000", "4000"):
            repeat = run(program, checkpoint, "replay", horizon, "2")
            assert repeat.returncode == 0 and repeat.stdout.splitlines() == [
                "last=5000 ran=0 pending=0",
            ], repeat
        assert checkpoint.read_text() == "5000\n"
        checkpoint.write_bytes(b"9000\0\n")
        invalid = run(program, checkpoint, "replay", "10000", "2")
        assert invalid.returncode == 1 and invalid.stdout == ""
        assert checkpoint.read_bytes() == b"9000\0\n"
    return {"persisted_last_ms": 5000, "retried_ms": 3000, "budgeted": True, "clock_rollback": True,
            "rejected_corrupt_checkpoint": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("program", type=Path)
    parser.add_argument("report", type=Path)
    args = parser.parse_args()
    result = verify(args.program)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print("[pass] cross-process cron catch-up, retry, bounded replay, clock rollback and invalid checkpoint")


if __name__ == "__main__":
    main()
