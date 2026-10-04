"""xacme 离线全链路验证：模拟 CA + test_flow 集成测试编排。

启动 tools/acme_mock_server.py（真实 TLS/JWS/EAB/TXT/签发校验），
驱动 extlibs/xacme 的 test_flow 可执行文件完成：
  公开客户端开户（含 strict EAB/contact 变体）→ dns-01 签发 →
  传播确认（本地 DNS）→ grant 配对 → Revoke → IssueStored 双跑。
随后用 cryptography 独立核验链+私钥配对、SAN、CA 链与 store 落盘。

运行前提：test_flow 已由 build.py 构建
（out/gcc/native/xacme_tests/test_flow[.exe]）。
"""

from __future__ import annotations

import datetime
import io
import json
import os
import subprocess
import sys
import tempfile
import threading
from types import SimpleNamespace
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
	sys.path.insert(0, str(ROOT))

from tools.acme_mock_server import acme_error

from cryptography import x509
from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import serialization
from cryptography.x509.oid import ExtensionOID

TEST_FLOW = Path(os.environ.get("XACME_TEST_FLOW", str(ROOT / "out/gcc/native/xacme_tests" / (
	"test_flow.exe" if os.name == "nt" else "test_flow")))).resolve()


class MockServer:
	def __init__(self, workdir: Path, extra_args: list[str]):
		self.workdir = workdir
		command = [
			sys.executable, str(ROOT / "tools/acme_mock_server.py"),
			"--workdir", str(workdir), *extra_args,
		]
		self.process = subprocess.Popen(
			command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
			text=True, encoding="utf-8")
		line = self.process.stdout.readline()
		self.info = json.loads(line)

	def stop(self):
		self.process.terminate()
		try:
			self.process.wait(timeout=5)
		except subprocess.TimeoutExpired:
			self.process.kill()
			self.process.wait(timeout=5)
		finally:
			if self.process.stdout is not None:
				self.process.stdout.close()
			if self.process.stderr is not None:
				self.process.stderr.close()


def verify_grant_chain(certs: list[x509.Certificate], ca_pems: tuple[str, ...]) -> None:
	"""Verify the supplied ordered chain to an explicitly trusted fixture CA.

	This checks signatures, validity, CA/key usage and path length. It is an
	offline fixture oracle, not a replacement for a general PKIX policy engine.
	"""
	anchors = [ca for pem in ca_pems for ca in x509.load_pem_x509_certificates(pem.encode())]
	assert anchors, "no explicitly trusted CA"
	now = datetime.datetime.now(datetime.timezone.utc)
	def der(cert): return cert.public_bytes(serialization.Encoding.DER)
	def check(path):
		assert 2 <= len(path) <= 16, "invalid certificate chain length"
		assert len({der(cert) for cert in path}) == len(path), "duplicate certificate in chain"
		for cert in path:
			assert cert.not_valid_before_utc <= now <= cert.not_valid_after_utc, "certificate outside validity period"
		try:
			assert not path[0].extensions.get_extension_for_class(x509.BasicConstraints).value.ca, "leaf is a CA"
		except x509.ExtensionNotFound: pass
		for index, issuer in enumerate(path[1:], 1):
			constraints = issuer.extensions.get_extension_for_class(x509.BasicConstraints).value
			assert constraints.ca, "certificate signer is not a CA"
			try:
				assert issuer.extensions.get_extension_for_class(x509.KeyUsage).value.key_cert_sign, "CA cannot sign certificates"
			except x509.ExtensionNotFound: pass
			below = sum(cert.subject != cert.issuer for cert in path[1:index])
			assert constraints.path_length is None or below <= constraints.path_length, "CA path length exceeded"
			path[index - 1].verify_directly_issued_by(issuer)
	last = certs[-1]
	if any(der(last) == der(anchor) for anchor in anchors):
		try: check(certs)
		except (InvalidSignature, ValueError, TypeError, x509.ExtensionNotFound) as exc:
			raise AssertionError("certificate chain is not valid for the trusted CA") from exc
		return
	for anchor in anchors:
		try: check([*certs, anchor])
		except (AssertionError, InvalidSignature, ValueError, TypeError, x509.ExtensionNotFound): continue
		return
	raise AssertionError("certificate chain does not verify to an explicitly trusted CA")


def verify_grant_pairing(chain_pem: str, key_pem: str, domain: str,
		*ca_pems: str):
	"""独立核验：私钥与叶证书配对、SAN 覆盖、链根属于给定 CA 集合。"""
	certs = x509.load_pem_x509_certificates(chain_pem.encode())
	leaf = certs[0]
	key = serialization.load_pem_private_key(key_pem.encode(), password=None)
	assert leaf.public_key().public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo) == \
		key.public_key().public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo), "grant key does not match leaf cert"
	sans = leaf.extensions.get_extension_for_class(
		x509.SubjectAlternativeName).value.get_values_for_type(x509.DNSName)
	assert domain in sans, f"san missing {domain}: {sans}"
	verify_grant_chain(certs, ca_pems)


def read_stored_grant(store: Path, domain: str) -> tuple[str, str]:
	"""Read one published generation, so the integration check matches the API."""
	base = store / "certs" / domain
	pointer = (base / "current").read_text(encoding="ascii")
	assert pointer.endswith("\n") and pointer.count("\n") == 1
	name = pointer[:-1]
	assert len(name) == 23 and name.startswith(".grant-")
	assert all(c in "0123456789abcdef" for c in name[7:])
	generation = base / name
	assert (generation / "meta.txt").is_file()
	return (
		(generation / "fullchain.pem").read_text(encoding="utf-8"),
		(generation / "key.pem").read_text(encoding="utf-8"),
	)


def run_flow_case(test: unittest.TestCase, extra_server: list[str],
		extra_env: dict[str, str]):
	workdir = Path(tempfile.mkdtemp(prefix="xacme-mock-"))
	os.makedirs(workdir / "out", exist_ok=True)
	server = MockServer(workdir, extra_server)
	try:
		env = dict(os.environ)
		env.update({
			"XACME_PEBBLE_URL": server.info["directory"],
			"XACME_PEBBLE_CA": server.info["ca_pem"],
			"XACME_CHALL_URL": "http://127.0.0.1:%d" % server.info["chall"],
			"XACME_PROPAGATE_RESOLVER": "127.0.0.1:%d" % server.info["dns"],
			"XACME_TEST_ROOT": str(workdir / "out"),
		})
		env.update(extra_env)
		result = subprocess.run(
			[str(TEST_FLOW)], env=env, capture_output=True, text=True,
			encoding="utf-8", errors="replace", timeout=180, cwd=str(ROOT))
		test.assertEqual(
			result.returncode, 0,
			"test_flow failed:\n" + result.stdout + result.stderr)
		test.assertIn("[PASS]", result.stdout)

		stats = fetch_stats(server)
		test.assertGreaterEqual(
			stats["rollovers"], 1, "keyChange rollover not exercised")
		test.assertGreaterEqual(
			stats["deactivated"], 1, "account deactivation not exercised")
		if extra_env.get("XACME_PREFER_ALT") == "1":
			test.assertGreater(
				stats["alt_served"], 0, "alternate chain never served")

		# A preferred alternate must verify to its root alone. Accepting both
		# roots here would let a silently retained primary chain pass.
		ca_texts = [
			Path(server.info["ca_pem"]).read_text(encoding="utf-8")]
		alt_pem = Path(server.info["ca_pem"]).parent / "alt.pem"
		if alt_pem.is_file():
			ca_texts.append(alt_pem.read_text(encoding="utf-8"))
		issue_roots = ca_texts[1:] if extra_env.get("XACME_PREFER_ALT") == "1" else ca_texts[:1]
		verify_grant_pairing(
			(workdir / "out/pebble_issued.pem").read_text(encoding="utf-8"),
			(workdir / "out/pebble_issued.key.pem").read_text(
				encoding="utf-8"),
			"test.xxrpa.com",
			*issue_roots)
		# 独立核验：store 落盘配对 + 账户持久化。
		store = workdir / "out/store_pebble"
		verify_grant_pairing(
			*read_stored_grant(store, "test.xxrpa.com"),
			"test.xxrpa.com",
			Path(server.info["ca_pem"]).read_text(encoding="utf-8"))
		# 独立核验：一站式 Obtain store（账户按 CA 隔离持久化）。
		obtain = workdir / "out/store_obtain"
		verify_grant_pairing(
			*read_stored_grant(obtain, "test.xxrpa.com"),
			"test.xxrpa.com",
			Path(server.info["ca_pem"]).read_text(encoding="utf-8"))
		self_accounts = list((obtain / "accounts").glob("*/account.pem"))
		test.assertTrue(self_accounts, "obtain account pem not persisted")
		return stats
	finally:
		server.stop()


def fetch_stats(server: MockServer) -> dict:
	"""拉取 mock 服务端统计（容忍注入期掐断，重试直至成功）。"""
	import ssl
	import time
	import urllib.request
	context = ssl.create_default_context()
	context.check_hostname = False
	context.verify_mode = ssl.CERT_NONE
	url = "https://127.0.0.1:%d/stats" % server.info["acme"]
	for _ in range(30):
		try:
			with urllib.request.urlopen(url, context=context, timeout=5) as r:
				return json.loads(r.read().decode())
		except Exception:
			time.sleep(0.5)
	raise AssertionError("stats endpoint unreachable")


class AcmeProblemResponseTests(unittest.TestCase):

	def test_error_response_supplies_registered_fresh_nonce(self):
		class Handler:
			def __init__(self):
				self.state = SimpleNamespace(lock=threading.RLock(), nonces=set())
				self.headers = {}
				self.wfile = io.BytesIO()
			def send_response(self, status): self.status = status
			def send_header(self, name, value): self.headers[name] = value
			def end_headers(self): pass
		handler = Handler()
		acme_error(handler, 400, "badNonce", "fixture rejection")
		first = handler.headers["Replay-Nonce"]
		self.assertRegex(first, r"^[A-Za-z0-9_-]+$")
		self.assertIn(first, handler.state.nonces)
		self.assertEqual(handler.status, 400)
		self.assertEqual(handler.headers["Content-Type"], "application/problem+json")
		self.assertEqual(json.loads(handler.wfile.getvalue())["type"],
			"urn:ietf:params:acme:error:badNonce")
		self.assertEqual(int(handler.headers["Content-Length"]), len(handler.wfile.getvalue()))
		handler.wfile = io.BytesIO()
		acme_error(handler, 400, "badNonce", "another rejection")
		second = handler.headers["Replay-Nonce"]
		self.assertNotEqual(first, second)
		self.assertIn(second, handler.state.nonces)


@unittest.skipUnless(TEST_FLOW.is_file(), "test_flow not built")
class AcmeMockFlowTests(unittest.TestCase):

	def run_dns_uncertain(self, fail_at, kind):
		workdir = Path(tempfile.mkdtemp(prefix="xacme-mock-"))
		server = MockServer(workdir, [])
		try:
			env = dict(os.environ)
			env.update({
				"XACME_PEBBLE_URL": server.info["directory"],
				"XACME_PEBBLE_CA": server.info["ca_pem"],
				"XACME_CHALL_URL": "http://127.0.0.1:%d" % server.info["chall"],
				"XACME_TEST_DNS_ADD_UNCERTAIN": str(fail_at),
				"XACME_TEST_DNS_UNCERTAIN_KIND": kind,
			})
			result = subprocess.run(
				[str(TEST_FLOW)], env=env, capture_output=True, text=True,
				encoding="utf-8", errors="replace", timeout=40, cwd=str(ROOT))
			self.assertEqual(result.returncode, 0,
				"uncertain DNS add fixture failed:\n" +
				result.stdout + result.stderr)
			self.assertIn("uncertain DNS add not replayed", result.stdout)
		finally:
			server.stop()

	def test_dns_add_uncertain_is_not_replayed(self):
		"""Transport/provider uncertainty and OOM all stop Add replay."""
		for kind in ("http", "provider", "memory"):
			with self.subTest(kind=kind):
				self.run_dns_uncertain(1, kind)

	def test_later_dns_add_uncertain_cleans_prior_txt(self):
		"""The first TXT is removed if a second authorization cannot be added."""
		for kind in ("http", "provider", "memory"):
			with self.subTest(kind=kind):
				self.run_dns_uncertain(2, kind)

	def test_issue_revoke_stored_flow(self):
		for reference in ("absolute", "path", "dot", "network"):
			with self.subTest(reference=reference):
				run_flow_case(self, ["--alternate-reference", reference], {"XACME_PREFER_ALT": "1"})

	def test_duplicate_domains_use_one_order_and_csr_identifier(self):
		run_flow_case(self, [], {"XACME_TEST_DUPLICATES": "1"})

	def test_rollover_response_lost_then_reconciled(self):
		stats = run_flow_case(self,
			["--drop-keychange-response-once"],
			{"XACME_TEST_ROLLOVER_REPEAT": "1"})
		self.assertEqual(stats["keychange_response_drops"], 1)
		self.assertEqual(stats["rollovers"], 1,
			"recovery or repeated call performed another rollover")

	def test_rollover_dropped_before_execution_preserves_old_key(self):
		workdir = Path(tempfile.mkdtemp(prefix="xacme-mock-"))
		server = MockServer(workdir, ["--drop-keychange-before-once"])
		try:
			env = dict(os.environ)
			env.update({
				"XACME_PEBBLE_URL": server.info["directory"],
				"XACME_PEBBLE_CA": server.info["ca_pem"],
				"XACME_CHALL_URL": "http://127.0.0.1:%d" % server.info["chall"],
				"XACME_TEST_ROLLOVER_DROP_BEFORE": "1",
			})
			result = subprocess.run(
				[str(TEST_FLOW)], env=env, capture_output=True, text=True,
				encoding="utf-8", errors="replace", timeout=40, cwd=str(ROOT))
			self.assertEqual(result.returncode, 0,
				"rollover pre-execution drop fixture failed:\n" +
				result.stdout + result.stderr)
			stats = fetch_stats(server)
			self.assertEqual(stats["keychange_before_drops"], 1)
			self.assertEqual(stats["rollovers"], 0)
		finally:
			server.stop()

	def test_rollover_store_failure_reports_partial_success(self):
		stats = run_flow_case(self, [],
			{"XACME_TEST_ROLLOVER_STORE_FAIL": "1"})
		self.assertEqual(stats["rollovers"], 1)

	def test_rollover_badnonce_rebuilds_outer_jws(self):
		stats = run_flow_case(self,
			["--keychange-badnonce-once"], {})
		self.assertEqual(stats["keychange_badnonces"], 1)
		self.assertEqual(stats["rollovers"], 1)

	def test_strict_eab_and_contact(self):
		run_flow_case(
			self,
			["--eab", "mock-kid-1:TEFBRSBpcyBhIHNlY3JldCBtYWM", "--require-contact"],
			{
				"XACME_EAB_KID": "mock-kid-1",
				"XACME_EAB_HMAC": "TEFBRSBpcyBhIHNlY3JldCBtYWM",
				"XACME_CONTACT_EMAIL": "ops@xxrpa.com",
			},
		)

	def test_transport_flakiness_retries(self):
		"""GET 及一次 POST-as-GET 断流，验证只读请求重试。"""
		workdir = Path(tempfile.mkdtemp(prefix="xacme-mock-"))
		os.makedirs(workdir / "out", exist_ok=True)
		server = MockServer(workdir,
			["--flaky-every", "4", "--flaky-get-only",
			 "--drop-authz-once"])
		try:
			env = dict(os.environ)
			env.update({
				"XACME_PEBBLE_URL": server.info["directory"],
				"XACME_PEBBLE_CA": server.info["ca_pem"],
				"XACME_CHALL_URL": "http://127.0.0.1:%d" % server.info["chall"],
				"XACME_PROPAGATE_RESOLVER": "127.0.0.1:%d" % server.info["dns"],
				"XACME_TEST_ROOT": str(workdir / "out"),
			})
			result = subprocess.run(
				[str(TEST_FLOW)], env=env, capture_output=True, text=True,
				encoding="utf-8", errors="replace", timeout=300, cwd=str(ROOT))
			self.assertEqual(
				result.returncode, 0,
				"test_flow failed under flakiness:\n" +
				result.stdout + result.stderr)
			stats = fetch_stats(server)
			self.assertGreater(
				stats["flaky_hits"], 0,
				"flakiness never fired; retry path not exercised")
			self.assertEqual(stats["authz_drops"], 1,
				"POST-as-GET transport retry was not exercised")
			print("[flaky] dropped requests survived:", stats["flaky_hits"])
		finally:
			server.stop()


if __name__ == "__main__":
	unittest.main()
