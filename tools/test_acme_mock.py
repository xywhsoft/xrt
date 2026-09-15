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

import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

from cryptography import x509
from cryptography.hazmat.primitives import serialization
from cryptography.x509.oid import ExtensionOID

TEST_FLOW = ROOT / "out/gcc/native/xacme_tests" / (
	"test_flow.exe" if os.name == "nt" else "test_flow")


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


def verify_grant_pairing(chain_pem: str, key_pem: str, domain: str,
		*ca_pems: str):
	"""独立核验：私钥与叶证书配对、SAN 覆盖、链根属于给定 CA 集合。"""
	certs = x509.load_pem_x509_certificates(chain_pem.encode())
	leaf = certs[0]
	key = serialization.load_pem_private_key(key_pem.encode(), password=None)
	assert leaf.public_key().public_numbers() == \
		key.public_key().public_numbers(), "grant key does not match leaf cert"
	sans = leaf.extensions.get_extension_for_class(
		x509.SubjectAlternativeName).value.get_values_for_type(x509.DNSName)
	assert domain in sans, f"san missing {domain}: {sans}"
	issuer_names = set()
	for ca_pem in ca_pems:
		for ca in x509.load_pem_x509_certificates(ca_pem.encode()):
			issuer_names.add(ca.subject)
	assert leaf.issuer in issuer_names, f"leaf issuer not mock CA"


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

		# 独立核验：Issue 产物配对（备用链用例叶证书由 ALT CA 签发）。
		ca_texts = [
			Path(server.info["ca_pem"]).read_text(encoding="utf-8")]
		alt_pem = Path(server.info["ca_pem"]).parent / "alt.pem"
		if alt_pem.is_file():
			ca_texts.append(alt_pem.read_text(encoding="utf-8"))
		verify_grant_pairing(
			(workdir / "out/pebble_issued.pem").read_text(encoding="utf-8"),
			(workdir / "out/pebble_issued.key.pem").read_text(
				encoding="utf-8"),
			"test.xxrpa.com",
			*ca_texts)
		# 独立核验：store 落盘配对 + 账户持久化。
		store = workdir / "out/store_pebble"
		verify_grant_pairing(
			(store / "certs/test.xxrpa.com/fullchain.pem").read_text(
				encoding="utf-8"),
			(store / "certs/test.xxrpa.com/key.pem").read_text(
				encoding="utf-8"),
			"test.xxrpa.com",
			Path(server.info["ca_pem"]).read_text(encoding="utf-8"))
		# 独立核验：一站式 Obtain store（账户按 CA 隔离持久化）。
		obtain = workdir / "out/store_obtain"
		verify_grant_pairing(
			(obtain / "certs/test.xxrpa.com/fullchain.pem").read_text(
				encoding="utf-8"),
			(obtain / "certs/test.xxrpa.com/key.pem").read_text(
				encoding="utf-8"),
			"test.xxrpa.com",
			Path(server.info["ca_pem"]).read_text(encoding="utf-8"))
		self_accounts = list((obtain / "accounts").glob("*/account.pem"))
		test.assertTrue(self_accounts, "obtain account pem not persisted")
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


@unittest.skipUnless(TEST_FLOW.is_file(), "test_flow not built")
class AcmeMockFlowTests(unittest.TestCase):

	def test_issue_revoke_stored_flow(self):
		run_flow_case(self, [], {"XACME_PREFER_ALT": "1"})

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
		"""25% 请求被掐断时全程仍须成功——重试韧性被真实证明。"""
		workdir = Path(tempfile.mkdtemp(prefix="xacme-mock-"))
		os.makedirs(workdir / "out", exist_ok=True)
		server = MockServer(workdir, ["--flakiness", "0.25"])
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
			print("[flaky] dropped requests survived:", stats["flaky_hits"])
		finally:
			server.stop()


if __name__ == "__main__":
	unittest.main()
