"""Independently parse Compose stdout and check an actual failed output sink."""
from __future__ import annotations

import argparse
import datetime
import email.policy
from email.parser import BytesParser
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default='gcc')
    parser.add_argument('--binary', type=Path, help='check an already built Compose example')
    parser.add_argument('--output-dir', type=Path)
    args = parser.parse_args()
    if args.binary is None:
        subprocess.run([sys.executable, str(ROOT/'tools/build.py'), '--compiler', args.compiler,
                        '--manifest', 'extlibs/xmail/config/modules.json', '--suite', 'mail_compose',
                        '--no-single', '--jobs', '4'], cwd=ROOT, check=True)
        binary = ROOT/'out'/Path(args.compiler).stem.lower()/'native/mail_compose'/('mail_compose_main.exe' if os.name == 'nt' else 'mail_compose_main')
    else:
        binary = args.binary.resolve()
    output = args.output_dir or ROOT/'out/mail_example_output'/sys.platform
    output.mkdir(parents=True, exist_ok=args.output_dir is None)
    binary_sha = hashlib.sha256(binary.read_bytes()).hexdigest()
    report = {'started_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(), 'platform': sys.platform,
              'binary_sha256': binary_sha, 'binary': str(binary), 'checks': {}, 'all_passed': False}
    stage = 'MIME_stdout'
    try:
        result = subprocess.run([str(binary)], capture_output=True, timeout=25)
        raw = result.stdout
        parsed = BytesParser(policy=email.policy.default).parsebytes(raw)
        report['checks']['MIME_stdout'] = {'exit_code': result.returncode, 'bytes': len(raw),
            'CRCRLF_count': raw.count(b'\r\r\n'), 'canonical_CRLF': re.search(rb'\r(?!\n)|(?<!\r)\n', raw) is None,
            'subject_matches': str(parsed['Subject']) == '示例邮件', 'body_matches': parsed.get_content().strip() == 'hello from xmail',
            'stdout_sha256': hashlib.sha256(raw).hexdigest()}
        item = report['checks']['MIME_stdout']
        assert result.returncode == 0 and raw and item['canonical_CRLF'] and item['subject_matches'] and item['body_matches']
        stage = 'read_only_output_sink'
        with tempfile.TemporaryDirectory(prefix='xrt-compose-output-') as directory:
            sink = Path(directory)/'read-only.bin'
            sentinel = b'unchanged output sink\n'
            sink.write_bytes(sentinel)
            with sink.open('rb') as stream:
                failed = subprocess.run([str(binary)], stdout=stream, stderr=subprocess.PIPE, timeout=25)
            unchanged = sink.read_bytes() == sentinel
        report['checks']['read_only_output_sink'] = {'exit_code': failed.returncode, 'sink_unchanged': unchanged}
        assert failed.returncode != 0 and unchanged
        report['all_passed'] = True
    except (AssertionError, subprocess.TimeoutExpired) as error:
        report['failure'] = {'stage': stage, 'type': type(error).__name__}
    finally:
        assert hashlib.sha256(binary.read_bytes()).hexdigest() == binary_sha
        report['binary_before_after_verified'] = True
        report['finished_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        (output/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print('[Compose stdout]', 'passed' if report['all_passed'] else 'failed', 'platform', sys.platform, 'stage', stage, flush=True)
    return 0 if report['all_passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
