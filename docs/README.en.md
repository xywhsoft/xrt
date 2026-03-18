# XRT Documentation Hub

> The current official documentation entry for XRT. API references are organized under `docs/api/`, leaving a clear structure for beginner guides, case studies, and topic-specific documents.

[中文](README.md) | [Project Overview](../../README.en.md)

---

## Documentation Structure

The current `docs/` tree is organized into 3 layers:

### 1. Documentation Hub

- Current page [README.en.md](README.en.md)

Responsibilities:

- Explain the documentation structure
- Provide a unified entry point
- Point to the current official mainline

### 2. API Reference

- [API Index](api/README.en.md)

Responsibilities:

- Types
- Core runtime
- Threads and coroutines
- future / task / promise / wait-source
- Memory and containers
- Data and text handling
- Current network mainline APIs

### 3. Supporting Documents

Responsibilities:

- Architecture
- feature trimming macros
- migration
- FAQ
- best practices
- examples
- performance notes
- beginner guides
- case studies

To keep API docs, guides, and case studies from being mixed together, the structure is explicitly defined as:

- `docs/api/`: API contracts, module capabilities, current mainline behavior
- `docs/guide/`: beginner guides, progressive tutorials, concept explanations
- `docs/case/`: complete cases, combined usage, real-world walkthroughs

When adding new documents later, decide first whether they are:

- contract / reference documents
- or guide / case documents

then place them in the proper subdirectory.


## Current Mainline Entry Points

### Project Positioning

- Root [README.en.md](../../README.en.md)

### API Reference

- [API Index](api/README.en.md)

The most important API mainline documents to read first are:

- [Base Runtime](api/api-base.en.md)
- [Thread Runtime](api/api-thread.en.md)
- [Coroutine Runtime](api/api-coroutine.en.md)
- [Future / Task / Promise](api/api-future-task-promise.en.md)
- [XNet v2](api/api-xnet-v2.en.md)
- [Network TLS](api/api-network-tls.en.md)
- [XHTTP](api/api-xhttp.en.md)
- [XHTTPD](api/api-xhttpd.en.md)
- [XWS](api/api-xws.en.md)

### Architecture and Baseline Documents

- [Architecture](ARCHITECTURE.en.md)
- [Feature Trimming Macros](FEATURE_TRIMMING_MACROS.en.md)

### Supporting Documents

- [Migration Guide](MIGRATION.en.md)
- [Best Practices](BEST_PRACTICES.en.md)
- [FAQ](FAQ.en.md)
- [Examples](EXAMPLES.en.md)
- [Performance](PERFORMANCE.en.md)
- [Guides](guide/README.en.md)
- [Case Studies](case/README.en.md)


## Current Documentation Rebuild Rules

The rebuild currently follows these rules:

- Chinese is fully rebuilt first
- English is synchronized after the Chinese version is reviewed
- API documents are grouped under `docs/api/`
- obsolete old network/TLS documents are moved to `dev/` as archives
- existing documents that are still valid must still be aligned against current code
- missing documents for already-existing mainline features must be added before this phase is considered complete


## Historical Documentation Note

The following are no longer part of the current official documentation entry:

- the legacy network module family
- old `nettls`

They have been moved out of `docs/` and are kept under `dev/` as archived historical material.

---

## Suggested Reading Order

If this is your first time entering the current XRT mainline, read in this order:

1. [Project Overview](../../README.en.md)
2. [Architecture](ARCHITECTURE.en.md)
3. [API Index](api/README.en.md)
4. [Examples](EXAMPLES.en.md)
5. [Migration Guide](MIGRATION.en.md)

If you want to start writing code directly, read these first:

1. [Base](api/api-base.en.md)
2. [Value](api/api-value.en.md)
3. [Thread](api/api-thread.en.md)
4. [Coroutine](api/api-coroutine.en.md)
5. [Future / Task / Promise](api/api-future-task-promise.en.md)
6. [XNet v2](api/api-xnet-v2.en.md)

If you want the first full learning path after that, continue with:

1. [Runtime and Thread Attach](guide/runtime-thread-attach.en.md)
2. [xvalue and JSON Intro](guide/xvalue-json-intro.en.md)
3. [Coroutine, Future, and Task Intro](guide/coroutine-future-task-intro.en.md)
4. [xnet-v2 and TLS Mainline Intro](guide/xnet-v2-tls-intro.en.md)
5. [Streaming LLM API Intro](guide/streaming-llm-api-intro.en.md)

If you prefer complete combined examples, continue with:

1. [Minimal HTTP Service](case/minimal-http-service.en.md)
2. [Thread, Coroutine, and Future Coordination](case/thread-coroutine-future.en.md)
3. [Streaming LLM API](case/streaming-llm-api.en.md)
4. [HTTP + JSON + Template Full Chain](case/http-json-template-chain.en.md)
5. [xnet-v2 Stream Wait-Source Walkthrough](case/xnet-stream-wait-source.en.md)
