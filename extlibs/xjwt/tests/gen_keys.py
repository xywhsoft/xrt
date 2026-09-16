#!/usr/bin/env python3
"""生成 tests/test_keys.h：openssl 密钥 PEM + JWKS JSON + openssl 签名的互操作令牌。

用法：cd tests && python gen_keys.py
依赖：openssl CLI。输出是自包含的 C 头文件，提交后测试可离线运行。
"""
import base64
import json
import subprocess
import tempfile
from pathlib import Path

KEYDIR = Path(tempfile.gettempdir()) / "jwtkeys"
KEYDIR.mkdir(exist_ok=True)

RSA_PRIV = KEYDIR / "test_rsa.pem"
RSA_PUB = KEYDIR / "test_rsa_pub.pem"
RSA8K_PRIV = KEYDIR / "test_rsa_8192.pem"
RSA8K_PUB = KEYDIR / "test_rsa_8192_pub.pem"
EC_PRIV = KEYDIR / "test_ec.pem"
EC_PUB = KEYDIR / "test_ec_pub.pem"


def run(*args, data=None):
    r = subprocess.run(args, capture_output=True, input=data)
    if r.returncode != 0:
        raise SystemExit(f"命令失败: {args}\n{r.stderr.decode(errors='replace')}")
    return r.stdout


def b64url(b: bytes) -> str:
    return base64.urlsafe_b64encode(b).rstrip(b"=").decode()


def make_keys():
    if not RSA_PRIV.exists():
        run("openssl", "genrsa", "-out", str(RSA_PRIV), "2048")
    if not RSA_PUB.exists():
        run("openssl", "rsa", "-in", str(RSA_PRIV), "-pubout", "-out", str(RSA_PUB))
    if not EC_PRIV.exists():
        run("openssl", "ecparam", "-name", "prime256v1", "-genkey", "-noout",
            "-out", str(EC_PRIV))
    if not EC_PUB.exists():
        run("openssl", "ec", "-in", str(EC_PRIV), "-pubout", "-out", str(EC_PUB))
    # PKCS#1 传统格式 RSA 私钥 / PKCS#8 格式 EC 私钥（覆盖两种封装）
    run("openssl", "rsa", "-in", str(RSA_PRIV), "-traditional",
        "-out", str(KEYDIR / "test_rsa_pkcs1.pem"))
    run("openssl", "pkcs8", "-topk8", "-nocrypt", "-in", str(EC_PRIV),
        "-out", str(KEYDIR / "test_ec_pkcs8.pem"))
    # RSA-8192（xrt 模数上限 1024 字节）——栈缓冲溢出回归
    if not RSA8K_PRIV.exists():
        run("openssl", "genrsa", "-out", str(RSA8K_PRIV), "8192")
    if not RSA8K_PUB.exists():
        run("openssl", "rsa", "-in", str(RSA8K_PRIV), "-pubout",
            "-out", str(RSA8K_PUB))


def der_ints(blob: bytes):
    """极简 DER：跳到内层找 INTEGER 序列（适用于 PKCS#1 RSAPublicKey）。"""
    ints, i = [], 0
    while i < len(blob) and len(ints) < 2:
        if blob[i] == 0x02:  # INTEGER
            n = blob[i + 1]
            if n & 0x80:
                nlen = n & 0x7F
                n = int.from_bytes(blob[i + 2:i + 2 + nlen], "big")
                vstart = i + 2 + nlen
            else:
                vstart = i + 2
            ints.append(blob[vstart:vstart + n])
            i = vstart + n
        else:
            i += 1
    return ints


def parse_rsa_pub():
    pem = RSA_PUB.read_text()
    b64 = "".join(l for l in pem.splitlines() if "-----" not in l)
    der = base64.b64decode(b64)
    # SPKI: SEQ { SEQ{...}, BIT STRING { SEQ { n, e } } }
    # 找 BIT STRING (0x03) 后的 SEQ
    bit = der.find(b"\x03\x82")
    if bit < 0:
        bit = der.find(b"\x03")
    blob = der[bit:]
    seq = blob.find(b"\x30")
    return der_ints(blob[seq:])  # [n, e]


def parse_ec_pub():
    pem = EC_PUB.read_text()
    b64 = "".join(l for l in pem.splitlines() if "-----" not in l)
    der = base64.b64decode(b64)
    bit = der.find(b"\x03")  # BIT STRING
    # 第一字节 unused bits = 0
    point = der[bit + 2:]
    # 65 字节：04 || x || y（可能带长度前缀，截取末 65 字节）
    if len(point) > 65:
        point = point[-65:]
    return point[1:33], point[33:65]


def openssl_token(priv, pub, alg_hash, header, claims):
    signing = b64url(header.encode()) + "." + b64url(claims.encode())
    dataf = KEYDIR / "signing.bin"
    dataf.write_bytes(signing.encode())
    sig = run("openssl", "dgst", alg_hash, "-sign", str(priv), str(dataf))
    return signing + "." + b64url(sig)


def main():
    make_keys()
    n, e = parse_rsa_pub()
    x, y = parse_ec_pub()

    jwks = {
        "keys": [
            {"kty": "RSA", "kid": "rsa-key-1", "use": "sig",
             "n": b64url(n), "e": b64url(e)},
            {"kty": "EC", "kid": "ec-key-1", "use": "sig", "crv": "P-256",
             "x": b64url(x), "y": b64url(y)},
        ]
    }

    # openssl 参考令牌（HS 部分不可用 openssl 签，只有 RS/ES）
    tok_rs = openssl_token(RSA_PRIV, RSA_PUB, "-sha256",
                           '{"alg":"RS256","typ":"JWT","kid":"rsa-key-1"}',
                           '{"sub":"interop-rs","role":"peer","exp":4102444800}')
    tok_es = openssl_token(EC_PRIV, EC_PUB, "-sha256",
                           '{"alg":"ES256","typ":"JWT","kid":"ec-key-1"}',
                           '{"sub":"interop-es","role":"peer","exp":4102444800}')
    tok_idp = openssl_token(RSA_PRIV, RSA_PUB, "-sha256",
                            '{"alg":"RS256","typ":"JWT","kid":"rsa-key-1"}',
                            '{"sub":"idp-alice","iss":"https://idp.example.com",'
                            '"aud":"myapp-web","exp":4102444800}')

    # 审计回归夹具：无 kid 单钥 JWKS、17 密钥超限 JWKS、坏好混合 JWKS
    jwks_nokid = {"keys": [{"kty": "RSA", "use": "sig",
                            "n": b64url(n), "e": b64url(e)}]}
    jwks_17 = {"keys": [
        {"kty": "RSA", "kid": f"k{i}", "use": "sig",
         "n": b64url(bytes([i % 256]) * 32), "e": b64url(b"\x01\x00\x01")}
        for i in range(17)
    ]}
    jwks_mixed = {"keys": [
        {"kty": "RSA", "kid": "broken", "n": b64url(n)},      # 有 n 无 e
        {"kty": "RSA", "kid": "rsa-key-1", "use": "sig",
         "n": b64url(n), "e": b64url(e)},
    ]}

    def cstr(text, name):
        raw = text if isinstance(text, bytes) else text.encode()
        s = raw.decode().replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n").replace("\r", "")
        return f'static const char {name}[] =\n\t"{s}";'

    out = ["/* 由 gen_keys.py 生成，勿手改。openssl 参考密钥 + 互操作令牌。 */",
           "#ifndef XJWT_TEST_KEYS_H",
           "#define XJWT_TEST_KEYS_H",
           "",
           cstr(RSA_PRIV.read_text(), "K_RSA_PRIV"),
           cstr(RSA_PUB.read_text(), "K_RSA_PUB"),
           cstr(EC_PRIV.read_text(), "K_EC_PRIV"),
           cstr(EC_PUB.read_text(), "K_EC_PUB"),
           cstr((KEYDIR / "test_rsa_pkcs1.pem").read_text(), "K_RSA_PRIV_PKCS1"),
           cstr((KEYDIR / "test_ec_pkcs8.pem").read_text(), "K_EC_PRIV_PKCS8"),
           cstr(RSA8K_PRIV.read_text(), "K_RSA8K_PRIV"),
           cstr(RSA8K_PUB.read_text(), "K_RSA8K_PUB"),
           cstr(json.dumps(jwks), "K_JWKS"),
           cstr(json.dumps(jwks_nokid), "K_JWKS_NOKID"),
           cstr(json.dumps(jwks_17), "K_JWKS_17"),
           cstr(json.dumps(jwks_mixed), "K_JWKS_MIXED"),
           cstr(tok_rs, "K_TOKEN_RS256_OPENSSL"),
           cstr(tok_es, "K_TOKEN_ES256_OPENSSL"),
           cstr(tok_idp, "K_TOKEN_IDP_OPENSSL"),
           "",
           "#endif",
           ""]
    Path(__file__).with_name("test_keys.h").write_text("\n".join(out), newline="\n")
    print("test_keys.h 已生成")


if __name__ == "__main__":
    main()
