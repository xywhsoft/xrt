#!/usr/bin/env python3
"""Offline scalar domain ABI, pruned core/graph, legacy and sanitizer matrix."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time

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
    inputs = {path for area in ('include', 'src', 'tests/value') for path in (root / area).rglob('*')
              if path.is_file() and path.suffix in ('.c', '.h')}
    inputs.update((root / 'single/xrt.h', root / 'single/xrt_decl.h', root / 'tests/test.h',
                   root / 'config/modules.json', Path(__file__).resolve(), *compilers))
    inputs = {str(path): sha(path) for path in sorted(inputs)}
    out.mkdir(parents=True)
    rows, failures, plans = [], [], {}
    report = dict(Complete=False, InputsUnchanged=False, LogsUnchanged=False, Rows=rows,
                  Failures=failures, Inputs=inputs, Plans=plans, NetworkTests=False,
                  ScalarShell64Bytes=88, TypeId64Offset=16, Data64Offset=64,
                  OtherAbiSizeGuarantee=False)

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')

    def run(name, command, sanitized=False):
        log = out / (name + '.log')
        env = dict(os.environ)
        if sanitized:
            env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
                       UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        start = time.monotonic()
        with log.open('wb') as stream:
            result = subprocess.run(list(map(str, command)), cwd=root, env=env,
                                    stdout=stream, stderr=subprocess.STDOUT, timeout=240)
        rows.append(dict(Name=name, Command=list(map(str, command)), Exit=result.returncode,
                         Log=str(log), Sha256=sha(log), Seconds=time.monotonic()-start))
        save()
        print(f'{name}: {result.returncode}', flush=True)
        if result.returncode:
            raise RuntimeError(name + ': ' + log.read_text(errors='replace')[-4000:])

    save()
    try:
        for family in ('value', 'value_graph'):
            sources, _, _, _, defines, links, *_ = _load_modules(family+',memory_debug', platform=platform_name())
            plans[family] = dict(Sources=sources, Defines=defines, Links=links)
            assert not any('/net/' in path or '/tls/' in path for path in sources)
            fixtures = ['test_value_scalar_domain', 'test_value', 'test_value_handle', 'test_value_oom']
            if family == 'value_graph':
                fixtures += ['test_value_graph', 'test_value_graph_contract', 'test_value_graph_oom']
            for index, cc in enumerate(compilers):
                modes = [('O0', []), ('O2', [])]
                if args.sanitize and cc.stem != 'tcc':
                    modes.append(('instrumented', ['-O1', '-g', '-fsanitize=address,undefined',
                                                  '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']))
                for opt, extra in modes:
                    common = [str(cc), '-std=c11', '-Wall', '-Wextra', '-Werror']
                    if os.name != 'nt': common += ['-D_GNU_SOURCE']
                    if cc.stem == 'tcc': common.remove('-std=c11')
                    common += extra or ['-'+opt]
                    libraries = ['-l'+link for link in links]
                    if os.name != 'nt': libraries += ['-pthread', '-lm']
                    for packaging in ('modular', 'single'):
                        label = f'{family}.{index}.{opt}.{packaging}'
                        directory = out / label
                        directory.mkdir()
                        if packaging == 'modular':
                            flags = common + ['-Iinclude'] + ['-D'+d for d in defines]
                            objects = []
                            for number, source in enumerate(sources):
                                obj = directory / (str(number)+'.o')
                                run(label+f'.provider.{number}', [*flags, '-c', source, '-o', obj])
                                objects.append(obj)
                        else:
                            flags = common + ['-Isingle', '-DXRT_IMPLEMENTATION', '-DXRT_MODULE_'+family.upper(),
                                              '-DXRT_MODULE_MEMORY_DEBUG']
                            objects = []
                        for fixture in fixtures:
                            lane = label+'.'+fixture
                            binary = directory / (fixture+('.exe' if os.name == 'nt' else ''))
                            try:
                                run(lane+'.build', [*flags, 'tests/value/'+fixture+'.c', *objects,
                                                    *libraries, '-o', binary])
                                run(lane+'.run', [binary], opt == 'instrumented')
                            except Exception as error:
                                failures.append(dict(Name=lane, Error=repr(error)))
                                save()
        report['InputsUnchanged'] = all(sha(path) == digest for path, digest in inputs.items())
        report['LogsUnchanged'] = all(sha(row['Log']) == row['Sha256'] for row in rows)
        report['Complete'] = not failures and report['InputsUnchanged'] and report['LogsUnchanged']
        save()
        assert report['Complete'], failures
    except BaseException as error:
        failures.append(dict(Name='matrix', Error=repr(error)))
        save()
        raise


if __name__ == '__main__':
    main()
