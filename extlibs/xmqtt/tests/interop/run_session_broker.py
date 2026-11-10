"""Opt-in Mosquitto recovery checks through a bounded independent TCP proxy.

Only framing/metadata is decoded here. No xmqtt or broker implementation imports.
The proxy drops one complete control packet, never alters a PUBLISH or payload.
"""
from pathlib import Path
import argparse
import getpass
import hashlib
import json
import os
import platform
import queue
import shutil
import socket
import subprocess
import tempfile
import threading
import time


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read_exact(sock, count):
    parts = bytearray()
    while len(parts) < count:
        chunk = sock.recv(count - len(parts))
        if not chunk:
            if not parts:
                raise EOFError
            raise ValueError('truncated frame')
        parts.extend(chunk)
    return bytes(parts)


def varint(data, offset):
    value = 0
    for index in range(4):
        octet = data[offset + index]
        value |= (octet & 127) << (7 * index)
        if not octet & 128:
            return value, offset + index + 1
    raise ValueError('overlong variable integer')


def packet(sock):
    head = read_exact(sock, 1)
    size = 0
    for index in range(4):
        octet = read_exact(sock, 1)
        head += octet
        size |= (octet[0] & 127) << (7 * index)
        if not octet[0] & 128:
            if size > 2 * 1024 * 1024:
                raise ValueError('proxy frame cap')
            body = read_exact(sock, size) if size else b''
            return head + body, body
    raise ValueError('overlong Remaining Length')


class Proxy:
    def __init__(self, backend, version, mode, host='127.0.0.1'):
        self.backend, self.version, self.mode = backend, version, mode
        self.listener = socket.socket()
        self.listener.bind((host, 0))
        self.port = self.listener.getsockname()[1]
        self.listener.listen(2)
        self.listener.settimeout(.1)
        self.cv = threading.Condition()
        self.stop = threading.Event()
        self.rows, self.errors, self.connections, self.threads = [], [], [], []
        self.dropped = False
        self.acceptor = threading.Thread(target=self.accept, daemon=True)
        self.acceptor.start()

    def accept(self):
        generation = 0
        while not self.stop.is_set():
            try:
                frontend, _ = self.listener.accept()
            except socket.timeout:
                continue
            except OSError:
                break
            self.connections.append(frontend)
            # Readiness checks connect/close without MQTT data. They must not
            # consume either protocol generation or touch the broker session.
            frontend.settimeout(20)
            try:
                if not frontend.recv(1, socket.MSG_PEEK):
                    frontend.close()
                    continue
            except OSError as exc:
                frontend.close()
                self.error('first MQTT byte: ' + str(exc))
                break
            generation += 1
            if generation > 2:
                frontend.close()
                self.error('connection cap')
                break
            try:
                backend = socket.create_connection(('127.0.0.1', self.backend), timeout=10)
            except OSError as exc:
                frontend.close()
                self.error(str(exc))
                break
            frontend.settimeout(20)
            backend.settimeout(20)
            self.connections.append(backend)
            for source, target, direction in [(frontend, backend, 'cs'), (backend, frontend, 'sc')]:
                thread = threading.Thread(target=self.relay, args=(source, target, direction, generation), daemon=True)
                self.threads.append(thread)
                thread.start()

    def error(self, text):
        with self.cv:
            self.errors.append(text)
            self.cv.notify_all()

    def relay(self, source, target, direction, generation):
        try:
            while not self.stop.is_set():
                raw, body = packet(source)
                kind = raw[0] >> 4
                row = dict(generation=generation, direction=direction, type=kind,
                           flags=raw[0] & 15, bytes=len(raw), forwarded=True)
                if kind == 2:
                    row['session_present'] = bool(body[0] & 1)
                    row['reason'] = body[1]
                if kind == 3:
                    topic_size = int.from_bytes(body[:2], 'big')
                    offset = 2 + topic_size
                    row['topic_bytes'] = topic_size
                    if topic_size:
                        row['topic'] = body[2:offset].decode('utf-8')
                    if (raw[0] >> 1) & 3:
                        row['identifier'] = int.from_bytes(body[offset:offset + 2], 'big')
                        offset += 2
                    if self.version == 5:
                        count, start = varint(body, offset)
                        row['property_hex'] = body[start:start + count].hex()
                elif kind in [4, 5, 6, 7, 8, 9, 10, 11]:
                    row['identifier'] = int.from_bytes(body[:2], 'big')
                    if kind in [4, 5, 6, 7]:
                        row['reason'] = body[2] if len(body) >= 3 else 0
                drop_direction, drop_type = [('sc', 4), ('sc', 5), ('sc', 7), ('cs', 5), ('cs', 7)][self.mode]
                with self.cv:
                    if generation == 1 and direction == drop_direction and kind == drop_type and not self.dropped:
                        row['forwarded'] = False
                        self.dropped = True
                if row['forwarded']:
                    target.sendall(raw)
                with self.cv:
                    if len(self.rows) >= 200:
                        raise ValueError('proxy trace cap')
                    row['order'] = len(self.rows)
                    self.rows.append(row)
                    self.cv.notify_all()
        except EOFError:
            pass
        except OSError as exc:
            # Close/abort can race the opposite relay at a complete frame.
            if not self.stop.is_set() and not isinstance(exc, (ConnectionResetError, BrokenPipeError)):
                self.error(type(exc).__name__ + ': ' + str(exc))
        except Exception as exc:
            if not self.stop.is_set():
                self.error(type(exc).__name__ + ': ' + str(exc))
        finally:
            for sock in [source, target]:
                try:
                    sock.shutdown(socket.SHUT_RDWR)
                except OSError:
                    pass

    def recovered(self):
        rows = [row for row in self.rows if row['generation'] == 2 and row['forwarded']]
        def has(direction, kind):
            return any(r['direction'] == direction and r['type'] == kind for r in rows)
        required = [[('cs', 3), ('sc', 4)],
                    [('cs', 3), ('sc', 5), ('cs', 6), ('sc', 7), ('cs', 7)],
                    [('cs', 6), ('sc', 7)],
                    [('sc', 3), ('cs', 5), ('sc', 6), ('cs', 7)],
                    [('sc', 6), ('cs', 7)]][self.mode]
        return all(has(direction, kind) for direction, kind in required)

    def wait(self, predicate):
        end = time.monotonic() + 20
        with self.cv:
            while not predicate():
                if self.errors:
                    raise AssertionError(self.errors)
                left = end - time.monotonic()
                if left <= 0:
                    raise TimeoutError('independent wire barrier: ' + json.dumps(self.rows[-12:]))
                self.cv.wait(min(left, .2))

    def verify(self):
        assert not self.errors, self.errors
        dropped = [r for r in self.rows if not r['forwarded']]
        assert len(dropped) == 1
        connacks = [r for r in self.rows if r['type'] == 2]
        assert len(connacks) == 2 and [r['session_present'] for r in connacks] == [False, True]
        assert all(not r['reason'] for r in connacks)
        first = next(r for r in self.rows if r['type'] == 3 and r['direction'] == 'cs')
        replay = [r for r in self.rows if r['generation'] == 2 and r['type'] == 3 and r['direction'] == 'cs']
        if self.mode < 2:
            assert len(replay) == 1 and replay[0]['flags'] & 8
            assert replay[0]['identifier'] == first['identifier']
            assert replay[0].get('topic') == first.get('topic') == 'xrt/qual/session/binary'
            if self.version == 5:
                assert first['property_hex'] == '230001' and replay[0]['property_hex'] == ''
        else:
            assert not replay, 'completed PUBLISH must not be retransmitted'
        if self.mode == 2:
            rel = next(r for r in self.rows if r['generation'] == 2 and r['direction'] == 'cs' and r['type'] == 6)
            assert rel['flags'] == 2 and rel['identifier'] == first['identifier']
        if self.mode >= 3:
            old = next(r for r in self.rows if r['generation'] == 1 and r['direction'] == 'sc' and r['type'] == 3)
            if self.mode == 3:
                new = next(r for r in self.rows if r['generation'] == 2 and r['direction'] == 'sc' and r['type'] == 3)
                assert new['flags'] & 8 and new['identifier'] == old['identifier']
            else:
                ack = next(r for r in self.rows if r['generation'] == 2 and r['direction'] == 'cs' and r['type'] == 7)
                assert ack['identifier'] == old['identifier'] and ack['reason'] == (0x92 if self.version == 5 else 0)
        assert self.recovered()

    def close(self):
        self.stop.set()
        self.listener.close()
        for sock in self.connections:
            try:
                sock.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            sock.close()
        self.acceptor.join(2)
        for thread in self.threads:
            thread.join(2)
        assert not self.acceptor.is_alive() and not any(t.is_alive() for t in self.threads), 'proxy threads reaped'


def case(broker_path, client, version, mode, fragmented, directory, name, windows_python=None, proxy_host='127.0.0.1'):
    directory.mkdir(parents=True, exist_ok=True)
    with socket.socket() as seed:
        seed.bind(('127.0.0.1', 0))
        port = seed.getsockname()[1]
    config = directory / (name + '.conf')
    log = directory / (name + '.log')
    config.write_text(('user ' + getpass.getuser() + '\n' if hasattr(os, 'geteuid') and os.geteuid() == 0 else '') +
                      f'persistence false\nlog_dest stdout\nlog_type all\nset_tcp_nodelay true\nmax_queued_messages 64\nmax_inflight_messages 16\nallow_anonymous true\nlistener {port} 127.0.0.1\nmax_topic_alias 10\n', encoding='utf-8')
    broker = proxy = probe = None
    readers = []
    stdout, stderr, output = [], [], queue.Queue(maxsize=16)
    row = dict(mqtt=version, mode=mode, fragmented=bool(fragmented), proxy_host=proxy_host, passed=False)
    error = None
    started = time.monotonic()
    with log.open('w', encoding='utf-8') as stream:
        try:
            broker = subprocess.Popen([str(broker_path), '-c', str(config.resolve())], stdout=stream, stderr=subprocess.STDOUT)
            end = time.monotonic() + 10
            while True:
                assert broker.poll() is None, 'broker failed before listen'
                try:
                    with socket.create_connection(('127.0.0.1', port), timeout=.2):
                        break
                except OSError:
                    assert time.monotonic() < end, 'broker listen deadline'
                    time.sleep(.05)
            proxy = Proxy(port, version, mode, proxy_host)
            if client.suffix.lower() == '.exe' and os.name != 'nt':
                assert windows_python, 'native Windows Python is required to verify WSL loopback forwarding readiness'
                # WSL publishes new localhost listeners asynchronously. Verify
                # TCP reachability from the actual client OS before starting
                # MQTT; never retry or mask a failed MQTT connection.
                code = ('import socket,time\nend=time.monotonic()+10\nwhile True:\n'
                        ' try:\n  s=socket.create_connection((' + repr(proxy_host) + ',' + str(proxy.port) + '),.2);s.close();break\n'
                        ' except OSError:\n  assert time.monotonic()<end,"cross-OS TCP readiness"\n  time.sleep(.05)\n')
                ready = subprocess.run([str(windows_python), '-c', code], capture_output=True, text=True, timeout=15)
                assert ready.returncode == 0, ready.stderr
                row['native_tcp_readiness_checked'] = True
            probe = subprocess.Popen([str(client), str(proxy.port), str(version), str(mode), str(fragmented), proxy_host], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding='utf-8')
            def read(source, lines, notify):
                try:
                    for line in source:
                        if sum(map(len, lines)) + len(line) > 32768:
                            raise ValueError('probe output cap')
                        lines.append(line)
                        if notify:
                            output.put_nowait(line.strip())
                except Exception as exc:
                    if proxy:
                        proxy.error('probe reader: ' + str(exc))
                finally:
                    if notify:
                        output.put_nowait(None)
            for source, lines, notify in [(probe.stdout, stdout, True), (probe.stderr, stderr, False)]:
                thread = threading.Thread(target=read, args=(source, lines, notify), daemon=True)
                readers.append(thread)
                thread.start()
            assert output.get(timeout=20) == 'WAIT_CUT', ''.join(stdout + stderr)
            proxy.wait(lambda: proxy.dropped and any(not r['forwarded'] for r in proxy.rows))
            probe.stdin.write('cut\n')
            probe.stdin.flush()
            assert output.get(timeout=20) == 'WAIT_DRAIN', ''.join(stdout + stderr)
            proxy.wait(proxy.recovered)
            probe.stdin.write('drain\n')
            probe.stdin.flush()
            probe.wait(timeout=25)
            assert probe.returncode == 0, ''.join(stdout + stderr)
            proxy.verify()
            row['passed'] = True
        except Exception as exc:
            error = type(exc).__name__ + ': ' + str(exc)
        finally:
            if probe:
                if probe.poll() is None:
                    probe.kill()
                probe.wait(timeout=10)
                for reader in readers:
                    reader.join(2)
                for source in [probe.stdin, probe.stdout, probe.stderr]:
                    source.close()
                row.update(probe_exit=probe.returncode, probe_reaped=probe.poll() is not None, readers_reaped=not any(t.is_alive() for t in readers))
            if proxy:
                try:
                    proxy.close()
                    row['proxy_reaped'] = True
                except Exception as exc:
                    error = (error + '; ' if error else '') + 'proxy cleanup: ' + str(exc)
                row.update(trace=proxy.rows, proxy_errors=proxy.errors)
            if broker:
                broker.terminate()
                try:
                    broker.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    broker.kill()
                    broker.wait(timeout=10)
                row.update(broker_pid=broker.pid, broker_exit=broker.returncode, broker_reaped=broker.poll() is not None)
    row.update(stdout=''.join(stdout), stderr=''.join(stderr), seconds=time.monotonic() - started,
               broker_log=log.name, broker_log_sha256=digest(log))
    if probe and not row['readers_reaped']:
        error = (error + '; ' if error else '') + 'probe reader cleanup'
    if error:
        row['error'] = error
        row['passed'] = False
    return row


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--broker', type=Path, required=True)
    parser.add_argument('--client', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--windows-python', type=Path, default=shutil.which('python.exe'),
                        help='Native Windows Python for WSL loopback readiness, only for .exe clients')
    parser.add_argument('--proxy-host', default='127.0.0.1', help='Numeric IPv4 address on the fixture host, also passed to the client')
    parser.add_argument('--versions', nargs='+', type=int, choices=[5, 311], default=[5, 311])
    parser.add_argument('--modes', nargs='+', type=int, choices=range(5), default=list(range(5)))
    parser.add_argument('--fragmented', nargs='+', type=int, choices=[0, 1], default=[0, 1])
    args = parser.parse_args()
    socket.inet_pton(socket.AF_INET, args.proxy_host)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    broker, client = args.broker.resolve(), args.client.resolve()
    version = subprocess.run([str(broker), '-h'], capture_output=True, text=True, timeout=15)
    assert 'mosquitto version 2.1.2' in version.stdout.splitlines(), 'pinned broker version'
    report = dict(schema=1, complete=False, broker_version='2.1.2', broker_sha256=digest(broker),
                  client_sha256=digest(client), runner_sha256=digest(__file__), broker_platform=platform.platform(),
                  client_platform='Windows native through WSL interoperability' if client.suffix.lower() == '.exe' and os.name != 'nt' else platform.platform(),
                  scope='Explicit volatile numeric TCP Session recovery; no broker restart/durable/TLS/WebSocket/automatic/performance/production acceptance', cases=[])
    try:
        for mqtt in args.versions:
            for mode in args.modes:
                for fragmented in args.fragmented:
                    name = f'{args.report.stem}-{mqtt}-{mode}-{fragmented}'
                    row = case(broker, client, mqtt, mode, fragmented, args.report.parent / 'logs', name, args.windows_python, args.proxy_host)
                    report['cases'].append(row)
                    args.report.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
                    print(mqtt, mode, fragmented, row['passed'], row.get('error', ''), flush=True)
                    assert row['passed'], row.get('error', 'case failed')
        report['complete'] = True
    finally:
        args.report.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
