"""Verify a live QoS 2 checkpoint, native process exit and fresh recovery with Mosquitto."""
import argparse
from itertools import product
import json
import os
from pathlib import Path
import re
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import time

from journal_oracle import PAGE, START, crc
from run_reconnect_store_broker import (
    FAULTS, LOOPBACK, PAYLOAD, TOPIC, TRANSPORTS, certificates, command,
    connect_subscriber, digest, field, gateway, readiness, subscribe, verify_protocol,
)
from run_session_broker import varint


# Its process handle is native to the client. In particular, WSL Python never
# signals only the WSL relay of a Windows process and mistakes it for an exit.
SUPERVISOR = r"""
import os
import subprocess
import sys

with subprocess.Popen(sys.argv[1:], stdin=subprocess.PIPE) as child:
    try:
        assert sys.stdin.buffer.read(1) == b'c'
        child.stdin.write(b'c')
        child.stdin.flush()
        assert sys.stdin.buffer.read(1) == b'x'
        assert child.poll() is None, 'client exited before requested termination'
        child.kill()
        code = child.wait(timeout=8)
        assert code == (1 if os.name == 'nt' else -9), code
        print('TERMINATED', child.pid, code, flush=True)
    finally:
        if child.poll() is None:
            child.kill()
            child.wait(timeout=8)
        child.stdin.close()
"""

COMPACTED = re.compile(r'^COMPACTED (\d+) (\d+) (\d+) (PUBLISH|PUBREL) (\d+) (\d+) (\d+)$', re.M)
TERMINATED = re.compile(r'^TERMINATED (\d+) (-?\d+)$', re.M)


def windows_path(path):
    return subprocess.check_output(['wslpath', '-w', str(path)], timeout=10, text=True).strip()


def wait_marker(process, path, matcher):
    deadline = time.monotonic() + 12
    while time.monotonic() < deadline:
        output = path.read_text(encoding='utf-8')
        match = matcher.search(output)
        if match:
            return match
        assert process.poll() is None, output
        time.sleep(.02)
    raise TimeoutError(f'client did not reach durable checkpoint: {output}')


def check_page(page):
    assert len(page) == PAGE and crc(page[:-4]) == struct.unpack_from('>I', page, PAGE-4)[0]


def checkpoint(root, match, mqtt, fault):
    generation, sequence, identifier = map(int, match.group(1, 2, 3))
    key, previous_bytes, selected_bytes = map(int, match.group(5, 6, 7))
    stage = match[4]
    assert (generation, sequence, key) == (2, 2, 1) and 0 < identifier <= 65535
    assert stage == ('PUBREL' if fault == 2 else 'PUBLISH')
    control = (root/'journal.slot').read_bytes()
    pages = [control[offset:offset+PAGE] for offset in range(0, len(control), PAGE)]
    assert len(pages) == 3 and pages[0][:8] == b'XMQSLT1\0' and pages[1] == pages[2]
    for page in pages:
        check_page(page)
    assert pages[1][:8] == b'XMQSEL1\0'
    selected = struct.unpack_from('>QQQI', pages[1], 8)
    assert selected[:3] == (generation, sequence, selected_bytes)
    assert (root/'journal.0').stat().st_size == previous_bytes > selected_bytes

    journal = (root/'journal.1').read_bytes()
    assert len(journal) == selected_bytes and journal[:8] == b'XMQWAL\0\0'
    check_page(journal[:PAGE])
    assert journal[10] == (5 if mqtt == 5 else 4)
    assert struct.unpack_from('>Q', journal, 16)[0] == generation
    witnesses = [journal[offset:offset+PAGE] for offset in (PAGE, 2*PAGE)]
    assert witnesses[0] == witnesses[1] and witnesses[0][:8] == b'XMQWCOM\0'
    check_page(witnesses[0])
    assert struct.unpack_from('>QQQI', witnesses[0], 8) == selected

    at, prior, payloads = START, 0, []
    for index in range(1, sequence+1):
        head = journal[at:at+48]
        assert head[:8] == b'XMQWTX1\0'
        total, number, length, chain, body_crc, reserved, head_crc = struct.unpack_from('>QQQIIII', head, 8)
        assert (total, number, chain, reserved) == (length+64, index, prior, 0)
        assert head_crc == crc(head[:44])
        payload = journal[at+48:at+48+length]
        assert body_crc == crc(payload)
        prior = crc(head+payload)
        assert struct.unpack_from('>8sII', journal, at+48+length) == (b'XMQWEND\0', prior, 0)
        at += total
        payloads.append(payload)
    assert at == len(journal) and prior == selected[3]
    meta, rows = payloads
    assert meta[:8] == b'XMQCKPT\0' and meta[8:11] == bytes((1, 5 if mqtt == 5 else 4, 7))
    assert struct.unpack_from('>QQ', meta, 16) == (1, key)
    assert struct.unpack_from('>Q', meta, 48)[0] == 1
    assert rows[:16] == b'XMQCROW\0\0\0\0\x01\0\0\0\0'
    row_key, epoch, wire_size, row_id, flags, reason = struct.unpack_from('>QQIHBB', rows, 16)
    assert (row_key, epoch, row_id, reason) == (key, 1, identifier, 0)
    assert flags & 3 == 3 and not flags & 48 and bool(flags & 8) == (fault == 2)
    wire = rows[40:]
    length, body = varint(wire, 1)
    properties = b'\0' if mqtt == 5 else b''
    assert len(wire) == wire_size == body+length and wire[0] == 0x34
    assert wire[body:] == field(TOPIC) + identifier.to_bytes(2, 'big') + properties + PAYLOAD
    return dict(generation=generation, sequence=sequence, packet_id=identifier, stage=stage,
                key=key, previous_bytes=previous_bytes, selected_bytes=selected_bytes,
                control_sha256=digest(root/'journal.slot'), journal_sha256=digest(root/'journal.1'))


def case(args, client, transport, mqtt, fault, scratch, name):
    cross = os.name != 'nt' and client.suffix.lower() == '.exe'
    windows_python = args.windows_python if cross else None
    if cross:
        assert windows_python, 'native Windows Python is required to terminate the native client'
    launcher = str(windows_python or sys.executable)
    logs = args.report.parent / (args.report.stem+'-logs')
    logs.mkdir(exist_ok=True)
    broker_log = logs/(name+'-broker.log')
    cut_log = logs/(name+'-cut.log')
    resume_log = logs/(name+'-resume.log')
    root = scratch/'slot'
    root.mkdir()
    for leaf in ('journal.slot', 'journal.0', 'journal.1'):
        (root/leaf).touch()
    directory = windows_path(root) if cross else str(root)
    ca = windows_path(scratch.parent/'ca.pem') if cross and transport.startswith(('tls', 'wss')) else str(scratch.parent/'ca.pem')

    with socket.socket() as seed:
        seed.bind((LOOPBACK, 0))
        port = seed.getsockname()[1]
    config = scratch/'broker.conf'
    as_root = 'user root\n' if hasattr(os, 'geteuid') and os.geteuid() == 0 else ''
    config.write_text(as_root + f'allow_anonymous true\npersistence false\nlog_dest stdout\n'
                      f'log_type all\nlistener {port} {LOOPBACK}\n', encoding='utf-8')
    with broker_log.open('wb') as log, subprocess.Popen(
            [str(args.broker), '-c', str(config)], stdout=log, stderr=subprocess.STDOUT) as broker:
        try:
            with connect_subscriber(broker, port) as sink:
                subscribe(sink)
                proxy = gateway(transport, port, mqtt, fault, scratch.parent, windows_python)
                try:
                    readiness(windows_python, proxy.port)
                    cmd = command(client, transport, proxy.port, mqtt, fault, ca)[:-1]
                    cut_cmd = [windows_path(client) if cross else cmd[0], *cmd[1:], 'compact-cut', directory]
                    with cut_log.open('wb') as log, subprocess.Popen(
                            [launcher, '-u', '-c', SUPERVISOR, *cut_cmd], stdin=subprocess.PIPE,
                            stdout=log, stderr=subprocess.STDOUT) as cutting:
                        try:
                            proxy.wait(lambda: any(not r['forwarded'] for r in proxy.rows) or cutting.poll() is not None)
                            assert cutting.poll() is None, cut_log.read_text(encoding='utf-8')
                            cutting.stdin.write(b'c')
                            cutting.stdin.flush()
                            match = wait_marker(cutting, cut_log, COMPACTED)
                            assert len([r for r in proxy.rows if r['type'] == 1]) == 1
                            cutting.stdin.write(b'x')
                            cutting.stdin.flush()
                            assert cutting.wait(timeout=12) == 0, cut_log.read_text(encoding='utf-8')
                        finally:
                            cutting.stdin.close()  # EOF also retires a live native child on a failed assertion.
                            if cutting.poll() is None:
                                cutting.wait(timeout=12)
                    exited = TERMINATED.search(cut_log.read_text(encoding='utf-8'))
                    assert exited and int(exited[2]) == (1 if cross or os.name == 'nt' else -9)
                    snapshot = checkpoint(root, match, mqtt, fault)
                    for thread in tuple(getattr(proxy, 'front_threads', proxy.threads)):
                        thread.join(8)
                        assert not thread.is_alive(), 'old transport did not retire after native process exit'
                    with resume_log.open('wb') as log:
                        recovered = subprocess.run([*cmd, 'compact-resume', directory],
                            stdout=log, stderr=subprocess.STDOUT, timeout=25)
                    output = resume_log.read_text(encoding='utf-8')
                    assert recovered.returncode == 0, (output, proxy.rows)
                    expected = ' '.join(str(snapshot[k]) for k in ('generation', 'sequence', 'packet_id', 'stage', 'key'))
                    assert f'RECOVERED {expected}' in output, output
                    assert 'Fresh process settled the compacted QoS 2 obligation.' in output, output
                    end = 5 if fault == 2 else 6
                    assert f'Committed graceful END at {end}.' in output and f'VERIFIED 2 {end}' in output, output
                    proxy.wait(lambda: any(r['type'] == 14 and r['generation'] == 2 for r in proxy.rows))
                    deliveries = verify_protocol(proxy, sink, transport, fault)
                    first = next(r for r in proxy.rows if r['direction'] == 'cs' and r['type'] == 3)
                    assert first['identifier'] == snapshot['packet_id']
                    assert (root/'journal.1').stat().st_size > snapshot['selected_bytes']
                    result = dict(checkpoint=snapshot, killed_pid=int(exited[1]), kill_exit_code=int(exited[2]),
                        trace=proxy.rows, observations=getattr(proxy, 'observations', None),
                        subscriber_deliveries=deliveries, output=output.splitlines(),
                        graceful_end_sequence=end, selected_after_resume_sha256=digest(root/'journal.1'))
                finally:
                    proxy.close()
        finally:
            broker.terminate()
            try:
                broker.wait(timeout=5)
            except subprocess.TimeoutExpired:
                broker.kill()
                broker.wait(timeout=5)
    result['logs'] = {path.name: digest(path) for path in (broker_log, cut_log, resume_log)}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--broker', type=Path, required=True)
    parser.add_argument('--form', action='append', nargs=2, metavar=('LABEL', 'PROGRAM'), required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--windows-python', type=Path, default=shutil.which('python.exe'))
    parser.add_argument('--openssl', default=shutil.which('openssl'))
    parser.add_argument('--transport', nargs='+', choices=TRANSPORTS, default=TRANSPORTS)
    parser.add_argument('--mqtt', nargs='+', type=int, choices=(311, 5), default=(311, 5))
    parser.add_argument('--fault', nargs='+', choices=FAULTS[1:], default=FAULTS[1:])
    args = parser.parse_args()
    forms = {}
    for label, path in args.form:
        if label in forms or not re.fullmatch(r'[a-zA-Z0-9_-]+', label):
            parser.error('form labels must be unique and use ASCII alphanumerics, hyphens or underscores')
        forms[label] = Path(path).resolve(strict=True)
    args.broker = args.broker.resolve(strict=True)
    args.report = args.report.resolve()
    args.report.parent.mkdir(parents=True, exist_ok=True)
    version = subprocess.check_output([str(args.broker), '-h'], timeout=8, text=True).splitlines()[0]
    sources = (Path(__file__).name, 'journal_oracle.py', 'run_reconnect_store_broker.py',
               'reconnect_ws_gateway.py', 'session_tls_proxy.py', 'run_session_broker.py',
               'run_session_tls_broker.py', 'run_mqtt_tls.py')
    report = dict(schema=1, complete=False, broker=version, broker_sha256=digest(args.broker),
                  forms={label: dict(path=str(path), sha256=digest(path)) for label, path in forms.items()},
                  sources={name: digest(Path(__file__).with_name(name)) for name in sources}, cases=[])
    try:
        with tempfile.TemporaryDirectory(prefix='xmqtt-compact-crash-', dir=args.report.parent) as directory:
            root = Path(directory)
            if any(t.startswith(('tls', 'wss')) for t in args.transport):
                if not args.openssl:
                    parser.error('OpenSSL is required for TLS and WSS')
                report['public_certificates'] = certificates(root, args.openssl)
            for label, transport, mqtt, dropped in product(forms, args.transport, args.mqtt, args.fault):
                name = f'{label}-{transport}-{mqtt}-{dropped}'
                row = dict(name=name, form=label, transport=transport, mqtt=mqtt, drop=dropped, passed=False)
                report['cases'].append(row)
                scratch = root/name
                scratch.mkdir()
                try:
                    row.update(case(args, forms[label], transport, mqtt, FAULTS.index(dropped), scratch, name))
                except Exception as error:
                    row['error'] = repr(error)
                    raise
                row['passed'] = True
                print(name, 'passed', flush=True)
        report['complete'] = True
    finally:
        args.report.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')


if __name__ == '__main__':
    main()
