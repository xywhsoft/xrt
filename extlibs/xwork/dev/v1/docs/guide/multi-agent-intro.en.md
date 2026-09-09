#Multi-Agent task graph

>Status: First draft in Chinese, awaiting manual review.

xwork's multi-agent capabilities are centered on the in-process agent pool and task graph. It is not a production-level "automatic planner", but provides reusable scheduling, dependencies, handoff, events and recovery infrastructure.

## Core Object

| Object | Function |
| --- | --- |
| `xwork_agent_pool` | Saves agent role, profile, capabilities and execution limits. |
| `xwork_task_graph` | Saves task nodes, dependencies, scheduling policies, and child run mappings. |
| handoff | Pass artifact refs, memory context refs, and structured results between agents/task. |
| child run | Each execution task can be mapped to a separate `xwork_run` to facilitate event and artifact auditing. |

## Scheduling semantics

- The task becomes ready after its dependencies are satisfied.
- `max_concurrency` controls the number of tasks executed concurrently.
- The dependency policy can be configured as fail-fast, require-all, or best-effort.
- Agents can configure retry, max turns, and timeout.
- pause/cancel takes effect cooperatively at scheduling boundaries.

## Life cycle diagram

```text
PENDING
  |
  | dependencies satisfied
  v
READY
  |
  | scheduled
  v
RUNNING
  | \
  |  \ failure
  |   v
  |  FAILED
  |
  | success
  v
COMPLETED
```

When dependencies are not satisfied, tasks may be in blocked/waiting semantics and enter ready after dependencies are completed. During recovery, the live status such as READY/RUNNING/BLOCKED will return to PENDING.

## Failure propagation example

```text
planner
  |
  +--> coder
  |      |
  |      +--> tester
  |
  +--> reviewer
```

- fail-fast: After `coder` fails, `tester` directly fails or is canceled and is no longer executed.
- require-all: `tester` must wait for all dependencies to succeed.
- best-effort: Downstream execution is still allowed after dependency failure, and the task callback determines how to handle it.

## Recovery semantics

The task graph snapshot will save the final task and handoff metadata. When restoring:

- Final states such as completed/failed/cancelled will be retained.
- Live status such as ready/running/blocked will return to pending.
- Native threads and callback stacks will not be restored.
- The caller must provide compatible runtime, agent pool, execution callback and host environments.

## Related examples

- [Multi-Agent claw Example](../case/multi-agent-claw.md)
- [Multi-Agent API](../api/api-multi-agent.md)
