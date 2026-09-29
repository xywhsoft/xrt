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
    "oauth2_minimal": ["extlibs/xoauth2/tests/test_minimal.c"],
    "oauth2": ["extlibs/xoauth2/tests/test_oauth2.c"],
    "oidc_compose": ["extlibs/xoauth2/tests/test_oidc_compose.c"],
    "jwt_example": ["extlibs/xjwt/examples/auth_middleware.c"],
    "oauth2_example": [
        "extlibs/xoauth2/examples/api_review.c",
        "extlibs/xoauth2/xoauth2.c",
        "extlibs/xjwt/xjwt.c",
    ],
    "oidc_example": [
        "extlibs/xoauth2/examples/oidc_login.c",
        "extlibs/xoauth2/xoauth2.c",
        "extlibs/xjwt/xjwt.c",
    ],
    "wechat_example": [
        "extlibs/xoauth2/examples/wechat_login.c",
        "extlibs/xoauth2/xoauth2.c",
    ],
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="gcc")
    parser.add_argument("--case", action="append", choices=CASES)
    args = parser.parse_args()

    output = ROOT / "out" / "auth_extensions"
    output.mkdir(parents=True, exist_ok=True)
    selected = args.case or list(CASES)
    for name in selected:
        binary = output / (name + (".exe" if os.name == "nt" else ""))
        command = [
            args.compiler, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
            "-I", str(ROOT / "single"),
            "-I", str(ROOT / "extlibs" / "xjwt"),
            "-I", str(ROOT / "extlibs" / "xoauth2"),
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
