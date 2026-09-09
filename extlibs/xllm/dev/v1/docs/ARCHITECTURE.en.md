# xllm Architecture

xllm is an LLM integration layer for C/C++ host programs. It organizes provider calls, conversation history, tool loops, memory/RAG, and diagnostics into one unified API.

[Back to Documentation Center](README.en.md)

## One-Sentence View

You can think of xllm as an adaptation layer between a host program and multiple LLM providers:

```text
Application / IDE / Agent host
  -> xllm runtime
    -> provider adapter + profile
    -> request / response
    -> session
    -> tool loop
    -> memory / RAG
    -> diagnostics
  -> OpenAI / GLM / Qwen / Kimi / Gemini / Ollama / ...
```

xllm does not replace your business system. It standardizes the common parts of LLM calls so you can describe requests, responses, tools, memory, and errors through one structure set.

## Core Object Relationships

| Object | How to Understand It |
| --- | --- |
| `xllm_runtime` | The xllm runtime environment inside an application; stores adapters, profiles, logs, and trace settings. |
| `xllm_profile` | Provider/model connection configuration, including base URL, auth, model name, and capabilities. |
| `xllm_adapter` | Provider adapter that converts xllm requests into provider requests. |
| `xllm_request` | Full low-level request structure for advanced calls and precise control. |
| `xllm_turn` | Convenient per-turn request object for sessions and normal chat. |
| `xllm_response` | Model response, including text, JSON, tool calls, usage, and errors. |
| `xllm_session` | Short-term conversation history manager with compact and state import/export support. |
| `xllm_memory` | Long-term knowledge and memory store with ingest, search, list, and context apply. |
| `xllm_tool_executor` | Tool execution callback provided by the host. |
| `xllm_error` | Failure diagnostic object. |

## Runtime, Adapter, and Profile

Runtime is the container for provider configuration. You usually create the runtime first, then register adapters and profiles:

```c
xllm_runtime_create(NULL, &runtime);
xllm_register_glm_native_adapter(runtime);
xllm_register_profile(runtime, &profile);
```

An adapter describes how to communicate with a provider. A profile describes which model to call, which auth to use, and which capabilities are supported.

The boundaries are important:

- Runtime owns lifecycle and global callbacks.
- Adapter owns protocol conversion.
- Profile owns model and account configuration.

## Request / Response Layer

Request/Response is the lowest-level call model in xllm. It is useful when you need full control over messages, tools, response format, context blocks, and call options.

Basic flow:

```text
xllm_request
  -> xllm_validate_request
  -> xllm_chat_ex
  -> xllm_response
```

If you only send one user text turn, use the more convenient `xllm_turn` and `xllm_send_ex`.

## Session Layer

Session manages short-term history. It is for continuous conversation scenarios such as chat windows, AI IDE panels, and command-line assistants.

```text
xllm_session
  turn 1 -> history
  turn 2 -> history + new turn
  compact -> summary/truncate
```

Session solves "context inside the current conversation." It is not long-term memory. Knowledge that must survive across days, sessions, or projects should use memory.

Continue with: [Session Introduction](guide/session-intro.en.md), [Session Chat Case](case/session-chat.en.md)

## Tool Loop Layer

Tool loop lets the model ask the host to execute tools:

```text
model emits tool call
  -> xllm calls executor
  -> host executes tool
  -> tool result returns to model
  -> model emits final answer
```

The model should not execute commands or edit files directly. Every tool call must go through host validation, permission control, and auditing.

Continue with: [Tool Loop Introduction](guide/tool-loop-intro.en.md), [Tool Loop Agent Case](case/tool-loop-agent.en.md)

## Memory / RAG Layer

Memory manages long-term knowledge and retrievable context. It can ingest:

- Project documents and source snippets.
- Conversation summaries.
- User preferences.
- Tasks, facts, and durable memory.

Typical RAG flow:

```text
ingest text/file/workspace
  -> search
  -> apply to request/turn
  -> model answer with retrieved context
```

Continue with: [Memory RAG Introduction](guide/memory-rag-intro.en.md), [Workspace RAG Case](case/workspace-rag.en.md)

## Diagnostics Layer

xllm provides three diagnostic surfaces:

- `xllm_error`: error details for a failed call.
- log callback: runtime logs.
- trace callback: structured request, response, stream, compact, and tool loop events.

Memory also provides a diagnostics snapshot for checking scheme, SQLite, embedder, and record/chunk counts.

Continue with: [Diagnostics Introduction](guide/diagnostics-intro.en.md)

## Recommended Combinations

| Application | Recommended Combination |
| --- | --- |
| Minimal chat | runtime + adapter + profile + xllm turn |
| Chat window | runtime + profile + session |
| Tool agent | session + tool executor + diagnostics |
| Documentation Q&A | memory ingest/search + request |
| AI IDE | session + workspace memory + tool loop + diagnostics |
| Release bundle verification | release gate + downstream smoke |

## Learning Path

If you are starting out:

1. [Your First xllm Program](guide/first-xllm-program.en.md)
2. [Provider and Profile Introduction](guide/provider-profile-intro.en.md)
3. [Request / Response Introduction](guide/request-response-intro.en.md)
4. [Minimal Chat Call](case/minimal-chat.en.md)

If you are building an AI IDE or agent:

1. [Session Introduction](guide/session-intro.en.md)
2. [Tool Loop Introduction](guide/tool-loop-intro.en.md)
3. [Memory RAG Introduction](guide/memory-rag-intro.en.md)
4. [AI IDE / xwork Agent Loop Integration](case/ai-ide-agent-loop.en.md)
