# xwork 0.1 API Freeze Checklist

This document records the release gate for freezing the `0.1.0` public source
surface. The canonical public surface remains `xwork.h`.

## Freeze Scope

The `0.1.0` source snapshot includes:

- Version macros and `xwork_version()`.
- Public status values and `xwork_status_cstr()`.
- Runtime, workspace, run, async run, tool registry, host service, policy,
  event, approval, checkpoint, artifact, persistence, profile, session policy,
  orchestrator, multi-agent, remote worker/control plane, and replay public
  structs/functions in `xwork.h`.
- File persistence format version `XWORK_PERSISTENCE_FORMAT_VERSION == 9`.
- Source aggregation through `xwork.c`.

## Compatibility Rules

Before `0.1.0` is declared frozen:

- Every public struct must have a matching `*_init` function unless it is an
  intentionally opaque handle.
- Every public owning object or list with heap members must have a matching
  reset/destroy function.
- Public functions must return the narrow `xwork_status` set for recoverable
  failures instead of leaking dependency-specific error codes.
- Public callback contracts must document ownership of input/output strings and
  cancellation behavior in `xwork.h`.
- File persistence changes that alter serialized data must bump
  `XWORK_PERSISTENCE_FORMAT_VERSION` and update `docs/PERSISTENCE_FORMAT.md`.
- Dependency refreshes must update `docs/COMPATIBILITY.md` when version or
  behavior changes.

After `0.1.0` is frozen:

- Patch releases may add docs, tests, compatible fixes, and internal behavior
  corrections.
- Adding fields to public structs or enum values requires a minor version bump.
- Removing, renaming, or changing signatures requires an explicit breaking
  compatibility note and should not happen in a patch release.

## Release Gate

Run these checks before tagging a source snapshot:

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -c xwork.c
gcc -std=c11 -Wall -Wextra -pedantic -Ilib\sqlite tests\xwork_core_smoke.c lib\sqlite\sqlite3.c -o tests\xwork_core_smoke.exe -lws2_32 -liphlpapi
tests\xwork_core_smoke.exe
gcc -std=c11 -Wall -Wextra -pedantic -Ilib\sqlite tests\xwork_persistence_smoke.c lib\sqlite\sqlite3.c -o tests\xwork_persistence_smoke.exe -lws2_32 -liphlpapi
tests\xwork_persistence_smoke.exe
```

The full default smoke matrix is listed in `tests/README.md` and mirrored by
`.github/workflows/ci.yml`.

## Current Review Status

- Version and status string contracts are present.
- Public init/reset/destroy coverage exists for the currently exposed owning
  objects and lists.
- Persistence format v9 is documented and has focused newer-version rejection
  coverage.
- Host tool JSON contracts are documented separately in
  `docs/HOST_TOOL_CONTRACTS.md`.
- Remaining release risk is dependency behavior drift from parallel `xllm` and
  `xrt` development; refreshes should be validated through the default smoke
  matrix before a frozen source snapshot is cut.
