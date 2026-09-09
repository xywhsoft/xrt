# xllm Release Checklist

This checklist defines the minimum release process for the Windows-first engineering bundle.

## Changelog Categories

Use these categories in `RELEASE_NOTES.md` for every release:

- `core`: runtime, request/response, errors, stream, tool call, structured output.
- `session`: history, compact/summary, state import/export, session bridge behavior.
- `memory`: ingest/search/list/remove, typed memory, retrieval, diagnostics, SQLite store, watcher worker.
- `provider`: adapter behavior, capability matrix, real-provider probe baselines, provider limitations.
- `release`: bundle layout, artifact verification, downstream integration, versioning, smoke reports.
- `security`: sensitive defaults, telemetry-free guarantees, delete/export governance, prompt-injection guidance.

## Required Gates

Before tagging a release:

1. Update `VERSION`, `xllm.h` version macros, and `RELEASE_NOTES.md`.
2. Run `cmd /c .\build.bat verify-version`.
3. Run `cmd /c .\build.bat singlehead`.
4. Run the relevant smoke suite and record report paths in `docs/SMOKE_BASELINE.md`.
5. Run `cmd /c .\build.bat static-analysis`.
6. Optionally run `cmd /c .\build.bat release-gate-options-test` directly when changing release-gate argument handling; this is already included in `static-analysis`.
7. Run `cmd /c .\build.bat release-bundle -VerifyCompile`.
8. Run `cmd /c .\build.bat verify-artifact -RootDir build\release_bundle -VerifyCompile`.
9. Run `cmd /c .\build.bat downstream-smoke`.
10. Apply release artifact signing policy from `docs\RELEASE_ARTIFACT_SIGNING.md` when producing an internal/public distribution artifact.
11. Run `cmd /c .\build.bat release-gate -RunDownstream` for final orchestration when time allows.

## Downstream Integration Smoke

`build.bat downstream-smoke` validates the release as a consumer would use it:

- Generates a release zip.
- Extracts `xllm-windows.zip`.
- Compiles a standalone minimal app against files from the extracted bundle.
- Links `xllm.c`, `xllm-session.c`, `xllm-memory.c`, and bundled SQLite.
- Runs a minimal runtime + memory ingest/search flow.
- Writes `downstream_integration_report.txt/json`.

This gate catches missing files, broken include paths, and bundle-only integration regressions that normal in-repo smoke can miss.

`build.bat release-gate -RunDownstream` runs this downstream integration smoke against the zip produced by the release gate, so the final release orchestration can cover both artifact verification and downstream consumption.

## Artifact Notes

- The default bundle remains Windows-first and expects MinGW-w64 `gcc` on `PATH`.
- Linux/macOS support remains a future build-track item; see `docs\CROSS_PLATFORM_BUILD_STRATEGY.md`.
- Real-provider probes require credentials and are not part of default release automation.
- Zip signing is not implemented yet; signing phases and public-release requirements are defined in `docs\RELEASE_ARTIFACT_SIGNING.md`.
- SQLite at-rest encryption is host-owned; release notes should not imply encrypted local memory unless a downstream product provides it.

## Release Note Template

```markdown
## x.y.z - YYYY-MM-DD

### core

### session

### memory

### provider

### release

### security

### known gaps
```
