"""Independent outgoing MQTT5 alias peer, including cancellation/reordering."""
import argparse
import hashlib
import json
from pathlib import Path
import socket
import subprocess
import threading
from run_client_wire import packet, read_packet, read_exact, string, take_string, vbi
from run_subscription_wire import closing

CASES = ['basic', 'snapshot', 'remap', 'cancel_bind', 'timeout_bind', 'flow', 'flow_cancel',
         'flow_timeout', 'tiny', 'qos1', 'qos2', 'reject_bind', 'restricted_bind', 'disabled',
         'over_max', 'max_id', 'max_topic', 'vbi127', 'vbi16383', 'cap_peer', 'cap_local',
         'partial_cancel', 'partial_timeout', 'peer_close', 'zero', 'duplicate', 'unknown',
         'private', 'duplex', 'graceful']


def marker(s, text, alias=False):
    props = b'\x23\0\x01' if alias else b''
    s.sendall(packet(0x30, string(b'incoming') + bytes([len(props)]) + props + text))


def receive(s, aliases, rows):
    while True:
        kind, body = read_packet(s)
        if kind == 0x62:
            assert len(body) == 2
            s.sendall(packet(0x70, body))
            matches=[r for r in rows if r['identifier']==int.from_bytes(body, 'big') and r['qos']==2];assert len(matches)==1;matches[0]['released']=True
            continue
        assert kind in (0x30, 0x32, 0x34), kind
        topic, at = take_string(body, 0)
        qos = (kind >> 1) & 3
        identifier = 0
        if qos:
            identifier = int.from_bytes(body[at:at+2], 'big')
            at += 2
        assert body[at] in (0, 3)
        count = body[at]
        at += 1
        if not count:
            assert topic == b'signal' and body[at:] == b'go'
            return dict(signal=True)
        assert body[at] == 0x23
        alias = int.from_bytes(body[at+1:at+3], 'big')
        assert alias
        at += 3
        if topic:
            aliases[alias] = topic
        assert alias in aliases, 'empty alias before wire binding'
        payload = body[at:]
        assert payload and payload[:-1] == b'p'*(len(payload)-1)
        index = payload[-1]-ord('0')
        assert 0 <= index < 10
        row = dict(index=index, topic=aliases[alias], compact=not topic, alias=alias, qos=qos,
                   identifier=identifier, wire_bytes=1+len(vbi(len(body)))+len(body), payload=len(payload))
        rows.append(row)
        return row


def acknowledge(s, row, reject=False):
    if row['qos']:
        body = row['identifier'].to_bytes(2, 'big')
        if reject:
            body += b'\x87\0'
        s.sendall(packet(0x50 if row['qos'] == 2 else 0x40, body))


def serve(listener, case, errors, rows):
    try:
        with listener.accept()[0] as s:
            s.settimeout(10)
            kind, body = read_packet(s)
            protocol, at = take_string(body, 0)
            assert kind == 0x10 and protocol == b'MQTT' and body[at:at+4] == b'\x05\x02\0\0'
            at += 4
            props = b'\x22\0\x01' if case == 'duplex' else b''
            assert body[at:at+1+len(props)] == bytes([len(props)])+props
            at += 1+len(props)
            identity, at = take_string(body, at)
            assert identity == b'send-alias' and at == len(body)
            maximum = 0 if case == 'disabled' else 65535 if case == 'max_id' else 2
            caps = b'\x22'+maximum.to_bytes(2, 'big')
            if case.startswith('flow'):
                caps += b'\x21\0\x01'
            if case == 'restricted_bind':
                caps += b'\x24\0'
            if case == 'cap_peer':
                caps += b'\x27\0\0\0\x28'
            s.sendall(packet(0x20, b'\0\0'+bytes([len(caps)])+caps))
            if case in ('zero', 'duplicate', 'unknown', 'private', 'disabled', 'over_max'):
                closing(s)
                return
            if case.startswith('partial_') or case == 'peer_close':
                assert read_exact(s, 1) in (b'\x30', b'\x32')
                if case == 'peer_close':
                    return
                if case == 'partial_cancel':
                    marker(s, b'cancel')
                try:
                    while s.recv(65536):
                        pass
                except ConnectionResetError:
                    pass
                return
            aliases = {}
            alpha, bravo, other = b'alias/alpha', b'alias/bravo', b'alias/other'
            if case in ('cap_peer', 'cap_local'):
                alpha = b't'*30
            if case == 'max_topic':
                alpha = b't'*65535
            if case.startswith('flow'):
                first = receive(s, aliases, rows)
                second = receive(s, aliases, rows)
                third = receive(s, aliases, rows)
                assert [r['index'] for r in (first, second, third)] == [0, 2, 3], 'blocked QoS1 cannot prevent QoS0 progress'
                assert [r['topic'] for r in (first, second, third)] == [alpha, bravo, other]
                assert not first['compact'] and not second['compact'] and not third['compact'], 'logical snapshot must repair the unsent mapping'
                if case != 'flow':
                    marker(s, b'cancel')
                    assert receive(s, aliases, rows).get('signal'), 'wait for completed local cancellation/timeout'
                acknowledge(s, first)
                if case == 'flow':
                    delayed = receive(s, aliases, rows)
                    assert delayed['index'] == 1 and delayed['topic'] == bravo and not delayed['compact']
                    acknowledge(s, delayed)
                last = receive(s, aliases, rows)
                assert last['index'] == 4 and last['topic'] == other and last['compact'] == (case != 'flow')
            else:
                indices = [1, 2, 3] if case in ('cancel_bind', 'timeout_bind') else [0, 1, 2, 3] if case == 'snapshot' else [1] if case == 'restricted_bind' else [0, 1] if case in ('cap_peer', 'cap_local', 'reject_bind') else [0, 1, 2]
                expected = [alpha, bravo, bravo] if case in ('cancel_bind', 'timeout_bind') else [alpha, alpha, bravo, bravo] if case == 'snapshot' else [alpha, bravo, bravo] if case == 'remap' else [alpha]*len(indices)
                compact = [False, False, True] if case in ('cancel_bind', 'timeout_bind', 'remap') else [False, True, False, True] if case == 'snapshot' else [False] if case == 'restricted_bind' else [False]+[True]*(len(indices)-1)
                for index, topic, short in zip(indices, expected, compact):
                    row = receive(s, aliases, rows)
                    assert row['index'] == index and row['topic'] == topic and row['compact'] == short, (case, index, row)
                    assert row['alias'] == (65535 if case == 'max_id' else 1)
                    if case.startswith('cap_'):
                        assert row['wire_bytes'] <= 40, 'actual compressed frame must obey wire caps'
                    acknowledge(s, row, reject=case == 'reject_bind' and index == 0)
            # Finish any pipelined QoS2 PUBRELs before reporting peer observation.
            if case == 'qos2':
                pending = {r['identifier'] for r in rows if not r.get('released')}
                while pending:
                    kind, body = read_packet(s)
                    identifier = int.from_bytes(body, 'big')
                    assert kind == 0x62 and identifier in pending
                    pending.remove(identifier)
                    s.sendall(packet(0x70, body))
            marker(s, b'done', alias=case == 'duplex')
            closing(s, graceful=case == 'graceful')
    except Exception as e:
        errors.append(repr(e))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--case')
    args = parser.parse_args()
    results = []
    for case in CASES:
        if args.case and case != args.case:
            continue
        errors, wire = [], []
        with socket.socket() as listener:
            listener.bind(('127.0.0.1', 0));listener.listen(1);listener.settimeout(12)
            thread = threading.Thread(target=serve, args=(listener, case, errors, wire), daemon=True);thread.start()
            r = subprocess.run([str(args.probe.resolve()), str(listener.getsockname()[1]), case], capture_output=True, text=True, timeout=25)
            thread.join(12)
            rows = [dict(row, topic=row['topic'].decode() if len(row['topic'])<=128 else None,topic_size=len(row['topic']),topic_sha256=hashlib.sha256(row['topic']).hexdigest()) for row in wire]
            record = dict(case=case, exit_code=r.returncode, stdout=r.stdout, stderr=r.stderr, peer_errors=errors, peer_alive=thread.is_alive(), wire=rows)
            results.append(record);args.report.parent.mkdir(parents=True, exist_ok=True);args.report.write_text(json.dumps(dict(complete=False, cases=results), indent=2)+'\n')
            assert not r.returncode and not errors and not thread.is_alive(), record
            observed = json.loads(r.stdout)['accepted']
            for row in wire:
                assert observed[row['index']] == row['wire_bytes'], 'AcceptedBytes must report the actual virtual frame'
            print('PASS', case, flush=True)
    assert results
    args.report.write_text(json.dumps(dict(complete=True, probe_sha256=hashlib.sha256(args.probe.read_bytes()).hexdigest(), cases=results), indent=2)+'\n')


if __name__ == '__main__':
    main()
