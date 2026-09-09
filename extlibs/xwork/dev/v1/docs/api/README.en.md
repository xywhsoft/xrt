# xwork API document index

> API documentation is organized using a mental model rather than a mechanical expansion of the declaration order of `xwork.h`.

English version: [xwork API Index](README.en.md)

## Current public API scope

`xwork.h` has covered the following core objects:

| Module | Main Objects/Capabilities |
| --- | --- |
| Basic runtime | `xwork_runtime`, status code, version, profile, xllm bootstrap. |
| workspace | `xwork_workspace`, workspace memory sync, workspace root and policy boundaries. |
| Tool system | `xwork_tool_def`, tool registration, tool executor, host service bridge. |
| Run life cycle | `xwork_run`, run state, step, event, summary, synchronous/asynchronous execution. |
| Orchestrator | xllm model turn, tool loop, streaming events, cancellation, approval pause/resume. |
| Approval and Policy | `xwork_approval_request`, automation mode, filesystem/process/network policy. |
| Artifact | `xwork_artifact`, artifact summary, patch/report/command/output typed metadata. |
| Persistence | `xwork_persistence_backend`, run snapshot, checkpoint, event/artifact query. |
| Multi-Agent | `xwork_agent_pool`, `xwork_task_graph`, task dependency, handoff, graph recovery. |
| Remote Worker | `xwork_control_plane`, worker, remote task, lease, output/blob chunk. |
| Replay | `xwork_replay_engine`, replay entry, manifest, filesystem ref, divergence report. |

## API reading path

1. First read the public contract description at the top of `xwork.h` to confirm object ownership, borrowing relationships, and thread safety boundaries.
2. Press the current task again to select the module document.
3. When writing integration code, give priority to using `*_init()` to initialize the option/result structure, and use the corresponding `*_reset()` to release the deep copy result.
4. When you need to save the state across processes or versions, confirm `XWORK_PERSISTENCE_FORMAT_VERSION` and `XWORK_REMOTE_PROTOCOL_VERSION_CURRENT`.

The error code is based on the top public contract of `xwork.h` and [Common Types and Conventions](types.md). The module page only supplements the most common error sources of the module and does not redefine the error code semantics.

## API page

Basic page:

- [API Page Template](API_PAGE_TEMPLATE.md)
- [Common Types and Conventions](types.md)
- [Runtime API](api-runtime.md)
- [Workspace API](api-workspace.md)
- [Tool API](api-tools.md)
- [Run API](api-run.md)
- [Orchestrator API](api-orchestrator.md)

Subsequent pages:

- [Policy / Approval API](api-policy-approval.md)
- [Artifact API](api-artifacts.md)
- [Persistence API](api-persistence.md)
- [Host Tools API](api-host-tools.md)
- [Profiles API](api-profiles.md)
- [Multi-Agent API](api-multi-agent.md)
- [Remote Worker API](api-remote-worker.md)
- [Replay API](api-replay.md)
- [xllm Integration API](api-xllm-integration.md)
- [Local Host API](api-local-host.md)

## API documentation writing standards

API documentation must be written at the granularity of `D:\git\xllm\docs\api`. You can't just do a module overview, and you can't just list function names.

Each module page contains at least:

- Description of constants, macros, enumerations and structures.
- API directory grouped by functionality.
- Separate section for each exposing `XWORK_API` functions.
- Function prototype, parameters, return value, resource ownership, supplementary instructions and sample code.
- Common errors, related APIs, related tutorials and related cases.

Each function section contains at least:

- **Function**: What problem does this function solve and when to use it.
- **Function Prototype**: Exact C prototype copied from `xwork.h`.
- **Parameters**: Explanation of input/output direction, possible `NULL`, lifetime, ownership, units, scope and default value one by one.
- **Return value**: success/failure semantics, error code, whether partial results are possible.
- **Resource ownership**: Who allocates, who releases, and which `reset` / `destroy` function is used to clean up.
- **Additional Notes**: Call order, thread safety, recovery boundaries, profile/workspace/host/replay differences and compatibility considerations.
- **Sample code**: Try to give small code snippets that can be directly learned; complex processes can be linked to `case/`.

Only when an API page covers all public `XWORK_API` functions assigned to the module, and each function has the above, can it be marked complete in the API reference rewrite spec.
