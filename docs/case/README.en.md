# XRT Case Studies

> Entry pages for complete chains, combined capability usage, and real engineering slices. Unlike `docs/guide/`, the `case/` directory focuses on end-to-end problem decomposition instead of the teaching sequence.

[Back to Docs Hub](../README.en.md)

---

## Contents

- [Purpose of the Case Studies](#purpose-of-the-case-studies)
- [Recommended Case Directions](#recommended-case-directions)
- [Currently Available Case Pages](#currently-available-case-pages)
- [Relationship to API and Guide Docs](#relationship-to-api-and-guide-docs)

---

## Purpose of the Case Studies

The `case/` directory is intended for:

- showing how a complete problem is solved with XRT
- showing how multiple subsystems cooperate
- showing end-to-end usage from the runtime up through networking, concurrency, templates, JSON, and AI-facing scenarios

So these documents are better written as:

- scenario background
- design reasoning
- module selection
- key code slices
- common mistakes
- why this combination is recommended

---

## Recommended Case Directions

The most valuable case directions for the current mainline are:

### 1. JSON / Value Combination Cases

Examples:

- configuration loading
- dynamic object assembly
- request/response data modeling

### 2. Template Rendering Cases

Examples:

- rendering HTML with `xvalue + template`
- report output
- code generation

### 3. Thread / Coroutine / Future Coordination Cases

Examples:

- threads for parallelism
- coroutines for single-thread orchestration
- future / task / promise for unified async results

### 4. `xnet-v2` Cases

Examples:

- stream readable / writable / drain / close
- dgram recv
- listener accept
- unified wait-source waiting

### 5. Network Application Layer Cases

Examples:

- HTTP client calls to third-party APIs
- HTTP server services
- bidirectional WebSocket communication
- TLS session as a unified transport security layer

### 6. Internet + AI Cases

Examples:

- calling LLM APIs
- streaming response handling
- multi-step async orchestration with future / coroutine
- constructing requests and presenting results with JSON / template

---

## Currently Available Case Pages

The current reviewed mainline already includes these formal case pages:

1. [Configuration System with xvalue + json](json-config-system.en.md)
2. [Rendering HTML with Template](template-render-html.en.md)
3. [Thread, Coroutine, and Future Coordination](thread-coroutine-future.en.md)
4. [xnet-v2 Stream Wait-Source Walkthrough](xnet-stream-wait-source.en.md)
5. [Minimal HTTP Service with XRT](minimal-http-service.en.md)
6. [Streaming LLM API with XRT](streaming-llm-api.en.md)
7. [Full HTTP + JSON + Template Service Chain](http-json-template-chain.en.md)

These already cover:

- `xvalue + json`
- `template`
- thread / coroutine / future
- `xnet-v2` wait-source
- HTTP service
- streaming LLM API
- the complete `HTTP + JSON + template` chain

---

## Relationship to API and Guide Docs

You can think about them this way:

- `api/`: module contracts and capability boundaries
- `guide/`: from-zero learning path
- `case/`: full multi-module problem decomposition

If you already know each module API but still do not know how to compose them into a full solution, this is the right place to read next.

---

This directory now serves as the formal English case-study entry for the current XRT mainline.
