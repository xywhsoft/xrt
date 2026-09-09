# 持久化、checkpoint 与 replay

> 状态：中文初稿，待人工审阅。

xwork 的 run 以“可恢复”为默认设计目标。关键状态通过 event、checkpoint、snapshot、artifact 和 replay cassette 进入持久化层。

## 可持久化对象

| 对象 | 用途 |
| --- | --- |
| run snapshot | 保存 run 的最新生命周期状态、pending tool、approval decision 等。 |
| event | 保存模型、工具、审批、任务和调度事件。 |
| checkpoint | 保存可恢复执行点。 |
| artifact | 保存文件、patch、命令、终端、诊断和报告产物。 |
| agent graph snapshot | 保存 multi-agent task graph 和 handoff 状态。 |
| control plane snapshot | 保存 worker、remote task、lease 和队列状态。 |
| replay cassette | 保存模型、工具、checkpoint、filesystem snapshot/ref 和 divergence 信息。 |

## 恢复边界

可以恢复：

- run 的最新 snapshot。
- 等待审批或待执行工具调用的参数。
- 已生成的事件和 artifact。
- 已持久化的 agent graph、remote control plane 和 replay manifest。

不能自动恢复：

- 活跃 OS process handle。
- 本地交互式 terminal session 的 live 状态。
- 正在执行中的线程栈或 callback 栈。
- 外部系统已经发生但未记录的副作用。

## Checkpoint / Recovery 层

checkpoint 和 latest snapshot 解决“进程重启后如何继续”的问题。

典型流程：

```text
configure file persistence
execute run
store latest snapshot / checkpoint / events / artifacts
process exits
recreate runtime/workspace/tools/host services
recover latest run snapshot
resume pending tool or approval boundary
```

恢复依赖调用方重新提供兼容环境：runtime、workspace、tool registry、host services、persistence backend、xllm runtime/profile。snapshot 只保存可序列化状态。

## Replay 的作用

replay 用于复现或审计一次 Agent run 中的关键输入输出。它可以：

- 记录模型请求/响应、工具请求/响应和 checkpoint。
- 记录 filesystem snapshot/ref。
- 在 strict 模式下阻止未记录副作用。
- 在 audit 模式下比较新结果和旧记录。
- 输出 first divergence 和 divergence report artifact。

## Replay 层

replay 解决“同样的模型/工具/host 输入输出是否还能匹配”的问题。

典型流程：

```text
create replay engine in record mode
execute model/tool/host calls
store replay cassette
load replay engine in strict or audit mode
replay expected entries/events
query first divergence
emit divergence report artifact
```

strict 模式适合 CI 回归；audit 模式适合审计差异但继续收集更多 divergence。

checkpoint/recovery 和 replay 可以组合使用，但它们不是同一层能力：checkpoint 恢复运行状态，replay 对比输入输出和副作用边界。

## 相关范例

- [Replay Agent Run 范例](../case/replay-agent-run.md)
- [Remote Worker 范例](../case/remote-worker-agent.md)
- [Persistence API](../api/api-persistence.md)
- [Replay API](../api/api-replay.md)
