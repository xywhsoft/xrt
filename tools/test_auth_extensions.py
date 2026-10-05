"""Build and run standalone JWT/OAuth2 tests and offline examples."""

from __future__ import annotations

import argparse
import os
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CASES = {
    "jwt_minimal": ["extlibs/xjwt/tests/test_minimal.c"],
    "jwt": ["extlibs/xjwt/tests/test_jwt.c"],
    "jwt_time": ["extlibs/xjwt/tests/test_time_bounds.c"],
    "oauth2_minimal": ["extlibs/xoauth2/tests/test_minimal.c"],
    "oauth2": ["extlibs/xoauth2/tests/test_oauth2.c"],
    "oauth2_flow_fault": ["extlibs/xoauth2/tests/test_flow_fault.c"],
    "oauth2_http_fault": ["extlibs/xoauth2/tests/test_http_fault.c"],
    "oidc_compose": ["extlibs/xoauth2/tests/test_oidc_compose.c"],
    "jwt_example": ["extlibs/xjwt/examples/auth_middleware.c"],
    "jwt_example_fault": ["extlibs/xjwt/tests/test_example_fault.c"],
    "oauth2_example_fault": [
        "extlibs/xoauth2/tests/test_api_review_fault.c",
        "extlibs/xoauth2/tests/support/implementation.c", "extlibs/xjwt/tests/support/implementation.c",
    ],
    "oidc_example_fault": [
        "extlibs/xoauth2/tests/test_oidc_example_fault.c",
        "extlibs/xoauth2/tests/support/implementation.c", "extlibs/xjwt/tests/support/implementation.c",
    ],
    "oauth2_example": [
        "extlibs/xoauth2/examples/api_review.c",
        "extlibs/xoauth2/tests/support/implementation.c",
        "extlibs/xjwt/tests/support/implementation.c",
    ],
    "oidc_example": [
        "extlibs/xoauth2/examples/oidc_login.c",
        "extlibs/xoauth2/tests/support/implementation.c",
        "extlibs/xjwt/tests/support/implementation.c",
    ],
    "oidc_live_usage": [
        "extlibs/xoauth2/examples/oidc_live.c",
        "extlibs/xoauth2/tests/support/implementation.c", "extlibs/xjwt/tests/support/implementation.c",
    ],
    "wechat_example": [
        "extlibs/xoauth2/examples/wechat_login.c",
        "extlibs/xoauth2/tests/support/implementation.c",
    ],
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="gcc")
    parser.add_argument("--output-dir", type=Path,
                        help="retain selected test binaries in a separate directory")
    parser.add_argument("--case", action="append", choices=CASES)
    parser.add_argument("--sanitize", action="store_true",
                        help="build selected Linux cases with ASan and UBSan")
    args = parser.parse_args()
    if args.sanitize and os.name == "nt":
        parser.error("--sanitize is supported by the Linux auth job only")

    output = (args.output_dir or ROOT / "out" / "auth_extensions").resolve()
    output.mkdir(parents=True, exist_ok=True)
    selected = args.case or list(CASES)
    for name in selected:
        binary = output / (name + (".exe" if os.name == "nt" else ""))
        command = [
            args.compiler, "-std=c11",
            *([] if os.name == "nt" else ["-D_GNU_SOURCE"]),
            "-O1" if args.sanitize else "-O2",
            "-Wall", "-Wextra", "-Werror",
            *(["-g", "-fsanitize=address,undefined",
               "-fno-omit-frame-pointer"] if args.sanitize else []),
            "-I", str(ROOT / "single"),
            "-I", str(ROOT / "extlibs" / "xjwt" / "include"),
            "-I", str(ROOT / "extlibs" / "xoauth2" / "include"),
            *(str(ROOT / source) for source in CASES[name]),
            "-o", str(binary),
        ]
        if os.name == "nt":
            command.extend(["-lws2_32", "-lbcrypt", "-ladvapi32", "-liphlpapi"])
        else:
            command.extend(["-pthread", "-lm"])
        print(f"[build] {name}", flush=True)
        subprocess.run(command, cwd=ROOT, check=True)
        print(f"[test] {name}", flush=True)
        subprocess.run([str(binary)], cwd=ROOT, check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
