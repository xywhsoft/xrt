"""Offline ACME store pruning tests using isolated synthetic directories."""

from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import unittest
from uuid import uuid4

from tools.prune_acme_store import PruneError, prune


ROOT = Path(__file__).resolve().parents[1]
TEST_BASE = ROOT / "out" / "assessment"
DOMAIN = "prune.example.com"


class AcmePruneTests(unittest.TestCase):
    def setUp(self) -> None:
        self.root = TEST_BASE / f"prune-acme-{uuid4().hex}"
        self.directory = self.root / "certs" / DOMAIN
        self.directory.mkdir(parents=True)
        (self.directory / "current.lock").write_bytes(b"")
        (self.directory / "notes.txt").write_text("unrelated\n", encoding="ascii")

    def tearDown(self) -> None:
        target = self.root.resolve(strict=True)
        if not target.is_relative_to(TEST_BASE.resolve(strict=True)):
            raise AssertionError(f"test cleanup escaped assessment directory: {target}")
        shutil.rmtree(target)

    def version(self, suffix: int, *, complete: bool, modified: int) -> Path:
        return self.version_at(self.directory, suffix,
                               complete=complete, modified=modified)

    def version_at(self, directory: Path, suffix: int, *,
                   complete: bool, modified: int) -> Path:
        path = directory / f".grant-{suffix:016x}"
        path.mkdir()
        (path / "fullchain.pem").write_text("chain\n", encoding="ascii")
        if complete:
            (path / "key.pem").write_text("key\n", encoding="ascii")
            (path / "meta.txt").write_text("directory=ca\n", encoding="ascii")
        else:
            (path / ".xacme-key-0123456789abcdef.tmp").write_text(
                "partial key\n", encoding="ascii"
            )
        os.utime(path, (modified, modified))
        return path

    def current(self, path: Path) -> None:
        (self.directory / "current").write_bytes((path.name + "\n").encode("ascii"))

    def test_preview_and_apply_keep_one_previous(self) -> None:
        active = self.version(1, complete=True, modified=100)
        older = self.version(2, complete=True, modified=200)
        newer = self.version(3, complete=True, modified=300)
        partial = self.version(4, complete=False, modified=400)
        self.current(active)

        preview = prune(self.root, DOMAIN, 1, False)
        self.assertEqual({item.name for item in preview.retained},
                         {active.name, newer.name})
        self.assertEqual({item.name for item in preview.removable},
                         {older.name, partial.name})
        self.assertTrue(older.exists() and partial.exists())

        applied = prune(self.root, DOMAIN, 1, True)
        self.assertEqual(len(applied.removable), 2)
        self.assertFalse(older.exists() or partial.exists())
        self.assertTrue(active.exists() and newer.exists())
        self.assertEqual((self.directory / "current").read_text(encoding="ascii"),
                         active.name + "\n")
        self.assertTrue((self.directory / "current.lock").exists())
        self.assertTrue((self.directory / "notes.txt").exists())

    def test_corrupt_pointer_refuses_deletion(self) -> None:
        old = self.version(5, complete=True, modified=100)
        (self.directory / "current").write_bytes(b"../escape\n")
        with self.assertRaises(PruneError):
            prune(self.root, DOMAIN, 0, True)
        self.assertTrue(old.exists())

    def test_cli_preview_requires_apply_to_remove(self) -> None:
        active = self.version(10, complete=True, modified=200)
        old = self.version(11, complete=True, modified=100)
        self.current(active)
        command = [
            sys.executable, str(ROOT / "tools" / "prune_acme_store.py"),
            "--root", str(self.root), "--domain", DOMAIN,
            "--keep-previous", "0",
        ]
        preview = subprocess.run(command, cwd=ROOT, check=True,
                                 text=True, capture_output=True)
        self.assertIn("[would remove]", preview.stdout)
        self.assertTrue(old.exists())
        applied = subprocess.run([*command, "--apply"], cwd=ROOT, check=True,
                                 text=True, capture_output=True)
        self.assertIn("[removed]", applied.stdout)
        self.assertFalse(old.exists())
        self.assertTrue(active.exists())

    def test_unknown_generation_file_refuses_deletion(self) -> None:
        active = self.version(6, complete=True, modified=200)
        old = self.version(7, complete=True, modified=100)
        (old / "unexpected.txt").write_text("keep\n", encoding="ascii")
        self.current(active)
        with self.assertRaises(PruneError):
            prune(self.root, DOMAIN, 0, True)
        self.assertTrue(old.exists())

    def test_active_writer_lock_refuses_pruning(self) -> None:
        active = self.version(12, complete=True, modified=200)
        old = self.version(13, complete=True, modified=100)
        self.current(active)
        signal = self.root / "writer-locked"
        code = """
import os, sys, time
with open(sys.argv[1], 'r+b') as handle:
    if os.name == 'nt':
        import msvcrt
        handle.seek(0)
        msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
    else:
        import fcntl
        fcntl.lockf(handle.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
    with open(sys.argv[2], 'w', encoding='ascii') as ready:
        ready.write('locked')
    time.sleep(20)
"""
        worker = subprocess.Popen(
            [sys.executable, "-c", code,
             str(self.directory / "current.lock"), str(signal)], cwd=ROOT
        )
        try:
            deadline = time.monotonic() + 5
            while not signal.exists() and worker.poll() is None:
                if time.monotonic() >= deadline:
                    self.fail("test writer did not acquire lock")
                time.sleep(0.01)
            self.assertIsNone(worker.poll(), "test writer exited before prune")
            with self.assertRaises(PruneError):
                prune(self.root, DOMAIN, 0, True)
            self.assertTrue(old.exists())
        finally:
            if worker.poll() is None:
                worker.terminate()
            worker.wait(timeout=5)

    def test_wildcard_uses_portable_directory(self) -> None:
        directory = self.root / "certs" / "%2A.wild.example.com"
        directory.mkdir()
        (directory / "current.lock").write_bytes(b"")
        active = self.version_at(directory, 14, complete=True, modified=200)
        old = self.version_at(directory, 15, complete=True, modified=100)
        (directory / "current").write_bytes((active.name + "\n").encode("ascii"))
        prune(self.root, "*.wild.example.com", 0, True)
        self.assertTrue(active.exists())
        self.assertFalse(old.exists())

    @unittest.skipIf(os.name == "nt", "legacy '*' directory is POSIX-only")
    def test_legacy_wildcard_directory_is_readable(self) -> None:
        directory = self.root / "certs" / "*.old.example.com"
        directory.mkdir()
        (directory / "current.lock").write_bytes(b"")
        active = self.version_at(directory, 16, complete=True, modified=200)
        (directory / "current").write_bytes((active.name + "\n").encode("ascii"))
        result = prune(self.root, "*.old.example.com", 0, False)
        self.assertEqual(result.current, active.name)
        mapped = self.root / "certs" / "%2A.old.example.com"
        mapped.mkdir()
        (mapped / "current.lock").write_bytes(b"")
        mapped_active = self.version_at(mapped, 17, complete=True, modified=300)
        (mapped / "current").write_bytes(
            (mapped_active.name + "\n").encode("ascii")
        )
        self.assertEqual(
            prune(self.root, "*.old.example.com", 0, False).current,
            mapped_active.name,
        )
        self.assertEqual(
            prune(self.root, "*.old.example.com", 0, False,
                  legacy_wildcard=True).current,
            active.name,
        )

    @unittest.skipIf(os.name == "nt", "symlink creation may require Windows privilege")
    def test_linked_generation_refuses_deletion(self) -> None:
        active = self.version(8, complete=True, modified=200)
        outside = self.root / "outside"
        outside.mkdir()
        (self.directory / ".grant-0000000000000009").symlink_to(
            outside, target_is_directory=True
        )
        self.current(active)
        with self.assertRaises(PruneError):
            prune(self.root, DOMAIN, 0, True)
        self.assertTrue(outside.exists())


if __name__ == "__main__":
    unittest.main()
