# xwork Multi-Agent Contract

This document fixes the public multi-agent ownership, lifetime, thread-safety,
scheduling, recovery, and event boundaries for `xwork`.

## Object Ownership

- `xwork_agent_pool_create()` returns an owned pool. Release it with
  `xwork_agent_pool_destroy()`.
- An agent pool borrows `xwork_runtime`; the runtime must outlive the pool and
  every graph created from that pool.
- `xwork_agent_pool_add_agent()` copies agent option strings. Returned
  `xwork_agent *` values are borrowed from the pool and become invalid when the
  pool is destroyed.
- `xwork_task_graph_create()` returns an owned graph. Release it with
  `xwork_task_graph_destroy()`.
- A task graph borrows its agent pool, cancel token, execute callback, and
  callback user data. Those values must remain valid while the graph can execute
  or query them.
- `xwork_task_graph_add_node()` copies task ids, instructions, profile ids,
  session ids, workspace ids, dependency ids, and handoff metadata.
- Task node `pUserData` is borrowed. Callers own synchronization and lifetime
  for any object referenced by task user data.
- Child `xwork_run` objects are allocated under the borrowed runtime. The graph
  stores task-to-run mappings but does not own the runtime.
- Summary, snapshot, list, handoff, and result structs returned by query APIs own
  their copied strings/arrays where the corresponding reset function documents
  it. Always call the matching reset function.

## Thread Safety

- Agent pools are not a concurrent mutation container. Serialize add/remove or
  snapshot operations that touch the same pool.
- A task graph protects execution re-entry with an internal lock, but graph
  structure mutation should be serialized with execution and query calls by the
  caller.
- Task execute callbacks may run concurrently up to graph max concurrency. A
  callback must protect shared `pUserData`, workspace state, and external
  resources that it touches.
- Callbacks must honor cancellation by checking the graph/run cancel state and by
  returning a terminal error when work cannot continue safely.
- `xwork_runtime` and `xwork_run` keep their own public lifetime rules. Multi
  agent code does not make borrowed runtime-owned objects independent or
  thread-local.

## Scheduling Semantics

- A task becomes ready when all required dependencies have reached a compatible
  terminal state for the configured dependency policy.
- `XWORK_TASK_DEPENDENCY_FAIL_FAST` fails dependents after a required dependency
  failure.
- `XWORK_TASK_DEPENDENCY_REQUIRE_ALL` requires all dependencies to complete
  successfully before a dependent task is ready.
- `XWORK_TASK_DEPENDENCY_BEST_EFFORT` allows dependents to run after dependency
  failures and leaves failure handling to the task callback.
- Max concurrency limits the number of concurrently executing task callbacks.
- Per-agent retry is implemented by the graph scheduler. Per-agent max turns and
  timeout are copied to task summaries/snapshots. When no custom task execute
  callback is configured, the graph default executor maps max turns to
  `xwork_orchestrator_options::iMaxTurns` and enforces timeout through async
  run wait/cancel.
- Pause leaves unfinished ready/running work resumable. Cancel marks unfinished
  work as cancelled or failed according to the current execution point.

## Handoff And Shared Context

- Handoff requests/results are copied into graph state and snapshots.
- Artifact refs and memory context refs are stable identifiers; the graph does
  not own the underlying artifact or memory storage.
- Shared workspace access is controlled by workspace policy and host-tool policy.
  Multi-agent scheduling does not bypass workspace path enforcement.
- Writable shared workspace state is intentionally non-deterministic unless the
  caller serializes writes or records the effects through replay/persistence.

## Events And Query Boundary

- Agent/task/scheduler event kinds are public and persisted through run events
  where an execution path creates the event.
- Task scheduled, started, joined, completed, failed, and cancelled states are
  queryable through task graph summary/snapshot APIs.
- Tasks that wait on dependencies record `XWORK_EVENT_TASK_BLOCKED` on their
  child run. When dependencies become ready, they record
  `XWORK_EVENT_TASK_UNBLOCKED` before execution starts.
- Aggregate report artifacts summarize task outputs; they are not a substitute
  for per-run event and artifact audit data.

## Persistence And Recovery

- Agent pool and task graph snapshots are persistence objects with copied string
  fields.
- Recovery preserves terminal task state and handoff metadata.
- READY, RUNNING, and BLOCKED live in-flight states recover as PENDING so they
  can be re-executed. Native threads and live callback stacks are never restored.
- Graph recovery requires the caller to provide a compatible runtime, pool,
  execute callback, policy, and host environment.
- Recovered graphs must assume external side effects may already have happened.
  Use replay entries, idempotent tools, or caller-owned locks for exactly-once
  behavior.
