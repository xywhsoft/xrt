# Replay Agent Run Example

> Corresponding source code: `examples/replay_agent_run.c`

This example demonstrates the basic capabilities of deterministic replay cassette.

## Problem solved

Agent runs mix model output, tool side effects, and file system state. replay allows these critical boundaries to be recorded, replayed and audited for regression testing, problem reproduction and security analysis.

## process

```text
record model/tool/checkpoint entries
record filesystem snapshot/ref
load cassette into strict replay engine
seek checkpoint
replay following tool entry
run audit replay with changed request
emit divergence report artifact
```

## Key points

- In strict replay mode, undocumented side effects should be blocked.
- audit replay will compare the new results with the recorded results and output divergence.
- filesystem snapshot/ref makes file state part of the replay input.
- The divergence report is saved using a structured report artifact.

## Strict and Audit

| Pattern | Behavior |
| --- | --- |
| record | Record entry/event, hash and raw payload. |
| strict | Fails when encountering the first mismatch, suitable for CI regression. |
| audit | Continue execution and accumulate divergence, suitable for security auditing and difference analysis. |

## Hash and divergence

- JSON payload uses `xwork_replay_hash_json()`, which sorts object keys and ignores meaningless whitespace.
- Use `xwork_replay_hash_text()` for non-JSON text.
- divergence will record expected/actual kind, key, hash and message.
- `xwork_replay_engine_emit_report_artifact()` can write divergence result as report artifact.

## Key API

| API | Function |
| --- | --- |
| `xwork_replay_engine_create()` | Create record/strict/audit replay engine. |
| `xwork_replay_engine_record_entry()` | Record cassette entry. |
| `xwork_replay_engine_record_filesystem_ref()` | Record typed filesystem ref. |
| `xwork_replay_engine_seek_checkpoint()` | Locate the entry after checkpoint. |
| `xwork_replay_engine_replay_entry()` | Compare expected entry. |
| `xwork_replay_engine_get_first_divergence()` | Query the first difference. |
| `xwork_replay_engine_emit_report_artifact()` | Issue divergence report artifact. |

## Suitable for expansion

- Save the Agent regression use case in CI as a replay cassette.
- Enable side-effect blocking for high-risk tool calls.
- Display divergence reports as test failures or security audit reports.
