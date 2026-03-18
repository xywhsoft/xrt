# XRT Architecture

> Architecture of the current source-tree mainline. This document only describes the design that has already entered the formal mainline. Historical network and TLS surfaces are no longer treated as the current architecture.

[中文](ARCHITECTURE.md) | [Back to Index](README.en.md)

---

## 1. Design Positioning

XRT is not intended to be a loose collection of helper functions. Its target is:

**A C infrastructure library for the Internet + AI era**

That goal requires XRT to stay:

- lightweight
- high-performance
- cross-platform
- single-header friendly
- complete and systematic instead of stitched together from unrelated fragments
- usable for Internet-facing applications
- usable for AI-agent style asynchronous orchestration and network interaction

Because of that, XRT is organized as a full infrastructure chain rather than isolated modules:

- runtime foundation
- concurrency runtime
- unified async model
- network mainline
- network application layer
- structured data and text processing


## 2. Current Layering

### 2.1 Runtime Foundation

Responsible for:

- core types
- process-level runtime state
- thread-level runtime state
- allocator entry points
- error handling
- temporary memory
- string, charset, path, time, file, hash, random utilities

Core files:

- [xrt.h](../../xrt.h)
- [xrt.c](../../xrt.c)
- [base.h](../../lib/base.h)
- [string.h](../../lib/string.h)
- [os.h](../../lib/os.h)
- [charset.h](../../lib/charset.h)
- [math.h](../../lib/math.h)
- [path.h](../../lib/path.h)
- [time.h](../../lib/time.h)
- [file.h](../../lib/file.h)
- [hash.h](../../lib/hash.h)

### 2.2 Concurrency Runtime

Responsible for:

- threads
- thread runtime attach model
- coroutines
- coroutine scheduler
- coroutine event waiting

Core files:

- [thread.h](../../lib/thread.h)
- [coroutine.h](../../lib/coroutine.h)

### 2.3 Unified Async Model

Responsible for:

- `future`
- `promise`
- `task`
- `wait-source`
- continuation
- combinator
- task group
- nested scope

This layer has already become a formal mainline.

Core files:

- [xrt.h](../../xrt.h)
- [xnet_sync.h](../../lib/xnet_sync.h)

### 2.4 Memory and Containers

Responsible for:

- block-structured memory management
- fixed-size pools
- general memory pools
- arrays, pointer arrays, stacks, linked lists, AVL trees, dictionaries and lists

This layer now includes:

- owner-thread by default
- explicit shared-root semantics
- debugging and dangerous-operation detection

Core files:

- [bsmm.h](../../lib/bsmm.h)
- [memunit.h](../../lib/memunit.h)
- [mempool_fs.h](../../lib/mempool_fs.h)
- [mempool.h](../../lib/mempool.h)
- [array_point.h](../../lib/array_point.h)
- [array.h](../../lib/array.h)
- [stack.h](../../lib/stack.h)
- [stack_dyn.h](../../lib/stack_dyn.h)
- [llist.h](../../lib/llist.h)
- [avltree_base.h](../../lib/avltree_base.h)
- [avltree.h](../../lib/avltree.h)
- [dict.h](../../lib/dict.h)
- [list.h](../../lib/list.h)

### 2.5 Structured Data and Text

Responsible for:

- `xvalue`
- JSON
- numeric/text conversion
- template engine
- regular expressions

Core files:

- [value.h](../../lib/value.h)
- [json.h](../../lib/json.h)
- [jnum.h](../../lib/jnum.h)
- [template.h](../../lib/template.h)
- [regex.h](../../lib/regex.h)

### 2.6 Current Network Mainline

The formal network mainline is now:

**xnet-v2 + xtlssession + xhttp/xhttpd/xws**

Responsible for:

- URL handling
- HTTP helpers
- network base layer
- port abstraction
- codecs
- engine
- stream
- dgram
- sync/future/wait-source
- TLS session
- HTTP client
- HTTP server
- WebSocket

Core files:

- [xurl.h](../../lib/xurl.h)
- [xhttp_util.h](../../lib/xhttp_util.h)
- [xnet_base.h](../../lib/xnet_base.h)
- [xnet_mem.h](../../lib/xnet_mem.h)
- [xnet_port.h](../../lib/xnet_port.h)
- [xnet_port_iocp.h](../../lib/xnet_port_iocp.h)
- [xnet_port_uring.h](../../lib/xnet_port_uring.h)
- [xcodec.h](../../lib/xcodec.h)
- [xcodec_http1.h](../../lib/xcodec_http1.h)
- [xcodec_ws.h](../../lib/xcodec_ws.h)
- [xnet_engine.h](../../lib/xnet_engine.h)
- [nettls.h](../../lib/nettls.h)
- [xnet_stream.h](../../lib/xnet_stream.h)
- [xnet_dgram.h](../../lib/xnet_dgram.h)
- [xnet_sync.h](../../lib/xnet_sync.h)
- [xhttp.h](../../lib/xhttp.h)
- [xhttpd.h](../../lib/xhttpd.h)
- [xws.h](../../lib/xws.h)


## 3. Runtime Model

### 3.1 Global and Thread State Separation

The runtime is now explicitly split into:

- `xrtGlobalData`
- `xrtThreadData`

Purpose:

- process-wide configuration stays unique
- thread-local error state, temporary memory, random state and later thread memory context are no longer mixed into one global structure

### 3.2 Process-Level Runtime

Mainly stores:

- `sNull`
- application path
- allocator function table
- global error callback
- high-resolution timing environment

### 3.3 Thread-Level Runtime

Mainly stores:

- `LastError`
- thread-local temporary memory
- default random state
- coroutine runtime
- current-thread deferred continuation queue
- later thread-bound memory context

### 3.4 Attach Model

The current mainline uses:

- `xrtInit()` to auto-attach the current thread
- `xrtThreadCreate()` to auto-attach created threads
- explicit host-thread attach for runtime-bound APIs:
	- `xrtThreadAttachCurrent()`
	- `xrtThreadDetachCurrent()`


## 4. Threads and Coroutines

### 4.1 Threads

The thread mainline now provides:

- explicit Create / Wait / Destroy lifecycle
- automatic runtime attach/detach wiring
- thread cleanup stack
- automatic release of thread-local runtime state

### 4.2 Coroutines

Coroutines are currently:

- thread-bound
- stackful
- cooperative
- scheduler-driven

They already support:

- sleep
- deadline wait
- event wait
- future/task/wait-source integration
- cancellation and join

They are no longer treated as an isolated subsystem. They are part of the unified async runtime.


## 5. Unified Async Mainline

This is one of the most important recent architecture changes in XRT.

The current async mainline is:

- `future`
- `promise`
- `task`
- `wait-source`

And the surrounding runtime pieces:

- continuation
- combinator
- task group
- nested scope

Execution entry points are already unified across:

- thread tasks
- coroutine tasks
- engine tasks
- delayed tasks

Waiting is also unified across:

- normal thread wait
- timeout wait
- deadline wait
- coroutine wait

This mainline is designed to support:

- network async operations
- coroutine orchestration
- service-side concurrency
- AI-agent style task coordination


## 6. Memory, Ownership and Shared Semantics

The memory and container layer is no longer documented as “generic containers only”.

The current design explicitly distinguishes:

- local objects
- shared roots
- owner-thread mutation rules
- stable-view requirements under shared access

That means XRT does not treat thread safety as “everything is magically safe”. Instead, it makes ownership and sharing contracts explicit.

This is important for:

- memory pools
- arrays
- dictionaries
- lists
- AVL trees
- `xvalue` container roots


## 7. Data Mainline

For structured data, the intended mainline is:

- JSON as exchange format
- `xvalue` as internal structured data model
- template engine as rendering layer on top of `xvalue`

This is why `xvalue`, `json`, `template` and network application modules are increasingly documented together instead of as unrelated features.


## 8. Network and Application Mainline

At the architecture level, the current recommendation is:

- `xnet-v2` for transport/runtime
- `xtlssession` for TLS
- `xhttp` for client-side HTTP
- `xhttpd` for server-side HTTP
- `xws` for WebSocket

The design intent is to avoid a fragmented surface like:

- one set of low-level legacy transport APIs
- another set of partial wrappers
- a third async bridge layer

Instead, the network stack is being tightened into one formal mainline with:

- unified wait-source
- unified future integration
- unified coroutine waiting


## 9. Documentation Boundary

This document describes the current mainline only.

It does **not** treat the following as the formal architecture:

- retired legacy network public surfaces
- deprecated TLS public surface based on `xtlsctx / xrtTls*`
- historical examples kept only for migration context

During this rebuild phase, documents under `docs/` should be interpreted with this rule:

- current mainline documents are authoritative
- archived legacy documents under `dev/` are historical reference only


## 10. Related Documents

- [Documentation Index](README.en.md)
- [API Index](api/README.en.md)
- [Migration Guide](MIGRATION.en.md)
- [Best Practices](BEST_PRACTICES.en.md)

---

<div align="center">

[⬆️ Back to Top](#xrt-architecture)

</div>
