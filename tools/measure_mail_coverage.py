"""Measure exact extension-owned mail coverage with source and test provenance.

--report-only verifies an existing report against its original inputs and executed
object profiles. Unbound counters cannot be relabeled with current source hashes.
The default union includes module tests and independent TLS example probes.
Static header implementations are outside this scope; --module-only is diagnostic.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import subprocess
import sys
from pathlib import Path, PurePosixPath

from gcov_coverage import CoverageUnion, compiler_gcov, gcov_json, source_hashes
import test_mail_tls_interop as interop

ROOT = Path(__file__).resolve().parents[1]
SUITES = {'xmail': 'xmail_tests', 'xpop3': 'xpop3_tests',
          'xsmtp': 'xsmtp_tests', 'ximap': 'ximap_tests'}
MINIMUMS = {'xmail': (73.0, 59.0), 'xpop3': (72.0, 55.0),
            'xsmtp': (75.0, 56.0), 'ximap': (74.0, 55.0)}
FLAGS = ['-std=c11', '-D_GNU_SOURCE=', '-O0', '--coverage', '-fprofile-update=atomic']
SCOPE = 'module_and_independent_tls_object_profiles'
MODULE_SCOPE = 'module_suite_object_profiles'


def coverage_percent(text: str) -> float:
    value = float(text)
    if not math.isfinite(value) or not 0 <= value <= 100:
        raise argparse.ArgumentTypeError('coverage minimum must be finite and between 0 and 100')
    return value


def _sources(product: str) -> list[Path]:
    manifest = ROOT / f'extlibs/{product}/config/modules.json'
    data = json.loads(manifest.read_text(encoding='utf-8'))
    prefix = f'extlibs/{product}/src/'
    selected = set()
    for module in data['modules']:
        for source in module.get('sources', []):
            if source.startswith(prefix) and source.endswith('.c'):
                relative = PurePosixPath(source)
                if '..' in relative.parts or relative.as_posix() != source:
                    raise RuntimeError(f'invalid owned source path: {source}')
                resolved = (ROOT / source).resolve()
                if not resolved.is_relative_to((ROOT / prefix).resolve()):
                    raise RuntimeError(f'owned source escapes product directory: {source}')
                selected.add(resolved)
    actual = {p.resolve() for p in (ROOT / prefix).rglob('*.c')}
    if not selected or selected != actual:
        raise RuntimeError(f'{product} manifest must list every owned C source')
    return sorted(selected)


def _object_dir(product: str, suite: str, compiler: str) -> Path:
    if Path(compiler).stem.lower() != 'gcc':
        raise ValueError('coverage tool currently supports --compiler gcc or a path to gcc')
    if sys.platform not in {'win32', 'linux'}:
        raise ValueError('mail coverage currently supports Windows and Linux')
    platform = 'windows' if os.name == 'nt' else 'linux'
    return ROOT / 'out/gcc' / ('native-' + platform) / suite / 'obj'


def measurement_dependencies() -> list[Path]:
    # All four mail manifests are loaded for dependent transport modules. Capture
    # their inputs conservatively; only the selected product contributes counters.
    inputs = [ROOT / 'config/modules.json', ROOT / 'single/xrt.h', ROOT / 'tests/test.h']
    for directory in ('include', 'src', 'tests/fixtures'):
        inputs += [p for p in (ROOT / directory).rglob('*')
                   if p.is_file() and p.suffix in {'.c', '.h', '.S', '.inc'}]
    for product in SUITES:
        inputs.append(ROOT / f'extlibs/{product}/config/modules.json')
        for directory in ('include', 'src', 'tests', 'examples'):
            inputs += [p for p in (ROOT / f'extlibs/{product}' / directory).rglob('*')
                       if p.is_file() and p.suffix in {'.c', '.h', '.S', '.inc'}]
    inputs += [ROOT / 'tools' / name for name in (
        'measure_mail_coverage.py', 'gcov_coverage.py', 'build.py', 'amalgamate.py',
        'xrt_manifest.py', 'xrt_text.py', 'generate_features.py', 'generate_extension_features.py',
        'test_mail_tls_interop.py')]
    return sorted(set(inputs))


def _hashes(paths: list[Path]) -> dict[str, str]:
    return {Path(path).relative_to(ROOT).as_posix(): digest
            for path, digest in source_hashes(paths).items()}


def _note(source: Path, object_dir: Path) -> Path:
    relative = source.relative_to(ROOT).with_suffix('')
    return object_dir / ('__'.join(relative.parts) + '.gcno')


def _profiles(sources: list[Path], object_dir: Path) -> list[Path]:
    return [p for source in sources for p in (_note(source, object_dir),
                                             _note(source, object_dir).with_suffix('.gcda'))]


def _interop_directory(product: str, protocol: str, compiler: str) -> Path:
    return interop.example_directory(protocol, compiler, product)


def _execution_path(product: str) -> Path:
    return ROOT / 'out/mail/tls-coverage' / sys.platform / product / 'execution.json'


def _interop_inputs(product: str, sources: list[Path], compiler: str) -> list[Path]:
    inputs = [_execution_path(product)]
    for protocol in interop.coverage_protocols(product):
        directory = _interop_directory(product, protocol, compiler)
        inputs += _profiles(sources, directory / 'obj')
        inputs.append(directory / (interop.EXAMPLES[protocol][2] + ('.exe' if os.name == 'nt' else '')))
    return inputs


def _build_interop(product: str, compiler: str) -> None:
    print(f'[coverage interop] {product} independent TLS examples', flush=True)
    subprocess.run([sys.executable, str(ROOT / 'tools/test_mail_tls_interop.py'),
                    '--compiler', compiler, '--coverage-product', product], cwd=ROOT, check=True)


def _execution(product: str, compiler: str) -> dict:
    data = _read_report(_execution_path(product))
    protocols = interop.coverage_protocols(product)
    if (type(data.get('schema')) is not int or data.get('schema') != 2 or data.get('product') != product or
            data.get('platform') != sys.platform or data.get('compile_flags') != FLAGS or
            type(data.get('ipv6_available')) is not bool):
        raise RuntimeError('independent TLS execution scope does not match measurement')
    expected = []
    for protocol in protocols:
        for mode in ('tls', 'starttls'):
            for hostname, reject, truncated, close_notify in [
                    ('localhost', False, False, True), ('localhost', False, False, False),
                    ('localhost', False, True, True), ('127.0.0.1', False, False, True),
                    *([('::1', False, False, True)] if data['ipv6_available'] else []),
                    ('127.0.0.1', True, False, True)]:
                expected.append({'protocol': protocol, 'mode': mode, 'hostname': hostname,
                                 'reject_identity': reject, 'truncated_response': truncated,
                                 'close_notify': close_notify})
    if not _strict_equal(data.get('cases'), expected):
        raise RuntimeError('independent TLS execution must include every required case exactly once')
    binaries = [_interop_directory(product, protocol, compiler) /
                (interop.EXAMPLES[protocol][2] + ('.exe' if os.name == 'nt' else '')) for protocol in protocols]
    if data.get('binary_sha256') != _hashes(binaries):
        raise RuntimeError('independent TLS executable changed since execution')
    return data


def _build(product: str, suite: str, compiler: str, object_dir: Path) -> None:
    for directory in (object_dir, object_dir.parent):
        if directory.is_dir():
            for counter in directory.glob('*.gcda'):
                counter.unlink()
    platform = 'windows' if os.name == 'nt' else 'linux'
    command = [sys.executable, str(ROOT / 'tools/build.py'), '--compiler', compiler,
               '--target-platform', platform, '--manifest', f'extlibs/{product}/config/modules.json',
               '--suite', suite, '--no-single', '--no-examples', '--rebuild', '--jobs', '4',
               *['--cflag=' + flag for flag in FLAGS], '--ldflag=--coverage']
    print(f'[coverage build] {product} ({suite})', flush=True)
    subprocess.run(command, cwd=ROOT, check=True)


def _collect(sources: list[Path], object_dir: Path, gcov: str,
             independent_dirs: dict[str, Path] | None = None) -> dict:
    union = CoverageUnion(ROOT, sources)
    for source in sources:
        union.add('module_suite:' + source.relative_to(ROOT).as_posix(),
                  gcov_json(_note(source, object_dir), gcov), sources=[source])
    for protocol, directory in (independent_dirs or {}).items():
        for source in sources:
            union.add('independent_tls:' + protocol + ':' + source.relative_to(ROOT).as_posix(),
                      gcov_json(_note(source, directory / 'obj'), gcov), sources=[source])
    return union.report()


def _metadata(report: dict, product: str, inputs: dict, profiles: dict, execution: dict | None) -> dict:
    return {**report, 'schema': 2, 'product': product, 'platform': sys.platform,
            'scope': SCOPE if execution is not None else MODULE_SCOPE, 'measured_source_kind': 'owned_c_files_only',
            'dependency_sha256': inputs, 'object_profile_sha256': profiles,
            'independent_tls_execution': execution,
            'compile_flags': FLAGS,
            'regression_minimum_percent': dict(zip(('lines', 'branches'), MINIMUMS[product]))}


def _read_report(path: Path) -> dict:
    if not path.is_file():
        raise RuntimeError('report-only requires an existing provenance report from a fresh measurement')
    def invalid_constant(value):
        raise RuntimeError('invalid nonfinite coverage report number: ' + value)
    def unique_object(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise RuntimeError('duplicate coverage report field: ' + key)
            result[key] = value
        return result
    try:
        result = json.loads(path.read_text(encoding='utf-8'), parse_constant=invalid_constant,
                            object_pairs_hook=unique_object)
    except (ValueError, UnicodeError) as error:
        raise RuntimeError('invalid coverage provenance report') from error
    if type(result) is not dict:
        raise RuntimeError('invalid coverage provenance report structure')
    return result


def _strict_equal(actual, expected) -> bool:
    if type(actual) is not type(expected):
        return False
    if isinstance(expected, dict):
        return actual.keys() == expected.keys() and all(_strict_equal(actual[k], v) for k, v in expected.items())
    if isinstance(expected, list):
        return len(actual) == len(expected) and all(_strict_equal(a, b) for a, b in zip(actual, expected))
    return actual == expected


def measure(product: str, compiler: str, gcov: str | None = None,
            report_only: bool = False, *, include_interop: bool = True) -> tuple[float, float]:
    output = ROOT / 'out/mail/coverage' / sys.platform / product
    path = output / 'coverage.json'
    if not report_only:
        output.mkdir(parents=True, exist_ok=True)
        path.unlink(missing_ok=True)
    sources = _sources(product)
    object_dir = _object_dir(product, SUITES[product], compiler)
    source_inputs = _hashes(sources)
    inputs = _hashes(measurement_dependencies())
    independent_dirs = {protocol: _interop_directory(product, protocol, compiler)
                        for protocol in interop.coverage_protocols(product)} if include_interop else {}
    profile_paths = _profiles(sources, object_dir)
    if include_interop:
        profile_paths += _interop_inputs(product, sources, compiler)
    scope = SCOPE if include_interop else MODULE_SCOPE
    if report_only:
        saved = _read_report(path)
        if (saved.get('schema') != 2 or saved.get('scope') != scope or
                saved.get('product') != product or saved.get('platform') != sys.platform):
            raise RuntimeError('coverage report provenance scope does not match this measurement')
        if saved.get('source_sha256') != source_inputs or saved.get('dependency_sha256') != inputs:
            raise RuntimeError('stale coverage report: source or test input no longer matches')
        try:
            profile_inputs = _hashes(profile_paths)
        except OSError as error:
            raise RuntimeError('coverage report requires the original executed object profiles') from error
        if saved.get('object_profile_sha256') != profile_inputs:
            raise RuntimeError('stale coverage report: executed object profiles changed')
    else:
        _build(product, SUITES[product], compiler, object_dir)
        if include_interop:
            _build_interop(product, compiler)
        profile_inputs = _hashes(profile_paths)
    execution = _execution(product, compiler) if include_interop else None
    report = _metadata(_collect(sources, object_dir, gcov or compiler_gcov(compiler), independent_dirs),
                       product, inputs, profile_inputs, execution)
    if (_hashes(sources) != source_inputs or _hashes(measurement_dependencies()) != inputs or
            _hashes(profile_paths) != profile_inputs):
        raise RuntimeError('mail test, runtime or profile input changed during coverage measurement')
    if report_only:
        if not _strict_equal(saved, report):
            raise RuntimeError('coverage report counts or metadata do not match executed object profiles')
    else:
        path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    for source, summary in report['files'].items():
        print(f"  {source}: {summary['covered_lines']}/{summary['lines']} lines "
              f"({summary['lines_percent']:.2f}%); "
              f"{summary['covered_branches']}/{summary['branches']} branch outcomes "
              f"({summary['branches_percent']:.2f}%)", flush=True)
    total = report['total']
    print(f"[coverage] {product}: {total['covered_lines']}/{total['lines']} lines "
          f"({total['lines_percent']:.2f}%); {total['covered_branches']}/{total['branches']} "
          f"branch outcomes ({total['branches_percent']:.2f}%)", flush=True)
    print('  exact counters, remaining gaps and original input hashes:', path, flush=True)
    return total['lines_percent'] / 100, total['branches_percent'] / 100


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--product', choices=[*SUITES, 'all'], default='all')
    parser.add_argument('--compiler', default='gcc')
    parser.add_argument('--gcov', help='default: gcov companion identified by the compiler')
    parser.add_argument('--report-only', action='store_true', help='verify the original report and profiles without rebuilding')
    parser.add_argument('--module-only', action='store_true', help='diagnostic baseline; excludes independent TLS probes')
    parser.add_argument('--min-lines', type=coverage_percent, help='default: product CI line floor')
    parser.add_argument('--min-branches', type=coverage_percent, help='default: product CI branch-outcome floor')
    args = parser.parse_args()
    products = list(SUITES) if args.product == 'all' else [args.product]
    failed = False
    for product in products:
        lines, branches = measure(product, args.compiler, args.gcov, args.report_only, include_interop=not args.module_only)
        line_floor, branch_floor = MINIMUMS[product]
        line_floor = line_floor if args.min_lines is None else args.min_lines
        branch_floor = branch_floor if args.min_branches is None else args.min_branches
        if lines * 100 < line_floor or branches * 100 < branch_floor:
            print(f'[coverage fail] {product} below {line_floor:g}% lines / {branch_floor:g}% branch outcomes')
            failed = True
    return int(failed)


if __name__ == '__main__':
    raise SystemExit(main())
