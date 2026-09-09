# xwork API Reference Rewrite Spec

This file tracks the work that upgraded `docs/api` from module overviews to function-by-function API reference pages.

Current status:

- Unique `XWORK_API` functions in `xwork.h`: `367`
- Covered by function-level `###` headings: `367`
- Missing function-level heading coverage: `0`

## Completion Criteria

- [x] Every public function has its own level-3 heading.
- [x] Every function section includes purpose, prototype, parameters, return value, ownership, notes, sample code, and related APIs.
- [x] Added `tools/check_api_reference_coverage.ps1` to compare `xwork.h` API functions with API reference headings.
- [x] Completed coverage for runtime, workspace, tools, run, policy, artifacts, persistence, multi-agent, remote worker, replay, local host, and shared helper pages.

## Module Status

| Module | Page | Status |
| --- | --- | --- |
| Common Types | `docs/api/types.md` | [x] |
| Runtime core | `docs/api/api-runtime.md` | [x] |
| Profiles | `docs/api/api-profiles.md` | [x] |
| xllm integration | `docs/api/api-xllm-integration.md` | [x] |
| Workspace | `docs/api/api-workspace.md` | [x] |
| Tools | `docs/api/api-tools.md` / `docs/api/api-host-tools.md` | [x] |
| Run / Event / Async / Query | `docs/api/api-run.md` / `docs/api/api-orchestrator.md` | [x] |
| Policy / Approval | `docs/api/api-policy-approval.md` | [x] |
| Artifacts | `docs/api/api-artifacts.md` | [x] |
| Persistence / Checkpoint | `docs/api/api-persistence.md` | [x] |
| Multi-Agent | `docs/api/api-multi-agent.md` | [x] |
| Remote Worker | `docs/api/api-remote-worker.md` | [x] |
| Replay | `docs/api/api-replay.md` | [x] |
| Local Host | `docs/api/api-local-host.md` | [x] |
