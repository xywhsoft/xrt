"""Guard WebSocket product boundaries and public layout across optional modules."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

import build
import amalgamate
from xrt_manifest import dependency_closure, expand_manifest_paths, load_manifest

ROOT = Path(__file__).resolve().parents[1]
PRODUCTS = ('xws', 'xws-client', 'xws-server', 'xws-group')


class WebSocketLayoutTests(unittest.TestCase):
    def closure(self, product, suite):
        overlays = expand_manifest_paths([Path(f'extlibs/{product}/config/modules.json')])
        modules = load_manifest()['modules'] + [m for p in overlays for m in load_manifest(p)['modules']]
        return dependency_closure([suite], modules, all_platforms=True)

    def test_public_entries_resolve_module_dependencies(self):
        compiler = shutil.which('gcc')
        self.assertIsNotNone(compiler)
        for product in PRODUCTS:
            overlays = expand_manifest_paths([Path(f'extlibs/{product}/config/modules.json')])
            flags = ['-I', str(ROOT / 'include')]
            for overlay in overlays:
                for include in load_manifest(overlay)['include_dirs']:
                    flags += ['-I', str(ROOT / include)]
            consumer = ROOT / f'extlibs/{product}/tests/package/test_consumer.c'
            result = subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
                                     *flags, '-fsyntax-only', str(consumer)],
                                    capture_output=True, text=True, encoding='utf-8', timeout=30)
            self.assertEqual(result.returncode, 0, product + ': ' + result.stderr)

    def test_system_includes_are_not_resolved_as_project_headers(self):
        for name in ('math.h', 'string.h'):
            self.assertIsNone(amalgamate._local_path('include/xrt/wait.h', name, ['include'], quoted=False))
        self.assertEqual(amalgamate._local_path('include/xrt/wait.h', 'time.h', ['include'], quoted=True),
                         'include/xrt/time.h')

    def test_base_and_group_have_no_http_dependency(self):
        for product in ('xws', 'xws-group'):
            with self.subTest(product=product):
                chain = expand_manifest_paths([Path(f'extlibs/{product}/config/modules.json')])
                self.assertFalse(any('xhttp' in p.as_posix() for p in chain))
                modules = self.closure(product, product.replace('-', '_'))
                self.assertFalse(any(s.startswith('extlibs/xhttp') for m in modules for s in m['sources']))

    def test_adapter_production_closures_are_one_way(self):
        for side, other in (('client', 'server'), ('server', 'client')):
            names = {m['name'] for m in self.closure(f'xws-{side}', f'xws_{side}')}
            self.assertFalse(any(n.startswith(f'websocket_{other}') for n in names))
            self.assertFalse(any(n.startswith(f'http_{other}') for n in names))
            self.assertNotIn('websocket_group', names)
        names = {m['name'] for m in self.closure('xws-server', 'websocket_server')}
        self.assertNotIn('http_server_router', names)

    def test_products_do_not_import_dependency_private_headers(self):
        for product in PRODUCTS:
            for path in (ROOT / 'extlibs' / product / 'src').rglob('*'):
                if path.suffix not in ('.h', '.c'):
                    continue
                text = path.read_text(encoding='utf-8')
                with self.subTest(source=path.relative_to(ROOT)):
                    self.assertNotRegex(text, r'#\s*include\s*[<"][^\n"]*(?:\.\./\.\./\.\.|/src/internal/)[^\n"]*[>"]')
                    self.assertNotRegex(text, r'#\s*include\s*"[^"\n]+\.c"')
                    self.assertNotIn('__xrtWsConnRangeValid', text if product != 'xws' else '')

    def test_optional_features_preserve_public_layout(self):
        compiler = shutil.which('gcc')
        if compiler is None:
            self.fail('gcc is required for WebSocket layout verification')
        variants = {
            'xws': ('websocket_connection', 'xws'),
            'xws-client': ('websocket_client', 'xws_client'),
            'xws-server': ('websocket_server', 'xws_server'),
        }
        with tempfile.TemporaryDirectory(prefix='xws-layout-') as temp:
            for product, suites in variants.items():
                results = []
                for suite in suites:
                    overlays = expand_manifest_paths([Path(f'extlibs/{product}/config/modules.json')])
                    _, _, _, _, defines, _, _, _, includes, _ = build._load_modules(suite, overlays)
                    types = ['xwsconnconfig']
                    if product != 'xws':
                        side = product.split('-')[1]
                        types += [f'xws{side}config', f'xws{side}handshake']
                    source = f'#include <stdio.h>\n#include <stddef.h>\n#include <{product}.h>\nint main(void) {{\n'
                    source += ''.join(f'printf("%zu ", sizeof({t}));\n' for t in types)
                    source += 'printf("%zu %zu", offsetof(xwsconnconfig, AsyncBytesLimit), offsetof(xwsconnconfig, Deflate));\n'
                    source += 'return 0; }\n'
                    output = Path(temp) / (suite + '.exe')
                    command = [compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', '-x', 'c', '-']
                    command += ['-D' + d for d in defines]
                    for inc in ['include', *includes]:
                        command += ['-I', str(ROOT / inc)]
                    command += ['-o', str(output)]
                    compiled = subprocess.run(command, input=source, text=True, encoding='utf-8', capture_output=True, timeout=60)
                    self.assertEqual(compiled.returncode, 0, compiled.stderr)
                    ran = subprocess.run([str(output)], capture_output=True, text=True, timeout=10)
                    self.assertEqual(ran.returncode, 0, ran.stderr)
                    results.append(ran.stdout)
                self.assertEqual(results[0], results[1], product)

    def test_four_products_link_to_one_external_core(self):
        compiler = shutil.which('gcc')
        self.assertIsNotNone(compiler, 'gcc is required')
        overlays = expand_manifest_paths([
            Path('extlibs/xws-server/config/modules.json'),
            Path('extlibs/xws-group/config/modules.json'),
        ])
        sources, _, _, _, defines, links, _, _, _, _ = build._load_modules(
            'websocket_client_future,websocket_server_future,websocket_group_future', overlays)
        manifests = [load_manifest(p) for p in overlays]
        products = ['xrt', *(p.parents[1].name for p in overlays)]
        headers = [ROOT / 'single/xrt.h', *(ROOT / load_manifest(p)['single_header'] for p in overlays)]
        preamble = ''.join(f'#include "{p.as_posix()}"\n' for p in headers)
        units = [('xrt', '#define XRT_IMPLEMENTATION\n' + preamble)]
        for product, manifest, header in zip(products[1:], manifests, headers[1:]):
            if any(s.startswith(manifest['product_root'] + '/src/') for s in sources):
                macro = manifest['implementation_macro']
                units.append((product, preamble + f'#define {macro}\n#include "{header.as_posix()}"\n'))
        main = preamble + '''
int main(void) {
    xwsconnconfig Connection;
    xwsserverconfig Server;
    xwsclientconfig Client;
    char Key[XWS_KEY_CAPACITY];
    xrtWsConnConfigInit(&Connection);
    xrtWsServerConfigInit(&Server);
    xrtWsClientConfigInit(&Client);
    if (!xrtWsConnConfigValid(&Connection) || !xrtWsServerConfigValid(&Server)) return 1;
    xhttprequest* Request = xrtWsClientRequestCreate(XRT_STR_LITERAL("ws://localhost/test"), &Client, Key);
    if (Request == NULL) return 2;
    xrtHttpRequestDestroy(Request);
    xwsgroup* Group = xrtWsGroupCreate(8);
    if (Group == NULL) return 3;
    xwsgroupsnapshot* Snapshot = xrtWsGroupSnapshotCreate(Group);
    if (Snapshot == NULL || xrtWsGroupSnapshotCount(Snapshot) != 0) return 4;
    xrtWsGroupSnapshotDestroy(Snapshot);
    xrtWsGroupDestroy(Group);
    return xrtVersion() == 0;
}
'''
        flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-O0', *('-D' + d for d in defines)]
        libraries = ['-l' + lib for lib in links]
        if sys.platform != 'win32':
            libraries += ['-pthread', '-lm']

        def run(command, **kwargs):
            result = subprocess.run(command, capture_output=True, text=True, encoding='utf-8', timeout=120, **kwargs)
            self.assertEqual(result.returncode, 0, result.stderr[:4000])
            return result

        with tempfile.TemporaryDirectory(prefix='xws-external-core-') as temp:
            directory = Path(temp)
            # Real separate object files ensure extensions cannot borrow static
            # symbols or silently embed a second Core implementation.
            objects = []
            for product, source in units:
                target = directory / (product + '.o')
                run([compiler, *flags, '-x', 'c', '-', '-c', '-o', str(target)], input=source)
                objects.append(str(target))
            consumer = directory / 'consumer.exe'
            run([compiler, *flags, '-x', 'c', '-', '-x', 'none', *objects, *libraries, '-o', str(consumer)], input=main)
            run([str(consumer)], cwd=directory)
            if sys.platform == 'win32':
                imports = []
                for product, source in units:
                    macro = product.upper().replace('-', '_')
                    prior = [u[0].upper().replace('-', '_') for u in units[:len(imports)]]
                    dll = directory / (product + '.dll')
                    archive = directory / (product + '.dll.a')
                    run([compiler, *flags, '-D' + macro + '_BUILD_SHARED',
                         *('-D' + p + '_USE_SHARED' for p in prior), '-x', 'c', '-', '-x', 'none',
                         '-shared', *imports, *libraries, '-Wl,--out-implib,' + str(archive), '-o', str(dll)], input=source)
                    imports.append(str(archive))
                run([compiler, *flags, *('-D' + p.upper().replace('-', '_') + '_USE_SHARED' for p, _ in units),
                     '-x', 'c', '-', '-x', 'none', *imports, *libraries, '-o', str(consumer)], input=main)
                run([str(consumer)], cwd=directory)


if __name__ == '__main__':
    unittest.main()
