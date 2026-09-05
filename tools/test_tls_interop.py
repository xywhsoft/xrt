#!/usr/bin/env python3
"""XRT 与 Python ssl/OpenSSL 的真实 TLS 双向互操作，不访问公网。"""

from __future__ import annotations

import argparse
import ctypes as c
from datetime import datetime, timedelta, timezone
from pathlib import Path
import shutil
import ssl
import subprocess
import sys
import tempfile
import time

from test_protocol_fuzz import _closure

ROOT = Path(__file__).resolve().parents[1]
MODULES = ("tls_client_resume", "tls_server_resume", "tls_identity_builtin",
	"tls_schedule_sha256", "tls_schedule_sha384", "tls_record_aes", "tls_record_chacha",
	"tls_key_exchange_x25519", "tls_key_exchange_p256", "tls_key_exchange_p384")


def build(compiler: str, output: Path) -> None:
	inputs = [_closure(module) for module in MODULES]
	sources, defines, links = [list(dict.fromkeys(value for item in inputs for value in item[i]))
		for i in range(3)]
	command = [compiler, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-shared"]
	if sys.platform != "win32":
		command += ["-fPIC", "-pthread"]
	command += ["-D" + name for name in defines] + ["-I", str(ROOT / "include")]
	command += [str(ROOT / source) for source in sources]
	command += [str(ROOT / "tools/probes/tls_peer.c")]
	command += ["-l" + link for link in links] + ["-o", str(output)]
	subprocess.run(command, check=True, timeout=240)


def fixtures(directory: Path):
	from cryptography import x509
	from cryptography.hazmat.primitives import hashes, serialization as s
	from cryptography.hazmat.primitives.asymmetric import ec, ed25519, rsa
	from cryptography.x509.oid import NameOID, ExtendedKeyUsageOID

	now = datetime.now(timezone.utc)
	root_key = rsa.generate_private_key(65537, 2048)
	root_name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "XRT ephemeral test CA")])
	root = (x509.CertificateBuilder().subject_name(root_name).issuer_name(root_name)
		.public_key(root_key.public_key()).serial_number(x509.random_serial_number())
		.not_valid_before(now - timedelta(days=1)).not_valid_after(now + timedelta(days=2))
		.add_extension(x509.BasicConstraints(ca=True, path_length=0), critical=True)
		.add_extension(x509.KeyUsage(False, False, False, False, False, True, True, False, False), critical=True)
		.add_extension(x509.SubjectKeyIdentifier.from_public_key(root_key.public_key()), critical=False)
		.sign(root_key, hashes.SHA256()))
	root_pem = root.public_bytes(s.Encoding.PEM)
	result = {}
	keys = {"rsa": rsa.generate_private_key(65537, 2048),
		"p256": ec.generate_private_key(ec.SECP256R1()),
		"p384": ec.generate_private_key(ec.SECP384R1()),
		"ed25519": ed25519.Ed25519PrivateKey.generate()}
	for name, key in keys.items():
		leaf_name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "localhost")])
		cert = (x509.CertificateBuilder().subject_name(leaf_name).issuer_name(root_name)
			.public_key(key.public_key()).serial_number(x509.random_serial_number())
			.not_valid_before(now - timedelta(days=1)).not_valid_after(now + timedelta(days=1))
			.add_extension(x509.BasicConstraints(ca=False, path_length=None), critical=True)
			.add_extension(x509.SubjectAlternativeName([x509.DNSName("localhost")]), critical=False)
			.add_extension(x509.ExtendedKeyUsage([ExtendedKeyUsageOID.SERVER_AUTH]), critical=False)
			.add_extension(x509.KeyUsage(True, False, name == "rsa", False, False, False, False, False, False), critical=True)
			.add_extension(x509.AuthorityKeyIdentifier.from_issuer_public_key(root_key.public_key()), critical=False)
			.sign(root_key, hashes.SHA256()))
		cert_path, key_path = directory / (name + ".pem"), directory / (name + ".key")
		cert_path.write_bytes(cert.public_bytes(s.Encoding.PEM))
		key_path.write_bytes(key.private_bytes(s.Encoding.PEM, s.PrivateFormat.PKCS8, s.NoEncryption()))
		result[name] = (cert_path, key_path, cert.public_bytes(s.Encoding.DER),
			key.private_bytes(s.Encoding.DER, s.PrivateFormat.PKCS8, s.NoEncryption()))
	return root.public_bytes(s.Encoding.DER), root_pem, result


def configure(dll):
	dll.xrtTestTlsCreate.argtypes = [c.c_int, c.c_int, c.c_uint16, c.c_uint16, c.c_int,
		c.c_char_p, c.c_void_p, c.c_size_t, c.c_void_p, c.c_size_t, c.c_void_p, c.c_size_t]
	dll.xrtTestTlsCreate.restype = c.c_void_p
	dll.xrtTestTlsStep.argtypes = [c.c_void_p, c.c_int, c.c_void_p, c.c_size_t,
		c.c_void_p, c.POINTER(c.c_size_t), c.c_void_p, c.POINTER(c.c_size_t)]
	dll.xrtTestTlsAlpn.argtypes = [c.c_void_p, c.c_uint16]
	dll.xrtTestTlsAlpn.restype = c.c_bool
	dll.xrtTlsSessionWrite.argtypes = [c.c_void_p, c.c_void_p, c.c_size_t, c.POINTER(c.c_size_t)]
	for name in ("xrtTlsClientKeyUpdate", "xrtTlsServerKeyUpdate"):
		getattr(dll, name).argtypes = [c.c_void_p, c.c_int]
	for name in ("xrtTlsSessionClose", "xrtTlsSessionDestroy"):
		getattr(dll, name).argtypes = [c.c_void_p]
	dll.xrtGetError.restype = c.c_void_p
	dll.xrtErrorMessage.argtypes = [c.c_void_p]
	dll.xrtErrorMessage.restype = c.c_char_p
	dll.xrtErrorCause.argtypes = [c.c_void_p]
	dll.xrtErrorCause.restype = c.c_void_p


def exchange(dll, root, root_pem, fixture, algorithm, version, cipher, server,
	hrr=False, bad_name=False):
	cert_path, key_path, cert_der, key_der = fixture
	signature = {"rsa": 0x0804, "p256": 0x0403, "p384": 0x0503, "ed25519": 0x0807}[algorithm]
	name = b"wrong.invalid" if bad_name else b"localhost"
	peer = dll.xrtTestTlsCreate(server, version, cipher, signature, server and hrr,
		name, cert_der, len(cert_der), key_der, len(key_der), root, len(root))
	assert peer, dll.xrtErrorMessage(dll.xrtGetError())
	context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT if server else ssl.PROTOCOL_TLS_SERVER)
	context.minimum_version = context.maximum_version = (
		ssl.TLSVersion.TLSv1_3 if version == 13 else ssl.TLSVersion.TLSv1_2)
	context.set_alpn_protocols(["xrt-interop"])
	if version == 12:
		context.set_ciphers({0xC02F: "ECDHE-RSA-AES128-GCM-SHA256", 0xC030: "ECDHE-RSA-AES256-GCM-SHA384",
			0xCCA8: "ECDHE-RSA-CHACHA20-POLY1305", 0xC02B: "ECDHE-ECDSA-AES128-GCM-SHA256",
			0xC02C: "ECDHE-ECDSA-AES256-GCM-SHA384", 0xCCA9: "ECDHE-ECDSA-CHACHA20-POLY1305"}[cipher])
	if server:
		context.load_verify_locations(cadata=root_pem.decode())
	else:
		context.load_cert_chain(cert_path, key_path)
		if hrr:
			context.set_ecdh_curve("prime256v1")
	incoming, outgoing = ssl.MemoryBIO(), ssl.MemoryBIO()
	python = context.wrap_bio(incoming, outgoing, server_side=not server,
		server_hostname=name.decode() if server else None)
	output, plain = c.create_string_buffer(262144), c.create_string_buffer(262144)
	wire, python_data, xrt_data = bytearray(), bytearray(), bytearray()
	ready, closed, closing, xrt_state = False, False, False, 0
	deadline = time.monotonic() + 30

	def pump(done):
		nonlocal ready, closed, xrt_state
		for step in range(20000):
			if time.monotonic() > deadline:
				raise TimeoutError("TLS peer stalled")
			if not ready:
				try:
					python.do_handshake()
					ready = True
				except ssl.SSLWantReadError:
					pass
			elif closing and not closed:
				try:
					python.unwrap()
					closed = True
				except ssl.SSLWantReadError:
					pass
			elif not closed:
				try:
					python_data.extend(python.read(65536))
				except ssl.SSLWantReadError:
					pass
			wire.extend(outgoing.read())
			length = min(len(wire), (1, 7, 61, 4096)[step % 4])
			chunk = bytes(wire[:length])
			del wire[:length]
			nout, nplain = c.c_size_t(len(output)), c.c_size_t(len(plain))
			xrt_state = dll.xrtTestTlsStep(peer, server, chunk, len(chunk),
				output, c.byref(nout), plain, c.byref(nplain))
			if xrt_state < 0:
				messages, error = [], dll.xrtGetError()
				while error:
					messages.append(dll.xrtErrorMessage(error).decode())
					error = dll.xrtErrorCause(error)
				raise RuntimeError("; ".join(messages))
			incoming.write(output.raw[:nout.value])
			xrt_data.extend(plain.raw[:nplain.value])
			if done() and not wire and not outgoing.pending and not nout.value:
				return
		raise AssertionError("TLS pump exceeded step budget")

	try:
		pump(lambda: ready and xrt_state == 1)
		assert not bad_name, "wrong-host certificate was accepted"
		assert python.selected_alpn_protocol() == "xrt-interop" and dll.xrtTestTlsAlpn(peer, cipher)
		payload = b"independent TLS peer\x00" * 1000
		for turn in range(2 if version == 13 else 1):
			if turn:
				update = dll.xrtTlsServerKeyUpdate if server else dll.xrtTlsClientKeyUpdate
				assert update(peer, 1) == 0
			written = c.c_size_t()
			assert dll.xrtTlsSessionWrite(peer, payload, len(payload), c.byref(written)) == 0
			assert written.value == len(payload)
			pump(lambda: len(python_data) >= len(payload))
			assert bytes(python_data) == payload
			python_data.clear()
			assert python.write(payload[::-1]) == len(payload)
			pump(lambda: len(xrt_data) >= len(payload))
			assert bytes(xrt_data) == payload[::-1]
			xrt_data.clear()
		assert dll.xrtTlsSessionClose(peer) == 0
		closing = True
		pump(lambda: closed and xrt_state == 2)
	except ssl.SSLCertVerificationError as error:
		if not bad_name or error.verify_code != 62:
			raise
	except RuntimeError as error:
		if not bad_name or not any(word in str(error).lower() for word in ("name", "identity")):
			raise
	finally:
		dll.xrtTlsSessionDestroy(peer)
		dll.xrtClearError()


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--compiler", default="gcc")
	parser.add_argument("--no-build", action="store_true")
	args = parser.parse_args()
	output = ROOT / "out" / "tls-interop"
	output.mkdir(parents=True, exist_ok=True)
	library = output / ("peer.dll" if sys.platform == "win32" else "peer.so")
	if not args.no_build:
		compiler = shutil.which(args.compiler)
		if compiler is None:
			raise SystemExit("C compiler not found: " + args.compiler)
		build(compiler, library)
	dll = c.CDLL(str(library))
	configure(dll)
	print(ssl.OPENSSL_VERSION, flush=True)
	count = 0
	with tempfile.TemporaryDirectory(prefix="certs-", dir=output) as directory:
		root, root_pem, identities = fixtures(Path(directory))
		for algorithm, fixture in identities.items():
			for version in (13, 12):
				if algorithm == "ed25519" and version == 12:
					continue
				ciphers = (0x1301, 0x1302, 0x1303) if version == 13 else (
					(0xC02F, 0xC030, 0xCCA8) if algorithm == "rsa" else (0xC02B, 0xC02C, 0xCCA9))
				for cipher in ciphers:
					for server in (False, True):
						exchange(dll, root, root_pem, fixture, algorithm, version, cipher, server)
						count += 1
				print(f"[PASS] {algorithm} TLS 1.{version - 10}: 3 ciphers, both roles, data/ALPN/close", flush=True)
		for server in (False, True):
			print(f"[test] XRT {'server' if server else 'client'} HRR and wrong-host", flush=True)
			exchange(dll, root, root_pem, identities["rsa"], "rsa", 13, 0x1301, server, hrr=True)
			exchange(dll, root, root_pem, identities["rsa"], "rsa", 13, 0x1301, server, bad_name=True)
			count += 2
		print("[PASS] HRR and wrong-host rejection in both roles", flush=True)
	print(f"[PASS] TLS/OpenSSL interoperability: {count} cases; TLS 1.3 KeyUpdate included")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
