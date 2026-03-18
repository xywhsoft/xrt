<div align="center">

# XRT

[![License: MIT](https://img.shields.io/badge/license-MIT-yellow.svg)](LICENSE)
[![Language: C](https://img.shields.io/badge/language-C-brightgreen.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Platforms](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-blue.svg)](#)

**A Cross-Platform C Infrastructure Library for the Internet + AI Era, Built in Pursuit of Excellence**

English | [简体中文](README.md)

</div>

**Documentation:** [Docs Home](docs/README.en.md) | [API Index](docs/api/README.en.md) | [Guides](docs/guide/README.en.md) | [Cases](docs/case/README.en.md)


## Positioning

**XRT is not a loose collection of helper functions, and it is not a toolbox made by stitching together a few third-party libraries.**

XRT aims to be a real, coherent C infrastructure stack:

- A general-purpose runtime foundation for applications
- A foundation for high-concurrency network services
- A foundation for modern workloads such as LLMs, AI agents, streaming, and structured data exchange
- Designed from the beginning for **Windows / Linux / macOS** and other cross-platform environments

Its goal is to let C deliver a development experience closer to modern languages in scenarios such as:

- General application development
- Cross-platform software development
- Multi-threaded and coroutine-based concurrency
- High-performance network clients and servers
- Full application stacks built around HTTP / WebSocket / TLS / JSON / templates
- AI agents, LLM API integration, streaming result handling, and tool orchestration


## What XRT Already Provides

XRT has already formed a real mainline architecture rather than a set of unrelated modules.  
If you view it as a capability map, it currently expands into the following layers:

### 1. Base Runtime

This layer consolidates the most common, repetitive, and platform-sensitive tasks found in real C projects:

- `base`
	- Basic memory allocation
	- Error state
	- Thread-local temporary memory
	- Runtime init / shutdown
- `os`
	- Process launch
	- Open file / directory / URL
	- Platform-specific system wrappers
- `string`
	- Search, replace, split, concatenate, format
	- Case handling
	- Common text transforms
- `charset`
	- UTF-8 / UTF-16 / UTF-32 conversion
	- BOM detection
	- Charset conversion primitives
- `time`
	- Current time
	- Date serialization
	- Time calculation and formatting helpers
- `file / path`
	- File I/O
	- Directory scanning
	- Path join / split / normalization
- `hash / math / xid`
	- 32-bit / 64-bit hashing
	- Random number generation
	- Distributed unique IDs

The value of this layer is not just “many functions.” It is that higher layers do not have to repeatedly solve:

- Platform differences
- Encoding differences
- Path differences
- Low-level time and string details


### 2. Memory Management

XRT does not stop at wrapping `malloc/free`. It provides multiple memory strategies for different workloads:

- `bsmm`
	- Small-block allocation for fixed-size structs
- `memunit`
	- Page-based management for small objects
- `mempool_fs`
	- Fast fixed-size object pools
- `mempool`
	- General-purpose multi-level memory pools
- Runtime thread-local temporary memory
	- Solves large numbers of short-lived result strings and temporary buffers
- Memory tracking and debugging foundations
	- Provides infrastructure for leak and lifetime analysis

Beyond allocation performance, XRT treats **memory debugging** as part of the infrastructure itself.  
Its goal is not to guess after a crash, but to push memory problems back to specific code locations whenever possible.

This line currently focuses on:

- **Memory leak localization**
	- Tracks allocation origins
	- Can point debugging output back to source files and line numbers
	- Helps identify which allocation path was not released correctly
- **Dangerous operation detection**
	- Double free
	- Invalid free
	- Block corruption caused by out-of-bounds writes
	- Use-after-free
	- Uninitialized or invalid-lifetime objects entering pool structures
	- Wrong-thread access to thread-local objects
- **Object lifetime diagnosis**
	- Which object is still retained
	- Which object is already in a freed/releasing state but still accessed
	- Which container or pool objects are being misused under local/shared semantics
- **Thread-related memory diagnostics**
	- Thread-local temporary memory
	- Thread-local error slots
	- Thread-bound memory contexts
	- Owner-thread / shared-mode misuse diagnostics

So XRT’s memory system is not only about “faster allocation.” It is also about:

- Faster allocation performance
- Clearer lifetime rules
- Stronger problem localization
- Better exposure of dangerous operations

In practice, the memory layer already forms multiple distinct lines instead of “just one allocator”:

- General allocation
- Fixed-block allocation
- Small-object optimization
- Thread-local short-lived memory
- Memory debugging and dangerous-operation detection


### 3. Data Structures

XRT ships with a substantial set of general-purpose data structures instead of pushing developers back to raw arrays and linked lists:

- `buffer`
	- Dynamic buffers
- `array`
	- Contiguous struct arrays
- `ptrarray`
	- Pointer arrays
- `stack / dynstack`
	- Fixed and dynamic stacks
- `llist`
	- Doubly linked lists
- `avltree`
	- Balanced binary trees
- `dict`
	- String-key dictionaries
- `list`
	- Integer-indexed lists

These structures are not isolated. They are increasingly aligned with:

- Memory model decisions
- Shared/local semantics
- `xvalue`
- Ownership rules introduced by the concurrency refactors


### 4. Concurrency Infrastructure

XRT now provides much more than “some thread APIs.” It already has a fairly complete concurrency foundation:

- `thread`
	- Thread creation
	- Attach / detach
	- Thread-local runtime state
	- Thread-local error state
	- Thread-local temporary memory
- `coroutine`
	- Coroutine create / resume / yield / exit
	- Coroutine scheduler
	- Sleep / deadline / event wait
	- Cleanup stacks
	- Join / cancel / result

This means XRT can already support:

- Multi-threaded services
- Single-thread, many-coroutine execution
- Deadline-driven async logic
- Hybrid orchestration across threads and coroutines


### 5. Modern Async Runtime

This is one of the most important mainlines created by the recent refactors, and one of the clearest differences between XRT and many traditional C libraries.

It now formally includes:

- `future`
	- Async result objects
	- State, result, error, waiting
- `promise`
	- Async producer side
- `task`
	- Unified task execution abstraction
- `wait-source`
	- Unified wait-source abstraction
- Continuations
	- `Then / Catch / Finally`
	- `Current / Engine / Coroutine Scheduler`
- Combinators
	- `WhenAny / WhenAll / Race`
- Structured concurrency
	- `task group`
	- `group join`
	- `close / reap / destroy`
	- `nested scope`
	- `parent cancel propagation`

The meaning of this line is simple:

XRT is no longer just “threads + coroutines + networking.” It is starting to own a unified asynchronous runtime model.


### 6. Network Infrastructure

XRT’s networking layer is not a thin socket wrapper. It has already formed a modern network runtime mainline:

- `xnet-v2`
	- engine
	- stream
	- dgram
	- listener
	- sync
	- TLS session
- `future / wait-source` integrated with the network layer
- `stream / dgram / listener` with:
	- synchronous waiting
	- coroutine waiting
	- timeout / deadline waiting
	- future bridges

In other words, the network layer is not only about moving bytes. It is also unifying:

- I/O waiting
- Task posting
- Cancellation
- Deadlines
- Future-based results


### 7. Network Application Layer

Above transport, XRT already provides the critical pieces needed to build real application protocols:

- `http`
	- HTTP client mainline
- `httpd`
	- HTTP server mainline
- `websocket`
	- WebSocket client and server foundations
- `tls`
	- Built-in TLS session mainline
- `url / http util`
	- URL, header, query, and form helpers

That means XRT is not “just sockets, now go rewrite HTTP/TLS yourself.” It can already directly support:

- API clients
- HTTPS services
- WebSocket services
- LLM API integration
- Streaming network applications


### 8. Structured Data Processing

This is another critical mainline, especially because modern applications and AI workloads rely heavily on structured data.

- `xvalue`
	- Dynamic value system
	- Container-like values such as array / list / coll / table
	- A data handling experience closer to scripting languages
- `json`
	- JSON parsing and generation
	- SAX mode
	- Structured exchange data handling

This allows XRT to work naturally with:

- Configuration
- Protocol payloads
- API requests and responses
- AI structured outputs
- Dynamic template data


### 9. Text, Templates, and Regex

In real systems, text processing is not a side feature. It is often core infrastructure.

XRT already includes:

- `template`
	- Template rendering
	- Text generation
	- HTML / text output foundations
- `regex`
	- Pattern matching
	- Extraction
	- Replacement and pattern recognition
- In combination with `string / charset / json / xvalue`
	- Forms a full text-processing pipeline

This matters for:

- Web output
- Config generation
- Result formatting
- Agent-side text handling


### 10. Cross-Platform Engineering Capability

Beyond the number of features, XRT also provides one often underappreciated but very important capability:

It is trying to make all of the above work as **one cross-platform mainline**.

That means these modules are not just convenient on a single platform. They are being designed and consolidated around:

- Windows
- Linux
- macOS
- Practical toolchains such as `gcc / tcc`


Compressed into one sentence:

**XRT is no longer just “some helper functions.” It already provides a full infrastructure stack from low-level runtime, memory, containers, concurrency, async runtime, and networking, up to HTTP / TLS / WebSocket / JSON / templates / regex.**

This is not a pile of unrelated capabilities. It is already forming a coherent loop of:

**runtime + memory + data structures + concurrency + async + networking + application protocols + data representation + text processing**


## Why “for the Internet + AI Era”

Because XRT is not solving the old C problem of “just get sockets working.”

It is aimed at the combinations that modern software repeatedly needs:

- The same codebase must run on Windows and Linux with behavior kept as consistent as possible
- One thread runs many coroutines with deadlines and waiting
- `future / task / wait-source` unify threads, coroutines, and network waits
- HTTP / TLS / WebSocket connect your code to real-world APIs
- `xvalue / json` handle complex structured data
- Templates and regex handle generation and extraction
- One runtime supports AI agent tool calls, concurrent requests, streaming waits, and result aggregation

Taken one module at a time, none of these ideas is unusual.  
But making them work inside **one lightweight, unified, zero-external-runtime mainline** is where XRT becomes valuable.


## Core Characteristics

### 1. High Performance

XRT is not trading speed away in order to become feature-rich.

There is already measured evidence that the modern XRT network mainline is pushing toward that goal of excellence.

Current benchmark references:

- [XNET Compare Baseline (2026-03-14)](../dev/bench/XNET_COMPARE_20260314.md)
- [Builtin TLS Validation and Benchmark Report (2026-03-15)](../dev/bench/TLS_BENCH_20260315.md)

Representative results include:

- **Windows TCP echo total throughput**
	- `xnet-v1`: `91.7 msg/s`
	- `xnet-v2`: `83178.1 msg/s`
- **Windows TLS echo total throughput**
	- `xnet-v1`: `99.9 msg/s`
	- `xnet-v2`: `59343.7 msg/s`
- **Windows UDP burst send**
	- `xnet-v1`: `322825.4 pps`
	- `xnet-v2`: `1718877.6 pps`
- **Debian 13 TCP echo total throughput**
	- `xnet-v1`: `36628.1 msg/s`
	- `xnet-v2`: `60240.8 msg/s`
- **Debian 13 UDP burst send**
	- `xnet-v1`: `340122.5 pps`
	- `xnet-v2`: `1355287.2 pps`

From the TLS-specific benchmarks:

- **Warm full handshake average latency**
	- `276510.100 us`
- **TLS 1.2 resumed handshake average latency**
	- `34466.544 us`
- **Resume-path connect-to-open latency**
	- About **8.0x lower**

These numbers do not mean XRT has already reached its final optimum on every platform and in every scenario. But they do prove:

- XRT has explicit performance goals
- XRT already maintains repeatable benchmark baselines
- The modern network mainline is not a conceptual implementation; it is measured, compared, and optimized


### 2. Lightweight

XRT does not aim to become a huge framework. It aims to be:

- Single-header distributable
- Zero external runtime dependencies
- Trimmable
- Embeddable
- Statically linkable
- Usable as a DLL / component / runtime foundation

The source tree already provides:

- `xrt.h` as the unified public API
- `xrt.c` as the unified implementation
- `singlehead/xrt.h` as the single-header distribution form

It is not about stuffing external libraries into one place. It is about integrating the capabilities that deserve to live under one long-term design.


### 3. Cross-Platform

XRT is not a convenience library for one platform. It is intended to become real portable infrastructure.

Its concern is not merely “does it compile,” but:

- Keeping Windows / Linux / macOS usage as consistent as possible
- Pulling thread, time, file, path, networking, and charset differences into the runtime itself
- Keeping higher-level business logic as free as possible from platform branches
- Preserving practical workflows with `gcc / tcc` for fast validation and formal builds

This matters because XRT is not targeting toy single-machine programs. It targets:

- Deployable network services
- Tools delivered on multiple platforms
- Long-lived infrastructure codebases
- AI / agent systems running in different environments

One of XRT’s core values is to pull cross-platform complexity out of product code and into the infrastructure layer.


### 4. Powerful, Complete, and Coherent

Many C libraries look powerful because they have many parts, but in reality they are just:

- a container library
- a JSON library
- a networking library
- a random generator
- a template library

Putting them side by side does not automatically create infrastructure.

XRT cares more about:

- Consistent models across modules
- Consistent lifetime rules
- Threads / coroutines / futures / wait-sources working together
- Networking and application protocols connecting directly into the runtime
- Data structures, memory management, and structured data handling being part of the same system

That is why XRT is pursuing:

**power without patchwork, completeness without pileup, and system-level coherence instead of accidental composition**


## What Makes XRT Different from Typical C Libraries

### It Solves the Whole Chain, Not Just One Point

Compared with single-purpose libraries, XRT looks more like a real project foundation:

| Area | Typical Single-Purpose Library | XRT’s Approach |
|---|---|---|
| Base runtime | A few helper functions | A real runtime mainline |
| Cross-platform | Business code full of `#ifdef` | Platform differences pushed into runtime and system abstractions |
| Data structures | A standalone container library | Containers aligned with memory model and shared/local semantics |
| Concurrency | Only threads or only coroutines | Threads, coroutines, futures, tasks, wait-sources together |
| Networking | Just socket wrappers | Transport, sync waiting, async waiting, TLS, HTTP, WS |
| JSON | Just parse / print | Integrated with `xvalue`, templates, and networking |
| AI workloads | Manual composition of waits and results | A unified async runtime foundation already exists |


### Not Just Easy to Start, But Harder to Outgrow

Many libraries feel convenient in a small example, but once the project becomes real, they break down into:

- Lifetime chaos
- Fragmented error models
- Totally split sync and async APIs
- No common abstraction between threads and networking

Recent XRT refactors were explicitly aimed at solving these problems early:

- Runtime global-state / thread-state split
- Thread-bound coroutine runtime
- Formalized `future / task / promise / wait-source`
- `task group / nested scope / parent cancel propagation`
- Unified waiting models between `xnet` and coroutines / futures

That is one of the real differences between XRT and “a library with many features.”


## Signals of Scale and Maturity

Based on the current source tree, XRT already includes:

- `50+` implementation header modules
- `5694` lines of public declarations in `xrt.h`
- `93` documentation files
- `59` test files

It also already has real engineering assets:

- A default main test flow
- Independent specialized tests
- Race / lifetime / ownership regressions
- Benchmark baselines and dedicated benchmark reports
- A single-header generation pipeline

This does not mean every part is fully finalized. But it does mean:

**XRT has already moved beyond the demo / prototype phase and is approaching real infrastructure engineering.**


## Typical Use Cases

- Base runtime for general C applications
- Windows / Linux dual-platform project foundations
- Multi-platform command-line tools, services, and embeddable runtime components
- High-concurrency network services
- TLS / HTTP / WebSocket clients and servers
- JSON / dynamic-value / template-driven applications
- AI agent tool invocation, concurrent requests, deadline control, and result aggregation
- LLM API integration, structured outputs, and streaming interaction foundations


## Design Principles

XRT is already explicitly following these principles:

- **High performance**
	- Real benchmarks, not verbal claims
- **Lightweight**
	- Avoid introducing external dependencies unless truly necessary
- **Single mainline**
	- Continuously remove legacy surfaces, names, and wrapper shells
- **Unified model**
	- Threads, coroutines, futures, and network waits should align where possible
- **Cross-platform first**
	- Absorb platform differences into the foundation instead of dumping them onto business code
- **Trimmable**
	- Remain flexible as an infrastructure library
- **No historical-burden compatibility by default**
	- While the project is still in a refactor-friendly stage, choose the right architecture first


## Current State

XRT already has a large amount of formal capability, but its goal is not “usable is enough.” Its goal is “freeze only when the design is strong enough.”

So the more accurate description today is:

- It already has a full infrastructure blueprint
- Multiple mainline modules have entered their post-refactor architecture
- Many core capabilities are close to production-grade, and some can already be used with production-grade thinking
- But the project is still actively cleaning legacy surfaces, unifying mainlines, and closing platform-validation and documentation gaps

That is why the README emphasizes “pursuit of excellence.”  
XRT is not trying to become a library that is merely “usable enough.” It is trying to make C infrastructure more modern, more systematic, and more reliable.


## Quick Start

The simplest usage pattern:

```c
#include "xrt.h"

int main()
{
	xrtInit();

	printf("xrt version: %s\n", XRT_VERSION);
	printf("rand32: %u\n", xrtRand32());

	xrtUnit();
	return 0;
}
```

Common build on Windows:

```bat
gcc -m64 main.c xrt.c -o app.exe -lws2_32 -liphlpapi
```

Or use TCC for fast validation:

```bat
tcc -m64 main.c xrt.c -o app.exe -lws2_32 -liphlpapi
```


## Documentation

- Main docs index: [docs/README.en.md](docs/README.en.md)
- Next-stage roadmap: [dev/XRT_ROADMAP_NEXT.md](dev/XRT_ROADMAP_NEXT.md)
- Network benchmarks: [dev/bench/XNET_COMPARE_20260314.md](dev/bench/XNET_COMPARE_20260314.md)
- TLS validation: [dev/bench/TLS_BENCH_20260315.md](dev/bench/TLS_BENCH_20260315.md)


## Acknowledgements

XRT insists on keeping one coherent mainline and owning the parts that should be owned internally, but that does not mean building in isolation.  
The project has borrowed from, integrated, learned from, or been inspired by the following excellent open-source projects:

| Project | Role in XRT | Compliance Note |
|---|---|---|
| [LJSON](https://gitee.com/lengjingzju/json) | XRT adopted its JNUM and SAX-side ideas in the `json` line | Keep copyright and license notices |
| [bbre](https://github.com/max-nurzia/bbre) | Used as the implementation foundation of the `regex` module | Keep copyright and disclaimer notices |
| [nmhash32x](https://github.com/gzm55/hash-garage) | Used as the 32-bit hash implementation | Keep copyright and disclaimer notices |
| [rapidhash](https://github.com/Nicoshev/rapidhash) | Used as the 64-bit hash implementation | Keep copyright and disclaimer notices |
| [PCG](https://github.com/imneme/pcg-c-basic) | Used as the random number generator foundation | Keep copyright and disclaimer notices |
| [mongoose](https://github.com/cesanta/mongoose) | Inspired parts of XRT’s HTTP and TLS engineering direction | No mongoose code is used in XRT |


## License

XRT itself is released under the [MIT License](LICENSE).

However, third-party implementations absorbed into XRT still remain subject to their own license requirements.  
If you redistribute XRT, you should preserve not only XRT’s own license, but also the copyright, disclaimer, and notice requirements of those third-party components where applicable.


## Final Words

If you are looking for more than a tiny library that only solves one isolated problem,  
and instead want a **C infrastructure mainline** that can support:

- a base runtime
- high concurrency
- modern networking
- structured data handling
- AI / agent workloads

then XRT is actively moving in that direction.

It is not trying to look powerful merely by listing many modules.  
It is trying to become:

**high-performance, lightweight, powerful, complete, coherent, and built in pursuit of excellence**
