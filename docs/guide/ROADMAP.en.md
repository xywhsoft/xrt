# XRT Guide Rebuild Roadmap

> Goal: rebuild `guide/` from a loose collection of topic notes into a staged course that goes from the first demo to complete projects, and eventually covers every public XRT module.

[Back to Guides](README.en.md) | [Back to Docs Hub](../README.en.md)

---

## 1. Writing Rules

Every formal guide page should answer these questions in a fixed order:

1. What problem does this module solve?
2. Where does a single main thread become a bottleneck?
3. How is this different from the neighboring options?
4. What does the smallest runnable demo look like?
5. When should the reader upgrade to a second, fuller demo?
6. What are the common mistakes and hard boundaries?
7. What should Windows and Linux users watch out for?
8. What should the reader learn next?

So a formal guide page should no longer be just an API summary. It should include at least:

- scenario motivation
- option comparison
- the smallest runnable example
- gradual expansion
- common traps
- exercises or extension tasks


## 2. Staged Route

### Stage 0: Meet XRT and Run the First Program

- Coverage: overall project structure, `xrt.h`, `xrt.c`
- Goal: understand the source tree, build modes, and the boundary of `xrtInit()` / `xrtUnit()`
- Planned pages:
1. the first XRT program
2. source tree, single-header distribution, and docs structure
3. minimal Windows / Linux build scripts

### Stage 1: Core Runtime and Tool Modules

- Coverage: `base`, `string`, `charset`, `time`, `path`, `file`, `os`, `math`, `hash`, `xid`, `network`
- Goal: give the reader enough foundation to build a normal cross-platform utility
- Planned pages:
1. runtime init, error slots, and temp memory
2. strings and character encoding
3. time, path, and file system
4. system launching, process opening, and local network information
5. common helpers: random, hash, and unique IDs

### Stage 2: Memory Models, Containers, and Data Structures

- Coverage: `bsmm`, `memunit`, `mempool_fs`, `mempool`, `buffer`, `array`, `ptrarray`, `stack`, `dynstack`, `llist`, `avltree`, `dict`, `list`
- Goal: explain when plain allocation is still enough, and when to move to pools, buffers, or containers
- Planned pages:
1. why XRT does more than wrap `malloc/free`
2. how to choose between fixed blocks, small-object pools, and general pools
3. sequential containers and linked structures
4. dictionaries, AVL trees, lists, and lookup structures

### Stage 3: Structured Data and Text Processing

- Coverage: `value`, `jnum`, `json`, `xson`, `template`, `regex`, `crypto`
- Goal: help readers move from raw strings to real data chains such as config, templating, protocol payloads, validation, and cryptography
- Planned pages:
1. the `xvalue` type system and ownership
2. `jnum` and number-text boundaries
3. `json` and `xson`
4. `template` expressions, control flow, and composition
5. `regex` for validation, extraction, and routing
6. `crypto` boundaries for signatures, digests, and secure transport building blocks

### Stage 4: Multitasking and Async Mainline

- Coverage: `thread`, `queue`, `coroutine`, `future/task/promise`, `wait-source`, `task group`
- Goal: explain why multitasking is needed, which models exist, and when to change models
- Planned pages:
1. why one main thread is not enough
2. threads: creation, attach, sync, fit, and cost
3. queues: producer/consumer, backpressure, close, and drain
4. coroutines: single-thread orchestration, wait, deadline, and cleanup
5. future / task / promise: unified async results, composition, and continuation
6. wait-source: unify future, stream, listener, and dgram into one waiting model
7. task group and structured concurrency: close, reap, and parent/child cancellation
8. thread / coroutine / future mixed-model selection

### Stage 5: System Capabilities and Tooling

- Coverage: `subprocess`, `file_async`
- Goal: let readers build tooling programs, batch jobs, and agent-like workflows
- Planned pages:
1. subprocess creation, stdout/stderr pipes, and future-based waiting
2. async file handles, whole-file helpers, and directory jobs
3. `subprocess + file_async + future` as one combined pipeline

### Stage 6: Network Mainline

- Coverage: `xurl`, `xhttp_util`, `xnet-v2`, `network-tls`, `xhttp`, `xhttpd`, `xws`, `xnet_proxy`
- Goal: teach the path from low-level waiting to application-layer protocols
- Planned pages:
1. URL, header, query, cookie, and multipart assembly
2. `xnet-v2` engine, stream, dgram, and listener
3. TLS session placement, boundaries, and lifecycle
4. HTTP client
5. HTTP server
6. WebSocket
7. shared proxy objects and proxy handshakes

### Stage 7: Full Project Ladder

- Goal: move from small features to real multi-module programs
- Planned cases:
1. config system: `file + path + value + json`
2. template rendering: `value + template + file`
3. multitask worker: `thread + queue + future`
4. tooling pipeline: `subprocess + file_async + future`
5. HTTP client call chain: `xurl + xhttp + TLS + proxy`
6. HTTP service: `xhttpd + json + template`
7. streaming LLM API: `xhttp/xws + future + coroutine`


## 3. Special Note on the Multitasking Track

This is the part readers care about most, and it is also the easiest part to write too shallowly.

This track must be taught in this order instead of jumping directly into function lists:

1. why one main thread is not enough
2. which work fits threads
3. which work fits coroutines
4. why `future / task / promise` is needed
5. why `wait-source` unifies waiting semantics
6. what role `queue` plays in workers, backpressure, and cross-thread handoff
7. what it means when threads execute, coroutines orchestrate, and futures carry results
8. which scenarios require mixed usage

Each page in this track should provide at least:

- one minimal example
- one upgraded example
- one bad example and why it is bad


## 4. How Current Pages Connect to the Roadmap

English mirror pages currently exist for only part of the rebuilt route. When an English mirror is missing, the first reviewed page is still Chinese-first and is linked directly below.

- [First XRT Program](first-xrt-program.en.md): Stage 0 entry
- [Time, Path, and File Intro](time-path-file-intro.md): Stage 1 `time / path / file` first formal page, Chinese first
- [Charset Intro](charset-intro.md): Stage 1 `charset` first formal page, Chinese first
- [OS Intro](os-intro.md): Stage 1 `os` first formal page, Chinese first
- [Local Network Info Intro](local-network-intro.md): Stage 1 `network(local info)` first formal page, Chinese first
- [XID Intro](xid-intro.md): Stage 1 `xid` first formal page, Chinese first
- [Math and Hash Intro](math-hash-intro.md): Stage 1 `math / hash` first formal page, Chinese first
- [Buffer Intro](buffer-intro.md): Stage 2 `buffer` first formal page, Chinese first
- [Array Intro](array-intro.md): Stage 2 `array` first formal page, Chinese first
- [PtrArray Intro](ptrarray-intro.md): Stage 2 `ptrarray` first formal page, Chinese first
- [Stack Intro](stack-intro.md): Stage 2 `stack` first formal page, Chinese first
- [DynStack Intro](dynstack-intro.md): Stage 2 `dynstack` first formal page, Chinese first
- [Dict Intro](dict-intro.md): Stage 2 `dict` first formal page, Chinese first
- [List Intro](list-intro.md): Stage 2 `list` first formal page, Chinese first
- [AVLTree Intro](avltree-intro.md): Stage 2 `avltree` first formal page, Chinese first
- [BSMM Intro](bsmm-intro.md): Stage 2 `bsmm` first formal page, Chinese first
- [MemUnit Intro](memunit-intro.md): Stage 2 `memunit` first formal page, Chinese first
- [FSMemPool Intro](mempool-fs-intro.md): Stage 2 `mempool_fs` first formal page, Chinese first
- [MemPool Intro](mempool-intro.md): Stage 2 `mempool` first formal page, Chinese first
- `llist`: no independent public header in the current source tree; keep only historical API notes for now
- [xvalue and JSON Intro](xvalue-json-intro.en.md): Stage 3 `value / json` first formal page
- [JNUM Intro](jnum-intro.md): Stage 3 `jnum` first formal page, Chinese first
- [XSON Intro](xson-intro.md): Stage 3 `xson` first formal page, Chinese first
- [Template Intro](template-intro.md): Stage 3 `template` first formal page, Chinese first
- [Regex Intro](regex-intro.md): Stage 3 `regex` first formal page, Chinese first
- [Crypto Intro](crypto-intro.md): Stage 3 `crypto` first formal page, Chinese first
- [Multitask Overview](multitask-overview.en.md): Stage 4 front page
- [Thread Intro](thread-intro.en.md): Stage 4 thread page
- [Queue Intro](queue-intro.en.md): Stage 4 queue page
- [Coroutine, Future, and Task Intro](coroutine-future-task-intro.en.md): part of Stage 4
- [Wait-Source Intro](wait-source-intro.en.md): Stage 4 wait-source page
- [Task Group Intro](task-group-intro.en.md): Stage 4 structured-concurrency page
- [Runtime and Thread Attach](runtime-thread-attach.en.md): thread-related entry page
- [Subprocess and Tool Execution](subprocess-intro.en.md): Stage 5 entry page
- [Async File and Directory Operations](file-async-intro.en.md): Stage 5 entry page
- [xnet-v2 and TLS Session Intro](xnet-v2-tls-intro.en.md): network entry page
- [xnet-v2 Stream Wait-Source Intro](xnet-stream-wait-source-intro.en.md): network wait-source entry page
- [XURL Intro](xurl-intro.en.md): Stage 6 `xurl` first formal page
- [HTTP Util Intro](http-util-intro.en.md): Stage 6 `xhttp_util` first formal page
- [Proxy Intro](proxy-intro.en.md): Stage 6 `proxy` first formal page
- [XWS Intro](xws-intro.en.md): Stage 6 `xws` first formal page


## 5. Completion Criteria

The guide mainline is only considered rebuilt when all 4 conditions are true:

1. every public XRT module has a guide entry
2. every stage has a smallest demo, an upgraded demo, and a complete case
3. the multitasking track explains thread, coroutine, future, queue, and wait-source as one coherent line
4. the English mirrors are synchronized after the Chinese mainline stabilizes
