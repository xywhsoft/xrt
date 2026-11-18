"""Kill owned cron writers and verify the store's recovered on-disk generation."""

import argparse
import hashlib
import json
from pathlib import Path
import queue
import subprocess
import tempfile
import threading
import zlib


def command(client, action, directory, success=True):
    result = subprocess.run([str(client), action, str(directory)], text=True,
                            capture_output=True, timeout=20)
    assert (result.returncode == 0) == success, result
    return result.stdout.strip()


def kill_at(binary, action, directory, marker, contender=None):
    with subprocess.Popen([str(binary), action, str(directory)], stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True) as child:
        lines = queue.Queue()
        reader = threading.Thread(target=lambda: lines.put(child.stdout.readline()), daemon=True)
        reader.start()
        try:
            assert lines.get(timeout=10).strip() == marker and child.poll() is None
            if contender:
                assert command(contender, "show", directory, success=False) == "OPEN 7004"
        finally:
            if child.poll() is None:
                child.kill()
            child.wait(timeout=10)
            reader.join(timeout=10)
        assert not reader.is_alive() and child.returncode != 0
        return child.returncode


def wire(directory):
    image = (directory / "job.xcs").read_bytes()
    assert len(image) == 8192
    return image


def slot(image, index):
    frame = image[index * 4096:index * 4096 + 216]
    crc = int.from_bytes(frame[212:], "little")
    if frame[:8] != b"XCST\x01\x00\x00\x00" or crc != zlib.crc32(frame[:212]):
        return None
    generation = int.from_bytes(frame[8:16], "little")
    assert generation % 2 == index
    assert tuple(int.from_bytes(frame[i:i + 8], "little") for i in (16, 24)) == (1, 1)
    if generation:
        assert frame[32:36] == b"XCCP"
        assert zlib.crc32(frame[32:208]) == int.from_bytes(frame[208:212], "little")
    else:
        assert not any(frame[32:212])
    return generation


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path, required=True)
    parser.add_argument("--fault-probe", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    client = args.client.resolve(strict=True)
    probe = args.fault_probe.resolve(strict=True)
    cases = []
    with tempfile.TemporaryDirectory(prefix="xcron-store-crash-") as base:
        for action, marker, binary, result, expected in (
            ("active", "ACTIVE 1 1000", client, "GEN 1 NEXT 1000 SLOTS 2", (0, 1)),
            ("interrupt-torn", "TORN", probe, "GEN 1 NEXT 1000 SLOTS 1", (None, 1)),
            ("interrupt-flush", "BEFORE FLUSH", probe, "GEN 2 NEXT 2000 SLOTS 2", (2, 1)),
        ):
            root = Path(base) / action
            root.mkdir()
            assert command(client, "init", root) == "GEN 1 NEXT 1000"
            before = wire(root)
            assert (slot(before, 0), slot(before, 1)) == (0, 1)
            exit_code = kill_at(binary, action, root, marker, contender=client)
            after = wire(root)
            assert (slot(after, 0), slot(after, 1)) == expected
            assert after[4096:] == before[4096:]
            observed = command(client, "show", root)
            assert observed == result
            assert wire(root) == after
            assert command(client, "step", root) == ("GEN 3 NEXT 3000" if expected[0]==2 else "GEN 2 NEXT 2000")
            cases.append(dict(action=action, kill_exit=exit_code, observed=observed,
                              before_sha256=digest(before), after_sha256=digest(after),
                              slot_generations=list(expected), contender_denied=True))
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(dict(client_sha256=digest(client.read_bytes()),
        probe_sha256=digest(probe.read_bytes()), runner_sha256=digest(Path(__file__).read_bytes()),
        cases=cases, runtime_material_removed=not Path(base).exists()), indent=2)+"\n", encoding="utf8")
    print("checkpoint store recovered all owned process interruption cases")


if __name__ == "__main__":
    main()
