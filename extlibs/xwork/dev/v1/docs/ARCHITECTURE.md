# xwork 架构说明

> 状态：中文初稿，待审阅。

本文说明 xwork 的分层、核心对象关系、orchestrator tool loop，以及 multi-agent、remote worker、replay 如何建立在 run 和 artifact 之上。

## 分层关系

```text
xrt
  |
xllm
  |
xwork
  |
xcode / xclaw
```

| 层 | 职责 |
| --- | --- |
| `xrt` | 基础运行时、文件、进程、终端、网络、容器和平台抽象。 |
| `xllm` | provider、模型请求/响应、流式事件、session、memory 和 tool call 协议。 |
| `xwork` | 工作区、run、工具执行、审批、artifact、checkpoint、persistence、multi-agent、remote worker、replay。 |
| `xcode` / `xclaw` | UI/CLI、产品策略、用户交互、部署和特定 host bridge。 |

xwork 的核心边界是：不做 provider 协议适配，不做产品 UI，不做云控制面部署；它把模型能力组织成可审批、可恢复、可审计的工作流能力。

## 核心对象关系

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

对象所有权原则：

- runtime 是顶层 owner。
- workspace、tool 和 run 通常挂在 runtime 下。
- host services、persistence callbacks、外部 xllm runtime、xllm memory、replay engine 多数是 borrowed。
- query 输出结构通常持有 deep-copy 结果，需要对应 `*_reset()`。

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

关键点：

- policy 必须在副作用前执行。
- approval pause 是 run 的可恢复状态。
- tool result 和 artifact 进入同一审计管线。
- cancel token、interrupt callback、async cancel 和 host context 走协作式取消。

## Multi-Agent 建立在 Run 之上

```text
agent pool
  |
task graph
  |
task node -> child xwork_run -> events/artifacts/checkpoints
```

task graph 负责依赖、并发、handoff 和 child run 映射。每个 agent/task 的执行结果仍进入 run/event/artifact 模型，因此产品可以统一审计单 agent 和多 agent。

## Remote Worker 建立在 Tool/Artifact 之上

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

xwork 定义 control-plane 对象和 decoded-message transport 边界，但 socket、auth、tenant/project 隔离、重试和 blob streaming 由宿主产品负责。

## Replay 建立在 Side-Effect Boundary 之上

```text
model/tool/host/checkpoint/filesystem ref
  |
replay entry/event
  |
record / strict / audit
  |
divergence result/report artifact
```

Replay 不恢复 live process 或 terminal。它记录和比较关键输入输出、hash 和 event sequence，用于回归、审计和问题复现。

## 相关文档

- [API 文档索引](api/README.md)
- [xllm 编排与工具循环](guide/xllm-orchestrator-intro.md)
- [多 Agent 任务图](guide/multi-agent-intro.md)
- [远程 Worker 与控制平面](guide/remote-worker-intro.md)
- [持久化、checkpoint 与 replay](guide/persistence-replay-intro.md)
