# Remote Worker Agent Example

> Corresponding source code: `examples/remote_worker_agent.c`

This example shows the minimal closed loop of xwork remote worker/control plane.

## Problem solved

Agent tasks may need to be executed on an isolated process, a remote machine, or a dedicated worker. xwork provides task allocation, leases, results, artifact chunks and recovery objects, but is not bound to specific network service implementations.

## process

```text
create runtime/local host/file persistence/control plane
register worker with process.exec capability
enqueue remote task
worker claims assignment
worker executes local process.exec
complete task with result summary
persist control plane snapshot
recover and orphan in-flight assignment
continue queued work
```

## Key points

- The control plane manages workers, heartbeats, leases and assignments.
- The host product is responsible for network layers such as socket/auth/retry/blob streaming.
- Artifact and output chunk can be queried independently, suitable for large output or remote transmission.
- In-flight assignments will be marked as orphaned when restored to avoid being mistakenly thought that they are still being executed.

## State flow

```text
worker register
  -> heartbeat online
task queued
  -> assignment claimed
  -> worker executes process.exec
  -> task completed / failed / cancelled
snapshot persisted
  -> recovery marks in-flight assignment orphaned
```

## Key API

| API | Function |
| --- | --- |
| `xwork_control_plane_create()` | Create a control plane. |
| `xwork_control_plane_start()` | Start plane. |
| `xwork_control_plane_register_worker()` | Register workers and capabilities. |
| `xwork_control_plane_enqueue_task()` | Submit remote task. |
| `xwork_control_plane_claim_task()` | worker claim assignment. |
| `xwork_control_plane_execute_next_local()` | Local worker quick execution. |
| `xwork_control_plane_complete_task()` | Mission accomplished. |
| `xwork_control_plane_upload_artifact()` | Upload artifact summary/blob chunk. |
| `xwork_control_plane_upload_output_chunk()` | Upload stdout/stderr chunk. |
| `xwork_file_persistence_recover_control_plane()` | Restore plane from snapshot. |

## Artifact / output chunk

- artifact upload for structured artifacts, blob refs, content hashes and chunk payloads.
- output chunk is used for stdout/stderr or terminal output slices.
- The snapshot will save the chunk summary, which can still be queried after restoration.
- Base64, retries and streaming at the network layer are handled by the host transport.

## Suitable for expansion

- Access to real HTTP/WebSocket/gRPC transport.
- Added worker auth and project/tenant isolation.
- Use blob chunk API to transfer large file products.
