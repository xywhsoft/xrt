"""Build the modular real Sentinel probe, including its mutual TLS paths."""

import argparse
import hashlib
import os
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "tools"))

import build
import release_identity
from xrt_manifest import expand_manifest_paths


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="gcc")
    parser.add_argument("--arch", default="native")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--sanitize", action="store_true", help="enable ASan and UBSan on supported compilers")
    options = parser.parse_args()
    compiler = build._compiler(options.compiler)
    platform = "windows" if os.name == "nt" else "linux" if sys.platform.startswith("linux") else "macos"
    manifests = expand_manifest_paths([ROOT / "extlibs/xredis/config/modules.json"])
    suite = "xredis_sentinel_events,xredis_tls_auth,tls_identity_rsa,pem"
    sources, _, _, _, features, links, _, _, includes, headers = build._load_modules(
        suite, manifests, platform=platform
    )
    cflags = ["-DSPR_TLS", "-O2"]
    ldflags = []
    if options.sanitize:
        cflags.extend(("-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"))
        ldflags.append("-fsanitize=address,undefined")
    for include in includes:
        cflags.extend(("-I", str(ROOT / include)))
    output = options.output.absolute()
    output.parent.mkdir(parents=True, exist_ok=True)
    print(release_identity.tool_version(compiler, ROOT), flush=True)
    objects = build._compile_objects(compiler, options.arch, sources, features,
                                    output.parent / (output.stem + "-obj"), False, cflags, headers, platform)
    probe = ROOT / "extlibs/xredis/tests/interop/redis_sentinel_pool.c"
    build._compile_program(compiler, options.arch, probe.relative_to(ROOT).as_posix(), objects, features,
                           links, output, cflags, ldflags, platform)
    with output.open("rb") as binary:
        print("client_sha256:", hashlib.file_digest(binary, "sha256").hexdigest())


if __name__ == "__main__":
    main()
