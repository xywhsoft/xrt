"""Internal JSON pipe bridge for Windows clients of the WSL mail server test."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
from contextlib import redirect_stdout

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from test_mail_tls_interop import build_examples, example_directory, EXAMPLES
from mail_server_test_inputs import snapshot, sha256

def main():
    if os.name != 'nt':
        raise ValueError('Windows bridge requires Windows Python')
    request = json.load(sys.stdin)
    compiler = request.get('compiler', 'gcc')
    if Path(compiler).is_absolute():
        os.environ['PATH'] = str(Path(compiler).parent) + os.pathsep + os.environ['PATH']
    if request['operation'] == 'build':
        before = snapshot(ROOT)
        with redirect_stdout(sys.stderr):
            build_examples(compiler)
        if snapshot(ROOT) != before:
            raise AssertionError('Windows source/build inputs changed during the build')
        response = {'built': True, 'source_inputs': before}
    elif request['operation'] == 'inputs':
        response = snapshot(ROOT)
    elif request['operation'] == 'run':
        kind = request['kind']
        binary = example_directory(kind, compiler) / (EXAMPLES[kind][2] + '.exe')
        digest = sha256(binary)
        env = dict(os.environ)
        env['X' + kind.upper() + '_USER'] = 'demo@example.test'
        env['X' + kind.upper() + '_PASSWORD'] = request['secret']
        result = subprocess.run([str(binary), *request['args']], cwd=ROOT, env=env,
                                text=True, encoding='utf-8', capture_output=True, timeout=45,
                                creationflags=subprocess.CREATE_NO_WINDOW)
        if sha256(binary) != digest:
            raise AssertionError('Windows client binary changed during execution')
        response = {'exit_code': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr,
                    'binary': str(binary), 'binary_sha256': digest}
    else:
        raise ValueError('unknown Windows bridge operation')
    print(json.dumps(response))

if __name__ == '__main__':
    main()
