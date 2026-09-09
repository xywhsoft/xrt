# xllm Versioning Rules

xllm uses semantic versioning with explicit 0.x rules until the public API reaches 1.0.

## Version Sources

The canonical version is stored in:

- `VERSION`
- `xllm.h` version macros and `xllm_version()`
- `RELEASE_NOTES.md`
- release bundle `release_metadata.json`

The release verification scripts must keep these aligned.

## SemVer Shape

Format:

- `MAJOR.MINOR.PATCH`
- optional prerelease suffix: `MAJOR.MINOR.PATCH-alpha.N`, `beta.N`, or `rc.N`

Examples:

- `0.8.0`
- `0.8.1`
- `1.0.0-rc.1`
- `1.0.0`

## 0.x Policy

Before 1.0, xllm may still change provisional and experimental APIs, but changes must be intentional and documented.

Patch release:

- bug fixes.
- smoke/release script fixes.
- documentation corrections.
- compatible diagnostics additions.

Minor release:

- new public APIs.
- new provider capabilities.
- new memory typed policies.
- behavior changes in provisional/experimental APIs.
- source-compatible additions to stable structs/enums.

Breaking change in 0.x:

- allowed only in minor releases.
- must be listed in `RELEASE_NOTES.md`.
- must update affected docs and smoke coverage.
- must not silently alter conservative safety defaults, such as automatic long-term memory writes.

## 1.0 Policy

After 1.0:

- MAJOR increments for source or ABI breaking public API changes.
- MINOR increments for backward-compatible features.
- PATCH increments for fixes and docs.

Stable APIs must not be broken without a major release.

## Public API Change Rules

- Public structs may only gain fields at the tail.
- Public enums may only gain values at the tail or in explicitly reserved extension ranges.
- Existing enum numeric values must not change.
- `*_options_init` must initialize all new fields to safe defaults.
- `*_result_reset` / free functions must remain valid for zero-initialized objects.
- Default behavior must remain conservative.
- Serialized `xllm_session_state` checkpoints follow the compatibility rules in `docs\SESSION_STATE_COMPATIBILITY.md`.

## Stability Labels

Use the labels in `docs\MEMORY_API_STABILITY.md`:

- `Stable`: source-compatible only.
- `Provisional`: compatible additions preferred; breaking 0.x minor changes allowed with release notes.
- `Experimental`: may change faster, but must remain covered by smoke or migration notes.

## Release Checklist

Before a release:

- Run version verification.
- Run release metadata verification.
- Run checksum verification.
- Run `singlehead`.
- Run release gate.
- Update `RELEASE_NOTES.md`.
- Record latest smoke report paths.

Recommended command:

```powershell
cmd /c .\build.bat release-gate -RunDownstream
```

## 1.0 Freeze Candidates

The following areas are candidates for 1.0 stabilization:

- core runtime/profile/request/response/error model.
- session history/compact/state export/import.
- memory basic ingest/search/list/remove/context apply.
- memory diagnostics.
- provider capability matrix and observability contract.

The following should remain provisional until broader downstream usage:

- watcher worker.
- task/fact/preference typed memory policies.
- retrieval eval/debug APIs.
- DB health check APIs.
