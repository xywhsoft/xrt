"""Drive the runnable server with Python stdlib HTTP/socket peers.

Usage: python -X utf8 extlibs/xrpc/tests/interop/http_server.py BINARY [RECEIPT]
This checks independent HTTP framing and JSON values, not JSON-RPC-library parity.
"""
import concurrent.futures
import hashlib
import http.client
import json
from pathlib import Path
import platform
import socket
import subprocess
import sys
import urllib.parse


def main():
    binary = Path(sys.argv[1]).resolve()
    digest = hashlib.sha256(binary.read_bytes()).hexdigest()
    process = subprocess.Popen([str(binary), '0', '10000'], stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, text=True)
    endpoint = urllib.parse.urlsplit(process.stdout.readline().strip())
    assert endpoint.hostname == '127.0.0.1' and endpoint.port
    count = 0

    def send(connection, document, *, chunked=False):
        payload = json.dumps(document, ensure_ascii=False).encode('utf-8')
        headers = {'Content-Type': 'application/json; charset=UTF-8'}
        body = iter([payload[:7], payload[7:]]) if chunked else payload
        connection.request('POST', '/rpc?trace=python', body=body, headers=headers,
                           encode_chunked=chunked)
        response = connection.getresponse()
        wire = response.read()
        assert response.getheader('Date')
        return response.status, json.loads(wire) if wire else None, response

    try:
        connection = http.client.HTTPConnection(endpoint.hostname, endpoint.port, timeout=5)
        huge = 10 ** 1200
        request = {'jsonrpc': '2.0', 'id': huge, 'method': 'echo', 'params': ['中文', '\x00', 7]}
        status, result, _ = send(connection, request)
        assert status == 200 and result == {'jsonrpc': '2.0', 'id': huge, 'result': request['params']}
        count += 1
        request['id'] = 'quoted\x00-id'
        status, result, _ = send(connection, request, chunked=True)
        assert status == 200 and result['id'] == request['id'] and result['result'] == request['params']
        count += 1
        notification = {'jsonrpc': '2.0', 'method': 'echo', 'params': []}
        status, result, response = send(connection, [notification, notification])
        assert status == 204 and result is None and response.getheader('Content-Length') is None
        count += 1
        status, result, _ = send(connection, [notification, request, 7])
        assert status == 200 and len(result) == 2 and result[0]['id'] == request['id']
        assert result[1]['error']['code'] == -32600 and result[1]['id'] is None
        count += 1
        connection.close()

        def worker(index):
            connection = http.client.HTTPConnection(endpoint.hostname, endpoint.port, timeout=5)
            for number in range(50):
                identity = f'{index}:{number}'
                status, result, _ = send(connection, {'jsonrpc': '2.0', 'id': identity,
                                                      'method': 'echo', 'params': [number]})
                assert status == 200 and result['id'] == identity and result['result'] == [number]
            connection.close()
            return 50

        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as executor:
            count += sum(executor.map(worker, range(8)))
        # Request EOF while a declared body is incomplete: no business dispatch.
        with socket.create_connection((endpoint.hostname, endpoint.port), timeout=5) as peer:
            peer.sendall(b'POST /rpc HTTP/1.1\r\nHost: a\r\nContent-Type: application/json\r\nContent-Length: 100\r\n\r\n{')
            peer.shutdown(socket.SHUT_WR)
            wire = b''
            while True:
                data = peer.recv(4096)
                if not data:
                    break
                wire += data
            assert wire.startswith(b'HTTP/1.1 400 '), wire
            count += 1
        stdout, stderr = process.communicate(timeout=20)
        assert process.returncode == 0, stderr
        assert hashlib.sha256(binary.read_bytes()).hexdigest() == digest
        result = {'binary': str(binary), 'binary_sha256': digest, 'platform': platform.platform(),
                  'python': sys.version, 'exchanges': count, 'concurrent_connections': 8,
                  'exit_code': process.returncode, 'stdout': stdout, 'stderr': stderr,
                  'limitation': 'Python stdlib HTTP framing/JSON interoperability, not an independent JSON-RPC library or native Linux production acceptance'}
        if len(sys.argv) > 2:
            Path(sys.argv[2]).write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
        print(f'PASS independent Python HTTP/JSON peer: {count} exchanges, eight concurrent persistent connections')
    finally:
        if process.poll() is None:
            process.kill()
            process.communicate()


if __name__ == '__main__':
    main()
