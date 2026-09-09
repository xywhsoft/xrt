# Remote Worker API

The Remote Worker API provides public objects for control plane, worker registry, lease, assignment queue, remote task, artifact blob chunk, and output chunk. It is used to offload agent work to local or remote workers for execution, while retaining auditable and recoverable task status.

## Module boundaries

- `XWORK_REMOTE_TRANSPORT_IN_PROCESS` means the control plane and worker interact through in-memory APIs in the same process.
- `XWORK_REMOTE_TRANSPORT_HTTP_BOUNDARY` only defines decoded control plane boundaries; networking, authentication, tenant isolation, retries and blob streaming are implemented by the host.
- The control plane does not kill OS processes or restore live terminal/process handles; assigned/running tasks become orphaned during snapshot recovery.
- Control plane and workers do not provide concurrent mutation container semantics; start/stop/register/heartbeat/enqueue/claim/complete/fail/cancel/upload/query should be serialized by the caller.
- The current remote protocol version is `1`，corresponding to `XWORK_REMOTE_PROTOCOL_VERSION_CURRENT`。

## Ownership agreement

| Object | Ownership |
| --- | --- |
| `xwork_control_plane_create` | Return owned plane, use `xwork_control_plane_destroy` to release. |
| `xwork_control_plane_register_worker` | The worker is owned by plane and returns the borrowed pointer. |
| options/result/upload/chunk input | Copy the strings and arrays that need to be retained when calling the API; the runtime pointer is borrowed. |
| summary/list/snapshot output | The output structure has deep-copy contents, freed using matching `*_reset`. |
| assignment output | The output structure has deep-copy fields, released using `xwork_remote_task_assignment_reset`. |

## Typical process

```text
xwork_control_plane_options_init
xwork_control_plane_create
xwork_control_plane_start
xwork_worker_options_init
xwork_control_plane_register_worker
xwork_remote_task_options_init
xwork_control_plane_enqueue_task
xwork_control_plane_claim_task
xwork_control_plane_complete_task
xwork_control_plane_get_snapshot
xwork_control_plane_destroy
```

## Initialization and release API

### xwork_control_plane_options_init

Initialize control plane options.

**Function:**

Set control plane creation parameter default values.

**Function prototype:**

```c
XWORK_API void xwork_control_plane_options_init(xwork_control_plane_options *pOptions);
```

**parameter:**

- `pOptions`: options to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

The default transport is in-process and the protocol version is the current version. A non-empty `sPlaneId` must be set before creating a plane.

**Example code:**

```c
xwork_control_plane_options opts;
xwork_control_plane_options_init(&opts);
opts.sPlaneId = "plane-1";
opts.pRuntime = runtime;
```

**Related API:**

- `xwork_control_plane_create`

---

### xwork_worker_options_init

Initialize worker options.

**Function:**

Prepare worker registration parameters.

**Function prototype:**

```c
XWORK_API void xwork_worker_options_init(xwork_worker_options *pOptions);
```

**parameter:**

- `pOptions`: options to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

The default protocol version is the current version. `pRuntime` overrides the plane runtime; otherwise the worker uses the plane runtime.

**Example code:**

```c
xwork_worker_options opts;
xwork_worker_options_init(&opts);
opts.sWorkerId = "worker-1";
opts.pRuntime = runtime;
```

**Related API:**

- `xwork_control_plane_register_worker`

---

### xwork_worker_summary_init

Initialize worker summary.

**Function:**

Prepare worker query result structure.

**Function prototype:**

```c
XWORK_API void xwork_worker_summary_init(xwork_worker_summary *pSummary);
```

**parameter:**

- `pSummary`: summary to be initialized; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

The default state is `XWORK_WORKER_REGISTERED`.

**Example code:**

```c
xwork_worker_summary summary;
xwork_worker_summary_init(&summary);
```

**Related API:**

- `xwork_control_plane_list_workers`

---

### xwork_worker_summary_reset

Release worker summary.

**Function:**

Release deep-copy strings such as worker id, display name, endpoint, etc.

**Function prototype:**

```c
XWORK_API void xwork_worker_summary_reset(xwork_worker_summary *pSummary);
```

**parameter:**

- `pSummary`: summary to be released; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases internal resources but does not release the structure itself.

**Additional Note:**

Return to init state after calling.

**Example code:**

```c
xwork_worker_summary_reset(&summary);
```

**Related API:**

- `xwork_worker_summary_init`

---

### xwork_worker_summary_list_init

Initialize the worker summary list.

**Function:**

Prepare an empty list to receive worker registry query results.

**Function prototype:**

```c
XWORK_API void xwork_worker_summary_list_init(xwork_worker_summary_list *pList);
```

**parameter:**

- `pList`: List to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

It should be initialized before calling `xwork_control_plane_list_workers`.

**Example code:**

```c
xwork_worker_summary_list list;
xwork_worker_summary_list_init(&list);
```

**Related API:**

- `xwork_control_plane_list_workers`

---

### xwork_worker_summary_list_reset

Release the worker summary list.

**Function:**

Free all worker summary and arrays in the list.

**Function prototype:**

```c
XWORK_API void xwork_worker_summary_list_reset(xwork_worker_summary_list *pList);
```

**parameter:**

- `pList`: List to free; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases the contents owned by the list.

**Additional Note:**

The list can be reused after it is released.

**Example code:**

```c
xwork_worker_summary_list_reset(&list);
```

**Related API:**

- `xwork_worker_summary_reset`

---

### xwork_worker_snapshot_init

Initialize worker snapshot.

**Function:**

Prepare worker snapshot for restoring worker registry.

**Function prototype:**

```c
XWORK_API void xwork_worker_snapshot_init(xwork_worker_snapshot *pSnapshot);
```

**parameter:**

- `pSnapshot`: snapshot to be initialized; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

The default protocol version is the current version, and the default status is registered.

**Example code:**

```c
xwork_worker_snapshot snapshot;
xwork_worker_snapshot_init(&snapshot);
```

**Related API:**

- `xwork_control_plane_get_snapshot`

---

### xwork_worker_snapshot_reset

Release the worker snapshot.

**Function:**

Release the string, capability array and label array in the worker snapshot.

**Function prototype:**

```c
XWORK_API void xwork_worker_snapshot_reset(xwork_worker_snapshot *pSnapshot);
```

**parameter:**

- `pSnapshot`: snapshot to be released; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Release internal deep-copy resources.

**Additional Note:**

`xwork_control_plane_create_from_snapshot` does not take over snapshot ownership.

**Example code:**

```c
xwork_worker_snapshot_reset(&snapshot);
```

**Related API:**

- `xwork_worker_snapshot_init`

---

### xwork_worker_snapshot_list_init

Initialize the worker snapshot list.

**Function:**

Prepare an empty worker snapshot list.

**Function prototype:**

```c
XWORK_API void xwork_worker_snapshot_list_init(xwork_worker_snapshot_list *pList);
```

**parameter:**

- `pList`: List to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

This list is typically used as `xwork_control_plane_snapshot.tWorkers`.

**Example code:**

```c
xwork_worker_snapshot_list list;
xwork_worker_snapshot_list_init(&list);
```

**Related API:**

- `xwork_worker_snapshot_list_reset`

---

### xwork_worker_snapshot_list_reset

Release the worker snapshot list.

**Function:**

Free each worker snapshot and array in the list.

**Function prototype:**

```c
XWORK_API void xwork_worker_snapshot_list_reset(xwork_worker_snapshot_list *pList);
```

**parameter:**

- `pList`: List to free; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases the contents owned by the list.

**Additional Note:**

`xwork_control_plane_snapshot_reset` calls it indirectly.

**Example code:**

```c
xwork_worker_snapshot_list_reset(&list);
```

**Related API:**

- `xwork_control_plane_snapshot_reset`

---

### xwork_remote_task_options_init

Initialize remote task options.

**Function:**

Prepare a remote task that can be enqueued.

**Function prototype:**

```c
XWORK_API void xwork_remote_task_options_init(xwork_remote_task_options *pOptions);
```

**parameter:**

- `pOptions`: options to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

The default task kind is host tool, and the default host service is process. The task id and request JSON must be set before joining the queue; the host tool task must also set the operation id.

**Example code:**

```c
xwork_remote_task_options opts;
xwork_remote_task_options_init(&opts);
opts.sTaskId = "task-1";
opts.sOperationId = XWORK_HOST_PROCESS_EXEC;
opts.sRequestJson = "{\"cmd\":\"echo hi\"}";
```

**Related API:**

- `xwork_control_plane_enqueue_task`

---

### xwork_remote_task_summary_init

Initialize remote task summary.

**Function:**

Prepare the remote task query result structure.

**Function prototype:**

```c
XWORK_API void xwork_remote_task_summary_init(xwork_remote_task_summary *pSummary);
```

**parameter:**

- `pSummary`: summary to be initialized; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

The default task kind is host tool, and the default status is queued.

**Example code:**

```c
xwork_remote_task_summary summary;
xwork_remote_task_summary_init(&summary);
```

**Related API:**

- `xwork_control_plane_get_task_summary`

---

### xwork_remote_task_summary_reset

Release remote task summary.

**Function:**

Release the string, artifact summary array, and output chunk array in the task summary.

**Function prototype:**

```c
XWORK_API void xwork_remote_task_summary_reset(xwork_remote_task_summary *pSummary);
```

**parameter:**

- `pSummary`: summary to be released; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases the resources owned by summary internally.

**Additional Note:**

Applies to results of `get_task_summary` and `list_tasks` fills.

**Example code:**

```c
xwork_remote_task_summary_reset(&summary);
```

**Related API:**

- `xwork_remote_task_summary_init`

---

### xwork_remote_task_summary_list_init

Initialize the remote task summary list.

**Function:**

Prepare an empty task summary list.

**Function prototype:**

```c
XWORK_API void xwork_remote_task_summary_list_init(xwork_remote_task_summary_list *pList);
```

**parameter:**

- `pList`: List to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

It should be initialized before calling `xwork_control_plane_list_tasks`.

**Example code:**

```c
xwork_remote_task_summary_list list;
xwork_remote_task_summary_list_init(&list);
```

**Related API:**

- `xwork_control_plane_list_tasks`

---

### xwork_remote_task_summary_list_reset

Release the remote task summary list.

**Function:**

Release all task summaries and arrays in the list.

**Function prototype:**

```c
XWORK_API void xwork_remote_task_summary_list_reset(xwork_remote_task_summary_list *pList);
```

**parameter:**

- `pList`: List to free; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases the contents owned by the list.

**Additional Note:**

The list can be reused after it is released.

**Example code:**

```c
xwork_remote_task_summary_list_reset(&list);
```

**Related API:**

- `xwork_remote_task_summary_reset`

---

### xwork_remote_task_snapshot_init

Initialize remote task snapshot.

**Function:**

Prepare task snapshot for control plane persistence and recovery.

**Function prototype:**

```c
XWORK_API void xwork_remote_task_snapshot_init(xwork_remote_task_snapshot *pSnapshot);
```

**parameter:**

- `pSnapshot`: snapshot to be initialized; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

The default protocol version is the current version, and the default status is queued.

**Example code:**

```c
xwork_remote_task_snapshot snapshot;
xwork_remote_task_snapshot_init(&snapshot);
```

**Related API:**

- `xwork_control_plane_get_snapshot`

---

### xwork_remote_task_snapshot_reset

Release the remote task snapshot.

**Function:**

Release the string, artifact summary array and output chunk array in the task snapshot.

**Function prototype:**

```c
XWORK_API void xwork_remote_task_snapshot_reset(xwork_remote_task_snapshot *pSnapshot);
```

**parameter:**

- `pSnapshot`: snapshot to be released; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Release resources owned by the snapshot.

**Additional Note:**

The recovery API does not take over the snapshot.

**Example code:**

```c
xwork_remote_task_snapshot_reset(&snapshot);
```

**Related API:**

- `xwork_remote_task_snapshot_init`

---

### xwork_remote_task_snapshot_list_init

Initialize the remote task snapshot list.

**Function:**

Prepare an empty task snapshot list.

**Function prototype:**

```c
XWORK_API void xwork_remote_task_snapshot_list_init(xwork_remote_task_snapshot_list *pList);
```

**parameter:**

- `pList`: List to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

This list is typically used as `xwork_control_plane_snapshot.tTasks`.

**Example code:**

```c
xwork_remote_task_snapshot_list list;
xwork_remote_task_snapshot_list_init(&list);
```

**Related API:**

- `xwork_remote_task_snapshot_list_reset`

---

### xwork_remote_task_snapshot_list_reset

Release the remote task snapshot list.

**Function:**

Release all remote task snapshots and arrays in the list.

**Function prototype:**

```c
XWORK_API void xwork_remote_task_snapshot_list_reset(xwork_remote_task_snapshot_list *pList);
```

**parameter:**

- `pList`: List to free; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases the contents owned by the list.

**Additional Note:**

`xwork_control_plane_snapshot_reset` calls it indirectly.

**Example code:**

```c
xwork_remote_task_snapshot_list_reset(&list);
```

**Related API:**

- `xwork_control_plane_snapshot_reset`

---

### xwork_remote_task_assignment_init

Initialize remote task assignment.

**Function:**

Prepare the assignment output structure after the worker claim task.

**Function prototype:**

```c
XWORK_API void xwork_remote_task_assignment_init(xwork_remote_task_assignment *pAssignment);
```

**parameter:**

- `pAssignment`: The assignment to be initialized; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

It should be initialized before calling `xwork_control_plane_claim_task`.

**Example code:**

```c
xwork_remote_task_assignment assignment;
xwork_remote_task_assignment_init(&assignment);
```

**Related API:**

- `xwork_control_plane_claim_task`

---

### xwork_remote_task_assignment_reset

Release remote task assignment.

**Function:**

Release the task id, assignment id, worker id, request JSON and other strings in the assignment.

**Function prototype:**

```c
XWORK_API void xwork_remote_task_assignment_reset(xwork_remote_task_assignment *pAssignment);
```

**parameter:**

- `pAssignment`: The assignment to be released; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases resources owned internally by the assignment.

**Additional Note:**

The worker should still reset the assignment output after completing the task.

**Example code:**

```c
xwork_remote_task_assignment_reset(&assignment);
```

**Related API:**

- `xwork_remote_task_assignment_init`

---

### xwork_remote_task_result_init

Initialize remote task result.

**Function:**

Prepare the results submitted when the worker completes the task.

**Function prototype:**

```c
XWORK_API void xwork_remote_task_result_init(xwork_remote_task_result *pResult);
```

**parameter:**

- `pResult`: result to be initialized; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated; what needs to be preserved is copied when `complete_task` is called.

**Additional Note:**

The default status code is `XWORK_OK`, and the default protocol version is the current version.

**Example code:**

```c
xwork_remote_task_result result;
xwork_remote_task_result_init(&result);
result.sVisibleSummary = "done";
```

**Related API:**

- `xwork_control_plane_complete_task`

---

### xwork_remote_output_chunk_init

Initialize remote output chunk.

**Function:**

Prepare stdout/stderr text chunk upload request.

**Function prototype:**

```c
XWORK_API void xwork_remote_output_chunk_init(xwork_remote_output_chunk *pChunk);
```

**parameter:**

- `pChunk`: chunk to be initialized; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated; the control plane copies text and metadata when uploading.

**Additional Note:**

The default stream is stdout.

**Example code:**

```c
xwork_remote_output_chunk chunk;
xwork_remote_output_chunk_init(&chunk);
chunk.sTaskId = "task-1";
chunk.sWorkerId = "worker-1";
chunk.sText = "line\n";
```

**Related API:**

- `xwork_control_plane_upload_output_chunk`

---

### xwork_remote_output_chunk_summary_init

Initialize output chunk summary.

**Function:**

Prepare output chunk query/snapshot element.

**Function prototype:**

```c
XWORK_API void xwork_remote_output_chunk_summary_init(
    xwork_remote_output_chunk_summary *pSummary
);
```

**parameter:**

- `pSummary`: summary to be initialized; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

The default stream is stdout.

**Example code:**

```c
xwork_remote_output_chunk_summary summary;
xwork_remote_output_chunk_summary_init(&summary);
```

**Related API:**

- `xwork_remote_output_chunk_summary_reset`

---

### xwork_remote_output_chunk_summary_reset

Release output chunk summary.

**Function:**

Release the content hash and text content.

**Function prototype:**

```c
XWORK_API void xwork_remote_output_chunk_summary_reset(
    xwork_remote_output_chunk_summary *pSummary
);
```

**parameter:**

- `pSummary`: summary to be released; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Free the internal deep-copy string.

**Additional Note:**

This structure is also nested within the remote task summary/snapshot.

**Example code:**

```c
xwork_remote_output_chunk_summary_reset(&summary);
```

**Related API:**

- `xwork_remote_output_chunk_summary_init`

---

### xwork_remote_output_chunk_summary_list_init

Initialize the output chunk summary list.

**Function:**

Prepare an empty output chunk summary list.

**Function prototype:**

```c
XWORK_API void xwork_remote_output_chunk_summary_list_init(
    xwork_remote_output_chunk_summary_list *pList
);
```

**parameter:**

- `pList`: List to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

List elements usually come from task summary or snapshot.

**Example code:**

```c
xwork_remote_output_chunk_summary_list list;
xwork_remote_output_chunk_summary_list_init(&list);
```

**Related API:**

- `xwork_remote_output_chunk_summary_list_reset`

---

### xwork_remote_output_chunk_summary_list_reset

Release the output chunk summary list.

**Function:**

Free all output chunk summaries and arrays in the list.

**Function prototype:**

```c
XWORK_API void xwork_remote_output_chunk_summary_list_reset(
    xwork_remote_output_chunk_summary_list *pList
);
```

**parameter:**

- `pList`: List to free; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases the contents owned by the list.

**Additional Note:**

Can be reused after release.

**Example code:**

```c
xwork_remote_output_chunk_summary_list_reset(&list);
```

**Related API:**

- `xwork_remote_output_chunk_summary_reset`

---

### xwork_remote_blob_chunk_summary_init

Initialize artifact blob chunk summary.

**Function:**

Prepare artifact blob chunk query result elements.

**Function prototype:**

```c
XWORK_API void xwork_remote_blob_chunk_summary_init(
    xwork_remote_blob_chunk_summary *pSummary
);
```

**parameter:**

- `pSummary`: summary to be initialized; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

The blob chunk can save the binary data pointer and size; the data in the query result is held by summary.

**Example code:**

```c
xwork_remote_blob_chunk_summary summary;
xwork_remote_blob_chunk_summary_init(&summary);
```

**Related API:**

- `xwork_control_plane_list_artifact_blobs`

---

### xwork_remote_blob_chunk_summary_reset

Release artifact blob chunk summary.

**Function:**

Release task/assignment/worker/artifact/blob/hash string and chunk data copies.

**Function prototype:**

```c
XWORK_API void xwork_remote_blob_chunk_summary_reset(
    xwork_remote_blob_chunk_summary *pSummary
);
```

**parameter:**

- `pSummary`: summary to be released; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Release internal deep-copy resources.

**Additional Note:**

After calling summary returns to init state.

**Example code:**

```c
xwork_remote_blob_chunk_summary_reset(&summary);
```

**Related API:**

- `xwork_remote_blob_chunk_summary_init`

---

### xwork_remote_blob_chunk_summary_list_init

Initialize the artifact blob chunk summary list.

**Function:**

Prepare an empty blob chunk summary list.

**Function prototype:**

```c
XWORK_API void xwork_remote_blob_chunk_summary_list_init(
    xwork_remote_blob_chunk_summary_list *pList
);
```

**parameter:**

- `pList`: List to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

It should be initialized before calling `xwork_control_plane_list_artifact_blobs`.

**Example code:**

```c
xwork_remote_blob_chunk_summary_list list;
xwork_remote_blob_chunk_summary_list_init(&list);
```

**Related API:**

- `xwork_control_plane_list_artifact_blobs`

---

### xwork_remote_blob_chunk_summary_list_reset

Release the artifact blob chunk summary list.

**Function:**

Free all blob chunk summaries and arrays in the list.

**Function prototype:**

```c
XWORK_API void xwork_remote_blob_chunk_summary_list_reset(
    xwork_remote_blob_chunk_summary_list *pList
);
```

**parameter:**

- `pList`: List to free; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases the contents owned by the list.

**Additional Note:**

`xwork_control_plane_snapshot_reset` will release the blob chunk list in the snapshot.

**Example code:**

```c
xwork_remote_blob_chunk_summary_list_reset(&list);
```

**Related API:**

- `xwork_remote_blob_chunk_summary_reset`

---

### xwork_remote_artifact_upload_init

Initialize remote artifact upload.

**Function:**

Prepare artifact summary and blob chunk upload request.

**Function prototype:**

```c
XWORK_API void xwork_remote_artifact_upload_init(xwork_remote_artifact_upload *pUpload);
```

**parameter:**

- `pUpload`: upload to be initialized; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated; the control plane copies artifact summary and chunk data when uploading.

**Additional Note:**

Task id, worker id, and artifact summary must be set; when there is chunk data, `pChunkData` and `iChunkSize` must match.

**Example code:**

```c
xwork_remote_artifact_upload upload;
xwork_remote_artifact_upload_init(&upload);
upload.sTaskId = "task-1";
upload.sWorkerId = "worker-1";
upload.pArtifact = &artifactSummary;
```

**Related API:**

- `xwork_control_plane_upload_artifact`

---

### xwork_control_plane_snapshot_init

Initialize control plane snapshot.

**Function:**

Prepare a control plane snapshot to save worker, task and blob chunk status.

**Function prototype:**

```c
XWORK_API void xwork_control_plane_snapshot_init(xwork_control_plane_snapshot *pSnapshot);
```

**parameter:**

- `pSnapshot`: snapshot to be initialized; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

The default transport is in-process and the protocol version is the current version.

**Example code:**

```c
xwork_control_plane_snapshot snapshot;
xwork_control_plane_snapshot_init(&snapshot);
```

**Related API:**

- `xwork_control_plane_get_snapshot`

---

### xwork_control_plane_snapshot_reset

Release the control plane snapshot.

**Function:**

Release plane id, worker snapshot list, remote task snapshot list and blob chunk list.

**Function prototype:**

```c
XWORK_API void xwork_control_plane_snapshot_reset(xwork_control_plane_snapshot *pSnapshot);
```

**parameter:**

- `pSnapshot`: snapshot to be released; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Release resources owned by the snapshot.

**Additional Note:**

The recovery API does not take over the snapshot.

**Example code:**

```c
xwork_control_plane_snapshot_reset(&snapshot);
```

**Related API:**

- `xwork_control_plane_create_from_snapshot`

---

## Control Plane life cycle

### xwork_control_plane_create

Create a control plane.

**Function:**

Create a remote task control plane and save worker registry, task queue and upload data.

**Function prototype:**

```c
XWORK_API xwork_status xwork_control_plane_create(
    const xwork_control_plane_options *pOptions,
    xwork_control_plane **ppPlane
);
```

**parameter:**

- `pOptions`: Creation parameter; must contain non-empty `sPlaneId`.
- `ppPlane`: Output owned plane.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

After success, `*ppPlane` is owned by the caller and released with `xwork_control_plane_destroy`; the runtime is borrowed.

**Additional Note:**

protocol version must be supported; allowed capability allowlist will be copied.

**Example code:**

```c
xwork_control_plane *plane = NULL;
xwork_control_plane_options opts;
xwork_control_plane_options_init(&opts);
opts.sPlaneId = "plane-1";
opts.pRuntime = runtime;
xwork_control_plane_create(&opts, &plane);
```

**Related API:**

- `xwork_control_plane_destroy`
- `xwork_control_plane_start`

---

### xwork_control_plane_create_from_snapshot

Restore the control plane from snapshot.

**Function:**

Rebuild the worker registry, task queue, result, output chunk and blob chunk.

**Function prototype:**

```c
XWORK_API xwork_status xwork_control_plane_create_from_snapshot(
    const xwork_control_plane_options *pOptions,
    const xwork_control_plane_snapshot *pSnapshot,
    xwork_control_plane **ppPlane
);
```

**parameter:**

- `pOptions`: Optional recovery parameters; can cover runtime, plane id, transport, policy and other operating environments.
- `pSnapshot`: source snapshot.
- `ppPlane`: Output owned plane.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

After success, the plane is owned by the caller; the snapshot is not taken over.

**Additional Note:**

During recovery, the assigned/running task will be marked as `XWORK_REMOTE_TASK_ORPHANED`, the status code is canceled, and the orphaned error message will be logged.

**Example code:**

```c
xwork_control_plane *plane = NULL;
xwork_control_plane_create_from_snapshot(&opts, &snapshot, &plane);
```

**Related API:**

- `xwork_control_plane_get_snapshot`

---

### xwork_control_plane_destroy

Destroy the control plane.

**Function:**

Release plane, worker registry, task records, blob chunks, and capability allowlist.

**Function prototype:**

```c
XWORK_API void xwork_control_plane_destroy(xwork_control_plane *pPlane);
```

**parameter:**

- `pPlane`: The plane to be destroyed; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases resources owned by a plane; it does not release the borrowed runtime and does not kill external processes or network connections.

**Additional Note:**

The external transport should be stopped by the host before destruction.

**Example code:**

```c
xwork_control_plane_destroy(plane);
```

**Related API:**

- `xwork_control_plane_create`

---

### xwork_control_plane_start

Start the control plane scheduling state.

**Function:**

Allow workers to claim queued tasks.

**Function prototype:**

```c
XWORK_API xwork_status xwork_control_plane_start(xwork_control_plane *pPlane);
```

**parameter:**

- `pPlane`: Target plane.

**Return value:**

Returns `XWORK_OK` or `XWORK_ERROR_INVALID_ARGUMENT`.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

start only changes the memory scheduling flag and does not start the network server.

**Example code:**

```c
xwork_control_plane_start(plane);
```

**Related API:**

- `xwork_control_plane_stop`
- `xwork_control_plane_claim_task`

---

### xwork_control_plane_stop

Stop the control plane scheduling state.

**Function:**

Prevent new claims from continuing to obtain tasks.

**Function prototype:**

```c
XWORK_API xwork_status xwork_control_plane_stop(xwork_control_plane *pPlane);
```

**parameter:**

- `pPlane`: Target plane.

**Return value:**

Returns `XWORK_OK` or `XWORK_ERROR_INVALID_ARGUMENT`.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

stop does not cancel assigned/running tasks or stop OS processes.

**Example code:**

```c
xwork_control_plane_stop(plane);
```

**Related API:**

- `xwork_control_plane_start`

---

### xwork_control_plane_set_time

Set the control plane current time.

**Function:**

Update plane internal `nowMs` for lease, heartbeat and snapshot.

**Function prototype:**

```c
XWORK_API xwork_status xwork_control_plane_set_time(
    xwork_control_plane *pPlane,
    size_t iNowMs
);
```

**parameter:**

- `pPlane`: Target plane.
- `iNowMs`: Current time, in milliseconds, provided by the host.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

xwork does not read the system clock; the host is responsible for providing monotonic or business time.

**Example code:**

```c
xwork_control_plane_set_time(plane, nowMs);
```

**Related API:**

- `xwork_control_plane_worker_heartbeat`
- `xwork_control_plane_sweep_stale`

---

## Worker Registry

### xwork_control_plane_register_worker

Register worker.

**Function:**

Add the worker to the control plane registry and initialize the lease state.

**Function prototype:**

```c
XWORK_API xwork_status xwork_control_plane_register_worker(
    xwork_control_plane *pPlane,
    const xwork_worker_options *pOptions,
    xwork_worker **ppWorker
);
```

**parameter:**

- `pPlane`: Target plane.
- `pOptions`: worker parameter; must contain non-empty `sWorkerId`.
- `ppWorker`: Optional output borrowed worker.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

The worker is owned by plane; the return pointer cannot be released and will become invalid after unregister/destroy.

**Additional Note:**

The worker protocol version must match the plane; when the capability allowlist is enabled, the worker capability must be allowed.

**Example code:**

```c
xwork_worker *worker = NULL;
xwork_worker_options opts;
xwork_worker_options_init(&opts);
opts.sWorkerId = "worker-1";
xwork_control_plane_register_worker(plane, &opts, &worker);
```

**Related APIs:**

- `xwork_control_plane_worker_heartbeat`
- `xwork_control_plane_unregister_worker`

---

### xwork_control_plane_unregister_worker

Log out of the worker.

**Function:**

Mark the worker as unregistered.

**Function prototype:**

```c
XWORK_API xwork_status xwork_control_plane_unregister_worker(
    xwork_control_plane *pPlane,
    const char *sWorkerId
);
```

**parameter:**

- `pPlane`: Target plane.
- `sWorkerId`: worker id.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

The worker records in the plane are not released, only the status is updated.

**Additional Note:**

Logging off does not automatically cancel assigned tasks; the caller should handle this in conjunction with the sweep/cancel strategy.

**Example code:**

```c
xwork_control_plane_unregister_worker(plane, "worker-1");
```

**Related API:**

- `xwork_control_plane_sweep_stale`

---

### xwork_control_plane_worker_heartbeat

Update worker heartbeat.

**Function:**

Refresh the worker's last heartbeat, lease expires, and set the status to online.

**Function prototype:**

```c
XWORK_API xwork_status xwork_control_plane_worker_heartbeat(
    xwork_control_plane *pPlane,
    const char *sWorkerId,
    size_t iNowMs
);
```

**parameter:**

- `pPlane`: Target plane.
- `sWorkerId`: worker id.
- `iNowMs`: current time in milliseconds.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

unregistered worker cannot heartbeat.

**Example code:**

```c
xwork_control_plane_worker_heartbeat(plane, "worker-1", nowMs);
```

**Related API:**

- `xwork_control_plane_sweep_stale`

---

### xwork_control_plane_sweep_stale

Clean up expired workers.

**Function:**

Mark stale workers based on lease expiration time and convert their assigned/running tasks to orphaned.

**Function prototype:**

```c
XWORK_API xwork_status xwork_control_plane_sweep_stale(
    xwork_control_plane *pPlane,
    size_t iNowMs,
    size_t *piOrphanedCount
);
```

**parameter:**

- `pPlane`: Target plane.
- `iNowMs`: current time in milliseconds.
- `piOrphanedCount`: Optional output of the number of orphaned tasks this time.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

No transfer of title.

**Additional Note:**

This API is the core boundary for recovery and worker lease management; after orphaned it is up to the host to retry, cancel, or handle manually.

**Example code:**

```c
size_t orphaned = 0;
xwork_control_plane_sweep_stale(plane, nowMs, &orphaned);
```

**Related API:**

- `xwork_control_plane_worker_heartbeat`

---

### xwork_control_plane_list_workers

List workers.

**Function:**

Get a summary list of worker registries in the control plane.

**Function prototype:**

```c
XWORK_API xwork_status xwork_control_plane_list_workers(
    const xwork_control_plane *pPlane,
    xwork_worker_summary_list *pList
);
```

**parameter:**

- `pPlane`: source plane.
- `pList`: Output list; should be init before calling.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

The list has deep-copy contents, released with `xwork_worker_summary_list_reset`.

**Additional Note:**

The function resets the output list to its old contents.

**Example code:**

```c
xwork_worker_summary_list list;
xwork_worker_summary_list_init(&list);
xwork_control_plane_list_workers(plane, &list);
xwork_worker_summary_list_reset(&list);
```

**Related API:**

- `xwork_worker_summary_list_reset`

---

## Remote Task life cycle

### xwork_control_plane_enqueue_task

Enqueue remote task.

**Function:**

Put the remote task into the control plane queue and wait for worker claim.

**Function prototype:**

```c
XWORK_API xwork_status xwork_control_plane_enqueue_task(
    xwork_control_plane *pPlane,
    const xwork_remote_task_options *pOptions
);
```

**parameter:**

- `pPlane`: Target plane.
- `pOptions`: Task parameters; must contain task id and request JSON.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

plane copies the task field; `pUserData` is borrowed.

**Additional Note:**

Capability allowlist, task policy, and network policy checks are performed. `XWORK_REMOTE_TASK_PROCESS_EXEC` will be mapped to process host service.

**Example code:**

```c
xwork_remote_task_options task;
xwork_remote_task_options_init(&task);
task.sTaskId = "task-1";
task.sOperationId = XWORK_HOST_PROCESS_EXEC;
task.sRequestJson = "{}";
xwork_control_plane_enqueue_task(plane, &task);
```

**Related API:**

- `xwork_control_plane_claim_task`

---

### xwork_control_plane_claim_task

The worker receives the task.

**Function:**

Find queued tasks matching capability for online workers and generate assignments.

**Function prototype:**

```c
XWORK_API xwork_status xwork_control_plane_claim_task(
    xwork_control_plane *pPlane,
    const char *sWorkerId,
    xwork_remote_task_assignment *pAssignment
);
```

**parameter:**

- `pPlane`: Target plane.
- `sWorkerId`: worker id to receive the task.
- `pAssignment`: Output assignment; should be init before calling.

**Return value:**

Returns `XWORK_OK`, `XWORK_ERROR_NOT_FOUND`, or other error codes.

**Resource ownership:**

assignment has deep-copy fields, released with `xwork_remote_task_assignment_reset`.

**Additional Note:**

The plane must have been started; the worker must be online; if there is no task to receive, it will return not found.

**Example code:**

```c
xwork_remote_task_assignment assignment;
xwork_remote_task_assignment_init(&assignment);
xwork_control_plane_claim_task(plane, "worker-1", &assignment);
xwork_remote_task_assignment_reset(&assignment);
```

**Related API:**

- `xwork_control_plane_complete_task`

---

### xwork_control_plane_complete_task

Complete remote task.

**Function:**

Submit task results based on assignment id and mark the task as completed or failed.

**Function prototype:**

```c
XWORK_API xwork_status xwork_control_plane_complete_task(
    xwork_control_plane *pPlane,
    const char *sAssignmentId,
    const xwork_remote_task_result *pResult
);
```

**parameter:**

- `pPlane`: Target plane.
- `sAssignmentId`: assignment id.
- `pResult`: task result.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

plane copies output, summary, error, and artifact summary.

**Additional Note:**

The result protocol version must be consistent with the task; when `iStatus == XWORK_OK`, the task is completed, otherwise it fails.

**Example code:**

```c
xwork_remote_task_result result;
xwork_remote_task_result_init(&result);
result.sVisibleSummary = "completed";
xwork_control_plane_complete_task(plane, assignment.sAssignmentId, &result);
```

**Related API:**

- `xwork_control_plane_fail_task`

---

### xwork_control_plane_fail_task

Quickly mark remote task failure.

**Function:**

Constructs a standard remote task result with the error text and submits it as a failed result.

**Function prototype:**

```c
XWORK_API xwork_status xwork_control_plane_fail_task(
    xwork_control_plane *pPlane,
    const char *sAssignmentId,
    const char *sErrorText,
    bool bRetryable
);
```

**parameter:**

- `pPlane`: Target plane.
- `sAssignmentId`: assignment id.
- `sErrorText`: error text; can be `NULL`.
- `bRetryable`: Whether to recommend retrying.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

The error text will be copied to the task record.

**Additional Note:**

This API is equivalent to submitting the complete result of `XWORK_ERROR_EXTERNAL_FAILURE`.

**Example code:**

```c
xwork_control_plane_fail_task(plane, assignmentId, "tool failed", true);
```

**Related API:**

- `xwork_control_plane_complete_task`

---

### xwork_control_plane_cancel_task

Cancel remote task.

**Function:**

Mark non-final tasks as canceled by task id.

**Function prototype:**

```c
XWORK_API xwork_status xwork_control_plane_cancel_task(
    xwork_control_plane *pPlane,
    const char *sTaskId,
    const char *sReason
);
```

**parameter:**

- `pPlane`: Target plane.
- `sTaskId`: task id.
- `sReason`: Optional cancellation reason.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

plane copies the cancellation reason to the task error message.

**Additional Note:**

Cancellation does not stop external OS processes that have been started by the worker; the worker/transport needs to receive and perform the cancellation itself.

**Example code:**

```c
xwork_control_plane_cancel_task(plane, "task-1", "user cancelled");
```

**Related API:**

- `xwork_control_plane_get_task_summary`

---

### xwork_control_plane_execute_next_local

Execute the next task on the local worker runtime.

**Function:**

Encapsulate claim, call worker runtime host service, complete/fail local shortcut path.

**Function prototype:**

```c
XWORK_API xwork_status xwork_control_plane_execute_next_local(
    xwork_control_plane *pPlane,
    const char *sWorkerId,
    xwork_remote_task_assignment *pAssignment
);
```

**parameter:**

- `pPlane`: Target plane.
- `sWorkerId`: local worker id.
- `pAssignment`: Optional output of the actual assignment performed.

**Return value:**

Returns host service execution status or control plane error code.

**Resource ownership:**

The output assignment, if populated, is reset by the caller.

**Additional Note:**

The worker must be registered and own the runtime. This API is suitable for in-process worker mocking, testing, and stand-alone agent shells.

**Example code:**

```c
xwork_remote_task_assignment assignment;
xwork_remote_task_assignment_init(&assignment);
xwork_control_plane_execute_next_local(plane, "worker-1", &assignment);
xwork_remote_task_assignment_reset(&assignment);
```

**Related API:**

- `xwork_runtime_invoke_host_service`

---

### xwork_control_plane_get_task_summary

Query a single remote task.

**Function:**

Get task status, assignment, results, artifact and output chunk summary by task id.

**Function prototype:**

```c
XWORK_API xwork_status xwork_control_plane_get_task_summary(
    const xwork_control_plane *pPlane,
    const char *sTaskId,
    xwork_remote_task_summary *pSummary
);
```

**parameter:**

- `pPlane`: source plane.
- `sTaskId`: task id.
- `pSummary`: Output summary; should be init before calling.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

summary has deep-copy content, released with `xwork_remote_task_summary_reset`.

**Additional Note:**

The function resets the output summary to the old content.

**Example code:**

```c
xwork_remote_task_summary summary;
xwork_remote_task_summary_init(&summary);
xwork_control_plane_get_task_summary(plane, "task-1", &summary);
xwork_remote_task_summary_reset(&summary);
```

**Related API:**

- `xwork_control_plane_list_tasks`

---

### xwork_control_plane_list_tasks

List remote tasks.

**Function:**

Get a summary list of all tasks in the control plane.

**Function prototype:**

```c
XWORK_API xwork_status xwork_control_plane_list_tasks(
    const xwork_control_plane *pPlane,
    xwork_remote_task_summary_list *pList
);
```

**parameter:**

- `pPlane`: source plane.
- `pList`: Output list; should be init before calling.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

The list has deep-copy contents, released with `xwork_remote_task_summary_list_reset`.

**Additional Note:**

Used for UI queue panels, recovery diagnostics, and test assertions.

**Example code:**

```c
xwork_remote_task_summary_list list;
xwork_remote_task_summary_list_init(&list);
xwork_control_plane_list_tasks(plane, &list);
xwork_remote_task_summary_list_reset(&list);
```

**Related API:**

- `xwork_control_plane_get_task_summary`

---

## Artifact and Output upload

### xwork_control_plane_upload_artifact

Upload remote artifact.

**Function:**

Append or update artifact summary for remote task and save optional blob chunk.

**Function prototype:**

```c
XWORK_API xwork_status xwork_control_plane_upload_artifact(
    xwork_control_plane *pPlane,
    const xwork_remote_artifact_upload *pUpload
);
```

**parameter:**

- `pPlane`: Target plane.
- `pUpload`: Upload request; must include task id, worker id, and artifact summary.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

plane copies artifact summary, blob metadata, and chunk data.

**Additional Note:**

The task cannot be queued, canceled, or orphaned; the assignment id, if provided, must match the task's current assignment.

**Example code:**

```c
xwork_remote_artifact_upload upload;
xwork_remote_artifact_upload_init(&upload);
upload.sTaskId = "task-1";
upload.sWorkerId = "worker-1";
upload.pArtifact = &artifactSummary;
xwork_control_plane_upload_artifact(plane, &upload);
```

**Related API:**

- `xwork_control_plane_list_artifact_blobs`

---

### xwork_control_plane_upload_output_chunk

Upload remote output chunk.

**Function:**

Append stdout/stderr text chunk for remote task.

**Function prototype:**

```c
XWORK_API xwork_status xwork_control_plane_upload_output_chunk(
    xwork_control_plane *pPlane,
    const xwork_remote_output_chunk *pChunk
);
```

**parameter:**

- `pPlane`: Target plane.
- `pChunk`: Output chunk; must contain task id, worker id and text.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

plane copies text, hash and chunk metadata.

**Additional Note:**

Task cannot be queued, canceled, or orphaned; assignment id, if provided, must match.

**Example code:**

```c
xwork_remote_output_chunk chunk;
xwork_remote_output_chunk_init(&chunk);
chunk.sTaskId = "task-1";
chunk.sWorkerId = "worker-1";
chunk.sText = "stdout line\n";
xwork_control_plane_upload_output_chunk(plane, &chunk);
```

**Related API:**

- `xwork_remote_output_chunk_init`

---

### xwork_control_plane_list_artifact_blobs

List artifact blob chunks.

**Function:**

Query uploaded blob chunks by task id and optional artifact id.

**Function prototype:**

```c
XWORK_API xwork_status xwork_control_plane_list_artifact_blobs(
    const xwork_control_plane *pPlane,
    const char *sTaskId,
    const char *sArtifactId,
    xwork_remote_blob_chunk_summary_list *pList
);
```

**parameter:**

- `pPlane`: source plane.
- `sTaskId`: task id.
- `sArtifactId`: Optional artifact id; when empty, returns all blob chunks for this task.
- `pList`: output list; should be init before calling.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

The list has deep-copy contents, released with `xwork_remote_blob_chunk_summary_list_reset`.

**Additional Note:**

The API can be used to restore artifact blobs, debug upload sequences, or build download responses.

**Example code:**

```c
xwork_remote_blob_chunk_summary_list list;
xwork_remote_blob_chunk_summary_list_init(&list);
xwork_control_plane_list_artifact_blobs(plane, "task-1", NULL, &list);
xwork_remote_blob_chunk_summary_list_reset(&list);
```

**Related API:**

- `xwork_control_plane_upload_artifact`

---

## Snapshot

### xwork_control_plane_get_snapshot

Get control plane snapshot.

**Function:**

Deep copies the control plane's worker registry, task status, and blob chunks.

**Function prototype:**

```c
XWORK_API xwork_status xwork_control_plane_get_snapshot(
    const xwork_control_plane *pPlane,
    xwork_control_plane_snapshot *pSnapshot
);
```

**parameter:**

- `pPlane`: source plane.
- `pSnapshot`: output snapshot; should be init before calling.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

The snapshot has deep-copy content and is released with `xwork_control_plane_snapshot_reset`.

**Additional Notes:**

snapshot does not contain a live network connection, thread, process, or terminal handle.

**Example code:**

```c
xwork_control_plane_snapshot snapshot;
xwork_control_plane_snapshot_init(&snapshot);
xwork_control_plane_get_snapshot(plane, &snapshot);
xwork_control_plane_snapshot_reset(&snapshot);
```

**Related API:**

- `xwork_control_plane_create_from_snapshot`

---

## Related documents

- [Host Tools API](api-host-tools.md)
- [Policy / Approval API](api-policy-approval.md)
- [Remote Worker and Control Plane](../guide/remote-worker-intro.md)
- [Remote Worker Agent Example](../case/remote-worker-agent.md)
- [Internal remote worker contract](../../dev/docs/REMOTE_WORKER.md)
