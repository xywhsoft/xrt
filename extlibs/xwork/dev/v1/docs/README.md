# xwork 文档中心

> 面向 xwork 使用者的正式文档入口。开发计划、历史记录和内部设计资料保留在 `dev/`。

## 快速入口

- [API 文档索引](api/README.md)
- [English documentation center](README.en.md)
- [教程索引](guide/README.md)
- [范例索引](case/README.md)
- [架构说明](ARCHITECTURE.md)
- [最佳实践](BEST_PRACTICES.md)
- [FAQ](FAQ.md)
- [迁移指南](MIGRATION.md)
- [集成与打包](INTEGRATION.md)
- [文档 Review Checklist](DOCS_REVIEW_CHECKLIST.md)
- [仓库示例说明](../examples/README.md)
- [根 README](../README.md)

## 按目标阅读

| 目标 | 建议阅读 |
| --- | --- |
| 第一次了解 xwork | [第一个 xwork 程序](guide/first-xwork-program.md) |
| 做 AI IDE 集成 | [xllm 编排与工具循环](guide/xllm-orchestrator-intro.md)、[AI IDE Agent 范例](case/ai-ide-agent.md) |
| 做 claw / 自主 Agent | [工具、审批与 artifact](guide/tool-approval-artifact-intro.md)、[claw 自主 Agent 范例](case/claw-autonomous-agent.md) |
| 做多 Agent 调度 | [多 Agent 任务图](guide/multi-agent-intro.md)、[多 Agent claw 范例](case/multi-agent-claw.md) |
| 做远程执行 | [远程 Worker 与控制平面](guide/remote-worker-intro.md)、[Remote Worker 范例](case/remote-worker-agent.md) |
| 做可恢复和 replay | [持久化、checkpoint 与 replay](guide/persistence-replay-intro.md)、[Replay 范例](case/replay-agent-run.md) |
| 查公共接口 | [API 文档索引](api/README.md)、[`xwork.h`](../xwork.h) |
| 理解架构取舍 | [架构说明](ARCHITECTURE.md)、[FAQ](FAQ.md) |
| 接入构建系统 | [集成与打包](INTEGRATION.md) |

## 文档分区

| 分区 | 内容 |
| --- | --- |
| `docs/api/` | 公共 API 的模块化说明，按使用模型组织。 |
| `docs/guide/` | 教程和概念引导，解释如何把 xwork 接入产品。 |
| `docs/case/` | 可运行范例解析，说明实际 Agent 闭环如何拼装。 |
| `docs/*.md` | 架构、最佳实践、FAQ、迁移和集成说明。 |
| `dev/` | 设计文档、开发 spec、历史跟踪和内部 contract。 |

## 文档成熟度

| 状态 | 说明 |
| --- | --- |
| 中文初稿 | 已按当前 API 和示例生成，等待人工审阅。 |
| 待审阅 | 内容结构稳定，但还需要人工确认措辞、示例和边界。 |
| 稳定 | 可作为翻译英文文档和发布文档的主源。 |

当前正式文档默认对应：

- 当前仓库中的 [`xwork.h`](../xwork.h)。
- `XWORK_PERSISTENCE_FORMAT_VERSION` 当前值。
- `XWORK_REMOTE_PROTOCOL_VERSION_CURRENT` 当前值。
- `examples/` 和 `tests/` 中的当前可运行示例。

发现文档问题时，优先判断它属于：

- API 名称或字段不匹配。
- 链接或路径失效。
- 代码片段不能反映当前 API。
- 正式文档与 `dev/` 内部 contract 事实冲突。

## 编写约定

- 中文文档先行，英文文档在中文主稿审阅后再生成。
- API 文档以对象所有权、生命周期、错误返回、线程边界和恢复边界为重点。
- 教程文档优先解释最小调用顺序，再解释可扩展点。
- 范例文档必须能对应到 `examples/` 下的可运行代码。
- 开发状态、计划和未稳定的内部细节不放入正式文档中心，保留在 `dev/`。
- 新增 public API、example、persistence format、remote protocol 或内置 host tool 时，必须同步更新对应正式文档。
