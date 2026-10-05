#!/usr/bin/env python3
"""Verify isolated extension headers with real C compilation and shared Core linkage."""
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import amalgamate
import build
from xrt_manifest import expand_manifest_paths, load_manifest

ROOT = amalgamate.ROOT

class ExtensionSingleTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.compiler = shutil.which("gcc")
        if not cls.compiler:
            raise RuntimeError("gcc is required")
        cls.products = {p.parents[1].name: p for p in
                        sorted((ROOT / "extlibs").glob("*/config/modules.json"))}

    def compile(self, source, *flags):
        result = subprocess.run([self.compiler, "-std=c11", "-x", "c", *flags, "-"],
                                input=source, text=True, encoding="utf-8",
                                capture_output=True, timeout=120)
        self.assertEqual(result.returncode, 0, result.stderr[:2500])
        return result

    def chain(self, product):
        path = self.products[product]
        return [p.parents[1].name for p in expand_manifest_paths([path])]

    def test_only_owned_assets_are_embedded(self):
        for product in self.products:
            for suffix in (".h", "_decl.h"):
                with self.subTest(product=product, suffix=suffix):
                    path = ROOT / "single/extlibs" / (product + suffix)
                    text = path.read_text(encoding="utf-8")
                    origins = re.findall(r"/\* (?:public|internal|source|feature selection): ([^\n]+) \*/", text)
                    self.assertTrue(origins)
                    self.assertTrue(all(p.startswith("extlibs/" + product + "/") for p in origins), origins)
                    self.assertNotIn("#define XRT_SINGLE_HEADER 1", text)
                    self.assertNotIn("#define XRT_DECLARATIONS 1", text)
                    self.assertNotIn("#include <xrt.h>", text)
                    if suffix == "_decl.h":
                        self.assertNotIn("/* source:", text)

    def test_core_is_required(self):
        for product in self.products:
            for suffix in (".h", "_decl.h"):
                with self.subTest(product=product, suffix=suffix):
                    header = (ROOT / "single/extlibs" / (product + suffix)).as_posix()
                    result = subprocess.run([self.compiler, "-E", "-x", "c", "-"],
                        input=f'#include "{header}"\n', text=True, encoding="utf-8",
                        capture_output=True, timeout=30)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("requires XRT", result.stderr)

    def test_all_declarations_compile_with_supplied_declarations(self):
        for product in self.products:
            with self.subTest(product=product):
                lines = [f'#include "{(ROOT / "single/xrt_decl.h").as_posix()}"']
                for dependency in self.chain(product):
                    lines.append(f'#include "{(ROOT / "single/extlibs" / (dependency + "_decl.h")).as_posix()}"')
                self.compile("\n".join(lines), "-Wall", "-Wextra", "-Werror", "-fsyntax-only")

    def test_all_product_implementations_compile_with_supplied_dependencies(self):
        for product in self.products:
            with self.subTest(product=product):
                chain = self.chain(product)
                lines = ["#define XRT_MODULE_ALL"]
                lines += [f"#define {p.upper().replace('-', '_')}_MODULE_ALL" for p in chain]
                # Some existing modules borrow dependency-internal fast paths.
                # Their single-TU contract requires the dependency implementation first.
                lines += ["#define XRT_IMPLEMENTATION",
                          f'#include "{(ROOT / "single/xrt.h").as_posix()}"']
                for dependency in chain:
                    lines += [f"#define {dependency.upper().replace('-', '_')}_IMPLEMENTATION",
                              f'#include "{(ROOT / "single/extlibs" / (dependency + ".h")).as_posix()}"']
                self.compile("\n".join(lines), "-D_GNU_SOURCE", "-Wall", "-Wextra", "-Werror", "-fsyntax-only")

    def test_two_extensions_link_with_one_external_core(self):
        overlays = expand_manifest_paths([
            Path("extlibs/xhttp/config/modules.json"),
            Path("extlibs/xmail/config/modules.json"),
        ])
        _, _, _, _, defines, links, _, _, _, _ = build._load_modules("cookie,mail_wire", overlays)
        core = (ROOT / "single/xrt.h").as_posix()
        cookie = (ROOT / "single/extlibs/xhttp.h").as_posix()
        wire = (ROOT / "single/extlibs/xmail.h").as_posix()
        sources = {
            "core": f'#define XRT_IMPLEMENTATION\n#include "{core}"\n',
            "cookie": f'#include "{core}"\n#define XHTTP_IMPLEMENTATION\n#include "{cookie}"\n',
            "wire": f'#include "{core}"\n#define XMAIL_IMPLEMENTATION\n#include "{wire}"\n',
            "main": f'#include "{core}"\n#include "{cookie}"\n#include "{wire}"\n'
                    'static void* pWire = (void*)&xrtMailLineRead;\n'
                    'int main(void) { xcookiepair pair; size_t offset = 0;\n'
                    'return !pWire || xrtVersion() == 0 ||\n'
                    'xrtCookieNext(XRT_STR_LITERAL("sid=abc"), &offset, &pair) != XCOOKIE_NEXT_ITEM; }\n',
        }
        with tempfile.TemporaryDirectory(prefix="xrt-extension-link-") as temp:
            directory = Path(temp); objects = []
            for name, source in sources.items():
                target = directory / (name + ".o")
                self.compile(source, "-O0", *("-D" + d for d in defines), "-c", "-o", str(target))
                objects.append(str(target))
            output = directory / "consumer.exe"
            result = subprocess.run([self.compiler, *objects, *("-l" + lib for lib in links),
                                     "-o", str(output)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(subprocess.run([str(output)]).returncode, 0)

if __name__ == "__main__":
    unittest.main()
