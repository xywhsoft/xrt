"""Offline normal TLS resume/immutable peer ownership acceptance; no sockets."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--tcc', type=Path, required=True)
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    assert not out.exists()
    out.mkdir(parents=True)
    sys.path.insert(0, str(root/'tools'))
    from xrt_manifest import load_manifest, dependency_closure
    modules = ['tls_client_resume', 'tls_server_resume', 'tls_identity_rsa',
               'tls_key_exchange_x25519', 'tls_record_aes', 'tls_schedule_sha256', 'memory_debug']
    closure = dependency_closure(modules, load_manifest(root/'config/modules.json')['modules'])
    sources = list(dict.fromkeys(s for m in closure for s in m['sources']))
    defines = ['-D'+m['feature'] for m in closure if m.get('feature')]
    gcc = Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    tcc = args.tcc.resolve()
    inputs = [root/'config/modules.json', Path(__file__).resolve(), gcc, tcc]
    inputs += [p for folder in ['src', 'include', 'tests/fixtures']
               for p in (root/folder).rglob('*') if p.is_file()]
    inputs += [root/p for p in ['single/xrt.h', 'single/xrt_decl.h', 'tests/test.h',
                               'tests/tls/test_tls_resume_peer.c', 'tools/xrt_manifest.py']]
    report = dict(Complete=False, Inputs={str(p): sha(p) for p in inputs}, Rows=[],
                  Sources=sources, Modules=modules, NetworkTraffic=False, SecurityTests=False)

    def save():
        (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf8')

    def run(name, command):
        command = list(map(str, command))
        executed = command
        if Path(command[0]) in [gcc, tcc]:
            rsp = out/(name+'.rsp')
            rsp.write_text('\n'.join('"'+s.replace('\\', '/').replace('"', '\\"')+'"'
                                     for s in command[1:])+'\n', encoding='utf8')
            executed = [command[0], '@'+str(rsp)]
        log = out/(name+'.log')
        start = time.monotonic()
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
                   UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        with log.open('wb') as stream:
            code = subprocess.run(executed, cwd=root, env=env, stdout=stream,
                                  stderr=subprocess.STDOUT, timeout=1200).returncode
        report['Rows'].append(dict(Name=name, Command=command, Exit=code,
                                  Log=str(log), Sha256=sha(log), Seconds=time.monotonic()-start))
        save()
        print(name, code, flush=True)
        assert code == 0, str(log)
        return log.read_text(errors='replace')

    implementation = out/'native.c'
    implementation.write_text('#define XRT_IMPLEMENTATION\n#include "single/xrt.h"\n', encoding='utf8')
    flags = ['-std=c11', '-I'+str(root), '-I'+str(root/'include'), *defines]
    libraries = ['-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
    if os.name != 'nt':
        flags += ['-D_GNU_SOURCE']
        libraries = ['-pthread', '-ldl', '-lm']
    modes = [('gcc-O0', gcc, ['-O0']), ('gcc-O2', gcc, ['-O2']), ('tcc', tcc, [])]
    if os.name != 'nt':
        modes += [('asan', gcc, ['-O1', '-g', '-fsanitize=address,undefined',
                                '-fno-omit-frame-pointer', '-fno-pie', '-no-pie'])]
    for style in ['modular', 'single']:
        for label, compiler, options in modes:
            name = style+'-'+label
            binary = out/(name+('.exe' if os.name == 'nt' else ''))
            strict = ['-Wall', '-Wextra', '-Werror'] if compiler == gcc else []
            run(name+'.build', [compiler, *flags, *options, *strict,
                root/'tests/tls/test_tls_resume_peer.c',
                *([root/s for s in sources] if style == 'modular' else [implementation]),
                *libraries, '-o', binary])
            assert '128 OOM prefixes; balanced owners' in run(name+'.run', [binary])
    report['InputsUnchanged'] = all(sha(p) == value for p, value in report['Inputs'].items())
    report['Complete'] = report['InputsUnchanged']
    report['Products'] = {str(p): sha(p) for p in out.iterdir() if p.is_file() and p.name != 'results.json'}
    save()
    assert report['Complete']


if __name__ == '__main__':
    main()
