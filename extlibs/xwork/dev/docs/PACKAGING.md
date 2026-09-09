# xwork Packaging

This repository currently ships `xwork` as a source-level C library.

## Public Surface

- `xwork.h` is the canonical public API header.
- `xwork.c` is the aggregate implementation translation unit.
- `src/xwork_*/*.c` files are internal implementation slices included by `xwork.c`.
- `lib/` contains vendored dependency headers and sources required by the current source build.
- `include/` is reserved for a future installed-header layout; it is not the canonical include path today.

## Single-C Usage

For the simplest integration, compile `xwork.c` into the consuming program or into a static library:

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -c xwork.c
```

Tests and examples that need sqlite-backed file persistence compile sqlite alongside the test binary:

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -Ilib\sqlite tests\xwork_orchestrator_smoke.c lib\sqlite\sqlite3.c -o tests\xwork_orchestrator_smoke.exe -lws2_32 -liphlpapi
```

The aggregate `xwork.c` includes all implementation slices in a fixed order. Consumers should not compile `src/xwork_*/*.c` separately unless they intentionally replace the aggregate model.

## Header Dependencies

`xwork.h` forward-declares `xllm` types for public interop. The implementation expects the vendored `lib/xrt.h`, `lib/xllm.h`, `lib/xllm-session.h`, and `lib/xllm-memory.h` headers to be available through the repository-relative includes used by `xwork.c` and tests.

## Version Rule

The public version is exposed by:

- `XWORK_VERSION_MAJOR`
- `XWORK_VERSION_MINOR`
- `XWORK_VERSION_PATCH`
- `xwork_version()`

Until the API is declared stable, `0.x` versions are treated as source-compatible snapshots. Any public struct layout, enum value, or function signature change may require a minor version bump. Patch bumps are reserved for compatible fixes, docs, tests, or internal behavior corrections.

Persistence format compatibility is tracked separately by `XWORK_PERSISTENCE_FORMAT_VERSION`.

The release checklist for freezing the current public source surface is tracked
in `docs/API_FREEZE_0_1.md`.
