"""Independent MQTT5 diagnostic close bytes, caps, partial frames and deadlines."""
import argparse
import hashlib
import json
from pathlib import Path
import socket
import struct
import subprocess
import threading
import time
from run_client_wire import packet, read_packet, read_exact, string, take_string, vbi


def incoming(version, identifier=1):
    return packet(0x32, string(b't') + identifier.to_bytes(2, 'big') + (b'\0' if version == 5 else b'') + b'x')


def serve(listener, version, case, errors):
    try:
        with listener.accept()[0] as s:
            s.settimeout(8)
            t, b = read_packet(s)
            protocol, at = take_string(b, 0)
            assert t == 0x10 and protocol == b'MQTT' and b[at:at+4] == bytes([version, 2, 0, 0])
            at += 4
            if version == 5:
                props = b'\x27\0\0\x10\0' if case == 'oversize' else b'\x21\0\x01' if case == 'window' else b''
                assert b[at:at+1+len(props)] == bytes([len(props)]) + props
                at += 1 + len(props)
            identity, at = take_string(b, at)
            assert identity == b'diag-probe' and at == len(b)
            props = b'\x27\0\0\0' + bytes([2 if case == 'cap_two' else 3]) if case in ('cap_two', 'cap_three') else b''
            connack = packet(0x20, b'\0\0' + (vbi(len(props)) + props if version == 5 else b''))
            reason = 0x81
            if case == 'before_ready':
                s.sendall(b'\0\0')
            elif case == 'window':
                reason = 0x93
                s.sendall(connack + incoming(version) + incoming(version, 2))
            elif case == 'partial_ack':
                s.sendall(connack + incoming(version) + b'\0\0')
            else:
                s.sendall(connack)
                if case == 'unexpected':
                    reason = 0x82
                    s.sendall(connack)
                elif case == 'alias':
                    reason = 0x94
                    s.sendall(packet(0x30, string(b't') + b'\x03\x23\0\x01x'))
                elif case == 'oversize':
                    reason = 0x95
                    s.sendall(b'\x30' + vbi(5000))
                elif case == 'server_close':
                    s.sendall(b'\xe0\x01\x89')
                elif case in ('partial_publish', 'native_queue'):
                    assert read_exact(s, 1) == b'\x30'
                    s.sendall(b'\0\0')
                    if case == 'native_queue':
                        time.sleep(.3)  # preserve native backpressure past 50ms
                        s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4 * 1024 * 1024)
                elif case != 'explicit_close':
                    s.sendall(b'\0\0')
            if case in ('partial_publish', 'native_queue'):
                received = bytearray(b'\x30')
                try:
                    while True:
                        b = s.recv(65536)
                        if not b:
                            break
                        received.extend(b)
                except ConnectionResetError:
                    pass
                n = 1024 * 1024 if case == 'partial_publish' else 16 * 1024 * 1024
                expected = packet(0x30, string(b'outgoing') + (b'\0' if version == 5 else b'') + b'p' * n)
                assert received, 'publication prefix'
                if len(received) < len(expected):
                    assert received == expected[:len(received)], 'diagnostic interleaved with incomplete publication'
                else:
                    tail = received[len(expected):]
                    assert received[:len(expected)] == expected and len(tail) <= 3 and tail == b'\xe0\x01\x81'[:len(tail)], 'invalid tail after complete publication'
                return
            if case == 'partial_ack':
                received = bytearray()
                try:
                    while True:
                        b = s.recv(32)
                        if not b:
                            break
                        received.extend(b)
                except ConnectionResetError:
                    pass
                assert len(received) < 4 and received == b'\x40\x02\0\x01'[:len(received)], 'diagnostic interleaved with partial PUBACK'
                return
            if version == 5 and case not in ('before_ready', 'cap_two', 'explicit_close', 'server_close', 'deadline', 'abort_drain'):
                if case == 'peer_close':
                    assert read_exact(s, 1) == b'\xe0'
                    s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))
                    return
                assert read_packet(s) == (0xe0, bytes([reason])), 'minimal final diagnostic reason'
            try:
                assert s.recv(1) == b'', 'packet after final DISCONNECT or abort'
            except ConnectionResetError:
                pass
    except BaseException as e:
        errors.append(repr(e))


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--probe', type=Path, required=True)
    p.add_argument('--report', type=Path, required=True)
    a = p.parse_args()
    rows = []
    common = ['malformed', 'one_byte', 'explicit_close', 'before_ready', 'partial_publish', 'partial_ack']
    for version in (4, 5):
        for case in common + (['unexpected', 'alias', 'oversize', 'window', 'cap_two', 'cap_three', 'native_queue', 'deadline', 'abort_drain', 'server_close', 'peer_close'] if version == 5 else []):
            errors = []
            with socket.socket() as listener:
                listener.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
                listener.bind(('127.0.0.1', 0))
                listener.listen(1)
                listener.settimeout(10)
                thread = threading.Thread(target=serve, args=(listener, version, case, errors), daemon=True)
                thread.start()
                r = subprocess.run([str(a.probe.resolve()), str(listener.getsockname()[1]), str(version), case], capture_output=True, text=True, timeout=15)
                thread.join(10)
                row = dict(version=version, case=case, exit_code=r.returncode, stdout=r.stdout, stderr=r.stderr, peer_errors=errors, peer_alive=thread.is_alive())
                rows.append(row)
                a.report.parent.mkdir(parents=True, exist_ok=True)
                a.report.write_text(json.dumps(dict(complete=False, cases=rows), indent=2) + '\n')
                assert r.returncode == 0 and not errors and not thread.is_alive(), row
                print('PASS', version, case, flush=True)
    a.report.write_text(json.dumps(dict(complete=True, probe_sha256=hashlib.sha256(a.probe.read_bytes()).hexdigest(), cases=rows), indent=2) + '\n')


if __name__ == '__main__':
    main()
