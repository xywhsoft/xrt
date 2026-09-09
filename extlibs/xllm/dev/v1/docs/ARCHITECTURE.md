# xllm 架构说明

xllm 是一个面向 C/C++ 宿主程序的 LLM 集成层。它把 provider 调用、会话历史、工具循环、memory/RAG 和诊断能力组织成一套统一 API。

[返回文档中心](README.md)

## 一句话理解

你可以把 xllm 理解为宿主程序和多个 LLM provider 之间的适配层：

```text
应用 / IDE / Agent 宿主
  -> xllm runtime
    -> provider adapter + profile
    -> request / response
    -> session
    -> tool loop
    -> memory / RAG
    -> diagnostics
  -> OpenAI / GLM / Qwen / Kimi / Gemini / Ollama / ...
```

xllm 不替代你的业务系统。它负责把 LLM 调用中共性的部分标准化，让你能用同一套结构描述请求、响应、工具、记忆和错误。

## 核心对象关系

| 对象 | 你可以怎样理解 |
| --- | --- |
| `xllm_runtime` | 一个应用里的 xllm 运行环境，保存 adapter、profile、日志和 trace 配置 |
| `xllm_profile` | 一个 provider/model 的连接配置，包括 base URL、认证、模型名和能力 |
| `xllm_adapter` | provider 适配器，把 xllm 请求转换成 provider 请求 |
| `xllm_request` | 底层完整请求结构，适合高级调用和精确控制 |
| `xllm_turn` | 一轮对话的便捷请求，适合 session 和普通聊天 |
| `xllm_response` | 模型响应，包括文本、JSON、工具调用、usage 和错误 |
| `xllm_session` | 短期对话历史管理，支持 compact 和 state 导入导出 |
| `xllm_memory` | 长期知识和记忆存储，支持 ingest、search、list、context apply |
| `xllm_tool_executor` | 宿主提供的工具执行回调 |
| `xllm_error` | 失败诊断对象 |

## Runtime、Adapter、Profile

Runtime 是所有 provider 配置的容器。你通常先创建 runtime，再注册 adapter 和 profile：

```c
xllm_runtime_create(NULL, &runtime);
xllm_register_glm_native_adapter(runtime);
xllm_register_profile(runtime, &profile);
```

Adapter 描述“怎么和 provider 通信”。Profile 描述“这次调用哪个模型、用什么认证、支持哪些能力”。

这三个对象的边界很重要：

- Runtime 管生命周期和全局回调。
- Adapter 管协议转换。
- Profile 管具体模型和账号配置。

## Request / Response 层

Request/Response 是 xllm 的最底层调用模型。它适合你需要完全控制消息、工具、response format、context block 和 call options 的场景。

基础流程：

```text
xllm_request
  -> xllm_validate_request
  -> xllm_chat_ex
  -> xllm_response
```

如果你只是发送一轮用户文本，可以使用更便捷的 `xllm_turn` 和 `xllm_send_ex`。

## Session 层

Session 管短期历史。它适合聊天窗口、AI IDE 面板、命令行助手等连续对话场景。

```text
xllm_session
  turn 1 -> history
  turn 2 -> history + new turn
  compact -> summary/truncate
```

Session 解决的是“当前对话里的上下文”。它不是长期记忆。跨天、跨会话、跨项目的知识应使用 memory。

继续阅读：[Session 入门](guide/session-intro.md)、[Session Chat 案例](case/session-chat.md)

## Tool Loop 层

Tool loop 让模型请求宿主执行工具：

```text
模型输出 tool call
  -> xllm 调用 executor
  -> 宿主执行工具
  -> tool result 回到模型
  -> 模型输出最终回答
```

模型不应该直接执行命令或修改文件。所有工具调用都要经过宿主校验、权限控制和审计。

继续阅读：[Tool Loop 入门](guide/tool-loop-intro.md)、[Tool Loop Agent 案例](case/tool-loop-agent.md)

## Memory / RAG 层

Memory 管长期知识和可检索上下文。它能写入：

- 项目文档和代码片段。
- 对话摘要。
- 用户偏好。
- 任务、事实和长期记忆。

典型 RAG 流程：

```text
ingest text/file/workspace
  -> search
  -> apply to request/turn
  -> model answer with retrieved context
```

继续阅读：[Memory RAG 入门](guide/memory-rag-intro.md)、[Workspace RAG 案例](case/workspace-rag.md)

## Diagnostics 层

xllm 提供三类诊断：

- `xllm_error`：一次失败的错误详情。
- log callback：运行时日志。
- trace callback：结构化请求、响应、流式、compact、tool loop 事件。

Memory 还提供 diagnostics 快照，用来检查 scheme、SQLite、embedder、record/chunk 数量。

继续阅读：[Diagnostics 入门](guide/diagnostics-intro.md)

## 推荐组合

| 应用 | 推荐组合 |
| --- | --- |
| 最小聊天 | runtime + adapter + profile + xllm turn |
| 聊天窗口 | runtime + profile + session |
| 工具 Agent | session + tool executor + diagnostics |
| 文档问答 | memory ingest/search + request |
| AI IDE | session + workspace memory + tool loop + diagnostics |
| 发布包验证 | release gate + downstream smoke |

## 学习路径

如果你刚开始：

1. [第一个 xllm 程序](guide/first-xllm-program.md)
2. [Provider 与 Profile 入门](guide/provider-profile-intro.md)
3. [Request / Response 入门](guide/request-response-intro.md)
4. [最小聊天调用](case/minimal-chat.md)

如果你要做 AI IDE 或 agent：

1. [Session 入门](guide/session-intro.md)
2. [Tool Loop 入门](guide/tool-loop-intro.md)
3. [Memory RAG 入门](guide/memory-rag-intro.md)
4. [AI IDE / xwork Agent Loop 集成](case/ai-ide-agent-loop.md)
