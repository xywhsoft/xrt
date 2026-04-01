# XRT Case Studies

> Entry pages for complete chains, combined capability usage, and real engineering slices. `docs/guide/` focuses on teaching order; `docs/case/` focuses on how multiple modules solve one complete problem together.

[Back to Docs Hub](../README.en.md)

---

## Contents

- [Purpose of the Case Studies](#purpose-of-the-case-studies)
- [Current Status](#current-status)
- [Target Case Ladder](#target-case-ladder)
- [How to Use the Existing Pages](#how-to-use-the-existing-pages)
- [Current Gaps](#current-gaps)
- [Relationship to API and Guide Docs](#relationship-to-api-and-guide-docs)

---

## Purpose of the Case Studies

The `case/` directory is intended for:

- showing how one complete problem is solved with XRT
- showing how multiple subsystems cooperate
- showing the full chain from the runtime up through networking, concurrency, templating, JSON, and AI-facing usage

Formal case pages should cover at least:

- scenario background
- goals and constraints
- module selection and why other options are not chosen
- key code structure
- run steps
- common failure points
- directions for extension


## Current Status

The `case/` tree already contains a useful set of pages, but it is still under rebuild:

- older pages were closer to topic notes than full teaching cases
- reviewed formal case pages now exist for several core chains
- the case ladder is better than before, but not yet complete from simple to complex
- the current reviewed formal case ladder now has English coverage
- newly rebuilt pages may still land in Chinese first before later English sync

If you want the staged teaching order behind these cases, read together with:

- [Guide Rebuild Roadmap](../guide/ROADMAP.en.md)


## Target Case Ladder

The formal case ladder now targets this order:

1. config system: `file + path + value + json`
2. template rendering page: `value + template + file`
3. multitask worker: `thread + queue + future`
4. subprocess and async-file pipeline: `subprocess + file_async + future`
5. HTTP client call chain: `xurl + http util + xhttp + TLS + proxy`
6. HTTP service: `xhttpd + json + template + value`
7. WebSocket session service: `xws + coroutine + queue`
8. streaming LLM API: `xhttp/xws + future + coroutine + json`
9. a small full project that combines config, logging, tasks, networking, and templates

The point of this ladder is not only to show that something runs. It is to show:

- why the modules are split this way
- which layer should use threads and which layer should use coroutines
- which layer should converge into future / wait-source
- how basic modules grow into complete features


## How to Use the Existing Pages

### 1. English-Synced Case Pages

The following case pages already exist in English:

1. [Configuration System with `xvalue + json`](json-config-system.en.md)
2. [Rendering HTML with Template](template-render-html.en.md)
3. [Thread, Coroutine, and Future Coordination](thread-coroutine-future.en.md)
4. [Queue + Future Multi-Producer Worker](queue-worker-future.en.md)
5. [Subprocess + Async File Tooling Pipeline](subprocess-file-async-pipeline.en.md)
6. [XHTTP Client Chain with URL, Proxy, and TLS](xhttp-client-proxy-tls.en.md)
7. [XWS Session Skeleton with Queue and Coroutine](xws-session-queue-coroutine.en.md)
8. [xnet-v2 Stream Wait-Source Walkthrough](xnet-stream-wait-source.en.md)
9. [Minimal HTTP Service with XRT](minimal-http-service.en.md)
10. [Streaming LLM API with XRT](streaming-llm-api.en.md)
11. [Full HTTP + JSON + Template Service Chain](http-json-template-chain.en.md)
12. [Signed Rule Bundle Import with Charset, Regex, and Crypto](signed-rule-bundle.en.md)
13. [Session Registry with `mempool + avltree + list`](session-registry-pool-index.en.md)

### 2. Current English Coverage Notes

At this point, the main reviewed formal case ladder has English coverage for its core concurrency, tooling, HTTP-client, WebSocket-session, signed-rule-import, and session-registry business pages.

So the remaining gaps are now mostly:

- the future small full-project case
- more business-style variants such as cache, batch, and multi-tenant index cases

These pages are currently useful when you want:

- the latest reviewed combined examples
- an end-to-end chain that matches the rebuilt Chinese mainline
- a practical route from basic modules to full features


## Current Gaps

The most obvious remaining case gaps are:

- a small full project that combines config, logging, tasks, networking, and templates
- more business-style variants around cache, batch, and multi-tenant indexing


## Relationship to API and Guide Docs

You can think about the three layers this way:

- `api/`: module contracts and boundaries
- `guide/`: from-zero learning route
- `case/`: how multiple modules cooperate to solve one complete problem

If you already roughly know each module API but still do not know how to compose them into a full solution, this is the right place to read next.
