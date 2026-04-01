# XRT Guides

> Beginner-oriented, progressive, concept-to-code entry pages. `docs/api/` defines contracts and boundaries; `docs/guide/` defines learning order, model comparison, and the mental steps needed to write working code.

[Back to Docs Hub](../README.en.md)

---

## Contents

- [Purpose of the Guide Pages](#purpose-of-the-guide-pages)
- [Current Status](#current-status)
- [Formal Learning Route](#formal-learning-route)
- [How to Use the Existing Pages](#how-to-use-the-existing-pages)
- [Current Gap](#current-gap)
- [Relationship to the API Docs](#relationship-to-the-api-docs)

---

## Purpose of the Guide Pages

The `guide/` directory is intended for:

- readers meeting XRT for the first time
- developers who want the shortest path to a working program
- readers who want to understand why the mainline is designed this way and when to switch models

So these pages should not stop at:

- listing APIs
- showing one code fragment
- saying "read the source for more"

They should answer, in order:

1. what problem a module solves
2. where a single main thread becomes insufficient
3. how it differs from neighboring options
4. how to write the smallest runnable demo
5. when to upgrade to a fuller version
6. what the common mistakes and boundaries are


## Current Status

The `guide/` tree is still under rebuild:

- many reviewed first-formal-guide pages already exist in Chinese
- English currently mirrors the entry pages and part of the reviewed route
- the rebuild is now organized by an explicit roadmap instead of loose topic notes
- the multitasking route has been rebuilt in Chinese as a coherent line
- the network route is also being rebuilt page by page instead of staying API-only
- coverage is much better than before, but it is still not the final course system

Use these two pages together:

- [Guide Rebuild Roadmap](ROADMAP.en.md)
- the current topic pages listed below


## Formal Learning Route

The formal route now follows these stages:

1. [Guide Rebuild Roadmap](ROADMAP.en.md)
2. [First XRT Program](first-xrt-program.en.md)
3. Stage 1: core runtime and tool modules
4. Stage 2: memory, containers, and data structures
5. Stage 3: structured data and text processing
6. Stage 4: multitasking and async mainline
7. Stage 5: system capabilities and tooling
8. Stage 6: network mainline
9. Stage 7: end-to-end cases

If you want the full stage breakdown, open:

- [ROADMAP.en.md](ROADMAP.en.md)


## How to Use the Existing Pages

### 1. English-Synced Entry Pages

These pages are already available in English and can support a first-pass learning route:

- [First XRT Program](first-xrt-program.en.md)
- [Multitasking Overview](multitask-overview.en.md)
- [Thread Intro](thread-intro.en.md)
- [Queue Intro](queue-intro.en.md)
- [Wait-Source Intro](wait-source-intro.en.md)
- [Task Group Intro](task-group-intro.en.md)
- [Runtime and Thread Attach](runtime-thread-attach.en.md)
- [xvalue and JSON Intro](xvalue-json-intro.en.md)
- [Coroutine, Future, and Task Intro](coroutine-future-task-intro.en.md)
- [xnet-v2 and TLS Session Intro](xnet-v2-tls-intro.en.md)
- [xnet-v2 Stream Wait-Source Intro](xnet-stream-wait-source-intro.en.md)
- [XURL Intro](xurl-intro.en.md)
- [HTTP Util Intro](http-util-intro.en.md)
- [Proxy Intro](proxy-intro.en.md)
- [XWS Intro](xws-intro.en.md)
- [Minimal HTTP Service Intro](minimal-http-service-intro.en.md)
- [Streaming LLM API Intro](streaming-llm-api-intro.en.md)
- [Subprocess and Tool Execution](subprocess-intro.en.md)
- [Async File and Directory Operations](file-async-intro.en.md)
- [Async HTTPD Handling](httpd-async-intro.en.md)

### 2. Chinese-First Formal Guide Pages Already Rebuilt

These are reviewed route pages that already exist, but their English mirrors are still pending:

- Stage 1 foundation: [Time, Path, and File Intro](time-path-file-intro.md), [Charset Intro](charset-intro.md), [OS Intro](os-intro.md), [Local Network Info Intro](local-network-intro.md), [XID Intro](xid-intro.md), [Math and Hash Intro](math-hash-intro.md)
- Stage 2 containers and memory: [Buffer Intro](buffer-intro.md), [Array Intro](array-intro.md), [PtrArray Intro](ptrarray-intro.md), [Stack Intro](stack-intro.md), [DynStack Intro](dynstack-intro.md), [Dict Intro](dict-intro.md), [List Intro](list-intro.md), [AVLTree Intro](avltree-intro.md), [BSMM Intro](bsmm-intro.md), [MemUnit Intro](memunit-intro.md), [FSMemPool Intro](mempool-fs-intro.md), [MemPool Intro](mempool-intro.md)
- Stage 3 structured data and text: [JNUM Intro](jnum-intro.md), [XSON Intro](xson-intro.md), [Template Intro](template-intro.md), [Regex Intro](regex-intro.md), [Crypto Intro](crypto-intro.md)
- Stage 4 multitasking: the currently reviewed formal pages are already mirrored in English
- Stage 6 network: the currently reviewed formal pages are already mirrored in English

These pages are useful when you want:

- the current shortest path to a rebuilt topic
- the most up-to-date explanation before English sync catches up
- the formal route beyond the older English quick-start pages


## Current Gap

The most visible remaining gap is:

- `llist`: the current source tree has no independent public header for it, so a formal guide page is still deferred and the old API pages remain historical notes only


## Relationship to the API Docs

Think about `guide/` and `api/` this way:

- `api/`: what the API is, what the contract is, what the hard boundary is
- `guide/`: what to learn first, why to write it this way, and how to combine modules into a runnable program

If you already know which function you need, go to:

- [API Index](../api/README.en.md)

If you are still deciding which mainline to learn first, start here and then open:

- [ROADMAP.en.md](ROADMAP.en.md)
