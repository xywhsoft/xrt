"""Build pinned, vendored upstream ACME test services in a new directory.

Requires Git and a Go 1.21+ bootstrap installation on PATH. Go's verified
toolchain download mechanism selects Go 1.26.0; no system packages or CA
certificates are installed. The default builds only the host platform.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess


SOURCE_URL = "https://github.com/letsencrypt/pebble.git"
SOURCE_COMMIT = "1fcb30cabf594e047cb92432b9392bc3dfa469f7"
GO_TOOLCHAIN = "go1.26.0"


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def sources(root: Path) -> dict[str, str]:
    return {p.relative_to(root).as_posix(): digest(p) for p in root.rglob("*")
            if p.is_file() and ".git" not in p.relative_to(root).parts}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path, help="new runtime directory")
    parser.add_argument("--platform", choices=("linux", "windows"), action="append")
    parser.add_argument("--arch", choices=("amd64", "arm64"))
    args = parser.parse_args()
    host = "windows" if os.name == "nt" else "linux"
    if platform.system() not in ("Linux", "Windows"):
        parser.error("the independent client fixture supports Linux and Windows")
    architecture = args.arch or {"AMD64": "amd64", "x86_64": "amd64",
                                 "aarch64": "arm64", "ARM64": "arm64"}.get(platform.machine())
    if architecture is None:
        parser.error("specify a supported --arch")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    source = output / "source"
    report = {"source_url": SOURCE_URL, "source_commit": SOURCE_COMMIT,
              "toolchain": GO_TOOLCHAIN, "architecture": architecture,
              "vendor_build": True, "cgo_enabled": False, "commands": [],
              "all_passed": False, "test_service_only": True,
              "global_system_ca_store_modified": False}
    flags = {"creationflags": subprocess.CREATE_NO_WINDOW} if os.name == "nt" else {}

    def run(label: str, command: list[str], cwd: Path, env: dict | None = None) -> None:
        log = output / (label + ".log")
        with log.open("xb") as stream:
            result = subprocess.run(command, cwd=cwd, env=env, stdout=stream,
                                    stderr=subprocess.STDOUT, timeout=600, **flags)
        report["commands"].append({"command": command, "exit_code": result.returncode,
                                   "log": log.name, "log_sha256": digest(log)})
        print(f"[Pebble build] {label}: exit {result.returncode}", flush=True)
        if result.returncode:
            raise RuntimeError(log.read_text(encoding="utf-8", errors="replace")[-3000:])

    try:
        source.mkdir()
        run("git-init", ["git", "init"], source)
        run("git-remote", ["git", "remote", "add", "origin", SOURCE_URL], source)
        run("git-fetch", ["git", "fetch", "--depth", "1", "origin", SOURCE_COMMIT], source)
        run("git-checkout", ["git", "checkout", "--detach", "FETCH_HEAD"], source)
        commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source,
                                         text=True, **flags).strip()
        if commit != SOURCE_COMMIT:
            raise RuntimeError("upstream checkout differs from the pinned commit")
        before = sources(source)
        env = {**os.environ, "GOTOOLCHAIN": GO_TOOLCHAIN, "CGO_ENABLED": "0", "GOFLAGS": "",
               "GOWORK": "off"}
        report["go_version"] = subprocess.check_output(["go", "version"], env=env,
                                                       text=True, **flags).strip()
        for target in dict.fromkeys(args.platform or [host]):
            selected = {**env, "GOOS": target, "GOARCH": architecture}
            for name in ("pebble", "pebble-challtestsrv"):
                binary = output / "bin" / target / (name + (".exe" if target == "windows" else ""))
                binary.parent.mkdir(parents=True, exist_ok=True)
                run(target + "-" + name, ["go", "build", "-mod=vendor", "-trimpath",
                    "-o", str(binary), "./cmd/" + name], source, selected)
        if sources(source) != before:
            raise RuntimeError("upstream sources changed while building vendored services")
        report.update(all_passed=True, source_sha256=before,
                      source_before_after_verified=True,
                      binary_sha256={p.relative_to(output).as_posix(): digest(p)
                                     for p in (output / "bin").rglob("*") if p.is_file()})
    finally:
        (output / "runtime.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"[verified] pinned independent runtime {SOURCE_COMMIT}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
