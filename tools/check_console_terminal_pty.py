"""Run a deterministic POSIX terminal integration test without touching the user's terminal."""
import fcntl
import os
import pty
import select
import struct
import subprocess
import termios
import time
import argparse
from pathlib import Path

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--sanitize", action="store_true", help="enable AddressSanitizer and UndefinedBehaviorSanitizer")
args = parser.parse_args()
output = root / "out" / "console_terminal_pty"
output.parent.mkdir(parents=True, exist_ok=True)
flags = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g"] if args.sanitize else []
subprocess.run(["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-pthread", *flags,
                "-I", str(root / "include"), str(root / "tests/console/test_console_terminal_pty.c"),
                "-o", str(output)], check=True)
master, slave = pty.openpty()
fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 80, 0, 0))
process = subprocess.Popen([str(output)], stdin=slave, stdout=slave, stderr=slave)
os.close(slave)
received = bytearray()

def wait_for(marker):
    deadline = time.monotonic() + 5
    while marker not in received:
        if select.select([master], [], [], .1)[0]:
            try:
                received.extend(os.read(master, 4096))
            except OSError:
                pass
        if marker not in received and (time.monotonic() >= deadline or process.poll() is not None):
            raise RuntimeError(f"terminal test failed before {marker!r}: {received!r}")

try:
    wait_for(b"READY")
    # Split both a UTF-8 scalar and an escape sequence across kernel reads.
    os.write(master, b"\xe4")
    time.sleep(.005)
    os.write(master, b"\xbd\xa0\x1b[")
    time.sleep(.005)
    os.write(master, b"A\x1b[1;5A\x1b[1;3A\x1b\xc3\xa9\x1b[200~a\0b\nZ\x1b[201~\x1b[<0;3;4M")
    wait_for(b"RESIZE")
    fcntl.ioctl(master, termios.TIOCSWINSZ, struct.pack("HHHH", 31, 91, 0, 0))
    while process.poll() is None:
        if select.select([master], [], [], .1)[0]:
            try:
                received.extend(os.read(master, 4096))
            except OSError:
                break
    if process.wait(timeout=5) != 0:
        raise RuntimeError(f"terminal child failed: {received!r}")
    assert b"\x1b[?1049l" in received and b"\x1b[?2004l" in received
    assert b"\x1b[?1006l" in received and b"\x1b[?25h" in received
    print("POSIX PTY: Unicode, fragmented keys, NUL paste, mouse, resize, timeout, exclusive input and mode restoration passed")
finally:
    if process.poll() is None:
        process.kill()
        process.wait()
    os.close(master)
