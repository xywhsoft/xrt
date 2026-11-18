"""Mutual TLS and role-specific ACL profiles for the owned Sentinel fixture."""

from pathlib import Path
import json
import shutil
import socket
import ssl
import subprocess
import sys

from run_redis_role import frame, send, sha
from run_redis_tls import certificates


class SentinelTls:
    # These accounts exist only on disposable loopback processes.
    operator = ("xredis-operator", "fixture-operator-pass")
    seed = ("xredis-seed", "fixture-seed-pass")
    data = ("xredis-data", "fixture-data-pass")

    def __init__(self, root, version, openssl):
        self.root = root
        self.version = version
        self.openssl = Path(openssl or shutil.which("openssl")).absolute()
        self.public_hashes = certificates(self.openssl, root)
        (root / "client.ext").write_text(
            "basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature\n"
            "extendedKeyUsage=clientAuth\n", encoding="utf-8")
        for name in ("native", "peer"):
            self.command("req", "-newkey", "rsa:2048", "-nodes", "-sha256",
                         "-subj", f"/CN=xredis-{name}-fixture",
                         "-keyout", name + ".key", "-out", name + ".csr")
            self.command("x509", "-req", "-in", name + ".csr", "-CA", "ca.pem",
                         "-CAkey", "ca.key", "-CAcreateserial", "-days", "2",
                         "-sha256", "-extfile", "client.ext", "-out", name + ".pem")
            self.public_hashes[name + ".pem"] = sha(root / (name + ".pem"))
        self.context = self.make_context()
        self.without_identity = self.make_context(identity=False)

    def command(self, *args):
        subprocess.run([str(self.openssl), *args], cwd=self.root, check=True,
                       capture_output=True, timeout=30)

    def make_context(self, identity=True):
        context = ssl.create_default_context(cafile=str(self.root / "ca.pem"))
        version = ssl.TLSVersion.TLSv1_2 if self.version == 12 else ssl.TLSVersion.TLSv1_3
        context.minimum_version = context.maximum_version = version
        if identity:
            context.load_cert_chain(str(self.root / "native.pem"), str(self.root / "native.key"))
        return context

    def probe_args(self, client):
        args = [str(self.root / name) for name in ("ca.pem", "native.pem", "native.key")]
        if sys.platform != "win32" and client.suffix.lower() == ".exe":
            args = [subprocess.check_output(["wslpath", "-w", path], text=True).strip() for path in args]
        return [*args, str(self.version)]

    def config(self, name, port, master, directory):
        paths = (
            ("tls-cert-file", "server.pem"), ("tls-key-file", "server.key"),
            ("tls-ca-cert-file", "ca.pem"), ("tls-client-cert-file", "peer.pem"),
            ("tls-client-key-file", "peer.key"),
        )
        tls = "\n".join(f"{key} {json.dumps(str(self.root / file))}" for key, file in paths)
        prefix = (f"bind 127.0.0.1\nport 0\ntls-port {port}\ntls-auth-clients yes\n"
                  f"tls-replication yes\ntls-protocols TLSv1.{self.version - 10}\n{tls}\n"
                  f"dir {json.dumps(str(directory))}\nsave \"\"\nappendonly no\n"
                  "user default off\nuser xredis-operator on >fixture-operator-pass ~* &* +@all\n")
        if name == "sentinel":
            return (prefix + f"sentinel monitor cache 127.0.0.1 {master} 1\n"
                    "sentinel down-after-milliseconds cache 5000\nsentinel failover-timeout cache 10000\n"
                    "sentinel auth-user cache xredis-monitor\nsentinel auth-pass cache fixture-monitor-pass\n"
                    "user xredis-seed on >fixture-seed-pass &+switch-master +auth +hello +ping +subscribe "
                    "+sentinel|get-master-addr-by-name +sentinel|replicas +sentinel|sentinels\n")
        replication = f"replicaof 127.0.0.1 {master}\n" if name == "replica" else ""
        priority = "replica-priority 0\n" if name == "standby" else ""
        return (prefix + replication + priority + "masteruser xredis-replication\nmasterauth fixture-replication-pass\n"
                "repl-diskless-sync-delay 0\n"
                "user xredis-replication on >fixture-replication-pass +ping +psync +replconf\n"
                "user xredis-monitor on >fixture-monitor-pass &__sentinel__:hello "
                "+multi +slaveof +ping +exec +subscribe +config|rewrite +role +publish +info "
                "+client|setname +client|kill +script|kill\n"
                "user xredis-data on >fixture-data-pass ~xredis-owned-failover-empty "
                "+auth +hello +ping +info +role +client|list +blpop +xread\n")

    def query(self, port, args, auth, context):
        with socket.create_connection(("127.0.0.1", port), timeout=2) as raw:
            with context.wrap_socket(raw, server_hostname="localhost") as stream, stream.makefile("rb") as reply:
                if auth is not None:
                    send(stream, ["AUTH", *auth])
                    assert frame(reply)[0] == b"OK"
                send(stream, args)
                return frame(reply)

    def exchange(self, port, args):
        return self.query(port, args, self.operator, self.context)

    def check_policy(self, seed, data):
        def denied(port, auth, args, reason):
            try:
                self.query(port, args, auth, self.context)
            except ValueError as error:
                assert reason in str(error), error
            else:
                raise AssertionError("unexpected success with insufficient ACL")

        for port in (seed, data):
            denied(port, None, ["PING"], "NOAUTH")
            with socket.create_connection(("127.0.0.1", port), timeout=2) as raw:
                try:
                    send(raw, ["PING"])
                    assert not raw.recv(256), "TLS-only listener returned a plaintext reply"
                except (ConnectionResetError, BrokenPipeError):
                    pass
            try:
                self.query(port, ["PING"], self.operator, self.without_identity)
            except (ssl.SSLError, ConnectionResetError, BrokenPipeError):
                pass
            except ValueError as error:
                assert str(error) == "incomplete fixture RESP line", error
            else:
                raise AssertionError("listener accepted a client without a certificate")
        denied(seed, self.data, ["PING"], "WRONGPASS")
        denied(data, self.seed, ["PING"], "WRONGPASS")
        denied(seed, self.seed, ["SENTINEL", "MYID"], "NOPERM")
        denied(data, self.data, ["CONFIG", "GET", "port"], "NOPERM")
        return dict(missing_identity_rejected=2, plaintext_rejected=2, redis_auth_required=2,
                    cross_role_login_rejected=2, restricted_commands_rejected=2)
