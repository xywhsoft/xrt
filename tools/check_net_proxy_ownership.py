"""Offline immutable Proxy physical ownership prerequisite, not Dial acceptance.

Only explicit ownership fixtures. No sockets, DNS, protocol parsing, malformed
traffic, fuzzing, security tests or shipping-library claims.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
from build import _load_modules, platform_name


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--compiler', action='append', required=True)
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve()
    assert not out.exists() and (not args.sanitize or os.name != 'nt')
    compilers = [Path(shutil.which(cc) or cc).resolve() for cc in args.compiler]
    sources, _, _, _, defines, links, *_ = _load_modules('net_proxy,memory_debug,atomic', platform=platform_name())
    modular = root/'tests/network/test_net_proxy_ownership.c'
    single = root/'tests/single/test_single_net_proxy_ownership.c'
    files = {p for folder in ('include', 'src') for p in (root/folder).rglob('*')
             if p.is_file() and p.suffix in ('.c', '.h')}
    files.update((Path(__file__).resolve(), modular, single, root/'tests/test.h', root/'tests/test_thread.h',
                  root/'single/xrt.h', root/'single/xrt_decl.h', root/'config/modules.json',
                  root/'tools/build.py', root/'docs/api/proxy.md', *compilers))
    inputs = {str(p): sha(p) for p in sorted(files)}
    rows, products, failures = [], [], []
    out.mkdir(parents=True)
    report = dict(Complete=False, InputsUnchanged=False, LogsUnchanged=False, ProductsUnchanged=False,
                  Inputs=inputs, Rows=rows, Products=products, Failures=failures, Sources=sources,
                  NetworkTraffic=False, ExternalNetworkTests=False, NetworkSecurityTests=False,
                  ShippingLibraryAcceptance=False, ProxyDialAcceptance=False, Scope=__doc__)

    def save():
        (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')

    def run(name, command, expected=None, instrumented=False):
        command = list(map(str, command)); invocation = command
        if command[0] in map(str, compilers) and Path(command[0]).stem != 'tcc':
            rsp = out/(name+'.rsp')
            rsp.write_text('\n'.join('"'+x.replace('\\', '/').replace('"', '\\"')+'"' for x in command[1:])+'\n', encoding='utf-8')
            invocation = [command[0], '@'+str(rsp)]; products.append(dict(Path=str(rsp), Sha256=sha(rsp)))
        env = dict(os.environ)
        if instrumented:
            env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        log = out/(name+'.log'); timeout = None
        with log.open('wb') as stream:
            try:
                result = subprocess.run(invocation, cwd=root, env=env, stdout=stream,
                                        stderr=subprocess.STDOUT, timeout=90).returncode
            except subprocess.TimeoutExpired as error:
                result = None; timeout = repr(error)
        rows.append(dict(Name=name, Command=command, Invocation=invocation, Exit=result,
                         Timeout=timeout, Log=str(log), Sha256=sha(log), ExpectedRefusal=expected))
        save(); print(name+': '+str(result), flush=True)
        assert result is not None and ((result == 0) if expected is None else
            (result == 1 and expected in log.read_text(encoding='utf-8', errors='replace'))), (name, str(log), timeout)

    def product(path):
        products.append(dict(Path=str(path), Sha256=sha(path)))

    save()
    for number, cc in enumerate(compilers):
        modes = [('O0', ['-O0']), ('O2', ['-O2'])]
        if args.sanitize and cc.stem != 'tcc':
            modes.append(('instrumented', ['-O1', '-g', '-fsanitize=address,undefined',
                                           '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']))
        for mode, flags in modes:
            label = f'{number}.{mode}'
            try:
                common = [cc, '-Wall', '-Wextra', '-Werror', *flags, '-Iinclude']
                if cc.stem != 'tcc':common += ['-std=c11']
                if os.name != 'nt':common += ['-D_GNU_SOURCE']
                common += ['-D'+name for name in defines]
                libraries = ['-l'+name for name in links]
                if os.name != 'nt':libraries += ['-pthread', '-lm']
                objects = []
                for index, source in enumerate(sources):
                    obj = out/f'{label}.{index}.o'
                    run(f'{label}.object-{index}', [*common, '-c', source, '-o', obj]);objects.append(obj);product(obj)
                for kind, fixture, selected in [('modular',modular,objects), ('single',single,[])]:
                    binary = out/(label+'.'+kind+('.exe' if os.name == 'nt' else ''))
                    run(label+'.'+kind+'.build', [*common,fixture,*selected,*libraries,'-o',binary]);product(binary)
                    run(label+'.'+kind+'.run', [binary], instrumented=mode == 'instrumented')
                # Independent mutations prove BOTH sides of the freeze contract,
                # using modular production objects except this one source unit.
                if number == 0 and mode == 'O0':
                    original = (root/'src/network/proxy.c').read_text(encoding='utf-8')
                    retain = 'xrtOwnershipRefRetain((volatile int32*)&pProxy->References)'
                    assert original.count(retain) == 1
                    a = original.index('XRT_API void xrtNetProxyRelease(')
                    b = original.index('static bool __xrtNetProxyOwnershipCount', a)
                    release = original[a:b]
                    changes = ('\txrtownershipscope Mutation = {0};\n',
                               '\tif (!xrtOwnershipMutationBegin(&Mutation)) abort();\n',
                               '\tif (!xrtOwnershipScopeEnd(&Mutation)) abort();\n')
                    assert all(release.count(text) == 1 for text in changes)
                    raw_release = release
                    for text in changes:raw_release = raw_release.replace(text, '', 1)
                    for name, text in [('retain-outside-freeze', original.replace(retain, retain.replace('xrtOwnershipRefRetain','xrtRefRetain'),1)),
                                       ('release-outside-freeze',original[:a]+raw_release+original[b:])]:
                        source = out/(name+'.c')
                        # Only relocate the original relative include, not logic.
                        text = text.replace('#include "../internal/xrt_proxy.h"',
                                            '#include "'+(root/'src/internal/xrt_proxy.h').as_posix()+'"',1)
                        source.write_text(text,encoding='utf-8');product(source)
                        obj = out/(name+'.o');run(name+'.object',[*common,'-c',source,'-o',obj]);product(obj)
                        binary = out/(name+('.exe' if os.name == 'nt' else ''))
                        selected = [obj if p == objects[sources.index('src/network/proxy.c')] else p for p in objects]
                        run(name+'.build', [*common,modular,*selected,*libraries,'-o',binary]);product(binary)
                        run(name+'.refusal',[binary], expected="native RC transition cannot escape another thread's freeze")
            except Exception as error:
                failures.append(dict(Name=label,Error=repr(error)));save()
    report['InputsUnchanged'] = all(sha(p) == digest for p, digest in inputs.items())
    report['LogsUnchanged'] = all(sha(row['Log']) == row['Sha256'] for row in rows)
    report['ProductsUnchanged'] = all(sha(p['Path']) == p['Sha256'] for p in products)
    report['Complete'] = not failures and all(report[key] for key in ('InputsUnchanged','LogsUnchanged','ProductsUnchanged'))
    save();assert report['Complete'],failures


if __name__ == '__main__':main()
