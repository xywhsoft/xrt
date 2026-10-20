"""TCP native ownership: offline graphs and ordinary self-created loopback.

Only explicit fixtures below are executed. No DNS, external hosts, scans,
malformed packets, fuzzing or network-security acceptance. This is a native
prerequisite milestone, NOT complete xlang managed-transport acceptance.
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
    # The selected existing Engine exit-window fixture also needs thread_key.
    sources, _, _, _, defines, links, *_ = _load_modules('net_tcp_ownership_tests,thread_key', platform=platform_name())
    minimal_sources, _, _, _, minimal_defines, minimal_links, *_ = _load_modules('net_buffer_ownership_tests', platform=platform_name())
    assert 'XRT_FEATURE_ATOMIC' not in minimal_defines and 'XRT_FEATURE_NET_ENGINE' not in minimal_defines
    fixtures = (
        'tests/network/test_net_buffer.c',
        'tests/network/test_net_buffer_pool.c',
        'tests/network/test_net_buffer_ref_oom.c',
        'tests/network/test_net_buffer_ownership.c',
        'tests/network/test_net_tcp_ownership.c',
        'tests/network/test_net_engine_ownership.c',
        'tests/network/test_net_tcp_bind.c',
        'tests/network/test_net_tcp_connect_timer.c',
        'tests/network/test_net_tcp_lifetime.c',
        'tests/network/test_net_tcp_pull.c',
    )
    singles = ('tests/single/test_single_net_tcp_ownership.c',
               'tests/single/test_single_net_engine_ownership.c',
               'tests/single/test_single_net_buffer_ownership.c')
    files = {p for area in ('include', 'src') for p in (root/area).rglob('*')
             if p.is_file() and p.suffix in ('.c', '.h')}
    files.update(root/name for name in (*fixtures, *singles, 'single/xrt.h', 'single/xrt_decl.h',
                                       'tests/test.h', 'config/modules.json', 'tools/build.py', 'tools/amalgamate.py'))
    files.update((Path(__file__).resolve(), *compilers))
    inputs = {str(p): sha(p) for p in sorted(files)}
    rows, products, failures = [], [], []
    out.mkdir(parents=True)
    report = dict(Complete=False, InputsUnchanged=False, LogsUnchanged=False,
                  Inputs=inputs, Rows=rows, Products=products, Failures=failures, Sources=sources,
                  MinimalBufferSources=minimal_sources, MinimalBufferDefines=minimal_defines,
                  OrdinaryLiteralLoopback=True, ExternalNetworkTests=False, NetworkSecurityTests=False,
                  ShippingLibraryAcceptance=False)

    def save():
        (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')

    def run(name, command, instrumented=False):
        command = list(map(str, command))
        invocation = command
        if command[0] in map(str, compilers) and Path(command[0]).stem != 'tcc':
            rsp = out/(name+'.rsp')
            rsp.write_text('\n'.join('"'+x.replace('\\', '/')+'"' for x in command[1:])+'\n', encoding='utf-8')
            invocation = [command[0], '@'+str(rsp)]
            products.append(dict(Path=str(rsp), Sha256=sha(rsp)))
        env = dict(os.environ)
        if instrumented:
            env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        log = out/(name+'.log')
        with log.open('wb') as stream:
            result = subprocess.run(invocation, cwd=root, env=env, stdout=stream, stderr=subprocess.STDOUT, timeout=180)
        rows.append(dict(Name=name, Command=command, Invocation=invocation, Exit=result.returncode,
                         Log=str(log), Sha256=sha(log)))
        save()
        print(name+': '+str(result.returncode), flush=True)
        assert result.returncode == 0, (name, str(log))

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
                libraries = ['-l'+name for name in links]
                if os.name != 'nt':
                    libraries += ['-pthread', '-lm']
                objects = []
                for index, source in enumerate(sources):
                    obj = out/f'{label}.{index}.o'
                    run(f'{label}.object-{index}', [*common, '-c', source, '-o', obj])
                    objects.append(obj)
                    products.append(dict(Path=str(obj), Sha256=sha(obj)))
                for fixture in (*fixtures, *singles):
                    package = Path(fixture).stem
                    try:
                        binary = out/(label+'.'+package+('.exe' if os.name == 'nt' else ''))
                        run(f'{label}.{package}.build', [*common, fixture,
                            *(objects if fixture not in singles else []), *libraries, '-o', binary])
                        products.append(dict(Path=str(binary), Sha256=sha(binary)))
                        run(f'{label}.{package}.run', [binary], mode == 'instrumented')
                        if 'tcp_ownership' in package:
                            run(f'{label}.{package}.native-run', [binary, 'native'], mode == 'instrumented')
                    except Exception as error:
                        failures.append(dict(Name=label+'.'+package, Error=repr(error)))
                        save()
                for fixture in ('tests/network/test_net_buffer_ownership.c', singles[2]):
                    try:
                        package = 'minimal.'+Path(fixture).stem
                        binary = out/(label+'.'+package+('.exe' if os.name == 'nt' else ''))
                        minimal_common = [cc, '-Wall', '-Wextra', '-Werror', *flags, '-Iinclude']
                        if cc.stem != 'tcc': minimal_common += ['-std=c11']
                        if os.name != 'nt': minimal_common += ['-D_GNU_SOURCE']
                        minimal_common += ['-D'+name for name in minimal_defines]
                        minimal_libraries = ['-l'+name for name in minimal_links]
                        if os.name != 'nt': minimal_libraries += ['-pthread', '-lm']
                        run(f'{label}.{package}.build', [*minimal_common, fixture,
                            *(minimal_sources if fixture not in singles else []), *minimal_libraries, '-o', binary])
                        products.append(dict(Path=str(binary), Sha256=sha(binary)))
                        run(f'{label}.{package}.run', [binary], mode == 'instrumented')
                    except Exception as error:
                        failures.append(dict(Name=label+'.'+package, Error=repr(error))); save()
            except Exception as error:
                failures.append(dict(Name=label, Error=repr(error)))
                save()
    report['InputsUnchanged'] = all(sha(p) == digest for p, digest in inputs.items())
    report['LogsUnchanged'] = all(sha(row['Log']) == row['Sha256'] for row in rows)
    report['Complete'] = not failures and report['InputsUnchanged'] and report['LogsUnchanged']
    save()
    assert report['Complete'], failures


if __name__ == '__main__':
    main()
