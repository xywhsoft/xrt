# xwork Replay Contract

This document records the current deterministic replay contract.

## Replay Entries

Replay entries are coarse cassette records for model/tool/host/filesystem/
process/terminal/artifact/checkpoint operations.

The v1 entry schema is represented by `xwork_replay_entry_options` and
`xwork_replay_entry_summary`:

- `eKind`: operation class.
- `sKey`: stable caller key inside the replay.
- `sOperationId`: model/tool/host operation id.
- `sRequestJson` / `sResponseJson`: model request/response payloads.
- `sArgumentsJson` / `sResultJson`: tool or host request/result payloads.
- `sRequestHash` / `sResponseHash` / `sArgumentsHash` / `sResultHash` /
  `sContentHash`: precomputed hashes for loaded cassettes.
- `iStatus`: operation status.

When JSON/text payloads are supplied, xwork computes a stable `fnv1a64` hash.
JSON payload fields use normalized JSON hashing via `xwork_replay_hash_json()`:
insignificant whitespace is removed, object keys are sorted, common string
escapes are normalized, and numeric exponents are normalized to lowercase.
Non-JSON text payloads continue to use `xwork_replay_hash_text()`. Replay
comparison still accepts legacy text-hash cassettes for compatibility when the
expected JSON text matches the old recorded text.

`xwork_replay_entry_summary` owns copies returned by list/replay/load APIs and
must be released with `xwork_replay_entry_summary_reset()` or
`xwork_replay_entry_summary_list_reset()`. Replay engines are not internally
synchronized; callers must serialize access to a single engine if multiple
threads may record or replay against it.

## Filesystem Snapshot References

Filesystem snapshot/ref records have a typed wrapper over replay filesystem
entries:

- `xwork_replay_filesystem_ref_options`: caller-provided ref id, path,
  metadata JSON, content hash, and status.
- `xwork_replay_engine_record_filesystem_ref()`: records a ref in record mode.
- `xwork_replay_engine_load_filesystem_ref()`: loads a ref cassette into a
  strict/audit replay engine.
- `xwork_replay_engine_replay_filesystem_ref()`: compares the expected ref id,
  path metadata, content hash, and status.
- `xwork_replay_engine_list_filesystem_refs()`: lists only entries whose
  operation id is `XWORK_REPLAY_FILESYSTEM_SNAPSHOT_REF`.

The typed ref API uses existing replay entry raw payload fields:
`sKey` stores the ref id, `sRequestJson` stores the path, `sArgumentsJson`
stores caller-defined metadata JSON, and `sContentHash` stores the filesystem
content hash. This keeps the `.replay` binary layout unchanged while giving
callers a stable snapshot/ref surface.

## Event Log Schema

Replay events are fine-grained ordered records for stream/event comparison.
The v1 event schema is represented by `xwork_replay_event_options` and
`xwork_replay_event_summary`:

- `eKind`: `generic`, `model_stream`, `run_event`, `tool_event`, or
  `terminal_interaction`.
- `sKey`: stable event subject id, such as response id, tool-call id,
  artifact id, terminal session id, or caller-defined event key.
- `sName`: event source/name, such as `xllm.model_event` or a host tool id.
- `iType`: provider/tool/run event type integer.
- `sPayloadJson`: structured event payload to hash.
- `sContentText`: text/chunk content to hash.
- `sPayloadHash` / `sContentHash`: precomputed hashes for loaded event logs.
- `iStatus`: event status.

`xwork_replay_event_options_from_model_event()` maps `xwork_model_event` into
the v1 event schema without allocating. It uses the response/tool/artifact id
as `sKey`, `xllm.model_event` as `sName`, the raw model event type as `iType`,
and text/argument/format deltas as content.

## Modes

- Record mode accepts `xwork_replay_engine_record_entry()` and
  `xwork_replay_engine_record_event()`.
- Strict mode compares loaded cassettes/logs and fails on the first mismatch.
- Audit mode compares loaded cassettes/logs but keeps running and records
  divergence details.

Entry replay uses `xwork_replay_engine_replay_entry()`.
Event sequence replay uses `xwork_replay_engine_replay_event()`.
Both surfaces feed `xwork_replay_result` and first-divergence query/reporting.
Entry replay always compares kind/key/request-side hashes. Response/result/
content hashes are compared when the caller supplies an expected response,
result, or content hash; this lets strict offline replay serve recorded host
outputs without calling the real host.

## Runtime Host Service Replay

`xwork_runtime_options::pReplayEngine` is a borrowed replay engine pointer.
When present, `xwork_runtime_invoke_host_service()` and
`xwork_runtime_invoke_host_service_ex()` integrate with it:

- record mode invokes the configured host service and records filesystem,
  process, or generic host-tool entries with request arguments and result
  payloads.
- record mode with side-effect blocking returns `XWORK_ERROR_PAUSED` before
  invoking side-effecting host services.
- strict/audit modes do not invoke the configured host service. They replay the
  next cassette entry and return the stored `sResultJson` as
  `xwork_tool_result.sOutputText`.

The replayed output pointer is owned by the runtime scratch buffer and remains
valid until the next replayed host-service call on that runtime or until
`xwork_runtime_destroy()`.

## Terminal Boundary

Terminal replay is currently event-log based. xwork records and compares
terminal interaction metadata/content hashes through
`XWORK_REPLAY_EVENT_TERMINAL_INTERACTION` and terminal replay entry kinds, but
it does not rehydrate live interactive terminal sessions. A replay can verify
the recorded terminal transcript/state hashes; hosts must rediscover or start
new live sessions outside deterministic replay.

## Persistence Boundary

File persistence format v14 stores replay manifests, entry cassettes, replay
results, raw entry request/response/argument/result payloads, and typed
filesystem snapshot refs via the existing entry payload fields. v9-v12 replay
files remain readable for hash-only replay comparison; raw host output replay
requires v13+ data. The v1 event log schema is public and smoke-tested in
memory, but is not yet stored in `.replay` files. Persisting replay event logs
will require a future persistence format bump.
