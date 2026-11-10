"""Automatic controller over the independent pinned broker/proxy ACK cuts."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import platform
import shutil
import subprocess
from run_session_broker import case


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--broker', required=True, type=Path)
    parser.add_argument('--client', required=True, type=Path)
    parser.add_argument('--report', required=True, type=Path)
    parser.add_argument('--proxy-host', default='127.0.0.1')
    parser.add_argument('--windows-python', type=Path, default=shutil.which('python.exe'))
    args = parser.parse_args()
    broker, client = args.broker.resolve(), args.client.resolve()
    version = subprocess.run([str(broker), '-h'], capture_output=True, text=True, timeout=15)
    assert 'mosquitto version 2.1.2' in version.stdout.splitlines()
    digest = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    report = dict(schema=1, complete=False, scope='Automatic volatile numeric TCP controller recovery; five outgoing/incoming ACK cuts, MQTT5/311, two packet-work budgets; no TLS/DNS/Auth/durable/WebSocket/performance/production acceptance', broker_sha256=digest(broker), client_sha256=digest(client), runner_sha256=digest(__file__), platform=platform.platform(), cases=[])
    args.report.parent.mkdir(parents=True, exist_ok=True)
    try:
        for version in [5, 311]:
            for mode in range(5):
                for fragmented in [0, 1]:
                    row = case(broker, client, version, mode, fragmented, args.report.parent / 'logs', f'{args.report.stem}-{version}-{mode}-{fragmented}', args.windows_python, args.proxy_host)
                    report['cases'].append(row)
                    args.report.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
                    print(version, mode, fragmented, row['passed'], row.get('error', ''), flush=True)
                    assert row['passed'], row.get('error', 'automatic case failed')
        report['complete'] = True
    finally:
        args.report.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
