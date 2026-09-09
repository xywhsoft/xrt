#Persistence, checkpoint and replay

>Status: First draft in Chinese, awaiting manual review.

xwork's run is designed to be "recoverable" by default. Key status enters the persistence layer through events, checkpoints, snapshots, artifacts, and replay cassettes.

## Persistible objects

| Object | Purpose |
| --- | --- |
| run snapshot | Save the latest life cycle status of run, pending tool, approval decision, etc. |
| event | Saves models, tools, approvals, tasks, and scheduling events. |
| checkpoint | Saves a resumable execution point. |
| artifact | Save files, patches, commands, terminal, diagnostic and reporting artifacts. |
| agent graph snapshot | Save multi-agent task graph and handoff state. |
| control plane snapshot | Save worker, remote task, lease and queue status. |
| replay cassette | Save model, tools, checkpoint, filesystem snapshot/ref and divergence information. |

## Restore boundaries

Can be restored:

- The latest snapshot of run.
- Parameters for tool calls awaiting approval or execution.
- Generated events and artifacts.
- Persistent agent graph, remote control plane and replay manifest.

Unable to automatically restore:

- Active OS process handle.
- The live state of a local interactive terminal session.
- The executing thread stack or callback stack.
- Side effects that have occurred but are not documented in external systems.

## Checkpoint / Recovery layer

checkpoint and latest snapshot solve the problem of "how to continue after the process is restarted".

Typical process:

```text
configure file persistence
execute run
store latest snapshot / checkpoint / events / artifacts
process exits
recreate runtime/workspace/tools/host services
recover latest run snapshot
resume pending tool or approval boundary
```

Recovery relies on the caller to re-provide compatible environments: runtime, workspace, tool registry, host services, persistence backend, xllm runtime/profile. snapshot only saves serializable state.

## The role of Replay

replay is used to reproduce or audit key input and output in an Agent run. It can:

- Log model requests/responses, tool requests/responses and checkpoints.
- Log filesystem snapshot/ref.
- Prevent undocumented side effects in strict mode.
- Compare new results with old records in audit mode.
- Output first divergence and divergence report artifacts.

## Replay layer

replay solves the problem of "whether the input and output of the same model/tool/host can still match".

Typical process:

```text
create replay engine in record mode
execute model/tool/host calls
store replay cassette
load replay engine in strict or audit mode
replay expected entries/events
query first divergence
emit divergence report artifact
```

strict mode is suitable for CI regression; audit mode is suitable for auditing differences but continues to collect more divergence.

Checkpoint/recovery and replay can be used in combination, but they are not the same layer of capabilities: checkpoint restores the running state, and replay compares input and output and side effect boundaries.

## Related examples

- [Replay Agent Run Example](../case/replay-agent-run.md)
- [Remote Worker Example](../case/remote-worker-agent.md)
- [Persistence API](../api/api-persistence.md)
- [Replay API](../api/api-replay.md)
