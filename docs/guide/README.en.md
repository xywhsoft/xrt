# XRT Guides

> Beginner-oriented, progressive, concept-to-code entry pages. Unlike `docs/api/`, the `guide/` directory focuses on the learning path instead of formal API contracts.

[Back to Docs Hub](../README.en.md)

---

## Contents

- [Purpose of the Guide Pages](#purpose-of-the-guide-pages)
- [Recommended Learning Path](#recommended-learning-path)
- [Currently Available Guide Pages](#currently-available-guide-pages)
- [Relationship to the API Docs](#relationship-to-the-api-docs)

---

## Purpose of the Guide Pages

The `guide/` directory is intended for:

- readers encountering XRT for the first time
- developers who want to build a working program quickly
- readers who want to understand why the mainline is designed this way

So the focus here is:

- concept explanation
- learning order
- progressive examples from small snippets to the full mainline
- common mistakes and recommended patterns

instead of enumerating API items one by one.

---

## Recommended Learning Path

If you are starting from scratch with XRT, read and practice in this order:

### 1. Understand What XRT Is

Goals:

- understand XRT positioning
- understand what “a C infrastructure library for the Internet + AI era” means
- understand the relationship between the source-tree mainline, single-header distribution, and the docs structure

Suggested entry points:

- [Project Overview](../../../README.en.md)
- [Documentation Hub](../README.en.md)
- [Architecture](../ARCHITECTURE.en.md)

### 2. Runtime and Basic Capabilities

Goals:

- learn `xrtInit()` / `xrtUnit()`
- understand `xrtGetError()`
- learn string, time, and file primitives

Suggested entry points:

- [Base](../api/api-base.en.md)
- [String](../api/api-string.en.md)
- [Time](../api/api-time.en.md)
- [File](../api/api-file.en.md)

### 3. Dynamic Data and JSON

Goals:

- learn `xvalue`
- learn array / list / coll / table
- learn JSON <-> `xvalue`

Suggested entry points:

- [Value](../api/api-value.en.md)
- [JSON](../api/api-json.en.md)

### 4. Concurrency and Async

Goals:

- understand the thread attach model
- understand coroutines and the scheduler
- understand future / task / promise / wait-source

Suggested entry points:

- [Thread](../api/api-thread.en.md)
- [Coroutine](../api/api-coroutine.en.md)
- [Future / Task / Promise](../api/api-future-task-promise.en.md)

### 5. Network Mainline

Goals:

- understand `xnet-v2`
- understand TLS sessions
- understand HTTP / HTTPD / WebSocket

Suggested entry points:

- [XNet V2](../api/api-xnet-v2.en.md)
- [Network TLS](../api/api-network-tls.en.md)
- [XHTTP](../api/api-xhttp.en.md)
- [XHTTPD](../api/api-xhttpd.en.md)
- [XWS](../api/api-xws.en.md)

---

## Currently Available Guide Pages

The current English guide entry points follow the reviewed Chinese mainline:

1. [First XRT Program](first-xrt-program.en.md)
2. [Runtime and Thread Attach](runtime-thread-attach.en.md)
3. [xvalue and JSON Intro](xvalue-json-intro.en.md)
4. [Coroutine, Future, and Task Intro](coroutine-future-task-intro.en.md)
5. [xnet-v2 and TLS Session Intro](xnet-v2-tls-intro.en.md)
6. [xnet-v2 Stream Wait-Source Intro](xnet-stream-wait-source-intro.en.md)
7. [Minimal HTTP Service Intro](minimal-http-service-intro.en.md)
8. [Streaming LLM API Intro](streaming-llm-api-intro.en.md)
9. [Subprocess and Tool Execution](subprocess-intro.en.md)

These pages are enough to support a first-pass learning path from runtime and data handling to concurrency, networking, and AI-facing usage.

---

## Relationship to the API Docs

You can think about `guide/` and `api/` this way:

- `api/`: what the API is, what the contract is, what the boundary is
- `guide/`: what to learn first, how to combine it, and why the mainline is written this way

If you already know which function or module you need, go to:

- [API Index](../api/README.en.md)

If you are still trying to understand which mainline to learn first, start here.

---

This directory now serves as the formal English guide entry for the current XRT mainline.
