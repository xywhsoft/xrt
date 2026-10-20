"""Offline ownership admission: numeric completion, active callback and real TLS tail.
Only selected existing numeric tests; never run the general network suites.
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
    sources, _, _, _, defines, links, *_ = _load_modules('net_resolver_future_ownership_tests', platform=platform_name())
    fixtures = ('tests/network/test_net_resolver_future_ownership.c', 'tests/single/test_single_net_resolver_future_ownership.c')
    paths = {path for area in ('include', 'src') for path in (root / area).rglob('*')
             if path.is_file() and path.suffix in ('.c', '.h')}
    paths.update((root / name for name in (*fixtures, 'single/xrt.h', 'single/xrt_decl.h', 'tests/test.h', 'config/modules.json')))
    paths.update((Path(__file__).resolve(), *compilers))
    inputs = {str(path): sha(path) for path in sorted(paths)}
    out.mkdir(parents=True)
    rows, products, failures = [], [], []
    report = dict(Complete=False, InputsUnchanged=False, LogsUnchanged=False, Inputs=inputs,
                  Rows=rows, Products=products, Failures=failures, Sources=sources,
                  Selectors=['active', 'exit', 'pending', 'unknown'], NumericOnly=True,
                  NetworkTraffic=False, ExternalNetworkTests=False, NetworkSecurityTests=False,
                  ShippingLibraryAcceptance=False)

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')

    def run(name, command, sanitized=False):
        command = list(map(str, command)); invocation = command
        if Path(command[0]) in compilers and Path(command[0]).stem != 'tcc':
            rsp = out / (name+'.rsp')
            rsp.write_text('\n'.join('"'+x.replace('\\', '/')+'"' for x in command[1:])+'\n', encoding='utf-8')
            invocation = [command[0], '@'+str(rsp)]
            products.append(dict(Path=str(rsp), Sha256=sha(rsp)))
        log = out / (name+'.log'); env = dict(os.environ)
        if sanitized:
            env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        with log.open('wb') as stream:
            result = subprocess.run(invocation, cwd=root, env=env, stdout=stream, stderr=subprocess.STDOUT, timeout=240)
        rows.append(dict(Name=name, Command=command, Invocation=invocation, Exit=result.returncode, Log=str(log), Sha256=sha(log)))
        save(); print(name+': '+str(result.returncode), flush=True)
        assert result.returncode == 0, (name, str(log))

    save()
    for index, cc in enumerate(compilers):
        modes = [('O0', ['-O0']), ('O2', ['-O2'])]
        if args.sanitize and cc.stem != 'tcc':
            modes.append(('instrumented', ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']))
        for opt, extra in modes:
            for packaging, fixture in zip(('modular', 'single'), fixtures):
                label = f'{index}.{opt}.{packaging}'; binary = out / (label+('.exe' if os.name == 'nt' else ''))
                try:
                    common = [str(cc), '-Wall', '-Wextra', '-Werror', *extra, '-Iinclude']
                    if cc.stem != 'tcc': common += ['-std=c11']
                    if os.name != 'nt': common += ['-D_GNU_SOURCE']
                    providers = sources if packaging == 'modular' else []
                    common += ['-D'+name for name in defines]
                    libraries = ['-l'+link for link in links]
                    if os.name != 'nt': libraries += ['-pthread', '-lm']
                    run(label+'.build', [*common, fixture, *providers, *libraries, '-o', binary])
                    products.append(dict(Path=str(binary), Sha256=sha(binary))); save()
                    for selector in report['Selectors']:
                        run(label+'.'+selector, [binary, selector], opt == 'instrumented')
                except Exception as error:
                    failures.append(dict(Name=label, Error=repr(error))); save()
    report['InputsUnchanged'] = all(sha(path)==digest for path, digest in inputs.items())
    report['LogsUnchanged'] = all(sha(row['Log'])==row['Sha256'] for row in rows)
    report['Complete'] = not failures and report['InputsUnchanged'] and report['LogsUnchanged']
    save(); assert report['Complete'], failures


if __name__ == '__main__':
    main()
