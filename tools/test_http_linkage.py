"""Compile the four HTTP single headers as separate units with exactly one Core."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build
from xrt_manifest import expand_manifest_paths

ROOT = Path(__file__).resolve().parents[1]


class HttpLinkageTests(unittest.TestCase):
    def test_external_core_and_response_abi_without_client_features(self):
        compiler = shutil.which("gcc")
        self.assertIsNotNone(compiler, "gcc is required")
        overlays = expand_manifest_paths([
            Path("extlibs/xhttp-client/config/modules.json"),
            Path("extlibs/xhttp-server/config/modules.json"),
        ])
        _, _, _, _, defines, links, *_ = build._load_modules(
            "http_client_decompress,http_server_raw,http_redirect_policy", overlays
        )
        core = (ROOT / "single/xrt.h").as_posix()
        products = ("xhttp", "xhttp-policy", "xhttp-client", "xhttp-server")
        headers = {p: (ROOT / f"single/extlibs/{p}.h").as_posix() for p in products}
        prefixes = {p: p.replace("-", "_").upper() for p in products}
        sources = {"core": f'#define XRT_IMPLEMENTATION\n#include "{core}"\n'}
        for product in products:
            declarations = f'#include "{core}"\n' + "".join(
                f'#include "{headers[dependency]}"\n' for dependency in products
            )
            sources[product] = declarations + f'#define {prefixes[product]}_IMPLEMENTATION\n#include "{headers[product]}"\n'
        sources["main"] = f'#include "{core}"\n' + "".join(
            f'#include "{headers[p]}"\n' for p in products
        ) + """
int main(void) {
    xhttpclientconfig Client; xhttpserverconfig Server; uint64 Delay;
    xrtHttpClientConfigInit(&Client); xrtHttpServerConfigInit(&Server);
    if (!Client.Timeout || !Server.WriteSize ||
        !xrtHttpRetryBackoff(100,400,4,&Delay) || Delay != 400) return 1;
    xhttpresponse* Response=xrtHttpBridgeResponseCreate(
        XHTTP_VERSION_1_1,200,XRT_STR_LITERAL("OK"),NULL);
    if (!Response) return 2;
    str Encoding=(str)xrtMalloc(5); if (!Encoding) return 3;
    memcpy(Encoding,"gzip",5);
    xrtHttpBridgeResponseSetDecoded(Response,Encoding,4);
    xstrview Original=xrtHttpResponseOriginalEncoding(Response);
    int Result=Original.Size != 4 || memcmp(Original.Data,"gzip",4);
    xrtHttpResponseDestroy(Response);
    return Result;
}
"""
        # Retry is a policy selection, independent of the Client retry executor.
        _, _, _, _, retry_defines, *_ = build._load_modules("http_retry_policy", overlays)
        defines = list(dict.fromkeys(defines + retry_defines))
        with tempfile.TemporaryDirectory(prefix="xrt-http-link-") as temp:
            directory = Path(temp)
            objects = []
            for name, source in sources.items():
                path = directory / (name + ".c")
                path.write_text(source, encoding="utf-8")
                target = directory / (name + ".o")
                flags = ["-D" + d for d in defines if name in ("xhttp-client", "main") or d != "XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS"]
                result = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O0",
                    *flags, "-c", str(path), "-o", str(target)], capture_output=True, text=True, timeout=120)
                self.assertEqual(result.returncode, 0, result.stderr[:3500])
                objects.append(str(target))
            output = directory / "consumer.exe"
            system = ["-pthread", "-lm"] if sys.platform != "win32" else []
            result = subprocess.run([compiler, *objects, *system, *("-l" + lib for lib in links),
                "-o", str(output)], capture_output=True, text=True, timeout=120)
            self.assertEqual(result.returncode, 0, result.stderr[:3500])
            self.assertEqual(subprocess.run([str(output)], timeout=30).returncode, 0)


if __name__ == "__main__":
    unittest.main()
