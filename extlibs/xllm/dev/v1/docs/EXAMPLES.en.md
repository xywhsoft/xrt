# xllm Examples Guide

This page explains which examples under `examples/` are good entry points, which ones are for provider probes, memory smoke tests, or release verification.

[Back to Documentation Center](README.en.md)

## Which Examples to Read First

If you are learning xllm for the first time, read in this order:

1. `examples/glm/glm_stateless.c`: minimal single-turn chat.
2. `examples/glm/glm_session.c`: session with short-term history.
3. `examples/smoke_auto_tool_loop.c`: minimal model of automatic tool loop.
4. `examples/memory/memory_basic.c`: broad memory ingest/search/list/apply coverage.
5. `examples/conversation_memory/conversation_memory.c`: explicit conversation memory.
6. `examples/ai_ide_memory/ai_ide_memory.c`: workspace RAG and citation injection.
7. `examples/agent_loop/agent_loop.c`: session + memory + tool executor agent loop.

Related case documents:

- [Minimal Chat Call](case/minimal-chat.en.md)
- [Session Chat](case/session-chat.en.md)
- [Tool Loop Agent](case/tool-loop-agent.en.md)
- [Workspace RAG](case/workspace-rag.en.md)
- [Conversation Memory](case/conversation-memory.en.md)
- [AI IDE Agent Loop](case/ai-ide-agent-loop.en.md)

## Provider Examples

These directories demonstrate real provider usage:

| Directory | Content |
| --- | --- |
| `examples/glm/` | GLM native session, stateless, and tool loop examples. |
| `examples/qwen/` | Qwen session and tool loop examples. |
| `examples/kimi/` | Kimi session and tool loop examples. |
| `examples/minimax/` | MiniMax session and tool loop examples. |
| `examples/doubao/` | Doubao session and tool loop examples. |
| `examples/gemini/` | Gemini / Vertex Gemini session and tool loop examples. |
| `examples/openai/` | OpenAI-compatible and Azure OpenAI paths. |
| `examples/anthropic/` | Anthropic-related session examples. |

Provider examples usually require the corresponding API key environment variables. Do not put keys in source code.

## Smoke Examples

Many `examples/smoke_*.c` files are minimal verification programs for specific behavior. They are useful when you need to check how one feature should work.

Common categories:

| Prefix / Name | Purpose |
| --- | --- |
| `smoke_chat_ex_low_level.c` | Low-level `xllm_chat_ex` calls. |
| `smoke_auto_tool_loop.c` | Automatic tool loop. |
| `smoke_session_*.c` | Session history, compact, state, and summary. |
| `smoke_memory_ingest_*.c` | Memory writes. |
| `smoke_memory_search_*.c` | Memory search and context injection. |
| `smoke_memory_workspace_*.c` | Workspace ingest/sync/status. |
| `smoke_memory_watcher_*.c` | File event queue, watcher bridge/pump/worker. |
| `smoke_*_native_*` | Provider native adapter behavior. |
| `smoke_real_provider_probe_*` | Real provider probes. |

Smoke examples are not tutorial articles, but they are very useful for checking API behavior and edge cases.

## Memory Examples

`examples/memory/memory_basic.c` has the broadest coverage. It demonstrates:

- Creating memory.
- Using a builtin embedder.
- Text, file, directory, and workspace ingest.
- Search, list records, and list chunks.
- Applying search results to a request.
- Workspace sync, change sets, and watcher-related paths.

If you only want a small scenario, read these first:

- `examples/conversation_memory/conversation_memory.c`
- `examples/ai_ide_memory/ai_ide_memory.c`

Read `memory_basic.c` when you need the full set of options.

## Agent Example

`examples/agent_loop/agent_loop.c` demonstrates a combined scenario:

- A mock adapter simulates a model that emits a tool call first and a final answer later.
- Memory provides agent policy context.
- Session stores the conversation.
- Tool executor runs `xwork.workspace.inspect`.
- The loop writes summary memory after the conversation.

Build:

```bat
cmd /c .\examples\agent_loop\build.bat
```

## AI IDE Memory Example

`examples/ai_ide_memory/ai_ide_memory.c` demonstrates:

- Creating a mock workspace.
- Indexing code and docs.
- Skipping hidden `.env`.
- Searching relevant snippets.
- Injecting a context block with source/chunk/bytes references.

Build:

```bat
cmd /c .\examples\ai_ide_memory\build.bat
```

## Choosing an Example

| Goal | Recommended Example |
| --- | --- |
| Get the first provider request working | `examples/glm/glm_stateless.c` |
| Learn session | `examples/glm/glm_session.c`, `smoke_session_history.c` |
| Learn compact | `smoke_session_compact.c`, `smoke_session_auto_compact.c` |
| Learn tool loop | `smoke_auto_tool_loop.c`, provider `*_tool_loop.c` |
| Learn memory | `examples/memory/memory_basic.c` |
| Learn workspace RAG | `examples/ai_ide_memory/ai_ide_memory.c` |
| Learn long-term conversation memory | `examples/conversation_memory/conversation_memory.c` |
| Learn full agent loop | `examples/agent_loop/agent_loop.c` |
| Learn release bundle consumption | [Release Bundle Consumption Case](case/release-bundle-consumption.en.md) |

## Notes When Reading Examples

- Some `smoke_*.c` files use a mock adapter to verify xllm behavior; that does not represent real provider output.
- Real provider examples require API keys and network access.
- Memory examples may use SQLite source and Windows system libraries.
- Schemas, error handling, and permission controls in examples may be simplified. Production applications should follow the best practices.

## Next Steps

- For systematic learning, read the [Tutorials](guide/README.en.md).
- For complete combinations, read the [Case Studies](case/README.en.md).
- For function details, read the [API Index](api/README.en.md).
