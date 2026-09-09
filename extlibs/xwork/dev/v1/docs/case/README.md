# xwork 范例索引

范例文档解释 `examples/` 下可运行程序的目标、流程、关键 API 和可扩展点。

English version: [xwork Examples](README.en.md)

## 当前范例

| 范例 | 说明 |
| --- | --- |
| [第一个 xwork 程序](first-xwork-program.md) | 最小 runtime/workspace/run 生命周期。 |
| [AI IDE Agent](ai-ide-agent.md) | 使用 `xcode` profile 跑通文件读取、dry-run patch、审批和最终报告。 |
| [claw 自主 Agent](claw-autonomous-agent.md) | 使用 `xclaw` profile 跑通 autonomous process.exec、artifact、持久化和恢复。 |
| [多 Agent claw](multi-agent-claw.md) | 使用 agent pool 和 task graph 跑通 planner/coder/tester/reviewer fan-out/fan-in。 |
| [Remote Worker Agent](remote-worker-agent.md) | 使用 control plane 和 worker loop 跑通远程任务 claim/execute/complete/recovery。 |
| [Replay Agent Run](replay-agent-run.md) | 记录和重放模型、工具、checkpoint 和 filesystem ref，并生成 divergence report。 |

## 如何选择范例

| 你要验证 | 推荐范例 |
| --- | --- |
| 最小 runtime/workspace/run 生命周期 | [第一个 xwork 程序](first-xwork-program.md) |
| AI IDE 审批和 patch UI | [AI IDE Agent](ai-ide-agent.md) |
| 自主命令执行、artifact 和恢复 | [claw 自主 Agent](claw-autonomous-agent.md) |
| 多角色任务拆分和 fan-out/fan-in | [多 Agent claw](multi-agent-claw.md) |
| worker 分发、lease 和远程结果 | [Remote Worker Agent](remote-worker-agent.md) |
| replay、strict/audit 和 divergence | [Replay Agent Run](replay-agent-run.md) |

## 运行入口

构建命令见 [examples/README.md](../../examples/README.md)。
