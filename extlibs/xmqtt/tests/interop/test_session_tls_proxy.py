"""Adversarial framing and handshake-interruption controls for TLS test IO."""
from pathlib import Path
import argparse
import json
import socket
import tempfile
import time

from run_mqtt_tls import certificates
from run_session_broker import case
from run_session_tls_broker import contexts
from session_tls_proxy import TLSProxy, extract, metadata, LIMIT


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--broker', type=Path, required=True)
    parser.add_argument('--client', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    args.report.parent.mkdir(parents=True, exist_ok=True)
    checks = []
    for wire in [b'\x30\x80\x80\x80\x80', b'\x30\xff\xff\xff\x7f']:
        try:
            extract(wire)
        except ValueError:
            checks.append('malformed/capped Remaining Length refused')
        else:
            raise AssertionError('invalid framing accepted')
    for kind, body in [(0x20, b'\0'), (0x32, b'\0\0'), (0x40, b'')]:
        try:
            metadata(bytes([kind, len(body)]) + body, body, 1, 'cs')
        except ValueError:
            checks.append('short metadata refused')
        else:
            raise AssertionError('short metadata accepted')
    assert extract(b'\x30') is None and extract(b'\x30\x80') is None
    assert extract(b'\x30\x80\x80\x01') == (4, 16384)
    assert LIMIT == 2 * 1024 * 1024
    with tempfile.TemporaryDirectory(prefix='tls-controls-', dir=args.report.parent) as name:
        root = Path(name)
        public = certificates(root, 'openssl')
        create = contexts(root, 13, False)
        for handshake in [False, True]:
            proxy = TLSProxy(0, 5, 0, '127.0.0.1', create, 13)
            peer = socket.create_connection(('127.0.0.1', proxy.port), timeout=2)
            if handshake:
                peer.sendall(b'\x16\x03\x01\x00\x40\x01')
            end = time.monotonic() + 3
            while len(proxy.connections) < (2 if handshake else 1):
                assert time.monotonic() < end, 'specific accept/handshake reached'
                time.sleep(.001)
            proxy.close()
            peer.close()
            assert not proxy.acceptor.is_alive() and not proxy.errors
            checks.append('mid-handshake joined' if handshake else 'pre-handshake joined')
        # No TLS args: this real probe must exit before connection. Cleanup of
        # broker, proxy and stdout/stderr readers is still required.
        def factory(backend, version, mode, host):
            return TLSProxy(backend, version, mode, host, create, 13)
        row = case(args.broker.resolve(), args.client.resolve(), 5, 0, 1,
                   args.report.parent / 'control-logs', 'early-exit', proxy_factory=factory)
        assert not row['passed'] and row['probe_exit'] == 2
        assert all(row[key] for key in ['probe_reaped', 'readers_reaped', 'proxy_reaped', 'broker_reaped'])
        checks.append('early probe exit cleanup verified')
    args.report.write_text(json.dumps(dict(complete=True, checks=checks, expected_failure=row,
                                         public_certificates=public,
                                         runtime_material_removed=not root.exists()), indent=2) + '\n', encoding='utf-8')
    print('TLS proxy controls passed', len(checks), flush=True)


if __name__ == '__main__':
    main()
