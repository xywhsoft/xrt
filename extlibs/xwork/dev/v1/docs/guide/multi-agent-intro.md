# 多 Agent 任务图

> 状态：中文初稿，待人工审阅。

xwork 的多 Agent 能力以 in-process agent pool 和 task graph 为核心。它不是一个产品级“自动规划器”，而是提供可复用的调度、依赖、handoff、事件和恢复基础设施。

## 核心对象

| 对象 | 作用 |
| --- | --- |
| `xwork_agent_pool` | 保存 agent role、profile、能力和执行限制。 |
| `xwork_task_graph` | 保存任务节点、依赖关系、调度策略和 child run 映射。 |
| handoff | 在 agent/task 之间传递 artifact ref、memory context ref 和结构化结果。 |
| child run | 每个执行任务可映射到独立 `xwork_run`，便于事件和 artifact 审计。 |

## 调度语义

- 任务在依赖满足后变为 ready。
- `max_concurrency` 控制并发执行数量。
- 依赖策略可配置 fail-fast、require-all 或 best-effort。
- agent 可配置 retry、max turns 和 timeout。
- pause/cancel 会在调度边界协作式生效。

## 生命周期图

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

依赖未满足时，任务可能处于 blocked/waiting 语义，并在依赖完成后进入 ready。恢复时 READY/RUNNING/BLOCKED 等 live 状态会回到 PENDING。

## 失败传播示例

```text
planner
  |
  +--> coder
  |      |
  |      +--> tester
  |
  +--> reviewer
```

- fail-fast：`coder` 失败后，`tester` 直接失败或取消，不再执行。
- require-all：`tester` 必须等待所有依赖成功。
- best-effort：依赖失败后仍允许下游执行，由 task callback 自行判断如何处理。

## 恢复语义

task graph snapshot 会保存终态任务和 handoff 元数据。恢复时：

- completed/failed/cancelled 等终态会保留。
- ready/running/blocked 等 live 状态会回到 pending。
- 原生线程和 callback 栈不会恢复。
- 调用方必须提供兼容 runtime、agent pool、执行 callback 和 host 环境。

## 相关范例

- [多 Agent claw 范例](../case/multi-agent-claw.md)
- [Multi-Agent API](../api/api-multi-agent.md)
