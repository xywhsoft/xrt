# xwork File Persistence Format

This document records the v1 file persistence contract for the built-in
`xwork_file_persistence` backend.

## Scope

The file backend is intended for local durable agent runs:

- latest run recovery
- checkpoint recovery
- event audit logs
- artifact storage and summary queries
- run index and step history queries
- multi-agent agent pool snapshot storage
- multi-agent task graph snapshot storage
- remote worker control plane snapshot storage
- deterministic replay manifest/cassette/result storage

It is not a distributed store or a multi-writer database.

## Root Layout

Configured root:

```text
<root>/
  runs/
    <hex-encoded-run-id>/
      latest.snapshot
      events.log
      checkpoints/
        <hex-encoded-checkpoint-id>.snapshot
      artifacts/
        <hex-encoded-artifact-id>.meta
  task_graphs/
    <hex-encoded-graph-id>.graph
  agent_pools/
    <hex-encoded-pool-id>.pool
  control_planes/
    <hex-encoded-plane-id>.plane
  replays/
    <hex-encoded-replay-id>.replay
```

Run ids, checkpoint ids, artifact ids, task graph ids, agent pool ids, and
control plane/replay ids are hex-encoded before they are used as filesystem names.
Callers should treat paths as backend-owned implementation details and use
public query/load APIs instead of constructing paths manually.

## Format Versions

`XWORK_PERSISTENCE_FORMAT_VERSION` is currently `14`.

Snapshot/meta files use the current binary snapshot format header and version.
Task graph `.graph` files, agent pool `.pool` files, and control plane
`.plane` files, and replay `.replay` files use separate binary headers with
the same format version compatibility rules.
The loader behavior is:

- current or older supported version: load with compatibility defaults
- unknown newer version: return `XWORK_ERROR_UNSUPPORTED`
- corrupt or incomplete file: return `XWORK_ERROR_EXTERNAL_FAILURE`
- missing object: return `XWORK_ERROR_NOT_FOUND`

Event logs use a separate line-oriented header:

```text
#xwork-events\t1
```

Event log format version is currently `1`.

## Snapshot Files

Snapshot files are used for:

- `latest.snapshot`
- `checkpoints/<checkpoint-id>.snapshot`

The latest snapshot is the recovery boundary used by
`xwork_runtime_recover_run_from_persistence()`. It includes enough run state to
recover:

- run id, parent id, agent id, task id, instruction, profile ids, autonomy, state
- workspace ids
- session policy
- pending tool call
- last approval request and approval decision state
- last checkpoint metadata
- last event metadata
- artifact summaries and typed artifact metadata
- run steps needed for query history

Live OS process handles and interactive terminal session handles are not
rehydrated. Persisted process/terminal artifacts are audit/output records; host
integrations must rediscover or restart live sessions explicitly.

Format `13` adds raw replay entry payload retention to replay `.replay` files:
request JSON, response JSON, arguments JSON, and result JSON are stored next to
their hashes. Older replay files load with empty raw payload fields and remain
usable for hash comparison, but cannot synthesize host-tool output during
offline replay.

Typed filesystem snapshot/ref replay records reuse the v13 raw payload layout:
the replay entry key stores the ref id, request JSON stores the path, arguments
JSON stores caller metadata, and content hash stores the filesystem content
hash.

Format `12` adds remote artifact blob chunk summaries to control plane
`.plane` snapshots: task/assignment/worker/artifact ids, blob ref, content
hash, chunk index/count, offset, chunk byte size, raw chunk payload, and
final-chunk flag. Older control plane snapshots load with an empty remote blob
chunk list.

Format `11` adds remote stdout/stderr output chunk summaries to control plane
`.plane` snapshots: stream, chunk index, offset, byte count, final-chunk flag,
content hash, and chunk text. Older control plane snapshots load with empty
remote output chunk lists.

Format `10` adds handoff summaries to task graph `.graph` snapshots:
handoff id, source/target task ids, reason, state/status/message, artifact refs,
memory context refs, shared workspace refs, and shared-context/workspace policy
flags. Older graph snapshots load with an empty handoff list.

Format `9` adds replay `.replay` files for deterministic replay manifests,
entry hash cassettes, and replay results.

Format `8` adds remote protocol version and task error classification to
control plane `.plane` snapshots. Older v7 control plane snapshots load with
protocol version/current classification defaults.

Format `7` adds remote task artifact summary refs to control plane `.plane`
snapshots. Older v6 control plane snapshots load with empty remote task
artifact lists.

Format `6` adds control plane `.plane` snapshots for remote worker registry,
lease state, assignment queue, remote task results, and recovery orphaning of
in-flight remote assignments.

Format `5` adds task graph pause state and pause reason to `.graph` snapshots
so a paused scheduler can be persisted and resumed later. Older v4 graph
snapshots load with pause unset.

Format `4` adds run-level `agent id` and `task id` fields to run snapshots so
run index queries can locate child runs by graph agent/task ownership. Older v3
snapshots load with these fields unset.

## Replay Files

Replay files are stored as `replays/<replay-id>.replay` and use the `XWORKRP1`
binary header plus `XWORK_PERSISTENCE_FORMAT_VERSION`.

Format `14` adds task node max turns and timeout to task graph `.graph`
snapshots. Older task graph snapshots load those fields as zero and recover the
agent defaults from the pool snapshot when possible.

The v13 replay file payload stores:

- `xwork_replay_manifest`: manifest id, replay id, optional source run id,
  optional creation text, hash algorithm, and entry count
- `xwork_replay_entry_summary` list: sequence, entry kind, key, operation id,
  request/response/arguments/result raw payloads, request/response/arguments/
  result/content hashes, and status
- `xwork_replay_result`: status, recorded/replayed/divergence counts,
  diverged flag, and first divergence details

Format v9-v12 files store hashes and summaries rather than raw provider/tool
payloads. `xwork_file_persistence_load_replay_engine()` rebuilds an engine by
loading the stored raw payloads when present and the precomputed hashes into
the replay cassette.
`xwork_replay_engine_seek_checkpoint()` can then position a strict/audit replay
engine at the first entry after a checkpoint entry whose key matches the
checkpoint id.

Replay event logs use the public `xwork_replay_event_options` /
`xwork_replay_event_summary` schema documented in `docs/REPLAY.md`. The schema
is available for in-memory record/replay and event sequence comparison, but v10
`.replay` files do not persist replay event logs yet. Persisting replay event
logs requires a future format version bump.

## Event Log

`events.log` is append-oriented and stores run events in sequence order. It is
used for:

- event listing
- last event lookup
- step history reconstruction where applicable
- audit trail for approvals, checkpoints, retries, cancellation, failure, and
  completion

The backend tolerates older event-log versions where explicitly supported and
rejects unknown newer versions.

## Artifact Metadata

Artifacts are stored as `.meta` records under `artifacts/`.

The persisted artifact metadata includes:

- id, kind, name, mime, storage ref, summary
- output class and output role
- report class and report subject ref
- content text and content stats when present
- patch stats and patch apply/file summary JSON when present
- command text, command output text, exit code, and I/O stats when present
- sequence

Artifact content is stored inside the metadata record for the current backend.
External blob stores can be represented by `sStorageRef`, but the built-in file
backend does not manage a distributed artifact blob store.

## Agent Pool Snapshots

Agent pool snapshots are stored as `.pool` records under `agent_pools/`.

The persisted pool snapshot includes:

- pool id
- each agent's id, display name, description, role
- agent profile ids, autonomy, max turns, timeout, and retry budget

These records are sufficient to reload an agent pool snapshot and rebuild an
in-process `xwork_agent_pool` through `xwork_agent_pool_create_from_snapshot()`.

## Task Graph Snapshots

Task graph snapshots are stored as `.graph` records under `task_graphs/`.

The persisted graph snapshot includes:

- graph id, max concurrency, failure policy, cancel state/reason, pause state/reason
- aggregate task graph result counters
- each node's task id, agent id, run id, parent run id, instruction
- node profile ids, workspace ids, dependency task ids, autonomy and session policy
- node state, status, attempt count and retry budget
- handoff summaries, including artifact refs, memory context refs, shared
  workspace refs, state/status/message, and shared-context/workspace policy flags

These records are sufficient to reload a graph snapshot and rebuild an
in-process `xwork_task_graph` through `xwork_task_graph_create_from_snapshot()`.
`xwork_file_persistence_recover_task_graph()` reloads both the agent pool and
task graph snapshots, rebuilds the pool/graph pair, and prepares the graph for
continued execution. Live in-flight worker/thread handles are not rehydrated:
nodes recorded as `READY`, `RUNNING`, or `BLOCKED` are restored as `PENDING` so
the scheduler can execute them again. Terminal nodes remain terminal and can
serve as already-completed dependency inputs.

## Control Plane Snapshots

Control plane snapshots are stored as `.plane` records under
`control_planes/`.

The persisted control plane snapshot includes:

- plane id, transport kind, protocol version, default lease timeout, logical
  time, start state
- next assignment sequence
- worker registry, including id/display/endpoint, protocol version,
  capabilities, labels, lease timestamps, lifecycle state, and counters
- remote task queue, including task id, assignment id, worker id, protocol
  version, kind, state, host service, operation id, request JSON, required
  capability, attempts, assignment/completion time, timeout, status, retryable
  flag, output text, visible summary, error kind, error message, artifact
  summaries, and stdout/stderr output chunk summaries
- remote artifact blob chunks, including blob refs/content hashes/chunk
  metadata and raw chunk payload bytes queryable through
  `xwork_control_plane_list_artifact_blobs()`

These records are sufficient to reload a control plane snapshot and rebuild an
in-process `xwork_control_plane` through
`xwork_control_plane_create_from_snapshot()`.
`xwork_file_persistence_recover_control_plane()` reloads the snapshot and
reattaches recovered workers to the caller-provided runtime. Live worker
processes, network connections, and in-flight execution handles are not
rehydrated: tasks recorded as `ASSIGNED` or `RUNNING` are restored as
`ORPHANED` with `XWORK_ERROR_CANCELLED`; queued and terminal tasks keep their
recorded states so queued work can be claimed after recovery.

## Atomicity

The file backend writes snapshot and artifact metadata through a temp-file then
rename flow. A half-written temp file must not become the committed object. The
durable smoke coverage includes corrupt/newer-file handling so callers get a
stable `xwork_status` instead of partially loaded objects.

`events.log` is append-oriented. A corrupt event record should be treated as a
backend failure rather than silently producing a misleading recovery state.

## Query Contract

The public query APIs are the stable interface:

- `xwork_file_persistence_list_runs`
- `xwork_file_persistence_list_run_summaries`
- `xwork_file_persistence_list_run_index`
- `xwork_file_persistence_query_run_index`
- `xwork_file_persistence_query_run_steps`
- `xwork_file_persistence_store_task_graph_snapshot`
- `xwork_file_persistence_load_task_graph_snapshot`
- `xwork_file_persistence_store_agent_pool_snapshot`
- `xwork_file_persistence_load_agent_pool_snapshot`
- `xwork_file_persistence_recover_task_graph`
- `xwork_file_persistence_store_control_plane_snapshot`
- `xwork_file_persistence_load_control_plane_snapshot`
- `xwork_file_persistence_recover_control_plane`
- `xwork_file_persistence_list_events`
- `xwork_file_persistence_load_event`
- `xwork_file_persistence_load_last_event`
- `xwork_file_persistence_list_checkpoints`
- `xwork_file_persistence_load_checkpoint`
- `xwork_file_persistence_load_last_checkpoint`
- `xwork_file_persistence_list_artifacts`
- `xwork_file_persistence_list_artifact_summaries`
- `xwork_file_persistence_query_artifact_summaries`
- `xwork_file_persistence_find_artifact_by_name`

Run index queries support filtering and sorting over persisted summaries,
approval/checkpoint presence, artifact/event counts, and state. Artifact summary
queries support filtering by kind, output/report metadata, name/mime/storage
prefixes, exit code, and sequence windows.

Pagination cursors are backend-owned values exposed through the public list
objects. Callers should pass them back to the next query and should not parse or
persist them as a stable external format.

## Compatibility Rule

When changing persisted fields:

1. Bump `XWORK_PERSISTENCE_FORMAT_VERSION` for incompatible or newly serialized fields.
2. Keep old-version reads working where possible by filling defaults.
3. Reject unknown newer versions with `XWORK_ERROR_UNSUPPORTED`.
4. Add or update smoke fixtures for corrupt, older, newer, and pagination cases.
5. Update this document and `docs/COMPATIBILITY.md`.

Focused current-format and newer-version rejection coverage lives in
`tests/xwork_persistence_smoke.c`. Broader corrupt/legacy/future event-log
coverage lives in `tests/xwork_orchestrator_smoke.c`.
