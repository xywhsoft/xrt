"""Independent numeric TCP subscription reconciliation, including one lost SUBACK."""
from pathlib import Path
import argparse
import getpass
import itertools
import json
import os
import platform
import shutil
import socket
import subprocess
import threading
import time
from run_session_broker import Proxy, digest, packet, varint


def text(data, offset):
    size = int.from_bytes(data[offset:offset + 2], 'big')
    end = offset + 2 + size
    assert end <= len(data), 'truncated string'
    return data[offset + 2:end].decode('utf-8'), end


class SubscriptionProxy(Proxy):
    def __init__(self, backend, version, persistent, fragmented, cut, host):
        self.persistent, self.fragmented, self.cut = persistent, fragmented, cut
        super().__init__(backend, version, 0, host)

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
            frontend.settimeout(20)
            try:
                if not frontend.recv(1, socket.MSG_PEEK):
                    frontend.close()
                    continue
                generation += 1
                assert generation <= 6, 'connection cap'
                backend = socket.create_connection(('127.0.0.1', self.backend), timeout=10)
                backend.settimeout(20)
                self.connections.append(backend)
                for sock in (frontend, backend):
                    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                for source, target, direction in [(frontend, backend, 'cs'), (backend, frontend, 'sc')]:
                    worker = threading.Thread(target=self.relay, args=(source, target, direction, generation), daemon=True)
                    self.threads.append(worker)
                    worker.start()
            except Exception as exc:
                self.error(str(exc))
                break

    def relay(self, source, target, direction, generation):
        try:
            while not self.stop.is_set():
                raw, body = packet(source)
                kind = raw[0] >> 4
                row = dict(generation=generation, direction=direction, type=kind, flags=raw[0] & 15, bytes=len(raw), forwarded=True)
                if kind == 1:
                    size = int.from_bytes(body[:2], 'big')
                    row['clean'] = bool(body[3 + size] & 2)
                elif kind == 2:
                    row.update(session_present=bool(body[0] & 1), reason=body[1])
                elif kind in (8, 10):
                    row['identifier'] = int.from_bytes(body[:2], 'big')
                    offset = 2
                    row['subscription_id'], row['users'] = None, []
                    if self.version == 5:
                        size, offset = varint(body, offset)
                        end = offset + size
                        while offset < end:
                            identifier, offset = varint(body, offset)
                            if identifier == 11:
                                row['subscription_id'], offset = varint(body, offset)
                            else:
                                assert identifier == 38, 'unexpected control property'
                                name, offset = text(body, offset)
                                value, offset = text(body, offset)
                                row['users'].append([name, value])
                        assert offset == end
                    row['filters'] = []
                    while offset < len(body):
                        name, offset = text(body, offset)
                        option = body[offset] if kind == 8 else None
                        offset += kind == 8
                        row['filters'].append([name, option])
                with self.cv:
                    if self.cut and generation == 1 and direction == 'sc' and kind == 9 and not self.dropped:
                        row['forwarded'] = False
                        self.dropped = True
                    assert len(self.rows) < 512, 'trace cap'
                    row['order'] = len(self.rows)
                    self.rows.append(row)
                if not row['forwarded']:
                    break
                if self.fragmented:
                    for offset in range(0, len(raw), 3):
                        target.sendall(raw[offset:offset + 3])
                else:
                    target.sendall(raw)
        except EOFError:
            pass
        except OSError as exc:
            if not self.stop.is_set() and not isinstance(exc, (ConnectionResetError, BrokenPipeError)):
                self.error(type(exc).__name__ + ': ' + str(exc))
        except Exception as exc:
            if not self.stop.is_set():
                self.error(type(exc).__name__ + ': ' + str(exc))
        finally:
            for sock in (source, target):
                try:
                    sock.shutdown(socket.SHUT_RDWR)
                except OSError:
                    pass

    def verify(self, result):
        assert not self.errors, self.errors
        connacks = [r for r in self.rows if r['type'] == 2]
        assert len(connacks) == result['connections'] == 3 + bool(self.cut)
        assert [r['session_present'] for r in connacks] == [False] + [bool(self.persistent)] * (len(connacks) - 1)
        assert all(r['reason'] == 0 for r in connacks)
        connects = [r for r in self.rows if r['type'] == 1]
        assert [r['clean'] for r in connects] == [not (self.version == 311 and self.persistent)] + [not self.persistent] * (len(connects) - 1)
        assert len([r for r in self.rows if not r['forwarded']]) == bool(self.cut)
        controls = [r for r in self.rows if r['direction'] == 'cs' and r['type'] in (8, 10)]
        assert len(controls) == result['requests']
        last = result['connections']
        removals = [r for r in controls if r['type'] == 10]
        assert len(removals) == bool(self.persistent)
        if removals:
            assert removals[0]['generation'] == last and removals[0]['filters'] == [['intent/a/+', None]]
        for row in controls:
            assert row['identifier'] and row['flags'] == 2 and len(row['filters']) == 1
            if row['type'] == 8:
                name, option = row['filters'][0]
                assert option == {'intent/a/+': 0x21 if self.version == 5 else 1, 'intent/b/+': 2, 'intent/c/+': 1}[name]
                if self.version == 5:
                    assert row['subscription_id'] == (99 if name == 'intent/c/+' else 77)
                    assert row['users'] == [['source', 'snapshot']]
        if self.persistent:
            known_resume = (2 if self.cut else 1) + 1
            assert not any(r['generation'] == known_resume for r in controls), 'known resumed filters must not be repeated'


def case(args, version, persistent, fragmented, cut):
    directory = args.report.parent / 'logs'
    directory.mkdir(parents=True, exist_ok=True)
    name = f'{args.report.stem}-{version}-{persistent}-{fragmented}-{cut}'
    with socket.socket() as seed:
        seed.bind(('127.0.0.1', 0))
        port = seed.getsockname()[1]
    config, log = directory / (name + '.conf'), directory / (name + '.log')
    config.write_text(('user ' + getpass.getuser() + '\n' if hasattr(os, 'geteuid') and os.geteuid() == 0 else '') + f'persistence false\nlog_dest stdout\nlog_type all\nset_tcp_nodelay true\nallow_anonymous true\nlistener {port} 127.0.0.1\n', encoding='utf-8')
    row = dict(version=version, persistent=persistent, fragmented=fragmented, cut=cut, passed=False)
    broker = proxy = probe = None
    start = time.monotonic()
    with log.open('w', encoding='utf-8') as stream:
        try:
            broker = subprocess.Popen([str(args.broker), '-c', str(config.resolve())], stdout=stream, stderr=subprocess.STDOUT)
            end = time.monotonic() + 10
            while True:
                assert broker.poll() is None, 'broker exited before listen'
                try:
                    with socket.create_connection(('127.0.0.1', port), .2):
                        break
                except OSError:
                    assert time.monotonic() < end, 'broker listen deadline'
                    time.sleep(.05)
            proxy = SubscriptionProxy(port, version, persistent, fragmented, cut, args.proxy_host)
            if args.client.suffix.lower() == '.exe' and os.name != 'nt':
                assert args.windows_python, 'native Python required for actual Windows reachability'
                code = f'import socket,time\nend=time.monotonic()+10\nwhile True:\n try:\n  s=socket.create_connection(({args.proxy_host!r},{proxy.port}),.2);s.close();break\n except OSError:\n  assert time.monotonic()<end,"cross-OS TCP readiness"\n  time.sleep(.05)\n'
                ready = subprocess.run([str(args.windows_python), '-c', code], capture_output=True, encoding='utf-8', timeout=15)
                assert ready.returncode == 0, ready.stderr
                row['native_tcp_readiness_checked'] = True
            command = [str(args.client), args.proxy_host, str(proxy.port), str(version), str(persistent), str(fragmented), str(cut)]
            row['command'] = command
            probe = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding='utf-8')
            stdout, stderr = probe.communicate(timeout=60)
            row.update(stdout=stdout, stderr=stderr, probe_exit=probe.returncode)
            assert probe.returncode == 0, stdout + stderr
            result = json.loads(stdout.strip().splitlines()[-1])
            assert result['passed']
            proxy.verify(result)
            row.update(passed=True, observation=result)
        except Exception as exc:
            row['error'] = type(exc).__name__ + ': ' + str(exc)
        finally:
            if probe:
                if probe.poll() is None:
                    probe.kill()
                    stdout, stderr = probe.communicate(timeout=10)
                    row.update(stdout=stdout, stderr=stderr, probe_exit=probe.returncode)
                row['probe_reaped'] = probe.poll() is not None
                for pipe in (probe.stdout, probe.stderr):
                    pipe.close()
            if proxy:
                try:
                    proxy.close()
                    row['proxy_reaped'] = True
                except Exception as exc:
                    row['passed'] = False
                    row['cleanup_error'] = str(exc)
                row.update(trace=proxy.rows, proxy_errors=proxy.errors)
            if broker:
                broker.terminate()
                try:
                    broker.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    broker.kill()
                    broker.wait(timeout=10)
                row.update(broker_reaped=broker.poll() is not None, broker_exit=broker.returncode)
    row.update(seconds=round(time.monotonic() - start, 4), broker_log=log.name, broker_log_sha256=digest(log), config_sha256=digest(config))
    return row


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--broker', required=True, type=Path)
    parser.add_argument('--client', required=True, type=Path)
    parser.add_argument('--report', required=True, type=Path)
    parser.add_argument('--proxy-host', default='127.0.0.1')
    parser.add_argument('--windows-python', type=Path, default=shutil.which('python.exe'))
    args = parser.parse_args()
    args.broker, args.client, args.report = args.broker.resolve(), args.client.resolve(), args.report.resolve()
    version = subprocess.run([str(args.broker), '-h'], capture_output=True, encoding='utf-8', timeout=15)
    assert 'mosquitto version 2.1.2' in version.stdout
    report = dict(schema=1, complete=False, scope='Volatile numeric TCP subscription intent: fresh/resumed sessions, options/properties/routing/offline removals and one lost SUBACK; controlled graceful OK/CLOSED retries are explicit. No TLS/DNS/Auth/durable/WebSocket/performance/production acceptance.', platform=platform.platform(), broker_sha256=digest(args.broker), client_sha256=digest(args.client), runner_sha256=digest(__file__), cases=[])
    args.report.parent.mkdir(parents=True, exist_ok=True)
    try:
        for values in itertools.product((5, 311), (0, 1), (0, 1), (0, 1)):
            row = case(args, *values)
            report['cases'].append(row)
            args.report.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
            print(*values, row['passed'], row.get('error', ''), flush=True)
            assert row['passed'], row.get('error', row.get('cleanup_error', 'case failed'))
        report['complete'] = True
    finally:
        args.report.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
