# Release Gate Introduction

Release gate is the final pre-release check. It verifies version consistency, single-header behavior, static checks, release bundle structure, checksums, and downstream smoke tests.

[Back to Tutorials](README.en.md) | [Release API / Tool Reference](../api/api-release.en.md) | [Diagnostics Introduction](diagnostics-intro.en.md)

## What You Will Learn

This guide is for learners who need to consume or verify xllm release packages. It explains:

- What release gate protects.
- How to run common release verification commands.
- How to understand a release bundle.
- How to verify checksums, metadata, and compile checks.
- What downstream users should inspect first when they encounter problems.

## Who Needs to Run Release Gate

If you only use xllm as a dependency, you usually do not need to run the full release gate. You only need to confirm that the downloaded release package is complete and can compile your minimal program.

If you publish xllm, or you provide the xllm release package to other team projects, you should run release gate.

| Role | Recommendation |
| --- | --- |
| Ordinary user | Verify checksums and compile a minimal example |
| Integration owner | Run artifact verification and downstream smoke |
| Release maintainer | Run the full release gate |

## What Release Gate Checks

A complete pre-release check should cover at least:

| Check | Purpose |
| --- | --- |
| Version consistency | `VERSION`, header macros, release notes, and runtime version match |
| Single-header verification | Header and implementation include modes work |
| Static checks | Basic quality, release parameters, script checks |
| Release bundle | Artifact structure is complete |
| Checksums | Files are not damaged or accidentally modified |
| Compile verification | Release package compiles in downstream environment |
| Downstream smoke | Consume the release package like a real user |

## Common Commands

Run all commands from the repository root.

### Verify Version

```bat
cmd /c .\build.bat verify-version
```

If it fails, synchronize these locations first:

- `VERSION`
- `XLLM_VERSION_MAJOR/MINOR/PATCH` in `xllm.h`
- Return value of `xllm_version()`
- `RELEASE_NOTES.md`

### Verify Single Header

```bat
cmd /c .\build.bat singlehead
```

This check can catch include-order and macro-combination problems involving `XLLM_IMPLEMENTATION`, `XLLM_SESSION_IMPLEMENTATION`, and `XLLM_MEMORY_IMPLEMENTATION`.

### Static Checks

```bat
cmd /c .\build.bat static-analysis
```

This is used for pre-release quality checks. If it fails, inspect the exact check reported by the script before skipping anything.

### Generate Release Bundle

```bat
cmd /c .\build.bat release-bundle -VerifyCompile
```

`-VerifyCompile` runs compile verification after generating the bundle. Keep it enabled before release.

### Verify Release Artifact

```bat
cmd /c .\build.bat verify-artifact -RootDir build\release_bundle -VerifyCompile
```

This checks the release directory, metadata, checksums, and can run compile verification again.

### Downstream Smoke

```bat
cmd /c .\build.bat downstream-smoke
```

This step simulates how a user consumes the zip. It catches cases where the repository builds, but files are missing from the release package.

### Final Gate

```bat
cmd /c .\build.bat release-gate -RunDownstream
```

`-RunDownstream` includes downstream smoke in the final gate. Use it before a real release.

## Reading a Release Bundle

A release package usually contains:

| Content | Purpose |
| --- | --- |
| `include/` | Public headers |
| `src/` | Source files needed by implementation |
| `lib/` or dependency directory | Required dependencies such as xrt and SQLite |
| `release_metadata.json` | Version, build time, file layout, and other metadata |
| `SHA256SUMS.txt` | Checksums for the release directory |
| `BUNDLE_SHA256SUMS.txt` | Checksums for files inside the bundle |
| Examples or smoke tests | Materials for downstream compile verification |

After receiving a release package, a learner should first check whether `include/` is complete, then verify checksums, and finally compile a minimal program.

## Checksum Verification

Checksums detect damaged or unexpectedly modified files. They do not prove publisher identity, but they are a basic part of release gate.

When verifying, check:

- Whether `SHA256SUMS.txt` covers key files in the release directory.
- Whether `BUNDLE_SHA256SUMS.txt` covers key files inside the zip.
- Whether the verify script reports missing, extra, or hash-mismatched files.

If checksum verification fails, do not continue using that package. Regenerate or re-download the release artifact first.

## Metadata Verification

`release_metadata.json` should answer:

- Which xllm version this package contains.
- Which headers and source files are included.
- When it was generated and for which target platform.
- Where checksum files are located.
- Whether compile verification was executed.

If the metadata version does not match the header version, the release flow did not synchronize versions correctly.

## Downstream Compile Verification

Downstream verification means compiling and running using only files provided by the release package, without relying on repository-internal paths.

A qualified downstream smoke should cover at least:

- Include `xllm.h` and print `xllm_version()`.
- Create `xllm_runtime`.
- Compile session or memory related headers.
- Run a small memory ingest/search flow.

If downstream smoke fails, first suspect:

- The release package is missing headers or source files.
- Include paths do not match the documentation.
- xrt or SQLite dependencies were not packaged correctly.
- Single-header implementation macro combinations were not tested.

## Minimal Verification for Users

If you only consume a release package, you can run a lighter verification:

1. Verify the downloaded file hash.
2. Extract it into a clean directory.
3. Compile a program that only calls `xllm_version()` and `xllm_runtime_create`.
4. If you use memory, run one ingest/search flow.
5. If you use a provider, run one minimal request with your API key.

This quickly separates "the package itself is broken" from "the business integration is broken".

## Common Mistakes

Do not release only because the repository builds successfully. Release packages may miss files; artifact and downstream verification are required.

Do not ignore version mismatches. Wrong versions make it impossible for users to know which release a bug belongs to.

Do not treat checksums as signatures. Public distribution or automatic update scenarios still need a publisher signing strategy.

Do not dismiss release gate failures as script noise. A final-gate failure usually means the package or documentation promises carry real risk.

## Next Steps

- To see each release command in detail, read [Release API / Tool Reference](../api/api-release.en.md).
- To troubleshoot failed output, read [Diagnostics Introduction](diagnostics-intro.en.md).
- To learn the first program, read [First xllm Program](first-xllm-program.en.md).
