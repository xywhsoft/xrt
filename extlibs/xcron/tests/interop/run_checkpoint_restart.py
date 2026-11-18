"""Exercise an xcron checkpoint through separate processes and a killed lease."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import queue
import subprocess
import tempfile
import threading
import zlib


def synchronize(path):
    with path.open("r+b") as file:
        data = file.read()
        os.fsync(file.fileno())
    assert len(data) == 180 and data[:5] == b"XCCP\x01"
    assert zlib.crc32(data[:176]) == int.from_bytes(data[176:], "little")
    return hashlib.sha256(data).hexdigest()


def run(probe, *args):
    return subprocess.run([str(probe), *map(str, args)], check=True, text=True, capture_output=True,
                          timeout=20).stdout.strip()


def interrupt(probe, checkpoint):
    with subprocess.Popen([str(probe), "active", str(checkpoint)], stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True) as child:
        ready = queue.Queue()
        reader = threading.Thread(target=lambda: ready.put(child.stdout.readline()), daemon=True)
        reader.start()
        try:
            line = ready.get(timeout=10).strip()
            assert line == "NEXT 1000" and child.poll() is None
        finally:
            if child.poll() is None:
                child.kill()
            child.wait(timeout=10)
            reader.join(timeout=10)
        assert not reader.is_alive() and child.returncode != 0
        return line, child.returncode


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    probe = args.client.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix="xcron-restart-") as directory:
        before = Path(directory) / "before.bin"
        after = Path(directory) / "after.bin"
        torn = Path(directory) / "torn.bin"
        run(probe, "save", before)
        original = synchronize(before)
        offered, killed = interrupt(probe, before)
        assert synchronize(before) == original
        replayed = run(probe, "inspect", before)
        assert replayed == offered
        assert run(probe, "advance", before, after) == offered
        committed = synchronize(after)
        assert original != committed
        resumed = run(probe, "inspect", after)
        assert resumed == "NEXT 2000"
        torn.write_bytes(after.read_bytes()[:91])
        failed = subprocess.run([str(probe), "inspect", str(torn)], text=True, capture_output=True, timeout=20)
        assert failed.returncode and not failed.stdout
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(dict(
            client_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),
            runner_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            interrupted=offered, terminated_exit=killed, replayed=replayed, resumed=resumed,
            earlier_checkpoint_sha256=original, advanced_checkpoint_sha256=committed,
            torn_checkpoint_exit=failed.returncode, synchronized_records=2,
        ), indent=2) + "\n", encoding="utf-8")
    print("checkpoint survived process termination and replayed the uncommitted lease")


if __name__ == "__main__":
    main()
