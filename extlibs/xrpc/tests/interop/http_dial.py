"""Run the DNS client example against a Python stdlib HTTP/JSON peer."""
import hashlib
import http.server
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import threading


def main():
    binary = Path(sys.argv[1]).resolve()
    digest = hashlib.sha256(binary.read_bytes()).hexdigest()
    observed, failures = [], []

    class Handler(http.server.BaseHTTPRequestHandler):
        protocol_version = 'HTTP/1.1'

        def log_message(self, *_):
            pass

        def do_POST(self):
            try:
                assert self.path == '/rpc' and self.headers['Host'] == 'localhost'
                assert self.headers.get_content_type() == 'application/json'
                message = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
                assert message['jsonrpc'] == '2.0' and message['method'] == 'echo'
                assert isinstance(message['id'], str) and message['params'] == [7]
                observed.append(message)
                wire = json.dumps({'jsonrpc': '2.0', 'id': message['id'],
                                   'result': message['params']}, separators=(',', ':')).encode()
                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.send_header('Content-Length', str(len(wire)))
                self.end_headers()
                self.wfile.write(wire)
            except Exception as error:
                failures.append(repr(error))
                self.close_connection = True

    with http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler) as server:
        worker = threading.Thread(target=server.serve_forever, daemon=True)
        worker.start()
        try:
            env = os.environ.copy()
            env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
            result = subprocess.run([str(binary), str(server.server_port)],
                                    env=env, capture_output=True, text=True, timeout=30)
            assert result.returncode == 0, result.stdout + result.stderr
            assert len(observed) == 1 and not failures, (observed, failures)
            assert hashlib.sha256(binary.read_bytes()).hexdigest() == digest
            receipt = {'binary': str(binary), 'binary_sha256': digest,
                       'platform': platform.platform(), 'python': sys.version,
                       'exchanges': len(observed), 'exit_code': result.returncode,
                       'stdout': result.stdout, 'stderr': result.stderr,
                       'limitation': 'stdlib HTTP/JSON server and OS localhost DNS, not an independent JSON-RPC library or external DNS performance'}
            if len(sys.argv) > 2:
                Path(sys.argv[2]).write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
            print('PASS DNS client against independent Python HTTP/JSON server')
        finally:
            server.shutdown()
            worker.join(timeout=5)


if __name__ == '__main__':
    main()
