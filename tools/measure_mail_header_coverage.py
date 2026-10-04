"""Report private-header functions from an already verified mail coverage run.

This is a separate scope, not an increase to the owned-C-file regression metric.
It uses only original, input-bound product object profiles. Sibling-library header
instantiations outside those profiles are not credited. No build or test is rerun.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

from gcov_coverage import compiler_gcov, gcov_json
from gcov_header_coverage import HeaderFunctionUnion
import measure_mail_coverage as coverage

ROOT = Path(__file__).resolve().parents[1]
SCOPE = 'owned_private_header_functions_in_original_product_object_profiles'


def header_inventory(product: str) -> dict[Path, list[str]]:
    """Inventory ordinary static C definitions; exclude declarations and data.

The current mail headers use ordinary C function declarators. Attribute- or
macro-generated definitions need explicit support before they can be measured.
Strings and comments are masked while retaining positions and line numbers.
"""
    result = {}
    for path in sorted((ROOT / f'extlibs/{product}/src').rglob('*.h')):
        text = path.read_text(encoding='utf-8')
        masked = re.sub(r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                        lambda match: re.sub(r'[^\n]', ' ', match.group()), text)
        names = re.findall(r'\bstatic\s+[^;{}=()]+?\b([A-Za-z_]\w*)\s*\([^;{}]*\)\s*\{', masked)
        if len(names) != len(set(names)):
            raise RuntimeError('duplicate private header function definitions: ' + str(path))
        result[path.resolve()] = sorted(names)
    return result


def _postprocessor_inputs() -> dict[str, str]:
    return coverage._hashes([ROOT / 'tools' / name for name in
                             ('measure_mail_header_coverage.py', 'gcov_header_coverage.py')])


def measure(product: str, compiler: str, gcov: str | None = None,
            report_only: bool = False) -> dict:
    directory = ROOT / 'out/mail/coverage' / sys.platform / product
    primary_path, path = directory / 'coverage.json', directory / 'header-functions.json'
    if not report_only:
        path.unlink(missing_ok=True)
    original = primary_path.read_bytes()
    # Never convert an unbound or stale gcda into a report of current inputs.
    coverage.measure(product, compiler, gcov, report_only=True)
    primary = coverage._read_report(primary_path)
    inventory = header_inventory(product)
    header_hashes = coverage._hashes(list(inventory))
    for relative, digest in header_hashes.items():
        if primary['dependency_sha256'].get(relative) != digest:
            raise RuntimeError('private header was not bound by the original measurement: ' + relative)
    tools = _postprocessor_inputs()
    union = HeaderFunctionUnion(ROOT, {path: names for path, names in inventory.items() if names})
    companion = gcov or compiler_gcov(compiler)
    for relative in sorted(primary['object_profile_sha256']):
        if relative.endswith('.gcno'):
            union.add(relative, gcov_json(ROOT / relative, companion))
    report = {**union.report(), 'schema': 1, 'product': product, 'platform': sys.platform,
              'scope': SCOPE, 'compile_flags': primary['compile_flags'],
              'primary_report_sha256': hashlib.sha256(original).hexdigest(),
              'postprocessor_sha256': tools, 'header_sha256': header_hashes,
              'definition_inventory': {path.relative_to(ROOT).as_posix(): names for path, names in inventory.items()},
              'applicable': any(inventory.values()),
              'object_profile_sha256': primary['object_profile_sha256']}
    if (primary_path.read_bytes() != original or _postprocessor_inputs() != tools or
            coverage._hashes(list(inventory)) != header_hashes or header_inventory(product) != inventory or
            coverage._hashes([ROOT / relative for relative in primary['dependency_sha256']]) != primary['dependency_sha256'] or
            coverage._hashes([ROOT / relative for relative in primary['object_profile_sha256']]) != primary['object_profile_sha256']):
        raise RuntimeError('mail coverage input changed during private header postprocessing')
    if report_only:
        if not coverage._strict_equal(coverage._read_report(path), report):
            raise RuntimeError('private header coverage report differs from original executed profiles')
    else:
        path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    total = report['total']
    if report['applicable']:
        print(f"[header coverage] {product}: {total['functions']} functions; "
              f"{total['covered_lines']}/{total['lines']} lines; "
              f"{total['covered_branches']}/{total['branches']} branch outcomes", flush=True)
    else:
        print(f'[header coverage] {product}: no owned private static function definitions (not applicable)', flush=True)
    print('  original profile scope and private header gaps:', path, flush=True)
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--product', choices=[*coverage.SUITES, 'all'], default='all')
    parser.add_argument('--compiler', default='gcc')
    parser.add_argument('--gcov')
    parser.add_argument('--report-only', action='store_true')
    args = parser.parse_args()
    for product in coverage.SUITES if args.product == 'all' else [args.product]:
        measure(product, args.compiler, args.gcov, args.report_only)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
