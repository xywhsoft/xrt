# Multi-Agent API

The Multi-Agent API provides in-process agent pool, task graph, dependency scheduling, handoff, child run mapping, pause/resume/cancel and snapshot recovery capabilities. It is oriented to scenarios such as AI IDE, claw, and automated R&D pipelines, and is used to organize multiple role agents into auditable and recoverable task graphs.

## Module boundaries

- The agent pool manages agent definitions and does not perform model inference.
- task graph manages task nodes, dependencies, concurrency limits, failure strategies, handoff and execution status.
- Each task will be mapped to `xwork_run` when executed, and the actual model-turn and tool-loop are completed by the run/orchestrator layer.
- Snapshot only restores the serializable state; native thread, callback stack, and external live handle will not be restored.
- Graph mutation should be serialized with execute/query; if task callback runs concurrently, it needs to protect shared resources by itself.

## Ownership agreement

| Object | Ownership |
| --- | --- |
| `xwork_agent_pool_create` | Returns the owned pool and the caller uses `xwork_agent_pool_destroy` to release it. |
| `xwork_agent_pool_add_agent` | pool copies the agent options string and returns the borrowed agent. |
| `xwork_task_graph_create` | Returns the owned graph and the caller releases it with |
| `xwork_task_graph` | Borrow agent pool, cancel token, execute callback and callback user data. |
| `xwork_task_graph_get_snapshot` | Deep copy snapshot content, the caller uses `xwork_task_graph_snapshot_reset` to release it. |
| `*_list_reset` / `*_snapshot_reset` | Free API allocated strings, arrays and nested lists. |

## Typical calling sequence

```text
xwork_agent_pool_options_init
xwork_agent_pool_create
xwork_agent_options_init
xwork_agent_pool_add_agent
xwork_task_graph_options_init
xwork_task_graph_create
xwork_task_node_options_init
xwork_task_graph_add_node
xwork_task_graph_add_dependency
xwork_task_graph_execute
xwork_task_graph_get_snapshot
xwork_task_graph_destroy
xwork_agent_pool_destroy
```

## Initialization and release convention

All `*_init` functions allow `NULL` to be passed in and do nothing at this time. All `*_reset` functions also allow `NULL` to be passed in and will revert to the post-init state after release. Before calling the API to obtain summary/list/snapshot, it is recommended to init first; reset before reusing the same structure to avoid leaking old content.

## Agent Pool and Agent

### xwork_agent_pool_options_init

Initialize `xwork_agent_pool_options`.

**Function:**

Clear the pool options to prepare for creating the agent pool.

**Function prototype:**

```c
XWORK_API void xwork_agent_pool_options_init(xwork_agent_pool_options *pOptions);
```

**parameter:**

- `pOptions`: options to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated; the caller still owns the structure itself.

**Additional Note:**

- `pRuntime` must be set before creating pool.
- When `sPoolId` is empty, the current implementation uses `"default"`.

**Example code:**

```c
xwork_agent_pool_options opts;
xwork_agent_pool_options_init(&opts);
opts.sPoolId = "main";
opts.pRuntime = runtime;
```

**Related API:**

- `xwork_agent_pool_create`

---

### xwork_agent_options_init

Initialize `xwork_agent_options`.

**Function:**

Set the default value of agent options for registering planner, coder, reviewer and other agents.

**Function prototype:**

```c
XWORK_API void xwork_agent_options_init(xwork_agent_options *pOptions);
```

**parameter:**

- `pOptions`: options to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated; the string field is provided by the caller during the `xwork_agent_pool_add_agent` call, and the pool copies the value that needs to be retained.

**Additional Note:**

- Default role is `XWORK_AGENT_ROLE_CUSTOM`.
- Default autonomous mode is `XWORK_AUTONOMY_SEMI_AUTO`.

**Example code:**

```c
xwork_agent_options opts;
xwork_agent_options_init(&opts);
opts.sAgentId = "coder";
opts.eRole = XWORK_AGENT_ROLE_CODER;
```

**Related API:**

- `xwork_agent_pool_add_agent`

---

### xwork_agent_snapshot_init

Initialize `xwork_agent_snapshot`.

**Function:**

Prepare an agent snapshot that can be populated by the snapshot API or constructed manually.

**Function prototype:**

```c
XWORK_API void xwork_agent_snapshot_init(xwork_agent_snapshot *pSnapshot);
```

**parameter:**

- `pSnapshot`: snapshot to be initialized; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

The default role and autonomy mode are consistent with `xwork_agent_options_init`.

**Example code:**

```c
xwork_agent_snapshot snapshot;
xwork_agent_snapshot_init(&snapshot);
```

**Related API:**

- `xwork_agent_snapshot_reset`
- `xwork_agent_pool_get_snapshot`

---

### xwork_agent_snapshot_reset

Release and reset `xwork_agent_snapshot`.

**Function:**

Release the snapshot internal string and restore it to the init state.

**Function prototype:**

```c
XWORK_API void xwork_agent_snapshot_reset(xwork_agent_snapshot *pSnapshot);
```

**parameter:**

- `pSnapshot`: snapshot to reset; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases the deep copy fields held by snapshot without releasing the structure itself.

**Additional Note:**

Only call reset on snapshots generated by the xwork API or constructed with the same ownership rules.

**Example code:**

```c
xwork_agent_snapshot_reset(&snapshot);
```

**Related API:**

- `xwork_agent_snapshot_init`

---

### xwork_agent_snapshot_list_init

Initialize the agent snapshot list.

**Function:**

Set the list to empty so that the snapshot API can populate it.

**Function prototype:**

```c
XWORK_API void xwork_agent_snapshot_list_init(xwork_agent_snapshot_list *pList);
```

**parameter:**

- `pList`: List to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

List elements are usually generated by `xwork_agent_pool_get_snapshot` deep copy.

**Example code:**

```c
xwork_agent_snapshot_list list;
xwork_agent_snapshot_list_init(&list);
```

**Related API:**

- `xwork_agent_snapshot_list_reset`

---

### xwork_agent_snapshot_list_reset

Release the agent snapshot list.

**Function:**

Release each agent snapshot and list array in the list.

**Function prototype:**

```c
XWORK_API void xwork_agent_snapshot_list_reset(xwork_agent_snapshot_list *pList);
```

**parameter:**

- `pList`: List to free; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases the elements and arrays owned by the list, but does not release the list structure itself.

**Additional Note:**

After the call, the list returns to the empty state and can be passed to the filling API again.

**Example code:**

```c
xwork_agent_snapshot_list_reset(&list);
```

**Related API:**

- `xwork_agent_snapshot_reset`

---

### xwork_agent_pool_snapshot_init

Initialize agent pool snapshot.

**Function:**

Prepare a pool snapshot for persisting or restoring the agent pool.

**Function prototype:**

```c
XWORK_API void xwork_agent_pool_snapshot_init(xwork_agent_pool_snapshot *pSnapshot);
```

**parameter:**

- `pSnapshot`: snapshot to be initialized; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

`tAgents` is initialized internally.

**Example code:**

```c
xwork_agent_pool_snapshot snapshot;
xwork_agent_pool_snapshot_init(&snapshot);
```

**Related API:**

- `xwork_agent_pool_get_snapshot`
- `xwork_agent_pool_snapshot_reset`

---

### xwork_agent_pool_snapshot_reset

Release the agent pool snapshot.

**Function:**

Release the pool id and agent snapshot list.

**Function prototype:**

```c
XWORK_API void xwork_agent_pool_snapshot_reset(xwork_agent_pool_snapshot *pSnapshot);
```

**parameter:**

- `pSnapshot`: snapshot to be released; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases the deep copy resources held inside the snapshot without releasing the structure itself.

**Additional Note:**

`xwork_agent_pool_create_from_snapshot` does not take over snapshot ownership.

**Example code:**

```c
xwork_agent_pool_snapshot_reset(&snapshot);
```

**Related API:**

- `xwork_agent_pool_create_from_snapshot`

---

### xwork_agent_pool_create

Create an agent pool.

**Function:**

Create an in-process agent pool based on runtime to register agents and borrow them from the task graph.

**Function prototype:**

```c
XWORK_API xwork_status xwork_agent_pool_create(
    const xwork_agent_pool_options *pOptions,
    xwork_agent_pool **ppPool
);
```

**parameter:**

- `pOptions`: pool creation parameter; must contain a valid `pRuntime`.
- `ppPool`: Output owned pool.

**Return value:**

- `XWORK_OK`: Created successfully.
- `XWORK_ERROR_INVALID_ARGUMENT`: Parameter is empty or runtime is invalid.
- `XWORK_ERROR_NO_MEMORY`: Memory allocation failed.

**Resource ownership:**

After success, `*ppPool` is owned by the caller and released using `xwork_agent_pool_destroy`. Pool borrows runtime, and runtime must live longer than pool.

**Additional Note:**

`sPoolId` is copied; default id is used when not set.

**Example code:**

```c
xwork_agent_pool *pool = NULL;
xwork_agent_pool_options opts;
xwork_agent_pool_options_init(&opts);
opts.pRuntime = runtime;
opts.sPoolId = "main";
xwork_status st = xwork_agent_pool_create(&opts, &pool);
```

**Related API:**

- `xwork_agent_pool_destroy`
- `xwork_agent_pool_add_agent`

---

### xwork_agent_pool_create_from_snapshot

Restore the agent pool from snapshot.

**Function:**

Rebuild the agent pool and the agent definitions within it using the pool snapshot taken previously.

**Function prototype:**

```c
XWORK_API xwork_status xwork_agent_pool_create_from_snapshot(
    xwork_runtime *pRuntime,
    const xwork_agent_pool_snapshot *pSnapshot,
    xwork_agent_pool **ppPool
);
```

**parameter:**

- `pRuntime`: The runtime borrowed by the pool after recovery.
- `pSnapshot`: agent pool snapshot.
- `ppPool`: Output owned pool.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

After success, the pool is owned by the caller; the snapshot is not taken over and is still reset by the caller.

**Additional Note:**

The recovery only contains agent metadata and does not restore the internal and external live state of the runtime.

**Example code:**

```c
xwork_agent_pool *pool = NULL;
xwork_status st = xwork_agent_pool_create_from_snapshot(runtime, &snapshot, &pool);
```

**Related API:**

- `xwork_agent_pool_get_snapshot`
- `xwork_agent_pool_destroy`

---

### xwork_agent_pool_destroy

Destroy agent pool.

**Function:**

Releases the pool and all agent definitions it owns.

**Function prototype:**

```c
XWORK_API void xwork_agent_pool_destroy(xwork_agent_pool *pPool);
```

**parameter:**

- `pPool`: The pool to be destroyed; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases resources owned by the pool; borrowed runtimes are not released.

**Additional Note:**

Before destroying the pool, the task graph borrowing the pool should be destroyed first.

**Example code:**

```c
xwork_agent_pool_destroy(pool);
```

**Related API:**

- `xwork_agent_pool_create`

---

### xwork_agent_pool_add_agent

Register the agent with the pool.

**Function:**

Copy agent options, add the agent definition to the pool, and return the borrowed agent pointer.

**Function prototype:**

```c
XWORK_API xwork_status xwork_agent_pool_add_agent(
    xwork_agent_pool *pPool,
    const xwork_agent_options *pOptions,
    xwork_agent **ppAgent
);
```

**parameter:**

- `pPool`: Target agent pool.
- `pOptions`: agent definition; must contain non-null `sAgentId`.
- `ppAgent`: Optional output borrowed agent.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

The agent is owned by the pool; `ppAgent` returns the borrowed pointer, which becomes invalid after the pool is destroyed.

**Additional Note:**

The agent id should be unique within the same pool.

**Example code:**

```c
xwork_agent *agent = NULL;
xwork_agent_options opts;
xwork_agent_options_init(&opts);
opts.sAgentId = "reviewer";
opts.eRole = XWORK_AGENT_ROLE_REVIEWER;
xwork_agent_pool_add_agent(pool, &opts, &agent);
```

**Related API:**

- `xwork_agent_pool_find_agent`
- `xwork_agent_get_id`

---

### xwork_agent_pool_get_agent_count

Get the number of agents.

**Function:**

Returns the number of registered agents in the pool.

**Function prototype:**

```c
XWORK_API size_t xwork_agent_pool_get_agent_count(const xwork_agent_pool *pPool);
```

**parameter:**

- `pPool`: agent pool; can be `NULL`.

**Return value:**

Returns the number of agents; when `pPool` is `NULL`, returns `0`.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

This value is a snapshot reading of the current in-memory pool.

**Example code:**

```c
size_t count = xwork_agent_pool_get_agent_count(pool);
```

**Related API:**

- `xwork_agent_pool_add_agent`

---

### xwork_agent_pool_find_agent

Find agent by id.

**Function:**

Find the specified agent id in the pool.

**Function prototype:**

```c
XWORK_API xwork_agent *xwork_agent_pool_find_agent(
    const xwork_agent_pool *pPool,
    const char *sAgentId
);
```

**parameter:**

- `pPool`：agent pool。
- `sAgentId`：agent id。

**Return value:**

Returns borrowed agent when found; returns `NULL` when not found or the parameter is invalid.

**Resource ownership:**

The return value is owned by the pool and cannot be released by the caller.

**Additional Note:**

The return pointer becomes invalid after the agent pool is destroyed.

**Example code:**

```c
xwork_agent *coder = xwork_agent_pool_find_agent(pool, "coder");
```

**Related API:**

- `xwork_agent_get_role`

---

### xwork_agent_pool_get_snapshot

Get agent pool snapshot.

**Function:**

Deep copy the pool id and all agent definitions to generate a durable snapshot.

**Function prototype:**

```c
XWORK_API xwork_status xwork_agent_pool_get_snapshot(
    const xwork_agent_pool *pPool,
    xwork_agent_pool_snapshot *pSnapshot
);
```

**parameter:**

- `pPool`: source pool.
- `pSnapshot`: output snapshot; should be init before calling.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

The snapshot has deep copy content, and the caller uses `xwork_agent_pool_snapshot_reset` to release it.

**Additional Note:**

The function resets the old contents of the output snapshot.

**Example code:**

```c
xwork_agent_pool_snapshot snapshot;
xwork_agent_pool_snapshot_init(&snapshot);
xwork_agent_pool_get_snapshot(pool, &snapshot);
xwork_agent_pool_snapshot_reset(&snapshot);
```

**Related API:**

- `xwork_agent_pool_create_from_snapshot`

---

### xwork_agent_get_id

Get agent id.

**Function:**

Returns the agent's id string.

**Function prototype:**

```c
XWORK_API const char *xwork_agent_get_id(const xwork_agent *pAgent);
```

**parameter:**

- `pAgent`: agent pointer; can be `NULL`.

**Return value:**

Returns borrowed id; returns `NULL` when `pAgent` is `NULL`.

**Resource ownership:**

The return value is owned by the agent pool and cannot be released by the caller.

**Additional Note:**

This id is available for task node's `sAgentId`.

**Example code:**

```c
const char *id = xwork_agent_get_id(agent);
```

**Related API:**

- `xwork_task_graph_add_node`

---

### xwork_agent_get_role

Get the agent role.

**Function:**

Returns `xwork_agent_role` for agent.

**Function prototype:**

```c
XWORK_API xwork_agent_role xwork_agent_get_role(const xwork_agent *pAgent);
```

**parameter:**

- `pAgent`: agent pointer; can be `NULL`.

**Return value:**

Returns the role; returns `XWORK_AGENT_ROLE_CUSTOM` if `pAgent` is `NULL`.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

Roles are used to host UI, logs, and scheduling policy decisions.

**Example code:**

```c
xwork_agent_role role = xwork_agent_get_role(agent);
```

**Related API:**

- `xwork_agent_pool_add_agent`

---

## Task Node and Task Graph

### xwork_task_node_options_init

Initialize task node options.

**Function:**

Prepare task node definition for adding to task graph.

**Function prototype:**

```c
XWORK_API void xwork_task_node_options_init(xwork_task_node_options *pOptions);
```

**parameter:**

- `pOptions`: options to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated; `xwork_task_graph_add_node` copies strings and arrays that need to be preserved.

**Additional Note:**

The default autonomous mode is `XWORK_AUTONOMY_SEMI_AUTO`.

**Example code:**

```c
xwork_task_node_options opts;
xwork_task_node_options_init(&opts);
opts.sTaskId = "implement";
opts.sAgentId = "coder";
opts.sInstruction = "Implement the feature.";
```

**Related API:**

- `xwork_task_graph_add_node`

---

### xwork_task_graph_options_init

Initialize task graph options.

**Function:**

Prepare task graph creation parameters.

**Function prototype:**

```c
XWORK_API void xwork_task_graph_options_init(xwork_task_graph_options *pOptions);
```

**parameter:**

- `pOptions`: options to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

- The default maximum concurrency is `1`.
- The default failure policy is `XWORK_TASK_FAILURE_FAIL_FAST`.
- `pAgentPool` must be set before creating graph.

**Example code:**

```c
xwork_task_graph_options opts;
xwork_task_graph_options_init(&opts);
opts.sGraphId = "feature-graph";
opts.pAgentPool = pool;
opts.iMaxConcurrency = 2;
```

**Related API:**

- `xwork_task_graph_create`

---

### xwork_task_node_summary_init

Initialize task node summary.

**Function:**

Prepare a task summary structure for querying the status of single or multiple nodes.

**Function prototype:**

```c
XWORK_API void xwork_task_node_summary_init(xwork_task_node_summary *pSummary);
```

**parameter:**

- `pSummary`: summary to be initialized; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

The default task status is `XWORK_TASK_PENDING`.

**Example code:**

```c
xwork_task_node_summary summary;
xwork_task_node_summary_init(&summary);
```

**Related API:**

- `xwork_task_graph_get_node_summary`

---

### xwork_task_node_summary_reset

Release task node summary.

**Function:**

Release the string deep copied by the API in summary and restore it to the init state.

**Function prototype:**

```c
XWORK_API void xwork_task_node_summary_reset(xwork_task_node_summary *pSummary);
```

**parameter:**

- `pSummary`: summary to be released; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases the resources held inside summary but does not release the structure itself.

**Additional Note:**

`pUserData` is a borrowed pointer and will not be released.

**Example code:**

```c
xwork_task_node_summary_reset(&summary);
```

**Related API:**

- `xwork_task_node_summary_init`

---

### xwork_task_node_summary_list_init

Initialize the task node summary list.

**Function:**

Prepare an empty list to receive graph node summaries.

**Function prototype:**

```c
XWORK_API void xwork_task_node_summary_list_init(xwork_task_node_summary_list *pList);
```

**parameter:**

- `pList`: List to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

This list should be initialized before calling `xwork_task_graph_list_node_summaries`.

**Example code:**

```c
xwork_task_node_summary_list list;
xwork_task_node_summary_list_init(&list);
```

**Related API:**

- `xwork_task_graph_list_node_summaries`

---

### xwork_task_node_summary_list_reset

Release the task node summary list.

**Function:**

Free all node summary and list arrays in the list.

**Function prototype:**

```c
XWORK_API void xwork_task_node_summary_list_reset(xwork_task_node_summary_list *pList);
```

**parameter:**

- `pList`: List to free; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases the contents owned by the list, but not the list structure itself.

**Additional Note:**

After freeing, the list can be passed back to the query API.

**Example code:**

```c
xwork_task_node_summary_list_reset(&list);
```

**Related API:**

- `xwork_task_node_summary_reset`

---

### xwork_task_node_snapshot_init

Initialize task node snapshot.

**Function:**

Prepare a task node snapshot for recovery or persistence.

**Function prototype:**

```c
XWORK_API void xwork_task_node_snapshot_init(xwork_task_node_snapshot *pSnapshot);
```

**parameter:**

- `pSnapshot`: snapshot to be initialized; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

After initialization, the field is empty and the status is the default pending semantics.

**Example code:**

```c
xwork_task_node_snapshot snapshot;
xwork_task_node_snapshot_init(&snapshot);
```

**Related API:**

- `xwork_task_graph_get_snapshot`

---

### xwork_task_node_snapshot_reset

Release the task node snapshot.

**Function:**

Release the string, workspace id array and dependency id array in the task node snapshot.

**Function prototype:**

```c
XWORK_API void xwork_task_node_snapshot_reset(xwork_task_node_snapshot *pSnapshot);
```

**parameter:**

- `pSnapshot`: snapshot to be released; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases the resources held within the snapshot but does not release the structure itself.

**Additional Note:**

After calling snapshot, return to init state.

**Example code:**

```c
xwork_task_node_snapshot_reset(&snapshot);
```

**Related API:**

- `xwork_task_node_snapshot_init`

---

### xwork_task_node_snapshot_list_init

Initialize the task node snapshot list.

**Function:**

Prepare an empty snapshot list.

**Function prototype:**

```c
XWORK_API void xwork_task_node_snapshot_list_init(xwork_task_node_snapshot_list *pList);
```

**parameter:**

- `pList`: List to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

This list is typically used as `xwork_task_graph_snapshot.tNodes`.

**Example code:**

```c
xwork_task_node_snapshot_list list;
xwork_task_node_snapshot_list_init(&list);
```

**Related API:**

- `xwork_task_node_snapshot_list_reset`

---

### xwork_task_node_snapshot_list_reset

Release the task node snapshot list.

**Function:**

Release all task node snapshots and arrays in the list.

**Function prototype:**

```c
XWORK_API void xwork_task_node_snapshot_list_reset(xwork_task_node_snapshot_list *pList);
```

**parameter:**

- `pList`: List to free; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases the contents owned by the list, but not the list structure itself.

**Additional Note:**

`xwork_task_graph_snapshot_reset` calls it indirectly.

**Example code:**

```c
xwork_task_node_snapshot_list_reset(&list);
```

**Related API:**

- `xwork_task_graph_snapshot_reset`

---

### xwork_task_graph_result_init

Initialize task graph result.

**Function:**

Clear the graph execution result count to zero.

**Function prototype:**

```c
XWORK_API void xwork_task_graph_result_init(xwork_task_graph_result *pResult);
```

**parameter:**

- `pResult`: result to be initialized; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

`xwork_task_graph_execute` populates the structure.

**Example code:**

```c
xwork_task_graph_result result;
xwork_task_graph_result_init(&result);
```

**Related API:**

- `xwork_task_graph_execute`

---

### xwork_task_graph_snapshot_init

Initialize task graph snapshot.

**Function:**

Prepare a graph snapshot to receive the complete task graph state.

**Function prototype:**

```c
XWORK_API void xwork_task_graph_snapshot_init(xwork_task_graph_snapshot *pSnapshot);
```

**parameter:**

- `pSnapshot`: snapshot to be initialized; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated; the internal list is initialized to empty.

**Additional Note:**

This structure should be initialized before calling `xwork_task_graph_get_snapshot`.

**Example code:**

```c
xwork_task_graph_snapshot snapshot;
xwork_task_graph_snapshot_init(&snapshot);
```

**Related API:**

- `xwork_task_graph_get_snapshot`
- `xwork_task_graph_snapshot_reset`

---

### xwork_task_graph_snapshot_reset

Release the task graph snapshot.

**Function:**

Release graph id, pause/cancel reason, task node snapshot list and handoff list.

**Function prototype:**

```c
XWORK_API void xwork_task_graph_snapshot_reset(xwork_task_graph_snapshot *pSnapshot);
```

**parameter:**

- `pSnapshot`: snapshot to be released; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases the resources held within the snapshot but does not release the structure itself.

**Additional Note:**

`xwork_task_graph_create_from_snapshot` does not take over snapshot ownership.

**Example code:**

```c
xwork_task_graph_snapshot_reset(&snapshot);
```

**Related API:**

- `xwork_task_graph_create_from_snapshot`

---

### xwork_task_graph_create

Create task graph.

**Function:**

Create a task graph for adding task nodes, declaring dependencies, and executing multi-agent workflows.

**Function prototype:**

```c
XWORK_API xwork_status xwork_task_graph_create(
    const xwork_task_graph_options *pOptions,
    xwork_task_graph **ppGraph
);
```

**parameter:**

- `pOptions`: Creation parameter; must contain a valid `pAgentPool`.
- `ppGraph`: Output owned graph.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

The graph is owned by the caller and released with `xwork_task_graph_destroy`; the graph borrows the agent pool, cancel token and callback.

**Additional Note:**

The default id is used when graph id is not set. When `iMaxConcurrency` is `0`, it is processed according to the implementation default value.

**Example code:**

```c
xwork_task_graph *graph = NULL;
xwork_task_graph_options opts;
xwork_task_graph_options_init(&opts);
opts.pAgentPool = pool;
opts.iMaxConcurrency = 2;
xwork_task_graph_create(&opts, &graph);
```

**Related API:**

- `xwork_task_graph_destroy`
- `xwork_task_graph_add_node`

---

### xwork_task_graph_create_from_snapshot

Restore the task graph from snapshot.

**Function:**

Rebuild the node, dependency, handoff and pause/cancel status of the task graph based on the snapshot.

**Function prototype:**

```c
XWORK_API xwork_status xwork_task_graph_create_from_snapshot(
    const xwork_task_graph_options *pOptions,
    const xwork_task_graph_snapshot *pSnapshot,
    xwork_task_graph **ppGraph
);
```

**parameter:**

- `pOptions`: The running environment parameters after recovery must provide compatible agent pool and callback.
- `pSnapshot`: source graph snapshot.
- `ppGraph`: Output owned graph.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

After success, the graph is owned by the caller; the snapshot is not taken over.

**Additional Note:**

Live in-flight states such as READY, RUNNING, and BLOCKED will be converted to a state that can continue to be scheduled according to the recovery boundary; external threads and live handles will not be recovered.

**Example code:**

```c
xwork_task_graph *graph = NULL;
xwork_task_graph_create_from_snapshot(&opts, &snapshot, &graph);
```

**Related API:**

- `xwork_task_graph_get_snapshot`

---

### xwork_task_graph_destroy

Destroy the task graph.

**Function:**

Release graph, nodes, handoffs and internal state.

**Function prototype:**

```c
XWORK_API void xwork_task_graph_destroy(xwork_task_graph *pGraph);
```

**parameter:**

- `pGraph`: The graph to be destroyed; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases graph-owned resources without releasing borrowed agent pool, cancel token, or callback user data.

**Additional Note:**

Do not destroy the graph while it is executing.

**Example code:**

```c
xwork_task_graph_destroy(graph);
```

**Related API:**

- `xwork_task_graph_create`

---

### xwork_task_graph_add_node

Add task node.

**Function:**

Adds a task node to the graph that is executed by the specified agent.

**Function prototype:**

```c
XWORK_API xwork_status xwork_task_graph_add_node(
    xwork_task_graph *pGraph,
    const xwork_task_node_options *pOptions
);
```

**parameter:**

- `pGraph`: target graph.
- `pOptions`: node definition; must contain `sTaskId` and `sAgentId`.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

graph copies fields such as task id, instruction, profile id, workspace id; `pUserData` is a borrowed pointer.

**Additional Note:**

`sAgentId` must be found in the graph's agent pool.

**Example code:**

```c
xwork_task_node_options node;
xwork_task_node_options_init(&node);
node.sTaskId = "review";
node.sAgentId = "reviewer";
node.sInstruction = "Review the patch.";
xwork_task_graph_add_node(graph, &node);
```

**Related API:**

- `xwork_task_graph_add_dependency`

---

### xwork_task_graph_add_dependency

Add task dependencies.

**Function:**

The statement `sAfterTaskId` must be executed after `sBeforeTaskId`.

**Function prototype:**

```c
XWORK_API xwork_status xwork_task_graph_add_dependency(
    xwork_task_graph *pGraph,
    const char *sBeforeTaskId,
    const char *sAfterTaskId
);
```

**parameter:**

- `pGraph`: target graph.
- `sBeforeTaskId`: predecessor task id.
- `sAfterTaskId`: post task id.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

graph replication dependency id.

**Additional Note:**

Both tasks must already exist; circular dependencies prevent the execution phase from advancing.

**Example code:**

```c
xwork_task_graph_add_dependency(graph, "implement", "review");
```

**Related API:**

- `xwork_task_graph_execute`

---

### xwork_task_graph_get_node_count

Get the number of task nodes.

**Function:**

Returns the number of task nodes in the graph.

**Function prototype:**

```c
XWORK_API size_t xwork_task_graph_get_node_count(const xwork_task_graph *pGraph);
```

**parameter:**

- `pGraph`: task graph; can be `NULL`.

**Return value:**

Returns the number of nodes; returns `0` when `pGraph` is `NULL`.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

This value does not represent the number of tasks completed.

**Example code:**

```c
size_t count = xwork_task_graph_get_node_count(graph);
```

**Related API:**

- `xwork_task_graph_list_node_summaries`

---

### xwork_task_graph_get_node_summary

Query the summary of a single node.

**Function:**

Get summary information such as node status, number of attempts, run id, number of dependencies, etc. by task id.

**Function prototype:**

```c
XWORK_API xwork_status xwork_task_graph_get_node_summary(
    const xwork_task_graph *pGraph,
    const char *sTaskId,
    xwork_task_node_summary *pSummary
);
```

**parameter:**

- `pGraph`: source graph.
- `sTaskId`: task id.
- `pSummary`: Output summary; should be init before calling.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

summary owns a deep copy of the string and frees it with `xwork_task_node_summary_reset`.

**Additional Note:**

The function resets the old contents of the output summary.

**Example code:**

```c
xwork_task_node_summary summary;
xwork_task_node_summary_init(&summary);
xwork_task_graph_get_node_summary(graph, "review", &summary);
xwork_task_node_summary_reset(&summary);
```

**Related API:**

- `xwork_task_graph_list_node_summaries`

---

### xwork_task_graph_list_node_summaries

List all node summaries.

**Function:**

Get a summary list of all task nodes in the graph.

**Function prototype:**

```c
XWORK_API xwork_status xwork_task_graph_list_node_summaries(
    const xwork_task_graph *pGraph,
    xwork_task_node_summary_list *pList
);
```

**parameter:**

- `pGraph`: source graph.
- `pList`: Output list; should be init before calling.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

The list has deep-copy elements, freed with `xwork_task_node_summary_list_reset`.

**Additional Note:**

The function resets the old contents of the output list.

**Example code:**

```c
xwork_task_node_summary_list list;
xwork_task_node_summary_list_init(&list);
xwork_task_graph_list_node_summaries(graph, &list);
xwork_task_node_summary_list_reset(&list);
```

**Related API:**

- `xwork_task_node_summary_list_reset`

---

### xwork_task_graph_get_node_run

Get the node corresponding to run.

**Function:**

Returns the `xwork_run` associated with a task node.

**Function prototype:**

```c
XWORK_API xwork_run *xwork_task_graph_get_node_run(
    const xwork_task_graph *pGraph,
    const char *sTaskId
);
```

**parameter:**

- `pGraph`: source graph.
- `sTaskId`: task id.

**Return value:**

Returns borrowed run if found; returns `NULL` if it does not exist or has not been created.

**Resource ownership:**

The return value is owned by the graph/run layer and cannot be destroyed by the caller.

**Additional Note:**

Commonly used to view child run events, summary or artifacts.

**Example code:**

```c
xwork_run *run = xwork_task_graph_get_node_run(graph, "implement");
```

**Related API:**

- `xwork_run_get_summary`

---

### xwork_task_graph_get_snapshot

Get task graph snapshot.

**Function:**

Deep copy the current state of the graph, including nodes, dependencies, handoffs, execution results, and pause/cancel flags.

**Function prototype:**

```c
XWORK_API xwork_status xwork_task_graph_get_snapshot(
    const xwork_task_graph *pGraph,
    xwork_task_graph_snapshot *pSnapshot
);
```

**parameter:**

- `pGraph`: source graph.
- `pSnapshot`: output snapshot; should be init before calling.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

The snapshot has deep-copy content and is released with `xwork_task_graph_snapshot_reset`.

**Additional Note:**

The snapshot can be handed over to the persistence backend for persistence, and then restored using create-from-snapshot.

**Example code:**

```c
xwork_task_graph_snapshot snapshot;
xwork_task_graph_snapshot_init(&snapshot);
xwork_task_graph_get_snapshot(graph, &snapshot);
xwork_task_graph_snapshot_reset(&snapshot);
```

**Related API:**

- `xwork_task_graph_create_from_snapshot`

---

### xwork_task_graph_execute

Execute task graph.

**Function:**

Schedule task nodes according to dependencies, maximum concurrency, and failure policies, and call the run execution logic corresponding to each node.

**Function prototype:**

```c
XWORK_API xwork_status xwork_task_graph_execute(
    xwork_task_graph *pGraph,
    xwork_task_graph_result *pResult
);
```

**parameter:**

- `pGraph`: graph to be executed.
- `pResult`: Optional output execution results; when passing `NULL`, only the graph internal results are updated.

**Return value:**

Returns `XWORK_OK`, execution error, or graph state error.

**Resource ownership:**

Graph ownership is not transferred; `pResult` does not contain dynamic resources.

**Additional Note:**

The graph prevents execute re-entry; pauses will stop advancement at scheduling boundaries and cancellations will be propagated to the configured cancel token.

**Example code:**

```c
xwork_task_graph_result result;
xwork_task_graph_result_init(&result);
xwork_status st = xwork_task_graph_execute(graph, &result);
```

**Related API:**

- `xwork_task_graph_pause`
- `xwork_task_graph_cancel`

---

### xwork_task_graph_cancel

Cancel task graph.

**Function:**

Set the graph cancellation state and propagate the cancellation reason to the configured xllm cancel token.

**Function prototype:**

```c
XWORK_API xwork_status xwork_task_graph_cancel(
    xwork_task_graph *pGraph,
    const char *sReason
);
```

**parameter:**

- `pGraph`: target graph.
- `sReason`: Optional cancellation reason.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

graph Copy cancellation reason string.

**Additional Note:**

Cancellation does not destroy the graph; the caller can still query the snapshot and summary.

**Example code:**

```c
xwork_task_graph_cancel(graph, "user requested stop");
```

**Related API:**

- `xwork_task_graph_is_cancelled`

---

### xwork_task_graph_is_cancelled

Check if the graph has requested cancellation.

**Function:**

Returns the current cancellation mark of the graph.

**Function prototype:**

```c
XWORK_API bool xwork_task_graph_is_cancelled(const xwork_task_graph *pGraph);
```

**parameter:**

- `pGraph`: task graph; can be `NULL`.

**Return value:**

Returns `true` if canceled; otherwise returns `false`.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

This function only checks the request status and does not guarantee that all running tasks have been stopped.

**Example code:**

```c
if (xwork_task_graph_is_cancelled(graph)) {
    /* stop launching extra work */
}
```

**Related API:**

- `xwork_task_graph_cancel`

---

### xwork_task_graph_pause

Pause the task graph.

**Function:**

Set a pause request so that execution stops at a scheduling boundary to continue starting new tasks.

**Function prototype:**

```c
XWORK_API xwork_status xwork_task_graph_pause(
    xwork_task_graph *pGraph,
    const char *sReason
);
```

**parameter:**

- `pGraph`: target graph.
- `sReason`: Optional pause reason.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

graph replication pause reason string.

**Additional Note:**

Pausing is different from canceling; after resuming, the graph can continue to schedule unfinished tasks.

**Example code:**

```c
xwork_task_graph_pause(graph, "waiting for approval");
```

**Related API:**

- `xwork_task_graph_resume`
- `xwork_task_graph_is_paused`

---

### xwork_task_graph_resume

Restore task graph.

**Function:**

Clear the pause request and reason for the pause.

**Function prototype:**

```c
XWORK_API xwork_status xwork_task_graph_resume(xwork_task_graph *pGraph);
```

**parameter:**

- `pGraph`: target graph.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

Release the pause reason saved in the graph.

**Additional Note:**

Restore does not automatically call `xwork_task_graph_execute`; the caller needs to advance execution again.

**Example code:**

```c
xwork_task_graph_resume(graph);
xwork_task_graph_execute(graph, &result);
```

**Related API:**

- `xwork_task_graph_pause`

---

### xwork_task_graph_is_paused

Check if the graph has requested a pause.

**Function:**

Returns the current pause mark of the graph.

**Function prototype:**

```c
XWORK_API bool xwork_task_graph_is_paused(const xwork_task_graph *pGraph);
```

**parameter:**

- `pGraph`: task graph; can be `NULL`.

**Return value:**

Returns `true` if paused; otherwise returns `false`.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

This function checks the request status, which does not mean that no tasks are being executed.

**Example code:**

```c
bool paused = xwork_task_graph_is_paused(graph);
```

**Related API:**

- `xwork_task_graph_pause`
- `xwork_task_graph_resume`

---

## Handoff

### xwork_handoff_request_options_init

Initialize handoff request options.

**Function:**

Prepare a handoff request to pass context between two task nodes.

**Function prototype:**

```c
XWORK_API void xwork_handoff_request_options_init(
    xwork_handoff_request_options *pOptions
);
```

**parameter:**

- `pOptions`: options to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated; the request API copies strings and arrays that need to be retained.

**Additional Note:**

Handoff id, from task id and to task id must be set.

**Example code:**

```c
xwork_handoff_request_options opts;
xwork_handoff_request_options_init(&opts);
opts.sHandoffId = "h1";
opts.sFromTaskId = "implement";
opts.sToTaskId = "review";
```

**Related API:**

- `xwork_task_graph_request_handoff`

---

### xwork_handoff_result_options_init

Initialize handoff result options.

**Function:**

Prepare handoff processing results for accepting, rejecting, or completing a handoff.

**Function prototype:**

```c
XWORK_API void xwork_handoff_result_options_init(
    xwork_handoff_result_options *pOptions
);
```

**parameter:**

- `pOptions`: options to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated; the resolve API copies the message.

**Additional Note:**

`eState` should be set to a final or intermediate processing state other than `XWORK_HANDOFF_PENDING`.

**Example code:**

```c
xwork_handoff_result_options opts;
xwork_handoff_result_options_init(&opts);
opts.sHandoffId = "h1";
opts.eState = XWORK_HANDOFF_ACCEPTED;
```

**Related API:**

- `xwork_task_graph_resolve_handoff`

---

### xwork_handoff_summary_init

Initialize handoff summary.

**Function:**

Prepare a handoff summary for receiving request or query results.

**Function prototype:**

```c
XWORK_API void xwork_handoff_summary_init(xwork_handoff_summary *pSummary);
```

**parameter:**

- `pSummary`: summary to be initialized; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

The default status is pending semantics.

**Example code:**

```c
xwork_handoff_summary summary;
xwork_handoff_summary_init(&summary);
```

**Related API:**

- `xwork_task_graph_request_handoff`

---

### xwork_handoff_summary_reset

Release handoff summary.

**Function:**

Free the string and reference arrays in the handoff summary.

**Function prototype:**

```c
XWORK_API void xwork_handoff_summary_reset(xwork_handoff_summary *pSummary);
```

**parameter:**

- `pSummary`: summary to be released; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases the resources held inside summary but does not release the structure itself.

**Additional Note:**

Artifact refs, memory refs, and workspace ids are all released as string arrays.

**Example code:**

```c
xwork_handoff_summary_reset(&summary);
```

**Related API:**

- `xwork_handoff_summary_init`

---

### xwork_handoff_summary_list_init

Initialize the handoff summary list.

**Function:**

Prepare an empty list to receive all handoffs in the graph.

**Function prototype:**

```c
XWORK_API void xwork_handoff_summary_list_init(xwork_handoff_summary_list *pList);
```

**parameter:**

- `pList`: List to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

It should be initialized before calling `xwork_task_graph_list_handoffs`.

**Example code:**

```c
xwork_handoff_summary_list list;
xwork_handoff_summary_list_init(&list);
```

**Related API:**

- `xwork_task_graph_list_handoffs`

---

### xwork_handoff_summary_list_reset

Release the handoff summary list.

**Function:**

Releases all handoff summaries and list arrays in the list.

**Function prototype:**

```c
XWORK_API void xwork_handoff_summary_list_reset(xwork_handoff_summary_list *pList);
```

**parameter:**

- `pList`: List to free; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releasing the resources owned by the list does not release the list structure itself.

**Additional Note:**

After release, the list returns to an empty state.

**Example code:**

```c
xwork_handoff_summary_list_reset(&list);
```

**Related API:**

- `xwork_handoff_summary_reset`

---

### xwork_task_graph_request_handoff

Create a handoff request.

**Function:**

Records a pending handoff between two task nodes, optionally with artifacts, memory context, and shared workspace references.

**Function prototype:**

```c
XWORK_API xwork_status xwork_task_graph_request_handoff(
    xwork_task_graph *pGraph,
    const xwork_handoff_request_options *pOptions,
    xwork_handoff_summary *pSummary
);
```

**parameter:**

- `pGraph`: target graph.
- `pOptions`: handoff request parameter.
- `pSummary`: Optional output summary.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

graph copies the requested content; summary, if populated, is reset by the caller.

**Additional Note:**

from/to task must exist; handoff id should be unique within the graph.

**Example code:**

```c
xwork_handoff_summary summary;
xwork_handoff_summary_init(&summary);
xwork_task_graph_request_handoff(graph, &opts, &summary);
xwork_handoff_summary_reset(&summary);
```

**Related API:**

- `xwork_task_graph_resolve_handoff`
- `xwork_task_graph_list_handoffs`

---

### xwork_task_graph_resolve_handoff

Handle handoff.

**Function:**

Update existing handoff status, status code and message.

**Function prototype:**

```c
XWORK_API xwork_status xwork_task_graph_resolve_handoff(
    xwork_task_graph *pGraph,
    const xwork_handoff_result_options *pOptions,
    xwork_handoff_summary *pSummary
);
```

**parameter:**

- `pGraph`: target graph.
- `pOptions`: Processing result; must contain handoff id.
- `pSummary`: Optional output updated summary.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

graph copies message; summary, if populated, reset by caller.

**Additional Note:**

`eState` cannot remain `XWORK_HANDOFF_PENDING`.

**Example code:**

```c
xwork_handoff_result_options result;
xwork_handoff_result_options_init(&result);
result.sHandoffId = "h1";
result.eState = XWORK_HANDOFF_COMPLETED;
xwork_task_graph_resolve_handoff(graph, &result, NULL);
```

**Related API:**

- `xwork_task_graph_request_handoff`

---

### xwork_task_graph_list_handoffs

List handoffs in graph.

**Function:**

Get the recorded handoff summary list of the current task graph.

**Function prototype:**

```c
XWORK_API xwork_status xwork_task_graph_list_handoffs(
    const xwork_task_graph *pGraph,
    xwork_handoff_summary_list *pList
);
```

**parameter:**

- `pGraph`: source graph.
- `pList`: Output list; should be init before calling.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

The list has deep-copy contents, released with `xwork_handoff_summary_list_reset`.

**Additional Note:**

This API can be used for UI display pending handoff or re-establishing context after recovery.

**Example code:**

```c
xwork_handoff_summary_list list;
xwork_handoff_summary_list_init(&list);
xwork_task_graph_list_handoffs(graph, &list);
xwork_handoff_summary_list_reset(&list);
```

**Related API:**

- `xwork_task_graph_request_handoff`

---

## Reporting and Aggregation Artifact

### xwork_task_graph_emit_agent_result_report

Generate results reporting artifacts for individual agent tasks.

**Function:**

Writes the child run results of the specified task to the artifact object provided by the caller.

**Function prototype:**

```c
XWORK_API xwork_status xwork_task_graph_emit_agent_result_report(
    const xwork_task_graph *pGraph,
    const char *sTaskId,
    const char *sArtifactId,
    xwork_artifact *pArtifact
);
```

**parameter:**

- `pGraph`: source graph.
- `sTaskId`: task id.
- `sArtifactId`: output artifact id.
- `pArtifact`: output artifact; should be initialized according to artifact API before calling.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

The artifact content is held by the output object and the caller presses the artifact API reset.

**Additional Note:**

This report is for a single task and does not replace the run event audit flow.

**Example code:**

```c
xwork_artifact artifact;
xwork_artifact_init(&artifact);
xwork_task_graph_emit_agent_result_report(graph, "review", "review-report", &artifact);
xwork_artifact_reset(&artifact);
```

**Related API:**

- `xwork_task_graph_emit_aggregate_report`
- `xwork_artifact_reset`

---

### xwork_task_graph_emit_aggregate_report

Generate aggregate reporting artifact.

**Function:**

Aggregate the execution results of the entire task graph into the artifact of the specified run.

**Function prototype:**

```c
XWORK_API xwork_status xwork_task_graph_emit_aggregate_report(
    const xwork_task_graph *pGraph,
    xwork_run *pRun,
    const char *sArtifactId,
    xwork_artifact *pArtifact
);
```

**parameter:**

- `pGraph`: source graph.
- `pRun`: run to receive aggregated reports.
- `sArtifactId`: output artifact id.
- `pArtifact`: Output artifact.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

The artifact content is held by the output object and the caller presses artifact API reset; run is not taken over.

**Additional Note:**

Aggregated reports are suitable as UI summaries or pipeline artifact indexes.

**Example code:**

```c
xwork_artifact artifact;
xwork_artifact_init(&artifact);
xwork_task_graph_emit_aggregate_report(graph, run, "graph-report", &artifact);
xwork_artifact_reset(&artifact);
```

**Related API:**

- `xwork_task_graph_emit_agent_result_report`
- `xwork_run`

---

## Related documents

- [Run API](api-run.md)
- [Artifact API](api-artifacts.md)
- [Multi-Agent Task Graph](../guide/multi-agent-intro.md)
- [Multi-Agent claw Example](../case/multi-agent-claw.md)
- [Internal multi-agent contract](../../dev/docs/MULTI_AGENT.md)
