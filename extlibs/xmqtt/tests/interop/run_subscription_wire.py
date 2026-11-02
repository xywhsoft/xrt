"""Independent byte peer for bounded MQTT subscribe/unsubscribe runtime.

Uses Python sockets and framing helpers only; never the library's codecs.
This is protocol/fault coverage, not reference-broker or performance parity.
"""
import argparse
import hashlib
import json
from pathlib import Path
import socket
import subprocess
import threading
from run_client_wire import packet, read_packet, string, take_string, vbi


def ack(version, unsub, identifier, reasons, properties=True):
    body = identifier.to_bytes(2, 'big')
    if version == 5:
        props = (b'\x1f' + string(b'accepted') + b'\x26' + string(b'tag') +
                 string(b'one') + b'\x26' + string(b'tag') + string(b'two')) if properties else b''
        body += vbi(len(props)) + props
    if not unsub or version == 5:
        body += bytes(reasons)
    return packet(0xb0 if unsub else 0x90, body)


def marker(s, version):
    s.sendall(packet(0x30, string(b'stream') + (b'\0' if version == 5 else b'') + b'seen'))


def closing(s, graceful=False):
    try:
        kind, body = read_packet(s)
    except (EOFError, ConnectionResetError):
        assert not graceful, 'missing graceful DISCONNECT'
        return
    assert kind == 0xe0 and (body in (b'', b'\x82') or len(body) == 1), (kind, body)
    if graceful:
        assert body == b''
        # Respond to final DISCONNECT with EOF; no MQTT ACK exists for it.
        return
    try:
        assert not s.recv(1), 'extra bytes after final DISCONNECT'
    except ConnectionResetError:
        pass


def control(s, version, unsub, name, second=False):
    kind, body = read_packet(s)
    assert kind == (0xa2 if unsub else 0x82), (kind, body)
    identifier = int.from_bytes(body[:2], 'big')
    assert identifier
    at = 2
    if version == 5:
        length = 0
        for shift in range(0, 28, 7):
            byte = body[at]
            at += 1
            length |= (byte & 127) << shift
            if not byte & 128:
                break
        expected = b''
        if not second and name in ('subid', 'extended'):
            expected = b'\x0b\x80\x01'
        if not second and name == 'extended':
            expected += b'\x26' + string(b'key') + string(b'v' * 140)
        assert body[at:at+length] == expected, 'property snapshot'
        at += length
    filters = [b'other'] if second else [b'test/zero', b'test/+', b'test/#']
    options = [0] if second else [0x24, 0x19, 0x22] if name == 'extended' else [0, 1, 2]
    for i, expected in enumerate(filters):
        topic, at = take_string(body, at)
        assert topic == expected, 'filter snapshot'
        if not unsub:
            assert body[at] == options[i], 'subscription options snapshot'
            at += 1
    assert at == len(body)
    return identifier


def held_publish(s, version):
    kind, body = read_packet(s)
    assert kind == 0x32
    topic, at = take_string(body, 0)
    assert topic == b'held'
    identifier = int.from_bytes(body[at:at+2], 'big')
    assert body[at+2:] == (b'\0' if version == 5 else b'')
    return identifier


def serve(listener, version, case, errors):
    try:
        unsub = case.startswith('unsub_')
        name = case[6:] if unsub else case
        with listener.accept()[0] as s:
            s.settimeout(8)
            kind, body = read_packet(s)
            protocol, at = take_string(body, 0)
            assert kind == 0x10 and protocol == b'MQTT' and body[at:at+4] == bytes([version, 2, 0, 0])
            at += 4
            if version == 5:
                props = b'\x17\0' if name in ('problem', 'problem_clean') else b''
                assert body[at:at+1+len(props)] == bytes([len(props)]) + props
                at += 1 + len(props)
            identity, at = take_string(body, at)
            assert identity == b'subscriptions' and at == len(body)
            if name == 'queued':
                s.settimeout(.05)
                try:
                    extra = s.recv(1)
                    raise AssertionError(('sent before CONNACK', extra))
                except socket.timeout:
                    pass
                s.settimeout(8)
            caps = {'wildcard': b'\x28\0', 'shared': b'\x2a\0', 'subid': b'\x29\0',
                    'max_packet': b'\x27\0\0\0\x05', 'max_qos': b'\x24\0',
                    'flow': b'\x21\0\x01', 'disabled': b'\x28\0\x2a\0'}
            props = caps.get(name, b'') if version == 5 else b''
            s.sendall(packet(0x20, b'\0\0' + (vbi(len(props)) + props if version == 5 else b'')))
            held = None
            if name in ('flow', 'id_busy', 'request_limit'):
                held = held_publish(s, version)
                assert held == (88 if name == 'flow' else 77)
            if name in ('request_limit', 'byte_limit', 'private_limit', 'object_limit', 'invalid', 'filter_limit',
                        'wildcard', 'shared', 'subid', 'max_packet'):
                if held:
                    s.sendall(packet(0x40, held.to_bytes(2, 'big')))
                closing(s)
                return
            first = control(s, version, unsub, name)
            assert first == (78 if name == 'id_busy' else 1 if name == 'auto' else 77)
            reasons = [0, 0x11, 0] if unsub else [0, 1, 2]
            if name in ('mixed', 'reject'):
                if name == 'reject':
                    reasons = [0x87 if version == 5 else 0x80] * 3
                else:
                    reasons[1] = 0x87 if version == 5 else 0x80
            if name in ('reverse', 'auto'):
                second = control(s, version, unsub, name, second=True)
                assert second == (2 if name == 'auto' else 333) and first != second
                s.sendall(ack(version, unsub, second, [0]))
            if name == 'short':
                reasons = reasons[:2]
            if name == 'long':
                reasons += [0]
            if name == 'qos':
                reasons[0] = 1  # Greater than requested QoS 0.
            if name == 'bad_reason':
                reasons[0] = 2 if unsub else 0x10
            if name == 'duplex':
                marker(s, version)  # Publication may arrive before SUBACK/after UNSUBSCRIBE.
            if name in ('cancel', 'close'):
                marker(s, version)
            if name not in ('timeout', 'cancel', 'close'):
                wire = ack(version, unsub, 65535 if name == 'wrong_id' else first, reasons,
                           properties=name != 'problem_clean')
                if name == 'wrong_kind':
                    wire = packet(0x40, first.to_bytes(2, 'big'))
                if name == 'partial':
                    s.sendall(wire[:-1])
                    return
                if name == 'tiny':
                    for byte in wire:
                        s.sendall(bytes([byte]))
                else:
                    s.sendall(wire)
                if name == 'duplicate':
                    s.sendall(wire)
            if held:
                s.sendall(packet(0x40, held.to_bytes(2, 'big')))
            closing(s, graceful=name == 'graceful')
    except Exception as e:
        errors.append(repr(e))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--case')
    args = parser.parse_args()
    rows = []
    for version in (4, 5):
        cases = ['ok', 'mixed', 'reject', 'short', 'long', 'qos', 'wrong_kind', 'wrong_id', 'duplicate',
                 'partial', 'timeout', 'cancel', 'close', 'metadata', 'request_limit', 'byte_limit',
                 'private_limit', 'object_limit', 'invalid', 'filter_limit', 'id_busy', 'auto', 'reverse',
                 'flow', 'duplex', 'graceful', 'worker', 'tiny', 'queued', 'unsub_ok', 'unsub_wrong_kind',
                 'unsub_wrong_id', 'unsub_duplicate', 'unsub_partial', 'unsub_timeout', 'unsub_cancel',
                 'unsub_metadata', 'unsub_id_busy', 'unsub_auto', 'unsub_reverse', 'unsub_flow', 'unsub_duplex',
                 'unsub_graceful', 'unsub_tiny', 'unsub_queued']
        if version == 5:
            cases += ['bad_reason', 'wildcard', 'shared', 'subid', 'max_packet', 'max_qos', 'problem',
                      'problem_clean', 'extended', 'unsub_mixed', 'unsub_reject', 'unsub_short', 'unsub_long',
                      'unsub_bad_reason', 'unsub_disabled', 'unsub_problem', 'unsub_problem_clean']
        for case in cases:
            if args.case and case != args.case:
                continue
            errors = []
            with socket.socket() as listener:
                listener.bind(('127.0.0.1', 0))
                listener.listen(1)
                listener.settimeout(10)
                thread = threading.Thread(target=serve, args=(listener, version, case, errors), daemon=True)
                thread.start()
                r = subprocess.run([str(args.probe.resolve()), str(listener.getsockname()[1]), str(version), case],
                                   capture_output=True, text=True, timeout=15)
                thread.join(10)
                rows.append(dict(version=version, case=case, exit_code=r.returncode, stdout=r.stdout,
                                 stderr=r.stderr, peer_errors=errors, peer_alive=thread.is_alive()))
                args.report.parent.mkdir(parents=True, exist_ok=True)
                args.report.write_text(json.dumps(dict(complete=False, cases=rows), indent=2)+'\n')
                assert r.returncode == 0 and not errors and not thread.is_alive(), rows[-1]
                print('PASS', version, case, flush=True)
    assert rows, 'no selected cases'
    args.report.write_text(json.dumps(dict(complete=True, probe_sha256=hashlib.sha256(args.probe.read_bytes()).hexdigest(), cases=rows), indent=2)+'\n')


if __name__ == '__main__':
    main()
