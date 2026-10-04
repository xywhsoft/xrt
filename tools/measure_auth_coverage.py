"""Measure owned JWT/OAuth2 coverage, including OAuth2's independent network/lifecycle probes."""

from __future__ import annotations

import argparse
import json
import math
import os
import subprocess
import sys
from pathlib import Path

from gcov_coverage import CoverageUnion, compiler_gcov, gcov_json, source_hashes

ROOT = Path(__file__).resolve().parents[1]
SOURCES = {
    "jwt": Path("extlibs/xjwt/tests/test_jwt.c"),
    "oauth2": Path("extlibs/xoauth2/tests/test_oauth2.c"),
}


def clear_counters(output: Path, stem: str) -> None:
    # Only generated counters for our known binary; no recursive workspace cleanup.
    for profile in output.glob(stem + "-*.gcda"):
        profile.unlink()


def notes_for(output: Path, stem: str) -> Path:
    notes = list(output.glob(stem + "-*.gcno"))
    if len(notes) != 1:
        raise RuntimeError(f"expected one notes file for {stem}, got {notes}")
    return notes[0]


def coverage_percent(text: str) -> float:
    value = float(text)
    if not math.isfinite(value) or not 0.0 <= value <= 100.0:
        raise argparse.ArgumentTypeError("coverage minimum must be finite and between 0 and 100")
    return value


def measurement_dependencies(name: str, *, include_interop: bool) -> list[Path]:
    library = ROOT / f"extlibs/x{name}"
    dependencies = [*library.glob("*.h"), *(library / "src").rglob("*.h"),
                    *(library / "tests").rglob("*.h"), library / f"x{name}.c",
                    ROOT / "single/xrt.h", ROOT / SOURCES[name],
                    ROOT / "tools/measure_auth_coverage.py", ROOT / "tools/gcov_coverage.py"]
    if name == "oauth2" and include_interop:
        dependencies += [library / "tests" / filename for filename in
                         ("test_tls_interop_client.c", "test_lifecycle_interop_client.c")]
        dependencies += [ROOT / "tools" / filename for filename in
                         ("test_oauth2_tls_interop.py", "http_tls_test_fixture.py")]
    return sorted(set(dependencies))


def measure(name: str, compiler: str, gcov: str | None = None,
            *, include_interop: bool = True) -> tuple[float, float]:
    output = ROOT / "out" / "auth_extensions" / "coverage" / sys.platform / name
    output.mkdir(parents=True, exist_ok=True)
    report_path = output / "coverage.json"
    report_path.unlink(missing_ok=True)
    sources = sorted((ROOT / f"extlibs/x{name}/src").rglob("*.c"))
    union = CoverageUnion(ROOT, sources)
    dependencies = measurement_dependencies(name, include_interop=include_interop)
    dependency_hashes = source_hashes(dependencies)
    gcov = gcov or compiler_gcov(compiler)
    clear_counters(output, name)
    binary = output / (name + (".exe" if os.name == "nt" else ""))
    source = SOURCES[name]
    command = [
        compiler, "-std=c11", "-D_GNU_SOURCE", "-O0", "--coverage",
        "-fprofile-update=atomic", "-Wall", "-Wextra", "-Werror",
        "-I", str(ROOT / "single"),
        "-I", str(ROOT / "extlibs" / "xjwt"),
        "-I", str(ROOT / "extlibs" / "xoauth2"),
        str(ROOT / source), "-o", str(binary),
    ]
    if os.name == "nt":
        command.extend(["-lws2_32", "-lbcrypt", "-ladvapi32", "-liphlpapi"])
    else:
        command.extend(["-pthread", "-lm"])
    print(f"[coverage build] {name} main", flush=True)
    subprocess.run(command, cwd=ROOT, check=True)
    print(f"[coverage test] {name} main", flush=True)
    subprocess.run([str(binary)], cwd=ROOT, check=True)
    union.add("main", gcov_json(notes_for(output, name), gcov))
    if name == "oauth2" and include_interop:
        interop = output / "interop"
        interop.mkdir(exist_ok=True)
        for stem in ("xoauth2_tls_client", "xoauth2_ownership_client"):
            clear_counters(interop, stem)
        print("[coverage test] OAuth2 independent HTTP/TLS and ownership (all groups)", flush=True)
        subprocess.run([sys.executable, str(ROOT / "tools/test_oauth2_tls_interop.py"),
                        "--compiler", compiler, "--coverage-dir", str(interop)],
                       cwd=ROOT, check=True)
        for label, stem in (("http_tls_lifecycle", "xoauth2_tls_client"),
                            ("unpublished_ownership", "xoauth2_ownership_client")):
            union.add(label, gcov_json(notes_for(interop, stem), gcov))
    if source_hashes(dependencies) != dependency_hashes:
        raise RuntimeError("authentication test or runtime input changed during coverage measurement")
    report = union.report()
    report["scope"] = "main_and_independent" if name == "oauth2" and include_interop else "main"
    report["dependency_sha256"] = {Path(path).relative_to(ROOT).as_posix(): digest
                                   for path, digest in dependency_hashes.items()}
    report["compile_flags"] = ["-std=c11", "-D_GNU_SOURCE", "-O0", "--coverage",
                               "-fprofile-update=atomic", "-Wall", "-Wextra", "-Werror"]
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    for path, file in report["files"].items():
        print(f"  {path}: {file['covered_lines']}/{file['lines']} lines "
              f"({file['lines_percent']:.2f}%); "
              f"{file['covered_branches']}/{file['branches']} branch outcomes "
              f"({file['branches_percent']:.2f}%)")
    for profile in report["profiles"]:
        print(f"  {profile['name']} adds {profile['new_lines']} lines / "
              f"{profile['new_branches']} branch outcomes")
    total = report["total"]
    print(f"  {name} union line coverage: {total['lines_percent']:.2f}%")
    print(f"  {name} union branch-outcome coverage: {total['branches_percent']:.2f}%")
    print(f"  exact counts, remaining gaps and source hashes: {report_path}")
    return total["lines_percent"], total["branches_percent"]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="gcc")
    parser.add_argument("--gcov", help="default: companion identified by the compiler")
    parser.add_argument("--library", action="append", choices=SOURCES,
                        help="select libraries; default measures both")
    parser.add_argument("--main-only", action="store_true",
                        help="diagnostic main-test baseline; excludes independent OAuth2 probes")
    parser.add_argument("--min-jwt", type=coverage_percent, default=85.0)
    parser.add_argument("--min-oauth2", type=coverage_percent,
                        help="default: 91 for the union; 80 for --main-only")
    parser.add_argument("--min-jwt-branches", type=coverage_percent, default=67.0)
    parser.add_argument("--min-oauth2-branches", type=coverage_percent,
                        help="default: 75 for the union; 60 for --main-only")
    args = parser.parse_args()
    oauth_lines = args.min_oauth2 if args.min_oauth2 is not None else (80.0 if args.main_only else 91.0)
    oauth_branches = (args.min_oauth2_branches if args.min_oauth2_branches is not None
                      else (60.0 if args.main_only else 75.0))
    limits = {"jwt": (args.min_jwt, args.min_jwt_branches),
              "oauth2": (oauth_lines, oauth_branches)}
    for name in dict.fromkeys(args.library or SOURCES):
        result = measure(name, args.compiler, args.gcov, include_interop=not args.main_only)
        if any(value < limit for value, limit in zip(result, limits[name])):
            print(f"{name} coverage fell below the extension baseline")
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
