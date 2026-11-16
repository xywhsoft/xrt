"""Independent bounded RFC6455/OpenSSL gateway to the existing MQTT ACK-cut proxy.

The real broker retains MQTT state. This gateway implements only HTTP/WS byte
transport; it is not broker-native WebSocket or durability qualification.
"""
import base64
import hashlib
import select
import socket
import ssl
import subprocess
import threading
from run_session_broker import Proxy, read_exact


def bind_front(host, windows_python):
    # Bind the actual WSL socket first, then check the same port on Windows
    # before listen publishes it. Neither allocator knows the other's occupied
    # or excluded ports. This bounded fixture setup never retries MQTT traffic.
    attempts = []
    for _ in range(32):
        front = socket.socket()
        try:
            front.bind((host, 0))
            port = front.getsockname()[1]
            if windows_python:
                code = ('import socket,sys\ns=socket.socket()\ntry: s.bind((' + repr(host) + ',' + str(port) + '))\n'
                        'except OSError as e: print(e.errno);sys.exit(2)\nfinally: s.close()\n')
                result = subprocess.run([str(windows_python), '-c', code], capture_output=True, text=True, timeout=15)
                assert result.returncode in (0, 2), result.stderr
                attempts.append(dict(port=port, windows_bindable=result.returncode == 0, error=result.stdout.strip()))
                if result.returncode:
                    front.close()
                    continue
            return front, attempts
        except BaseException:
            front.close()
            raise
    raise RuntimeError('no cross-OS bindable fixture port within32 attempts: ' + repr(attempts))


class WebSocketProxy(Proxy):
    def __init__(self, backend, version, mode, host, *, tls, split, cert, key, drop_packets=True, windows_python=None):
        self.front, self.front_port_candidates = bind_front(host, windows_python)
        try:
            super().__init__(backend, version, mode, host, drop_packets=drop_packets)
        except BaseException:
            self.front.close()
            raise
        self.tcp_port = self.port
        self.front.listen(4)
        self.front.settimeout(.1)
        self.port = self.front.getsockname()[1]
        self.tls, self.split, self.cert, self.key = tls, split, cert, key
        self.front_sockets, self.front_threads, self.observations = [], [], []
        self.front_acceptor = threading.Thread(target=self.front_accept, daemon=True)
        self.front_acceptor.start()

    def front_accept(self):
        generation = 0
        while not self.stop.is_set():
            try:
                s, _ = self.front.accept()
            except socket.timeout:
                continue
            except OSError:
                break
            s.settimeout(15)
            self.front_sockets.append(s)
            try:
                # Native Windows readiness connects/closes without protocol data.
                if not s.recv(1, socket.MSG_PEEK):
                    s.close()
                    continue
            except OSError as e:
                self.error('WS readiness: ' + repr(e))
                s.close()
                break
            generation += 1
            if generation > 2:
                self.error('WS generation cap')
                s.close()
                break
            t = threading.Thread(target=self.bridge, args=(s, generation), daemon=True)
            self.front_threads.append(t)
            t.start()

    @staticmethod
    def send_frame(s, opcode, data, fin=True):
        n = len(data)
        assert n <= 4 * 1024 * 1024
        h = bytes([(128 if fin else 0) | opcode])
        h += bytes([n]) if n < 126 else b'\x7e' + n.to_bytes(2, 'big') if n < 65536 else b'\x7f' + n.to_bytes(8, 'big')
        s.sendall(h + data)

    @staticmethod
    def receive_frame(s):
        h = read_exact(s, 2)
        assert not h[0] & 0x70 and h[1] & 128, 'RSV/client masking'
        op, fin, n = h[0] & 15, bool(h[0] & 128), h[1] & 127
        if n == 126:
            n = int.from_bytes(read_exact(s, 2), 'big')
            assert n >= 126
        elif n == 127:
            n = int.from_bytes(read_exact(s, 8), 'big')
            assert 65536 <= n < 2**63
        assert n <= 4 * 1024 * 1024
        if op & 8:
            assert fin and n <= 125
        mask = read_exact(s, 4)
        data = read_exact(s, n) if n else b''
        return op, fin, bytes(b ^ mask[i % 4] for i, b in enumerate(data))

    def bridge(self, s, generation):
        backend = None
        row = dict(generation=generation, tls=self.tls, client_frames=0, server_frames=0,
                   client_bytes=0, server_bytes=0, pings=0, pongs=0, close_sent=False, close_received=False)
        try:
            if self.tls:
                ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
                ctx.load_cert_chain(self.cert, self.key)
                ctx.minimum_version = ctx.maximum_version = ssl.TLSVersion.TLSv1_2 if self.tls == 12 else ssl.TLSVersion.TLSv1_3
                ctx.set_servername_callback(lambda so, name, c: row.update(sni=name))
                s = ctx.wrap_socket(s, server_side=True)
                self.front_sockets.append(s)
                row['tls_version'] = s.version()
            # Read the request without consuming any hypothetical WS/MQTT prefix.
            head = bytearray()
            while not head.endswith(b'\r\n\r\n'):
                assert len(head) < 16384
                head += read_exact(s, 1)
            lines = head.decode('ascii').split('\r\n')
            assert lines[0] == 'GET /mqtt?managed=1 HTTP/1.1'
            fields = {}
            for line in lines[1:]:
                if line:
                    k, v = line.split(':', 1)
                    assert k.lower() not in fields
                    fields[k.lower()] = v.strip()
            assert fields['host'] == '127.0.0.1:' + str(self.port)
            assert fields['sec-websocket-protocol'] == 'mqtt' and 'sec-websocket-extensions' not in fields
            key = fields['sec-websocket-key']
            assert len(base64.b64decode(key, validate=True)) == 16
            accept = base64.b64encode(hashlib.sha1((key + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11').encode()).digest()).decode()
            s.sendall(('HTTP/1.1 103 Early Hints\r\n\r\nHTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Protocol: mqtt\r\nSec-WebSocket-Accept: ' + accept + '\r\n\r\n').encode())
            row.update(authority=fields['host'], key_sha256=hashlib.sha256(key.encode()).hexdigest(), upgraded=True)
            backend = socket.create_connection(('127.0.0.1', self.tcp_port), timeout=15)
            backend.settimeout(15)
            self.front_sockets.append(backend)
            fragmented = False
            backend_open = True
            while not self.stop.is_set():
                ready = [s] if self.tls and s.pending() else select.select([s, backend] if backend_open else [s], [], [], .1)[0]
                if s in ready:
                    op, fin, data = self.receive_frame(s)
                    row['client_frames'] += 1
                    if op == 8:
                        assert len(data) >= 2 and data[:2] == b'\x03\xe8'
                        row['close_received'] = True
                        if not row['close_sent']:
                            self.send_frame(s, 8, data)
                            row['close_sent'] = True
                        if self.tls:
                            s = s.unwrap()
                            self.front_sockets.append(s)
                            row['authenticated_tls_close'] = True
                        while s.recv(4096):
                            pass
                        break
                    if op == 9:
                        self.send_frame(s, 10, data)
                        continue
                    if op == 10:
                        assert data == b'p'
                        row['pongs'] += 1
                        continue
                    assert (op == 2 and not fragmented) or (op == 0 and fragmented), 'Binary/continuation order'
                    fragmented = not fin
                    backend.sendall(data)
                    row['client_bytes'] += len(data)
                if backend in ready:
                    data = backend.recv(4096)
                    if not data:
                        backend_open = False
                        if not row['close_sent']:
                            self.send_frame(s, 8, b'\x03\xe8')
                            row['close_sent'] = True
                    else:
                        row['server_bytes'] += len(data)
                        if self.split and len(data) > 1:
                            self.send_frame(s, 2, data[:1], False)
                            self.send_frame(s, 9, b'p')
                            self.send_frame(s, 0, data[1:])
                            row['server_frames'] += 2
                            row['pings'] += 1
                        else:
                            self.send_frame(s, 2, data)
                            row['server_frames'] += 1
            if generation == 2:
                assert row['close_received'] and row['close_sent']
                assert not self.tls or row.get('authenticated_tls_close')
        except (EOFError, ConnectionResetError, BrokenPipeError, ssl.SSLEOFError) as e:
            row['abort'] = type(e).__name__
            if generation != 1 and not self.stop.is_set():
                self.error('WS final generation: ' + repr(e))
        except Exception as e:
            if not self.stop.is_set():
                self.error('WS gateway: ' + repr(e))
        finally:
            for sock in [s, backend]:
                if sock:
                    try:
                        sock.shutdown(socket.SHUT_RDWR)
                    except OSError:
                        pass
                    sock.close()
            with self.cv:
                self.observations.append(row)
                self.cv.notify_all()

    def verify(self):
        if self.drop_packets:
            super().verify()
        else:
            assert not self.errors and not any(not r['forwarded'] for r in self.rows)
            acks = [r for r in self.rows if r['type'] == 2]
            assert [r['session_present'] for r in acks] == [False, True]
            assert len([r for r in self.rows if r['direction'] == 'sc' and r['type'] == 4]) == 2
        self.front_threads[-1].join(10)
        assert len(self.observations) == 2 and len({r['key_sha256'] for r in self.observations}) == 2
        assert all(r.get('upgraded') and r['client_bytes'] and r['server_bytes'] for r in self.observations)
        last = next(r for r in self.observations if r['generation'] == 2)
        assert last['close_received'] and last['close_sent'] and last['pongs'] == last['pings']
        assert not self.tls or last.get('authenticated_tls_close')

    def close(self):
        self.stop.set()
        self.front.close()
        for s in self.front_sockets:
            try:
                s.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            s.close()
        self.front_acceptor.join(2)
        for t in self.front_threads:
            t.join(2)
        super().close()
        assert not self.front_acceptor.is_alive() and not any(t.is_alive() for t in self.front_threads)
