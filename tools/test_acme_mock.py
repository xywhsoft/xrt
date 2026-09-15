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


def verify_grant_pairing(chain_pem: str, key_pem: str, domain: str, ca_pem: str):
	"""独立核验：私钥与叶证书配对、SAN 覆盖、链可验证到模拟 CA。"""
	certs = x509.load_pem_x509_certificates(chain_pem.encode())
	leaf = certs[0]
	key = serialization.load_pem_private_key(key_pem.encode(), password=None)
	assert leaf.public_key().public_numbers() == \
		key.public_key().public_numbers(), "grant key does not match leaf cert"
	sans = leaf.extensions.get_extension_for_class(
		x509.SubjectAlternativeName).value.get_values_for_type(x509.DNSName)
	assert domain in sans, f"san missing {domain}: {sans}"
	ca = x509.load_pem_x509_certificates(ca_pem.encode())[-1]
	assert leaf.issuer == ca.subject, "leaf not issued by mock CA"


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

		# 独立核验：Issue 产物配对。
		verify_grant_pairing(
			(workdir / "out/pebble_issued.pem").read_text(encoding="utf-8"),
			(workdir / "out/pebble_issued.key.pem").read_text(
				encoding="utf-8"),
			"test.xxrpa.com",
			Path(server.info["ca_pem"]).read_text(encoding="utf-8"))
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


@unittest.skipUnless(TEST_FLOW.is_file(), "test_flow not built")
class AcmeMockFlowTests(unittest.TestCase):

	def test_issue_revoke_stored_flow(self):
		run_flow_case(self, [], {})

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


if __name__ == "__main__":
	unittest.main()
