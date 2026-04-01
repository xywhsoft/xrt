# XRT Documentation Hub

> The current official documentation entry for XRT. `docs/api/` is the source-aligned contract layer; `docs/guide/` and `docs/case/` are being rebuilt into progressive teaching material and end-to-end walkthroughs. Chinese remains the canonical mainline, and English entry pages are synchronized after the reviewed Chinese pages settle.

[中文](README.md) | [Project Overview](../README.en.md)

---

## Current Status

- `docs/api/`: the best place to check contracts, types, and module boundaries.
- `docs/guide/`: being rebuilt into step-by-step teaching pages from the smallest demo to fuller compositions.
- `docs/case/`: being rebuilt into full problem walkthroughs instead of topic notes.
- [Documentation Audit](DOCS_AUDIT.md): tracks stale pages, link issues, and module coverage gaps.
- [Guide Rebuild Roadmap](guide/ROADMAP.en.md): defines the staged teaching route and case ladder.


## Documentation Structure

The current `docs/` tree is organized into 4 layers:

### 1. Documentation Hub

- Current page [README.en.md](README.en.md)
- [Documentation Audit](DOCS_AUDIT.md)

Responsibilities:

- explain the documentation structure
- mark the current rebuild state
- provide a unified entry point
- track rebuild progress

### 2. API Reference

- [API Index](api/README.en.md)

Responsibilities:

- types and public structs
- core runtime
- threads, coroutines, queues, and async primitives
- memory, containers, text, and structured data
- the current network mainline APIs

### 3. Guides

- [Guide Entry](guide/README.en.md)
- [Guide Rebuild Roadmap](guide/ROADMAP.en.md)

Responsibilities:

- from-zero teaching
- concept explanation
- model comparison
- progressive learning from the smallest demo to combined examples

### 4. Case Studies

- [Case Entry](case/README.en.md)

Responsibilities:

- end-to-end problem decomposition
- cross-module cooperation
- combined examples that move from "it runs" toward "it can be shipped"


## Recommended Entry Points

### Project Positioning

- Root [README.en.md](../README.en.md)
- [Architecture](ARCHITECTURE.en.md)
- [Feature Trimming Macros](FEATURE_TRIMMING_MACROS.en.md)

### API Mainline

- [API Index](api/README.en.md)
- [Base Runtime](api/api-base.en.md)
- [Value Dynamic Type System](api/api-value.en.md)
- [JNUM Number Parsing and Conversion](api/api-jnum.en.md)
- [JSON Processing](api/api-json.en.md)
- [XSON Processing](api/api-xson.en.md)
- [Thread Runtime](api/api-thread.en.md)
- [Queue](api/api-queue.en.md)
- [Coroutine Runtime](api/api-coroutine.en.md)
- [Future / Task / Promise](api/api-future-task-promise.en.md)
- [Subprocess](api/api-subprocess.md) (Chinese for now)
- [Async File](api/api-file-async.md) (Chinese for now)
- [XURL](api/api-xurl.en.md)
- [HTTP Util](api/api-http-util.en.md)
- [XNet V2](api/api-xnet-v2.en.md)
- [XNet Proxy](api/api-xnet-proxy.md) (Chinese for now)
- [Network TLS](api/api-network-tls.en.md)
- [XHTTP](api/api-xhttp.en.md)
- [XHTTPD](api/api-xhttpd.en.md)
- [XWS](api/api-xws.en.md)

### Guides and Cases

- [Guide Entry](guide/README.en.md)
- [Guide Rebuild Roadmap](guide/ROADMAP.en.md)
- [Case Entry](case/README.en.md)
- [Examples](EXAMPLES.en.md)


## Current Rebuild Rules

- Chinese is kept complete and source-aligned first.
- English is synchronized after the reviewed Chinese pages settle.
- API docs must stay consistent with code.
- Guide pages must explain why, when, and how to build something step by step.
- Case pages must show how multiple modules solve one complete problem together.
- Historical pages and internal drafts must not remain disguised as current official entry points.


## Suggested Reading Order

If this is your first time approaching XRT, read in this order:

1. [Project Overview](../README.en.md)
2. [Architecture](ARCHITECTURE.en.md)
3. [API Index](api/README.en.md)
4. [Documentation Audit](DOCS_AUDIT.md)
5. [Guide Rebuild Roadmap](guide/ROADMAP.en.md)

If you want to start writing code directly, read these first:

1. [Base Runtime](api/api-base.en.md)
2. [Value Dynamic Type System](api/api-value.en.md)
3. [JSON Processing](api/api-json.en.md)
4. [Thread Runtime](api/api-thread.en.md)
5. [Queue](api/api-queue.en.md)
6. [Coroutine Runtime](api/api-coroutine.en.md)
7. [Future / Task / Promise](api/api-future-task-promise.en.md)
8. [Subprocess](api/api-subprocess.md) (Chinese for now)
9. [Async File](api/api-file-async.md) (Chinese for now)
10. [XURL](api/api-xurl.en.md)
11. [HTTP Util](api/api-http-util.en.md)
12. [XNet V2](api/api-xnet-v2.en.md)

If you want the current progressive teaching route, continue with:

1. [Guide Entry](guide/README.en.md)
2. [Guide Rebuild Roadmap](guide/ROADMAP.en.md)
3. [First XRT Program](guide/first-xrt-program.en.md)
4. [Multitasking Overview](guide/multitask-overview.en.md)
5. [Thread Intro](guide/thread-intro.en.md)
6. [Queue Intro](guide/queue-intro.en.md)
7. [Wait-Source Intro](guide/wait-source-intro.en.md)
8. [Task Group Intro](guide/task-group-intro.en.md)
9. [XURL Intro](guide/xurl-intro.en.md)
10. [HTTP Util Intro](guide/http-util-intro.en.md)
11. [Proxy Intro](guide/proxy-intro.en.md)
12. [XWS Intro](guide/xws-intro.en.md)

If you prefer full combined examples, continue with:

1. [Case Entry](case/README.en.md)
2. [Configuration System with `xvalue + json`](case/json-config-system.en.md)
3. [Template Rendering to HTML](case/template-render-html.en.md)
4. [Queue + Future Multi-Producer Worker](case/queue-worker-future.en.md)
5. [Subprocess + Async File Tooling Pipeline](case/subprocess-file-async-pipeline.en.md)
6. [XHTTP Client Chain with URL, Proxy, and TLS](case/xhttp-client-proxy-tls.en.md)
7. [XWS Session Skeleton with Queue and Coroutine](case/xws-session-queue-coroutine.en.md)
8. [Thread, Coroutine, and Future Coordination](case/thread-coroutine-future.en.md)
9. [Streaming LLM API with XRT](case/streaming-llm-api.en.md)
10. [Full HTTP + JSON + Template Service Chain](case/http-json-template-chain.en.md)
11. [Signed Rule Bundle Import with Charset, Regex, and Crypto](case/signed-rule-bundle.en.md)
12. [Session Registry with `mempool + avltree + list`](case/session-registry-pool-index.en.md)
13. [A Local Console Service that Combines Config, Logging, Tasks, Networking, and Templates](case/local-console-service.en.md)
14. [Upgrade the Local Console Service into a Subprocess Probe Dashboard](case/subprocess-probe-dashboard.en.md)
15. [Upgrade the Local Console Service into a Batch Task Dashboard](case/batch-task-dashboard.md) (Chinese for now)
