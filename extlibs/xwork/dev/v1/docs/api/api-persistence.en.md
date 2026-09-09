# Persistence API

> Status: Chinese function-by-function reference, waiting for manual review.

The Persistence API is responsible for saving run, event, checkpoint, artifact, agent pool, task graph, remote control plane and replay cassette to the durable backend.

## Module positioning

Persistence provides xwork with local durable agent run capabilities. The built-in file backend is not a distributed database, nor a multi-writer store; if you need a remote DB or object store, you should implement a custom `xwork_persistence_backend`.

## This page covers the statement

| Category | Statement |
| --- | --- |
| Structure | `xwork_persistence_backend`, `xwork_file_persistence_options`, `xwork_file_persistence` |
| Function | All `### xwork_*` sections on this page |

## Format version

Currently `XWORK_PERSISTENCE_FORMAT_VERSION` is `14`.

Read rules:

- Current or older supported versions: loaded by compatible defaults.
- Unknown update version: Return `XWORK_ERROR_UNSUPPORTED`.
- Corrupt or incomplete file: Returns `XWORK_ERROR_EXTERNAL_FAILURE`.
- Object does not exist: return `XWORK_ERROR_NOT_FOUND`.

## Ownership Rules

- `pPersistenceBackend` in runtime options is copied by value.
- backend callback function pointer and `pUserData` are borrowed and must override the runtime lifetime.
- `xwork_file_persistence_configure_backend` will copy the root path to the store and configure the backend callback table to point to the store.
- All list/load/query output structures are owned by the caller; the corresponding reset must be called after filling.
- The pool/graph/control plane/run returned by the recover API is an owned object, and the caller is responsible for destroying it.

## Common error codes

- `XWORK_ERROR_INVALID_ARGUMENT`: Invalid store, backend, runtime, run id, object id, or output structure.
- `XWORK_ERROR_NOT_FOUND`: Object does not exist.
- `XWORK_ERROR_UNSUPPORTED`: Format version updated or capability not supported.
- `XWORK_ERROR_NO_MEMORY`: Allocation or deep-copy failed.
- `XWORK_ERROR_EXTERNAL_FAILURE`: File I/O, corrupt record, or backend callback failed.

## General example

```c
#include "xwork.h"

int configure_store(void) {
    xwork_file_persistence_options options;
    xwork_file_persistence store;
    xwork_persistence_backend backend;

    xwork_file_persistence_options_init(&options);
    xwork_file_persistence_init(&store);
    xwork_persistence_backend_init(&backend);

    options.sRootPath = ".xwork_store";
    if (xwork_file_persistence_configure_backend(&store, &options, &backend) != XWORK_OK) {
        return 1;
    }

    xwork_file_persistence_reset(&store);
    return 0;
}
```

---

### xwork_persistence_backend_init

Initialize the persistence backend callback table.

**Function:**

Clear the callback table before creating a custom backend or receiving a file backend configuration.

**Function prototype:**

```c
XWORK_API void xwork_persistence_backend_init(xwork_persistence_backend *pBackend);
```

**parameter:**

- `pBackend`: Output parameter. Can be `NULL`; cleared if not `NULL`.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. callback and `pUserData` are managed by the caller.

**Additional Note:**

- The runtime will copy the backend table by value.

**Example code:**

```c
xwork_persistence_backend backend;
xwork_persistence_backend_init(&backend);
```

**Related API:**

- `xwork_file_persistence_configure_backend`

---

### xwork_file_persistence_options_init

Initialize file persistence options.

**Function:**

Prepare to configure the built-in file backend.

**Function prototype:**

```c
XWORK_API void xwork_file_persistence_options_init(
    xwork_file_persistence_options *pOptions
);
```

**parameter:**

- `pOptions`: Output parameter. Can be `NULL`; cleared if not `NULL`.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. `sRootPath` is borrowed from the caller and copied during configure.

**Additional Note:**

- `sRootPath` is a required field to configure the file backend.

**Example code:**

```c
xwork_file_persistence_options options;
xwork_file_persistence_options_init(&options);
options.sRootPath = ".xwork_store";
```

**Related API:**

- `xwork_file_persistence_configure_backend`

---

### xwork_file_persistence_init

Initialize file persistence store.

**Function:**

Prepare an `xwork_file_persistence` structure for configuring the file backend.

**Function prototype:**

```c
XWORK_API void xwork_file_persistence_init(xwork_file_persistence *pStore);
```

**parameter:**

- `pStore`: Output parameter. Can be `NULL`; cleared if not `NULL`.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. After configure the store has a copy of the root path.

**Additional Note:**

- Call `xwork_file_persistence_reset` after use is complete.

**Example code:**

```c
xwork_file_persistence store;
xwork_file_persistence_init(&store);
```

**Related API:**

- `xwork_file_persistence_reset`

---

### xwork_file_persistence_reset

Release and reset the file persistence store.

**Function:**

Release the root path held by the file backend store and return to the init state.

**Function prototype:**

```c
XWORK_API void xwork_file_persistence_reset(xwork_file_persistence *pStore);
```

**parameter:**

- `pStore`: input/output parameters. Can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Release the copy of root path owned by the store.

**Additional Note:**

- Persistence data on disk will not be deleted.

**Example code:**

```c
xwork_file_persistence_reset(&store);
```

**Related API:**

- `xwork_file_persistence_init`

---

### xwork_file_persistence_configure_backend

Configure the built-in file backend.

**Function:**

Bind the file store to the root path and populate the `xwork_persistence_backend` callback table for use by the runtime.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_configure_backend(
    xwork_file_persistence *pStore,
    const xwork_file_persistence_options *pOptions,
    xwork_persistence_backend *pBackend
);
```

**parameter:**

- `pStore`: input/output parameters. Must not be `NULL`.
- `pOptions`: input parameters. Must be other than `NULL`, and `sRootPath` must be a non-empty string.
- `pBackend`: Output parameter. Must not be `NULL`. Receive callback table on success.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

The store has a copy of the root path; the backend callback table borrows the store as user data, and the store must cover the runtime lifetime.

**Additional Note:**

- The function creates the necessary directories.
- Reconfiguration will reset store/backend first.

**Example code:**

```c
xwork_file_persistence_configure_backend(&store, &options, &backend);
```

**Related API:**

- `xwork_runtime_create`

---

### xwork_file_persistence_list_runs

List persistent run ids.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_list_runs(
    const xwork_file_persistence *pStore,
    xwork_string_list *pList
);
```

**Function:**

Scan the file backend for saved runs.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `pList`: Output parameter. Must be other than `NULL`, receives a list of owned strings.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

The caller must free the list with `xwork_string_list_reset`.

**Additional Note:**

- What is returned is the run id, and the run content is not loaded.

**Example code:**

```c
xwork_string_list list;
xwork_string_list_init(&list);
xwork_file_persistence_list_runs(&store, &list);
xwork_string_list_reset(&list);
```

**Related API:**

- `xwork_runtime_list_persisted_runs`

---

### xwork_file_persistence_list_run_summaries

List all run summaries.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_list_run_summaries(
    const xwork_file_persistence *pStore,
    xwork_run_summary_list *pList
);
```

**Function:**

Load the summary of each run in the file backend.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `pList`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

The caller must `xwork_run_summary_list_reset`.

**Additional Note:**

- for run history UI.

**Example code:**

```c
xwork_run_summary_list list;
xwork_run_summary_list_init(&list);
xwork_file_persistence_list_run_summaries(&store, &list);
xwork_run_summary_list_reset(&list);
```

**Related API:**

- `xwork_runtime_list_persisted_run_summaries`

---

### xwork_file_persistence_list_run_index

List run index.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_list_run_index(
    const xwork_file_persistence *pStore,
    xwork_run_index_list *pList
);
```

**Function:**

Returns the run index containing summary and last objects.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `pList`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

The caller must `xwork_run_index_list_reset`.

**Additional Note:**

- Equivalent to run index query without query condition.

**Example code:**

```c
xwork_run_index_list list;
xwork_run_index_list_init(&list);
xwork_file_persistence_list_run_index(&store, &list);
xwork_run_index_list_reset(&list);
```

**Related API:**

- `xwork_file_persistence_query_run_index`

---

### xwork_file_persistence_query_run_index

Query run index by condition.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_query_run_index(
    const xwork_file_persistence *pStore,
    const xwork_run_index_query *pQuery,
    xwork_run_index_list *pList
);
```

**Function:**

Query run index based on run state, autonomy, last event/checkpoint/approval and other conditions.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `pQuery`: input parameters. Can be `NULL`.
- `pList`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

The caller must reset the output list.

**Additional Note:**

- The query string field is borrowed.

**Example code:**

```c
xwork_run_index_query query;
xwork_run_index_query_init(&query);
```

**Related API:**

- `xwork_run_index_query_init`

---

### xwork_file_persistence_list_checkpoints

List the checkpoint id of the run.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_list_checkpoints(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_string_list *pList
);
```

**Function:**

Scan the checkpoint directory of the specified run.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `sRunId`: input parameters. Must be a non-empty run id.
- `pList`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

The caller must `xwork_string_list_reset`.

**Additional Note:**

- Only returns the id and does not load the checkpoint content.

**Example code:**

```c
xwork_file_persistence_list_checkpoints(&store, "run-1", &list);
```

**Related API:**

- `xwork_file_persistence_load_checkpoint`

---

### xwork_file_persistence_list_events

List the event id of run.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_list_events(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_string_list *pList
);
```

**Function:**

Lists the event ids that can be loaded in the event log of the specified run.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `pList`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

List of callers reset.

**Additional Note:**

- Used for audit history traversal.

**Example code:**

```c
xwork_file_persistence_list_events(&store, "run-1", &list);
```

**Related API:**

- `xwork_file_persistence_load_event`

---

### xwork_file_persistence_list_artifacts

List the artifact ids for the run.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_list_artifacts(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_string_list *pList
);
```

**Function:**

Lists saved artifacts for the specified run.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `pList`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

List of callers reset.

**Additional Note:**

- Only returns the id and does not load the artifact content.

**Example code:**

```c
xwork_file_persistence_list_artifacts(&store, "run-1", &list);
```

**Related API:**

- `xwork_file_persistence_load_artifact`

---

### xwork_file_persistence_list_artifact_summaries

List the artifact summary for the run.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_list_artifact_summaries(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_artifact_summary_list *pList
);
```

**Function:**

Loads the artifact metadata list for the specified run.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `pList`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

The caller must `xwork_artifact_summary_list_reset`.

**Additional Note:**

- Does not load full content text.

**Example code:**

```c
xwork_file_persistence_list_artifact_summaries(&store, "run-1", &summaries);
```

**Related API:**

- `xwork_file_persistence_query_artifact_summaries`

---

### xwork_file_persistence_query_artifact_summaries

Query artifact summary by condition.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_query_artifact_summaries(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    const xwork_artifact_summary_query *pQuery,
    xwork_artifact_summary_list *pList
);
```

**Function:**

Query artifact summary by kind, output class, role, name, MIME, storage ref, exit code, and sequence.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `pQuery`: input parameters. Can be `NULL`.
- `pList`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

The caller reset outputs the list.

**Additional Note:**

- An empty query is equivalent to list summaries.

**Example code:**

```c
xwork_artifact_summary_query query;
xwork_artifact_summary_query_init(&query);
```

**Related API:**

- `xwork_artifact_summary_query_init`

---

### xwork_file_persistence_query_run_steps

Query persistence run step.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_query_run_steps(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    const xwork_run_step_query *pQuery,
    xwork_run_step_list *pList
);
```

**Function:**

Generate step list from persistent event/checkpoint.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `pQuery`: input parameters. Can be `NULL`.
- `pList`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

The caller must `xwork_run_step_list_reset`.

**Additional Note:**

- step is a query projection derived from event/checkpoint.

**Example code:**

```c
xwork_file_persistence_query_run_steps(&store, "run-1", NULL, &steps);
```

**Related API:**

- `xwork_run_step_query_init`

---

### xwork_file_persistence_load_event

Load the specified event.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_load_event(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    const char *sEventId,
    xwork_event *pEvent
);
```

**Function:**

Loads the specified event for the specified run from the file backend.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `sEventId`: input parameters. Must be non-empty.
- `pEvent`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

The event receives the owned field and the caller resets it.

**Additional Note:**

- It is recommended to init the output structure first.

**Example code:**

```c
xwork_event event;
xwork_event_init(&event);
xwork_file_persistence_load_event(&store, "run-1", "event-1", &event);
xwork_event_reset(&event);
```

**Related API:**

- `xwork_file_persistence_list_events`

---

### xwork_file_persistence_load_last_event

Load the last event.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_load_last_event(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_event *pEvent
);
```

**Function:**

Loads the last event of the specified run.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `pEvent`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

Caller reset event.

**Additional Note:**

- Returns `XWORK_ERROR_NOT_FOUND` if there is no event.

**Example code:**

```c
xwork_file_persistence_load_last_event(&store, "run-1", &event);
```

**Related API:**

- `xwork_runtime_load_persisted_last_event`

---

### xwork_file_persistence_load_run_snapshot

Load latest run snapshot.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_load_run_snapshot(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_run_snapshot *pSnapshot
);
```

**Function:**

Load the latest snapshot of the specified run for recovery of the run.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `pSnapshot`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

snapshot receives the owned field and the caller resets.

**Additional Note:**

- The workspace/tool/xllm/host service must be re-registered before recovery.

**Example code:**

```c
xwork_run_snapshot snapshot;
xwork_run_snapshot_init(&snapshot);
xwork_file_persistence_load_run_snapshot(&store, "run-1", &snapshot);
xwork_run_snapshot_reset(&snapshot);
```

**Related API:**

- `xwork_runtime_recover_run`

---

### xwork_file_persistence_load_checkpoint_snapshot

Loading checkpoint corresponds to run snapshot.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_load_checkpoint_snapshot(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    const char *sCheckpointId,
    xwork_run_snapshot *pSnapshot
);
```

**Function:**

Load the run snapshot saved by the specified checkpoint.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `sCheckpointId`: input parameters. Must be non-empty.
- `pSnapshot`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

Caller reset snapshot.

**Additional Note:**

- Used for recovery from historical checkpoints.

**Example code:**

```c
xwork_file_persistence_load_checkpoint_snapshot(&store, "run-1", "ckpt-1", &snapshot);
```

**Related API:**

- `xwork_file_persistence_load_run_snapshot`

---

### xwork_file_persistence_store_task_graph_snapshot

Save task graph snapshot.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_store_task_graph_snapshot(
    const xwork_file_persistence *pStore,
    const xwork_task_graph_snapshot *pSnapshot
);
```

**Function:**

Write multi-agent task graph status to file backend.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `pSnapshot`: input parameters. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

The function reads the snapshot without taking over ownership.

**Additional Note:**

- snapshot id must be valid.

**Example code:**

```c
xwork_file_persistence_store_task_graph_snapshot(&store, &snapshot);
```

**Related API:**

- `xwork_file_persistence_load_task_graph_snapshot`

---

### xwork_file_persistence_load_task_graph_snapshot

Load task graph snapshot.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_load_task_graph_snapshot(
    const xwork_file_persistence *pStore,
    const char *sGraphId,
    xwork_task_graph_snapshot *pSnapshot
);
```

**Function:**

Loads the persistent state of the specified task graph.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `sGraphId`: input parameters. Must be non-empty.
- `pSnapshot`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

Caller reset snapshot.

**Additional Note:**

- Resume execution also requires agent pool and runtime.

**Example code:**

```c
xwork_file_persistence_load_task_graph_snapshot(&store, "graph-1", &snapshot);
```

**Related API:**

- `xwork_file_persistence_recover_task_graph`

---

### xwork_file_persistence_store_agent_pool_snapshot

Save agent pool snapshot.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_store_agent_pool_snapshot(
    const xwork_file_persistence *pStore,
    const xwork_agent_pool_snapshot *pSnapshot
);
```

**Function:**

Save the agent pool configuration and agent snapshot.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `pSnapshot`: input parameters. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

The function reads the snapshot without taking over ownership.

**Additional Note:**

- for multi-agent recovery.

**Example code:**

```c
xwork_file_persistence_store_agent_pool_snapshot(&store, &pool_snapshot);
```

**Related API:**

- `xwork_file_persistence_load_agent_pool_snapshot`

---

### xwork_file_persistence_load_agent_pool_snapshot

Load agent pool snapshot.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_load_agent_pool_snapshot(
    const xwork_file_persistence *pStore,
    const char *sPoolId,
    xwork_agent_pool_snapshot *pSnapshot
);
```

**Function:**

Load the persistent configuration of the specified agent pool.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `sPoolId`: input parameters. Must be non-empty.
- `pSnapshot`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

Caller reset snapshot.

**Additional Note:**

- load only returns data and does not create a live pool.

**Example code:**

```c
xwork_file_persistence_load_agent_pool_snapshot(&store, "pool-1", &snapshot);
```

**Related API:**

- `xwork_agent_pool_create_from_snapshot`

---

### xwork_file_persistence_store_control_plane_snapshot

Save the control plane snapshot.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_store_control_plane_snapshot(
    const xwork_file_persistence *pStore,
    const xwork_control_plane_snapshot *pSnapshot
);
```

**Function:**

Save remote worker control plane state.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `pSnapshot`: input parameters. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

The function reads the snapshot without taking over ownership.

**Additional Note:**

- Contains worker, task, lease, output/blob chunk summary.

**Example code:**

```c
xwork_file_persistence_store_control_plane_snapshot(&store, &snapshot);
```

**Related API:**

- `xwork_file_persistence_load_control_plane_snapshot`

---

### xwork_file_persistence_load_control_plane_snapshot

Load the control plane snapshot.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_load_control_plane_snapshot(
    const xwork_file_persistence *pStore,
    const char *sPlaneId,
    xwork_control_plane_snapshot *pSnapshot
);
```

**Function:**

Load remote worker control plane persistent state.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `sPlaneId`: input parameters. Must be non-empty.
- `pSnapshot`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

Caller reset snapshot.

**Additional Note:**

- load only returns the snapshot and does not start the control plane.

**Example code:**

```c
xwork_file_persistence_load_control_plane_snapshot(&store, "plane-1", &snapshot);
```

**Related API:**

- `xwork_file_persistence_recover_control_plane`

---

### xwork_file_persistence_store_replay

Save replay engine cassette.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_store_replay(
    const xwork_file_persistence *pStore,
    const xwork_replay_engine *pEngine
);
```

**Function:**

Save the replay manifest, entries, events, filesystem refs, and result.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `pEngine`: input parameters. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

The function reads the replay engine and does not take over ownership.

**Additional Note:**

- For the specific recording/playback capabilities of the replay engine, see the Replay API.

**Example code:**

```c
xwork_file_persistence_store_replay(&store, engine);
```

**Related API:**

- `xwork_file_persistence_load_replay_engine`

---

### xwork_file_persistence_list_replays

List replay ids.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_list_replays(
    const xwork_file_persistence *pStore,
    xwork_string_list *pList
);
```

**Function:**

Scan replay cassettes saved in the file backend.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `pList`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

Caller reset string list.

**Additional Note:**

- Only replay id is returned.

**Example code:**

```c
xwork_file_persistence_list_replays(&store, &list);
```

**Related API:**

- `xwork_file_persistence_load_replay_manifest`

---

### xwork_file_persistence_load_replay_manifest

Load the replay manifest.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_load_replay_manifest(
    const xwork_file_persistence *pStore,
    const char *sReplayId,
    xwork_replay_manifest *pManifest
);
```

**Function:**

Load the replay's manifest metadata.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `sReplayId`: input parameters. Must be non-empty.
- `pManifest`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

Caller reset manifest.

**Additional Note:**

- Do not load the entry list.

**Example code:**

```c
xwork_file_persistence_load_replay_manifest(&store, "replay-1", &manifest);
```

**Related API:**

- `xwork_replay_manifest_reset`

---

### xwork_file_persistence_load_replay_entries

Load replay entry summaries.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_load_replay_entries(
    const xwork_file_persistence *pStore,
    const char *sReplayId,
    xwork_replay_entry_summary_list *pList
);
```

**Function:**

Load the entry summary list of replay cassette.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `sReplayId`: input parameters. Must be non-empty.
- `pList`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

Caller reset entry summary list.

**Additional Note:**

- Only the summary is loaded, not the complete payload.

**Example code:**

```c
xwork_file_persistence_load_replay_entries(&store, "replay-1", &entries);
```

**Related API:**

- `xwork_replay_entry_summary_list_reset`

---

### xwork_file_persistence_load_replay_result

Load replay result.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_load_replay_result(
    const xwork_file_persistence *pStore,
    const char *sReplayId,
    xwork_replay_result *pResult
);
```

**Function:**

Load replay execution results and first divergence.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `sReplayId`: input parameters. Must be non-empty.
- `pResult`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

Caller reset replay result.

**Additional Note:**

- Used to replay history UI or CI gate.

**Example code:**

```c
xwork_file_persistence_load_replay_result(&store, "replay-1", &result);
```

**Related API:**

- `xwork_replay_result_reset`

---

### xwork_file_persistence_load_replay_engine

Load replay engine.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_load_replay_engine(
    const xwork_file_persistence *pStore,
    const char *sReplayId,
    const xwork_replay_options *pOptions,
    xwork_replay_engine **ppEngine
);
```

**Function:**

Build a live replay engine from a saved replay cassette.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `sReplayId`: input parameters. Must be non-empty.
- `pOptions`: input parameters. Default options can be used for `NULL`.
- `ppEngine`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

Upon success `*ppEngine` is owned by the caller and must be `xwork_replay_engine_destroy`.

**Additional Note:**

- The live replay engine is not a snapshot and needs to be explicitly destroyed.

**Example code:**

```c
xwork_replay_engine *engine = NULL;
xwork_file_persistence_load_replay_engine(&store, "replay-1", NULL, &engine);
xwork_replay_engine_destroy(engine);
```

**Related API:**

- `xwork_file_persistence_store_replay`

---

### xwork_file_persistence_recover_task_graph

Restore the agent pool and task graph.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_recover_task_graph(
    const xwork_file_persistence *pStore,
    xwork_runtime *pRuntime,
    const char *sPoolId,
    const char *sGraphId,
    const xwork_task_graph_options *pExecutionOptions,
    xwork_agent_pool **ppPool,
    xwork_task_graph **ppGraph
);
```

**Function:**

Create live objects from persistent agent pool snapshots and task graph snapshots.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `pRuntime`: input/output parameters. Must not be `NULL`.
- `sPoolId`: input parameters. Must be non-empty.
- `sGraphId`: input parameters. Must be non-empty.
- `pExecutionOptions`: input parameters. Can be `NULL`.
- `ppPool`: Output parameter. Must not be `NULL`.
- `ppGraph`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence/multi-agent error code.

**Resource ownership:**

After success, the pool and graph are owned by the caller and released using the corresponding destroy function respectively.

**Additional Note:**

- The runtime must have been reconfigured workspace, tool, host service and xllm.

**Example code:**

```c
xwork_file_persistence_recover_task_graph(&store, runtime, "pool-1", "graph-1", NULL, &pool, &graph);
```

**Related API:**

- `xwork_task_graph_destroy`
- `xwork_agent_pool_destroy`

---

### xwork_file_persistence_recover_control_plane

Restore remote control plane.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_recover_control_plane(
    const xwork_file_persistence *pStore,
    xwork_runtime *pRuntime,
    const char *sPlaneId,
    const xwork_control_plane_options *pOptions,
    xwork_control_plane **ppPlane
);
```

**Function:**

Create a live control plane from a control plane snapshot.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `pRuntime`: input/output parameters. Must not be `NULL`.
- `sPlaneId`: input parameters. Must be non-empty.
- `pOptions`: input parameters. Can be `NULL`.
- `ppPlane`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence/remote error code.

**Resource ownership:**

Upon success `*ppPlane` is owned by the caller and must be `xwork_control_plane_destroy`.

**Additional Note:**

- Recovery does not automatically reconnect worker network connections.

**Example code:**

```c
xwork_file_persistence_recover_control_plane(&store, runtime, "plane-1", NULL, &plane);
```

**Related API:**

- `xwork_control_plane_destroy`

---

### xwork_file_persistence_load_last_approval_request

Load the last approval request.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_load_last_approval_request(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_approval_request *pRequest
);
```

**Function:**

Read the last recorded approval request for the specified run.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `pRequest`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

Caller reset request.

**Additional Note:**

- Returns `XWORK_ERROR_NOT_FOUND` when there is no approval request.

**Example code:**

```c
xwork_file_persistence_load_last_approval_request(&store, "run-1", &request);
```

**Related API:**

- `xwork_runtime_load_persisted_last_approval_request`

---

### xwork_file_persistence_load_run_summary

Load run summary.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_load_run_summary(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_run_summary *pSummary
);
```

**Function:**

Read the summary of the specified run.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `pSummary`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

Caller reset summary.

**Additional Note:**

- summary is suitable for list pages and does not contain the full run snapshot.

**Example code:**

```c
xwork_file_persistence_load_run_summary(&store, "run-1", &summary);
```

**Related API:**

- `xwork_file_persistence_load_run_snapshot`

---

### xwork_file_persistence_load_checkpoint

Load checkpoint.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_load_checkpoint(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    const char *sCheckpointId,
    xwork_checkpoint *pCheckpoint
);
```

**Function:**

Read the specified checkpoint metadata for the specified run.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `sCheckpointId`: input parameters. Must be non-empty.
- `pCheckpoint`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

Caller reset checkpoint.

**Additional Note:**

- To restore run state, use checkpoint snapshot.

**Example code:**

```c
xwork_file_persistence_load_checkpoint(&store, "run-1", "ckpt-1", &checkpoint);
```

**Related API:**

- `xwork_file_persistence_load_checkpoint_snapshot`

---

### xwork_file_persistence_load_last_checkpoint

Load the last checkpoint.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_load_last_checkpoint(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_checkpoint *pCheckpoint
);
```

**Function:**

Read the last recorded checkpoint of the specified run.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `pCheckpoint`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

Caller reset checkpoint.

**Additional Note:**

- Returns `XWORK_ERROR_NOT_FOUND` when there is no checkpoint.

**Example code:**

```c
xwork_file_persistence_load_last_checkpoint(&store, "run-1", &checkpoint);
```

**Related API:**

- `xwork_runtime_load_persisted_last_checkpoint`

---

### xwork_file_persistence_load_artifact

Load artifact.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_load_artifact(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    const char *sArtifactId,
    xwork_artifact *pArtifact
);
```

**Function:**

Read the specified artifact for the specified run.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `sArtifactId`: input parameters. Must be non-empty.
- `pArtifact`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

Caller reset artifact.

**Additional Note:**

- Whether content is inlined depends on the artifact options when saving.

**Example code:**

```c
xwork_file_persistence_load_artifact(&store, "run-1", "artifact-1", &artifact);
```

**Related API:**

- `xwork_file_persistence_list_artifacts`

---

### xwork_file_persistence_load_last_artifact

Load the last artifact.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_load_last_artifact(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_artifact *pArtifact
);
```

**Function:**

Read the last recorded artifact of the specified run.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `pArtifact`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

Caller reset artifact.

**Additional Note:**

- Returns `XWORK_ERROR_NOT_FOUND` when there is no artifact.

**Example code:**

```c
xwork_file_persistence_load_last_artifact(&store, "run-1", &artifact);
```

**Related API:**

- `xwork_runtime_load_persisted_last_artifact`

---

### xwork_file_persistence_find_artifact_by_name

Find artifacts by name.

**Function prototype:**

```c
XWORK_API xwork_status xwork_file_persistence_find_artifact_by_name(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    const char *sArtifactName,
    xwork_artifact *pArtifact
);
```

**Function:**

Exactly find and load the artifact by name in the specified run.

**parameter:**

- `pStore`: input parameters. Must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `sArtifactName`: input parameters. Must be non-empty.
- `pArtifact`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or a generic persistence error code.

**Resource ownership:**

Caller reset artifact.

**Additional Note:**

- When there are multiple artifacts with the same name, an implementation-defined match is returned. It is recommended that the business layer use a unique name.

**Example code:**

```c
xwork_file_persistence_find_artifact_by_name(&store, "run-1", "final.md", &artifact);
```

**Related API:**

- `xwork_runtime_find_persisted_artifact_by_name`

---

## Runtime Facade

The Runtime facade function calls the underlying backend through the `xwork_persistence_backend` currently configured by the runtime. Their output ownership is consistent with the corresponding file backend function.

### xwork_runtime_list_persisted_runs

List persistent run ids.

**Function prototype:**

```c
XWORK_API xwork_status xwork_runtime_list_persisted_runs(
    const xwork_runtime *pRuntime,
    xwork_string_list *pList
);
```

**parameter:**

- `pRuntime`: input parameters. A persistence backend must be configured.
- `pList`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or backend error code.

**Resource ownership:**

Caller reset list.

**Additional Note:**

- The facade does not care whether the backend is a file or a custom implementation.

**Example code:**

```c
xwork_runtime_list_persisted_runs(runtime, &list);
```

**Related API:**

- `xwork_file_persistence_list_runs`

---

### xwork_runtime_list_persisted_checkpoints

List persistent checkpoint ids.

**Function prototype:**

```c
XWORK_API xwork_status xwork_runtime_list_persisted_checkpoints(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_string_list *pList
);
```

**parameter:**

- `pRuntime`: input parameters. Backend must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `pList`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or backend error code.

**Resource ownership:**

Caller reset list.

**Additional Note:**

- List only ids.

**Example code:**

```c
xwork_runtime_list_persisted_checkpoints(runtime, "run-1", &list);
```

**Related API:**

- `xwork_runtime_load_persisted_checkpoint`

---

### xwork_runtime_list_persisted_events

List persistent event ids.

**Function prototype:**

```c
XWORK_API xwork_status xwork_runtime_list_persisted_events(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_string_list *pList
);
```

**parameter:**

- `pRuntime`: input parameters. Backend must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `pList`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or backend error code.

**Resource ownership:**

Caller reset list.

**Additional Note:**

- List only ids.

**Example code:**

```c
xwork_runtime_list_persisted_events(runtime, "run-1", &list);
```

**Related API:**

- `xwork_runtime_load_persisted_event`

---

### xwork_runtime_list_persisted_artifacts

List persistence artifact ids.

**Function prototype:**

```c
XWORK_API xwork_status xwork_runtime_list_persisted_artifacts(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_string_list *pList
);
```

**parameter:**

- `pRuntime`: input parameters. Backend must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `pList`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or backend error code.

**Resource ownership:**

Caller reset list.

**Additional Note:**

- List only ids.

**Example code:**

```c
xwork_runtime_list_persisted_artifacts(runtime, "run-1", &list);
```

**Related API:**

- `xwork_runtime_load_persisted_artifact`

---

### xwork_runtime_list_persisted_artifact_summaries

List persistence artifact summary.

**Function prototype:**

```c
XWORK_API xwork_status xwork_runtime_list_persisted_artifact_summaries(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_artifact_summary_list *pList
);
```

**parameter:**

- `pRuntime`: input parameters. Backend must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `pList`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or backend error code.

**Resource ownership:**

Caller reset list.

**Additional Note:**

- Does not load full content.

**Example code:**

```c
xwork_runtime_list_persisted_artifact_summaries(runtime, "run-1", &summaries);
```

**Related API:**

- `xwork_runtime_query_persisted_artifact_summaries`

---

### xwork_runtime_query_persisted_artifact_summaries

Query the persistence artifact summary.

**Function prototype:**

```c
XWORK_API xwork_status xwork_runtime_query_persisted_artifact_summaries(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const xwork_artifact_summary_query *pQuery,
    xwork_artifact_summary_list *pList
);
```

**parameter:**

- `pRuntime`: input parameters. Backend must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `pQuery`: input parameters. Can be `NULL`.
- `pList`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or backend error code.

**Resource ownership:**

Caller reset list.

**Additional Note:**

- If the backend does not provide native query, the runtime can fall back to list and then filter.

**Example code:**

```c
xwork_runtime_query_persisted_artifact_summaries(runtime, "run-1", NULL, &summaries);
```

**Related API:**

- `xwork_artifact_summary_query_init`

---

### xwork_runtime_query_persisted_run_steps

Query persistence run step.

**Function prototype:**

```c
XWORK_API xwork_status xwork_runtime_query_persisted_run_steps(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const xwork_run_step_query *pQuery,
    xwork_run_step_list *pList
);
```

**parameter:**

- `pRuntime`: input parameters. Backend must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `pQuery`: input parameters. Can be `NULL`.
- `pList`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or backend error code.

**Resource ownership:**

Caller reset list.

**Additional Note:**

- If the backend does not support native query, the runtime can be derived from event/checkpoint.

**Example code:**

```c
xwork_runtime_query_persisted_run_steps(runtime, "run-1", NULL, &steps);
```

**Related API:**

- `xwork_run_step_query_init`

---

### xwork_runtime_list_persisted_run_summaries

List persistence run summary.

**Function prototype:**

```c
XWORK_API xwork_status xwork_runtime_list_persisted_run_summaries(
    const xwork_runtime *pRuntime,
    xwork_run_summary_list *pList
);
```

**parameter:**

- `pRuntime`: input parameters. Backend must be configured.
- `pList`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or backend error code.

**Resource ownership:**

Caller reset list.

**Additional Note:**

- Suitable for historical run list UI.

**Example code:**

```c
xwork_runtime_list_persisted_run_summaries(runtime, &list);
```

**Related API:**

- `xwork_runtime_list_persisted_run_index`

---

### xwork_runtime_list_persisted_run_index

List the persistent run index.

**Function prototype:**

```c
XWORK_API xwork_status xwork_runtime_list_persisted_run_index(
    const xwork_runtime *pRuntime,
    xwork_run_index_list *pList
);
```

**parameter:**

- `pRuntime`: input parameters. Backend must be configured.
- `pList`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or backend error code.

**Resource ownership:**

Caller reset list.

**Additional Note:**

- Equivalent to index query without query condition.

**Example code:**

```c
xwork_runtime_list_persisted_run_index(runtime, &index);
```

**Related API:**

- `xwork_runtime_query_persisted_run_index`

---

### xwork_runtime_query_persisted_run_index

Query persistence run index.

**Function prototype:**

```c
XWORK_API xwork_status xwork_runtime_query_persisted_run_index(
    const xwork_runtime *pRuntime,
    const xwork_run_index_query *pQuery,
    xwork_run_index_list *pList
);
```

**parameter:**

- `pRuntime`: input parameters. Backend must be configured.
- `pQuery`: input parameters. Can be `NULL`.
- `pList`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or backend error code.

**Resource ownership:**

Caller reset list.

**Additional Note:**

- The query string field is borrowed from the caller.

**Example code:**

```c
xwork_runtime_query_persisted_run_index(runtime, NULL, &index);
```

**Related API:**

- `xwork_run_index_query_init`

---

### xwork_runtime_load_persisted_run_summary

Load persistence run summary.

**Function prototype:**

```c
XWORK_API xwork_status xwork_runtime_load_persisted_run_summary(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_run_summary *pSummary
);
```

**parameter:**

- `pRuntime`: input parameters. Backend must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `pSummary`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or backend error code.

**Resource ownership:**

Caller reset summary.

**Additional Note:**

- Do not resume live run.

**Example code:**

```c
xwork_runtime_load_persisted_run_summary(runtime, "run-1", &summary);
```

**Related API:**

- `xwork_runtime_recover_run_from_persistence`

---

### xwork_runtime_load_persisted_last_event

Load the persisted last event.

**Function prototype:**

```c
XWORK_API xwork_status xwork_runtime_load_persisted_last_event(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_event *pEvent
);
```

**parameter:**

- `pRuntime`: input parameters. Backend must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `pEvent`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or backend error code.

**Resource ownership:**

Caller reset event.

**Additional Note:**

- Return not found when there is no event.

**Example code:**

```c
xwork_runtime_load_persisted_last_event(runtime, "run-1", &event);
```

**Related API:**

- `xwork_runtime_load_persisted_event`

---

### xwork_runtime_load_persisted_last_approval_request

Load the persisted last approval request.

**Function prototype:**

```c
XWORK_API xwork_status xwork_runtime_load_persisted_last_approval_request(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_approval_request *pRequest
);
```

**parameter:**

- `pRuntime`: input parameters. Backend must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `pRequest`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or backend error code.

**Resource ownership:**

Caller reset request.

**Additional Note:**

- Used to restore approval UI.

**Example code:**

```c
xwork_runtime_load_persisted_last_approval_request(runtime, "run-1", &request);
```

**Related API:**

- `xwork_run_submit_approval`

---

### xwork_runtime_load_persisted_last_checkpoint

Load the last persistent checkpoint.

**Function prototype:**

```c
XWORK_API xwork_status xwork_runtime_load_persisted_last_checkpoint(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_checkpoint *pCheckpoint
);
```

**parameter:**

- `pRuntime`: input parameters. Backend must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `pCheckpoint`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or backend error code.

**Resource ownership:**

Caller reset checkpoint.

**Additional Note:**

- Only load checkpoint metadata.

**Example code:**

```c
xwork_runtime_load_persisted_last_checkpoint(runtime, "run-1", &checkpoint);
```

**Related API:**

- `xwork_runtime_load_persisted_checkpoint`

---

### xwork_runtime_load_persisted_last_artifact

Load the persistent last artifact.

**Function prototype:**

```c
XWORK_API xwork_status xwork_runtime_load_persisted_last_artifact(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_artifact *pArtifact
);
```

**parameter:**

- `pRuntime`: input parameters. Backend must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `pArtifact`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or backend error code.

**Resource ownership:**

Caller reset artifact.

**Additional Note:**

- Returns not found when there is no artifact.

**Example code:**

```c
xwork_runtime_load_persisted_last_artifact(runtime, "run-1", &artifact);
```

**Related API:**

- `xwork_runtime_load_persisted_artifact`

---

### xwork_runtime_load_persisted_event

Load persistent events.

**Function prototype:**

```c
XWORK_API xwork_status xwork_runtime_load_persisted_event(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const char *sEventId,
    xwork_event *pEvent
);
```

**parameter:**

- `pRuntime`: input parameters. Backend must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `sEventId`: input parameters. Must be non-empty.
- `pEvent`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or backend error code.

**Resource ownership:**

Caller reset event.

**Additional Note:**

- Used for auditing and step queries.

**Example code:**

```c
xwork_runtime_load_persisted_event(runtime, "run-1", "event-1", &event);
```

**Related API:**

- `xwork_runtime_list_persisted_events`

---

### xwork_runtime_load_persisted_checkpoint

Load persistent checkpoint.

**Function prototype:**

```c
XWORK_API xwork_status xwork_runtime_load_persisted_checkpoint(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const char *sCheckpointId,
    xwork_checkpoint *pCheckpoint
);
```

**parameter:**

- `pRuntime`: input parameters. Backend must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `sCheckpointId`: input parameters. Must be non-empty.
- `pCheckpoint`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or backend error code.

**Resource ownership:**

Caller reset checkpoint.

**Additional Note:**

- Only load checkpoint metadata.

**Example code:**

```c
xwork_runtime_load_persisted_checkpoint(runtime, "run-1", "ckpt-1", &checkpoint);
```

**Related API:**

- `xwork_runtime_load_persisted_last_checkpoint`

---

### xwork_runtime_load_persisted_artifact

Load persistence artifacts.

**Function prototype:**

```c
XWORK_API xwork_status xwork_runtime_load_persisted_artifact(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const char *sArtifactId,
    xwork_artifact *pArtifact
);
```

**parameter:**

- `pRuntime`: input parameters. Backend must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `sArtifactId`: input parameters. Must be non-empty.
- `pArtifact`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or backend error code.

**Resource ownership:**

Caller reset artifact.

**Additional Note:**

- Whether content is available depends on the backend.

**Example code:**

```c
xwork_runtime_load_persisted_artifact(runtime, "run-1", "artifact-1", &artifact);
```

**Related API:**

- `xwork_runtime_find_persisted_artifact_by_name`

---

### xwork_runtime_find_persisted_artifact_by_name

Find persistence artifacts by name.

**Function prototype:**

```c
XWORK_API xwork_status xwork_runtime_find_persisted_artifact_by_name(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const char *sArtifactName,
    xwork_artifact *pArtifact
);
```

**parameter:**

- `pRuntime`: input parameters. Backend must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `sArtifactName`: input parameters. Must be non-empty.
- `pArtifact`: Output parameter. Must not be `NULL`.

**Return value:**

Returns `XWORK_OK` or backend error code.

**Resource ownership:**

Caller reset artifact.

**Additional Note:**

- It is recommended that the business layer ensure that the artifact name is unique.

**Example code:**

```c
xwork_runtime_find_persisted_artifact_by_name(runtime, "run-1", "final.md", &artifact);
```

**Related API:**

- `xwork_runtime_load_persisted_artifact`

---

### xwork_runtime_recover_run

Resume a live run from a run snapshot.

**Function prototype:**

```c
XWORK_API xwork_status xwork_runtime_recover_run(
    xwork_runtime *pRuntime,
    const xwork_run_snapshot *pSnapshot,
    xwork_run **ppRun
);
```

**parameter:**

- `pRuntime`: input/output parameters. Must not be `NULL`.
- `pSnapshot`: input parameters. Must not be `NULL`.
- `ppRun`: Output parameter. Must not be `NULL`.

**Return value:**

Return `XWORK_OK` or run to recover the error code.

**Resource ownership:**

After successful run attaches to the runtime and is explicitly destroyed or runtime destroyed by the caller.

**Additional Note:**

- Compatible workspace/tool/xllm/host service must be registered before recovery.
- Does not restore live process, terminal, thread or callback stacks.

**Example code:**

```c
xwork_runtime_recover_run(runtime, &snapshot, &run);
```

**Related API:**

- `xwork_file_persistence_load_run_snapshot`

---

### xwork_runtime_recover_run_from_persistence

Resume live run from persistence latest snapshot.

**Function prototype:**

```c
XWORK_API xwork_status xwork_runtime_recover_run_from_persistence(
    xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_run **ppRun
);
```

**parameter:**

- `pRuntime`: input/output parameters. Backend must be configured.
- `sRunId`: input parameters. Must be non-empty.
- `ppRun`: Output parameter. Must not be `NULL`.

**Return value:**

Return `XWORK_OK` or backend/run recovery error code.

**Resource ownership:**

After successful run attaches to the runtime.

**Additional Note:**

- This is a convenient entry point for calling `xwork_runtime_recover_run` after loading the latest snapshot.

**Example code:**

```c
xwork_runtime_recover_run_from_persistence(runtime, "run-1", &run);
```

**Related API:**

- `xwork_runtime_recover_run`

## Restore boundaries

Serializable status, workspace id, pending tool, approval decision, last checkpoint, artifact metadata, agent/task/worker/replay snapshot can be restored. Live OS process handles, interactive terminal sessions, thread stacks, network connections, callback stacks, or user UI sessions cannot be restored.

## Thread boundaries

The built-in file backend is not designed as a multi-process/multi-writer database. Concurrent writes to the same store root should be serialized by the caller. The runtime facade's concurrency boundaries are consistent with the underlying backend.

## Related documents

- [Run API](api-run.md)
- [Artifact API](api-artifacts.md)
- [Replay API](api-replay.md)
- [Persistence, Checkpoints, and Replay](../guide/persistence-replay-intro.md)
- [Internal persistence format](../../dev/docs/PERSISTENCE_FORMAT.md)
