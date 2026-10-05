"""Measure all ACME owned sources across the module suite, mock CA and independent probes."""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

from gcov_coverage import CoverageUnion, compiler_gcov, gcov_json, source_hashes
from measure_auth_coverage import clear_counters, coverage_percent, notes_for


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "extlibs/xacme/config/modules.json"
SUITE = "xacme_tests"


def owned_sources() -> list[Path]:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    sources = sorted({ROOT / source for module in manifest["modules"]
                      for source in module.get("sources", [])
                      if source.startswith("extlibs/xacme/src/") and source.endswith(".c")})
    if set(sources) != set((ROOT / "extlibs/xacme/src").rglob("*.c")):
        raise RuntimeError("ACME manifest does not list every owned C source")
    return sources


def source_note(output: Path, source: Path) -> Path:
    relative = source.relative_to(ROOT).with_suffix("")
    return output / "obj" / ("__".join(relative.parts) + ".gcno")


def embedding_sources(probe_name: str, sources: list[Path]) -> list[Path]:
    """Derive the expected owned scope from C includes independently of gcov."""
    probe = ROOT / "extlibs/xacme/tests/acme" / probe_name
    embedded = {(probe.parent / path).resolve() for path in
                re.findall(r'^\s*#include\s+"([^"\n]+\.c)"',
                           probe.read_text(encoding="utf-8"), re.MULTILINE)}
    selected = sorted(embedded & set(sources))
    if not selected:
        raise RuntimeError(f"ACME probe embeds no owned C sources: {probe_name}")
    return selected


def verify_generated_sources(root: Path) -> None:
    """Do not combine modular counters with a stale single-header runtime."""
    commands = [[sys.executable, str(root / "tools/amalgamate.py"), "--check"],
                [sys.executable, str(root / "tools/amalgamate.py"), "--manifest",
                 "extlibs/xacme/config/modules.json", "--check"]]
    for command in commands:
        subprocess.run(command, cwd=root, check=True)


def measure(args: argparse.Namespace) -> dict:
    compiler_name = Path(args.compiler).stem.lower()
    if compiler_name != "gcc":
        raise ValueError("ACME coverage currently requires --compiler gcc or a path to gcc")
    platform = "windows" if os.name == "nt" else "linux"
    if sys.platform not in {"win32", "linux"}:
        raise ValueError("ACME coverage currently supports Windows and Linux")
    build_output = ROOT / "out" / compiler_name / ("native-" + platform) / SUITE
    output = ROOT / "out/acme/coverage" / sys.platform
    output.mkdir(parents=True, exist_ok=True)
    report_path = output / "coverage.json"
    report_path.unlink(missing_ok=True)
    sources = owned_sources()
    union = CoverageUnion(ROOT, sources)
    dependencies = [MANIFEST, ROOT / "config/modules.json"]
    for directory in ("include", "extlibs/xacme/include", "extlibs/xacme/src/internal"):
        dependencies += sorted((ROOT / directory).rglob("*.h"))
    # The modular suite links Core objects, while standalone probes embed the single header.
    dependencies += sorted(path for path in (ROOT / "src").rglob("*")
                           if path.is_file() and path.suffix in {".c", ".h", ".S", ".inc"})
    dependencies += sorted(path for path in (ROOT / "extlibs/xacme/tests").rglob("*")
                           if path.is_file() and path.suffix in {".c", ".h"})
    # test_dnstxt_fuzz.c embeds this source outside the tests directory.
    dependencies += sorted(path for path in (ROOT / "extlibs/xacme/fuzz").rglob("*")
                           if path.is_file() and path.suffix in {".c", ".h"})
    dependencies += [ROOT / "tools" / name for name in (
        "measure_acme_coverage.py", "gcov_coverage.py", "measure_auth_coverage.py", "build.py",
        "amalgamate.py", "xrt_manifest.py", "xrt_text.py", "generate_features.py",
        "generate_extension_features.py", "test_acme_http_interop.py", "http_tls_test_fixture.py",
        "test_acme_mock.py", "test_acme_grant_verifier.py", "acme_mock_server.py", "test_acme_provider_wire.py")]
    dependencies += [ROOT / "single/xrt.h", ROOT / "single/xrt_decl.h", ROOT / "tests/test.h",
                     ROOT / "single/extlibs/xacme.h", ROOT / "single/extlibs/xacme_decl.h"]
    inputs = source_hashes(dependencies)
    if not args.module_only:
        print("[coverage] verify matching generated Core and ACME runtimes", flush=True)
        verify_generated_sources(ROOT)
    gcov = args.gcov or compiler_gcov(args.compiler)
    # Object profiles are shared by each module-test executable, and intentionally accumulate.
    for directory in (build_output, build_output / "obj"):
        if directory.is_dir():
            for profile in directory.glob("*.gcda"):
                profile.unlink()
    temp_base = (args.temp_root or Path(tempfile.gettempdir())).resolve()
    with tempfile.TemporaryDirectory(prefix="xacme-coverage-store-", dir=temp_base) as temp:
        native = Path(temp).resolve()
        if native.parent != temp_base or not native.name.startswith("xacme-coverage-store-"):
            raise RuntimeError("unexpected temporary store path")
        # POSIX private-key permissions must be measured on a filesystem that implements them.
        if os.name != "nt":
            permission_probe = native / "mode-probe"
            permission_probe.write_bytes(b"probe")
            permission_probe.chmod(0o600)
            if permission_probe.stat().st_mode & 0o777 != 0o600:
                raise RuntimeError("use --temp-root on a native POSIX filesystem (e.g. /tmp in WSL)")
        # Coverage is offline: do not activate live account/CA tests from ambient configuration.
        env = {key: value for key, value in os.environ.items() if not key.startswith("XACME_")}
        env["XACME_TEST_ROOT"] = str(native / "store")
        env["PYTHONUNBUFFERED"] = "1"
        for key in ("TMPDIR", "TEMP", "TMP"):
            env[key] = str(native)
        print("[coverage] build and run complete modular ACME suite", flush=True)
        subprocess.run([sys.executable, str(ROOT / "tools/build.py"), "--compiler", args.compiler,
                        "--target-platform", platform, "--manifest", str(MANIFEST.relative_to(ROOT)),
                        "--suite", SUITE, "--no-single", "--no-examples", "--rebuild",
                        "--cflag=-std=c11", "--cflag=-D_GNU_SOURCE=", "--cflag=-O0", "--cflag=--coverage",
                        "--cflag=-fprofile-update=atomic", "--ldflag=--coverage"],
                       cwd=ROOT, env=env, check=True)
        for source in sources:
            union.add("module_suite:" + source.stem, gcov_json(source_note(build_output, source), gcov),
                      sources=[source])
        main_report = union.report()["total"]
        print(f"[coverage] module baseline: {main_report['lines_percent']:.2f}% lines / "
              f"{main_report['branches_percent']:.2f}% branch outcomes", flush=True)
        if not args.module_only:
            interop = output / "interop"
            interop.mkdir(exist_ok=True)
            for stem in ("acme_http_client", "acme_ownership_client", "acme_provider_body_client", "acme_provider_wire_client"):
                clear_counters(interop, stem)
            print("[coverage] all independent HTTP/TLS, DNS and ownership groups", flush=True)
            subprocess.run([sys.executable, str(ROOT / "tools/test_acme_http_interop.py"),
                            "--compiler", args.compiler, "--coverage-dir", str(interop)],
                           cwd=ROOT, env=env, check=True)
            union.add("http_tls", gcov_json(notes_for(interop, "acme_http_client"), gcov),
                      sources=embedding_sources("test_http_interop_client.c", sources))
            ownership_sources = embedding_sources("test_lifecycle_interop_client.c", sources)
            union.add("ownership_dns_obtain", gcov_json(notes_for(interop, "acme_ownership_client"), gcov),
                      sources=ownership_sources)
            body_sources = embedding_sources("test_provider_body_client.c", sources)
            union.add("provider_bodies", gcov_json(notes_for(interop, "acme_provider_body_client"), gcov),
                      sources=body_sources)
            wire_sources = embedding_sources("test_provider_wire_client.c", sources)
            union.add("provider_wire", gcov_json(notes_for(interop, "acme_provider_wire_client"), gcov),
                      sources=wire_sources)
            flow = build_output / ("test_flow.exe" if os.name == "nt" else "test_flow")
            if not flow.is_file():
                raise RuntimeError("mock CA coverage requires the newly built flow executable")
            env["XACME_TEST_FLOW"] = str(flow)
            print("[coverage] adversarial grant chain oracle", flush=True)
            subprocess.run([sys.executable, "-m", "unittest", "tools.test_acme_grant_verifier", "-v"],
                           cwd=ROOT, env=env, check=True)
            print("[coverage] all registered mock CA scenarios (no skips permitted)", flush=True)
            mock_runner = (
                "import sys,unittest; "
                "suite=unittest.defaultTestLoader.loadTestsFromName('tools.test_acme_mock'); "
                "expected=suite.countTestCases(); "
                "result=unittest.TextTestRunner(verbosity=2).run(suite); "
                "sys.exit(0 if expected>0 and result.wasSuccessful() and result.testsRun==expected and not result.skipped else 1)"
            )
            subprocess.run([sys.executable, "-c", mock_runner], cwd=ROOT, env=env, check=True)
            for source in sources:
                # These counters contain module tests and mock executions; union prevents double credit.
                union.add("module_and_mock:" + source.stem, gcov_json(source_note(build_output, source), gcov),
                          sources=[source])
    if source_hashes(dependencies) != inputs:
        raise RuntimeError("ACME manifest/headers or runtime changed during coverage measurement")
    report = union.report()
    report["measured_source_kind"] = "owned_c_files_only"
    report["regression_minimum_percent"] = {
        "lines": args.min_lines if args.min_lines is not None else (44.0 if args.module_only else 70.0),
        "branches": args.min_branches if args.min_branches is not None else (37.0 if args.module_only else 55.0),
    }
    report["scope"] = "module_only" if args.module_only else "module_mock_independent"
    report["module_baseline"] = main_report
    report["dependency_sha256"] = {Path(path).relative_to(ROOT).as_posix(): digest
                                   for path, digest in inputs.items()}
    report["compile_flags"] = ["-std=c11", "-D_GNU_SOURCE=", "-O0", "--coverage",
                               "-fprofile-update=atomic", "-Wall", "-Wextra", "-Werror"]
    report["independent_compile_flags"] = [flag for flag in report["compile_flags"]
                                           if flag != "-D_GNU_SOURCE="] + ["-D_GNU_SOURCE"]
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    for source, file in report["files"].items():
        print(f"  {source}: {file['covered_lines']}/{file['lines']} lines ({file['lines_percent']:.2f}%); "
              f"{file['covered_branches']}/{file['branches']} branch outcomes ({file['branches_percent']:.2f}%)")
    for profile in report["profiles"]:
        print(f"  {profile['name']} adds {profile['new_lines']} lines / {profile['new_branches']} branch outcomes")
    total = report["total"]
    print(f"[coverage] ACME union: {total['covered_lines']}/{total['lines']} lines "
          f"({total['lines_percent']:.2f}%), {total['covered_branches']}/{total['branches']} "
          f"branch outcomes ({total['branches_percent']:.2f}%)")
    print(f"[coverage] remaining gaps and source hashes: {report_path}")
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="gcc")
    parser.add_argument("--gcov")
    parser.add_argument("--temp-root", type=Path, help="native filesystem for private-key tests")
    parser.add_argument("--module-only", action="store_true", help="diagnostic baseline without mock/independent tests")
    parser.add_argument("--min-lines", type=coverage_percent,
                        help="regression floor: 70 for the union; 44 for --module-only")
    parser.add_argument("--min-branches", type=coverage_percent,
                        help="regression floor: 55 for the union; 37 for --module-only")
    args = parser.parse_args()
    report = measure(args)
    total = report["total"]
    minimums = report["regression_minimum_percent"]
    if total["lines_percent"] < minimums["lines"] or total["branches_percent"] < minimums["branches"]:
        print("ACME coverage fell below the configured baseline")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
