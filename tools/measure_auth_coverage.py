"""Measure extension-owned JWT/OAuth2 line coverage with GCC gcov."""

from __future__ import annotations

import argparse
import os
import re
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCES = {
    "jwt": Path("extlibs/xjwt/tests/test_jwt.c"),
    "oauth2": Path("extlibs/xoauth2/tests/test_oauth2.c"),
}
REPORT = re.compile(
    r"File '([^']+)'\r?\nLines executed:([0-9]+(?:\.[0-9]+)?)% of ([0-9]+)"
)


def measure(name: str, compiler: str) -> float:
    output = ROOT / "out" / "auth_extensions" / "coverage" / name
    output.mkdir(parents=True, exist_ok=True)
    # Repeated local runs must not mix counters from a previous binary.
    for profile in output.glob(f"{name}-*.gcda"):
        profile.unlink()
    binary = output / (name + (".exe" if os.name == "nt" else ""))
    source = SOURCES[name]
    command = [
        compiler, "-std=c11", "-O0", "--coverage", "-Wall", "-Wextra", "-Werror",
        "-I", str(ROOT / "single"),
        "-I", str(ROOT / "extlibs" / "xjwt"),
        "-I", str(ROOT / "extlibs" / "xoauth2"),
        str(ROOT / source), "-o", str(binary),
    ]
    if os.name == "nt":
        command.extend(["-lws2_32", "-lbcrypt", "-ladvapi32", "-liphlpapi"])
    else:
        command.extend(["-pthread", "-lm"])
    print(f"[coverage build] {name}", flush=True)
    subprocess.run(command, cwd=ROOT, check=True)
    print(f"[coverage test] {name}", flush=True)
    subprocess.run([str(binary)], cwd=ROOT, check=True)
    notes = list(output.glob(f"{name}-*.gcno"))
    if len(notes) != 1:
        raise RuntimeError(f"expected one gcov notes file for {name}, got {notes}")
    report = subprocess.run(
        ["gcov", "-n", "-o", str(notes[0]), str(source)],
        cwd=ROOT, check=True, capture_output=True, text=True,
    ).stdout
    prefix = f"extlibs/x{name}/src/" if name == "jwt" else "extlibs/xoauth2/src/"
    files = []
    for path, percent, count in REPORT.findall(report):
        path = path.replace("\\", "/")
        # gcov reports absolute paths when GCC received absolute source paths.
        if prefix in path:
            path = path[path.index(prefix):]
        if path.startswith(prefix) and path.endswith(".c"):
            files.append((path, float(percent), int(count)))
    if not files:
        raise RuntimeError(f"gcov reported no extension source files for {name}")
    for path, percent, count in files:
        print(f"  {path}: {percent:.2f}% of {count} lines")
    total = sum(count for _, _, count in files)
    weighted = sum(percent * count for _, percent, count in files) / total
    print(f"  {name} weighted line coverage: {weighted:.2f}%")
    return weighted


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="gcc")
    parser.add_argument("--min-jwt", type=float, default=85.0)
    parser.add_argument("--min-oauth2", type=float, default=80.0)
    args = parser.parse_args()
    result = {name: measure(name, args.compiler) for name in SOURCES}
    if result["jwt"] < args.min_jwt or result["oauth2"] < args.min_oauth2:
        print("coverage fell below the extension baseline")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
