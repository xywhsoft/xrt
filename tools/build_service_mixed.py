"""Build a service extension mixed-load probe and record its inputs."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys

from release_identity import git_state, tool_version

ROOT = Path(__file__).resolve().parents[1]
PROBES = {
    "xredis": ("redis_mixed.c", ("POOL_SHARED_CONNECT", "TRACKING", "SUBSCRIBER_NOTIFY")),
    "xmqtt": ("mqtt_mixed.c", ("CLIENT", "SERVER")),
}


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def inputs(compile_command: list[str]) -> dict[str, str]:
    dependencies = subprocess.check_output(
        [*compile_command, "-MM", "-MT", "probe"], cwd=ROOT, text=True, encoding="utf-8",
    )
    # The fixed target separates the prerequisites even with Windows drive letters.
    _, listed = dependencies.replace("\\\n", "").split(": ", 1)
    paths = (re.sub(r"\\([ \t#])", r"\1", name).replace("$$", "$")
             for name in re.findall(r"(?:\\[ \t#]|\S)+", listed))
    files = {(ROOT / name).resolve() for name in paths}
    files.add(Path(__file__).resolve())
    files.add(ROOT / "tools/release_identity.py")
    return {path.relative_to(ROOT).as_posix(): digest(path) for path in sorted(files)}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("product", choices=PROBES)
    parser.add_argument("--output", type=Path, required=True, help="new binary path, with .exe on Windows")
    parser.add_argument("--compiler", default="gcc")
    parser.add_argument("--tls", action="store_true", help="build the TLS mixed-load probe")
    args = parser.parse_args()
    windows = sys.platform == "win32"
    if not windows and sys.platform != "linux":
        parser.error("the mixed probes target Linux and Windows")
    output = args.output.absolute()
    if windows and output.suffix.lower() != ".exe":
        parser.error("use an .exe output on Windows")
    receipt = output.with_name(output.name + ".build.json")
    if output.exists() or receipt.exists():
        parser.error("choose a new binary and receipt path")
    compiler = shutil.which(args.compiler)
    if compiler is None:
        parser.error("compiler executable not found")

    stem = args.product.upper()
    folder = f"extlibs/{args.product}"
    source, modules = PROBES[args.product]
    extra_defines = ()
    if args.tls:
        if args.product == "xmqtt":
            modules += ("CLIENT_TLS", "SERVER_TLS")
            extra_defines = ("-DXMQTT_MIXED_TLS", "-DXRT_MODULE_TLS_IDENTITY_RSA")
        else:
            modules += ("TLS",)
    platform_define = "-DWIN32_LEAN_AND_MEAN" if windows else "-D_GNU_SOURCE"
    links = ["-lws2_32", "-liphlpapi", "-lbcrypt", "-ladvapi32"] if windows else ["-pthread", "-lm"]
    compile_command = [
        compiler, "-std=gnu11", "-O2", "-Wall", "-Wextra", "-Werror", platform_define,
        *(f"-D{stem}_MODULE_{stem}_{module}" for module in modules), *extra_defines,
        "-DXRT_IMPLEMENTATION", f"-D{stem}_IMPLEMENTATION", "-Iinclude", f"-I{folder}/include",
        "-include", f"{args.product}/features.h", "-include", "./single/xrt.h",
        "-include", f"./single/extlibs/{args.product}.h", f"{folder}/tests/interop/{source}",
    ]
    fingerprint = inputs(compile_command)
    commit, dirty = git_state(ROOT)
    version = tool_version(compiler, ROOT)
    output.parent.mkdir(parents=True, exist_ok=True)
    command = [*compile_command, "-o", str(output), *links]
    subprocess.run(command, cwd=ROOT, check=True)
    if inputs(compile_command) != fingerprint or git_state(ROOT)[0] != commit:
        raise SystemExit("build inputs changed; discard this binary and use a new output path")
    record = dict(product=args.product, platform=platform.platform(), commit=commit,
                  worktree_dirty=dirty, compiler=version, command=command,
                  inputs=fingerprint, binary_sha256=digest(output))
    if args.product == "xmqtt":
        record["transport"] = "tls13" if args.tls else "tcp"
    with receipt.open("x", encoding="utf-8") as stream:
        json.dump(record, stream, indent=2)
        stream.write("\n")
    print(receipt)


if __name__ == "__main__":
    main()
