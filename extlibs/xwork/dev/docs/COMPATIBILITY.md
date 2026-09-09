# xwork Compatibility Matrix

This file records the dependency snapshot that the current `xwork` tree is validated against.

## Current Snapshot

| Component | Version / Snapshot | Location | Notes |
| --- | --- | --- | --- |
| xwork | 0.1.0 | `xwork.h` | `XWORK_VERSION_MAJOR/MINOR/PATCH` |
| xllm | 0.1.0 | `lib/xllm.h`, `lib/xllm-session.h`, `lib/xllm-memory.h` | `XLLM_VERSION_MAJOR/MINOR/PATCH` |
| xrt | vendored snapshot | `lib/xrt.h` | No public `XRT_VERSION_*` macro is currently exposed in the header. Track by copied header snapshot until xrt adds a public version. |
| sqlite | vendored amalgamation | `lib/sqlite/sqlite3.c`, `lib/sqlite/sqlite3.h` | Used by file persistence tests/examples. |

## Update Rule

When refreshing `lib/` from upstream `xrt` or `xllm`:

1. Copy the dependency files into `lib/`.
2. Update this matrix if version macros or snapshot identity changed.
3. Run the default smoke set from `tests/README.md`.
4. Run `gcc -std=c11 -Wall -Wextra -pedantic -c xwork.c`.
5. Record behavior-impacting changes in `CHANGELOG.md`.

`xwork` should not depend on unstable private dependency internals unless the call site is covered by smoke tests and the dependency snapshot is updated together with this file.

