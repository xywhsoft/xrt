# xllm 示例说明

本文按学习顺序解释 `examples/` 中哪些示例适合入门，哪些适合 provider probe、memory smoke 或发布验证。

[返回文档中心](README.md)

## 先读哪些示例

如果你是第一次学习 xllm，建议按这个顺序：

1. `examples/glm/glm_stateless.c`：最小单轮聊天。
2. `examples/glm/glm_session.c`：带短期历史的 session。
3. `examples/smoke_auto_tool_loop.c`：自动 tool loop 的最小模型。
4. `examples/memory/memory_basic.c`：memory ingest/search/list/apply 的大集合。
5. `examples/conversation_memory/conversation_memory.c`：显式 conversation memory。
6. `examples/ai_ide_memory/ai_ide_memory.c`：工作区 RAG 和引用注入。
7. `examples/agent_loop/agent_loop.c`：session + memory + tool executor 的 agent loop。

对应案例文档：

- [最小聊天调用](case/minimal-chat.md)
- [Session Chat](case/session-chat.md)
- [Tool Loop Agent](case/tool-loop-agent.md)
- [Workspace RAG](case/workspace-rag.md)
- [Conversation Memory](case/conversation-memory.md)
- [AI IDE Agent Loop](case/ai-ide-agent-loop.md)

## Provider 示例

这些目录展示真实 provider 的调用方式：

| 目录 | 内容 |
| --- | --- |
| `examples/glm/` | GLM native session、stateless、tool loop |
| `examples/qwen/` | Qwen session 和 tool loop |
| `examples/kimi/` | Kimi session 和 tool loop |
| `examples/minimax/` | MiniMax session 和 tool loop |
| `examples/doubao/` | Doubao session 和 tool loop |
| `examples/gemini/` | Gemini / Vertex Gemini session 和 tool loop |
| `examples/openai/` | OpenAI compatible 和 Azure OpenAI 路径 |
| `examples/anthropic/` | Anthropic 相关 session 示例 |

Provider 示例通常需要设置对应 API key 环境变量。不要把 key 写进源码。

## Smoke 示例

根目录下大量 `examples/smoke_*.c` 是针对某个行为的最小验证程序。它们适合你在调试时查“某个功能应该怎样工作”。

常见分类：

| 前缀/名称 | 用途 |
| --- | --- |
| `smoke_chat_ex_low_level.c` | 底层 `xllm_chat_ex` 调用 |
| `smoke_auto_tool_loop.c` | 自动工具循环 |
| `smoke_session_*.c` | session history、compact、state、summary |
| `smoke_memory_ingest_*.c` | memory 写入 |
| `smoke_memory_search_*.c` | memory 搜索与上下文注入 |
| `smoke_memory_workspace_*.c` | workspace ingest/sync/status |
| `smoke_memory_watcher_*.c` | file event queue、watcher bridge/pump/worker |
| `smoke_*_native_*` | provider native adapter 行为 |
| `smoke_real_provider_probe_*` | 真实 provider probe |

Smoke 示例不是教程文章，但它们非常适合核对 API 行为和边界条件。

## Memory 示例

`examples/memory/memory_basic.c` 覆盖范围最广。它展示：

- 创建 memory。
- 使用内置 embedder。
- 文本、文件、目录、工作区 ingest。
- 搜索、list records、list chunks。
- 把搜索结果注入 request。
- workspace sync、change set、watcher 相关路径。

如果你只想学习一个小场景，先读：

- `examples/conversation_memory/conversation_memory.c`
- `examples/ai_ide_memory/ai_ide_memory.c`

如果你要查完整选项，再读 `memory_basic.c`。

## Agent 示例

`examples/agent_loop/agent_loop.c` 展示了一个组合场景：

- mock adapter 模拟模型先发 tool call 再 final answer。
- memory 提供 agent 策略上下文。
- session 保存对话。
- tool executor 执行 `xwork.workspace.inspect`。
- 对话结束后写回 summary memory。

构建：

```bat
cmd /c .\examples\agent_loop\build.bat
```

## AI IDE Memory 示例

`examples/ai_ide_memory/ai_ide_memory.c` 展示：

- 创建模拟 workspace。
- 索引代码和文档。
- 跳过隐藏 `.env`。
- 检索相关片段。
- 注入带 source/chunk/bytes 的 context block。

构建：

```bat
cmd /c .\examples\ai_ide_memory\build.bat
```

## 如何选择示例

| 目标 | 建议示例 |
| --- | --- |
| 跑通第一个 provider 请求 | `examples/glm/glm_stateless.c` |
| 学 session | `examples/glm/glm_session.c`、`smoke_session_history.c` |
| 学 compact | `smoke_session_compact.c`、`smoke_session_auto_compact.c` |
| 学工具循环 | `smoke_auto_tool_loop.c`、provider `*_tool_loop.c` |
| 学 memory | `examples/memory/memory_basic.c` |
| 学工作区 RAG | `examples/ai_ide_memory/ai_ide_memory.c` |
| 学长期对话记忆 | `examples/conversation_memory/conversation_memory.c` |
| 学完整 agent loop | `examples/agent_loop/agent_loop.c` |
| 学发布包消费 | [发布包消费案例](case/release-bundle-consumption.md) |

## 阅读示例时注意

- `smoke_*.c` 有时使用 mock adapter，目的是验证 xllm 行为，不代表真实 provider 输出。
- 真实 provider 示例需要 API key 和网络。
- Memory 示例可能使用 SQLite 源码和 Windows 系统库。
- 示例里的 schema、错误处理和权限控制为了简洁可能不完整，生产应用应参考最佳实践补齐。

## 下一步

- 想先系统学习，读 [教程入口](guide/README.md)。
- 想看完整组合，读 [范例解析](case/README.md)。
- 想查函数细节，读 [API 索引](api/README.md)。
