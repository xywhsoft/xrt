"""Verify replay and committed advancement across live cron processes."""

import argparse
import json
from pathlib import Path
import queue
import subprocess
import tempfile
import threading

from run_checkpoint_store_crash import command, digest, wire


def interrupt(client, directory):
    with subprocess.Popen([str(client), "interrupt", str(directory)], stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True) as child:
        lines = queue.Queue()

        def read_markers():
            for _ in range(2):
                lines.put(child.stdout.readline().strip())

        reader = threading.Thread(target=read_markers, daemon=True)
        reader.start()
        try:
            assert lines.get(timeout=10) == "GEN 1 NEXT 1000 SUCCEEDED 0"
            assert lines.get(timeout=10) == "LEASE ACTIVE" and child.poll() is None
            locked = subprocess.run([str(client), "observe", str(directory)], capture_output=True, timeout=10)
            assert locked.returncode == 3 and not locked.stdout and child.poll() is None
        finally:
            if child.poll() is None:
                child.kill()
            child.wait(timeout=10)
            reader.join(timeout=10)
        assert not reader.is_alive() and child.returncode != 0
        return child.returncode


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    client = args.client.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix="xcron-live-checkpoint-") as base:
        directory = Path(base)
        assert command(client, "init", directory).splitlines() == [
            "GEN 0 NEXT 1000 SUCCEEDED 0", "COMMITTED 1"]
        before = wire(directory)

        kill_exit = interrupt(client, directory)
        assert wire(directory) == before
        replay = command(client, "advance", directory).splitlines()
        assert replay == ["GEN 1 NEXT 1000 SUCCEEDED 0", "COMMITTED 2"]
        after = wire(directory)
        assert after[:4096] != before[:4096] and after[4096:] == before[4096:]

        observed = command(client, "observe", directory)
        assert observed == "GEN 2 NEXT 2000 SUCCEEDED 1"
        assert wire(directory) == after
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(dict(
        client_sha256=digest(client.read_bytes()), runner_sha256=digest(Path(__file__).read_bytes()),
        kill_exit=kill_exit, contender_denied=True, before_sha256=digest(before), after_sha256=digest(after),
        replay=replay, observed=observed, fixture_removed=not directory.exists()), indent=2) + "\n", encoding="utf8")
    print("live service replayed uncommitted lease and advanced committed cursor")


if __name__ == "__main__":
    main()
