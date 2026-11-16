"""Real automatic MQTT recovery through independent WS/OpenSSL-to-TCP transport.

Mosquitto remains the actual protocol/session peer. This is a transport gateway
qualification, not broker-native WS, broker restart or durable-state acceptance.
All forms and OS runs are sequential; each case reaps its processes and threads.
"""
import argparse
import hashlib
import json
import platform
import os
from pathlib import Path
import shutil
import ssl
import subprocess
from run_session_broker import case
from reconnect_ws_gateway import WebSocketProxy


def digest(path):
    with Path(path).open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def main():
    p = argparse.ArgumentParser(__doc__)
    p.add_argument('--broker', type=Path, required=True)
    p.add_argument('--certificate', type=Path, required=True)
    p.add_argument('--key', type=Path, required=True)
    p.add_argument('--ca', type=Path, required=True)
    p.add_argument('--windows-ca', required=True)
    p.add_argument('--windows-python', type=Path, default=shutil.which('python.exe'))
    p.add_argument('--form', nargs=2, action='append', metavar=('LABEL', 'CLIENT'), required=True)
    p.add_argument('--report', type=Path, required=True)
    p.add_argument('--versions', type=int, nargs='+', choices=[5, 311], default=[5, 311])
    p.add_argument('--transports', type=int, nargs='+', choices=[0, 12, 13], default=[0, 12, 13])
    p.add_argument('--modes', type=int, nargs='+', choices=range(5), default=list(range(5)))
    p.add_argument('--fragmented', type=int, nargs='+', choices=[0, 1], default=[0, 1])
    a = p.parse_args()
    version = subprocess.run([str(a.broker), '-h'], capture_output=True, text=True, timeout=15)
    assert version.returncode == 0 and 'mosquitto version 2.0.22' in version.stdout.splitlines(), version.stdout + version.stderr
    report = dict(schema=1, complete=False, scope=__doc__, platform=platform.platform(), ssl=ssl.OPENSSL_VERSION,
                  broker_sha256=digest(a.broker), broker_version='2.0.22',
                  certificate_sha256=digest(a.certificate), ca_sha256=digest(a.ca),
                  runner_sha256=digest(__file__), gateway_sha256=digest(Path(__file__).with_name('reconnect_ws_gateway.py')),
                  mqtt_proxy_sha256=digest(Path(__file__).with_name('run_session_broker.py')), private_keys_recorded=False,
                  forms={n:dict(path=str(Path(path).resolve()), sha256=digest(path)) for n, path in a.form}, cases=[])
    a.report.parent.mkdir(parents=True, exist_ok=True)
    try:
        for name, path in a.form:
            client = Path(path).resolve()
            for tls in a.transports:
                for mqtt in a.versions:
                    for mode in a.modes:
                        for fragmented in a.fragmented:
                            windows_python = a.windows_python if client.suffix.lower() == '.exe' and os.name != 'nt' else None
                            def factory(backend, v, m, host):
                                return WebSocketProxy(backend, v, m, host, tls=tls, split=bool(fragmented),
                                                      cert=a.certificate, key=a.key, windows_python=windows_python)
                            label = f'{a.report.stem}-{name}-{tls}-{mqtt}-{mode}-{fragmented}'
                            ca = a.windows_ca if client.suffix == '.exe' else str(a.ca.resolve())
                            row = case(a.broker, client, mqtt, mode, fragmented, a.report.parent / 'logs', label,
                                       a.windows_python, proxy_factory=factory, client_args=(str(int(tls != 0)), ca))
                            row.update(form=name, tls=tls)
                            report['cases'].append(row)
                            a.report.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
                            print(name, tls, mqtt, mode, fragmented, row['passed'], row.get('error', ''), flush=True)
                            assert row['passed'], row.get('error')
        report['complete'] = True
    finally:
        a.report.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
