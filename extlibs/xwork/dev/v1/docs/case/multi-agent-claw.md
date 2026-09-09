# 多 Agent claw 范例

> 对应源码：`examples/multi_agent_claw.c`

这个范例展示 xwork 的 in-process 多 Agent 调度能力。

## 解决的问题

复杂 Agent 任务通常需要拆分为规划、实现、测试、审阅等角色。xwork 提供 task graph，让这些角色可以共享工作区、传递 handoff，并保留每个子任务的 run/event/artifact 记录。

## 流程

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

## 关键点

- task graph 负责依赖调度，不负责自动规划任务内容。
- 每个任务可以映射到 child run，方便独立审计。
- handoff 可携带 artifact ref 和 memory context ref。
- 恢复后 running/blocked 任务会回到 pending，以便重新调度。

## 任务图

```text
planner
  |
  +--> coder
  |      |
  |      +--> tester
  |
  +--> reviewer
```

这个结构覆盖 fan-out、fan-in、child run 审计和 aggregate report。

## 关键 API

| API | 作用 |
| --- | --- |
| `xwork_agent_pool_create()` | 创建 agent pool。 |
| `xwork_agent_pool_add_agent()` | 添加 planner/coder/tester/reviewer。 |
| `xwork_task_graph_create()` | 创建 task graph。 |
| `xwork_task_graph_add_node()` | 添加任务节点。 |
| `xwork_task_graph_add_dependency()` | 添加依赖边。 |
| `xwork_task_graph_execute()` | 执行 scheduler。 |
| `xwork_task_graph_emit_agent_result_report()` | 发出每个任务报告。 |
| `xwork_task_graph_emit_aggregate_report()` | 发出聚合报告。 |
| `xwork_file_persistence_store_task_graph_snapshot()` | 保存 graph snapshot。 |
| `xwork_runtime_query_persisted_run_index()` | 按 parent/agent/task 查询 child run。 |

## Child run index

child run index 让产品可以从父 run 找到各个 agent/task 的执行记录。常见查询维度包括 parent run id、agent id、task id、last event sequence 和 artifact count。

## 适合扩展

- 用真实 planner 生成 task graph。
- 将不同角色绑定到不同模型/profile。
- 对共享写入 workspace 的任务加锁或使用 replay 做审计。
