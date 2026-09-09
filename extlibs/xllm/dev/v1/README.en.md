# xllm

xllm is a C library for LLM integration with cross-platform goals. It is designed for developers who need to bring large-model capabilities into local applications, AI IDEs, agent hosts, or automation tools, while using one consistent layer for provider access, session management, tool loops, local memory/RAG, and diagnostics.

The project is not meant to be another thin HTTP wrapper. Its goal is to turn the recurring parts of real LLM applications into stable C APIs: requests, model capabilities, tool calls, short-term history, long-term memory, errors, and release verification can all be described through one set of structures.

## What It Is For

- Calling multiple LLM providers from a C/C++ host program.
- Keeping short-term conversation history for chat windows, command-line assistants, or AI IDE panels.
- Letting a model request host-provided tools through tool calls, such as file, retrieval, test, or business tools.
- Writing project documents, source snippets, conversation summaries, and user preferences into local memory, then retrieving relevant context for a request.
- Building repeatable verification flows for release bundles, downstream consumption, and smoke matrices.

## Core Capabilities

| Capability | Description |
| --- | --- |
| Core Runtime | Manages runtime, adapters, profiles, sync/async chat entry points, and error objects. |
| Provider/Profile | Describes provider, auth, base URL, model, and capability through a unified profile. |
| Request/Response | Supports text, multimodal parts, JSON output, tool calls, usage, artifacts, and stream events. |
| Session | Keeps short-term conversation history, with compact, summary, and state export/import support. |
| Tool Loop | Lets the model emit tool calls, while the host executor runs tools and returns tool results. |
| Memory/RAG | Supports ingesting, searching, and injecting text, files, directories, workspaces, conversation summaries, tasks, facts, and preferences. |
| Workspace Sync | Supports workspace ingest/sync, file event queues, watcher bridge/pump/worker flows. |
| Diagnostics | Provides `xllm_error`, log callbacks, trace callbacks, memory diagnostics, and release gate reports. |

## Documentation

Start from the formal documentation center: [docs/README.en.md](docs/README.en.md).

Recommended reading order:

1. [Your First xllm Program](docs/guide/first-xllm-program.en.md)
2. [Provider and Profile Introduction](docs/guide/provider-profile-intro.en.md)
3. [Request / Response Introduction](docs/guide/request-response-intro.en.md)
4. [Session Introduction](docs/guide/session-intro.en.md)
5. [Tool Loop Introduction](docs/guide/tool-loop-intro.en.md)
6. [Memory RAG Introduction](docs/guide/memory-rag-intro.en.md)
7. [Case Studies](docs/case/README.en.md)
8. [API Index](docs/api/README.en.md)

If you are building an AI IDE or agent integration, start with:

- [xllm Architecture](docs/ARCHITECTURE.en.md)
- [AI IDE / xwork Agent Loop Integration](docs/case/ai-ide-agent-loop.en.md)
- [Workspace RAG with Memory](docs/case/workspace-rag.en.md)
- [Minimal Agent with Tool Loop](docs/case/tool-loop-agent.en.md)

## Quick Start

### Requirements

The local build and release verification entry points that are currently documented in this repository mainly use Windows scripts:

- Windows
- PowerShell
- MinGW-w64 `gcc` on `PATH`

xllm's code and dependency choices target cross-platform support. Other platforms can wire the same source and dependency boundaries into their own build systems; the repository can continue adding more portable build entry points over time.

Some features need additional dependencies:

- SQLite source or library for SQLite-backed memory.
- A locally built `sqlite-vec` extension for accelerated vector candidate retrieval.
- Provider API keys for real provider examples.
- ONNX Runtime and `multilingual-e5-small` assets for builtin E5 embedding smoke coverage.

### Build the Single-Head Output

```powershell
cmd /c .\build.bat singlehead
```

### List the Smoke Matrix

```powershell
cmd /c .\build.bat smoke-list
```

### Run a Core Smoke Subset

```powershell
cmd /c .\build.bat smoke -Filter "core,session"
```

### Build the GLM Examples

```powershell
cmd /c .\examples\glm\build.bat
```

Set this before running real GLM examples:

```powershell
$env:GLM_API_KEY="your GLM API key"
```

### Build Agent and AI IDE Memory Examples

```powershell
cmd /c .\examples\agent_loop\build.bat
cmd /c .\examples\ai_ide_memory\build.bat
```

## Release and Verification

Build a release bundle:

```powershell
cmd /c .\build.bat release-bundle -VerifyCompile
```

Verify an existing release artifact:

```powershell
cmd /c .\build.bat verify-artifact -RootDir build\release_bundle -VerifyCompile
```

Run downstream consumption smoke:

```powershell
cmd /c .\build.bat downstream-smoke
```

Run the full release gate:

```powershell
cmd /c .\build.bat release-gate -RunDownstream
```

See [Release Gate Introduction](docs/guide/release-gate-intro.en.md) and [Release Bundle Consumption](docs/case/release-bundle-consumption.en.md) for more details.

## Repository Layout

| Path | Description |
| --- | --- |
| `xllm.h` | Core runtime, provider/profile, request/response, and diagnostics APIs. |
| `xllm-session.h` | Session, turn helpers, compact, state, and tool executor entry points. |
| `xllm-memory.h` | Memory store, ingest, search, workspace sync, and watcher APIs. |
| `xllm-memory-bridge.h` | Optional convenience layer for memory + session composition. |
| `src/` | Internal implementation modules. |
| `examples/` | Provider examples, memory examples, agent loop examples, and smoke sources. |
| `tests/smoke/` | Local smoke matrix and build scripts. |
| `tests/eval/` | Local eval and benchmark suites for memory, context packing, session summary, and related flows. |
| `tools/` | Single-head, static analysis, release bundle, release gate, and artifact verification tools. |
| `docs/` | Formal learner-facing documentation. |
| `dev/docs/` | Archived design and development documents. |
| `XLLM_DOCUMENTATION_SPEC.md` | Documentation rebuild progress tracker. |
| `XLLM_AGENT_INFRA_SPEC.md` | Agent infrastructure roadmap and progress tracker. |

## Documentation Language Policy

The formal documentation is written and reviewed in Chinese first. English translations are generated as `.en.md` files after the Chinese version has been reviewed.

This file is the English translation of the reviewed Chinese project README.

## Status

xllm is under active evolution. The core runtime, session layer, memory layer, tool loop, workspace sync, and release gate all have learner-facing public documentation. Before integration, read [Best Practices](docs/BEST_PRACTICES.en.md) and verify your provider, model capabilities, and host permission policy in your own environment.
