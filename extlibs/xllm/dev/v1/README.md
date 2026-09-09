# xllm

xllm 是一个以跨平台为目标的 C 语言 LLM 集成库。它面向需要把大模型能力接入本地应用、AI IDE、Agent 宿主或自动化工具的开发者，提供统一的 provider 调用、会话管理、工具循环、本地 memory/RAG 和诊断能力。

项目的目标不是再封装一个简单 HTTP 客户端，而是把真实 LLM 应用里反复出现的能力整理成稳定的 C API：你可以用同一套结构描述请求、模型能力、工具调用、短期历史、长期记忆、错误和发布验证。

## 适合什么场景

- 在 C/C++ 宿主程序中调用多个 LLM provider。
- 为聊天窗口、命令行助手或 AI IDE 保存短期对话历史。
- 让模型通过 tool call 调用宿主提供的文件、检索、测试或业务工具。
- 把项目文档、源码片段、对话摘要和用户偏好写入本地 memory，再按问题检索注入上下文。
- 为 release bundle、下游消费和 smoke 矩阵建立可重复验证流程。

## 核心能力

| 能力 | 说明 |
| --- | --- |
| Core Runtime | 管理 runtime、adapter、profile、同步/异步聊天入口和错误对象。 |
| Provider/Profile | 用统一 profile 描述 provider、认证、base URL、模型和 capability。 |
| Request/Response | 支持文本、多模态 part、JSON 输出、tool call、usage、artifact 和流式事件。 |
| Session | 保存短期对话历史，支持 compact、summary、state export/import。 |
| Tool Loop | 支持模型发出 tool call，由宿主 executor 执行工具并回传 tool result。 |
| Memory/RAG | 支持文本、文件、目录、工作区、会话摘要、task/fact/preference 的写入、搜索和上下文注入。 |
| Workspace Sync | 支持工作区 ingest/sync、文件事件队列、watcher bridge/pump/worker。 |
| Diagnostics | 支持 `xllm_error`、log callback、trace callback、memory diagnostics 和 release gate 报告。 |

## 文档入口

正式文档从 [docs/README.md](docs/README.md) 开始。

建议阅读顺序：

1. [从零开始写第一个 xllm 程序](docs/guide/first-xllm-program.md)
2. [Provider 与 Profile 入门](docs/guide/provider-profile-intro.md)
3. [Request / Response 入门](docs/guide/request-response-intro.md)
4. [Session 入门](docs/guide/session-intro.md)
5. [Tool Loop 入门](docs/guide/tool-loop-intro.md)
6. [Memory RAG 入门](docs/guide/memory-rag-intro.md)
7. [范例解析](docs/case/README.md)
8. [API 索引](docs/api/README.md)

如果你要做 AI IDE 或 Agent 集成，建议直接阅读：

- [xllm 架构说明](docs/ARCHITECTURE.md)
- [AI IDE / xwork Agent Loop 集成](docs/case/ai-ide-agent-loop.md)
- [用 Memory 做工作区 RAG](docs/case/workspace-rag.md)
- [用 Tool Loop 写一个最小 Agent](docs/case/tool-loop-agent.md)

## 快速开始

### 环境要求

当前仓库已经整理好的本地构建和发布验证入口主要是 Windows 脚本：

- Windows
- PowerShell
- MinGW-w64 `gcc` 在 `PATH` 中

xllm 的代码和依赖选型以跨平台为目标。其他平台可以按相同源码和依赖边界接入自己的构建系统；仓库后续也会继续补齐更通用的构建入口。

部分功能需要额外依赖：

- 使用 SQLite-backed memory 时需要 SQLite 源码或库。
- 使用 `sqlite-vec` 加速向量候选检索时，需要先构建本地扩展。
- 运行真实 provider 示例时，需要对应 provider 的 API key。
- 使用内置 E5 embedding smoke 时，需要 ONNX Runtime 和 `multilingual-e5-small` 资产。

### 构建 single-head

```powershell
cmd /c .\build.bat singlehead
```

### 查看 smoke 矩阵

```powershell
cmd /c .\build.bat smoke-list
```

### 运行核心 smoke 子集

```powershell
cmd /c .\build.bat smoke -Filter "core,session"
```

### 构建 GLM 示例

```powershell
cmd /c .\examples\glm\build.bat
```

运行真实 GLM 示例前设置：

```powershell
$env:GLM_API_KEY="你的 GLM API Key"
```

### 构建 Agent 和 AI IDE Memory 示例

```powershell
cmd /c .\examples\agent_loop\build.bat
cmd /c .\examples\ai_ide_memory\build.bat
```

## 发布和验证

生成 release bundle：

```powershell
cmd /c .\build.bat release-bundle -VerifyCompile
```

验证已有 release artifact：

```powershell
cmd /c .\build.bat verify-artifact -RootDir build\release_bundle -VerifyCompile
```

运行下游消费 smoke：

```powershell
cmd /c .\build.bat downstream-smoke
```

运行发布总闸：

```powershell
cmd /c .\build.bat release-gate -RunDownstream
```

更多说明见 [Release Gate 入门](docs/guide/release-gate-intro.md) 和 [发布包消费案例](docs/case/release-bundle-consumption.md)。

## 目录结构

| 路径 | 说明 |
| --- | --- |
| `xllm.h` | core runtime、provider/profile、request/response、diagnostics API。 |
| `xllm-session.h` | session、turn helper、compact、state、tool executor 入口。 |
| `xllm-memory.h` | memory store、ingest、search、workspace sync、watcher API。 |
| `xllm-memory-bridge.h` | 可选的 memory + session bridge 便捷层。 |
| `src/` | 内部实现模块。 |
| `examples/` | provider 示例、memory 示例、agent loop 示例和 smoke 源码。 |
| `tests/smoke/` | 本地 smoke 矩阵和构建脚本。 |
| `tests/eval/` | memory、context packer、session summary 等本地评测和 benchmark。 |
| `tools/` | single-head、static analysis、release bundle、release gate、artifact 验证等工具。 |
| `docs/` | 面向使用者的正式文档体系，中文为主文档，英文版使用 `.en.md` 文件。 |
| `dev/docs/` | 旧设计和开发文档归档。 |
| `XLLM_DOCUMENTATION_SPEC.md` | 当前文档重建进度跟踪。 |
| `XLLM_AGENT_INFRA_SPEC.md` | Agent infrastructure 路线和进度跟踪。 |

## 当前文档语言策略

当前正式文档先以中文版本完成并审阅，英文翻译以 `.en.md` 文件提供。

本仓库当前 README 是中文项目介绍版，英文版见 [README.en.md](README.en.md)。

## 许可证和状态

xllm 当前处于持续演进阶段，核心 runtime、session、memory、tool loop、workspace sync 和 release gate 已形成可学习的公开文档体系。使用前建议先阅读 [最佳实践](docs/BEST_PRACTICES.md)，并根据你的 provider、模型能力和宿主权限策略完成自己的集成验证。
