"""Internal JSON pipe bridge: no credentials in command arguments or files."""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import queue
import subprocess
import sys
import threading

from hydra_test_inputs import snapshot, sha256

ROOT = Path(__file__).resolve().parents[1]
CLIENTS = {'probe': 'extlibs/xoauth2/tests/test_hydra_interop_client.c',
           'live': 'extlibs/xoauth2/examples/oidc_live.c'}


def timed_line(stream, timeout=30):
    pending = queue.Queue()
    threading.Thread(target=lambda: pending.put(stream.readline()), daemon=True).start()
    return pending.get(timeout=timeout)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default='gcc')
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    if os.name != 'nt': parser.error('this bridge requires Windows Python')
    if Path(args.compiler).is_absolute():
        os.environ['PATH'] = str(Path(args.compiler).parent) + os.pathsep + os.environ['PATH']
    before = snapshot(ROOT)
    request = json.loads(timed_line(sys.stdin))
    output = args.output_dir.resolve()
    if request['operation'] == 'build':
        output.mkdir(parents=True, exist_ok=False)
        builds = {}
        for name, source in CLIENTS.items():
            binary = output / (name + '.exe')
            command = [args.compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                       '-I', str(ROOT / 'single'), '-I', str(ROOT / 'extlibs/xjwt/include'), '-I', str(ROOT / 'extlibs/xoauth2/include'),
                       str(ROOT / source), str(ROOT / 'extlibs/xoauth2/tests/support/implementation.c'),
                       str(ROOT / 'extlibs/xjwt/tests/support/implementation.c'), '-o', str(binary),
                       '-lws2_32', '-lbcrypt', '-ladvapi32', '-liphlpapi']
            build = subprocess.run(command, capture_output=True, text=True, timeout=240)
            (output / (name + '-build.log')).write_text(build.stdout + build.stderr, encoding='utf-8')
            if build.returncode: raise RuntimeError('Windows Hydra client build failed: ' + name)
            builds[name] = {'binary': str(binary), 'binary_sha256': sha256(binary), 'command': command}
        version = subprocess.run([args.compiler, '--version'], capture_output=True, text=True, check=True).stdout.splitlines()[0]
        assert snapshot(ROOT) == before, 'Windows Hydra source inputs changed during build'
        print(json.dumps({'source_inputs': before, 'builds': builds, 'compiler_version': version}), flush=True)
    elif request['operation'] == 'run':
        if request['client'] not in CLIENTS: raise ValueError('unknown client')
        binary = output / (request['client'] + '.exe')
        digest = sha256(binary)
        if digest != request['binary_sha256']: raise AssertionError('Windows binary differs from recorded build')
        env = dict(os.environ, XOAUTH2_CLIENT_SECRET=request['secret'])
        client = subprocess.Popen([str(binary), *request['args']], cwd=ROOT, stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding='utf-8',
                                  env=env, creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            authorize = timed_line(client.stdout)
            if not authorize.startswith('AUTHORIZE '): raise RuntimeError('Windows client authorization startup failed')
            print(json.dumps({'authorize': authorize[len('AUTHORIZE '):].strip()}), flush=True)
            callback = json.loads(timed_line(sys.stdin))
            stdout, stderr = client.communicate(callback['code'] + '\n' + callback['state'] + '\n', timeout=60)
            assert snapshot(ROOT) == before, 'Windows Hydra source inputs changed during execution'
            assert sha256(binary) == digest, 'Windows binary changed during execution'
            print(json.dumps({'exit_code': client.returncode, 'stdout': stdout, 'stderr': stderr,
                              'binary_sha256': digest, 'source_inputs': before}), flush=True)
            return client.returncode
        finally:
            if client.poll() is None: client.kill(); client.wait()
    else:
        raise ValueError('unknown bridge operation')
    return 0


if __name__ == '__main__': raise SystemExit(main())
