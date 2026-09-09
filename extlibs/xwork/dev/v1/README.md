# xwork

`xwork` 是位于 `xllm` 之上的 C 语言 Agent 工作流运行时库。

它的目标是把 AI IDE、命令行自主 Agent、远程 Worker 和可恢复任务流中会重复出现的能力沉淀为通用基础设施，供后续 `xcode`、`xclaw` 等产品复用。

## 定位

默认分层如下：

```text
xrt
xllm
xwork
xcode / xclaw
```

`xllm` 负责统一模型调用、provider 适配、session、memory 和流式响应。

`xwork` 负责围绕工作区、工具、审批、任务状态、artifact、持久化、远程执行和 replay 去编排模型调用。

## 核心能力

| 能力 | 说明 |
| --- | --- |
| Runtime / Workspace | 管理运行时、工作区、profile、共享 xllm runtime 和 workspace memory。 |
| Tool Registry | 注册模型可调用工具，统一工具定义、参数、执行器和取消上下文。 |
| Orchestrator | 驱动 model turn + tool loop，处理工具调用、审批暂停、恢复和最终 summary。 |
| Policy / Approval | 对文件、进程、网络、终端和高风险操作做策略判断与审批请求建模。 |
| Host Tools | 内置 filesystem、process、terminal、vcs、editor host tool contract。 |
| Artifacts | 记录文件内容、patch、命令输出、终端状态、诊断和结构化报告。 |
| Persistence | 保存 run、event、checkpoint、artifact、agent graph、remote plane 和 replay cassette。 |
| Multi-Agent | 提供 in-process agent pool、task graph、依赖调度、handoff 和恢复边界。 |
| Remote Worker | 提供 control plane、worker registry、lease、assignment、artifact/output chunk 协议对象。 |
| Deterministic Replay | 记录和重放模型、工具、checkpoint、filesystem snapshot/ref，并报告 divergence。 |

## 当前阶段能力边界

已经可用：

- 单 run 的 runtime/workspace/tool/orchestrator 闭环。
- xllm model turn + tool loop、流式事件、审批暂停/恢复和异步取消。
- 本地 filesystem/process/terminal/vcs/editor host tool contract。
- artifact、checkpoint、file persistence 和 run/event/artifact 查询。
- in-process multi-agent task graph。
- in-process remote worker/control plane 对象和 decoded HTTP transport 边界。
- deterministic replay cassette、filesystem snapshot/ref 和 divergence report。

仍需由宿主产品完成的生产化集成：

- UI、CLI、IDE 面板和人机交互。
- 真实网络 server/client、worker auth、tenant/project 隔离和部署控制面。
- 产品级 autonomous planner 策略。
- 外部数据库、对象存储或分布式多写者 persistence backend。
- 模型 provider 具体行为漂移的长期运维。

## 适用场景

适合：

- AI IDE 的 Agent runtime。
- claw 类命令行自主 Agent。
- 可审批、可恢复的工具执行编排。
- 多 Agent 任务图和本地/远程 worker 调度。
- Agent run 的 artifact、checkpoint、replay 和审计。

不适合直接作为：

- 完整 AI IDE 产品。
- 完整云端 control plane。
- 模型 provider SDK 的替代品。
- 无需审计和恢复的简单 chat wrapper。

## 文档入口

- [文档中心](docs/README.md)
- [English README](README.en.md)
- [API 文档索引](docs/api/README.md)
- [教程索引](docs/guide/README.md)
- [范例索引](docs/case/README.md)
- [开发与设计资料](dev/docs/README.md)

建议阅读顺序：

1. 先读 [文档中心](docs/README.md)，确认 xwork 的文档分区。
2. 再读 [第一个 xwork 程序](docs/guide/first-xwork-program.md)，理解最小 runtime/workspace/run。
3. 如果接入 AI IDE 或 claw，读 [xllm 编排与工具循环](docs/guide/xllm-orchestrator-intro.md)。
4. 如果需要多 Agent、远程 Worker 或 replay，读对应的范例解析。
5. 最后查 [API 文档索引](docs/api/README.md) 和 `xwork.h`。

## 快速编译检查

从仓库根目录执行：

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -c xwork.c
```

运行示例可参考 [examples/README.md](examples/README.md)。

## 依赖关系

| 依赖 | 说明 |
| --- | --- |
| `xrt` | 基础运行时、文件、进程、终端和平台能力。 |
| `xllm` | provider、模型请求/响应、session、memory 和流式事件。 |
| `sqlite` | 当前仓库示例和 smoke 使用的本地依赖之一。 |
| 平台库 | Windows 示例通常链接 `ws2_32`、`iphlpapi` 等系统库。 |

仓库的 `lib/` 目录保存当前 xwork 构建所需的依赖快照。

## 目录结构

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

关键目录说明：

| 路径 | 说明 |
| --- | --- |
| `xwork.h` | 公共 API。 |
| `xwork.c` | 聚合实现入口。 |
| `src/` | 各模块内部实现。 |
| `examples/` | 可运行集成范例。 |
| `tests/` | smoke 和行为验证。 |
| `docs/` | 面向使用者的正式文档。 |
| `dev/` | 设计、开发计划、历史跟踪和内部说明。 |

## 文档语言策略

文档先生成中文主稿，人工审阅稳定后再翻译英文文档。

英文文档使用 `.en.md` 后缀，与中文文档保持一一对应；中文文档是后续改动的主源。
