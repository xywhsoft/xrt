#Multi-Agent claw example

> Corresponding source code: `examples/multi_agent_claw.c`

This example demonstrates xwork's in-process multi-Agent scheduling capabilities.

## Problem solved

Complex Agent tasks usually need to be split into roles such as planning, implementation, testing, and review. xwork provides a task graph that allows these roles to share workspaces, pass handoffs, and maintain run/event/artifact records for each subtask.

## process

```text
create runtime/workspace/file persistence
create agent pool
add planner/coder/tester/reviewer
create task graph
add dependency edges
execute graph with max concurrency
emit per-task report artifact
persist and recover graph snapshot
query child run index
```

## Key points

- The task graph is responsible for dependency scheduling and is not responsible for automatically planning task content.
- Each task can be mapped to a child run to facilitate independent auditing.
- handoff can carry artifact ref and memory context ref.
- After recovery, running/blocked tasks will return to pending for rescheduling.

## Task map

```text
planner
  |
  +--> coder
  |      |
  |      +--> tester
  |
  +--> reviewer
```

This structure covers fan-out, fan-in, child run audits, and aggregate report.

## Key API

| API | Function |
| --- | --- |
| `xwork_agent_pool_create()` | Create agent pool. |
| `xwork_agent_pool_add_agent()` | Add planner/coder/tester/reviewer. |
| `xwork_task_graph_create()` | Create task graph. |
| `xwork_task_graph_add_node()` | Add task node. |
| `xwork_task_graph_add_dependency()` | Add dependent edges. |
| `xwork_task_graph_execute()` | Execute scheduler. |
| `xwork_task_graph_emit_agent_result_report()` | Issue a report for each task. |
| `xwork_task_graph_emit_aggregate_report()` | Issue an aggregate report. |
| `xwork_file_persistence_store_task_graph_snapshot()` | Save graph snapshot. |
| `xwork_runtime_query_persisted_run_index()` | Query child run by parent/agent/task. |

## Child run index

The child run index allows the product to find the execution records of each agent/task from the parent run. Common query dimensions include parent run id, agent id, task id, last event sequence, and artifact count.

## Suitable for expansion

- Generate task graph using real planner.
- Bind different roles to different models/profiles.
- Lock the tasks that share writing to the workspace or use replay for auditing.
