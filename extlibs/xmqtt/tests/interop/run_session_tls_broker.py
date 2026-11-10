"""Opt-in independent TLS/Mosquitto volatile Session recovery matrix.

Runtime-only CA/client keys are removed before return. TLS terminates at the
bounded proxy and the independent broker uses local TCP. This is correctness/
lifetime evidence, not throughput, system DNS, durable or production acceptance.
"""
from pathlib import Path
import argparse
import itertools
import json
import os
import platform
import shutil
import subprocess
import tempfile

from run_mqtt_tls import certificates, digest
from run_session_broker import case
from session_tls_proxy import TLSProxy
import ssl


def contexts(root, tls, mutual):
    def create():
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.minimum_version = context.maximum_version = ssl.TLSVersion.TLSv1_2 if tls == 12 else ssl.TLSVersion.TLSv1_3
        context.set_ciphers('ECDHE-RSA-AES128-GCM-SHA256')
        context.set_alpn_protocols(['mqtt'])
        context.load_cert_chain(root / 'server.pem', root / 'server.key')
        if mutual:
            context.verify_mode = ssl.CERT_REQUIRED
            context.load_verify_locations(root / 'ca.pem')
        return context
    return create


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--broker', type=Path, required=True)
    parser.add_argument('--client', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--proxy-host', default='127.0.0.1')
    parser.add_argument('--windows-python', type=Path, default=shutil.which('python.exe'))
    parser.add_argument('--openssl', default=shutil.which('openssl'))
    parser.add_argument('--tls', nargs='+', type=int, choices=[12, 13], default=[12, 13])
    parser.add_argument('--mqtt', nargs='+', type=int, choices=[5, 311], default=[5, 311])
    parser.add_argument('--factories', nargs='+', type=int, choices=[0, 1, 2], default=[0, 1, 2])
    parser.add_argument('--scenarios', nargs='+', choices=['full', 'mutual', 'resume', 'fallback'], default=['full', 'mutual', 'resume', 'fallback'])
    parser.add_argument('--faults', nargs='+', choices=['ack', 'prefix'], default=['ack', 'prefix'])
    parser.add_argument('--modes', nargs='+', type=int, choices=range(5), default=list(range(5)))
    parser.add_argument('--fragmented', nargs='+', type=int, choices=[0, 1], default=[0, 1])
    args = parser.parse_args()
    assert args.openssl
    args.report.parent.mkdir(parents=True, exist_ok=True)
    report = dict(schema=1, complete=False, scope=__doc__, broker_sha256=digest(args.broker),
                  client_sha256=digest(args.client), runner_sha256=digest(__file__),
                  proxy_sha256=digest(Path(__file__).with_name('session_tls_proxy.py')),
                  fixture_platform=platform.platform(), cases=[])
    temporary = None
    try:
        with tempfile.TemporaryDirectory(prefix='tls-session-keys-', dir=args.report.parent) as temporary:
            root = Path(temporary)
            report['public_certificates'] = certificates(root, args.openssl)
            def path(name):
                value = str((root / name).resolve())
                return subprocess.check_output(['wslpath', '-w', value], text=True).strip() if args.client.suffix.lower() == '.exe' and os.name != 'nt' else value
            for tls, mqtt, factory, scenario, fault, fragmented in itertools.product(args.tls, args.mqtt, args.factories, args.scenarios, args.faults, args.fragmented):
                if tls == 12 and scenario in ['resume', 'fallback']:
                    continue
                modes = args.modes if fault == 'ack' else [0]
                for mode in modes:
                    mutual = scenario == 'mutual'
                    resume = 1 if scenario == 'resume' else 2 if scenario == 'fallback' else 0
                    create = contexts(root, tls, mutual)
                    def proxy(backend, version, drop, host):
                        return TLSProxy(backend, version, drop, host, create, tls, mutual, resume, fault == 'prefix')
                    extra = [tls, factory, path('ca.pem'), path('client.pem') if mutual else '-', path('client.key') if mutual else '-', resume, int(fault == 'prefix')]
                    name = f'{args.report.stem}-t{tls}-v{mqtt}-f{factory}-{scenario}-{fault}-m{mode}-b{fragmented}'
                    row = case(args.broker.resolve(), args.client.resolve(), mqtt, mode, fragmented,
                               args.report.parent / 'logs', name, args.windows_python, args.proxy_host,
                               proxy_factory=proxy, client_args=extra)
                    row.update(tls=tls, factory=factory, scenario=scenario, fault=fault)
                    report['cases'].append(row)
                    args.report.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
                    print(tls, mqtt, factory, scenario, fault, mode, fragmented, row['passed'], row.get('error', ''), flush=True)
                    assert row['passed'], row
        report['complete'] = True
    finally:
        report['runtime_material_removed'] = temporary is not None and not Path(temporary).exists()
        args.report.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
