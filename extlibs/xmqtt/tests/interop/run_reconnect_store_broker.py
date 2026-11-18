"""Verify durable automatic reconnect against Mosquitto through independent gateways.

The broker owns MQTT state. Plain TLS and WebSocket terminate at separate
loopback gateways. Reports include original and replay packet identities, the
subscriber's payloads and graceful transport close of the replacement.
"""
import argparse
from itertools import product
import json
import os
from pathlib import Path
import select
import shutil
import socket
import ssl
import subprocess
import tempfile
import time

from reconnect_ws_gateway import WebSocketProxy, bind_front
from run_mqtt_tls import certificates, digest
from run_session_broker import Proxy, packet
from run_session_tls_broker import contexts
from session_tls_proxy import TLSProxy


LOOPBACK = '127.0.0.1'
TRANSPORTS = ('tcp', 'tls12', 'tls13', 'ws', 'wss12', 'wss13')
FAULTS = ('puback', 'pubrec', 'pubcomp')
TOPIC = b'xrt/example/reconnect-store'
PAYLOAD = b'durable automatic reconnect'


def field(data):
    return len(data).to_bytes(2, 'big') + data


def send(sock, header, data):
    assert len(data) < 128
    sock.sendall(bytes((header, len(data))) + data)


def connect_subscriber(broker, port):
    end = time.monotonic() + 10
    while True:
        assert broker.poll() is None, 'broker exited during setup'
        try:
            sink = socket.create_connection((LOOPBACK, port), .3)
        except OSError:
            assert time.monotonic() < end, 'broker setup deadline'
            time.sleep(.05)
            continue
        return sink


def subscribe(sock):
    sock.settimeout(8)
    # Use independent MQTT 3.1.1 bytes to inspect delivered data for both clients.
    send(sock, 0x10, b'\0\x04MQTT\x04\x02\0\0' + field(b'durable-reconnect-readback'))
    assert packet(sock)[1] == b'\0\0'
    send(sock, 0x82, b'\0\1' + field(TOPIC) + b'\0')
    assert packet(sock)[1] == b'\0\1\0'


def readiness(windows_python, port):
    if windows_python:
        # WSL forwards listeners into Windows asynchronously. Test TCP
        # reachability without introducing an MQTT, TLS or WS connection.
        probe = """
import socket, sys, time
end = time.monotonic() + 10
while True:
    try:
        with socket.create_connection(('127.0.0.1', int(sys.argv[1])), .25):
            pass
        break
    except OSError:
        if time.monotonic() >= end:
            raise
        time.sleep(.05)
"""
        subprocess.run([str(windows_python), '-c', probe, str(port)], check=True, timeout=15)


def gateway(transport, backend, mqtt, fault, root, windows_python):
    if transport == 'tcp' or transport.startswith('tls'):
        listener, _ = bind_front(LOOPBACK, windows_python)
        try:
            if transport == 'tcp':
                return Proxy(backend, mqtt, fault, listener=listener)
            tls = int(transport[-2:])
            ciphers = ('TLS_AES_128_GCM_SHA256', 'TLS_AES_256_GCM_SHA384',
                       'TLS_CHACHA20_POLY1305_SHA256') if tls == 13 else ()
            return TLSProxy(backend, mqtt, fault, LOOPBACK, contexts(root, tls, False), tls,
                            accepted_ciphers=ciphers, listener=listener)
        except BaseException:
            listener.close()
            raise
    tls = int(transport[-2:]) if transport.startswith('wss') else 0
    return WebSocketProxy(backend, mqtt, fault, LOOPBACK, tls=tls, split=True,
                          cert=root / 'server.pem', key=root / 'server.key', windows_python=windows_python)


def command(client, transport, port, mqtt, fault, ca):
    version = str(mqtt)
    if transport == 'tcp':
        arguments = [LOOPBACK, str(port), version]
    elif transport.startswith('tls'):
        arguments = ['tls', 'localhost', str(port), version, ca]
    else:
        scheme = 'ws' if transport == 'ws' else 'wss'
        arguments = [f'{scheme}://{LOOPBACK}:{port}/mqtt?managed=1', version]
        if scheme == 'wss':
            arguments.append(ca)
    return [str(client), *arguments, 'expect-replay' if fault == 0 else 'replay-qos2']


def cut(proxy):
    # The client still holds generation one's lease. Retire its real transport
    # after the independent observer captures the withheld acknowledgement.
    sockets = tuple(getattr(proxy, 'front_sockets', proxy.connections))
    for sock in sockets:
        try:
            sock.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass


def verify(proxy, sink, transport, fault, output):
    assert not proxy.errors, proxy.errors
    rows = proxy.rows
    acks = [r for r in rows if r['type'] == 2]
    assert [r['session_present'] for r in acks] == [False, True], rows
    assert all(r['reason'] == 0 for r in acks), rows
    drops = [r for r in rows if not r['forwarded']]
    assert len(drops) == 1 and drops[0]['generation'] == 1, rows
    assert drops[0]['direction'] == 'sc' and drops[0]['type'] == (4, 5, 7)[fault], rows
    publishes = [r for r in rows if r['type'] == 3 and r['direction'] == 'cs']
    first = publishes[0]
    identifier = first['identifier']
    original_flags = 2 if fault == 0 else 4
    assert first['topic'] == TOPIC.decode() and first['generation'] == 1, rows
    if fault == 2:
        assert len(publishes) == 1 and first['flags'] == original_flags, rows
    else:
        assert [r['flags'] for r in publishes] == [original_flags, original_flags | 8], rows
        assert [r['generation'] for r in publishes] == [1, 2], rows
        assert publishes[1]['identifier'] == identifier and publishes[1]['topic'] == first['topic'], rows
    if fault:
        releases = [r for r in rows if r['type'] == 6 and r['direction'] == 'cs']
        assert [r['generation'] for r in releases] == ([1, 2] if fault == 2 else [2]), rows
        assert all(r['flags'] == 2 and r['identifier'] == identifier for r in releases), rows
    assert any(r['direction'] == 'sc' and r['generation'] == 2 and r['forwarded'] and
               r['type'] == (7 if fault else 4) and r['identifier'] == identifier for r in rows), rows
    assert any(r['type'] == 14 and r['direction'] == 'cs' and r['generation'] == 2 for r in rows), rows
    for thread in getattr(proxy, 'front_threads', proxy.threads):
        thread.join(10)
        assert not thread.is_alive(), 'transport retirement'
    assert not proxy.errors, proxy.errors
    if transport.startswith('tls'):
        assert len(proxy.observations['handshakes']) == 2
        close = next(r for r in proxy.observations['closes'] if r['generation'] == 2)
        assert close['notify'], proxy.observations
    elif transport != 'tcp':
        observations = proxy.observations
        assert len(observations) == 2 and len({r['key_sha256'] for r in observations}) == 2, observations
        assert all(r['upgraded'] and r['client_bytes'] and r['server_bytes'] for r in observations), observations
        close = next(r for r in observations if r['generation'] == 2)
        assert close['close_received'] and close['close_sent'] and close['pings'] == close['pongs'], close
        if transport.startswith('wss'):
            assert close['authenticated_tls_close'], close
    deliveries = 1 if fault else 2
    for _ in range(deliveries):
        raw, body = packet(sink)
        assert raw[0] == 0x30 and body == field(TOPIC) + PAYLOAD, (raw.hex(), body.hex())
    ready, _, _ = select.select([sink], [], [], .1)
    assert not ready, 'unexpected extra delivery'
    assert f'Generation 2 settled durable QoS {2 if fault else 1}.' in output, output
    assert f'Committed graceful END at {7 if fault else 6}.' in output, output
    if fault:
        assert f'Original PUBREL checkpoint {3 if fault == 2 else 0}.' in output, output
    return deliveries


def case(args, client, transport, mqtt, fault, root, name):
    # A new broker process gives the fixed client id a fresh first session.
    with socket.socket() as seed:
        seed.bind((LOOPBACK, 0))
        port = seed.getsockname()[1]
    config = root / 'broker.conf'
    as_root = 'user root\n' if hasattr(os, 'geteuid') and os.geteuid() == 0 else ''
    config.write_text(as_root + f'allow_anonymous true\npersistence false\nlog_dest stdout\n'
                      f'log_type all\nlistener {port} {LOOPBACK}\n', encoding='utf-8')
    on_windows = client.suffix.lower() == '.exe' and os.name != 'nt'
    windows_python = args.windows_python if on_windows else None
    ca = str(root / 'ca.pem')
    if on_windows:
        assert windows_python, 'supply --windows-python for native Windows clients under WSL'
        ca = subprocess.check_output(['wslpath', '-w', ca], text=True, timeout=10).strip()
    logs = args.report.parent / (args.report.stem + '-logs')
    logs.mkdir(parents=True, exist_ok=True)
    server_log = logs / (name + '-broker.log')
    client_log = logs / (name + '-client.log')
    with server_log.open('wb') as log:
        server = subprocess.Popen([str(args.broker), '-c', str(config)], stdout=log, stderr=subprocess.STDOUT)
        try:
            with connect_subscriber(server, port) as sink:
                subscribe(sink)
                proxy = gateway(transport, port, mqtt, fault, root, windows_python)
                try:
                    readiness(windows_python, proxy.port)
                    with client_log.open('wb') as log:
                        worker = subprocess.Popen(command(client, transport, proxy.port, mqtt, fault, ca),
                                                  stdout=log, stderr=subprocess.STDOUT)
                        try:
                            proxy.wait(lambda: any(not r['forwarded'] for r in proxy.rows) or worker.poll() is not None)
                            assert worker.poll() is None, client_log.read_text(encoding='utf-8')
                            cut(proxy)
                            exit_code = worker.wait(timeout=22)
                            output = client_log.read_text(encoding='utf-8')
                            assert exit_code == 0, (output, proxy.rows)
                        finally:
                            if worker.poll() is None:
                                worker.kill()
                                worker.wait(timeout=5)
                    proxy.wait(lambda:any(r['type'] == 14 for r in proxy.rows))
                    received = verify(proxy, sink, transport, fault, output)
                finally:
                    proxy.close()
                result = dict(trace=proxy.rows, observations=getattr(proxy, 'observations', None),
                              subscriber_deliveries=received, output=output.splitlines(), client_log=digest(client_log),
                              proxy_threads_reaped=True)
        finally:
            server.terminate()
            try:
                server.wait(timeout=5)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait(timeout=5)
    result['broker_log'] = digest(server_log)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--broker', required=True, type=Path)
    parser.add_argument('--form', action='append', nargs=2, metavar=('LABEL', 'PROGRAM'), required=True)
    parser.add_argument('--report', required=True, type=Path)
    parser.add_argument('--windows-python', type=Path, default=shutil.which('python.exe'))
    parser.add_argument('--openssl', default=shutil.which('openssl'))
    parser.add_argument('--transport', nargs='+', choices=TRANSPORTS, default=TRANSPORTS)
    parser.add_argument('--mqtt', nargs='+', choices=(311, 5), type=int, default=(311, 5))
    parser.add_argument('--fault', nargs='+', choices=FAULTS, default=FAULTS)
    args = parser.parse_args()
    forms = {}
    for label, program in args.form:
        if not label or label in forms or any(not (c.isascii() and (c.isalnum() or c in '-_')) for c in label):
            parser.error('form labels must be unique and use ASCII alphanumerics, hyphens or underscores')
        forms[label] = Path(program).resolve(strict=True)
    args.broker = args.broker.resolve(strict=True)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    broker = subprocess.run([str(args.broker), '-h'], capture_output=True, text=True, check=True, timeout=8)
    version = broker.stdout.splitlines()[0]
    report = dict(schema=1, complete=False, broker=version, broker_sha256=digest(args.broker), ssl=ssl.OPENSSL_VERSION,
                  forms={label: dict(path=str(program), sha256=digest(program)) for label, program in forms.items()},
                  sources={file: digest(Path(__file__).with_name(file)) for file in
                           (Path(__file__).name, 'reconnect_ws_gateway.py', 'session_tls_proxy.py',
                            'run_session_broker.py', 'run_session_tls_broker.py', 'run_mqtt_tls.py')},
                  cases=[])
    try:
        with tempfile.TemporaryDirectory(prefix='xrt-durable-reconnect-') as scratch:
            root = Path(scratch)
            if any(t.startswith(('tls', 'wss')) for t in args.transport):
                if not args.openssl:
                    parser.error('OpenSSL is required for the secure transports')
                report['public_certificates'] = certificates(root, args.openssl)
            for label, transport, mqtt, dropped in product(forms, args.transport, args.mqtt, args.fault):
                name = f'{label}-{transport}-{mqtt}-{dropped}'
                row = dict(name=name, form=label, transport=transport, mqtt=mqtt, drop=dropped, passed=False)
                report['cases'].append(row)
                try:
                    row.update(case(args, forms[label], transport, mqtt, FAULTS.index(dropped), root, name))
                    row['passed'] = True
                    print(name, 'passed', flush=True)
                except Exception as error:
                    row['error'] = repr(error)
                    raise
        report['complete'] = True
    finally:
        args.report.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')


if __name__ == '__main__':
    main()
