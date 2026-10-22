"""Keep HTTP message, policy and transport library boundaries independently verifiable."""
from pathlib import Path
import re
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build
import test_protocol_fuzz as protocol_fuzz
from xrt_manifest import expand_manifest_paths, load_manifest

ROOT = Path(__file__).resolve().parents[1]
PRODUCTS = ("xhttp", "xhttp-policy", "xhttp-client", "xhttp-server")


class HttpLayoutTests(unittest.TestCase):
    def closure(self, product, suite):
        manifests = expand_manifest_paths([Path(f"extlibs/{product}/config/modules.json")])
        return build._load_modules(suite, manifests, platform="windows")

    def test_message_library_owns_no_io_or_policy_runtime(self):
        sources, _, _, _, features, *_ = self.closure("xhttp", "xhttp")
        self.assertFalse(any(f.startswith(("XRT_FEATURE_NET", "XRT_FEATURE_TLS", "XRT_FEATURE_FILE")) for f in features))
        self.assertNotIn("XHTTP_FEATURE_HTTP_CLIENT", features)
        self.assertNotIn("XHTTP_FEATURE_HTTP_SERVER", features)
        self.assertNotIn("XHTTP_FEATURE_COOKIE_JAR", features)
        self.assertNotIn("XHTTP_FEATURE_HTTP_CACHE_STORE", features)
        self.assertFalse(any(p.startswith(("extlibs/xhttp-client/", "extlibs/xhttp-server/", "extlibs/xhttp-policy/")) for p in sources))

    def test_policy_owns_no_transport(self):
        _, _, _, _, features, *_ = self.closure("xhttp-policy", "xhttp_policy")
        self.assertFalse(any(f.startswith(("XRT_FEATURE_NET", "XRT_FEATURE_TLS")) for f in features))
        self.assertNotIn("XHTTP_FEATURE_HTTP_CLIENT", features)
        self.assertNotIn("XHTTP_FEATURE_HTTP_SERVER", features)

    def test_http_fuzz_closures_follow_product_boundaries(self):
        for target, product in (
            ("auth", "xhttp"), ("route", "xhttp-server"),
            ("router", "xhttp-server"), ("sse", "xhttp"),
        ):
            with self.subTest(target=target):
                config = protocol_fuzz.TARGETS[target]
                sources, features, _, includes = protocol_fuzz._closure(
                    config["module"], config["manifest"],
                )
                self.assertTrue(features)
                self.assertIn(f"extlibs/{product}/include", includes)
                self.assertTrue(any(path.startswith(f"extlibs/{product}/src/") for path in sources))

    def test_client_and_server_do_not_import_each_other(self):
        for product, suite, other in (
            ("xhttp-client", "xhttp_client", "xhttp-server"),
            ("xhttp-server", "xhttp_server", "xhttp-client"),
        ):
            with self.subTest(product=product):
                sources, *_ = self.closure(product, suite)
                self.assertFalse(any(p.startswith(f"extlibs/{other}/") for p in sources))

    def test_single_tests_select_their_leaf_modules(self):
        # Aggregates must not inject all features into a minimal single test.
        paths = expand_manifest_paths([
            Path("extlibs/xhttp-client/config/modules.json"),
            Path("extlibs/xhttp-server/config/modules.json"),
        ])
        _, _, singles, _, _, _, modules, by_name, *_ = build._load_modules(
            "xhttp_tests,xhttp_policy_tests,xhttp_client_tests,xhttp_server_tests",
            paths, platform="windows",
        )
        defines = build._single_test_defines(singles, modules, by_name, platform="windows")
        minimal = "extlibs/xhttp/tests/single/test_single_form_data_core.c"
        self.assertIn(minimal, defines)
        self.assertEqual(defines[minimal], [])  # The source selects FORM_DATA itself.
        for product in PRODUCTS:
            aggregate = by_name[product.replace("-", "_") + "_tests"]
            self.assertEqual(
                set(aggregate["single_test_owners"]), set(aggregate["single_tests"]),
            )

    def test_implementation_and_public_headers_have_one_owner(self):
        owners = {}
        for product in PRODUCTS:
            manifest = load_manifest(ROOT / f"extlibs/{product}/config/modules.json")
            for module in manifest["modules"]:
                for key in ("sources", "public_headers", "internal_headers"):
                    for path in module.get(key, []):
                        self.assertTrue(path.startswith(f"extlibs/{product}/"), path)
                        self.assertEqual(owners.setdefault(path, product), product, path)
            registered = {p for m in manifest["modules"] for p in m.get("sources", [])}
            actual = {p.relative_to(ROOT).as_posix() for p in (ROOT / f"extlibs/{product}/src").rglob("*.c")}
            self.assertEqual(registered, actual, product)
            for path in (ROOT / f"extlibs/{product}/src").rglob("*"):
                if path.suffix not in (".c", ".h"):
                    continue
                text = path.read_text(encoding="utf-8")
                self.assertNotRegex(text, r'#\s*include\s*[<"][^>"\n]*\.\./[^>"\n]*src/internal')
                self.assertNotRegex(text, r'#\s*define\s+XRT_IMPLEMENTATION')


if __name__ == "__main__":
    unittest.main()
