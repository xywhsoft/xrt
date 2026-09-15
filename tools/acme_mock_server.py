#!/usr/bin/env python3
"""本地 ACME 模拟 CA：替代 Pebble 的零依赖离线验证服务端。

能力（全部真实验证，不做"假装通过"）：
  - TLS ACME 端点（自签 CA + 服务器证书，SAN=127.0.0.1）；
  - 目录/nonce（单次使用）/账户/订单/dns-01 授权/finalize/证书下载/吊销；
  - 完整 JWS 校验：ES256 签名、nonce 一次性、url 绑定、kid 归属；
  - strict EAB：HS256 绑定体 HMAC 与内嵌 JWK 一致性校验；
  - dns-01 挑战值 = base64url(sha256(token.thumbprint)) 严格比对；
  - CSR 真实解析并用 cryptography 签发 90 天叶证书（链=叶+CA）；
  - challtestsrv 兼容 HTTP API（set-txt/clear-txt）+ UDP DNS TXT 应答。

用法：
  python tools/acme_mock_server.py --workdir <dir> [--acme P] [--chall P]
      [--dns P] [--eab <kid>:<mac-b64url>] [--require-contact]
就绪后向 stdout 打印一行 JSON：{"acme":..., "chall":..., "dns":...,
"directory":"https://127.0.0.1:<acme>/dir", "ca_pem":"<ca.pem 路径>"}
"""

from __future__ import annotations

import argparse
import base64
import datetime
import hashlib
import json
import os
import random
import secrets
import ssl
import threading
import http.server
import socketserver
import sys

from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.x509.oid import NameOID

HOST = "127.0.0.1"


def b64u_decode(text: str) -> bytes:
	return base64.urlsafe_b64decode(text + "=" * (-len(text) % 4))


def b64u_encode(data: bytes) -> str:
	return base64.urlsafe_b64encode(data).rstrip(b"=").decode()


def jwk_thumbprint(jwk: dict) -> str:
	canonical = json.dumps(
		{"crv": jwk["crv"], "kty": jwk["kty"], "x": jwk["x"], "y": jwk["y"]},
		separators=(",", ":"), sort_keys=True,
	)
	return b64u_encode(hashlib.sha256(canonical.encode()).digest())


def jwk_public_key(jwk: dict) -> ec.EllipticCurvePublicKey:
	return ec.EllipticCurvePublicNumbers(
		int.from_bytes(b64u_decode(jwk["x"]), "big"),
		int.from_bytes(b64u_decode(jwk["y"]), "big"),
		ec.SECP256R1(),
	).public_key()


def raw_to_der_sig(raw: bytes) -> bytes:
	"""JOSE r||s（64 字节）→ DER ECDSA 签名。"""
	r = int.from_bytes(raw[:32], "big")
	s = int.from_bytes(raw[32:], "big")

	def integer(value: int) -> bytes:
		out = value.to_bytes((value.bit_length() + 8) // 8 or 1, "big")
		if len(out) > 1 and out[0] == 0 and out[1] < 0x80:
			out = out[1:]
		return bytes([0x02, len(out)]) + out

	body = integer(r) + integer(s)
	return bytes([0x30, len(body)]) + body


class State:
	"""服务端全部状态（线程锁保护）。"""

	def __init__(self, eab_kid: str, eab_mac: bytes, require_contact: bool):
		# RLock：部分处理器在持锁状态下调用 _respond_json（其内部加
		# nonce 锁），同线程重入必须允许。
		self.lock = threading.RLock()
		self.nonces = set()
		self.accounts = {}          # kid url -> {jwk, contact, thumb}
		self.orders = {}            # url -> order dict（含 authz 与状态）
		self.authzs = {}            # url -> authz dict
		self.challenges = {}        # url -> challenge dict
		self.txt = {}               # fqdn -> [values]
		self.certificates = {}      # cert url -> pem chain
		self.revoked = set()        # 叶证书 SHA-256 指纹
		self.eab_kid = eab_kid
		self.eab_mac = eab_mac
		self.require_contact = require_contact
		self.flakiness = 0.0
		self.flaky_hits = 0
		self.alt_served = 0
		self.rollovers = 0
		self.serial = 0
		self.ca_cert = None
		self.ca_key = None


def acme_error(handler, status: int, typ: str, detail: str) -> None:
	body = json.dumps({"type": typ, "detail": detail}).encode()
	handler.send_response(status)
	handler.send_header("Content-Type", "application/problem+json")
	handler.send_header("Content-Length", str(len(body)))
	handler.end_headers()
	handler.wfile.write(body)


def new_nonce() -> str:
	return secrets.token_urlsafe(16)


class AcmeHandler(http.server.BaseHTTPRequestHandler):
	protocol_version = "HTTP/1.0"
	server_version = "xacme-mock/1"
	state: State = None  # type: ignore[assignment]
	base = ""            # https://127.0.0.1:<port>

	def log_message(self, fmt, *args):
		if os.environ.get("ACME_MOCK_VERBOSE"):
			sys.stderr.write("[acme] " + fmt % args + "\n")

	# ---------- 基础 ----------

	def _respond_json(self, obj, status=200, location=None, extra=None):
		body = json.dumps(obj).encode()
		st = self.state
		nonce = new_nonce()
		with st.lock:
			st.nonces.add(nonce)
		self.send_response(status)
		self.send_header("Content-Type", "application/json")
		self.send_header("Replay-Nonce", nonce)
		if location:
			self.send_header("Location", location)
		self.send_header("Link", '<%s/dir>; rel="index"' % self.base)
		for key, value in (extra or {}).items():
			self.send_header(key, value)
		self.send_header("Content-Length", str(len(body)))
		self.end_headers()
		self.wfile.write(body)

	def _read_body(self) -> bytes:
		length = int(self.headers.get("Content-Length", "0"))
		return self.rfile.read(length) if length else b""

	def _jws_context(self, body: bytes):
		"""解析并完整校验外层 JWS；返回 (protected, payload, account|None)。"""
		st = self.state
		try:
			obj = json.loads(body)
			protected = json.loads(b64u_decode(obj["protected"]))
			payload_text = b64u_decode(obj["payload"]).decode("utf-8", "replace")
			signature = b64u_decode(obj["signature"])
		except Exception:
			raise AcmeAbort(400, "malformed", "jws decode failed")
		if protected.get("alg") != "ES256":
			raise AcmeAbort(400, "malformed", "alg must be ES256")
		if protected.get("url") != self.base + self.path:
			raise AcmeAbort(400, "malformed", "jws url mismatch")
		with st.lock:
			nonce_ok = protected.get("nonce") in st.nonces
			if nonce_ok:
				st.nonces.discard(protected.get("nonce"))
		if not nonce_ok:
			raise AcmeAbort(400, "badNonce", "replay nonce invalid or reused")
		signing_input = (obj["protected"] + "." + obj["payload"]).encode()
		jwk = protected.get("jwk")
		account = None
		if jwk is None:
			kid = protected.get("kid", "")
			with st.lock:
				account = st.accounts.get(kid)
			if account is None:
				raise AcmeAbort(400, "accountDoesNotExist", kid)
			jwk = account["jwk"]
		try:
			jwk_public_key(jwk).verify(
				raw_to_der_sig(signature), signing_input,
				ec.ECDSA(hashes.SHA256()),
			)
		except Exception:
			raise AcmeAbort(400, "malformed", "jws signature invalid")
		return protected, payload_text, account

	# ---------- 路由 ----------

	def _flaky_drop(self) -> bool:
		fl = self.state.flakiness
		if (fl > 0.0) and (random.random() < fl):
			with self.state.lock:
				self.state.flaky_hits += 1
			self.close_connection = True
			return True
		return False

	def do_GET(self):
		if self._flaky_drop():
			return
		if self.path == "/dir":
			self._respond_json({
				"newNonce": self.base + "/nonce",
				"newAccount": self.base + "/acct",
				"newOrder": self.base + "/order",
				"revokeCert": self.base + "/revoke",
				"keyChange": self.base + "/keychange",
			})
			return
		if self.path == "/stats":
			with self.state.lock:
				self._respond_json({
					"flaky_hits": self.state.flaky_hits,
					"accounts": len(self.state.accounts),
					"orders": len(self.state.orders),
					"revoked": len(self.state.revoked),
					"alt_served": self.state.alt_served,
					"rollovers": self.state.rollovers,
				})
			return
		if self.path == "/nonce":
			nonce = new_nonce()
			with self.state.lock:
				self.state.nonces.add(nonce)
			self.send_response(204)
			self.send_header("Replay-Nonce", nonce)
			self.send_header("Content-Length", "0")
			self.end_headers()
			return
		acme_error(self, 404, "malformed", "unknown GET " + self.path)

	def do_POST(self):
		if self._flaky_drop():
			return
		try:
			self._dispatch()
		except AcmeAbort as abort:
			acme_error(self, abort.status, abort.typ, abort.detail)
		except BrokenPipeError:
			pass

	def _dispatch(self):
		st = self.state
		body = self._read_body()
		path = self.path

		if path == "/acct":
			return self._new_account(body)
		if path == "/revoke":
			return self._revoke(body)
		if path == "/keychange":
			return self._key_change(body)
		protected, payload_text, account = self._jws_context(body)
		if path == "/order":
			return self._new_order(payload_text, account)
		if path.startswith("/authz/"):
			return self._get_authz(path)
		if path.startswith("/chal/"):
			return self._trigger_challenge(path)
		if path.startswith("/order-") and "/final" in path:
			return self._finalize(path, payload_text)
		if path.startswith("/order-"):
			return self._get_order(path)
		if path.startswith("/cert-"):
			return self._download_cert(path, account)
		if path.startswith("/certalt-"):
			return self._download_cert_alt(path, account)
		raise AcmeAbort(404, "malformed", "unknown POST " + path)

	# ---------- 账户 ----------

	def _new_account(self, body):
		st = self.state
		protected, payload_text, account = self._jws_context(body)
		jwk = protected["jwk"]
		thumb = jwk_thumbprint(jwk)
		try:
			payload = json.loads(payload_text)
		except Exception:
			raise AcmeAbort(400, "malformed", "payload not json")
		if not payload.get("termsOfServiceAgreed"):
			raise AcmeAbort(400, "userActionRequired", "must agree to terms")
		contact = payload.get("contact", [])
		if st.require_contact and not contact:
			raise AcmeAbort(400, "invalidContact", "contact required")
		with st.lock:
			for kid_url, existing in st.accounts.items():
				if existing["thumb"] == thumb:
					self._respond_json(
						{"status": "valid", "contact": existing["contact"]},
						status=200, location=kid_url)
					return
			# EAB 仅在开户时要求（RFC 8555 §7.3.4）。
			binding = payload.get("externalAccountBinding")
			if st.eab_mac:
				if binding is None:
					raise AcmeAbort(
						400, "externalAccountRequired", "eab missing")
				self._verify_eab(binding, jwk)
			url = self.base + "/acct/%d" % len(st.accounts)
			st.accounts[url] = {
				"jwk": jwk, "contact": contact, "thumb": thumb}
		self._respond_json(
			{"status": "valid", "contact": contact},
			status=201, location=url)

	def _verify_eab(self, binding, outer_jwk):
		st = self.state
		try:
			inner = json.loads(b64u_decode(binding["protected"]))
			inner_payload = b64u_decode(binding["payload"])
			signature = b64u_decode(binding["signature"])
			assert inner["alg"] == "HS256"
			assert inner["kid"] == st.eab_kid
			assert inner["url"] == self.base + "/acct"
			assert json.loads(inner_payload) == outer_jwk
			digest = hmac_sha256(
				st.eab_mac,
				(binding["protected"] + "." + binding["payload"]).encode())
			assert secrets.compare_digest(digest, signature)
		except AcmeAbort:
			raise
		except Exception:
			raise AcmeAbort(400, "unauthorized", "eab binding invalid")

	# ---------- 订单 ----------

	def _new_order(self, payload_text, account):
		st = self.state
		try:
			identifiers = json.loads(payload_text)["identifiers"]
			domains = [item["value"] for item in identifiers]
			assert domains and all(item["type"] == "dns" for item in identifiers)
		except Exception:
			raise AcmeAbort(400, "malformed", "identifiers invalid")
		with st.lock:
			oid = len(st.orders)
			order_url = self.base + "/order-%d" % oid
			authz_urls = []
			for domain in domains:
				aid = len(st.authzs)
				authz_url = self.base + "/authz/%d" % aid
				token = secrets.token_urlsafe(24)
				chal_url = self.base + "/chal/%d" % len(st.challenges)
				st.authzs[authz_url] = {
					"domain": domain, "status": "pending", "token": token,
					"thumb": account["thumb"]}
				st.challenges[chal_url] = {
					"authz": authz_url, "status": "pending",
					"url": chal_url, "token": token}
				authz_urls.append(authz_url)
			st.orders[order_url] = {
				"status": "pending", "domains": domains,
				"authzs": authz_urls, "finalize":
					self.base + "/order-%d/final" % oid}
		self._respond_json({
			"status": "pending", "identifiers": [
				{"type": "dns", "value": d} for d in domains],
			"authorizations": authz_urls,
			"finalize": self.base + "/order-%d/final" % oid,
		}, status=201, location=order_url)

	def _order_json(self, order_url):
		order = self.state.orders[order_url]
		out = {"status": order["status"], "authorizations": order["authzs"],
			"finalize": order["finalize"]}
		if order.get("certificate"):
			out["certificate"] = order["certificate"]
		return out

	def _get_order(self, path):
		with self.state.lock:
			order = self.state.orders.get(self.base + path)
			if order is None:
				raise AcmeAbort(404, "malformed", "no order")
			if order["status"] == "pending" and all(
					self.state.authzs[a]["status"] == "valid"
					for a in order["authzs"]):
				order["status"] = "ready"
			self._respond_json(self._order_json(self.base + path))
			return

	def _get_authz(self, path):
		with self.state.lock:
			authz = self.state.authzs.get(self.base + path)
			if authz is None:
				raise AcmeAbort(404, "malformed", "no authz")
			chal = [c for c in self.state.challenges.values()
				if c["authz"] == self.base + path][0]
			self._respond_json({
				"status": authz["status"],
				"identifier": {"type": "dns", "value": authz["domain"]},
				"challenges": [{
					"type": "dns-01", "status": chal["status"],
					"url": chal["url"], "token": chal["token"]}],
			})

	def _trigger_challenge(self, path):
		st = self.state
		with st.lock:
			chal = st.challenges.get(self.base + path)
			if chal is None:
				raise AcmeAbort(404, "malformed", "no challenge")
			authz = st.authzs[chal["authz"]]
			fqdn = "_acme-challenge." + authz["domain"]
			expected = b64u_encode(hashlib.sha256(
				(authz["token"] + "." + authz["thumb"]).encode()).digest())
			visible = expected in st.txt.get(fqdn, [])
			chal["status"] = "valid" if visible else "invalid"
			authz["status"] = chal["status"]
			detail = "" if visible else "txt %s not visible" % fqdn
			self._respond_json({
				"type": "dns-01", "status": chal["status"],
				"url": chal["url"], "token": authz["token"],
				"error": None if visible else {
					"type": "urn:ietf:params:acme:error:dns", "detail": detail},
			})

	def _finalize(self, path, payload_text):
		raise AcmeAbort(500, "serverInternal", "not installed")

	def _key_change(self, body):
		"""RFC 8555 §7.3.5：外层新钥 JWS（嵌新 JWK），内层旧钥签名。"""
		st = self.state
		try:
			obj = json.loads(body)
			outer = json.loads(b64u_decode(obj["protected"]))
			inner_obj = json.loads(b64u_decode(obj["payload"]))
			inner = json.loads(b64u_decode(inner_obj["protected"]))
			inner_payload = json.loads(
				b64u_decode(inner_obj["payload"]))
		except Exception:
			raise AcmeAbort(400, "malformed", "keyChange decode failed")
		if outer.get("alg") != "ES256" or "jwk" not in outer:
			raise AcmeAbort(400, "malformed", "outer must embed new JWK")
		if outer.get("url") != self.base + "/keychange":
			raise AcmeAbort(400, "malformed", "outer url mismatch")
		with st.lock:
			nonce_ok = outer.get("nonce") in st.nonces
			if nonce_ok:
				st.nonces.discard(outer.get("nonce"))
		if not nonce_ok:
			raise AcmeAbort(400, "badNonce", "outer nonce invalid")
		new_jwk = outer["jwk"]
		# 外层签名用新 JWK 验证。
		try:
			jwk_public_key(new_jwk).verify(
				raw_to_der_sig(b64u_decode(obj["signature"])),
				(obj["protected"] + "." + obj["payload"]).encode(),
				ec.ECDSA(hashes.SHA256()),
			)
		except Exception:
			raise AcmeAbort(400, "malformed", "outer signature invalid")
		# 内层：旧账户钥签名，kid 归属，oldKey 与现钥一致。
		with st.lock:
			account = st.accounts.get(inner.get("kid", ""))
		if account is None:
			raise AcmeAbort(400, "accountDoesNotExist", "inner kid")
		if inner.get("url") != self.base + "/keychange":
			raise AcmeAbort(400, "malformed", "inner url mismatch")
		try:
			jwk_public_key(account["jwk"]).verify(
				raw_to_der_sig(b64u_decode(inner_obj["signature"])),
				(inner_obj["protected"] + "." +
					inner_obj["payload"]).encode(),
				ec.ECDSA(hashes.SHA256()),
			)
		except Exception:
			raise AcmeAbort(400, "malformed", "inner signature invalid")
		if inner_payload.get("account") != inner.get("kid"):
			raise AcmeAbort(400, "malformed", "account mismatch")
		if inner_payload.get("oldKey") != account["jwk"]:
			raise AcmeAbort(400, "malformed", "oldKey mismatch")
		with st.lock:
			account["jwk"] = new_jwk
			account["thumb"] = jwk_thumbprint(new_jwk)
			st.rollovers += 1
		self._respond_json({})

	# ---------- 证书 ----------

	def _revoke(self, body):
		st = self.state
		protected, payload_text, account = self._jws_context(body)
		try:
			der = b64u_decode(json.loads(payload_text)["certificate"])
			cert = x509.load_der_x509_certificate(der)
		except Exception:
			raise AcmeAbort(400, "malformed", "certificate decode failed")
		fingerprint = cert.fingerprint(hashes.SHA256())
		with st.lock:
			already = fingerprint in st.revoked
			st.revoked.add(fingerprint)
		self._respond_json({})

	def _download_cert(self, path, account):
		with self.state.lock:
			chain = self.state.certificates.get(self.base + path)
		if chain is None:
			raise AcmeAbort(404, "malformed", "no certificate")
		nonce = new_nonce()
		with self.state.lock:
			self.state.nonces.add(nonce)
		alt_url = self.base + path.replace("/cert-", "/certalt-")
		self.send_response(200)
		self.send_header("Content-Type", "application/pem-certificate-chain")
		self.send_header("Replay-Nonce", nonce)
		self.send_header(
			"Link", '<%s>; rel="alternate"' % alt_url)
		self.send_header("Content-Length", str(len(chain.encode())))
		self.end_headers()
		self.wfile.write(chain.encode())

	def _download_cert_alt(self, path, account):
		with self.state.lock:
			chain = self.state.certificates.get(
				self.base + path.replace("/certalt-", "/cert-"))
			if chain is None:
				raise AcmeAbort(404, "malformed", "no alt certificate")
			self.state.alt_served += 1
		nonce = new_nonce()
		with self.state.lock:
			self.state.nonces.add(nonce)
		self.send_response(200)
		self.send_header("Content-Type", "application/pem-certificate-chain")
		self.send_header("Replay-Nonce", nonce)
		self.send_header("Content-Length", str(len(chain.encode())))
		self.end_headers()
		self.wfile.write(chain.encode())


class AcmeAbort(Exception):
	def __init__(self, status, typ, detail):
		super().__init__(detail)
		self.status = status
		self.typ = typ
		self.detail = detail


def hmac_sha256(key: bytes, data: bytes) -> bytes:
	import hmac
	return hmac.new(key, data, hashlib.sha256).digest()


class ChallHandler(http.server.BaseHTTPRequestHandler):
	"""challtestsrv 兼容：POST /set-txt 与 /clear-txt。"""
	protocol_version = "HTTP/1.0"
	state: State = None  # type: ignore[assignment]

	def log_message(self, fmt, *args):
		if os.environ.get("ACME_MOCK_VERBOSE"):
			sys.stderr.write("[chall] " + fmt % args + "\n")

	def do_POST(self):
		length = int(self.headers.get("Content-Length", "0"))
		try:
			obj = json.loads(self.rfile.read(length) or b"{}")
			host, value = obj["host"], obj.get("value", "")
		except Exception:
			self.send_response(400)
			self.send_header("Content-Length", "0")
			self.end_headers()
			return
		with self.state.lock:
			if self.path == "/set-txt":
				self.state.txt.setdefault(host, []).append(value)
			elif self.path == "/clear-txt":
				self.state.txt[host] = [
					v for v in self.state.txt.get(host, []) if v != value]
				if not self.state.txt[host]:
					self.state.txt.pop(host, None)
			else:
				self.send_response(404)
				self.send_header("Content-Length", "0")
				self.end_headers()
				return
		body = b"{}"
		self.send_response(200)
		self.send_header("Content-Length", str(len(body)))
		self.end_headers()
		self.wfile.write(body)


class DnsHandler(socketserver.BaseRequestHandler):
	"""极简 UDP DNS：仅 TXT 查询，从 chall 记录仓应答。"""
	state: State = None  # type: ignore[assignment]

	def handle(self):
		data, sock = self.request[0], self.request[1]
		if len(data) < 12:
			return
		qend = 12
		while qend < len(data) and data[qend] != 0:
			qend += 1 + data[qend]
		qend += 5
		try:
			labels = []
			pos = 12
			while data[pos] != 0:
				labels.append(data[pos + 1:pos + 1 + data[pos]].decode())
				pos += 1 + data[pos]
			name = ".".join(labels)
		except Exception:
			return
		with self.state.lock:
			values = list(self.state.txt.get(name, []))
		answer = b"".join(
			b"\xc0\x0c\x00\x10\x00\x01\x00\x00\x00\x3c\x00\x00" +
			len(v).to_bytes(1, "big") + v.encode()
			for v in values)
		header = data[:2] + b"\x84\x00" + data[4:6] + \
			len(values).to_bytes(2, "big") + b"\x00\x00\x00\x00"
		sock.sendto(header + data[12:qend] + answer, self.client_address)


def generate_ca(workdir: str):
	key = ec.generate_private_key(ec.SECP256R1())
	name = x509.Name([
		x509.NameAttribute(NameOID.COMMON_NAME, "xacme mock CA")])
	now = datetime.datetime.now(datetime.timezone.utc)
	cert = (x509.CertificateBuilder()
		.subject_name(name).issuer_name(name)
		.public_key(key.public_key()).serial_number(x509.random_serial_number())
		.not_valid_before(now - datetime.timedelta(days=1))
		.not_valid_after(now + datetime.timedelta(days=3650))
		.add_extension(x509.BasicConstraints(ca=True, path_length=0), True)
		.sign(key, hashes.SHA256()))
	key_path = os.path.join(workdir, "ca-key.pem")
	cert_path = os.path.join(workdir, "ca.pem")
	with open(key_path, "wb") as f:
		f.write(key.private_bytes(
			serialization.Encoding.PEM,
			serialization.PrivateFormat.PKCS8,
			serialization.NoEncryption()))
	with open(cert_path, "wb") as f:
		f.write(cert.public_bytes(serialization.Encoding.PEM))
	return key, cert, cert_path


def generate_tls_cert(workdir, ca_key, ca_cert):
	key = ec.generate_private_key(ec.SECP256R1())
	name = x509.Name([
		x509.NameAttribute(NameOID.COMMON_NAME, "127.0.0.1")])
	now = datetime.datetime.now(datetime.timezone.utc)
	cert = (x509.CertificateBuilder()
		.subject_name(name).issuer_name(ca_cert.subject)
		.public_key(key.public_key()).serial_number(x509.random_serial_number())
		.not_valid_before(now - datetime.timedelta(days=1))
		.not_valid_after(now + datetime.timedelta(days=365))
		.add_extension(
			x509.SubjectAlternativeName([x509.IPAddress(ipaddress_module())]),
			True)
		.sign(ca_key, hashes.SHA256()))
	path = os.path.join(workdir, "server.pem")
	with open(path, "wb") as f:
		f.write(cert.public_bytes(serialization.Encoding.PEM))
		f.write(ca_cert.public_bytes(serialization.Encoding.PEM))
	keypath = os.path.join(workdir, "server-key.pem")
	with open(keypath, "wb") as f:
		f.write(key.private_bytes(
			serialization.Encoding.PEM,
			serialization.PrivateFormat.PKCS8,
			serialization.NoEncryption()))
	return path, keypath


def ipaddress_module():
	import ipaddress
	return ipaddress.IPv4Address("127.0.0.1")


def main() -> int:
	parser = argparse.ArgumentParser()
	parser.add_argument("--workdir", required=True)
	parser.add_argument("--acme", type=int, default=0)
	parser.add_argument("--chall", type=int, default=0)
	parser.add_argument("--dns", type=int, default=0)
	parser.add_argument("--eab", default="")
	parser.add_argument("--flakiness", type=float, default=0.0)
	parser.add_argument("--require-contact", action="store_true")
	args = parser.parse_args()

	os.makedirs(args.workdir, exist_ok=True)
	eab_kid, eab_mac = "", b""
	if args.eab:
		eab_kid, mac_text = args.eab.split(":", 1)
		eab_mac = b64u_decode(mac_text)
	state = State(eab_kid, eab_mac, args.require_contact)
	state.flakiness = max(0.0, min(0.9, args.flakiness))
	state.ca_key, state.ca_cert, ca_path = generate_ca(args.workdir)

	acme_server = http.server.ThreadingHTTPServer((HOST, args.acme), AcmeHandler)
	acme_port = acme_server.server_address[1]
	chall_server = http.server.ThreadingHTTPServer((HOST, args.chall), ChallHandler)
	chall_port = chall_server.server_address[1]
	dns_server = socketserver.UDPServer((HOST, args.dns), DnsHandler)
	dns_port = dns_server.server_address[1]

	base = "https://%s:%d" % (HOST, acme_port)
	AcmeHandler.state = state
	AcmeHandler.base = base
	ChallHandler.state = state
	DnsHandler.state = state

	cert_path, key_path = generate_tls_cert(
		args.workdir, state.ca_key, state.ca_cert)
	context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
	context.load_cert_chain(cert_path, key_path)
	acme_server.socket = context.wrap_socket(acme_server.socket, server_side=True)

	# finalize 处理器：_dispatch 已完成 JWS 校验，这里只处理已解析载荷。
	def finalize_impl(handler, path, payload_text):
		st = state
		with st.lock:
			order = st.orders.get(base + path.replace("/final", ""))
			if order is None or order["status"] not in ("ready", "processing"):
				raise AcmeAbort(400, "orderNotReady", "order not ready")
			try:
				csr = x509.load_der_x509_csr(
					b64u_decode(json.loads(payload_text)["csr"]))
			except Exception:
				raise AcmeAbort(400, "malformed", "csr decode failed")
			sans = csr.extensions.get_extension_for_class(
				x509.SubjectAlternativeName).value.get_values_for_type(
				x509.DNSName)
			if not sans:
				raise AcmeAbort(400, "malformed", "csr has no SAN")
			now = datetime.datetime.now(datetime.timezone.utc)
			leaf = (x509.CertificateBuilder()
				.subject_name(x509.Name([x509.NameAttribute(
					NameOID.COMMON_NAME, sans[0])]))
				.issuer_name(st.ca_cert.subject)
				.public_key(csr.public_key())
				.serial_number(x509.random_serial_number())
				.not_valid_before(now - datetime.timedelta(days=1))
				.not_valid_after(now + datetime.timedelta(days=90))
				.add_extension(
					x509.SubjectAlternativeName(
						[x509.DNSName(d) for d in sans]), False)
				.sign(st.ca_key, hashes.SHA256()))
			serial = leaf.serial_number
			cert_url = base + "/cert-%d" % serial
			chain = (leaf.public_bytes(serialization.Encoding.PEM) +
				st.ca_cert.public_bytes(serialization.Encoding.PEM)).decode()
			st.certificates[cert_url] = chain
			order["status"] = "valid"
			order["certificate"] = cert_url
			handler._respond_json({
				"status": "valid", "certificate": cert_url})

	AcmeHandler._finalize = finalize_impl

	for server in (dns_server, chall_server, acme_server):
		thread = threading.Thread(target=server.serve_forever, daemon=True)
		thread.start()

	print(json.dumps({
		"acme": acme_port, "chall": chall_port, "dns": dns_port,
		"directory": base + "/dir", "ca_pem": ca_path,
	}), flush=True)
	try:
		threading.Event().wait()
	except KeyboardInterrupt:
		pass
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
