"""Verify that a reopened APP inbox owns migration and consumption progress."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile


def check(executable, directory, version):
    legacy = directory / "session.journal"
    inbox = directory / "inbox.journal"
    legacy.touch()
    inbox.touch()

    def run(command, *, success=True, stdout=subprocess.PIPE):
        result = subprocess.run(
            [str(executable), command, str(directory), version],
            stdout=stdout,
            stderr=subprocess.PIPE,
            text=True,
            timeout=30,
        )
        assert (result.returncode == 0) == success, result
        return result.stdout

    run("seed")
    source = legacy.read_bytes()
    assert run("migrate") == "Imported 2, pending 2.\n"
    staged = inbox.read_bytes()
    assert run("migrate") == "Imported 0, pending 2.\n"
    assert inbox.read_bytes() == staged
    assert legacy.read_bytes() == source

    # A broken output sink fails the actual APP effect before its confirmation.
    reader, writer = os.pipe()
    os.close(reader)
    try:
        run("consume", success=False, stdout=writer)
    finally:
        os.close(writer)
    assert inbox.read_bytes() == staged

    packet = bytes.fromhex("30050001740061" if version == "5" else "300400017461")
    assert staged.count(packet) == 1
    corrupt = bytearray(staged)
    corrupt[staged.index(packet) + len(packet) - 1] ^= 1
    inbox.write_bytes(corrupt)
    assert run("consume", success=False) == ""
    inbox.write_bytes(staged)

    saved = legacy.with_suffix(".saved")
    legacy.rename(saved)
    size = len(packet)
    assert run("consume") == (
        f"Processing 1/1 ({size} bytes, last byte 61).\n"
        f"Processing 1/3 ({size} bytes, last byte 63).\n"
        "No pending deliveries.\n"
    )
    confirmed = inbox.read_bytes()
    assert confirmed != staged
    assert run("consume") == "No pending deliveries.\n"
    saved.rename(legacy)
    assert run("migrate") == "Imported 0, pending 0.\n"
    assert inbox.read_bytes() == confirmed
    assert legacy.read_bytes() == source

    saved_inbox = inbox.with_suffix(".saved")
    inbox.rename(saved_inbox)
    os.link(legacy, inbox)
    assert run("migrate", success=False) == ""
    assert legacy.read_bytes() == source
    return {
        "version": version,
        "imported_keys": [1, 3],
        "replayed_confirmations": 2,
        "failed_effect_retried": True,
        "corruption_refused_before_effect": True,
        "aliased_files_refused": True,
        "source_sha256": hashlib.sha256(source).hexdigest(),
        "inbox_sha256": hashlib.sha256(confirmed).hexdigest(),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    args = parser.parse_args()
    reports = []
    for version in ("5", "311"):
        with tempfile.TemporaryDirectory(prefix="xrt inbox migration ") as directory:
            reports.append(check(args.executable.resolve(strict=True), Path(directory), version))
    print(json.dumps(reports, indent=2))


if __name__ == "__main__":
    main()
