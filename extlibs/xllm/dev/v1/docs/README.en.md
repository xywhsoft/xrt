# xllm Documentation Center

> Formal entry point for xllm users. Start with `guide/` to build the usage model, use `api/` for public interfaces, and read `case/` for complete integration examples.

[Project Introduction](../README.en.md)

---

## Quick Links

- [API Index](api/README.en.md)
- [Tutorials](guide/README.en.md)
- [Case Studies](case/README.en.md)
- [Examples Guide](EXAMPLES.en.md)
- [Architecture](ARCHITECTURE.en.md)
- [Best Practices](BEST_PRACTICES.en.md)
- [FAQ](FAQ.en.md)
- [Migration Guide](MIGRATION.en.md)
- [Performance and Release Notes](PERFORMANCE.en.md)

## Read by Goal

### First Time with xllm

1. [Project Introduction](../README.en.md)
2. [Your First xllm Program](guide/first-xllm-program.en.md)
3. [Provider and Profile Introduction](guide/provider-profile-intro.en.md)
4. [Request / Response Introduction](guide/request-response-intro.en.md)
5. [Minimal Chat Case](case/minimal-chat.en.md)

### Building an AI IDE or Agent Integration

1. [xllm Architecture](ARCHITECTURE.en.md)
2. [Session Introduction](guide/session-intro.en.md)
3. [Tool Loop Introduction](guide/tool-loop-intro.en.md)
4. [Memory RAG Introduction](guide/memory-rag-intro.en.md)
5. [AI IDE Agent Loop Case](case/ai-ide-agent-loop.en.md)

### Using Local Memory and Workspace Indexing

1. [Memory API](api/api-memory.en.md)
2. [Memory Ingest API](api/api-memory-ingest.en.md)
3. [Memory Search API](api/api-memory-search.en.md)
4. [Workspace Index Introduction](guide/workspace-index-intro.en.md)
5. [Workspace RAG Case](case/workspace-rag.en.md)

### Delivering or Consuming Release Bundles

1. [Release Gate Introduction](guide/release-gate-intro.en.md)
2. [Release API / Tooling Notes](api/api-release.en.md)
3. [Release Bundle Consumption Case](case/release-bundle-consumption.en.md)

## Documentation Sections

- `api/`
  Public types, functions, lifetimes, ownership, error handling, and module boundaries.
- `guide/`
  Ordered learning material that explains when to use each capability and how to move from a minimal program to a full integration.
- `case/`
  Complete examples that combine core, session, provider, memory, tool loop, and release gate flows.

## How to Use These Docs

- If you are new to xllm, start with `guide/` instead of reading headers first.
- If you already know the module name, use `api/` for functions, structs, and call order.
- If you are integrating xllm into a real host, read `case/` for complete flows.
