"""Actual Dial/Future ownership graphs and ordinary self-created loopback only.

Explicit fixture allowlist: no DNS, external hosts, scans, malformed traffic,
fuzzing or security acceptance. This is a native prerequisite, not complete
xlang managed-transport or shipping-library acceptance.
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
    sources, _, _, _, defines, links, *_ = _load_modules(
        'net_tcp_dial_ownership_tests,net_port_select,thread_key,'+('net_port_iocp' if os.name=='nt' else 'net_port_epoll'), platform=platform_name())
    fixtures = ('tests/network/test_net_tcp_dial_ownership.c',
                'tests/network/test_net_tcp_dial_future.c',
                'tests/network/test_net_tcp_dial_future_oom.c',
                'tests/network/test_net_tcp_dial_future_publish.c',
                'tests/network/test_net_tcp_future_ownership.c',
                'tests/network/test_net_tcp_ownership.c')
    singles = ('tests/single/test_single_net_tcp_dial_ownership.c',
               'tests/single/test_single_net_tcp_dial_future.c',
               'tests/single/test_single_net_tcp_future_ownership.c',
               'tests/single/test_single_net_tcp_ownership.c')
    files = {p for area in ('include', 'src') for p in (root/area).rglob('*')
             if p.is_file() and p.suffix in ('.c', '.h')}
    # Include transitive fixture includes, not just top-level wrapper files.
    files.update(p for p in (root/'tests').rglob('*') if p.is_file() and p.suffix in ('.c', '.h'))
    files.update(root/name for name in ('single/xrt.h', 'single/xrt_decl.h',
                                       'config/modules.json', 'tools/build.py', 'tools/amalgamate.py', 'docs/api/tcp.md'))
    files.update((Path(__file__).resolve(), *compilers))
    inputs = {str(p): sha(p) for p in sorted(files)}
    rows, products, failures = [], [], []
    out.mkdir(parents=True)
    report = dict(Complete=False, InputsUnchanged=False, LogsUnchanged=False, ProductsUnchanged=False,
                  Inputs=inputs, Rows=rows, Products=products, Failures=failures, Sources=sources,
                  OrdinaryLiteralLoopback=True, ExternalNetworkTests=False, NetworkSecurityTests=False,
                  ShippingLibraryAcceptance=False,Scope=__doc__,
                  NewGraphCasesPerBackend=136,ConstructorBudgetsPerBackend=96,
                  OwnershipPolicyRejectionTests=True,PublicLanguageApiAccepted=False)

    def save():
        (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')

    def run(name, command, instrumented=False):
        command = list(map(str, command))
        invocation = command
        if command[0] in map(str, compilers) and Path(command[0]).stem != 'tcc':
            rsp = out/(name+'.rsp')
            rsp.write_text('\n'.join('"'+x.replace('\\', '/').replace('"', '\\"')+'"' for x in command[1:])+'\n', encoding='utf-8')
            invocation = [command[0], '@'+str(rsp)]
            products.append(dict(Path=str(rsp), Sha256=sha(rsp)))
        env = dict(os.environ)
        if instrumented:
            env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        log = out/(name+'.log')
        timeout = None
        with log.open('wb') as stream:
            try:
                exit_code = subprocess.run(invocation, cwd=root, env=env, stdout=stream,
                                           stderr=subprocess.STDOUT, timeout=180).returncode
            except subprocess.TimeoutExpired as error:
                timeout = repr(error)
                exit_code = None
        rows.append(dict(Name=name, Command=command, Invocation=invocation, Exit=exit_code,
                         Timeout=timeout, Log=str(log), Sha256=sha(log)))
        save()
        print(name+': '+str(exit_code), flush=True)
        assert exit_code == 0, (name, str(log), timeout)

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
                if cc.stem != 'tcc':
                    common += ['-std=c11']
                if os.name != 'nt':
                    common += ['-D_GNU_SOURCE']
                common += ['-D'+name for name in defines]
                # Explicit single selection: neither these tests nor their
                # declarations silently acquire the complete crypto/TLS stack.
                common += ['-DXRT_MODULE_'+name for name in ('NET_TCP_DIAL_FUTURE','NET_TCP_SYNC',
                    'MEMORY_DEBUG','THREAD_KEY','NET_PORT_SELECT', 'NET_PORT_IOCP' if os.name=='nt' else 'NET_PORT_EPOLL')]
                libraries = ['-l'+name for name in links]
                if os.name != 'nt':
                    libraries += ['-pthread', '-lm']
                objects = []
                for index, source in enumerate(sources):
                    obj = out/f'{label}.{index}.o'
                    run(f'{label}.object-{index}', [*common, '-c', source, '-o', obj])
                    objects.append(obj)
                    products.append(dict(Path=str(obj), Sha256=sha(obj)))
                variants = [(fixture, (), '') for fixture in (*fixtures, *singles)]
                for fixture, extra, suffix in variants:
                    package = Path(fixture).stem+suffix
                    try:
                        binary = out/(label+'.'+package+('.exe' if os.name == 'nt' else ''))
                        run(f'{label}.{package}.build', [*common, *extra, fixture,
                            *(objects if fixture not in singles else []), *libraries, '-o', binary])
                        products.append(dict(Path=str(binary), Sha256=sha(binary)))
                        run(f'{label}.{package}.run', [binary], mode == 'instrumented')
                        if fixture in (fixtures[0],*fixtures[-2:],singles[0],*singles[-2:]):
                            run(f'{label}.{package}.native-run', [binary, 'native'], mode == 'instrumented')
                    except Exception as error:
                        failures.append(dict(Name=label+'.'+package, Error=repr(error)))
                        save()
            except Exception as error:
                failures.append(dict(Name=label, Error=repr(error)))
                save()
    report['InputsUnchanged'] = all(sha(p) == digest for p, digest in inputs.items())
    report['LogsUnchanged'] = all(sha(row['Log']) == row['Sha256'] for row in rows)
    report['ProductsUnchanged'] = all(sha(product['Path']) == product['Sha256'] for product in products)
    report['Complete'] = not failures and all(report[key] for key in
        ('InputsUnchanged', 'LogsUnchanged', 'ProductsUnchanged'))
    save()
    assert report['Complete'], failures


if __name__ == '__main__':
    main()
