"""Download and SHA256-verify Ubuntu Dovecot/Postfix packages without installing.

Uses the configured APT repository metadata. Dovecot 2.4 is required by the
private test fixture (Ubuntu 26.04 currently supplies it). The runtime manifest
records exact package versions, package hashes and extracted file hashes.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import shutil

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', required=True, type=Path)
parser.add_argument('--reuse-packages', type=Path, help='previous runtime directory; cached packages are reverified')
args = parser.parse_args()
if sys.platform != 'linux':
    parser.error('requires Linux with apt-cache, apt-get and dpkg-deb')
policy = subprocess.check_output(['apt-cache', 'policy', 'dovecot-core'], text=True)
candidate = next(line.split('Candidate:', 1)[1].strip() for line in policy.splitlines() if 'Candidate:' in line)
if not candidate.split(':')[-1].startswith('2.4.'):
    parser.error('the fixture requires Dovecot 2.4; use a matching Ubuntu repository/environment')
root = args.output.resolve()
root.mkdir(parents=True, exist_ok=False)
download = root / 'packages'
download.mkdir()
extracted = root / 'extracted'
extracted.mkdir()
packages = ['dovecot-core', 'dovecot-imapd', 'dovecot-pop3d', 'dovecot-lmtpd', 'dovecot-sieve', 'postfix', 'libexttextcat-2.0-0', 'liblua5.4-0', 'libpcre2-32-0', 'libicu78', 'libunwind8', 'libnsl2']
manifest = {'packages': [], 'system_packages_installed': False}
for package in packages:
    policy = subprocess.check_output(['apt-cache', 'policy', package], text=True)
    version = next(line.split('Candidate:', 1)[1].strip() for line in policy.splitlines() if 'Candidate:' in line)
    metadata = subprocess.check_output(['apt-cache', 'show', package + '=' + version], text=True)
    expected = next(line[8:] for line in metadata.splitlines() if line.startswith('SHA256: '))
    before = set(download.glob('*.deb'))
    cached = next((p for p in (args.reuse_packages / 'packages').glob('*.deb') if hashlib.sha256(p.read_bytes()).hexdigest() == expected), None) if args.reuse_packages else None
    if cached is not None:
        shutil.copy2(cached, download / cached.name)
        result = subprocess.CompletedProcess([], 0, 'Copied previously SHA256-verified package\n', '')
    else:
        result = subprocess.run(['apt-get', 'download', package + '=' + version], cwd=download, text=True, capture_output=True, timeout=180)
    (root / (package + '.download.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
    assert result.returncode == 0, (package, result.stderr)
    created = set(download.glob('*.deb')) - before
    assert len(created) == 1
    file = created.pop()
    assert hashlib.sha256(file.read_bytes()).hexdigest() == expected
    subprocess.run(['dpkg-deb', '-x', str(file), str(extracted)], check=True)
    manifest['packages'].append({'name': package, 'version': version, 'file': file.name, 'sha256': expected})
    print('[downloaded and verified]', package, version, flush=True)
env = dict(os.environ)
env['LD_LIBRARY_PATH'] = ':'.join(str(extracted / p) for p in ['usr/lib/x86_64-linux-gnu', 'usr/lib/dovecot', 'usr/lib/postfix'])
for name, executable in [('dovecot', 'usr/sbin/dovecot'), ('postfix', 'usr/sbin/postconf')]:
    command = [str(extracted / executable), '--version'] if name == 'dovecot' else [str(extracted / executable), '-d', 'mail_version']
    result = subprocess.run(command, env=env, text=True, capture_output=True)
    print(name, result.returncode, result.stdout, result.stderr, flush=True)
    manifest[name] = {'exit_code': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr}
manifest['files'] = {p.relative_to(extracted).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in extracted.rglob('*') if p.is_file()}
manifest['host_openssl'] = subprocess.check_output(['openssl', 'version'], text=True).strip()
(root / 'runtime.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
assert all(manifest[name]['exit_code'] == 0 for name in ['dovecot', 'postfix'])
