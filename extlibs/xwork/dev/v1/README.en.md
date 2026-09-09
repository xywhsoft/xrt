# xwork

`xwork` is a C-language Agent workflow runtime library that sits on top of `xllm`.

Its goal is to precipitate recurring capabilities in AI IDE, command line autonomous Agent, remote Worker and resumable task flow into a common infrastructure for reuse by subsequent products such as `xcode` and `xclaw`.

## position

The default layering is as follows:

```text
xrt
xllm
xwork
xcode / xclaw
```

`xllm` is responsible for unifying model calls, provider adaptation, session, memory and streaming responses.

`xwork` is responsible for orchestrating model calls around workspaces, tools, approvals, task status, artifacts, persistence, remote execution and replay.

## Core Competencies

| Capabilities | Description |
| --- | --- |
| Runtime / Workspace | Manage runtime, workspace, profile, shared xllm runtime and workspace memory. |
| Tool Registry | Register model callable tools and unify tool definitions, parameters, executors and cancellation contexts. |
| Orchestrator | Drives the model turn + tool loop, handling tool invocation, approval pause, resume and final summary. |
| Policy / Approval | Model policy judgments and approval requests for files, processes, networks, terminals, and high-risk operations. |
| Host Tools | Built-in filesystem, process, terminal, vcs, editor host tool contract. |
| Artifacts | Log file contents, patches, command output, terminal status, diagnostics, and structured reports. |
| Persistence | Saves run, event, checkpoint, artifact, agent graph, remote plane and replay cassette. |
| Multi-Agent | Provides in-process agent pool, task graph, dependency scheduling, handoff and recovery boundaries. |
| Remote Worker | Provides control plane, worker registry, lease, assignment, artifact/output chunk protocol objects. |
| Deterministic Replay | Record and replay models, tools, checkpoints, filesystem snapshots/refs, and report divergence. |

## Current stage capability boundary

Already available:

- Single run runtime/workspace/tool/orchestrator closed loop.
- xllm model turn + tool loop, streaming events, approval pause/resume and asynchronous cancellation.
- Local filesystem/process/terminal/vcs/editor host tool contract.
- artifact, checkpoint, file persistence and run/event/artifact queries.
- in-process multi-agent task graph.
- in-process remote worker/control plane objects and decoded HTTP transport boundaries.
- deterministic replay cassette, filesystem snapshot/ref and divergence report.

Production integration that still needs to be done by the host product:

- UI, CLI, IDE panels and human-computer interaction.
- Real network server/client, worker auth, tenant/project isolation and deployment control plane.
- Product-level autonomous planner strategy.
- External database, object storage, or distributed multi-writer persistence backend.
- Long-term operation and maintenance of model provider's specific behavior drift.

## Applicable scenarios

Suitable:

- Agent runtime of AI IDE.
- Claw-like command line autonomous agent.
- Approvable, recoverable tool execution orchestration.
- Multi-Agent task graph and local/remote worker scheduling.
- Agent run artifacts, checkpoints, replays and audits.

Not suitable directly as:

- Complete AI IDE product.
- Complete cloud control plane.
- Replacement for model provider SDK.
- Simple chat wrapper without auditing and recovery.

## Document entry

- [文档中心](docs/README.md)
- [English README](README.en.md)
- [API 文档索引](docs/api/README.md)
- [教程索引](docs/guide/README.md)
- [范例索引](docs/case/README.md)
- [开发与设计资料](dev/docs/README.md)

Recommended reading order:

1. Read [文档中心](docs/README.md) first to confirm the document partition of xwork.
2. Read [第一个 xwork 程序](docs/guide/first-xwork-program.md) again and understand the minimum runtime/workspace/run.
3. If connected to AI IDE or claw, read [xllm 编排与工具循环](docs/guide/xllm-orchestrator-intro.md).
4. If you need multiple Agents, remote Workers or replay, read the corresponding sample analysis.
5. Finally check [API 文档索引](docs/api/README.md) and `xwork.h`.

## Quick compile check

Execute from the repository root directory:

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -c xwork.c
```

## Single Header

Generate the single-header distribution:

```powershell
.\build_single_head.bat
```

The generated file is `singlehead/xwork.h`. Define `XWORK_IMPLEMENTATION`
in exactly one translation unit before including it. The single header embeds
only xwork itself; provide `xrt`, `xllm`, and `sqlite` according to your build
layout.

Example build and run commands are documented in [examples/README.md](examples/README.md).

## Dependencies

| Dependencies | Description |
| --- | --- |
| `xrt` | Basic runtime, file, process, terminal and platform capabilities. |
| `xllm` | provider, model request/response, session, memory and streaming events. |
| `sqlite` | The current warehouse example and one of the local dependencies used by smoke. |
| Platform Libraries | Windows examples typically link against system libraries such as `ws2_32`, `iphlpapi`, etc. |

The `lib/` directory of the repository holds a snapshot of dependencies required for the current xwork build.

## Directory structure

```text
xwork/
  README.md
  xwork.h
  xwork.c
  include/
  src/
  examples/
  tests/
  docs/
    api/
    guide/
    case/
  dev/
```

Key directory description:

| Path | Description |
| --- | --- |
| `xwork.h` | Public API. |
| `xwork.c` | Aggregation implementation entrance. |
| `src/` | Implemented internally in each module. |
| `examples/` | Runs the integration example. |
| `tests/` | smoke and behavioral validation. |
| `docs/` | Formal user-facing documentation. |
| `dev/` | Design, development plans, historical tracking and internal instructions. |

## Document language strategy

The Chinese master draft of the document is first generated, and the English document is translated after manual review and stabilization.

English documents use the `.en.md` suffix to maintain a one-to-one correspondence with Chinese documents; Chinese documents are the main source of subsequent changes.
