# HTTP Util Fuzz Harness

This workspace now includes a fuzz-friendly harness for the public URL / HTTP utility layer:

- [`fuzz_http_util_core.c`](/D:/Git/xrt/dev/fuzz_http_util_core.c)

It exercises these public APIs with arbitrary input:

- `xrtUrlParseViewN`
- `xrtUrlParseTargetN`
- `xrtUrlParseAuthorityN`
- `xrtUrlResolveTo`
- `xrtUrlNormalizePathTo`
- `xrtQueryParseToN`
- `xrtCookieParseToN`
- `xrtFormUrlEncodedParseToN`
- `xrtHttpTokenParseToN`
- `xrtHttpHeaderSplitLineN`
- `xrtHttpHeaderParseBlockToN`
- `xrtSetCookieParseN`
- `xrtHttpParamFindN`
- `xrtHttpMediaTypeParseN`
- `xrtHttpContentDispositionParseN`
- `xrtHttpQuotedStringDecodeToN`
- `xrtHttpQuotedStringBuildToN`
- `xrtHttpDecodeExtValueTo`
- `xrtHttpBuildExtValueTo`
- `xrtMultipartBoundaryFromContentTypeN`
- `xrtMultipartParseToN`
- `xrtMultipartStream*`

## Standalone smoke build

```bash
gcc dev/fuzz_http_util_core.c xrt.c -I . -o dev/fuzz_http_util_core.exe -lws2_32 -liphlpapi
./dev/fuzz_http_util_core.exe
```

Workspace build helpers:

- [`build_test_http_util_fuzz.sh`](/D:/Git/xrt/build_test_http_util_fuzz.sh)
- [`build_GCC_TEST_HTTP_UTIL_FUZZ_x64.bat`](/D:/Git/xrt/build_GCC_TEST_HTTP_UTIL_FUZZ_x64.bat)
- [`build_test_http_util_fuzz_corpus.sh`](/D:/Git/xrt/build_test_http_util_fuzz_corpus.sh)
- [`build_GCC_TEST_HTTP_UTIL_FUZZ_CORPUS_x64.bat`](/D:/Git/xrt/build_GCC_TEST_HTTP_UTIL_FUZZ_CORPUS_x64.bat)
- [`build_test_http_util_fuzz_sanitize.sh`](/D:/Git/xrt/build_test_http_util_fuzz_sanitize.sh)
- [`build_test_http_util_libfuzzer.sh`](/D:/Git/xrt/build_test_http_util_libfuzzer.sh)

Default regression integration:

- [`build_test_modern.sh`](/D:/Git/xrt/build_test_modern.sh)
- [`build_GCC_TEST_MODERN_x64.bat`](/D:/Git/xrt/build_GCC_TEST_MODERN_x64.bat)

The standalone `main(...)` runs several built-in seeds or accepts one or more input files.

Tracked corpus:

- [`dev/fuzz/http_util_corpus`](/D:/Git/xrt/dev/fuzz/http_util_corpus)

## libFuzzer build

When building with clang/libFuzzer, define `XRT_FUZZ_LIBFUZZER_ONLY` so the standalone `main(...)` is omitted:

```bash
clang -DXRT_FUZZ_LIBFUZZER_ONLY -fsanitize=fuzzer,address dev/fuzz_http_util_core.c xrt.c -I . -o dev/fuzz_http_util_core_fuzzer
```

Workspace helper:

```bash
sh build_test_http_util_libfuzzer.sh
```

GitHub Actions workflow:

- [.github/workflows/http-util-fuzz.yml](/D:/Git/xrt/.github/workflows/http-util-fuzz.yml)

## Notes

- The harness uses bounded array sizes and a bounded multipart stream config.
- It is intentionally focused on parser safety and malformed-input tolerance.
- This is a safety entrypoint, not a protocol compliance test.
- The tracked corpus is aimed at malformed-input regression and can be used as a starting seed set for libFuzzer.
- The sanitizer corpus helper keeps the standalone `main(...)` so it works with `clang` or `gcc` ASan/UBSan builds.
- The dedicated libFuzzer helper requires `clang` and `-DXRT_FUZZ_LIBFUZZER_ONLY`.
- The GitHub Actions lane runs tracked corpus, ASan/UBSan corpus, a libFuzzer build smoke, the focused URL/HTTP util test, and the modern integration flow on Ubuntu.
