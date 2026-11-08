"""Independent stdlib HTTPS peer, runtime CA, positive and rejected identities.

Run after building examples/http_tls/main.c, optionally --binary a linked
package consumer with the same CLI. Only public certificate hashes are recorded.
Temporary keys never enter the repository or receipts and are removed on exit.
"""
import argparse
import base64
import hashlib
import http.server
import json
import os
from pathlib import Path
import platform
import shutil
import ssl
import subprocess
import sys
import tempfile
import threading

root = Path(__file__).resolve().parents[4]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary', type=Path)
parser.add_argument('--report', type=Path)
parser.add_argument('--auth-mode', choices=['basic','bearer'], default='bearer')
args = parser.parse_args()
name, arch, suffix = ('windows', 'native', '.exe') if os.name == 'nt' else ('wsl', 'x64', '')
out = root / 'out/service-extensions/xrpc-http-headers'
binary = (args.binary or root / 'out/gcc' / arch / 'xrpc' / ('examples_http_tls_main' + suffix)).resolve()
report = args.report or out / (name + '-auth-'+args.auth_mode+'-interop.json')
openssl = shutil.which('openssl')
if not openssl and os.name == 'nt':
    candidate = Path('C:/Program Files/Git/usr/bin/openssl.exe')
    if candidate.is_file():
        openssl = str(candidate)
assert openssl and binary.is_file(), 'OpenSSL and the built TLS example are required'
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
out.mkdir(parents=True, exist_ok=True)
authorization = 'Basic '+base64.b64encode(b'public:fixture').decode() if args.auth_mode=='basic' else 'Bearer public-fixture'
cases = []

with tempfile.TemporaryDirectory(prefix='tls-peer-', dir=out) as temporary:
    temp = Path(temporary).resolve()
    assert temp.is_relative_to(out.resolve()), 'temporary-key directory escaped task output'

    def certificate(*command):
        subprocess.run([openssl, *command], cwd=temp, check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=30)

    (temp/'authorization.txt').write_bytes((authorization+'\r\n').encode('ascii'))
    for prefix in ['ca', 'other']:
        certificate('req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-sha256', '-days', '2',
                    '-subj', '/CN=XRT RPC local ' + prefix,
                    '-addext', 'basicConstraints=critical,CA:TRUE',
                    '-addext', 'keyUsage=critical,keyCertSign,cRLSign',
                    '-keyout', prefix + '.key', '-out', prefix + '.pem')
    certificate('req', '-new', '-newkey', 'rsa:2048', '-nodes', '-sha256',
                '-subj', '/CN=rpc.test', '-keyout', 'peer.key', '-out', 'peer.csr')
    (temp / 'extensions.cnf').write_text(
        'basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\n'
        'extendedKeyUsage=serverAuth\nsubjectAltName=DNS:rpc.test\n', encoding='utf-8')
    certificate('x509', '-req', '-in', 'peer.csr', '-CA', 'ca.pem', '-CAkey', 'ca.key',
                '-CAcreateserial', '-days', '2', '-sha256', '-extfile', 'extensions.cnf', '-out', 'peer.pem')

    for version, alpn, identity, trust, expected in [
        ('TLSv1_3', True, 'rpc.test', 'ca', 0),
        ('TLSv1_2', True, 'rpc.test', 'ca', 0),
        ('TLSv1_3', False, 'rpc.test', 'ca', 0),
        ('TLSv1_3', True, 'wrong.test', 'ca', 1),
        ('TLSv1_3', True, 'rpc.test', 'other', 1),
    ]:
        requests, errors, completed_closes = [], [], []
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.minimum_version = context.maximum_version = getattr(ssl.TLSVersion, version)
        context.load_cert_chain(temp / 'peer.pem', temp / 'peer.key')
        if version == 'TLSv1_2':
            context.set_ciphers('ECDHE-RSA-AES128-GCM-SHA256')
        if alpn:
            context.set_alpn_protocols(['http/1.1'])

        class Server(http.server.ThreadingHTTPServer):
            daemon_threads = False
            block_on_close = True

            def get_request(self):
                sock, address = super().get_request()
                sock.settimeout(15)
                try:
                    return context.wrap_socket(sock, server_side=True), address
                except (ssl.SSLError, OSError):
                    sock.close()
                    raise

            def handle_error(self, request, client_address):
                errors.append('request-handler-failure')

        class Handler(http.server.BaseHTTPRequestHandler):
            protocol_version = 'HTTP/1.1'

            def handle(self):
                self.reply_complete = False
                try:
                    super().handle()
                except (ConnectionResetError, ssl.SSLEOFError) as exc:
                    # Public client Close deliberately aborts. Only a reset
                    # while waiting after a completely flushed reply is normal.
                    if not self.reply_complete:
                        raise
                    completed_closes.append(type(exc).__name__)

            def finish(self):
                try:
                    super().finish()
                except (ConnectionResetError, ssl.SSLEOFError, BrokenPipeError) as exc:
                    if not getattr(self, 'reply_complete', False):
                        raise
                    completed_closes.append(type(exc).__name__)

            def log_message(self, *unused):
                pass

            def do_POST(self):
                try:
                    assert self.path == '/rpc' and self.headers['Host'] == 'localhost'
                    assert self.headers.get_all('Authorization') == [authorization]
                    if args.auth_mode=='basic':
                        assert base64.b64decode(self.headers['Authorization'].split(' ',1)[1],validate=True)==b'public:fixture'
                    length = int(self.headers['Content-Length'])
                    assert 0 < length <= 65536
                    data = json.loads(self.rfile.read(length))
                    assert data['jsonrpc'] == '2.0' and data['method'] == 'echo'
                    assert data['params'] == [7] and isinstance(data['id'], str)
                    requests.append({'version': self.connection.version(),
                                     'alpn': self.connection.selected_alpn_protocol(),
                                     'cipher': self.connection.cipher()[0]})
                    body = json.dumps({'jsonrpc': '2.0', 'id': data['id'], 'result': [7]}).encode()
                    self.send_response(200)
                    self.send_header('Content-Type', 'application/json')
                    self.send_header('Content-Length', str(len(body)))
                    self.end_headers()
                    self.wfile.write(body)
                    self.wfile.flush()
                    self.reply_complete = True
                except Exception as exc:
                    errors.append(type(exc).__name__)
                    self.close_connection = True

        server = Server(('127.0.0.1', 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            command = [str(binary), 'localhost', str(server.server_port), str(temp / (trust + '.pem')), identity, str(temp/'authorization.txt')]
            env = os.environ.copy()
            env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
            result = subprocess.run(command, text=True, capture_output=True, timeout=40, env=env)
        finally:
            server.shutdown()
            server.server_close()
            thread.join(20)
            assert not thread.is_alive(), 'HTTPS listener did not retire'
        assert result.returncode == expected, (version, alpn, identity, trust, result.returncode, result.stdout, result.stderr)
        assert 'AddressSanitizer' not in result.stderr and 'runtime error:' not in result.stderr and 'LeakSanitizer' not in result.stderr, result.stderr
        assert not errors, errors
        assert len(requests) == (1 if expected == 0 else 0), requests
        if expected == 0:
            assert '[PASS] trusted TLS identity' in result.stdout
            assert requests[0]['version'] == version.replace('_', '.')
            assert requests[0]['alpn'] == ('http/1.1' if alpn else None)
        else:
            assert 'TLS_CALL_TRANSPORT_NOT_SENT' in result.stderr, result.stderr
        cases.append({'tls_version': version, 'offer_http11': alpn, 'identity': identity,
                      'trust': trust, 'expected_exit': expected, 'exit_code': result.returncode,
                      'requests': requests, 'completed_reply_closes': completed_closes,
                      'stdout': result.stdout, 'stderr': result.stderr})
    certificates = {name: sha(temp / name) for name in ['ca.pem', 'other.pem', 'peer.pem']}

report.parent.mkdir(parents=True, exist_ok=True)
report.write_text(json.dumps({'binary': str(binary), 'binary_sha256': sha(binary),
    'script_sha256': sha(Path(__file__)), 'auth_mode': args.auth_mode, 'platform': platform.platform(), 'python': sys.version,
    'python_tls': ssl.OPENSSL_VERSION, 'certificate_tool': subprocess.check_output([openssl, 'version'], text=True).strip(),
    'public_certificate_sha256': certificates, 'cases': cases, 'exchanges': 3,
    'rejected_before_rpc': 2, 'exit_code': 0}, indent=2) + '\n', encoding='utf-8')
print('PASS trusted TLS 1.3/1.2, HTTP/1.1/no ALPN, wrong identity/CA refused before RPC')
