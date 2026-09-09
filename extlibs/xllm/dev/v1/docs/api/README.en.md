# xllm API Index

> Formal API reference entry point. The pages are organized by how you use xllm, not by source file order.

[Back to Documentation Center](../README.en.md)

---

## 1. Basic Types and Runtime

| Module | Document | Description |
| --- | --- | --- |
| Types | [types.en.md](types.en.md) | Core enums, error codes, log levels, trace kinds, and public object conventions. |
| Core Runtime | [api-core.en.md](api-core.en.md) | Runtime, profile, adapter registration, and sync/async chat entry points. |
| Request / Response | [api-request-response.en.md](api-request-response.en.md) | Requests, output items, JSON output, tool calls, artifacts, and resource cleanup. |
| Diagnostics | [api-diagnostics.en.md](api-diagnostics.en.md) | Log callbacks, trace callbacks, error objects, and diagnostic events. |

## 2. Providers and Model Capabilities

| Module | Document | Description |
| --- | --- | --- |
| Providers | [api-providers.en.md](api-providers.en.md) | Built-in provider adapters, capabilities, profile configuration, and capability checks. |
| Release | [api-release.en.md](api-release.en.md) | Versioning, release bundles, verification scripts, and consumer-side checks. |

## 3. Sessions and Tool Calls

| Module | Document | Description |
| --- | --- | --- |
| Session | [api-session.en.md](api-session.en.md) | Short-term conversation history, compact, state export/import, and session chat. |
| Tools | [api-tools.en.md](api-tools.en.md) | Tool definitions, tool choice, sync/async tool executors, and tool results. |

## 4. Memory and RAG

| Module | Document | Description |
| --- | --- | --- |
| Memory | [api-memory.en.md](api-memory.en.md) | Memory store lifecycle, scope, profile, record/chunk basics. |
| Memory Ingest | [api-memory-ingest.en.md](api-memory-ingest.en.md) | Text, file, directory, workspace, conversation summary, task/fact/preference ingest. |
| Memory Search | [api-memory-search.en.md](api-memory-search.en.md) | Search, list, debug, context apply, delete, and lifecycle maintenance. |
| Memory Workspace | [api-memory-workspace.en.md](api-memory-workspace.en.md) | Workspace sync, status, health check, and change sets. |
| Memory Watcher | [api-memory-watcher.en.md](api-memory-watcher.en.md) | File event queue, watcher bridge, pump, and worker. |
| Memory Bridge | [api-memory-bridge.en.md](api-memory-bridge.en.md) | Opt-in session + memory convenience composition; not a replacement for host policy. |

## Recommended API Reading Order

1. [types.en.md](types.en.md)
2. [api-core.en.md](api-core.en.md)
3. [api-request-response.en.md](api-request-response.en.md)
4. [api-providers.en.md](api-providers.en.md)
5. [api-session.en.md](api-session.en.md)
6. [api-tools.en.md](api-tools.en.md)
7. [api-memory.en.md](api-memory.en.md)
8. [api-memory-ingest.en.md](api-memory-ingest.en.md)
9. [api-memory-search.en.md](api-memory-search.en.md)
10. [api-memory-bridge.en.md](api-memory-bridge.en.md)

## Boundary with Tutorials and Cases

This directory documents API contracts, call order, and ownership. To understand when to use a capability, read the [Tutorials](../guide/README.en.md). To see full combinations, read the [Case Studies](../case/README.en.md).

## API Documentation Standard

Each API module page follows the level of detail used by `D:\git\xrt\docs\api\api-time.md`. Pages should not be only overviews or function lists.

New API pages should start from [API_PAGE_TEMPLATE.en.md](API_PAGE_TEMPLATE.en.md).

Each module page should include:

- Constants, macros, enums, and structs.
- A function catalog organized by task.
- One section for each public function.
- Function prototype, parameters, return value, ownership, notes, and examples.
- Common mistakes, related APIs, related tutorials, and related cases.

Each function section should include:

- **Purpose**: what the function solves and when to use it.
- **Prototype**: the exact C prototype copied from the public header.
- **Parameters**: input/output direction, nullability, lifetime, ownership, units, ranges, and defaults.
- **Return Value**: success/failure semantics, error object behavior, and whether partial output is possible.
- **Ownership**: who allocates, who frees, and which `reset/free/destroy` function to call.
- **Notes**: call order, thread-safety expectations, provider or memory scheme differences, and compatibility notes.
- **Example**: a practical code snippet; complex flows can link to `case/`.

Only mark an API page complete in `XLLM_DOCUMENTATION_SPEC.md` after it covers every assigned public `XLLM_API` function with this level of detail.
