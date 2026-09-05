#!/usr/bin/env python3
"""对真实随机盲化的 CRT/完整指数路径做 Python 大整数差分；不进入运行库。"""

from __future__ import annotations

import argparse
import ctypes as c
from pathlib import Path
import secrets
import shutil
import subprocess
import sys

from test_protocol_fuzz import _closure

ROOT = Path(__file__).resolve().parents[1]


class Public(c.Structure):
	_fields_ = [("n", c.c_void_p), ("ns", c.c_size_t),
		("e", c.c_void_p), ("es", c.c_size_t)]


class Private(c.Structure):
	_fields_ = [("public", Public)] + [item
		for name in ("d", "p", "q", "dp", "dq", "qi")
		for item in ((name, c.c_void_p), (name + "s", c.c_size_t))]


def private_view(numbers, crt: bool) -> Private:
	key = Private()
	key.backing = []
	values = [numbers.public_numbers.n, numbers.public_numbers.e,
		numbers.d, numbers.p, numbers.q, numbers.dmp1, numbers.dmq1, numbers.iqmp]
	for index, (name, number) in enumerate(zip(("n", "e", "d", "p", "q", "dp", "dq", "qi"), values)):
		if not crt and index >= 3:
			continue
		data = number.to_bytes((number.bit_length() + 7) // 8, "big")
		buffer = c.create_string_buffer(data)
		key.backing.append(buffer)
		target = key.public if index < 2 else key
		setattr(target, name, c.cast(buffer, c.c_void_p).value)
		setattr(target, name + "s", len(data))
	return key


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--compiler", default="gcc")
	parser.add_argument("--sizes", default="1024,2048,3072,4096,8192")
	parser.add_argument("--samples", type=int, default=3)
	args = parser.parse_args()
	try:
		from cryptography.hazmat.primitives.asymmetric import rsa
	except ImportError as error:
		raise SystemExit("RSA differential tests require Python cryptography") from error
	compiler = shutil.which(args.compiler)
	if compiler is None:
		raise SystemExit("C compiler not found: " + args.compiler)
	sources, defines, links, _ = _closure("crypto_rsa_private")
	output = ROOT / "out" / "rsa-blinding"
	output.mkdir(parents=True, exist_ok=True)
	library = output / ("rsa.dll" if sys.platform == "win32" else "rsa.so")
	command = [compiler, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-shared"]
	if sys.platform != "win32":
		command += ["-fPIC", "-pthread"]
	command += ["-D" + name for name in defines] + ["-I", str(ROOT / "include")]
	command += [str(ROOT / source) for source in sources]
	command += ["-l" + link for link in links] + ["-o", str(library)]
	subprocess.run(command, check=True, timeout=180)
	dll = c.CDLL(str(library))
	dll.xrtRsaPrivate.argtypes = [c.POINTER(Private), c.c_void_p, c.c_size_t, c.c_void_p]
	dll.xrtRsaPrivate.restype = c.c_bool
	dll.xrtGetError.restype = c.c_void_p
	dll.xrtErrorMessage.argtypes = [c.c_void_p]
	dll.xrtErrorMessage.restype = c.c_char_p
	count = 0
	for bits in map(int, args.sizes.split(",")):
		for exponent in (3, 65537):
			numbers = rsa.generate_private_key(exponent, bits).private_numbers()
			n = numbers.public_numbers.n
			size = (n.bit_length() + 7) // 8
			keys = [private_view(numbers, crt) for crt in (True, False)]
			values = [0, 1, 2, n - 1, numbers.p, numbers.q]
			values += [secrets.randbelow(n) for _ in range(args.samples)]
			for value in values:
				expected = pow(value, numbers.d, n).to_bytes(size, "big")
				for key in keys:
					# 原位运算同时验证解盲后的复核仍对照原始输入。
					buffer = c.create_string_buffer(value.to_bytes(size, "big"), size)
					if not dll.xrtRsaPrivate(c.byref(key), buffer, size, buffer):
						raise AssertionError(dll.xrtErrorMessage(dll.xrtGetError()))
					assert buffer.raw == expected, (bits, exponent, value, bool(key.p))
					count += 1
			# 不在范围内的输入不发布输出。
			bad = c.create_string_buffer(n.to_bytes(size, "big"), size)
			before = bad.raw
			assert not dll.xrtRsaPrivate(c.byref(keys[0]), bad, size, bad)
			assert bad.raw == before
			dll.xrtClearError()
			print(f"[PASS] RSA {bits} e={exponent}: CRT/full, Python pow, alias, rejection", flush=True)
	print(f"[PASS] RSA blinding differential: {count} private operations")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
