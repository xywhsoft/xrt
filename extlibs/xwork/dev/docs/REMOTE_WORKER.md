# Remote Worker / Control Plane Contract

This document defines the P3 in-process remote worker/control-plane contract.
It is the stable boundary for callers that build distributed or agent-worker
systems on top of `xwork`.

## Ownership

- `xwork_control_plane_create()` returns an owned `xwork_control_plane *`.
  Release it with `xwork_control_plane_destroy()`.
- `xwork_control_plane_create_from_snapshot()` creates a new owned plane from a
  deep-copied snapshot. It does not retain the snapshot object.
- `xwork_control_plane_register_worker()` stores a control-plane owned worker
  record. The optional returned `xwork_worker *` is borrowed and remains valid
  only until the worker is unregistered or the plane is destroyed.
- `xwork_control_plane_options` and `xwork_worker_options` are copied at create
  or register time for strings, capability lists, and label lists. Runtime
  pointers are borrowed and must outlive the plane or worker that uses them.
- `xwork_remote_task_options`, `xwork_remote_task_result`,
  `xwork_remote_artifact_upload`, and `xwork_remote_output_chunk` inputs are
  copied during the call. The caller can release request/result buffers after
  the API returns.
- Artifact upload chunk payloads are copied into plane-owned blob chunk
  storage when `pChunkData`/`iChunkSize` are provided. Query returned
  `xwork_remote_blob_chunk_summary_list` objects own copied payload bytes and
  must be reset by the caller.
- Query and snapshot outputs allocate owned strings/lists inside the output
  struct. Call the matching `*_reset()` function before reuse or release.

## Thread Safety

- `xwork_control_plane` and `xwork_worker` are not internally synchronized for
  concurrent mutation.
- Serialize calls that mutate a plane: start/stop, time update, worker
  register/unregister/heartbeat, stale sweep, enqueue, claim, complete, fail,
  cancel, upload artifact, and upload output chunk.
- Read-only summary/snapshot queries should also be externally serialized
  against concurrent mutation if callers need a coherent view. This includes
  artifact blob chunk queries.
- `xwork_control_plane_execute_next_local()` claims a task, invokes the
  worker runtime host service, and completes the assignment in one synchronous
  call; do not concurrently mutate the same plane or worker runtime while it
  is executing.
- A worker runtime is borrowed. Its host services, workspace policy, and
  process/terminal state follow the normal `xwork_runtime` thread-safety
  rules.

## Shutdown

- `xwork_control_plane_stop()` marks the plane as stopped and prevents future
  task enqueue/claim paths that require a started plane. It does not kill local
  OS processes or interactive terminal sessions already owned by a borrowed
  worker runtime.
- `xwork_control_plane_destroy()` releases plane-owned workers, tasks,
  assignments, artifacts, artifact blob chunks, output chunks, snapshots, and
  copied strings. It does not destroy borrowed runtimes.
- Callers that use local host process or terminal services must stop or reset
  those host sessions through the owning runtime/host layer before destroying
  the runtime if live process cleanup is required.
- In-flight assignments are durable state, not live execution handles. Recovery
  marks assigned/running tasks as orphaned so callers can decide whether to
  retry, cancel, or inspect the old worker lease.

## Transport Boundary

- `XWORK_REMOTE_TRANSPORT_IN_PROCESS` is implemented. The control plane and
  worker share process memory, and authentication is represented by possession
  of the `xwork_control_plane *`.
- `XWORK_REMOTE_TRANSPORT_HTTP_BOUNDARY` is also accepted by the control plane.
  It represents a decoded HTTP JSON transport: callers own socket serving,
  worker authentication, request signing or mTLS, replay protection,
  tenant/project isolation, HTTP retry, and blob/data-plane streaming, then pass
  decoded messages into the same control-plane APIs. The plane snapshots retain
  the HTTP transport kind for recovery/audit.
- Protocol version must match `XWORK_REMOTE_PROTOCOL_VERSION_CURRENT`.
  Unknown future versions are rejected.

## Wire JSON Schema

The HTTP transport boundary should carry UTF-8 JSON objects. Unknown fields are
ignored for forward compatibility. Required fields are listed explicitly below.
All ids are case-sensitive strings. All counters/timestamps use unsigned JSON
numbers. `protocol_version` must equal `XWORK_REMOTE_PROTOCOL_VERSION_CURRENT`.

Enum strings:

- `transport`: `in_process`, `http`
- `worker_state`: `registered`, `online`, `stale`, `offline`, `unregistered`
- `task_kind`: `host_tool`, `process_exec`
- `task_state`: `queued`, `assigned`, `running`, `completed`, `failed`, `cancelled`, `orphaned`
- `host_service`: `filesystem`, `process`, `vcs`, `network`, `diagnostics`, `editor`
- `output_stream`: `stdout`, `stderr`

Register request, worker to control plane:

```json
{
  "type": "worker.register",
  "protocol_version": 1,
  "worker_id": "local-worker",
  "display_name": "Local Worker",
  "endpoint": "http://127.0.0.1:9001",
  "capabilities": ["process.exec", "filesystem.read_text"],
  "labels": ["os=windows", "arch=x64"],
  "lease_timeout_ms": 30000
}
```

Register response, control plane to worker:

```json
{
  "type": "worker.registered",
  "protocol_version": 1,
  "plane_id": "project-plane",
  "worker_id": "local-worker",
  "state": "online",
  "lease_expires_ms": 123456
}
```

Heartbeat request:

```json
{
  "type": "worker.heartbeat",
  "protocol_version": 1,
  "worker_id": "local-worker",
  "now_ms": 123000
}
```

Heartbeat response:

```json
{
  "type": "worker.heartbeat_ack",
  "protocol_version": 1,
  "worker_id": "local-worker",
  "state": "online",
  "lease_expires_ms": 153000
}
```

Assignment message, control plane to worker:

```json
{
  "type": "task.assignment",
  "protocol_version": 1,
  "task_id": "remote-process",
  "assignment_id": "remote-process:assignment:1",
  "worker_id": "local-worker",
  "task_kind": "host_tool",
  "host_service": "process",
  "operation_id": "exec",
  "required_capability": "process.exec",
  "request_json": "{\"command\":\"echo hello\"}",
  "attempt_count": 1,
  "retryable": false
}
```

Result upload, worker to control plane:

```json
{
  "type": "task.result",
  "protocol_version": 1,
  "task_id": "remote-process",
  "assignment_id": "remote-process:assignment:1",
  "worker_id": "local-worker",
  "status": "ok",
  "output_text": "{\"ok\":true}",
  "visible_summary": "process.exec ok",
  "error_kind": "",
  "error_message": "",
  "retryable": false,
  "artifacts": [
    {
      "artifact_id": "remote-diagnostics-artifact",
      "kind": "report",
      "output_class": "json",
      "output_role": "diagnostics",
      "report_class": "diagnostics",
      "report_subject_ref": "remote-task://remote-process",
      "name": "remote-diagnostics.json",
      "mime_type": "application/json",
      "storage_ref": "remote://worker/local-worker/artifacts/remote-diagnostics-artifact",
      "summary": "remote diagnostics",
      "sequence": 1
    }
  ]
}
```

Failure result uses the same message with `status` set to a stable
`xwork_status_cstr()` value, `error_kind` set to a machine-readable category,
`error_message` set to the redacted human detail, and `retryable` indicating
whether the control plane may requeue.

Artifact chunk upload, worker to control plane:

```json
{
  "type": "artifact.upload",
  "protocol_version": 1,
  "task_id": "remote-process",
  "assignment_id": "remote-process:assignment:1",
  "worker_id": "local-worker",
  "artifact": {
    "artifact_id": "remote-uploaded-log",
    "kind": "output",
    "output_class": "text",
    "output_role": "remote.artifact.upload",
    "name": "remote-uploaded.log",
    "mime_type": "text/plain",
    "storage_ref": "remote://worker/local-worker/artifacts/uploaded-log",
    "summary": "remote uploaded log",
    "sequence": 2
  },
  "blob_ref": "remote://worker/local-worker/artifacts/uploaded-log",
  "content_hash": "sha256:...",
  "chunk_index": 0,
  "chunk_count": 1,
  "offset_bytes": 0,
  "chunk_base64": "bG9nCg==",
  "chunk_size": 4,
  "final_chunk": true
}
```

At the C API boundary, `chunk_base64` is decoded by the transport layer and
passed as `xwork_remote_artifact_upload::pChunkData` plus `iChunkSize`.
`xwork_control_plane_upload_artifact()` upserts both the artifact summary and
the plane-owned blob chunk. Callers can retrieve copied chunks with
`xwork_control_plane_list_artifact_blobs()`, and control-plane persistence
stores those chunks in format v12 snapshots.

Output chunk upload, worker to control plane:

```json
{
  "type": "task.output_chunk",
  "protocol_version": 1,
  "task_id": "remote-process",
  "assignment_id": "remote-process:assignment:1",
  "worker_id": "local-worker",
  "stream": "stdout",
  "chunk_index": 0,
  "offset_bytes": 0,
  "text": "hello\n",
  "byte_count": 6,
  "content_hash": "sha256:...",
  "final_chunk": true
}
```

Error envelope for transport-level failures:

```json
{
  "type": "error",
  "protocol_version": 1,
  "status": "unsupported",
  "error_kind": "protocol_version",
  "error_message": "unsupported protocol_version",
  "retryable": false
}
```

HTTP endpoints:

- `POST /v1/workers/register` carries `worker.register`.
- `POST /v1/workers/{worker_id}/heartbeat` carries `worker.heartbeat`.
- `POST /v1/workers/{worker_id}/claim` returns `task.assignment` or `404`.
- `POST /v1/tasks/{task_id}/result` carries `task.result`.
- `POST /v1/tasks/{task_id}/artifacts` carries `artifact.upload`.
- `POST /v1/tasks/{task_id}/output` carries `task.output_chunk`.

## Execution Boundary

- A remote task is scheduled by required capability. Capability allowlists are
  enforced at control-plane enqueue/register boundaries when configured.
- Process and filesystem tasks execute through the worker runtime host
  services and therefore inherit command policy, workspace root enforcement,
  network policy, approval mode, and secret redaction behavior.
- Remote terminal support is an in-process host-tool bridge for
  `process.start_terminal`, `process.list_terminals`, `process.terminal_read`,
  `process.terminal_write`, `process.terminal_resize`, and
  `process.terminal_stop` when the worker advertises the matching capabilities
  and `xrtProcessTerminalSupported()` is true.
- Live terminal sessions and OS process handles are not serialized into
  control-plane snapshots. Persisted task output and artifacts are audit data;
  callers must rediscover or restart live sessions after process restart.

## Persistence And Recovery

- Control-plane snapshots deep-copy worker registry, task queue/state,
  artifact summaries, output chunks, protocol version, and lease timestamps.
- File persistence stores snapshots in the current persistence format. Format
  v11 includes remote stdout/stderr output chunks.
- Recovery preserves completed/failed/cancelled/orphaned task state, keeps
  queued tasks resumable, and degrades assigned/running work to orphaned
  because no live worker thread/process handle is rehydrated.
- Artifact upload currently persists artifact summaries/refs and upload
  metadata. A production remote data plane still needs a real blob store and
  transport chunk transfer.
