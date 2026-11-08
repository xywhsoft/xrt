"""Independent MQTT5 alias byte peer; no library codecs or reference client."""
import argparse
import hashlib
import json
from pathlib import Path
import socket
import subprocess
import threading
from run_client_wire import packet, read_packet, string, take_string
from run_subscription_wire import closing

# name, terminal client status, owned publications, advertised maximum, reason
CASES = [
    ('basic', 4, 2, 2, 0), ('coalesced', 4, 2, 2, 0), ('fragmented', 4, 2, 2, 0),
    ('remap_short', 4, 4, 2, 0), ('remap_same', 4, 4, 2, 0), ('remap_long', 4, 4, 2, 0),
    ('independent', 4, 4, 2, 0), ('max_id', 4, 2, 65535, 0), ('qos1', 4, 2, 2, 0),
    ('qos2', 4, 2, 2, 0), ('duplicate2_rebind', 4, 2, 2, 0), ('unknown', 6, 0, 2, 0x82),
    ('zero', 6, 0, 2, 0x94), ('zero_after_user', 6, 0, 2, 0x94), ('over_max', 6, 0, 2, 0x94),
    ('default_none', 6, 0, 0, 0x94), ('explicit_zero', 6, 0, 0, 0x94),
    ('duplicate_alias', 6, 0, 2, 0x82), ('duplicate_alias_after_user', 6, 0, 2, 0x82),
    ('empty_no_alias', 6, 0, 2, 0x81), ('invalid_topic', 6, 0, 2, 0x81),
    ('long_topic', 4, 2, 2, 0), ('truncated', 5, 0, 2, 0), ('metadata_limit', 7, 1, 2, 0),
    ('message_bytes', 7, 0, 2, 0), ('queue_limit', 7, 1, 2, 0),
    ('private_table', 7, 0, 65535, 0), ('private_entry', 7, 0, 2, 0),
    ('private_replace', 7, 1, 2, 0), ('resolved_bytes', 7, 1, 2, 0),
    ('cache_same_capacity', 4, 3, 2, 0), ('duplex', 4, 2, 2, 0),
    ('graceful', 4, 2, 2, 0), ('close_callback', 4, 1, 2, 0), ('per_connection', 4, 2, 2, 0),
    ('server_caps', 4, 2, 2, 0), ('properties', 4, 2, 2, 0),
    ('no_alias_mapping', 6, 1, 2, 0x82), ('plain_does_not_rebind', 4, 3, 2, 0),
]


def publish(topic, alias, qos=0, identifier=0, dup=False, user=False, properties=False, repeat=False):
    props = b'\x26' + string(b'k') + string(b'v') if user or properties else b''
    if alias is not None:
        props += b'\x23' + alias.to_bytes(2, 'big')
    if repeat:
        props += b'\x23' + alias.to_bytes(2, 'big')
    if properties:
        props += b'\x0b\x80\x01'
    body = string(topic) + (identifier.to_bytes(2, 'big') if qos else b'') + bytes([len(props)]) + props + b'body'
    return packet(0x30 | (qos << 1) | (8 if dup else 0), body)


def exchange(s, topic, alias, qos, identifier):
    s.sendall(publish(topic, alias, qos, identifier))
    assert read_packet(s) == (0x40 if qos == 1 else 0x50, identifier.to_bytes(2, 'big'))
    if qos == 2:
        s.sendall(packet(0x62, identifier.to_bytes(2, 'big')))
        assert read_packet(s) == (0x70, identifier.to_bytes(2, 'big'))


def start(s, case, maximum):
    kind, body = read_packet(s)
    protocol, at = take_string(body, 0)
    assert kind == 0x10 and protocol == b'MQTT' and body[at:at+4] == b'\x05\x02\0\0'
    at += 4
    props = b'' if case == 'default_none' else b'\x22' + maximum.to_bytes(2, 'big')
    assert body[at:at+1+len(props)] == bytes([len(props)]) + props
    at += 1 + len(props)
    identity, at = take_string(body, at)
    assert identity == b'alias-probe' and at == len(body)
    caps = b'\x22\0\x01' if case == 'server_caps' else b''
    s.sendall(packet(0x20, b'\0\0' + bytes([len(caps)]) + caps))
    kind, body = read_packet(s)
    topic, at = take_string(body, 0)
    assert kind == 0x30 and topic == b'go' and body[at:] == b'\0start'


def serve(listener, case, maximum, reason, errors):
    try:
        for phase in range(2 if case == 'per_connection' else 1):
            with listener.accept()[0] as s:
                s.settimeout(8)
                start(s, case, maximum)
                if phase:
                    s.sendall(publish(b'', 1))
                    assert read_packet(s) == (0xe0, b'\x82')
                    assert not s.recv(1)
                    continue
                alias = 65535 if maximum == 65535 else 2 if case == 'server_caps' else 1
                alpha, bravo = b'topic/alpha', b'topic/bravo'
                if case in ('qos1', 'qos2'):
                    qos = 1 if case == 'qos1' else 2
                    exchange(s, alpha, alias, qos, 7)
                    exchange(s, b'', alias, qos, 8)
                elif case == 'duplicate2_rebind':
                    s.sendall(publish(alpha, 1, 2, 7))
                    assert read_packet(s) == (0x50, b'\0\x07')
                    s.sendall(publish(bravo, 1, 2, 7, dup=True))
                    assert read_packet(s) == (0x50, b'\0\x07')
                    s.sendall(publish(b'', 1) + packet(0x62, b'\0\x07'))
                    assert read_packet(s) == (0x70, b'\0\x07')
                elif case == 'graceful':
                    s.sendall(publish(alpha, 1) + publish(b'', 1, 2, 7))
                    assert read_packet(s) == (0x50, b'\0\x07')
                    s.settimeout(.05)
                    try:
                        raise AssertionError(('disconnect before PUBREL/PUBCOMP', s.recv(1)))
                    except socket.timeout:
                        pass
                    s.settimeout(8)
                    s.sendall(packet(0x62, b'\0\x07'))
                    assert read_packet(s) == (0x70, b'\0\x07')
                else:
                    frames = [publish(alpha, alias), publish(b'', alias)]
                    if case.startswith('remap_'):
                        topic = b'x' if case == 'remap_short' else bravo if case == 'remap_same' else b'topic/much/longer/than/before'
                        frames += [publish(topic, alias), publish(b'', alias)]
                    elif case == 'independent':
                        frames = [publish(alpha, 1), publish(bravo, 2), publish(b'', 1), publish(b'', 2)]
                    elif case == 'unknown':
                        frames = [publish(b'', 1)]
                    elif case in ('zero', 'zero_after_user'):
                        frames = [publish(alpha, 0, user=case.endswith('after_user'))]
                    elif case == 'over_max':
                        frames = [publish(alpha, maximum+1)]
                    elif case in ('duplicate_alias', 'duplicate_alias_after_user'):
                        frames = [publish(alpha, 1, user=case.endswith('after_user'), repeat=True)]
                    elif case == 'empty_no_alias':
                        frames = [publish(b'', None)]
                    elif case == 'invalid_topic':
                        frames = [publish(b'bad/#', 1)]
                    elif case in ('long_topic', 'private_entry'):
                        frames = [publish(b'a'*65535, 1)] + ([publish(b'', 1)] if case == 'long_topic' else [])
                    elif case == 'private_replace':
                        frames = [publish(alpha, 1), publish(b'a'*65535, 1)]
                    elif case == 'cache_same_capacity':
                        frames = [publish(alpha, 1), publish(bravo, 1), publish(b'', 1)]
                    elif case == 'properties':
                        frames = [publish(alpha, 1, properties=True), publish(b'', 1, properties=True)]
                    elif case == 'no_alias_mapping':
                        frames = [publish(alpha, None), publish(b'', 1)]
                    elif case == 'plain_does_not_rebind':
                        frames = [publish(alpha, 1), publish(bravo, None), publish(b'', 1)]
                    elif case == 'close_callback':
                        frames = frames[:1]
                    elif case == 'truncated':
                        s.sendall(frames[0][:-1])
                        continue
                    if case == 'duplex':
                        kind, body = read_packet(s)
                        topic, at = take_string(body, 0)
                        assert kind == 0x32 and topic == b'out' and body[at:] == b'\0\x58\0payload'
                        s.sendall(packet(0x40, b'\0\x58'))
                    if case == 'fragmented':
                        for byte in b''.join(frames):
                            s.sendall(bytes([byte]))
                    else:
                        s.sendall(b''.join(frames))
                if reason:
                    assert read_packet(s) == (0xe0, bytes([reason])), 'exact protocol diagnostic'
                    assert not s.recv(1)
                else:
                    closing(s, graceful=case == 'graceful')
    except Exception as e:
        errors.append(repr(e))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--case')
    args = parser.parse_args()
    rows = []
    for case, status, count, maximum, reason in CASES:
        if args.case and args.case != case:
            continue
        errors = []
        with socket.socket() as listener:
            listener.bind(('127.0.0.1', 0))
            listener.listen(2)
            listener.settimeout(10)
            thread = threading.Thread(target=serve, args=(listener, case, maximum, reason, errors), daemon=True)
            thread.start()
            r = subprocess.run([str(args.probe.resolve()), str(listener.getsockname()[1]), case, str(status), str(count), str(maximum), str(reason)], capture_output=True, text=True, timeout=20)
            thread.join(10)
            rows.append(dict(case=case, exit_code=r.returncode, stdout=r.stdout, stderr=r.stderr, peer_errors=errors, peer_alive=thread.is_alive()))
            args.report.parent.mkdir(parents=True, exist_ok=True)
            args.report.write_text(json.dumps(dict(complete=False, cases=rows), indent=2)+'\n')
            assert not r.returncode and not errors and not thread.is_alive(), rows[-1]
            print('PASS', case, flush=True)
    assert rows, 'no cases selected'
    args.report.write_text(json.dumps(dict(complete=True, probe_sha256=hashlib.sha256(args.probe.read_bytes()).hexdigest(), cases=rows), indent=2)+'\n')


if __name__ == '__main__':
    main()
