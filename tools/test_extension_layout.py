"""Keep every extension on the shared manifest and build workflow."""
from pathlib import Path
import json
import unittest

ROOT = Path(__file__).resolve().parents[1]
MIGRATED = ('xllm', 'xllm-session', 'xwork', 'xjwt', 'xoauth2')

class ExtensionLayoutTests(unittest.TestCase):
    def test_every_extension_has_one_common_manifest(self):
        products = [p for p in (ROOT / 'extlibs').iterdir() if p.is_dir()]
        self.assertTrue(products)
        for product in products:
            with self.subTest(product=product.name):
                manifest = json.loads((product / 'config/modules.json').read_text(encoding='utf-8'))
                self.assertEqual(manifest['single_header'], f'single/extlibs/{product.name}.h')
                self.assertEqual(manifest['declaration_header'], f'single/extlibs/{product.name}_decl.h')
                self.assertTrue((product / 'include' / (product.name + '.h')).is_file())
                self.assertFalse((product / '.github').exists())
                self.assertFalse((product / 'build.sh').exists())
                self.assertFalse((product / 'build.bat').exists())

    def test_migrated_production_units_never_embed_runtime_or_sources(self):
        for name in MIGRATED:
            product = ROOT / 'extlibs' / name
            manifest = json.loads((product / 'config/modules.json').read_text(encoding='utf-8'))
            owned = {s for m in manifest['modules'] for s in m['sources']}
            actual = {p.relative_to(ROOT).as_posix() for p in (product / 'src').rglob('*.c')}
            with self.subTest(product=name):
                self.assertEqual(owned, actual)
                self.assertFalse((product / (name + '.c')).exists())
                self.assertFalse((product / (name + '-xrt.h')).exists())
                self.assertFalse((product / (name + '-xrt.c')).exists())
            for path in owned:
                content = (ROOT / path).read_text(encoding='utf-8')
                with self.subTest(source=path):
                    self.assertNotRegex(content, r'#\s*define\s+XRT_IMPLEMENTATION')
                    self.assertNotRegex(content, r'#\s*include\s+"[^"\n]+\.c"')

    def test_oauth2_keeps_jwt_an_application_dependency(self):
        manifest = json.loads((ROOT / 'extlibs/xoauth2/config/modules.json').read_text(encoding='utf-8'))
        modules = {m['name']: m for m in manifest['modules']}
        self.assertNotIn('xjwt', modules['xoauth2']['depends'])
        self.assertIn('xjwt', modules['xoauth2_oidc']['depends'])

if __name__ == '__main__':
    unittest.main()
