"""Independent byte-exact QoS1 receive peer; no library codec or broker source."""
import argparse
import hashlib
import json
from pathlib import Path
import socket
import subprocess
import threading
from run_client_wire import packet, read_packet, read_exact, string, take_string, vbi


def publish(version, identifier=1, duplicate=False, qos=1):
    props = b'\x26' + string(b'key') + string(b'value') + b'\x09' + string(b'\0x')
    return packet(0x30 | qos << 1 | (8 if duplicate else 0), string(b'incoming') + (identifier.to_bytes(2, 'big') if qos else b'') + (vbi(len(props)) + props if version == 5 else b'') + (b'received' if qos else b'done'))


def ack(s, identifier):
    assert read_packet(s) == (0x40, identifier.to_bytes(2, 'big')), 'ordered minimal PUBACK'


def serve(listener, version, case, errors):
    try:
        with listener.accept()[0] as s:
            s.settimeout(8)
            t, b = read_packet(s)
            protocol, at = take_string(b, 0)
            assert t == 0x10 and protocol == b'MQTT' and b[at:at+4] == bytes([version, 2, 0, 0])
            at += 4
            if version == 5:
                props = b'\x21\0\x01' if case == 'receive_window' else b''
                assert b[at:at+1+len(props)] == bytes([len(props)]) + props
                at += 1 + len(props)
            identity, at = take_string(b, at)
            assert identity == b'receive-probe' and at == len(b)
            props = b'\x27\0\0\0\x03' if case == 'restricted' else b''
            connack = packet(0x20, b'\0\0' + (vbi(len(props)) + props if version == 5 else b''))
            if case == 'receive_window':
                s.sendall(connack + publish(version, 1) + publish(version, 2))
            else:
                s.sendall(connack)
                if case == 'duplex':
                    t, b = read_packet(s)
                    topic, at = take_string(b, 0)
                    assert t == 0x32 and topic == b'outgoing' and b[at:at+2] == b'\0\x07'
                    s.sendall(publish(version, 7) + packet(0x40, b'\0\x07'))
                    ack(s, 7)
                elif case == 'partial_outgoing':
                    assert read_exact(s, 1) == b'\x30'
                    s.sendall(publish(version, 1))
                    n = 0
                    for i in range(4):
                        b = read_exact(s, 1)[0]
                        n += (b & 127) << (7 * i)
                        if not b & 128:
                            break
                    b = read_exact(s, n)
                    topic, at = take_string(b, 0)
                    assert topic == b'outgoing'
                    assert b[at:] == (b'\0' if version == 5 else b'') + b'p' * 1024, 'PUBACK interleaved into PUBLISH'
                    ack(s, 1)
                elif case in ('ordered', 'tiny', 'partial_ack', 'backpressure'):
                    wire = b''.join(publish(version, i) for i in (65535, 1, 42))
                    if case == 'tiny':
                        for b in wire:
                            s.sendall(bytes([b]))
                    else:
                        s.sendall(wire)
                    for i in (65535, 1, 42):
                        ack(s, i)
                elif case == 'duplicate':
                    s.sendall(publish(version, 9))
                    ack(s, 9)
                    s.sendall(publish(version, 9, duplicate=True))
                    ack(s, 9)
                elif case == 'qos2':
                    s.sendall(publish(version,9,qos=2));assert read_packet(s)==(0x50,b'\0\x09');s.sendall(packet(0x62,b'\0\x09'));assert read_packet(s)==(0x70,b'\0\x09');s.sendall(publish(version,qos=0))
                elif case == 'queue_limit':
                    s.sendall(publish(version, 1))
                    ack(s, 1)
                    s.sendall(publish(version, 2))
                elif case == 'close_partial_ack':
                    s.sendall(publish(version, 1))
                    assert read_exact(s, 1) == b'\x40'
                    return
                else:
                    s.sendall(publish(version, 0 if case == 'zero_id' else 1, qos=2 if case == 'qos2' else 1))
                if case in ('ordered', 'tiny', 'partial_ack', 'backpressure', 'duplicate', 'duplex', 'partial_outgoing'):
                    s.sendall(publish(version, qos=0))  # peer-observed ACK barrier
            reason = {'zero_id': 0x81, 'receive_window': 0x93}.get(case)
            if version == 5 and reason is not None:
                assert read_packet(s) == (0xe0, bytes([reason])), 'final diagnostic DISCONNECT'
            try:
                assert s.recv(1) == b'', 'unexpected ACK or replay after failure'
            except ConnectionResetError:
                pass
    except BaseException as e:
        errors.append(repr(e))


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--probe', type=Path, required=True)
    p.add_argument('--report', type=Path, required=True)
    a = p.parse_args()
    common = [('ordered', 4, 4), ('tiny', 4, 4), ('partial_ack', 4, 4), ('backpressure', 4, 4),
              ('duplicate', 4, 3), ('duplex', 4, 2), ('partial_outgoing', 4, 2),
              ('queue_limit', 7, 1), ('object_limit', 7, 0), ('private_limit', 7, 0),
              ('zero_id', 6, 0), ('qos2', 4, 2), ('reentrant', 4, 1), ('close_partial_ack', 5, 1)]
    rows = []
    for version in (4, 5):
        for case, close, received in common + ([('receive_window', 6, 1), ('restricted', 13, 1)] if version == 5 else []):
            errors = []
            with socket.socket() as listener:
                listener.bind(('127.0.0.1', 0))
                listener.listen(1)
                listener.settimeout(10)
                thread = threading.Thread(target=serve, args=(listener, version, case, errors), daemon=True)
                thread.start()
                command = [str(a.probe.resolve()), str(listener.getsockname()[1]), str(version), case, str(close), str(received)]
                r = subprocess.run(command, capture_output=True, text=True, timeout=15)
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
