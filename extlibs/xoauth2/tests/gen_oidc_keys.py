#!/usr/bin/env python3
"""生成 OIDC 组合测试夹具：EC P-256 密钥对 + 匹配的 JWKS JSON。

用法：cd tests && python gen_oidc_keys.py（依赖 openssl CLI）
输出 oidc_keys.h，提交后测试可离线运行。
"""
import base64
import json
import subprocess
import tempfile
from pathlib import Path

KEYDIR = Path(tempfile.gettempdir()) / "oidckey"
KEYDIR.mkdir(exist_ok=True)
PRIV = KEYDIR / "oidc_ec.pem"
PUB = KEYDIR / "oidc_ec_pub.pem"


def run(*args):
    r = subprocess.run(args, capture_output=True)
    if r.returncode != 0:
        raise SystemExit(f"命令失败: {args}\n{r.stderr.decode(errors='replace')}")
    return r.stdout


def main():
    run("openssl", "ecparam", "-name", "prime256v1", "-genkey", "-noout",
        "-out", str(PRIV))
    run("openssl", "ec", "-in", str(PRIV), "-pubout", "-out", str(PUB))

    pem = PUB.read_text()
    b64 = "".join(l for l in pem.splitlines() if "-----" not in l)
    der = base64.b64decode(b64)
    bit = der.find(b"\x03")
    point = der[bit + 2:]
    if len(point) > 65:
        point = point[-65:]

    def b64u(b):
        return base64.urlsafe_b64encode(b).rstrip(b"=").decode()

    jwks = {"keys": [{"kty": "EC", "kid": "oidc-ec-1", "use": "sig",
                      "crv": "P-256",
                      "x": b64u(point[1:33]), "y": b64u(point[33:65])}]}

    def cstr(text, name):
        s = (text.replace("\\", "\\\\").replace('"', '\\"')
                 .replace("\n", "\\n").replace("\r", ""))
        return f'static const char {name}[] =\n\t"{s}";'

    out = ["/* 由 gen_oidc_keys.py 生成，勿手改。OIDC 组合测试夹具。 */",
           "#ifndef XOAUTH2_TEST_OIDC_KEYS_H",
           "#define XOAUTH2_TEST_OIDC_KEYS_H",
           "",
           cstr(PRIV.read_text(), "OIDC_EC_PRIV"),
           cstr(PUB.read_text(), "OIDC_EC_PUB"),
           'static const char OIDC_JWKS[] =\n\t"%s";' %
           json.dumps(jwks).replace('"', '\\"'),
           "",
           "#endif", ""]
    Path(__file__).with_name("oidc_keys.h").write_text(
        "\n".join(out), newline="\n")
    print("oidc_keys.h 已生成:", json.dumps(jwks)[:100])


if __name__ == "__main__":
    main()
