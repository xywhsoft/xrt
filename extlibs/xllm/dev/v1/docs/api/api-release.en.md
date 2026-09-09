# xllm Release API / Tool Reference

> Related files: `VERSION`, `xllm.h`, `build.bat`, `RELEASE_NOTES.md`

This page is not a runtime API reference. It explains xllm release packages and release verification tools for users. You will learn how to confirm versions, understand release bundles, verify checksums, and understand what release gate protects.

## Version Information

xllm version comes from two places:

```c
#define XLLM_VERSION_MAJOR 0
#define XLLM_VERSION_MINOR 1
#define XLLM_VERSION_PATCH 0
```

At runtime, call:

```c
XLLM_API const char *xllm_version(void);
```

Before release, `VERSION`, version macros in `xllm.h`, the return value of `xllm_version()`, and `RELEASE_NOTES.md` must stay consistent.

## What the Release Package Contains

The current Windows release package usually contains:

| Content | Purpose |
| --- | --- |
| `xllm-windows.zip` | Zip package for downstream users. |
| `include/` | Public headers, such as `xllm.h`, `xllm-session.h`, and `xllm-memory.h`. |
| `src/` | Implementation files required by single-header / implementation mode. |
| `lib/` or third-party dependencies | Required dependencies such as xrt and SQLite. |
| `BUNDLE_SHA256SUMS.txt` | Checksums for files inside the bundle. |
| `SHA256SUMS.txt` | Checksums for the release output root. |
| `release_metadata.json` | Release metadata. |
| verify scripts | Verify package completeness, compileability, and checksum match. |

## Common Release Commands

### verify-version

Checks whether version declarations are consistent.

```bat
cmd /c .\build.bat verify-version
```

If versions are inconsistent, synchronize `VERSION`, header version macros, and release notes.

### singlehead

Verifies that single-header / implementation combinations still work.

```bat
cmd /c .\build.bat singlehead
```

This step catches issues caused by include order, implementation macros, and header splitting.

### static-analysis

Runs static checks and release-parameter checks.

```bat
cmd /c .\build.bat static-analysis
```

### release-bundle

Generates the release package.

```bat
cmd /c .\build.bat release-bundle -VerifyCompile
```

`-VerifyCompile` performs compile verification after bundle generation. Keep it enabled before release.

### verify-artifact

Verifies an already generated release artifact.

```bat
cmd /c .\build.bat verify-artifact -RootDir build\release_bundle -VerifyCompile
```

It checks checksums, metadata, package structure, and optionally compile verification.

### downstream-smoke

Extracts the release package like a downstream user, compiles a minimal program, and runs a memory ingest/search flow.

```bat
cmd /c .\build.bat downstream-smoke
```

This catches "builds inside the repository, but files are missing from the release package" problems.

### release-gate

Runs the final pre-release gate.

```bat
cmd /c .\build.bat release-gate -RunDownstream
```

`-RunDownstream` includes downstream smoke in the final release flow.

## Checksums and Signing

The current release tool baseline is checksum verification:

- The release output root generates `SHA256SUMS.txt`.
- The bundle generates `BUNDLE_SHA256SUMS.txt`.
- Verify scripts check whether file contents match.

Checksums detect file damage or accidental modification, but they do not prove publisher identity. The recommended signing strategy evolves in phases:

1. Local engineering releases continue to use checksum-only gate.
2. Internal candidate releases add optional detached signatures.
3. Windows distribution adds Authenticode when needed.
4. IDE or auto-update scenarios must verify publisher signature and checksums.

## Release Notes Categories

`RELEASE_NOTES.md` should use these categories:

| Category | Content |
| --- | --- |
| `core` | runtime, request/response, errors, stream, tools, structured output. |
| `session` | history, compact, summary, state import/export, session bridge. |
| `memory` | ingest/search/list/remove, typed memory, diagnostics, SQLite, watcher. |
| `provider` | adapter behavior, capability matrix, real provider probe results. |
| `release` | bundle layout, artifact verification, downstream integration, versioning. |
| `security` | safe defaults, no telemetry guarantee, deletion/export governance, prompt injection guidance. |
| `known gaps` | Known limitations. |

## Minimal Pre-Release Checklist

1. Update `VERSION`, `xllm.h` version macros, and `RELEASE_NOTES.md`.
2. Run `cmd /c .\build.bat verify-version`.
3. Run `cmd /c .\build.bat singlehead`.
4. Run related smoke tests and record report paths.
5. Run `cmd /c .\build.bat static-analysis`.
6. Run `cmd /c .\build.bat release-bundle -VerifyCompile`.
7. Run `cmd /c .\build.bat verify-artifact -RootDir build\release_bundle -VerifyCompile`.
8. Run `cmd /c .\build.bat downstream-smoke`.
9. Apply signing strategy for the release channel.
10. If time allows, run `cmd /c .\build.bat release-gate -RunDownstream`.

## Common Learner Questions

### I only want to use xllm. Do I need to run release gate?

No. Release gate is for maintainers publishing packages. Users only need to confirm that the downloaded bundle passes verification and compile it according to the tutorials.

### Why is downstream smoke important?

It simulates how someone uses the zip after receiving it. It catches missing includes, missing source files, missing SQLite files, and broken example links.

### Is the release package already signed?

In the current strategy, the default baseline is checksum verification. Signing is a later supply-chain hardening stage. Before public distribution or IDE auto-update, complete the signing strategy.

## Related Documentation

- [Diagnostics API](api-diagnostics.en.md)
- [Core API](api-core.en.md)
- [Back to API Index](README.en.md)
