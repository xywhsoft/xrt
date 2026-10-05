"""Build real static/shared JWT and OAuth2 consumers with one shared Core runtime.

The libraries use the common module manifests and separate source translation
units. These probes verify independent libraries sharing one Core runtime. Public auth symbols
are selected from their headers; internal helpers are not exported by the DSOs.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
from pathlib import Path

import build as xrt_build
from xrt_manifest import expand_manifest_paths

ROOT = Path(__file__).resolve().parents[1]
LIBRARIES = ('jwt', 'oauth2')


def public_symbols(name: str) -> list[str]:
    source = (ROOT / f'extlibs/x{name}/include/x{name}/api.h').read_text(encoding='utf-8')
    masked = re.sub(r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', ' ', source)
    symbols = re.findall(r'\b(x' + name + r'[A-Za-z_]\w*)\s*\([^;{}]*\)\s*;', masked)
    if not symbols or len(symbols) != len(set(symbols)):
        raise RuntimeError('invalid public auth symbol inventory: ' + name)
    return sorted(symbols)


def inputs() -> list[Path]:
    result = [ROOT / 'single/xrt.h', ROOT / 'tools/test_auth_package.py']
    for name in LIBRARIES:
        library = ROOT / f'extlibs/x{name}'
        result += [*(library / 'include').rglob('*.h'), library / 'config/modules.json',
                   *[path for path in (library / 'src').rglob('*') if path.suffix in {'.c', '.h'}],
                   library / 'tests/package/test_consumer.c']
    return sorted(set(result))


def hashes(paths: list[Path]) -> dict[str, str]:
    return {path.relative_to(ROOT).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}


def build(compiler: str, archiver: str) -> dict:
    if sys.platform not in {'win32', 'linux'}:
        raise RuntimeError('auth delivery acceptance currently supports Windows and Linux')
    output = ROOT / 'out/auth_extensions/package' / sys.platform
    output.mkdir(parents=True, exist_ok=True)
    report_path = output / 'delivery.json'
    report_path.unlink(missing_ok=True)
    before = hashes(inputs())
    overlays = expand_manifest_paths([ROOT / f'extlibs/x{name}/config/modules.json' for name in LIBRARIES])
    _, _, _, _, modules, _, _, _, _, _ = xrt_build._load_modules('xjwt,xoauth2', overlays)
    config = output / 'modules.h'
    config.write_text(''.join('#define ' + module + '\n' for module in modules), encoding='utf-8')
    runtime = output / 'runtime.c'
    runtime.write_text('#define XRT_IMPLEMENTATION\n#include <xrt.h>\n', encoding='utf-8')
    composed = output / 'composed.c'
    composed.write_text('int test_jwt_package(void);\nint test_oauth2_package(void);\n'
                        'int main(void) { return test_jwt_package() || test_oauth2_package(); }\n', encoding='utf-8')
    flags = ['-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
             *([] if os.name == 'nt' else ['-D_GNU_SOURCE']), '-include', str(config),
             '-I', str(ROOT / 'single'), *[flag for name in LIBRARIES for flag in ('-I', str(ROOT / f'extlibs/x{name}/include'))]]
    system = ['-lws2_32', '-lbcrypt', '-ladvapi32', '-liphlpapi'] if os.name == 'nt' else ['-pthread', '-lm']
    symbols = {name: public_symbols(name) for name in LIBRARIES}
    executed, products = [], []
    exports_verified = {}

    def run(*arguments):
        subprocess.run(list(arguments), cwd=ROOT, check=True)

    for kind in ('static', 'shared'):
        directory = output / kind
        directory.mkdir(exist_ok=True)
        shared = kind == 'shared'
        core_object = directory / 'runtime.o'
        run(compiler, *flags, *(['-DXRT_BUILD_SHARED'] if shared else []),
            *(['-fPIC', '-fvisibility=hidden'] if shared and os.name != 'nt' else []),
            '-c', str(runtime), '-o', str(core_object))
        if not shared:
            core_library = directory / 'libxrt_auth.a'
            core_library.unlink(missing_ok=True)
            run(archiver, 'rcs', str(core_library), str(core_object))
        elif os.name == 'nt':
            core_library = directory / 'libxrt_auth.dll.a'
            core_dso = directory / 'xrt_auth.dll'
            run(compiler, '-shared', str(core_object), '-o', str(core_dso),
                '-Wl,--out-implib,' + str(core_library), *system)
            products.append(core_dso)
        else:
            core_library = directory / 'libxrt_auth.so'
            run(compiler, '-shared', str(core_object), '-o', str(core_library), '-Wl,-soname,libxrt_auth.so', *system)
        products.append(core_library)
        libraries = {}
        for name in LIBRARIES:
            manifest = json.loads((ROOT / f'extlibs/x{name}/config/modules.json').read_text(encoding='utf-8'))
            objects = []
            for source in manifest['modules'][0]['sources']:
                obj = directory / (Path(source).stem + '.o')
                # Separate DSOs export via the explicit public inventory. Their
                # Core calls bind through the one runtime library's import stubs.
                run(compiler, *flags, *(['-fPIC'] if shared and os.name != 'nt' else []),
                    '-c', str(ROOT / source), '-o', str(obj))
                objects.append(str(obj))
            if not shared:
                library = directory / f'libx{name}.a'
                library.unlink(missing_ok=True)
                run(archiver, 'rcs', str(library), *objects)
            elif os.name == 'nt':
                exports = directory / (name + '.def')
                exports.write_text('EXPORTS\n' + ''.join('  ' + symbol + '\n' for symbol in symbols[name]), encoding='utf-8')
                dso, library = directory / f'x{name}.dll', directory / f'libx{name}.dll.a'
                run(compiler, '-shared', *objects, str(exports), str(core_library), '-o', str(dso),
                    '-Wl,--out-implib,' + str(library), *system)
                products.append(dso)
            else:
                exports = directory / (name + '.map')
                exports.write_text('{ global:\n' + ''.join('  ' + symbol + ';\n' for symbol in symbols[name]) +
                                   'local: *; };\n', encoding='utf-8')
                library = directory / f'libx{name}.so'
                run(compiler, '-shared', *objects, '-L', str(directory), '-lxrt_auth', '-o', str(library),
                    '-Wl,--version-script=' + str(exports), '-Wl,-rpath,$ORIGIN', *system)
            libraries[name] = library
            products.append(library)
            if shared:
                if os.name == 'nt':
                    table = subprocess.run(['objdump', '-p', str(dso)], check=True, capture_output=True, text=True).stdout
                    marker = '[Ordinal/Name Pointer] Table'
                    if marker not in table:
                        raise RuntimeError('missing PE export table: ' + name)
                    table = table.split(marker, 1)[1].strip().split('\n\n', 1)[0]
                    exported = set(re.findall(
                        r'^\s*\[\s*\d+\]\s+(?:\+base\[\s*\d+\]\s+[0-9a-fA-F]+\s+)?(\S+)\s*$', table, re.M))
                else:
                    table = subprocess.run(['nm', '-D', '--defined-only', str(library)], check=True, capture_output=True, text=True).stdout
                    exported = {line.split()[-1] for line in table.splitlines() if line.split()}
                if exported != set(symbols[name]):
                    raise RuntimeError(f'auth public export inventory mismatch: {name}: {sorted(exported)}')
                exports_verified[name] = sorted(exported)
        for name in (*LIBRARIES, 'composed'):
            selected = LIBRARIES if name == 'composed' else (name,)
            sources = [str(ROOT / f'extlibs/x{library}/tests/package/test_consumer.c') for library in selected]
            binary = directory / (name + ('.exe' if os.name == 'nt' else ''))
            run(compiler, *flags, *(['-DXRT_USE_SHARED'] if shared else []),
                *(['-DAUTH_PACKAGE_COMPOSED', str(composed)] if name == 'composed' else []), *sources,
                *[str(libraries[library]) for library in selected], str(core_library),
                *(['-Wl,-rpath,$ORIGIN'] if shared and os.name != 'nt' else []),
                '-o', str(binary), *system)
            run(str(binary))
            executed.append(kind + '/' + name)
            products.append(binary)
            print('[auth package]', kind, name, 'passed', flush=True)
    if hashes(inputs()) != before:
        raise RuntimeError('auth delivery input changed during build or consumption')
    result = {'schema': 1, 'platform': sys.platform, 'scope': 'separate_library_and_composed_static_shared_consumers',
              'dependency_sha256': before, 'public_symbols': symbols, 'executed_consumers': executed,
              'artifact_sha256': hashes(products), 'core_runtime_instances_per_composed_process': 1}
    result['verified_shared_exports'] = exports_verified
    report_path.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print('[auth package] delivery report:', report_path, flush=True)
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default='gcc')
    parser.add_argument('--archiver', default='ar')
    args = parser.parse_args()
    build(args.compiler, args.archiver)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
