"""Measure ACME/JWT/OAuth2 private headers from existing executed object profiles.

No build or test is rerun. The original owned-C report must exactly match a fresh
union of the selected profiles before any private-header report is published.
Header functions remain a separate scope and never inflate the owned-C metric.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

from gcov_coverage import CoverageUnion, compiler_gcov, gcov_json
from gcov_header_coverage import HeaderFunctionUnion
from measure_mail_coverage import _read_report, _strict_equal
import measure_acme_coverage as acme
import measure_auth_coverage as auth
import measure_mail_header_coverage as headers

ROOT = Path(__file__).resolve().parents[1]
PRODUCTS = ('xacme', 'xjwt', 'xoauth2')
SCOPE = 'owned_private_header_functions_in_original_product_object_profiles'


def hashes(paths: list[Path]) -> dict[str, str]:
    return {path.resolve().relative_to(ROOT).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in sorted(set(paths))}


def postprocessor_inputs() -> dict[str, str]:
    return hashes([ROOT / 'tools' / name for name in
                   ('measure_extension_header_coverage.py', 'gcov_header_coverage.py',
                    'gcov_coverage.py', 'measure_acme_coverage.py', 'measure_auth_coverage.py',
                    'measure_mail_header_coverage.py', 'measure_mail_coverage.py')])


def original_profiles(product: str, primary: dict) -> tuple[list[Path], list[tuple[Path, list[Path]]]]:
    """Derive the original product scope, without including test or sibling objects."""
    if product == 'xacme':
        if primary['scope'] != 'module_mock_independent':
            raise RuntimeError('ACME private headers require the complete original coverage run')
        sources = acme.owned_sources()
        platform = 'windows' if sys.platform == 'win32' else 'linux'
        build = ROOT / f'out/gcc/native-{platform}/xacme_tests'
        profiles = [(acme.source_note(build, source), [source]) for source in sources]
        interop = ROOT / f'out/acme/coverage/{sys.platform}/interop'
        for stem, probe in (
            ('acme_http_client', 'test_http_interop_client.c'),
            ('acme_ownership_client', 'test_lifecycle_interop_client.c'),
            ('acme_provider_body_client', 'test_provider_body_client.c'),
            ('acme_provider_wire_client', 'test_provider_wire_client.c'),
        ):
            profiles.append((auth.notes_for(interop, stem), acme.embedding_sources(probe, sources)))
    else:
        name = product[1:]
        expected = 'main' if name == 'jwt' else 'main_and_independent'
        if primary['scope'] != expected:
            raise RuntimeError('authentication private headers require the complete original coverage run')
        sources = sorted((ROOT / f'extlibs/{product}/src').rglob('*.c'))
        output = ROOT / f'out/auth_extensions/coverage/{sys.platform}/{name}'
        profiles = [(auth.notes_for(output, name), sources)]
        if name == 'oauth2':
            for stem in ('xoauth2_tls_client', 'xoauth2_ownership_client'):
                profiles.append((auth.notes_for(output / 'interop', stem), sources))
    if {path.relative_to(ROOT).as_posix() for path in sources} != set(primary['source_sha256']):
        raise RuntimeError('original report does not contain every owned source')
    return sources, profiles


def measure(product: str, compiler: str = 'gcc', gcov: str | None = None,
            report_only: bool = False) -> dict:
    directory = ROOT / (f'out/acme/coverage/{sys.platform}' if product == 'xacme' else
                        f'out/auth_extensions/coverage/{sys.platform}/{product[1:]}')
    primary_path, path = directory / 'coverage.json', directory / 'header-functions.json'
    if not report_only:
        path.unlink(missing_ok=True)
    original = primary_path.read_bytes()
    primary = _read_report(primary_path)
    for field in ('source_sha256', 'dependency_sha256'):
        if not primary.get(field) or hashes([ROOT / relative for relative in primary[field]]) != primary[field]:
            raise RuntimeError('stale or unbound original coverage inputs: ' + field)
    sources, profiles = original_profiles(product, primary)
    inventory = headers.header_inventory(product)
    header_hashes = hashes(list(inventory))
    for relative, digest in header_hashes.items():
        if primary['dependency_sha256'].get(relative) != digest:
            raise RuntimeError('private header not bound by original measurement: ' + relative)
    postprocessors = postprocessor_inputs()
    profile_paths = [path for note, _ in profiles for path in (note, note.with_suffix('.gcda'))]
    profile_hashes = hashes(profile_paths)
    owned = CoverageUnion(ROOT, sources)
    private = HeaderFunctionUnion(ROOT, {path: names for path, names in inventory.items() if names})
    companion = gcov or compiler_gcov(compiler)
    for note, selected in profiles:
        label = note.relative_to(ROOT).as_posix()
        document = gcov_json(note, companion)
        owned.add(label, document, sources=selected)
        private.add(label, document)
    recomputed = owned.report()
    # ACME's final module objects also include the subsequent mock executions.
    # Repeated execution changes counts and contributions, not the union of gaps.
    for field in ('gcc_version', 'gcov_format', 'source_sha256', 'files', 'total'):
        if not _strict_equal(recomputed[field], primary[field]):
            raise RuntimeError('original profiles differ from primary owned-C report: ' + field)
    report = {**private.report(), 'schema': 1, 'product': product, 'platform': sys.platform,
              'scope': SCOPE, 'applicable': any(inventory.values()),
              'compile_flags': primary['compile_flags'],
              'independent_compile_flags': primary.get('independent_compile_flags', primary['compile_flags']),
              'primary_report_sha256': hashlib.sha256(original).hexdigest(),
              'postprocessor_sha256': postprocessors, 'header_sha256': header_hashes,
              'definition_inventory': {path.relative_to(ROOT).as_posix(): names for path, names in inventory.items()},
              'object_profile_sha256': profile_hashes,
              'primary_dependency_sha256': primary['dependency_sha256'],
              'primary_owned_c_union_verified': True}
    if (primary_path.read_bytes() != original or postprocessor_inputs() != postprocessors or
            hashes(list(inventory)) != header_hashes or headers.header_inventory(product) != inventory or
            hashes([ROOT / relative for relative in primary['source_sha256']]) != primary['source_sha256'] or
            hashes([ROOT / relative for relative in primary['dependency_sha256']]) != primary['dependency_sha256'] or
            hashes(profile_paths) != profile_hashes):
        raise RuntimeError('original coverage input changed during private header postprocessing')
    if report_only:
        if not _strict_equal(_read_report(path), report):
            raise RuntimeError('private header coverage differs from original executed profiles')
    else:
        path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    total = report['total']
    if report['applicable']:
        print(f"[header coverage] {product}: {total['functions']} functions; "
              f"{total['covered_lines']}/{total['lines']} lines; "
              f"{total['covered_branches']}/{total['branches']} branch outcomes", flush=True)
    else:
        print(f'[header coverage] {product}: no owned private static functions (not applicable)', flush=True)
    print('  verified original owned-C union and header gaps:', path, flush=True)
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--product', choices=(*PRODUCTS, 'all'), default='all')
    parser.add_argument('--compiler', default='gcc')
    parser.add_argument('--gcov')
    parser.add_argument('--report-only', action='store_true')
    args = parser.parse_args()
    for product in PRODUCTS if args.product == 'all' else (args.product,):
        measure(product, args.compiler, args.gcov, args.report_only)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
