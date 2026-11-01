"""Independent stdlib TCP peer: byte-exact MQTT5/311 and hostile stream cases.

Does not import the library's codecs or another MQTT implementation. This is
wire interoperability/fault evidence, not Mosquitto parity or a benchmark.
"""
import argparse
import hashlib
import json
from pathlib import Path
import socket
import subprocess
import threading
import time


def vbi(n):
    out = bytearray()
    while True:
        b = n % 128
        n //= 128
        out.append(b | (128 if n else 0))
        if not n:
            return bytes(out)


def string(b):
    return len(b).to_bytes(2, 'big') + b


def packet(t, body):
    return bytes([t]) + vbi(len(body)) + body


def read_exact(s, n):
    out = bytearray()
    while len(out) < n:
        b = s.recv(n - len(out))
        if not b:
            raise EOFError(f'EOF at {len(out)}/{n}')
        out += b
    return bytes(out)


def read_packet(s):
    first = read_exact(s, 1)[0]
    n = 0
    for i in range(4):
        b = read_exact(s, 1)[0]
        n += (b & 127) << (7 * i)
        if not b & 128:
            return first, read_exact(s, n)
    raise AssertionError('oversized VBI')


def take_string(b, at):
    n = int.from_bytes(b[at:at+2], 'big')
    assert at + 2 + n <= len(b)
    return b[at+2:at+2+n], at+2+n


def check_connect(s, version, case):
    t, b = read_packet(s)
    assert t == 0x10
    protocol, at = take_string(b, 0)
    assert protocol == b'MQTT' and b[at] == version and b[at+1] == 0xc2
    assert b[at+2:at+4] == b'\0\0'
    at += 4
    if version == 5:
        n = b[at]
        properties = b[at+1:at+1+n]
        assert properties == (b'\x27\0\0\x10\0' if case == 'advertised_limit' else b'')
        at += 1 + n
    identity, at = take_string(b, at)
    username, at = take_string(b, at)
    password, at = take_string(b, at)
    assert identity == (b'' if case == 'assigned' else b'probe-copy')
    assert username == b'user-copy' and password == b'password-copy' and at == len(b)


def publish(version, payload=b'first\0packet', properties=b''):
    return packet(0x30, string(b'wire/test') + (vbi(len(properties)) + properties if version == 5 else b'') + payload)


def serve(listener, version, case, errors):
    try:
        with listener.accept()[0] as s:
            s.settimeout(8)
            check_connect(s, version, case)
            if case == 'setup_timeout':
                try:
                    assert s.recv(1) == b''
                except ConnectionResetError:
                    pass
                return
            props = b'\x12' + string(b'assigned-wire') if case == 'assigned' else b''
            ack = packet(0x20, bytes([0, 0x87 if case == 'refuse' and version == 5 else 5 if case == 'refuse' else 0]) + (vbi(len(props)) + props if version == 5 else b''))
            if case == 'before_connack':
                s.sendall(b'\xd0\0')
            elif case == 'roundtrip':
                for b in ack:
                    s.sendall(bytes([b]))
                    time.sleep(0.001)
                props = b'\x26' + string(b'key') + string(b'value') + b'\x09' + string(b'a\0b') if version == 5 else b''
                s.sendall(publish(version, b'first\0packet', props) + publish(version, b'p' * 4096))
                t, b = read_packet(s)
                topic, at = take_string(b, 0)
                assert t == 0x30 and topic == b'wire/test'
                if version == 5:
                    assert b[at] == 0
                    at += 1
                assert b[at:] == b'p' * 7
                s.sendall(publish(version, b[at:]))
            else:
                s.sendall(ack)
                if case == 'duplicate':
                    s.sendall(ack)
                elif case == 'malformed':
                    s.sendall(b'\x30\x80\x80\x80\x80')
                elif case == 'local_limit':
                    s.sendall(b'\x30' + vbi(9000))
                elif case == 'advertised_limit':
                    s.sendall(b'\x30' + vbi(5000))
                elif case == 'receive_limit':
                    s.sendall(publish(version) * 2)
                elif case == 'object_limit':
                    s.sendall(publish(version))
                elif case == 'private_limit':
                    s.sendall(publish(version, b'a' * 6000) + publish(version, b'b' * 7000))
                elif case == 'alias':
                    s.sendall(publish(version, properties=b'\x23\0\x01'))
                elif case == 'qos1':
                    s.sendall(packet(0x32, string(b'wire/test') + b'\0\x01' + (b'\0' if version == 5 else b'') + b'payload'))
                    assert read_packet(s) == (0x40, b'\0\x01')
                    s.sendall(publish(version, b'ack-observed'))
                elif case == 'disconnect':
                    s.sendall(packet(0xe0, b'\x89'))
                elif case == 'unsolicited_auth':
                    s.sendall(b'\xf0\0')
                elif case == 'partial_close':
                    assert read_exact(s, 1) == b'\x30'
                    return
                elif case == 'truncated':
                    s.sendall(b'\x30\x10\0')
                    s.shutdown(socket.SHUT_WR)
            reason = {'duplicate': 0x82, 'malformed': 0x81, 'alias': 0x94,
                      'advertised_limit': 0x95, 'unsolicited_auth': 0x82}.get(case)
            if version == 5 and reason is not None:
                assert read_packet(s) == (0xe0, bytes([reason])), 'final diagnostic DISCONNECT'
            try:
                assert s.recv(1) == b'', 'unexpected client bytes/replay'
            except ConnectionResetError:
                pass
    except BaseException as e:
        errors.append(repr(e))


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--probe', type=Path, required=True)
    p.add_argument('--report', type=Path, required=True)
    a = p.parse_args()
    common = [('roundtrip', 0, 4), ('refuse', 1, 1), ('setup_timeout', 2, 2),
              ('before_connack', 6, 6), ('duplicate', 0, 6), ('malformed', 0, 6),
              ('local_limit', 0, 7), ('receive_limit', 0, 7), ('object_limit', 0, 7),
              ('private_limit', 0, 7), ('qos1', 0, 4), ('truncated', 0, 5),
              ('partial_close', 0, 5)]
    rows = []
    for version in [4, 5]:
        cases = common + ([('assigned', 0, 4), ('alias', 0, 6), ('disconnect', 0, 12),
                           ('unsolicited_auth', 0, 6), ('advertised_limit', 0, 6)] if version == 5 else [])
        for case, ready, close in cases:
            errors = []
            with socket.socket() as listener:
                listener.bind(('127.0.0.1', 0))
                listener.listen(1)
                listener.settimeout(10)
                port = listener.getsockname()[1]
                thread = threading.Thread(target=serve, args=(listener, version, case, errors), daemon=True)
                thread.start()
                command = [str(a.probe.resolve()), str(port), str(version), case, str(ready), str(close)]
                r = subprocess.run(command, capture_output=True, text=True, timeout=15)
                thread.join(10)
                row = {'version': version, 'case': case, 'exit_code': r.returncode,
                       'stdout': r.stdout, 'stderr': r.stderr, 'peer_errors': errors, 'peer_alive': thread.is_alive()}
                rows.append(row)
                a.report.parent.mkdir(parents=True, exist_ok=True)
                a.report.write_text(json.dumps({'complete': False, 'cases': rows}, indent=2)+'\n')
                assert r.returncode == 0 and not errors and not thread.is_alive(), row
                print('PASS', version, case, flush=True)
    a.report.write_text(json.dumps({'complete': True, 'probe_sha256': hashlib.sha256(a.probe.read_bytes()).hexdigest(), 'cases': rows}, indent=2)+'\n')


if __name__ == '__main__':
    main()
