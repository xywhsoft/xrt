"""Independent MQTT QoS1 sender peer, including window/id and hostile ACKs."""
import argparse
import hashlib
import json
from pathlib import Path
import socket
import subprocess
import threading
from run_client_wire import packet, read_packet, read_exact, string, take_string, vbi


def ack(version, identifier, reason=0):
    body = identifier.to_bytes(2, 'big')
    if version == 5:
        props = b'\x1f' + string(b'accepted') + b'\x26' + string(b'tag') + string(b'one') + b'\x26' + string(b'tag') + string(b'two')
        body += bytes([reason]) + vbi(len(props)) + props
    return packet(0x40, body)


def publish_read(s, version):
    t, body = read_packet(s)
    topic, at = take_string(body, 0)
    assert topic == b'qos/one'
    qos = (t >> 1) & 3
    assert t in (0x30, 0x32)
    identifier = 0
    if qos:
        identifier = int.from_bytes(body[at:at+2], 'big')
        assert identifier
        at += 2
    if version == 5:
        assert body[at] == 0
        at += 1
    assert body[at:] == b'original'
    return qos, identifier


def serve(listener, version, case, errors):
    try:
        with listener.accept()[0] as s:
            s.settimeout(8)
            t, b = read_packet(s)
            protocol, at = take_string(b, 0)
            assert t == 0x10 and protocol == b'MQTT' and b[at] == version and b[at+1] == 2
            assert int.from_bytes(b[at+2:at+4], 'big') == (1 if case == 'window' else 0)
            at += 4
            if version == 5:
                props = b'\x17\0' if case in ('problem_info', 'problem_info_clean') else b''
                assert b[at:at+1+len(props)] == bytes([len(props)]) + props
                at += 1 + len(props)
            identity, at = take_string(b, at)
            assert identity == b'qos-one' and at == len(b)
            props = b'\x21\0\x01' if case in ('window', 'unsent_ack') else b''
            s.sendall(packet(0x20, b'\0\0' + (vbi(len(props)) + props if version == 5 else b'')))
            if case != 'private':
                qos, first = publish_read(s, version)
                assert qos == 1 and first == (65535 if case == 'auto' else 77 if case in ('id_busy', 'id_reuse') else 1)
                if case in ('window', 'unsent_ack'):
                    assert version == 5 and publish_read(s, version) == (0, 0), 'QoS1 sent beyond Receive Maximum'
                    if case == 'unsent_ack':
                        s.sendall(ack(version, 257))
                    else:
                        assert read_packet(s) == (0xc0, b''), 'control traffic blocked at zero credit'
                        s.sendall(b'\xd0\0' + ack(version, first))
                        assert publish_read(s, version) == (1, 257)
                        s.sendall(ack(version, 257))
                elif case in ('reverse', 'auto'):
                    qos, second = publish_read(s, version)
                    assert qos == 1 and second != first
                    if case == 'reverse':
                        assert second == 257
                    s.sendall(ack(version, second) + ack(version, first))
                elif case == 'id_busy':
                    assert publish_read(s, version) == (0, 0)
                    s.sendall(ack(version, first))
                elif case == 'id_reuse':
                    s.sendall(ack(version, first))
                    assert publish_read(s, version) == (1, first)
                    s.sendall(ack(version, first))
                elif case == 'timeout':
                    pass
                elif case == 'cancel':
                    s.sendall(packet(0x30, string(b'seen') + (b'\0' if version == 5 else b'') + b'seen'))
                elif case == 'unknown_ack':
                    s.sendall(ack(version, 499))
                elif case == 'zero_ack':
                    s.sendall(b'\x40\x02\0\0')
                elif case == 'wrong_kind':
                    s.sendall(packet(0x50, first.to_bytes(2, 'big')))
                elif case == 'bad_reason':
                    s.sendall(packet(0x40, first.to_bytes(2, 'big') + b'\x01'))
                elif case == 'partial_ack':
                    s.sendall(b'\x40\x02\0')
                    s.shutdown(socket.SHUT_WR)
                else:
                    reason = 0x87 if case == 'reject' else 0x10 if case == 'no_subscribers' else 0
                    wire = packet(0x40, first.to_bytes(2, 'big')) if case == 'problem_info_clean' else ack(version, first, reason)
                    if case == 'tiny':
                        for b in wire:
                            s.sendall(bytes([b]))
                    else:
                        s.sendall(wire + (wire if case == 'duplicate_ack' else b''))
            reason = {'duplicate_ack': 0x82, 'unknown_ack': 0x82, 'zero_ack': 0x81,
                      'wrong_kind': 0x82, 'unsent_ack': 0x82, 'bad_reason': 0x81,
                      'problem_info': 0x82}.get(case)
            if version == 5 and reason is not None:
                assert read_packet(s) == (0xe0, bytes([reason])), 'final diagnostic DISCONNECT'
            try:
                assert s.recv(1) == b'', 'unexpected packet/replay'
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
    for version in (4, 5):
        cases = ['success', 'tiny', 'reverse', 'auto', 'id_busy', 'id_reuse', 'timeout', 'cancel',
                 'metadata', 'private', 'duplicate_ack', 'unknown_ack', 'zero_ack', 'wrong_kind', 'partial_ack']
        if version == 5:
            cases += ['window', 'unsent_ack', 'reject', 'no_subscribers', 'bad_reason', 'problem_info', 'problem_info_clean']
        for case in cases:
            errors = []
            with socket.socket() as listener:
                listener.bind(('127.0.0.1', 0))
                listener.listen(1)
                listener.settimeout(10)
                thread = threading.Thread(target=serve, args=(listener, version, case, errors), daemon=True)
                thread.start()
                r = subprocess.run([str(a.probe.resolve()), str(listener.getsockname()[1]), str(version), case], capture_output=True, text=True, timeout=15)
                thread.join(10)
                rows.append({'version': version, 'case': case, 'exit_code': r.returncode,
                             'stdout': r.stdout, 'stderr': r.stderr, 'peer_errors': errors, 'peer_alive': thread.is_alive()})
                a.report.parent.mkdir(parents=True, exist_ok=True)
                a.report.write_text(json.dumps({'complete': False, 'cases': rows}, indent=2)+'\n')
                assert r.returncode == 0 and not errors and not thread.is_alive(), rows[-1]
                print('PASS', version, case, flush=True)
    a.report.write_text(json.dumps({'complete': True, 'probe_sha256': hashlib.sha256(a.probe.read_bytes()).hexdigest(), 'cases': rows}, indent=2)+'\n')


if __name__ == '__main__':
    main()
