"""Independent Python/OpenSSL client against the actual C HTTPS server.

Build examples/http_server_tls first. --binary accepts an equivalent static or
shared package consumer. Keys are generated at runtime and removed on exit.
"""
import argparse
import hashlib
import http.client
import json
import os
from pathlib import Path
import platform
import queue
import shutil
import socket
import ssl
import subprocess
import sys
import tempfile
import threading

root = Path(__file__).resolve().parents[4]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary', type=Path)
parser.add_argument('--report', type=Path)
args = parser.parse_args()
name, arch, suffix = ('windows', 'native', '.exe') if os.name == 'nt' else ('wsl', 'x64', '')
out = root / 'out/service-extensions/xrpc-http-server-tls'
binary = (args.binary or root/'out/gcc'/arch/'xrpc'/('examples_http_server_tls_main'+suffix)).resolve()
report = args.report or out/(name+'-server-tls-interop.json')
openssl = shutil.which('openssl')
if not openssl and os.name == 'nt':
    candidate = Path('C:/Program Files/Git/usr/bin/openssl.exe')
    if candidate.is_file():
        openssl = str(candidate)
assert openssl and binary.is_file(), 'OpenSSL and built HTTPS example required'
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
out.mkdir(parents=True, exist_ok=True)
cases = []
with tempfile.TemporaryDirectory(prefix='https-server-', dir=out) as temporary:
    temp = Path(temporary).resolve()
    assert temp.is_relative_to(out.resolve())

    def certificate(*command):
        subprocess.run([openssl, *command], cwd=temp, check=True, stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL, timeout=30)

    certificate('req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-sha256', '-days', '2',
                '-subj', '/CN=XRT local interop CA', '-addext', 'basicConstraints=critical,CA:TRUE',
                '-addext', 'keyUsage=critical,keyCertSign,cRLSign', '-keyout', 'ca.key', '-out', 'ca.pem')
    (temp/'extensions.cnf').write_text('basicConstraints=critical,CA:FALSE\n'
        'keyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\n'
        'subjectAltName=DNS:rpc.test\n', encoding='utf-8')
    for kind, key_args in [
        ('rsa', ['-algorithm', 'RSA', '-pkeyopt', 'rsa_keygen_bits:2048']),
        ('p256', ['-algorithm', 'EC', '-pkeyopt', 'ec_paramgen_curve:P-256']),
        ('p384', ['-algorithm', 'EC', '-pkeyopt', 'ec_paramgen_curve:P-384']),
        ('ed25519', ['-algorithm', 'ED25519']),
    ]:
        certificate('genpkey', *key_args, '-out', 'peer.key')
        certificate('req', '-new', '-key', 'peer.key', '-subj', '/CN=rpc.test', '-out', 'peer.csr')
        certificate('x509', '-req', '-in', 'peer.csr', '-CA', 'ca.pem', '-CAkey', 'ca.key',
                    '-CAcreateserial', '-days', '2', '-sha256', '-extfile', 'extensions.cnf', '-out', 'peer.pem')
        certificate('x509', '-in', 'peer.pem', '-outform', 'DER', '-out', 'peer.der')
        certificate('pkcs8', '-topk8', '-nocrypt', '-in', 'peer.key', '-outform', 'DER', '-out', 'key.der')
        command = [str(binary), str(temp/'peer.der'), str(temp/'key.der'), '0', '7000', kind]
        env = os.environ.copy()
        env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   text=True, env=env)
        lines = queue.Queue()

        def read_output():
            for line in process.stdout:
                lines.put(line)
            lines.put(None)

        reader = threading.Thread(target=read_output)
        reader.start()
        try:
            endpoint = lines.get(timeout=10)
            assert endpoint and endpoint.startswith('https://127.0.0.1:'), endpoint
            port = int(endpoint.split(':')[2].split('/')[0])
            exchanges = []
            versions = [('TLSv1_3', True), ('TLSv1_3', False)]
            if kind != 'ed25519':
                versions.append(('TLSv1_2', True))
            for version, alpn in versions:
                context = ssl.create_default_context(cafile=str(temp/'ca.pem'))
                context.minimum_version = context.maximum_version = getattr(ssl.TLSVersion, version)
                if alpn:
                    context.set_alpn_protocols(['http/1.1'])
                if version == 'TLSv1_2':
                    context.set_ciphers('ECDHE-RSA-AES128-GCM-SHA256' if kind == 'rsa'
                                        else 'ECDHE-ECDSA-AES128-GCM-SHA256')
                with socket.create_connection(('127.0.0.1', port), timeout=5) as tcp:
                    with context.wrap_socket(tcp, server_hostname='rpc.test') as secure:
                        assert secure.version() == version.replace('_', '.')
                        assert secure.selected_alpn_protocol() == ('http/1.1' if alpn else None)
                        connection = http.client.HTTPConnection('localhost', timeout=5)
                        connection.sock = secure
                        documents = [
                            {'jsonrpc': '2.0', 'id': 'ordinary', 'method': 'echo', 'params': [7]},
                            {'jsonrpc': '2.0', 'method': 'echo', 'params': [8]},
                            [{'jsonrpc': '2.0', 'id': 9, 'method': 'echo', 'params': [9]},
                             {'jsonrpc': '2.0', 'method': 'echo', 'params': [10]}],
                        ]
                        for index, document in enumerate(documents):
                            connection.request('POST', '/rpc', json.dumps(document),
                                               {'Content-Type': 'application/json'})
                            response = connection.getresponse()
                            body = response.read()
                            assert response.status == (204 if index == 1 else 200), (response.status, body)
                            if index == 0:
                                assert json.loads(body) == {'jsonrpc': '2.0', 'id': 'ordinary', 'result': [7]}
                            elif index == 1:
                                assert not body
                            else:
                                assert json.loads(body) == [{'jsonrpc': '2.0', 'id': 9, 'result': [9]}]
                        exchanges.append({'version': secure.version(), 'alpn': secure.selected_alpn_protocol(),
                                          'cipher': secure.cipher()[0], 'http_requests': 3})
                        connection.close()
            # A cleartext request cannot reach JSON-RPC or receive an HTTP reply.
            with socket.create_connection(('127.0.0.1', port), timeout=5) as tcp:
                tcp.sendall(b'POST /rpc HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n')
                try:
                    rejected = tcp.recv(256)
                except ConnectionResetError:
                    rejected = b''
                assert not rejected.startswith(b'HTTP/'), rejected
            assert process.wait(timeout=15) == 0
            reader.join(5)
            assert not reader.is_alive()
            tail = []
            while True:
                line = lines.get(timeout=1)
                if line is None:
                    break
                tail.append(line)
            errors = process.stderr.read()
            assert not errors, errors
            assert len(tail) == 1 and tail[0].startswith('stats '), tail
            stats = json.loads(tail[0][6:])
            expected = len(versions)*3
            assert stats['requests'] == stats['dispatched'] == expected, stats
            assert stats['connections'] == stats['queued'] == stats['active_handshakes'] == 0, stats
            assert stats['admitted'] == stats['completed']+stats['failed'] and stats['failed'] >= 1, stats
            cases.append({'key': kind, 'exchanges': exchanges, 'stats': stats,
                          'certificate_sha256': sha(temp/'peer.der'), 'exit_code': process.returncode})
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=10)
            reader.join(5)
            process.stdout.close()
            process.stderr.close()
report.parent.mkdir(parents=True, exist_ok=True)
report.write_text(json.dumps({'binary': str(binary), 'binary_sha256': sha(binary),
    'script_sha256': sha(Path(__file__)), 'platform': platform.platform(), 'python': sys.version,
    'python_tls': ssl.OPENSSL_VERSION, 'certificate_tool': subprocess.check_output([openssl, 'version'], text=True).strip(),
    'cases': cases, 'http_exchanges': 33, 'plaintext_rejections': 4, 'exit_code': 0}, indent=2)+'\n', encoding='utf-8')
print('[PASS] independent HTTPS server: four keys, TLS 1.3/1.2, ALPN/absence, 33 RPC HTTP exchanges, plaintext rejection, physical counters')
