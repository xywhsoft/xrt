"""Verify APP-owned cron checkpoint replay across separate processes."""
import argparse
from contextlib import contextmanager
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import tempfile


def run(program, checkpoint, *args, **process_options):
    return subprocess.run(
        [str(program.resolve()), args[0], str(checkpoint), *args[1:]],
        capture_output=True, text=True, timeout=10, **process_options,
    )


@contextmanager
def deny_checkpoint_write(checkpoint):
    if os.name == "nt":
        import ctypes
        from ctypes import wintypes

        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.CreateFileW.argtypes = (
            wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, wintypes.LPVOID,
            wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE,
        )
        kernel.CreateFileW.restype = wintypes.HANDLE
        kernel.CloseHandle.argtypes = (wintypes.HANDLE,)
        kernel.CloseHandle.restype = wintypes.BOOL
        # Share reads and writes while denying the delete access used by replacement.
        access_read, share_read_write, open_existing = 0x80000000, 0x3, 3
        handle = kernel.CreateFileW(str(checkpoint), access_read, share_read_write,
                                    None, open_existing, 0x80, None)
        if handle == wintypes.HANDLE(-1).value:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            yield
        finally:
            if not kernel.CloseHandle(handle):
                raise ctypes.WinError(ctypes.get_last_error())
    else:
        directory = checkpoint.parent
        directory_mode = stat.S_IMODE(directory.stat().st_mode)
        checkpoint_mode = stat.S_IMODE(checkpoint.stat().st_mode)
        try:
            checkpoint.chmod(0o444)
            directory.chmod(0o555)
            yield
        finally:
            directory.chmod(directory_mode)
            checkpoint.chmod(checkpoint_mode)


def verify_checkpoint_failure(program, checkpoint):
    options = {}
    target = program
    if os.name == "posix" and os.geteuid() == 0:
        # A privileged child could bypass the directory permissions.
        target = checkpoint.parent / "unprivileged-catchup"
        shutil.copyfile(program, target)
        target.chmod(0o555)
        options = {"user": 65534, "group": 65534, "extra_groups": []}
    contents = set(checkpoint.parent.iterdir())
    with deny_checkpoint_write(checkpoint):
        denied = run(target, checkpoint, "replay", "4000", "2", **options)
    assert denied.returncode == 1 and denied.stdout.splitlines() == ["ran 2000"], denied
    assert denied.stderr.splitlines() == ["checkpoint write failed after 2000"], denied
    assert checkpoint.read_bytes() == b"1000\n"
    assert set(checkpoint.parent.iterdir()) == contents

    resumed = run(program, checkpoint, "replay", "4000", "2")
    assert resumed.returncode == 0 and resumed.stdout.splitlines() == [
        "ran 2000", "ran 3000", "last=3000 ran=2 pending=1",
    ], resumed
    assert checkpoint.read_bytes() == b"3000\n"


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
        assert run(program, checkpoint, "seed", "1000").returncode == 0
        verify_checkpoint_failure(program, checkpoint)
    return {"persisted_last_ms": 5000, "retried_ms": 3000, "budgeted": True, "clock_rollback": True,
            "rejected_corrupt_checkpoint": True, "checkpoint_failure_retried_ms": 2000}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("program", type=Path)
    parser.add_argument("report", type=Path)
    args = parser.parse_args()
    result = verify(args.program)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print("[pass] cross-process cron catch-up, retry, bounded replay, clock rollback, invalid checkpoint and checkpoint write failure")


if __name__ == "__main__":
    main()
