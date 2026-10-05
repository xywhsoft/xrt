"""Test real Hydra HTTPS: independent oracle, C fault controls and OIDC example.

Run on Linux with the checksum-pinned Hydra v2.3.0 SQLite binary. Optionally
exercise a separately frozen Windows checkout through Windows Python in WSL.
Authorization codes, cookies, client secrets and tokens remain in memory.
"""
from __future__ import annotations
import argparse
import base64
import hashlib
import http.cookiejar
import json
import os
from pathlib import Path
import queue
import secrets
import socket
import ssl
import subprocess
import sys
import threading
import time
import urllib.error
import urllib.parse
import urllib.request

from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import padding, rsa
from cryptography import __version__ as cryptography_version
from hydra_test_inputs import snapshot, sha256
from test_mail_tls_interop import certificates

ROOT = Path(__file__).resolve().parents[1]
HYDRA_SHA256 = 'd34aa4c8f89ca0cf70d29a37506041c542c8351e5852e5a2dcc206134ddac127'
CLIENTS = {'probe': 'extlibs/xoauth2/tests/test_hydra_interop_client.c',
           'live': 'extlibs/xoauth2/examples/oidc_live.c'}
STYLES = {'basic': 'client_secret_basic', 'body': 'client_secret_post', 'public': 'none'}


def timed_line(stream, timeout=30):
    pending = queue.Queue()
    threading.Thread(target=lambda: pending.put(stream.readline()), daemon=True).start()
    return pending.get(timeout=timeout)


def encode(data):
    return base64.urlsafe_b64encode(data).decode('ascii').rstrip('=')


def decode(data):
    return base64.urlsafe_b64decode(data + '=' * (-len(data) % 4))


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args): return None


class Provider:
    def __init__(self, issuer, admin, redirect, ca):
        self.issuer, self.admin, self.redirect = issuer, admin, redirect
        self.opener = urllib.request.build_opener(
            urllib.request.HTTPSHandler(context=ssl.create_default_context(cafile=ca)),
            urllib.request.HTTPCookieProcessor(http.cookiejar.CookieJar()), NoRedirect())

    def request(self, url, method='GET', data=None, headers=None):
        body = json.dumps(data).encode() if isinstance(data, dict) else data
        supplied = {'Content-Type': 'application/json'} if isinstance(data, dict) else {}
        supplied.update(headers or {})
        request = urllib.request.Request(url, data=body, headers=supplied, method=method)
        try: response = self.opener.open(request, timeout=10)
        except urllib.error.HTTPError as error: response = error
        with response:
            body = response.read(1048577)
            if len(body) > 1048576: raise ValueError('oversized Hydra oracle response')
            return response.status, response.headers, json.loads(body) if body.startswith(b'{') else body

    def authorize(self, url, client_id):
        origin = urllib.parse.urlsplit(url)
        expected = urllib.parse.urlsplit(self.discovery['authorization_endpoint'])
        assert (origin.scheme, origin.netloc, origin.path) == (expected.scheme, expected.netloc, expected.path)
        initial = urllib.parse.parse_qs(origin.query, strict_parsing=True)
        assert initial['client_id'] == [client_id] and initial['redirect_uri'] == [self.redirect]
        assert initial['response_type'] == ['code'] and initial['code_challenge_method'] == ['S256']
        assert len(initial['state']) == len(initial['nonce']) == len(initial['code_challenge']) == 1
        for _ in range(16):
            # Login/consent external URLs are intercepted, never followed.
            assert urllib.parse.urlsplit(url).netloc == urllib.parse.urlsplit(self.issuer).netloc
            status, headers, _ = self.request(url)
            assert status in (302, 303), ('authorization status', status)
            url = urllib.parse.urljoin(url, headers['Location'])
            parts = urllib.parse.urlsplit(url)
            query = urllib.parse.parse_qs(parts.query)
            callback = urllib.parse.urlsplit(self.redirect)
            if (parts.scheme, parts.netloc, parts.path) == (callback.scheme, callback.netloc, callback.path):
                assert query.get('state') == initial['state'] and len(query.get('code', [])) == 1
                return query['code'][0], query['state'][0]
            if (parts.scheme, parts.netloc, parts.path) == ('https', 'login.example.test', '/login'):
                challenge = query['login_challenge'][0]
                status, _, reply = self.request(self.admin + '/admin/oauth2/auth/requests/login/accept?' +
                    urllib.parse.urlencode({'login_challenge': challenge}), 'PUT', {'subject': 'user-42', 'remember': False})
            elif (parts.scheme, parts.netloc, parts.path) == ('https', 'login.example.test', '/consent'):
                challenge = query['consent_challenge'][0]
                status, _, reply = self.request(self.admin + '/admin/oauth2/auth/requests/consent/accept?' +
                    urllib.parse.urlencode({'consent_challenge': challenge}), 'PUT', {
                        'grant_scope': ['openid', 'profile', 'email', 'offline_access'], 'remember': False,
                        'session': {'id_token': {'email': 'alice@example.test'}, 'access_token': {'email': 'alice@example.test'}}})
            else: raise AssertionError('unexpected authorization redirect destination')
            assert status == 200 and isinstance(reply.get('redirect_to'), str)
            url = reply['redirect_to']
        raise RuntimeError('Hydra authorization exceeded redirect limit')

    def token(self, client_id, secret, style, form):
        headers = {'Content-Type': 'application/x-www-form-urlencoded'}
        if style == 'basic':
            headers['Authorization'] = 'Basic ' + base64.b64encode((client_id + ':' + secret).encode()).decode()
        else:
            form = dict(form, client_id=client_id)
            if style == 'body': form['client_secret'] = secret
        status, _, body = self.request(self.discovery['token_endpoint'], 'POST',
            urllib.parse.urlencode(form).encode(), headers)
        assert status == 200 and body.get('access_token') and body.get('refresh_token') and body.get('token_type', '').lower() == 'bearer', ('oracle token response', status)
        return body

    def verify(self, token, client_id, nonce=None, original=None):
        compact = token['id_token']; segments = compact.split('.')
        assert len(segments) == 3
        header = json.loads(decode(segments[0])); claims = json.loads(decode(segments[1]))
        assert header['alg'] == 'RS256'
        status, _, jwks = self.request(self.discovery['jwks_uri']); assert status == 200
        matching = [key for key in jwks['keys'] if key.get('kid') == header.get('kid') and key['kty'] == 'RSA']
        assert len(matching) == 1
        key = matching[0]
        public = rsa.RSAPublicNumbers(int.from_bytes(decode(key['e']), 'big'), int.from_bytes(decode(key['n']), 'big')).public_key()
        public.verify(decode(segments[2]), (segments[0] + '.' + segments[1]).encode('ascii'), padding.PKCS1v15(), hashes.SHA256())
        assert claims['iss'] == self.issuer and claims['sub'] == 'user-42'
        audience = [claims['aud']] if isinstance(claims['aud'], str) else claims['aud']
        assert audience == [client_id]
        assert type(claims['exp']) is int and type(claims['iat']) is int
        assert claims['exp'] > int(time.time()) - 30 and abs(claims['iat'] - int(time.time())) <= 30
        if original is None: assert claims['nonce'] == nonce
        else:
            assert claims['iss'] == original['iss'] and claims['sub'] == original['sub'] and claims['aud'] == original['aud']
            if 'nonce' in claims: assert claims['nonce'] == original['nonce']
            if 'auth_time' in claims and 'auth_time' in original: assert claims['auth_time'] == original['auth_time']
        if 'at_hash' in claims:
            assert claims['at_hash'] == encode(hashlib.sha256(token['access_token'].encode('ascii')).digest()[:16])
        return claims

    def oracle(self, client_id, secret, style):
        verifier = secrets.token_urlsafe(32); nonce, state = secrets.token_urlsafe(32), secrets.token_urlsafe(32)
        url = self.discovery['authorization_endpoint'] + '?' + urllib.parse.urlencode({
            'response_type': 'code', 'client_id': client_id, 'redirect_uri': self.redirect,
            'scope': 'openid profile email offline_access', 'state': state, 'nonce': nonce,
            'code_challenge': encode(hashlib.sha256(verifier.encode()).digest()), 'code_challenge_method': 'S256'})
        code, returned_state = self.authorize(url, client_id); assert returned_state == state
        token = self.token(client_id, secret, style, {'grant_type': 'authorization_code',
            'redirect_uri': self.redirect, 'code': code, 'code_verifier': verifier})
        claims = self.verify(token, client_id, nonce)
        status, _, user = self.request(self.discovery['userinfo_endpoint'], headers={'Authorization': 'Bearer ' + token['access_token']})
        assert status == 200 and user['sub'] == claims['sub']
        updated = self.token(client_id, secret, style, {'grant_type': 'refresh_token', 'refresh_token': token['refresh_token']})
        if 'id_token' in updated: self.verify(updated, client_id, original=claims)
        status, _, user = self.request(self.discovery['userinfo_endpoint'], headers={'Authorization': 'Bearer ' + updated['access_token']})
        assert status == 200 and user['sub'] == claims['sub']
        return {'style': style, 'passed': True, 'initial_at_hash': 'at_hash' in claims,
                'refresh_id_token': 'id_token' in updated}


def build(args, output):
    records = {}
    for name, source in CLIENTS.items():
        binary = output / name
        command = [args.compiler, '-std=c11', '-D_GNU_SOURCE', '-O1' if args.sanitize else '-O2',
                   '-Wall', '-Wextra', '-Werror',
                   *(['-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if args.sanitize else []),
                   '-I', str(ROOT / 'single'), '-I', str(ROOT / 'extlibs/xjwt/include'), '-I', str(ROOT / 'extlibs/xoauth2/include'), str(ROOT / source),
                   str(ROOT / 'extlibs/xoauth2/tests/support/implementation.c'), str(ROOT / 'extlibs/xjwt/tests/support/implementation.c'),
                   '-o', str(binary), '-pthread', '-lm']
        result = subprocess.run(command, capture_output=True, text=True, timeout=240)
        (output / (name + '-build.log')).write_text(result.stdout + result.stderr)
        if result.returncode: raise RuntimeError('Hydra C build failed: ' + name)
        records[name] = {'binary': str(binary), 'binary_sha256': sha256(binary), 'command': command}
    return records


def run_client(provider, args, output, platform, builds, name, style, mode, secret, ca, before):
    client_id = 'xrt-' + style
    arguments = [provider.issuer, client_id, provider.redirect, str(ca)]
    arguments += [mode, style] if name == 'probe' else [style, 'RS256']
    if platform == 'windows':
        command = [args.windows_python, '-X', 'utf8', args.windows_root.rstrip('/\\') + '/tools/hydra_windows_bridge.py',
                   '--compiler', args.windows_compiler, '--output-dir', args.windows_output]
        arguments[3] = args.windows_output.rstrip('/\\') + '/ca.pem'
    else: command = [builds[name]['binary'], *arguments]
    env = dict(os.environ, XOAUTH2_CLIENT_SECRET=secret if mode != 'secret' else 'deliberately-invalid-fixture',
               ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    process = subprocess.Popen(command, cwd=ROOT, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, text=True, encoding='utf-8', env=env)
    try:
        if platform == 'windows':
            process.stdin.write(json.dumps({'operation': 'run', 'client': name, 'args': arguments,
                'secret': env['XOAUTH2_CLIENT_SECRET'], 'binary_sha256': builds[name]['binary_sha256']}) + '\n')
            process.stdin.flush()
            authorize = json.loads(timed_line(process.stdout))['authorize']
        else:
            line = timed_line(process.stdout)
            assert line.startswith('AUTHORIZE '), 'C authorization startup failed'
            authorize = line[len('AUTHORIZE '):].strip()
        code, state = provider.authorize(authorize, client_id)
        payload = json.dumps({'code': code, 'state': state}) + '\n' if platform == 'windows' else code + '\n' + state + '\n'
        stdout, stderr = process.communicate(payload, timeout=90)
        if platform == 'windows':
            reply = json.loads(stdout)
            assert reply['exit_code'] == process.returncode and reply['binary_sha256'] == builds[name]['binary_sha256']
            assert reply['source_inputs'] == before
            stdout, stderr = reply['stdout'], reply['stderr'] + stderr
        else: assert sha256(Path(builds[name]['binary'])) == builds[name]['binary_sha256']
        (output / f'{platform}-{name}-{style}-{mode}.log').write_text(stdout + stderr, encoding='utf-8')
        assert process.returncode == 0 and stdout.startswith('PASS '), (platform, name, style, mode, process.returncode)
        if name == 'live': assert 'PASS refreshed token and initial identity binding' in stdout
        print('[Hydra C]', platform, name, style, mode, 'passed', flush=True)
        return {'platform': platform, 'client': name, 'style': style, 'mode': mode, 'exit_code': process.returncode,
                'binary_sha256': builds[name]['binary_sha256']}
    finally:
        if process.poll() is None:
            process.stdin.close(); process.stdin = None
            try: process.communicate(timeout=5)
            except subprocess.TimeoutExpired: process.kill(); process.communicate(timeout=10)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--hydra', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--compiler', default='gcc')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--windows-python')
    parser.add_argument('--windows-root', help='Windows path to checkout with identical source inputs')
    parser.add_argument('--windows-compiler', default='gcc')
    parser.add_argument('--windows-output', help='fresh Windows output directory, also mounted at --windows-mount')
    parser.add_argument('--windows-mount', type=Path, help='WSL view of --windows-output')
    args = parser.parse_args()
    if sys.platform != 'linux': parser.error('Hydra test host must be Linux; Windows C uses the optional bridge')
    bridge = [args.windows_python, args.windows_root, args.windows_output, args.windows_mount]
    if any(bridge) and not all(bridge): parser.error('all four Windows bridge paths are required together')
    hydra = args.hydra.resolve()
    if sha256(hydra) != HYDRA_SHA256: raise ValueError('Hydra binary differs from the pinned v2.3.0 SQLite release')
    version = subprocess.run([str(hydra), 'version'], capture_output=True, text=True, check=True)
    version_text = (version.stdout + version.stderr).strip()
    assert 'v2.3.0' in version_text
    output = args.output_dir.resolve(); output.mkdir(parents=True, exist_ok=False)
    before = snapshot(ROOT); builds = build(args, output)
    compiler_version = subprocess.run([args.compiler, '--version'], capture_output=True, text=True, check=True).stdout.splitlines()[0]
    windows = None
    if args.windows_python:
        command = [args.windows_python, '-X', 'utf8', args.windows_root.rstrip('/\\') + '/tools/hydra_windows_bridge.py',
                   '--compiler', args.windows_compiler, '--output-dir', args.windows_output]
        result = subprocess.run(command, input=json.dumps({'operation': 'build'}) + '\n', text=True, capture_output=True, timeout=500)
        (output / 'windows-build-stderr.log').write_text(result.stderr)
        if result.returncode: raise RuntimeError('Windows Hydra bridge build failed')
        windows = json.loads(result.stdout); assert windows['source_inputs'] == before, 'Windows and Linux test inputs differ'
        (output / 'windows-build.json').write_text(json.dumps(windows, indent=2) + '\n')
    service = output / 'service'; service.mkdir()
    process = None; records = []; oracle = []
    try:
        ca, leaf, _, key = certificates(service)
        if windows:
            (args.windows_mount / 'ca.pem').write_bytes(ca.read_bytes())
        ports = []
        for _ in range(2):
            with socket.socket() as listener:
                listener.bind(('127.0.0.1', 0)); ports.append(listener.getsockname()[1])
        assert ports[0] != ports[1]
        provider = Provider('https://localhost:' + str(ports[0]) + '/',
                            'https://localhost:' + str(ports[1]), 'http://127.0.0.1:18999/callback', ca)
        env = dict(os.environ, DSN='memory', SERVE_PUBLIC_HOST='127.0.0.1', SERVE_ADMIN_HOST='127.0.0.1',
                   SERVE_PUBLIC_PORT=str(ports[0]), SERVE_ADMIN_PORT=str(ports[1]), SERVE_TLS_CERT_PATH=str(leaf),
                   SERVE_TLS_KEY_PATH=str(key), SERVE_PUBLIC_TLS_ENABLED='true', SERVE_ADMIN_TLS_ENABLED='true',
                   URLS_SELF_ISSUER=provider.issuer, URLS_LOGIN='https://login.example.test/login',
                   URLS_CONSENT='https://login.example.test/consent', SECRETS_SYSTEM=secrets.token_hex(32),
                   SECRETS_COOKIE=secrets.token_hex(32), LOG_LEVEL='fatal', LOG_FORMAT='json',
                   OAUTH2_PKCE_ENFORCED='true', HOME=str(service))
        with (service / 'server.log').open('wb') as stream:
            process = subprocess.Popen([str(hydra), 'serve', 'all', '--dev', '--sqa-opt-out'], env=env,
                                       stdout=stream, stderr=subprocess.STDOUT, start_new_session=True)
        for _ in range(100):
            if process.poll() is not None: raise RuntimeError('Hydra exited during startup')
            try:
                if provider.request(provider.admin + '/health/ready')[0] == 200: break
            except (OSError, urllib.error.URLError): pass
            time.sleep(.1)
        else: raise RuntimeError('Hydra not ready')
        status, _, provider.discovery = provider.request(provider.issuer + '.well-known/openid-configuration')
        assert status == 200 and provider.discovery['issuer'] == provider.issuer
        for style, method in STYLES.items():
            client_id = 'xrt-' + style; secret = secrets.token_urlsafe(32) if style != 'public' else ''
            registration = {'client_id': client_id, 'grant_types': ['authorization_code', 'refresh_token'],
                'response_types': ['code'], 'redirect_uris': [provider.redirect],
                'scope': 'openid profile email offline_access', 'token_endpoint_auth_method': method,
                'id_token_signed_response_alg': 'RS256'}
            if secret: registration['client_secret'] = secret
            status, _, _ = provider.request(provider.admin + '/admin/clients', 'POST', registration); assert status == 201
            oracle.append(provider.oracle(client_id, secret, style))
            print('[Hydra independent oracle]', style, 'passed', flush=True)
            for platform, current_builds in [('linux', builds), *([('windows', windows['builds'])] if windows else [])]:
                for mode in ['success', 'pkce', *(['secret'] if style != 'public' else [])]:
                    records.append(run_client(provider, args, output, platform, current_builds, 'probe', style, mode, secret, ca, before))
                records.append(run_client(provider, args, output, platform, current_builds, 'live', style, 'success', secret, ca, before))
        assert snapshot(ROOT) == before, 'Hydra test source inputs changed during execution'
    finally:
        if process is not None and process.poll() is None:
            process.terminate()
            try: process.wait(timeout=10)
            except subprocess.TimeoutExpired: process.kill(); process.wait(timeout=10)
        for path in service.glob('*.key'): path.unlink()
    assert process is not None and process.poll() is not None
    assert sha256(hydra) == HYDRA_SHA256
    report = {'actual_hydra_passed': True, 'hydra_version': version_text, 'hydra_sha256': HYDRA_SHA256,
              'source_inputs': before, 'builds': builds, 'compiler_version': compiler_version,
              'sanitize': args.sanitize, 'python_version': sys.version, 'cryptography_version': cryptography_version,
              'oracle': oracle, 'cases': records, 'service_stopped': True, 'server_exit_code': process.returncode,
              'private_keys_removed': not list(service.glob('*.key')), 'client_credentials_recorded': False}
    (output / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    return 0


if __name__ == '__main__': raise SystemExit(main())
