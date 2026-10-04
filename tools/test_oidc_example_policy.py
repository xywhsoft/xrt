"""Check the actual OIDC example using independently signed Python JWT fixtures."""
from __future__ import annotations

import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import queue
import re
import subprocess
import sys
import threading
import time

from cryptography.hazmat.primitives import hashes, serialization
from cryptography import __version__ as cryptography_version
from cryptography.hazmat.primitives.asymmetric import ec, padding, utils

ROOT = Path(__file__).resolve().parents[1]

def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def inputs() -> dict[str, str]:
    files = [ROOT / 'single/xrt.h', ROOT / 'tools/test_oidc_example_policy.py']
    for name in ['xjwt', 'xoauth2']:
        library = ROOT / 'extlibs' / name
        files += [*library.glob('*.h'), library / (name + '.c')]
        files += [p for directory in ['src', 'examples'] for p in (library / directory).rglob('*') if p.suffix in {'.c', '.h'}]
    files += [ROOT / 'extlibs/xjwt/tests/test_keys.h', ROOT / 'extlibs/xoauth2/tests/oidc_keys.h', ROOT / 'extlibs/xoauth2/tests/test_oidc_idtoken_policy.c']
    return {path.relative_to(ROOT).as_posix(): sha(path) for path in sorted(set(files))}

def constant(path: Path, name: str) -> bytes:
    match = re.search(r'\b' + re.escape(name) + r'\[\]\s*=\s*((?:"(?:[^"\\]|\\.)*"\s*)+);', path.read_text(encoding='utf-8'))
    if match is None:
        raise ValueError('missing fixture constant: ' + name)
    return ''.join(json.loads(part) for part in re.findall(r'"(?:[^"\\]|\\.)*"', match[1])).encode()

def b64(data: bytes) -> str:
    return base64.urlsafe_b64encode(data).decode('ascii').rstrip('=')

def number(value: int) -> str:
    return b64(value.to_bytes((value.bit_length() + 7) // 8, 'big'))

def sign(claims: dict, private, alg: str, kid: str) -> str:
    compact = lambda value: json.dumps(value, separators=(',', ':'), ensure_ascii=True).encode()
    message = (b64(compact({'alg': alg, 'kid': kid})) + '.' + b64(compact(claims))).encode('ascii')
    if alg == 'ES256':
        r, s = utils.decode_dss_signature(private.sign(message, ec.ECDSA(hashes.SHA256())))
        signature = r.to_bytes(32, 'big') + s.to_bytes(32, 'big')
    else:
        signature = private.sign(message, padding.PKCS1v15(), hashes.SHA256())
    return message.decode() + '.' + b64(signature)

CASES = {
    'baseline': (True, 'required'),
    'missing-exp': (False, 'required'),
    'missing-iat': (False, 'required'),
    'string-iat': (False, 'required'),
    'future-iat': (False, 'application-policy'),
    'unknown-extra-audience': (False, 'required'),
    'nonstring-extra-audience': (False, 'required'),
    'wrong-azp': (False, 'application-policy'),
    'wrong-at-hash': (False, 'application-policy'),
    'alternate-signing-algorithm': (False, 'registration-policy'),
    'non-ascii-subject': (False, 'required'),
    'subject-255': (True, 'required'),
    'subject-256': (False, 'required'),
    'one-audience-array': (True, 'required'),
    'matching-azp': (True, 'application-policy'),
    'matching-at-hash': (True, 'application-policy'),
    'expired': (False, 'required'),
    'wrong-nonce': (False, 'required'),
    'wrong-userinfo-subject': (False, 'required'),
    'missing-id-email': (True, 'optional-claim'),
    'missing-userinfo-email': (True, 'optional-claim'),
    'old-iat': (False, 'application-policy'),
}

REFRESH_CASES = {
    'refresh-same': (True, 'required'),
    'refresh-no-id-token': (True, 'optional-claim'),
    'refresh-nonce-omitted': (True, 'required'),
    'refresh-nonce-same': (True, 'required'),
    'refresh-nonce-changed': (False, 'required'),
    'refresh-subject-changed': (False, 'required'),
    'refresh-issuer-changed': (False, 'required'),
    'refresh-audience-changed': (False, 'required'),
    'refresh-audience-order': (True, 'audience-set-policy'),
    'refresh-one-audience-array': (True, 'audience-set-policy'),
    'refresh-added-trusted-audience': (False, 'required'),
    'refresh-auth-time-changed': (False, 'required'),
    'refresh-auth-time-omitted': (True, 'optional-claim'),
    'refresh-original-no-auth-time': (True, 'optional-claim'),
    'refresh-exp-missing': (False, 'required'),
    'refresh-iat-missing': (False, 'required'),
    'refresh-future-iat': (False, 'application-policy'),
    'refresh-old-iat': (False, 'application-policy'),
    'refresh-wrong-at-hash': (False, 'application-policy'),
    'refresh-algorithm-changed': (False, 'registration-policy'),
    'refresh-non-bearer-with-id-token': (False, 'application-token-policy'),
    'refresh-non-bearer-without-id-token': (False, 'application-token-policy'),
}

def read_line(stream) -> str:
    pending = queue.Queue()
    threading.Thread(target=lambda: pending.put(stream.readline()), daemon=True).start()
    return pending.get(timeout=15)

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default='gcc')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--observe-original', action='store_true', help='preserve original behavior; require concrete old defects')
    parser.add_argument('--initial-only', action='store_true')
    parser.add_argument('--case', action='append', choices=[*CASES, *REFRESH_CASES], help='select focused policy cases')
    args = parser.parse_args()
    if args.sanitize and os.name == 'nt': parser.error('sanitizer requires Linux')
    output = args.output_dir.resolve(); output.mkdir(parents=True, exist_ok=False)
    before = inputs()
    private = serialization.load_pem_private_key(constant(ROOT / 'extlibs/xoauth2/tests/oidc_keys.h', 'OIDC_EC_PRIV'), password=None)
    public = private.public_key().public_numbers()
    rsa_private = serialization.load_pem_private_key(constant(ROOT / 'extlibs/xjwt/tests/test_keys.h', 'K_RSA_PRIV'), password=None)
    rsa_public = rsa_private.public_key().public_numbers()
    jwks = {'keys': [
        {'kty': 'EC', 'kid': 'oidc-ec-1', 'use': 'sig', 'crv': 'P-256', 'x': b64(public.x.to_bytes(32, 'big')), 'y': b64(public.y.to_bytes(32, 'big'))},
        {'kty': 'RSA', 'kid': 'independent-rsa-1', 'use': 'sig', 'n': number(rsa_public.n), 'e': number(rsa_public.e)},
    ]}
    binary = output / ('oidc_policy.exe' if os.name == 'nt' else 'oidc_policy')
    command = [args.compiler, '-std=c11', *([] if os.name == 'nt' else ['-D_GNU_SOURCE']),
        '-O1' if args.sanitize else '-O2', '-Wall', '-Wextra', '-Werror',
        *(['-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if args.sanitize else []),
        '-I', str(ROOT / 'single'), '-I', str(ROOT / 'extlibs/xjwt'),
        str(ROOT / 'extlibs/xoauth2/tests/test_oidc_idtoken_policy.c'),
        str(ROOT / 'extlibs/xoauth2/xoauth2.c'), str(ROOT / 'extlibs/xjwt/xjwt.c'), '-o', str(binary),
        *(['-lws2_32', '-lbcrypt', '-ladvapi32', '-liphlpapi'] if os.name == 'nt' else ['-pthread', '-lm'])]
    build = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
    (output / 'build.log').write_text(build.stdout + build.stderr, encoding='utf-8')
    if build.returncode: raise RuntimeError('policy fixture build failed: ' + build.stderr)
    records = []
    cases = dict(CASES)
    if not args.initial_only and not args.observe_original: cases.update(REFRESH_CASES)
    if args.case:
        if any(name not in cases for name in args.case): parser.error('selected case excluded by current mode')
        cases = {name: cases[name] for name in dict.fromkeys(args.case)}
    for name, (expected, kind) in cases.items():
        refresh = name in REFRESH_CASES
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        process = subprocess.Popen([str(binary), *(['refresh'] if refresh else [])], cwd=ROOT, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, text=True, encoding='utf-8', env=env)
        try:
            for _ in range(8):
                line = read_line(process.stdout)
                if line.startswith('FIXTURE '): break
                if not line: raise RuntimeError('example exited before independent fixture request')
            else: raise RuntimeError('independent fixture request missing')
            claims = json.loads(line[len('FIXTURE '):]); now = int(time.time())
            claims.update(exp=now + 600, iat=now)
            if name == 'missing-exp': del claims['exp']
            elif name == 'missing-iat': del claims['iat']
            elif name == 'string-iat': claims['iat'] = str(now)
            elif name == 'future-iat': claims.update(iat=now + 3600, exp=now + 7200)
            elif name == 'old-iat': claims['iat'] = now - 3600
            elif name == 'unknown-extra-audience': claims['aud'] = ['demo-client-id', 'untrusted-api']
            elif name == 'nonstring-extra-audience': claims['aud'] = ['demo-client-id', 42]
            elif name == 'wrong-azp': claims['azp'] = 'another-client'
            elif name == 'matching-azp': claims['azp'] = 'demo-client-id'
            elif name == 'wrong-at-hash': claims['at_hash'] = 'deliberately-wrong'
            elif name == 'matching-at-hash': claims['at_hash'] = b64(hashlib.sha256(b'acc-demo').digest()[:16])
            elif name == 'non-ascii-subject': claims['sub'] = 'user-\u00e9'
            elif name in {'subject-255', 'subject-256'}: claims['sub'] = 's' * int(name.split('-')[1])
            elif name == 'one-audience-array': claims['aud'] = ['demo-client-id']
            elif name == 'expired': claims['exp'] = now - 60
            elif name == 'wrong-nonce': claims['nonce'] = 'wrong-nonce'
            elif name == 'missing-id-email': del claims['email']
            alternate = name == 'alternate-signing-algorithm'
            token = sign(claims, rsa_private if alternate else private, 'RS256' if alternate else 'ES256', 'independent-rsa-1' if alternate else 'oidc-ec-1')
            user = {'sub': 'wrong-user' if name == 'wrong-userinfo-subject' else claims['sub'], 'email': 'alice@example.com'}
            if name == 'missing-userinfo-email': del user['email']
            payload = token + '\n' + json.dumps(jwks, separators=(',', ':')) + '\n' + json.dumps(user, separators=(',', ':')) + '\n'
            if refresh:
                original = dict(claims, iat=now - 5, auth_time=now - 60)
                if name in {'refresh-audience-changed', 'refresh-audience-order'}:
                    original['aud'] = ['demo-client-id', 'trusted-api']
                replacement = dict(original, iat=now, exp=now + 600)
                del replacement['nonce']
                replacement['at_hash'] = b64(hashlib.sha256(b'new-access').digest()[:16])
                if name == 'refresh-nonce-same': replacement['nonce'] = original['nonce']
                elif name == 'refresh-nonce-changed': replacement['nonce'] = 'another-nonce'
                elif name == 'refresh-subject-changed': replacement['sub'] = 'another-subject'
                elif name == 'refresh-issuer-changed': replacement['iss'] = 'https://another.example'
                elif name == 'refresh-audience-changed': replacement['aud'] = 'demo-client-id'
                elif name == 'refresh-audience-order': replacement['aud'] = list(reversed(original['aud']))
                elif name == 'refresh-one-audience-array': replacement['aud'] = ['demo-client-id']
                elif name == 'refresh-added-trusted-audience': replacement['aud'] = ['demo-client-id', 'trusted-api']
                elif name == 'refresh-auth-time-changed': replacement['auth_time'] = now
                elif name == 'refresh-auth-time-omitted': del replacement['auth_time']
                elif name == 'refresh-original-no-auth-time': del original['auth_time']
                elif name == 'refresh-exp-missing': del replacement['exp']
                elif name == 'refresh-iat-missing': del replacement['iat']
                elif name == 'refresh-future-iat': replacement.update(iat=now + 3600, exp=now + 7200)
                elif name == 'refresh-old-iat': replacement['iat'] = now - 3600
                elif name == 'refresh-wrong-at-hash': replacement['at_hash'] = 'wrong'
                changed_alg = name == 'refresh-algorithm-changed'
                initial_token = sign(original, private, 'ES256', 'oidc-ec-1')
                replacement_token = sign(replacement, rsa_private if changed_alg else private,
                    'RS256' if changed_alg else 'ES256', 'independent-rsa-1' if changed_alg else 'oidc-ec-1')
                initial_body = {'access_token': 'acc-demo', 'token_type': 'Bearer', 'expires_in': 3600, 'refresh_token': 'rt-original', 'id_token': initial_token}
                replacement_body = {'access_token': 'new-access', 'token_type': 'Bearer', 'expires_in': 3600, 'refresh_token': 'rt-replacement'}
                if name.startswith('refresh-non-bearer-'): replacement_body['token_type'] = 'mac'
                if name not in {'refresh-no-id-token', 'refresh-non-bearer-without-id-token'}:
                    replacement_body['id_token'] = replacement_token
                payload = json.dumps(jwks, separators=(',', ':')) + '\n' + json.dumps(initial_body, separators=(',', ':')) + '\n' + json.dumps(replacement_body, separators=(',', ':')) + '\n'
            stdout, stderr = process.communicate(payload, timeout=20)
            (output / (name + '.log')).write_text(stdout + stderr, encoding='utf-8')
            if process.returncode not in (0, 1) or f'POLICY_RESULT {process.returncode}' not in stdout:
                raise AssertionError((name, process.returncode, stderr))
            actual = process.returncode == 0
            records.append({'case': name, 'rule': kind, 'expected_accept': expected, 'actual_accept': actual, 'exit_code': process.returncode})
            print('[policy]', name, 'accepted', actual, 'expected', expected, flush=True)
            if not args.observe_original and actual != expected: raise AssertionError('policy mismatch: ' + name)
        finally:
            if process.poll() is None: process.kill(); process.wait()
    assert inputs() == before, 'policy source/fixture changed during execution'
    if args.observe_original:
        by_name = {record['case']: record for record in records}
        assert by_name['baseline']['actual_accept']
        for name in ['missing-exp', 'missing-iat', 'string-iat', 'unknown-extra-audience', 'alternate-signing-algorithm']:
            assert by_name[name]['actual_accept'], 'original defect not reproduced: ' + name
    compiler_version = subprocess.run([args.compiler, '--version'], text=True, capture_output=True, check=True).stdout.splitlines()[0]
    report = {'source_inputs': before, 'platform': sys.platform, 'sanitize': args.sanitize,
              'build_command': command, 'compiler_version': compiler_version,
              'python_version': sys.version, 'cryptography_version': cryptography_version,
              'observed_original': args.observe_original, 'cases': records, 'binary': str(binary), 'binary_sha256': sha(binary),
              'all_expected': all(record['expected_accept'] == record['actual_accept'] for record in records)}
    (output / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    return 0

if __name__ == '__main__': raise SystemExit(main())
