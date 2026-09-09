# Run API

> Status: Chinese function-by-function reference, waiting for manual review. This article first covers the complete function index of the run main path and event/step/checkpoint/summary/query/async; field-level details can be expanded later.

`xwork_run` represents an Agent task run. It carries instructions, workspace references, lifecycle states, events, steps, checkpoints, approvals, artifacts, and summaries.

## Module positioning

The Run API is responsible for the life cycle of xwork tasks, state advancement, querying, synchronous/asynchronous execution and recoverable object snapshots. It does not directly determine how the model answers, nor does it directly perform host side effects; these are done collaboratively by the orchestrator, tool, policy, and host service.

## This page covers the statement

| Category | Statement |
| --- | --- |
| Opaque objects | `xwork_run`, `xwork_run_async` |
| Structure | `xwork_run_options`, `xwork_event`, `xwork_run_step`, `xwork_run_index_entry`, `xwork_run_index_list`, `xwork_run_index_query` |
| Function | All `### xwork_*` sections on this page |

## Ownership Rules

- The run returned by `xwork_run_create` is attached to the runtime, and the caller can explicitly `xwork_run_destroy`, or it can be released uniformly by `xwork_runtime_destroy`.
- The run id, parent run id, agent id, task id, instruction, profile id and workspace id in run options will be copied.
- The workspace pointed to by the workspace id must be registered in the runtime and remain valid during the run life cycle.
- The `*_get_*` API output to the structure will deep-copy the mutable string; the caller must call the corresponding `*_reset`.
- The `const char *` returned by the getter is a borrowed pointer and will become invalid after being destroyed by run.
- async handle is created with `xwork_run_execute_async` and must be released with `xwork_run_async_destroy`.

## Common calling sequence

```text
xwork_run_options_init
xwork_run_create
xwork_run_execute / xwork_run_execute_async
xwork_run_get_summary / xwork_run_get_snapshot
xwork_run_destroy / xwork_runtime_destroy
```

## General init/reset convention

The following init functions allow `NULL` to be passed in. No operation is performed when `NULL` is passed in; the default value is written when it is not `NULL`. The reset function allows passing in `NULL`, which will release the deep copy fields owned by the structure and reinitialize it.

---

### xwork_run_options_init

Initialize run creation options.

**Function:**

Set stable defaults before calling `xwork_run_create`, then fill in the instruction, workspace id and profile/autonomy configuration.

**Function prototype:**

```c
XWORK_API void xwork_run_options_init(xwork_run_options *pOptions);
```

**parameter:**

- `pOptions`: Output parameter. It can be `NULL`; cleared when it is not `NULL`. The default `eAutonomy` is `XWORK_AUTONOMY_SEMI_AUTO`, and `tSessionPolicy` is initialized.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. All string and workspace id array fields are provided by the caller before create.

**Additional Note:**

- `sInstruction` is the core input for creating a run and must usually be non-empty.
- `psWorkspaceIds` can be empty; when non-empty, each id must be found in the runtime.

**Example code:**

```c
#include "xwork.h"

int main(void) {
    xwork_run_options options;
    xwork_run_options_init(&options);
    options.sInstruction = "Summarize the workspace.";
    return 0;
}
```

**Related API:**

- `xwork_run_create`
- `xwork_profile_apply_run_options`

---

### xwork_run_summary_init

Initialize run summary.

**Function:**

Used to prepare to receive the summary returned by the `xwork_run_get_summary` or persistence query.

**Function prototype:**

```c
XWORK_API void xwork_run_summary_init(xwork_run_summary *pSummary);
```

**parameter:**

- `pSummary`: Output parameter. Can be `NULL`; cleared and written to default autonomy/state when not `NULL`.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. The copy of the string written by subsequent APIs is owned by summary and needs to be released by `xwork_run_summary_reset`.

**Additional Note:**

- Default `eAutonomy` is `XWORK_AUTONOMY_SEMI_AUTO`.
- Default `eState` is `XWORK_RUN_CREATED`.

**Example code:**

```c
#include "xwork.h"

int main(void) {
    xwork_run_summary summary;
    xwork_run_summary_init(&summary);
    xwork_run_summary_reset(&summary);
    return 0;
}
```

**Related API:**

- `xwork_run_get_summary`
- `xwork_run_summary_reset`

---

### xwork_run_summary_reset

Release and reset the run summary.

**Function:**

Release the string fields in the summary that are deep-copyed by xwork so that the structure can be reused or end its life cycle safely.

**Function prototype:**

```c
XWORK_API void xwork_run_summary_reset(xwork_run_summary *pSummary);
```

**parameter:**

- `pSummary`: input/output parameters. Can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Release `sRunId`, `sParentRunId`, `sAgentId`, `sTaskId`, `sInstruction`, etc. owned string copies.

**Additional Note:**

- It is not safe to call reset on an uninitialized structure; use init first.

**Example code:**

```c
xwork_run_summary summary;
xwork_run_summary_init(&summary);
/* fill summary */
xwork_run_summary_reset(&summary);
```

**Related API:**

- `xwork_run_summary_init`
- `xwork_run_get_summary`

---

### xwork_run_summary_list_init

Initialize the run summary list.

**Function:**

Prepare to receive a list of run summaries returned by persistence or runtime queries.

**Function prototype:**

```c
XWORK_API void xwork_run_summary_list_init(xwork_run_summary_list *pList);
```

**parameter:**

- `pList`: Output parameter. Can be `NULL`; cleared if not `NULL`.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. After the query function populates, the list has a copy of `pItems` and the internal summary field.

**Additional Note:**

- Call `xwork_run_summary_list_reset` after use.

**Example code:**

```c
xwork_run_summary_list list;
xwork_run_summary_list_init(&list);
xwork_run_summary_list_reset(&list);
```

**Related API:**

- `xwork_run_summary_list_reset`
- `xwork_runtime_list_persisted_run_summaries`

---

### xwork_run_summary_list_reset

Free and reset the run summary list.

**Function:**

Free the list array and the deep-copy fields of each summary.

**Function prototype:**

```c
XWORK_API void xwork_run_summary_list_reset(xwork_run_summary_list *pList);
```

**parameter:**

- `pList`: input/output parameters. Can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases the `pItems` array and the strings owned by each `xwork_run_summary`.

**Additional Note:**

- The list returns to the init state after reset.

**Example code:**

```c
xwork_run_summary_list list;
xwork_run_summary_list_init(&list);
xwork_run_summary_list_reset(&list);
```

**Related API:**

- `xwork_run_summary_list_init`

---

### xwork_run_index_entry_init

Initialize run index entry.

**Function:**

Prepare to receive run index query items, including summary, last approval/event/checkpoint/artifact.

**Function prototype:**

```c
XWORK_API void xwork_run_index_entry_init(xwork_run_index_entry *pEntry);
```

**parameter:**

- `pEntry`: Output parameter. Can be `NULL`; initializes nested objects when not `NULL`.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. The populated entry holds the deep-copy fields of the nested object.

**Additional Note:**

- Call `xwork_run_index_entry_reset` after use.

**Example code:**

```c
xwork_run_index_entry entry;
xwork_run_index_entry_init(&entry);
xwork_run_index_entry_reset(&entry);
```

**Related API:**

- `xwork_run_index_entry_reset`
- `xwork_runtime_query_persisted_run_index`

---

### xwork_run_index_entry_reset

Release and reset the run index entry.

**Function:**

Release the owned fields of entry's embedded summary, approval, event, checkpoint, and artifact.

**Function prototype:**

```c
XWORK_API void xwork_run_index_entry_reset(xwork_run_index_entry *pEntry);
```

**parameter:**

- `pEntry`: input/output parameters. Can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases all deep-copied fields owned by entry.

**Additional Note:**

- After reset, entry returns to init state.

**Example code:**

```c
xwork_run_index_entry entry;
xwork_run_index_entry_init(&entry);
xwork_run_index_entry_reset(&entry);
```

**Related API:**

- `xwork_run_index_entry_init`

---

### xwork_run_index_list_init

Initialize the run index list.

**Function:**

Prepare to receive run index query results.

**Function prototype:**

```c
XWORK_API void xwork_run_index_list_init(xwork_run_index_list *pList);
```

**parameter:**

- `pList`: Output parameter. Can be `NULL`; cleared if not `NULL`.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. After the query is populated, the list has the `pItems` array.

**Additional Note:**

- Call `xwork_run_index_list_reset` after use.

**Example code:**

```c
xwork_run_index_list list;
xwork_run_index_list_init(&list);
xwork_run_index_list_reset(&list);
```

**Related API:**

- `xwork_run_index_list_reset`

---

### xwork_run_index_list_reset

Free and reset the run index list.

**Function:**

Free the list array and the deep-copy fields owned by each index entry.

**Function prototype:**

```c
XWORK_API void xwork_run_index_list_reset(xwork_run_index_list *pList);
```

**parameter:**

- `pList`: input/output parameters. Can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases the `pItems` and its element contents owned by the list.

**Additional Note:**

- After reset, the same list variable can be reused to receive the next query.

**Example code:**

```c
xwork_run_index_list list;
xwork_run_index_list_init(&list);
xwork_run_index_list_reset(&list);
```

**Related API:**

- `xwork_run_index_list_init`

---

### xwork_run_index_query_init

Initialize run index query conditions.

**Function:**

Used to construct persistent run index query filtering and sorting conditions.

**Function prototype:**

```c
XWORK_API void xwork_run_index_query_init(xwork_run_index_query *pQuery);
```

**parameter:**

- `pQuery`: Output parameter. Can be `NULL`; default filter and sort values ​​are written when not `NULL`.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. Query string fields are borrowed and provided by the caller.

**Additional Note:**

- The default state is `XWORK_RUN_CREATED`, the autonomy is `XWORK_AUTONOMY_SEMI_AUTO`, and the sorting is run id ascending order.
- Specific field meanings are used by the persistence API.

**Example code:**

```c
xwork_run_index_query query;
xwork_run_index_query_init(&query);
```

**Related API:**

- `xwork_runtime_query_persisted_run_index`

---

### xwork_event_init

Initialize event.

**Function:**

Prepare to receive run event or persistence event query results.

**Function prototype:**

```c
XWORK_API void xwork_event_init(xwork_event *pEvent);
```

**parameter:**

- `pEvent`: Output parameter. Can be `NULL`; cleared and written to default kind/state when not `NULL`.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. The filled event has a deep-copy string, which is used to call `xwork_event_reset`.

**Additional Note:**

- event represents run life cycle, tool, approval, checkpoint and other events.

**Example code:**

```c
xwork_event event;
xwork_event_init(&event);
xwork_event_reset(&event);
```

**Related API:**

- `xwork_run_get_event`
- `xwork_event_reset`

---

### xwork_event_reset

Release and reset the event.

**Function:**

Release the id, run id, tool id, summary and other deep-copy fields in the event.

**Function prototype:**

```c
XWORK_API void xwork_event_reset(xwork_event *pEvent);
```

**parameter:**

- `pEvent`: input/output parameters. Can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases the copy of the string owned by event.

**Additional Note:**

- Query output structures should be reset when no longer used.

**Example code:**

```c
xwork_event event;
xwork_event_init(&event);
xwork_event_reset(&event);
```

**Related API:**

- `xwork_event_init`
- `xwork_run_get_last_event`

---

### xwork_run_step_init

Initialize run step.

**Function:**

Prepare to receive step results aggregated by event/checkpoint.

**Function prototype:**

```c
XWORK_API void xwork_run_step_init(xwork_run_step *pStep);
```

**parameter:**

- `pStep`: Output parameter. Can be `NULL`; clear when not `NULL` and write to the default run state.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. After filling, step has deep-copy field and needs to be reset.

**Additional Note:**

- step is a UI/query-oriented projection, not an independent execution object.

**Example code:**

```c
xwork_run_step step;
xwork_run_step_init(&step);
xwork_run_step_reset(&step);
```

**Related API:**

- `xwork_run_get_step`
- `xwork_run_step_reset`

---

### xwork_run_step_reset

Release and reset the run step.

**Function:**

Release the string field in step that was deep-copyed by xwork.

**Function prototype:**

```c
XWORK_API void xwork_run_step_reset(xwork_run_step *pStep);
```

**parameter:**

- `pStep`: input/output parameters. Can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases the copy of the string owned by step.

**Additional Note:**

- Step back to init state after reset.

**Example code:**

```c
xwork_run_step step;
xwork_run_step_init(&step);
xwork_run_step_reset(&step);
```

**Related API:**

- `xwork_run_step_init`

---

### xwork_run_step_list_init

Initialize step list.

**Function:**

Prepare to receive the step list returned by `xwork_run_query_steps`.

**Function prototype:**

```c
XWORK_API void xwork_run_step_list_init(xwork_run_step_list *pList);
```

**parameter:**

- `pList`: Output parameter. Can be `NULL`; cleared if not `NULL`.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. After the query is populated the list has `pItems`.

**Additional Note:**

- Call `xwork_run_step_list_reset` after use.

**Example code:**

```c
xwork_run_step_list list;
xwork_run_step_list_init(&list);
xwork_run_step_list_reset(&list);
```

**Related API:**

- `xwork_run_query_steps`
- `xwork_run_step_list_reset`

---

### xwork_run_step_list_reset

Free and reset the step list.

**Function:**

Free the list array and deep-copy fields of each step.

**Function prototype:**

```c
XWORK_API void xwork_run_step_list_reset(xwork_run_step_list *pList);
```

**parameter:**

- `pList`: input/output parameters. Can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases the `pItems` array and its element contents.

**Additional Note:**

- The list can be reused after reset.

**Example code:**

```c
xwork_run_step_list list;
xwork_run_step_list_init(&list);
xwork_run_step_list_reset(&list);
```

**Related API:**

- `xwork_run_step_list_init`

---

### xwork_run_step_query_init

Initialize step query conditions.

**Function:**

Filter criteria used to construct `xwork_run_query_steps`, such as event type, status, sequence range, and limit.

**Function prototype:**

```c
XWORK_API void xwork_run_step_query_init(xwork_run_step_query *pQuery);
```

**parameter:**

- `pQuery`: Output parameter. Can be `NULL`; cleared if not `NULL`.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. Query string fields are borrowed and provided by the caller.

**Additional Note:**

- An empty query means no filtering.

**Example code:**

```c
xwork_run_step_query query;
xwork_run_step_query_init(&query);
query.iLimit = 20u;
```

**Related API:**

- `xwork_run_query_steps`

---

### xwork_checkpoint_init

Initialize checkpoint.

**Function:**

Prepare to receive run checkpoint or persistence checkpoint query results.

**Function prototype:**

```c
XWORK_API void xwork_checkpoint_init(xwork_checkpoint *pCheckpoint);
```

**parameter:**

- `pCheckpoint`: Output parameter. Can be `NULL`; cleared and written to default kind/state when not `NULL`.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. The populated checkpoint has a deep-copy string and needs to be reset.

**Additional Note:**

- Checkpoint saves recoverable boundaries, but does not save live threads, processes, or callback stacks.

**Example code:**

```c
xwork_checkpoint checkpoint;
xwork_checkpoint_init(&checkpoint);
xwork_checkpoint_reset(&checkpoint);
```

**Related API:**

- `xwork_run_get_checkpoint`
- `xwork_checkpoint_reset`

---

### xwork_checkpoint_reset

Release and reset checkpoint.

**Function:**

Release checkpoint id, pending step, session state ref, tool output ref and other deep-copy fields.

**Function prototype:**

```c
XWORK_API void xwork_checkpoint_reset(xwork_checkpoint *pCheckpoint);
```

**parameter:**

- `pCheckpoint`: input/output parameters. Can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases the copy of the string owned by checkpoint.

**Additional Note:**

- Checkpoint returns to init state after reset.

**Example code:**

```c
xwork_checkpoint checkpoint;
xwork_checkpoint_init(&checkpoint);
xwork_checkpoint_reset(&checkpoint);
```

**Related API:**

- `xwork_checkpoint_init`

---

### xwork_run_snapshot_init

Initialize run snapshot.

**Function:**

Prepare to receive a run snapshot from `xwork_run_get_snapshot` or persistence loading.

**Function prototype:**

```c
XWORK_API void xwork_run_snapshot_init(xwork_run_snapshot *pSnapshot);
```

**parameter:**

- `pSnapshot`: Output parameter. Can be `NULL`; clear and write default state when not `NULL`.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. The filled snapshot has deep copy fields and workspace id arrays and needs to be reset.

**Additional Note:**

- snapshot is used to restore the serializable state of the run and does not include live runtime resources.

**Example code:**

```c
xwork_run_snapshot snapshot;
xwork_run_snapshot_init(&snapshot);
xwork_run_snapshot_reset(&snapshot);
```

**Related API:**

- `xwork_run_get_snapshot`
- `xwork_run_snapshot_reset`

---

### xwork_run_snapshot_reset

Release and reset the run snapshot.

**Function:**

Releases the string, workspace id array, and nested status fields owned by the snapshot.

**Function prototype:**

```c
XWORK_API void xwork_run_snapshot_reset(xwork_run_snapshot *pSnapshot);
```

**parameter:**

- `pSnapshot`: input/output parameters. Can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases all deep copy fields owned by snapshot.

**Additional Note:**

- The snapshot life cycle should be clear before and after using `xwork_runtime_recover_run`.

**Example code:**

```c
xwork_run_snapshot snapshot;
xwork_run_snapshot_init(&snapshot);
xwork_run_snapshot_reset(&snapshot);
```

**Related API:**

- `xwork_run_snapshot_init`
- `xwork_runtime_recover_run`

---

### xwork_run_create

Create run.

**Function:**

Create an agent task to run and attach it to the runtime.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_create(
    xwork_runtime *pRuntime,
    const xwork_run_options *pOptions,
    xwork_run **ppRun
);
```

**parameter:**

- `pRuntime`: input/output parameters. Must not be `NULL`.
- `pOptions`: input parameters. Must be non-`NULL`, usually requires non-empty `sInstruction`.
- `ppRun`: Output parameter. Must not be `NULL`. Receives the run pointer on success; `NULL` on failure.

**Return value:**

- `XWORK_OK`: Created successfully.
- `XWORK_ERROR_INVALID_ARGUMENT`: Invalid runtime, options, output pointer, or required field.
- `XWORK_ERROR_NOT_FOUND`: The referenced workspace id is not registered.
- `XWORK_ERROR_NO_MEMORY`: Allocation or string copy failed.

**Resource ownership:**

After a successful run the run is owned by the runtime. The caller can destroy it explicitly or leave it to the runtime for destruction.

**Additional Note:**

- `XWORK_EVENT_RUN_CREATED` is logged when created.
- The workspace id array is deep-copied, but the workspace itself retains runtime ownership.

**Example code:**

```c
#include "xwork.h"

int create_run(xwork_runtime *runtime) {
    xwork_run_options options;
    xwork_run *run = NULL;
    xwork_run_options_init(&options);
    options.sInstruction = "Inspect this repository.";
    return xwork_run_create(runtime, &options, &run) == XWORK_OK ? 0 : 1;
}
```

**Related API:**

- `xwork_run_options_init`
- `xwork_run_destroy`

---

### xwork_run_destroy

Destroy run.

**Function:**

Release memory related to run and its events, checkpoints, artifacts, and snapshots in advance.

**Function prototype:**

```c
XWORK_API void xwork_run_destroy(xwork_run *pRun);
```

**parameter:**

- `pRun`: input/destroy parameters. Can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Release run itself and internal owned fields, and remove them from the runtime linked list.

**Additional Note:**

- Do not destroy run while it is executing asynchronously; cancel/wait async handle first.

**Example code:**

```c
void close_run(xwork_run *run) {
    xwork_run_destroy(run);
}
```

**Related API:**

- `xwork_run_create`
- `xwork_run_async_destroy`

---

### xwork_run_start

Advance run to running.

**Function:**

Used for manual lifecycle advancement or orchestrator execution entry, setting the launchable run to running and logging events.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_start(xwork_run *pRun);
```

**parameter:**

- `pRun`: input/output parameters. Must not be `NULL`.

**Return value:**

- `XWORK_OK`: Status advancement successful.
- `XWORK_ERROR_INVALID_ARGUMENT`: run is empty.
- `XWORK_ERROR_INVALID_STATE`: Current status does not allow startup.

**Resource ownership:**

Resource ownership is not transferred.

**Additional Note:**

- The orchestrator is automatically called when needed.

**Example code:**

```c
xwork_status status = xwork_run_start(run);
```

**Related API:**

- `xwork_run_complete`
- `xwork_run_execute`

---

### xwork_run_set_waiting_approval

Mark the run as pending approval.

**Function:**

Used to enter the pause boundary when a tool or side effect requires manual/policy approval.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_set_waiting_approval(xwork_run *pRun);
```

**parameter:**

- `pRun`: input/output parameters. Must not be `NULL`.

**Return value:**

- `XWORK_OK`: Status advancement successful.
- `XWORK_ERROR_INVALID_ARGUMENT`: run is empty.
- `XWORK_ERROR_INVALID_STATE`: The conversion is not allowed in the current state.

**Resource ownership:**

Resource ownership is not transferred.

**Additional Note:**

- Approval request details are typically logged via the approval API or orchestrator.

**Example code:**

```c
xwork_status status = xwork_run_set_waiting_approval(run);
```

**Related API:**

- `xwork_run_submit_approval`
- `xwork_run_resume`

---

### xwork_run_set_waiting_tool

Mark run as a waiting tool.

**Function:**

Used for scenarios where tool calls are suspended, waiting for external tool results, or resuming pending tools.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_set_waiting_tool(xwork_run *pRun);
```

**parameter:**

- `pRun`: input/output parameters. Must not be `NULL`.

**Return value:**

- `XWORK_OK`: Status advancement successful.
- `XWORK_ERROR_INVALID_ARGUMENT`: run is empty.
- `XWORK_ERROR_INVALID_STATE`: The conversion is not allowed in the current state.

**Resource ownership:**

Resource ownership is not transferred.

**Additional Note:**

- Generally used by orchestrator/tool ​​loop.

**Example code:**

```c
xwork_status status = xwork_run_set_waiting_tool(run);
```

**Related API:**

- `xwork_run_resume`
- `xwork_runtime_find_tool`

---

### xwork_run_set_paused

Mark run as paused.

**Function:**

Used to enter the resumable pause boundary and record the paused event.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_set_paused(xwork_run *pRun);
```

**parameter:**

- `pRun`: input/output parameters. Must not be `NULL`.

**Return value:**

- `XWORK_OK`: Suspended successfully.
- `XWORK_ERROR_INVALID_ARGUMENT`: run is empty.
- `XWORK_ERROR_INVALID_STATE`: Current status does not allow suspension.

**Resource ownership:**

Resource ownership is not transferred.

**Additional Note:**

- The paused state can be restored by trying `xwork_run_resume`.

**Example code:**

```c
xwork_status status = xwork_run_set_paused(run);
```

**Related API:**

- `xwork_run_resume`

---

### xwork_run_submit_approval

Submit the approval results.

**Function:**

Write external approval decisions back to run, used to restore tools/side effects waiting for approval.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_submit_approval(
    xwork_run *pRun,
    xwork_approval_state eDecision
);
```

**parameter:**

- `pRun`: input/output parameters. Must not be `NULL`.
- `eDecision`: input parameters. Approval status, such as approved/denied.

**Return value:**

- `XWORK_OK`: Submission successful.
- `XWORK_ERROR_INVALID_ARGUMENT`: run is invalid.
- `XWORK_ERROR_INVALID_STATE`: run currently has no approval boundaries to submit.

**Resource ownership:**

Resource ownership is not transferred.

**Additional Note:**

- Normally continue calling `xwork_run_resume` after committing or let the orchestrator resume.

**Example code:**

```c
xwork_status status = xwork_run_submit_approval(run, XWORK_APPROVAL_APPROVED);
```

**Related API:**

- `xwork_run_get_last_approval_request`
- `xwork_run_resume`

---

### xwork_run_load_checkpoint

Load the specified checkpoint into run.

**Function:**

Set the recovery target of run to the specified checkpoint for subsequent resume.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_load_checkpoint(
    xwork_run *pRun,
    const char *sCheckpointId
);
```

**parameter:**

- `pRun`: input/output parameters. Must not be `NULL`.
- `sCheckpointId`: input parameters. Must be a non-empty checkpoint id.

**Return value:**

- `XWORK_OK`: Loaded successfully.
- `XWORK_ERROR_INVALID_ARGUMENT`: Invalid parameter.
- `XWORK_ERROR_NOT_FOUND`: checkpoint id does not exist.
- `XWORK_ERROR_INVALID_STATE`: The current state cannot be loaded.

**Resource ownership:**

Checkpoint ownership is not transferred; run internally copies/references its recoverable fields.

**Additional Note:**

- Loading checkpoint does not restore live process/thread/terminal.

**Example code:**

```c
xwork_status status = xwork_run_load_checkpoint(run, "checkpoint-1");
```

**Related API:**

- `xwork_run_get_checkpoint`
- `xwork_run_resume`

---

### xwork_run_resume

Resume run.

**Function:**

Resumes the run state from a paused, waiting, or checkpoint boundary so that the orchestrator can continue execution.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_resume(xwork_run *pRun);
```

**parameter:**

- `pRun`: input/output parameters. Must not be `NULL`.

**Return value:**

- `XWORK_OK`: Recovery successful.
- `XWORK_ERROR_INVALID_ARGUMENT`: run is empty.
- `XWORK_ERROR_INVALID_STATE`: terminal run or unrecoverable boundary.

**Resource ownership:**

Resource ownership is not transferred.

**Additional Note:**

- If there is a pending tool call, compatible tool registration and approval status are required before recovery.

**Example code:**

```c
xwork_status status = xwork_run_resume(run);
```

**Related API:**

- `xwork_run_load_checkpoint`
- `xwork_run_submit_approval`

---

### xwork_run_complete

Mark run as complete.

**Function:**

For manual life cycle advancement or when the orchestrator ends successfully, set run to terminal completed.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_complete(xwork_run *pRun);
```

**parameter:**

- `pRun`: input/output parameters. Must not be `NULL`.

**Return value:**

- `XWORK_OK`: Completed successfully.
- `XWORK_ERROR_INVALID_ARGUMENT`: run is empty.
- `XWORK_ERROR_INVALID_STATE`: Current status does not allow completion.

**Resource ownership:**

Resource ownership is not transferred.

**Additional Note:**

- The run completed event will be logged after success.

**Example code:**

```c
xwork_status status = xwork_run_complete(run);
```

**Related API:**

- `xwork_run_start`
- `xwork_run_fail`

---

### xwork_run_cancel

Mark the run as canceled.

**Function:**

Used to set run to terminal canceled after collaborative cancellation is completed.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_cancel(xwork_run *pRun);
```

**parameter:**

- `pRun`: input/output parameters. Must not be `NULL`.

**Return value:**

- `XWORK_OK`: Cancellation status written successfully.
- `XWORK_ERROR_INVALID_ARGUMENT`: run is empty.
- `XWORK_ERROR_INVALID_STATE`: Current status does not allow cancellation.

**Resource ownership:**

Resource ownership is not transferred.

**Additional Note:**

- Asynchronous execution cancellation should take precedence by calling `xwork_run_async_cancel`.

**Example code:**

```c
xwork_status status = xwork_run_cancel(run);
```

**Related API:**

- `xwork_run_async_cancel`
- `xwork_run_fail`

---

### xwork_run_fail

Mark run as failed.

**Function:**

After using for unrecoverable errors, set run to terminal failed.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_fail(xwork_run *pRun);
```

**parameter:**

- `pRun`: input/output parameters. Must not be `NULL`.

**Return value:**

- `XWORK_OK`: Failure status written successfully.
- `XWORK_ERROR_INVALID_ARGUMENT`: run is empty.
- `XWORK_ERROR_INVALID_STATE`: Failed transitions are not allowed in the current state.

**Resource ownership:**

Resource ownership is not transferred.

**Additional Note:**

- A run failed event will be logged on success.

**Example code:**

```c
xwork_status status = xwork_run_fail(run);
```

**Related API:**

- `xwork_run_complete`
- `xwork_run_cancel`

---

### xwork_run_get_id

Get run id.

**Function:**

Used for logging, UI, persistent index or parent-child task association.

**Function prototype:**

```c
XWORK_API const char *xwork_run_get_id(const xwork_run *pRun);
```

**parameter:**

- `pRun`: input parameters. Can be `NULL`.

**Return value:**

Returns the borrowed run id; returns `NULL` when `pRun` is `NULL`.

**Resource ownership:**

The return value is owned by run and cannot be released by the caller.

**Additional Note:**

- The pointer becomes invalid after destruction by run.

**Example code:**

```c
const char *id = xwork_run_get_id(run);
```

**Related API:**

- `xwork_run_get_instruction`

---

### xwork_run_get_instruction

Get the run instruction.

**Function:**

Read the user task instructions copied when creating the run.

**Function prototype:**

```c
XWORK_API const char *xwork_run_get_instruction(const xwork_run *pRun);
```

**parameter:**

- `pRun`: input parameters. Can be `NULL`.

**Return value:**

Returns the borrowed instruction; returns `NULL` when `pRun` is `NULL`.

**Resource ownership:**

The return value is owned by run and cannot be released by the caller.

**Additional Note:**

- Do not modify the return string.

**Example code:**

```c
const char *text = xwork_run_get_instruction(run);
```

**Related API:**

- `xwork_run_get_id`

---

### xwork_run_get_state

Get the current status of run.

**Function:**

Used for UI status display, scheduling judgment or test assertion.

**Function prototype:**

```c
XWORK_API xwork_run_state xwork_run_get_state(const xwork_run *pRun);
```

**parameter:**

- `pRun`: input parameters. Can be `NULL`.

**Return value:**

Returns the current state; returns `XWORK_RUN_FAILED` when `pRun` is `NULL`.

**Resource ownership:**

No pointer is returned and ownership is not transferred.

**Additional Note:**

- The status only represents the current memory status of run and is not equivalent to the historical status in persistence.

**Example code:**

```c
xwork_run_state state = xwork_run_get_state(run);
```

**Related API:**

- `xwork_run_start`
- `xwork_run_complete`

---

### xwork_run_get_autonomy

Gets the run autonomy mode.

**Function:**

Read the run configuration when it is created or the autonomy after the profile is applied.

**Function prototype:**

```c
XWORK_API xwork_autonomy_mode xwork_run_get_autonomy(const xwork_run *pRun);
```

**parameter:**

- `pRun`: input parameters. Can be `NULL`.

**Return value:**

Returns run autonomy; returns `XWORK_AUTONOMY_MANUAL` when `pRun` is `NULL`.

**Resource ownership:**

Does not return a pointer.

**Additional Note:**

- autonomy affects the default behavior of policy/orchestrator but does not replace product-level security policies.

**Example code:**

```c
xwork_autonomy_mode mode = xwork_run_get_autonomy(run);
```

**Related API:**

- `xwork_profile_apply_run_options`

---

### xwork_run_get_workspace_count

Get the number of workspaces referenced by run.

**Function:**

Workspace id used to traverse run bindings.

**Function prototype:**

```c
XWORK_API size_t xwork_run_get_workspace_count(const xwork_run *pRun);
```

**parameter:**

- `pRun`: input parameters. Can be `NULL`.

**Return value:**

Returns the number of workspace ids; when `pRun` is `NULL`, returns `0`.

**Resource ownership:**

Does not return a pointer.

**Additional Note:**

- Returns the number of workspace ids copied when the run was created.

**Example code:**

```c
size_t count = xwork_run_get_workspace_count(run);
```

**Related API:**

- `xwork_run_get_workspace_id`

---

### xwork_run_get_workspace_id

Get the workspace id of run by index.

**Function:**

Used to traverse the workspace id referenced by run and resolve to the runtime workspace.

**Function prototype:**

```c
XWORK_API const char *xwork_run_get_workspace_id(const xwork_run *pRun, size_t iIndex);
```

**parameter:**

- `pRun`: input parameters. Can be `NULL`.
- `iIndex`: input parameters. 0-based index, must be less than workspace count.

**Return value:**

Returns the borrowed workspace id; returns `NULL` when the parameter is invalid or out of bounds.

**Resource ownership:**

The return value is owned by run and cannot be released by the caller.

**Additional Note:**

- The id can be resolved using `xwork_runtime_find_workspace`.

**Example code:**

```c
const char *workspace_id = xwork_run_get_workspace_id(run, 0u);
```

**Related API:**

- `xwork_run_get_workspace_count`
- `xwork_runtime_find_workspace`

---

### xwork_run_get_summary

Get run summary.

**Function:**

Copies a run's summary information for use by the UI, logging, or persistence.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_get_summary(const xwork_run *pRun, xwork_run_summary *pSummary);
```

**parameter:**

- `pRun`: input parameters. Must not be `NULL`.
- `pSummary`: Output parameter. Must not be `NULL`, it is recommended to be init.

**Return value:**

- `XWORK_OK`: Copy successful.
- `XWORK_ERROR_INVALID_ARGUMENT`: Invalid parameter.
- `XWORK_ERROR_NO_MEMORY`: String copy failed.

**Resource ownership:**

`pSummary` receives a copy of an owned string, the caller must `xwork_run_summary_reset`.

**Additional Note:**

- If summary already holds data before calling, the function will be reset and overwritten.

**Example code:**

```c
xwork_run_summary summary;
xwork_run_summary_init(&summary);
if (xwork_run_get_summary(run, &summary) == XWORK_OK) {
    xwork_run_summary_reset(&summary);
}
```

**Related API:**

- `xwork_run_summary_init`
- `xwork_run_summary_reset`

---

### xwork_run_get_snapshot

Get run snapshot.

**Function:**

Copy a run's serializable state for persistence, recovery, or testing.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_get_snapshot(
    const xwork_run *pRun,
    xwork_run_snapshot *pSnapshot
);
```

**parameter:**

- `pRun`: input parameters. Must not be `NULL`.
- `pSnapshot`: Output parameter. Must not be `NULL`, it is recommended to be init.

**Return value:**

- `XWORK_OK`: Copy successful.
- `XWORK_ERROR_INVALID_ARGUMENT`: Invalid parameter.
- `XWORK_ERROR_NO_MEMORY`: Copy failed.

**Resource ownership:**

snapshot gets the owned field, the caller must `xwork_run_snapshot_reset`.

**Additional Note:**

- Snapshot does not include live model sessions, threads, processes and callback stacks.

**Example code:**

```c
xwork_run_snapshot snapshot;
xwork_run_snapshot_init(&snapshot);
xwork_run_get_snapshot(run, &snapshot);
xwork_run_snapshot_reset(&snapshot);
```

**Related API:**

- `xwork_run_snapshot_init`
- `xwork_runtime_recover_run`

---

### xwork_run_get_last_event

Get the last event.

**Function:**

Read the most recently logged events from run.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_get_last_event(const xwork_run *pRun, xwork_event *pEvent);
```

**parameter:**

- `pRun`: input parameters. Must not be `NULL`.
- `pEvent`: Output parameter. Must not be `NULL`.

**Return value:**

- `XWORK_OK`: Copy successful.
- `XWORK_ERROR_INVALID_ARGUMENT`: Invalid parameter.
- `XWORK_ERROR_NOT_FOUND`: No event yet.
- `XWORK_ERROR_NO_MEMORY`: Copy failed.

**Resource ownership:**

event receives an owned field, the caller must `xwork_event_reset`.

**Additional Note:**

- There is usually an `RUN_CREATED` event after creating the run.

**Example code:**

```c
xwork_event event;
xwork_event_init(&event);
xwork_run_get_last_event(run, &event);
xwork_event_reset(&event);
```

**Related API:**

- `xwork_run_get_event`
- `xwork_event_reset`

---

### xwork_run_get_last_approval_request

Get the last approval request.

**Function:**

Read the latest approval request for UI display or resume the approval process.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_get_last_approval_request(
    const xwork_run *pRun,
    xwork_approval_request *pRequest
);
```

**parameter:**

- `pRun`: input parameters. Must not be `NULL`.
- `pRequest`: Output parameter. Must not be `NULL`.

**Return value:**

- `XWORK_OK`: Copy successful.
- `XWORK_ERROR_INVALID_ARGUMENT`: Invalid parameter.
- `XWORK_ERROR_NOT_FOUND`: No approval requested.
- `XWORK_ERROR_NO_MEMORY`: Copy failed.

**Resource ownership:**

request receives the owned field, the caller must `xwork_approval_request_reset`.

**Additional Note:**

- Approval status updated by `xwork_run_submit_approval`.

**Example code:**

```c
xwork_approval_request request;
xwork_approval_request_init(&request);
xwork_run_get_last_approval_request(run, &request);
xwork_approval_request_reset(&request);
```

**Related API:**

- `xwork_run_submit_approval`
- `xwork_approval_request_reset`

---

### xwork_run_get_last_checkpoint

Get the last checkpoint.

**Function:**

Read the latest checkpoint for UI, diagnostic or recovery prompts.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_get_last_checkpoint(
    const xwork_run *pRun,
    xwork_checkpoint *pCheckpoint
);
```

**parameter:**

- `pRun`: input parameters. Must not be `NULL`.
- `pCheckpoint`: Output parameter. Must not be `NULL`.

**Return value:**

- `XWORK_OK`: Copy successful.
- `XWORK_ERROR_INVALID_ARGUMENT`: Invalid parameter.
- `XWORK_ERROR_NOT_FOUND`: No checkpoint.
- `XWORK_ERROR_NO_MEMORY`: Copy failed.

**Resource ownership:**

checkpoint accepts an owned field, the caller must `xwork_checkpoint_reset`.

**Additional Note:**

- Checkpoint is a recoverable state and does not mean that live resources can be recovered.

**Example code:**

```c
xwork_checkpoint checkpoint;
xwork_checkpoint_init(&checkpoint);
xwork_run_get_last_checkpoint(run, &checkpoint);
xwork_checkpoint_reset(&checkpoint);
```

**Related API:**

- `xwork_run_get_checkpoint`
- `xwork_checkpoint_reset`

---

### xwork_run_get_last_memory_context

Get the last memory context.

**Function:**

Read the memory context text and workspace number recently parsed by the orchestrator.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_get_last_memory_context(
    const xwork_run *pRun,
    xwork_memory_context *pContext
);
```

**parameter:**

- `pRun`: input parameters. Must not be `NULL`.
- `pContext`: Output parameter. Must not be `NULL`, it is recommended to be init.

**Return value:**

- `XWORK_OK`: Copy successful.
- `XWORK_ERROR_INVALID_ARGUMENT`: Invalid parameter.
- `XWORK_ERROR_NOT_FOUND`: No memory context.
- `XWORK_ERROR_NO_MEMORY`: Copy failed.

**Resource ownership:**

context receives a copy of owned `sText`, the caller must `xwork_memory_context_reset`.

**Additional Note:**

- The memory context is not equivalent to the workspace memory object itself.

**Example code:**

```c
xwork_memory_context context;
xwork_memory_context_init(&context);
xwork_run_get_last_memory_context(run, &context);
xwork_memory_context_reset(&context);
```

**Related API:**

- `xwork_memory_context_init`
- `xwork_memory_context_reset`

---

### xwork_run_get_event_count

Get the number of events.

**Function:**

Used to traverse the run event log.

**Function prototype:**

```c
XWORK_API size_t xwork_run_get_event_count(const xwork_run *pRun);
```

**parameter:**

- `pRun`: input parameters. Can be `NULL`.

**Return value:**

Returns the number of events; returns `0` if run is empty.

**Resource ownership:**

Does not return a pointer.

**Additional Note:**

- event index is 0-based.

**Example code:**

```c
size_t count = xwork_run_get_event_count(run);
```

**Related API:**

- `xwork_run_get_event`

---

### xwork_run_get_event

Get event by index.

**Function:**

Copies the specified event log entry.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_get_event(
    const xwork_run *pRun,
    size_t iIndex,
    xwork_event *pEvent
);
```

**parameter:**

- `pRun`: input parameters. Must not be `NULL`.
- `iIndex`: input parameters. 0-based event index.
- `pEvent`: Output parameter. Must not be `NULL`.

**Return value:**

- `XWORK_OK`: Copy successful.
- `XWORK_ERROR_INVALID_ARGUMENT`: Invalid parameter.
- `XWORK_ERROR_NOT_FOUND`: Index out of bounds.
- `XWORK_ERROR_NO_MEMORY`: Copy failed.

**Resource ownership:**

event receives an owned field, which the caller must reset.

**Additional Note:**

- Read `xwork_run_get_event_count` before traversing.

**Example code:**

```c
xwork_event event;
xwork_event_init(&event);
xwork_run_get_event(run, 0u, &event);
xwork_event_reset(&event);
```

**Related API:**

- `xwork_run_get_event_count`

---

### xwork_run_get_step_count

Get the number of steps.

**Function:**

Returns the number of events that can be projected into step.

**Function prototype:**

```c
XWORK_API size_t xwork_run_get_step_count(const xwork_run *pRun);
```

**parameter:**

- `pRun`: input parameters. Can be `NULL`.

**Return value:**

Returns the number of steps; returns `0` if run is empty.

**Resource ownership:**

Does not return a pointer.

**Additional Note:**

- The current implementation uses event count as step count.

**Example code:**

```c
size_t count = xwork_run_get_step_count(run);
```

**Related API:**

- `xwork_run_get_step`

---

### xwork_run_get_step

Get step by index.

**Function:**

Project events and correlatable checkpoints into UI/query friendly steps.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_get_step(
    const xwork_run *pRun,
    size_t iIndex,
    xwork_run_step *pStep
);
```

**parameter:**

- `pRun`: input parameters. Must not be `NULL`.
- `iIndex`: input parameters. 0-based index.
- `pStep`: Output parameter. Must not be `NULL`.

**Return value:**

- `XWORK_OK`: Copy successful.
- `XWORK_ERROR_INVALID_ARGUMENT`: Invalid parameter.
- `XWORK_ERROR_NOT_FOUND`: Index out of bounds.
- `XWORK_ERROR_NO_MEMORY`: Copy failed.

**Resource ownership:**

step receives the owned field and the caller must reset it.

**Additional Note:**

- step is not an independent storage object, but derived from event/checkpoint.

**Example code:**

```c
xwork_run_step step;
xwork_run_step_init(&step);
xwork_run_get_step(run, 0u, &step);
xwork_run_step_reset(&step);
```

**Related API:**

- `xwork_run_get_step_count`
- `xwork_run_query_steps`

---

### xwork_run_query_steps

Query the step list.

**Function:**

Filter the run step by query criteria and return the list.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_query_steps(
    const xwork_run *pRun,
    const xwork_run_step_query *pQuery,
    xwork_run_step_list *pList
);
```

**parameter:**

- `pRun`: input parameters. Must not be `NULL`.
- `pQuery`: input parameters. Can be `NULL`; no filtering when `NULL`.
- `pList`: Output parameter. Must not be `NULL`, it is recommended to be init.

**Return value:**

- `XWORK_OK`: Query successful.
- `XWORK_ERROR_INVALID_ARGUMENT`: Invalid parameter.
- `XWORK_ERROR_NO_MEMORY`: List allocation or field copy failed.

**Resource ownership:**

list receives owned `pItems` and each step field, the caller must `xwork_run_step_list_reset`.

**Additional Note:**

- `iLimit` sets `bHasMore` and `iNextAfterSequence` when hit.

**Example code:**

```c
xwork_run_step_query query;
xwork_run_step_list list;
xwork_run_step_query_init(&query);
xwork_run_step_list_init(&list);
query.iLimit = 10u;
xwork_run_query_steps(run, &query, &list);
xwork_run_step_list_reset(&list);
```

**Related API:**

- `xwork_run_step_query_init`
- `xwork_run_step_list_reset`

---

### xwork_run_get_checkpoint_count

Get the checkpoint number.

**Function:**

Used to traverse run recorded checkpoints.

**Function prototype:**

```c
XWORK_API size_t xwork_run_get_checkpoint_count(const xwork_run *pRun);
```

**parameter:**

- `pRun`: input parameters. Can be `NULL`.

**Return value:**

Returns the number of checkpoints; returns `0` when run is empty.

**Resource ownership:**

Does not return a pointer.

**Additional Note:**

- checkpoint index is 0-based.

**Example code:**

```c
size_t count = xwork_run_get_checkpoint_count(run);
```

**Related API:**

- `xwork_run_get_checkpoint`

---

### xwork_run_get_checkpoint

Get checkpoint by index.

**Function:**

Copy the checkpoint specified in run.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_get_checkpoint(
    const xwork_run *pRun,
    size_t iIndex,
    xwork_checkpoint *pCheckpoint
);
```

**parameter:**

- `pRun`: input parameters. Must not be `NULL`.
- `iIndex`: input parameters. 0-based checkpoint index.
- `pCheckpoint`: Output parameter. Must not be `NULL`.

**Return value:**

- `XWORK_OK`: Copy successful.
- `XWORK_ERROR_INVALID_ARGUMENT`: Invalid parameter.
- `XWORK_ERROR_NOT_FOUND`: Index out of bounds.
- `XWORK_ERROR_NO_MEMORY`: Copy failed.

**Resource ownership:**

checkpoint receives the owned field and the caller must reset it.

**Additional Note:**

- If the checkpoint id is known, `xwork_run_load_checkpoint` can be used to select the recovery target.

**Example code:**

```c
xwork_checkpoint checkpoint;
xwork_checkpoint_init(&checkpoint);
xwork_run_get_checkpoint(run, 0u, &checkpoint);
xwork_checkpoint_reset(&checkpoint);
```

**Related API:**

- `xwork_run_get_checkpoint_count`
- `xwork_run_load_checkpoint`

---

### xwork_run_get_artifact_count

Get the artifact quantity.

**Function:**

Used to traverse the artifacts emitted within the run.

**Function prototype:**

```c
XWORK_API size_t xwork_run_get_artifact_count(const xwork_run *pRun);
```

**parameter:**

- `pRun`: input parameters. Can be `NULL`.

**Return value:**

Returns the artifact quantity; returns `0` if run is empty.

**Resource ownership:**

Does not return a pointer.

**Additional Note:**

- The artifact emit API is described further in the artifact documentation.

**Example code:**

```c
size_t count = xwork_run_get_artifact_count(run);
```

**Related API:**

- `xwork_run_get_artifact`
- `xwork_run_emit_artifact`

---

### xwork_run_get_artifact

Get artifact by index.

**Function:**

Copies the artifact specified in run.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_get_artifact(
    const xwork_run *pRun,
    size_t iIndex,
    xwork_artifact *pArtifact
);
```

**parameter:**

- `pRun`: input parameters. Must not be `NULL`.
- `iIndex`: input parameters. 0-based artifact index.
- `pArtifact`: Output parameter. Must be other than `NULL`, it is recommended to be init.

**Return value:**

- `XWORK_OK`: Copy successful.
- `XWORK_ERROR_INVALID_ARGUMENT`: Invalid parameter.
- `XWORK_ERROR_NOT_FOUND`: Index out of bounds.
- `XWORK_ERROR_NO_MEMORY`: Copy failed.

**Resource ownership:**

The artifact receives an owned field and the caller must `xwork_artifact_reset`.

**Additional Note:**

- Whether large content is inlined depends on artifact creation options and persistence backend.

**Example code:**

```c
xwork_artifact artifact;
xwork_artifact_init(&artifact);
xwork_run_get_artifact(run, 0u, &artifact);
xwork_artifact_reset(&artifact);
```

**Related API:**

- `xwork_run_get_artifact_count`
- `xwork_artifact_reset`

---

### xwork_run_get_last_output_text

Get the last model output text.

**Function:**

Read the last output text recorded in run, suitable for quick UI display.

**Function prototype:**

```c
XWORK_API const char *xwork_run_get_last_output_text(const xwork_run *pRun);
```

**parameter:**

- `pRun`: input parameters. Can be `NULL`.

**Return value:**

Returns a borrowed string; returns `NULL` if run is empty or there is no output yet.

**Resource ownership:**

The return value is owned by run and cannot be released by the caller.

**Additional Note:**

- To persist the complete output, use the artifact or persistence API.

**Example code:**

```c
const char *text = xwork_run_get_last_output_text(run);
```

**Related API:**

- `xwork_run_execute`
- `xwork_run_emit_output_artifact`

---

### xwork_run_execute

Execute run synchronously.

**Function:**

Runs the minimal model-turn + tool-loop orchestrator until completed, paused, canceled or failed.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_execute(
    xwork_run *pRun,
    const xwork_orchestrator_options *pOptions
);
```

**parameter:**

- `pRun`: input/output parameters. Must be other than `NULL` and belong to a valid runtime.
- `pOptions`: input parameters. Can be `NULL`; `NULL` uses default orchestrator options.

**Return value:**

- `XWORK_OK`: Execution completed.
- `XWORK_ERROR_INVALID_ARGUMENT`: Invalid parameter or options.
- `XWORK_ERROR_INVALID_STATE`: run is not executable or reentrant in its current state.
- `XWORK_ERROR_PAUSED`: Execution stopped at approval/pause boundary.
- `XWORK_ERROR_CANCELLED`: Collaborative cancellation.
- `XWORK_ERROR_EXTERNAL_FAILURE`: Model, tool, host service or persistence failed.

**Resource ownership:**

Does not transfer run or options ownership. callback/user data/cancel token in options are managed by the caller.

**Additional Note:**

- Concurrent execution of the same run is not allowed.
- Event, checkpoint, artifact and summary status will be recorded during execution.

**Example code:**

```c
xwork_orchestrator_options options;
xwork_orchestrator_options_init(&options);
xwork_status status = xwork_run_execute(run, &options);
```

**Related API:**

- `xwork_orchestrator_options_init`
- `xwork_run_execute_async`

---

### xwork_run_execute_async

Run asynchronously.

**Function:**

Execute run in a background thread and return async handles for wait, timeout, status, and cancel.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_execute_async(
    xwork_run *pRun,
    const xwork_orchestrator_options *pOptions,
    xwork_run_async **ppAsync
);
```

**parameter:**

- `pRun`: input/output parameters. Must be other than `NULL` and belong to a valid runtime.
- `pOptions`: input parameters. Can be `NULL`; shallow-copy to async handle when not `NULL`.
- `ppAsync`: Output parameter. Must not be `NULL`. Receive owned async handle on success.

**Return value:**

- `XWORK_OK`: Background execution started.
- `XWORK_ERROR_INVALID_ARGUMENT`: Invalid parameter or orchestrator options.
- `XWORK_ERROR_NO_MEMORY`: Handle or cancel token allocation failed.
- `XWORK_ERROR_UNSUPPORTED`: The current platform does not support thread entry.
- `XWORK_ERROR_EXTERNAL_FAILURE`: Thread creation failed.

**Resource ownership:**

The async handle is owned by the caller and must be `xwork_run_async_destroy`. If a cancel token is not provided, the handle creates and owns an internal token.

**Additional Note:**

- options are shallow-copy, callback, user data, profile strings and caller-owned cancel token must live until handle is completed or destroyed.
- Do not start multiple async/sync executions simultaneously for the same run.

**Example code:**

```c
xwork_run_async *async = NULL;
if (xwork_run_execute_async(run, NULL, &async) == XWORK_OK) {
    (void)xwork_run_async_wait(async);
    xwork_run_async_destroy(async);
}
```

**Related API:**

- `xwork_run_async_wait`
- `xwork_run_async_cancel`
- `xwork_run_async_destroy`

---

### xwork_run_async_wait

Wait for the asynchronous run to complete.

**Function:**

Blocks until the background execution associated with the async handle ends and returns to the run execution status.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_async_wait(xwork_run_async *pAsync);
```

**parameter:**

- `pAsync`: input/output parameters. Must not be `NULL`.

**Return value:**

- Returns the final state of `xwork_run_execute` in the background.
- `XWORK_ERROR_INVALID_ARGUMENT`: handle is empty.
- `XWORK_ERROR_EXTERNAL_FAILURE`: Thread wait or status read exception.

**Resource ownership:**

The handle is not destroyed; the caller still needs `xwork_run_async_destroy`.

**Additional Note:**

- wait can be called separately from destroy.

**Example code:**

```c
xwork_status status = xwork_run_async_wait(async);
```

**Related API:**

- `xwork_run_async_wait_timeout`
- `xwork_run_async_destroy`

---

### xwork_run_async_wait_timeout

Wait for asynchronous run with timeout.

**Function:**

Waits for up to the specified number of milliseconds and tells the caller whether it is complete via `pbCompleted`.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_async_wait_timeout(
    xwork_run_async *pAsync,
    size_t iTimeoutMs,
    bool *pbCompleted
);
```

**parameter:**

- `pAsync`: input/output parameters. Must not be `NULL`.
- `iTimeoutMs`: input parameters. Timeout time in milliseconds.
- `pbCompleted`: Output parameter. Must not be `NULL`.

**Return value:**

- `XWORK_OK` and `*pbCompleted == false`: Wait timeout but not error.
- Returns to the background run final state on completion and sets `*pbCompleted == true`.
- `XWORK_ERROR_INVALID_ARGUMENT`: Invalid parameter.
- `XWORK_ERROR_EXTERNAL_FAILURE`: Thread wait or status read failed.

**Resource ownership:**

Do not destroy handle.

**Additional Note:**

- `xwork_run_async_cancel` can be called after a timeout.

**Example code:**

```c
bool completed = false;
xwork_status status = xwork_run_async_wait_timeout(async, 1000u, &completed);
```

**Related API:**

- `xwork_run_async_cancel`
- `xwork_run_async_wait`

---

### xwork_run_async_get_status

Read the current state of the async handle.

**Function:**

Non-blocking reading of background execution status and completion flags.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_async_get_status(
    const xwork_run_async *pAsync,
    xwork_status *pStatus,
    bool *pbCompleted
);
```

**parameter:**

- `pAsync`: input parameters. Must not be `NULL`.
- `pStatus`: Output parameter. Must not be `NULL`.
- `pbCompleted`: Output parameter. Must not be `NULL`.

**Return value:**

- `XWORK_OK`: read successfully.
- `XWORK_ERROR_INVALID_ARGUMENT`: Invalid parameter.

**Resource ownership:**

No transfer of title.

**Additional Note:**

- The returned status is the current recording status when not completed and does not represent the final result.

**Example code:**

```c
xwork_status run_status;
bool completed;
xwork_run_async_get_status(async, &run_status, &completed);
```

**Related API:**

- `xwork_run_async_wait`

---

### xwork_run_async_cancel

Requests cancellation of an asynchronous run.

**Function:**

Request background execution to stop as soon as possible through the async handle's cancel token and thread stop boundary.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_async_cancel(
    xwork_run_async *pAsync,
    const char *sReason
);
```

**parameter:**

- `pAsync`: input/output parameters. Must not be `NULL`.
- `sReason`: input parameters. Can be `NULL`; `NULL` uses the default cancellation reason.

**Return value:**

- `XWORK_OK`: Cancellation request issued or handle completed.
- `XWORK_ERROR_INVALID_ARGUMENT`: handle is empty.

**Resource ownership:**

The handle is not destroyed; the caller still needs to wait/destroy.

**Additional Note:**

- Cancellation is collaborative, and actual stopping relies on the model/tool/host service checking the cancel token.

**Example code:**

```c
xwork_run_async_cancel(async, "timeout");
```

**Related API:**

- `xwork_run_async_wait`
- `xwork_run_cancel`

---

### xwork_run_async_destroy

Destroy async handle.

**Function:**

Releases the async handle, thread object, internal cancel token, and lock resources.

**Function prototype:**

```c
XWORK_API void xwork_run_async_destroy(xwork_run_async *pAsync);
```

**parameter:**

- `pAsync`: input/destroy parameters. Can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases resources owned by handle; if handle created an internal cancel token, it will also be destroyed.

**Additional Note:**

- If the background has not been completed, destroy will first request cancellation and wait for the thread to end.
- destroy does not destroy the run.

**Example code:**

```c
xwork_run_async_destroy(async);
```

**Related API:**

- `xwork_run_execute_async`
- `xwork_run_async_cancel`

## Artifact Emission API

The following artifact emission functions belong to the emission entry point of the run API, but the artifact structure fields, schema, query and detailed semantics are described in [Artifact API](api-artifacts.md).

### xwork_run_emit_artifact

Emit generic artifacts.

**Function:**

Hang tools, models, or product layer products on run and include them in the event/persistence pipeline.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_emit_artifact(
    xwork_run *pRun,
    const xwork_artifact_options *pOptions,
    xwork_artifact *pArtifact
);
```

**parameter:**

- `pRun`: input/output parameters. Must not be `NULL`.
- `pOptions`: input parameters. Must not be `NULL`, see Artifact API for specific fields.
- `pArtifact`: Output parameter. Can be `NULL`; receives deep-copy artifact when not `NULL`.

**Return value:**

- `XWORK_OK`: issued successfully.
- `XWORK_ERROR_INVALID_ARGUMENT`: Invalid parameter or artifact options.
- `XWORK_ERROR_NO_MEMORY`: Copy/allocation failed.
- `XWORK_ERROR_EXTERNAL_FAILURE`: persistence write failed.

**Resource ownership:**

run saves a copy of the artifact; `pArtifact` If not `NULL`, the caller needs `xwork_artifact_reset`.

**Additional Note:**

- This function is the base path from which various typed artifacts emit functions.

**Example code:**

```c
xwork_artifact_options options;
xwork_artifact_options_init(&options);
/* fill options */
(void)xwork_run_emit_artifact(run, &options, NULL);
```

**Related API:**

- `xwork_artifact_options_init`
- `xwork_run_get_artifact`

---

### xwork_run_emit_patch_artifact

Issue the patch artifact.

**Function:**

Log patch, apply result, and file-level patch summary.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_emit_patch_artifact(
    xwork_run *pRun,
    const xwork_patch_artifact_options *pOptions,
    xwork_artifact *pArtifact
);
```

**parameter:**

- `pRun`: Must not be `NULL`.
- `pOptions`: Must not be `NULL`, see Artifact API for fields.
- `pArtifact`: Can be `NULL`; receives a copy when not `NULL`.

**Return value:**

Returns `XWORK_OK` or the error code of the artifact's path.

**Resource ownership:**

run saves a copy; the output artifact is reset by the caller.

**Additional Note:**

- Suitable for `filesystem.apply_patch` or code editing tools to record structured products.

**Example code:**

```c
xwork_patch_artifact_options options;
xwork_patch_artifact_options_init(&options);
(void)xwork_run_emit_patch_artifact(run, &options, NULL);
```

**Related API:**

- `xwork_patch_artifact_options_init`
- `XWORK_PATCH_APPLY_RESULT_SCHEMA_V1`

---

### xwork_run_emit_report_artifact

Issue report artifact.

**Function:**

Record diagnostics, summaries, test reports, or replay reports.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_emit_report_artifact(
    xwork_run *pRun,
    const xwork_report_artifact_options *pOptions,
    xwork_artifact *pArtifact
);
```

**parameter:**

- `pRun`: Must not be `NULL`.
- `pOptions`: must not be `NULL`.
- `pArtifact`: Can be `NULL`.

**Return value:**

Returns `XWORK_OK` or the error code of the artifact's path.

**Resource ownership:**

run saves a copy; the output artifact is reset by the caller.

**Additional Note:**

- For details on report classification, subject ref and schema, see Artifact API.

**Example code:**

```c
xwork_report_artifact_options options;
xwork_report_artifact_options_init(&options);
(void)xwork_run_emit_report_artifact(run, &options, NULL);
```

**Related API:**

- `xwork_report_artifact_options_init`
- `XWORK_REPORT_SCHEMA_V1`

---

### xwork_run_emit_output_artifact

Emit output artifacts.

**Function:**

Log model output, tool output, or user-visible content.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_emit_output_artifact(
    xwork_run *pRun,
    const xwork_output_artifact_options *pOptions,
    xwork_artifact *pArtifact
);
```

**parameter:**

- `pRun`: Must not be `NULL`.
- `pOptions`: must not be `NULL`.
- `pArtifact`: Can be `NULL`.

**Return value:**

Returns `XWORK_OK` or the error code of the artifact's path.

**Resource ownership:**

run saves a copy; the output artifact is reset by the caller.

**Additional Note:**

- For details on output role and content stats, see Artifact API.

**Example code:**

```c
xwork_output_artifact_options options;
xwork_output_artifact_options_init(&options);
(void)xwork_run_emit_output_artifact(run, &options, NULL);
```

**Related API:**

- `xwork_output_artifact_options_init`
- `xwork_run_get_last_output_text`

---

### xwork_run_emit_command_artifact

Issue command artifact.

**Function:**

Log command execution, stdout/stderr statistics, and terminal/process results.

**Function prototype:**

```c
XWORK_API xwork_status xwork_run_emit_command_artifact(
    xwork_run *pRun,
    const xwork_command_artifact_options *pOptions,
    xwork_artifact *pArtifact
);
```

**parameter:**

- `pRun`: Must not be `NULL`.
- `pOptions`: must not be `NULL`.
- `pArtifact`: Can be `NULL`.

**Return value:**

Returns `XWORK_OK` or the error code of the artifact's path.

**Resource ownership:**

run saves a copy; the output artifact is reset by the caller.

**Additional Note:**

- Suitable for process/terminal host tools to record structured execution results.

**Example code:**

```c
xwork_command_artifact_options options;
xwork_command_artifact_options_init(&options);
(void)xwork_run_emit_command_artifact(run, &options, NULL);
```

**Related API:**

- `xwork_command_artifact_options_init`
- `XWORK_TERMINAL_STATE_SCHEMA_V1`

## Error handling

- `XWORK_ERROR_INVALID_ARGUMENT`: Invalid run/options/output parameter.
- `XWORK_ERROR_NOT_FOUND`: workspace, event, checkpoint, artifact, or persisted object does not exist.
- `XWORK_ERROR_INVALID_STATE`: The life cycle status does not allow the operation, or the same run can be re-entered.
- `XWORK_ERROR_PAUSED`: Execution is paused at resumable boundaries such as approval or side-effect-blocking.
- `XWORK_ERROR_CANCELLED`: Collaborative cancellation.
- `XWORK_ERROR_EXTERNAL_FAILURE`: Model, host service, thread, persistence, or underlying dependency failed.

## Restore boundaries

Run snapshot can restore the run's serializable status, workspace id, pending tool, approval decision, last checkpoint, artifact metadata, etc. Live processes, terminal sessions, threads, model sessions, and callback stacks are not restored.

## Thread boundaries

Concurrent executions or concurrent mutations are not allowed within the same run. `xwork_run_async_*` only synchronizes the async handle state; other accesses to run itself still require the caller to comply with life cycle boundaries.

## Related documents

- [Orchestrator API](api-orchestrator.md)
- [Artifact API](api-artifacts.md)
- [Persistence, Checkpoints, and Replay](../guide/persistence-replay-intro.md)
- [First xwork Program](../guide/first-xwork-program.md)
