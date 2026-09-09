# xwork architecture description

>Status: First draft in Chinese, awaiting review.

This article explains xwork's layering, core object relationships, orchestrator tool loop, and how multi-agent, remote worker, and replay are built on run and artifact.

## Hierarchical relationship

```text
xrt
  |
xllm
  |
xwork
  |
xcode / xclaw
```

| Layers | Responsibilities |
| --- | --- |
| `xrt` | Basic runtime, file, process, terminal, network, container and platform abstractions. |
| `xllm` | provider, model request/response, streaming events, session, memory and tool call protocols. |
| `xwork` | workspace, run, tool execution, approval, artifact, checkpoint, persistence, multi-agent, remote worker, replay. |
| `xcode` / `xclaw` | UI/CLI, product strategy, user interaction, deployment, and specific host bridges. |

The core boundary of xwork is: it does not do provider protocol adaptation, does not do product UI, and does not do cloud control plane deployment; it organizes model capabilities into approvable, recoverable, and auditable workflow capabilities.

## Core Object Relationship

```text
xwork_runtime
  |
  +-- xwork_workspace
  |
  +-- tool registry
  |
  +-- xwork_run
  |     |
  |     +-- events
  |     +-- steps
  |     +-- approval request
  |     +-- checkpoints
  |     +-- artifacts
  |
  +-- host services
  |
  +-- persistence backend
```

Object ownership principles:

- runtime is the top-level owner.
- workspace, tool and run are usually hung under runtime.
- Host services, persistence callbacks, external xllm runtime, xllm memory, replay engine are mostly borrowed.
- The query output structure usually holds deep-copy results and needs to correspond to `*_reset()`.

## Orchestrator Tool Loop

```text
run_execute
  |
  v
prepare model input
  |
  v
xllm model turn
  |
  +--> stream event callback
  |
  v
tool calls?
  |
  +-- no --> final output -> complete run
  |
  +-- yes
       |
       v
     policy / approval
       |
       +-- needs approval --> WAITING_APPROVAL -> submit approval -> resume
       |
       v
     host service / tool executor
       |
       v
     tool result + artifacts + events
       |
       v
     next model turn
```

Key points:

- policy must be executed before side effects.
- approval pause is a resumable state of run.
- tool result and artifact enter the same audit pipeline.
- cancel token, interrupt callback, async cancel and host context use collaborative cancellation.

## Multi-Agent is built on Run

```text
agent pool
  |
task graph
  |
task node -> child xwork_run -> events/artifacts/checkpoints
```

The task graph is responsible for dependencies, concurrency, handoff and child run mapping. The execution results of each agent/task still enter the run/event/artifact model, so the product can uniformly audit single agents and multiple agents.

## Remote Worker is built on Tool/Artifact

```text
control plane
  |
worker registry / lease
  |
assignment
  |
worker executes host tool
  |
result + artifact refs + output chunks
```

xwork defines control-plane objects and decoded-message transport boundaries, but socket, auth, tenant/project isolation, retries, and blob streaming are the responsibility of the host product.

## Replay is built on Side-Effect Boundary

```text
model/tool/host/checkpoint/filesystem ref
  |
replay entry/event
  |
record / strict / audit
  |
divergence result/report artifact
```

Replay does not resume the live process or terminal. It records and compares key inputs and outputs, hashes, and event sequences for regression, auditing, and problem reproduction.

## Related documents

- [API Reference Index](api/README.md)
- [xllm Orchestration and Tool Loop](guide/xllm-orchestrator-intro.md)
- [Multi-Agent Task Graph](guide/multi-agent-intro.md)
- [Remote Worker and Control Plane](guide/remote-worker-intro.md)
- [Persistence, Checkpoints, and Replay](guide/persistence-replay-intro.md)
