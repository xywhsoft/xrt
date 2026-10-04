"""Check RFC 5958 RSA/EC fixture wrapping against independent OpenSSL."""

from __future__ import annotations

import ast
import base64
import re
import subprocess
import tempfile
from pathlib import Path


KEYS = Path(__file__).with_name("test_keys.h")


def fixture(name: str) -> str:
    source = KEYS.read_text(encoding="utf-8")
    match = re.search(
        rf"static const char {re.escape(name)}\[\]\s*=\s*(.*?);", source, re.S
    )
    if match is None:
        raise ValueError(f"missing fixture: {name}")
    literals = re.findall(r'"(?:\\.|[^"\\])*"', match.group(1))
    if not literals:
        raise ValueError(f"empty fixture: {name}")
    return "".join(ast.literal_eval(part) for part in literals)


def pem_der(pem: str) -> bytes:
    body = "".join(line for line in pem.splitlines() if not line.startswith("-----"))
    return base64.b64decode(body, validate=True)


def der_len(length: int) -> bytes:
    if length < 128:
        return bytes((length,))
    encoded = length.to_bytes((length.bit_length() + 7) // 8, "big")
    return bytes((0x80 | len(encoded),)) + encoded


def der_tlv(data: bytes, offset: int) -> tuple[int, int, int]:
    if offset + 2 > len(data):
        raise ValueError("truncated DER")
    tag = data[offset]
    length_byte = data[offset + 1]
    if length_byte < 128:
        length, head = length_byte, offset + 2
    else:
        count = length_byte & 0x7F
        if count == 0 or offset + 2 + count > len(data):
            raise ValueError("invalid DER length")
        length = int.from_bytes(data[offset + 2 : offset + 2 + count], "big")
        if length < 128:
            raise ValueError("nonminimal DER length")
        head = offset + 2 + count
    end = head + length
    if end > len(data):
        raise ValueError("DER value exceeds input")
    return tag, head, end


def fields(der: bytes) -> list[tuple[int, bytes, bytes]]:
    tag, start, end = der_tlv(der, 0)
    if tag != 0x30 or end != len(der):
        raise ValueError("expected one DER SEQUENCE")
    result = []
    offset = start
    while offset < end:
        field_tag, content, next_offset = der_tlv(der, offset)
        result.append(
            (field_tag, der[offset:next_offset], der[content:next_offset])
        )
        offset = next_offset
    return result


def wrap_v2(private_der: bytes, public_der: bytes) -> bytes:
    private = fields(private_der)
    public = fields(public_der)
    if len(private) != 3 or private[0] != (2, b"\x02\x01\x00", b"\x00"):
        raise ValueError("expected PKCS#8 v1 fixture")
    if len(public) != 2 or public[1][0] != 3 or public[1][2][:1] != b"\x00":
        raise ValueError("expected SPKI with byte-aligned public key")
    content = b"\x02\x01\x01" + private[1][1] + private[2][1]
    content += b"\x81" + der_len(len(public[1][2])) + public[1][2]
    return b"\x30" + der_len(len(content)) + content


def encode_pem(der: bytes) -> str:
    body = base64.b64encode(der).decode("ascii")
    lines = "\n".join(body[i : i + 64] for i in range(0, len(body), 64))
    return f"-----BEGIN PRIVATE KEY-----\n{lines}\n-----END PRIVATE KEY-----\n"


def check_pair(private_name: str, public_name: str) -> None:
    private_der = pem_der(fixture(private_name))
    public_pem = fixture(public_name)
    public_der = pem_der(public_pem)
    with tempfile.TemporaryDirectory(prefix="xjwt-rfc5958-") as temporary:
        root = Path(temporary)
        private_path = root / "private.pem"
        public_path = root / "public.pem"
        extracted_path = root / "extracted.der"
        message_path = root / "message.bin"
        signature_path = root / "signature.bin"
        private_path.write_text(
            encode_pem(wrap_v2(private_der, public_der)), encoding="ascii"
        )
        public_path.write_text(public_pem, encoding="ascii")
        message_path.write_bytes(b"xjwt RFC 5958 independent interop\n")
        subprocess.run(
            ["openssl", "pkey", "-in", str(private_path), "-pubout", "-outform",
             "DER", "-out", str(extracted_path)],
            check=True, capture_output=True,
        )
        if extracted_path.read_bytes() != public_der:
            raise AssertionError(f"public key mismatch: {private_name}")
        subprocess.run(
            ["openssl", "dgst", "-sha256", "-sign", str(private_path),
             "-out", str(signature_path), str(message_path)],
            check=True, capture_output=True,
        )
        verified = subprocess.run(
            ["openssl", "dgst", "-sha256", "-verify", str(public_path),
             "-signature", str(signature_path), str(message_path)],
            check=True, capture_output=True, text=True,
        )
        if "Verified OK" not in verified.stdout:
            raise AssertionError(f"signature rejected: {private_name}")


if __name__ == "__main__":
    check_pair("K_RSA_PRIV", "K_RSA_PUB")
    check_pair("K_EC_PRIV_PKCS8", "K_EC_PUB")
    print("RFC 5958 RSA/EC OpenSSL interop passed")
