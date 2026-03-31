# XRT Performance Notes

> Performance notes for the current mainline. This page does not try to preserve scattered legacy-era benchmark descriptions. It explains what the current mainline is optimizing for, what baselines already exist, and how those results should be interpreted.

[中文](PERFORMANCE.md) | [Back to Docs Hub](README.en.md)

---

## Contents

- [Performance Design Goals](#performance-design-goals)
- [Current Mainline Performance Strategy](#current-mainline-performance-strategy)
- [Performance Baselines Already Established](#performance-baselines-already-established)
- [How to Understand Performance by Subsystem](#how-to-understand-performance-by-subsystem)
- [How to Benchmark Correctly](#how-to-benchmark-correctly)

---

## Performance Design Goals

XRT is not trying to win only on isolated micro-benchmarks. It is aiming for:

- high performance
- light weight
- cross-platform behavior
- coherent architecture
- stable throughput, latency, and resource usage in real infrastructure scenarios

So the current mainline performance goals include:

1. keep the low-level runtime and common utility paths light
2. keep memory and container operations fast on the current-thread local path
3. keep coroutines, future, and wait-source from splitting into multiple redundant execution models
4. make `xnet-v2` clearly outperform the old network mainline in real scenarios
5. let the HTTP / HTTPD / WebSocket layers build on the unified network substrate instead of each layer reinventing state machines

---

## Current Mainline Performance Strategy

### 1. Thread-Local Fast Paths First

Runtime state, default random generators, temporary memory, and many lightweight facilities have already moved into thread state.

The core reason is:

- keep the most common paths away from global locks whenever possible
- prepare for thread-bound memory contexts and the next shared-mode work

### 2. Explicit Separation Between Local and Shared

After phase-2, the memory/container model no longer assumes that every object is naturally thread-safe.

Instead it explicitly separates:

- local roots: current-thread fast paths
- shared roots: explicitly shared paths

This prevents local performance from being dragged down by “lock everything just in case”.

### 3. Unified Async Mainline

The current mainline already includes:

- future
- task
- promise
- wait-source
- coroutine wait
- thread wait

The value of this is not just API neatness. It also means:

- modules do not each carry their own async result model
- fewer wrappers and fewer useless bridges

### 4. Unified Low-Level Network Mainline

`xnet-v2` stream / dgram / listener / future / wait-source / TLS session have already started collapsing into one mainline.

That lets:

- HTTP
- HTTPD
- WebSocket
- coroutine waits
- future waits

all reuse the same foundation instead of layering redundant logic on top.

---

## Performance Baselines Already Established

### 1. Old-vs-New Network Mainline Baselines

There are already formal benchmark assets under:

- [XNET_COMPARE_20260314.md](../dev/bench/XNET_COMPARE_20260314.md)
- [XRT_FOUNDATION_BENCHMARK_METHOD.md](../dev/bench/XRT_FOUNDATION_BENCHMARK_METHOD.md)
- [TLS_BENCH_20260315.md](../dev/bench/TLS_BENCH_20260315.md)

These baseline sets already cover:

- Windows / Linux
- TCP echo
- TLS echo
- UDP burst
- queue pressure

### 2. Representative Recorded Results

From previously established baselines:

- On Windows, `xnet-v2` vs the old mainline:
	- TCP total: `91.7 -> 83178.1 msg/s`
	- TLS total: `99.9 -> 59343.7 msg/s`
	- UDP send: `322825.4 -> 1718877.6 pps`
- On Debian, `xnet-v2` vs the old mainline:
	- TCP total: `36628.1 -> 60240.8 msg/s`
	- TLS: the old mainline was unstable at the time, while `xnet-v2` completed reliably at about `39332.4 msg/s`
	- UDP send: `340122.5 -> 1355287.2 pps`

This is enough to show that “the new network mainline is significantly better than the old one” is not just a slogan.

### 3. How These Numbers Should Be Interpreted

These numbers do show:

- the `xnet-v2` direction is correct
- the new mainline is not “architecturally cleaner but slower”

But they do not mean:

- every platform and every scenario is already fully closed as production-grade

Performance conclusions must still be read together with stability and race validation.

---

## How to Understand Performance by Subsystem

### 1. Memory and Containers

The current focus is not to maximize isolated container micro-benchmarks at all costs. The real goals are:

- keep local-root fast paths light
- make shared-root boundaries explicit
- reject wrong-thread access early
- leave room for future thread-bound memory contexts

### 2. Coroutines

The current coroutine mainline already has:

- thread-bound runtime
- scheduler core
- stack allocator
- future / wait-source integration

The performance significance here is mostly:

- fewer split waiting models
- less meaningless polling
- one mainline for both network waits and coroutine waits

### 3. Future / Task / Promise

The performance value of this mainline is:

- one unified execution and result model
- less cross-module wrapper duplication
- continuation / combinator / task group built on one core

The next performance focus should be:

- stress regression
- long-running behavior
- destroy / cancel / close race behavior

And more specifically:

- combinator stability under concurrent completions (`WhenAny / WhenAll / Race`)
- join / reap / cancel costs for `task group / nested scope` with many children
- the runtime cost of current-thread deferred continuation pumping in real wait chains

### 4. Network and Application Layers

The low-level network performance mainline is already fairly clear:

- engine / worker
- stream / dgram / listener
- future / wait-source
- TLS session

HTTP / WebSocket performance should be evaluated on top of this same substrate, not as fully separate islands.

---

## How to Benchmark Correctly

### 1. Define the Goal First

Be clear about what you are measuring:

- throughput
- latency
- memory usage
- jitter
- long-run stability

Do not collapse all of these into a vague “fast or not”.

### 2. Keep Conditions Fixed

Each benchmark should ideally record:

- platform
- compiler
- compile flags
- core count
- pinning or non-pinning
- duration
- data size

### 3. Record Throughput and Latency Together

Throughput alone is not enough. Also record:

- average latency
- p50 / p95 / p99 where appropriate

### 4. Benchmark Network in Layers

Split the problem into:

- TLS on / off
- localhost / real network
- client / server
- queue pressure / normal path

### 5. Always Pair Stress with Correctness

For an infrastructure library, “fast but destroy/cancel races can still break it” is not good enough.

So performance testing should be paired with:

- timeout / deadline
- cancel / close
- destroy / release
- multi-waiter / multi-continuation
- listener / stream / dgram race regression

For the current async mainline, these should also become long-term baseline items:

- concurrent completion stress for `WhenAny / Race`
- batch child, close / destroy boundaries for `task group`
- multi-parent cancel propagation
- nested-scope join completion timing

---

## Related Documents

- [Project Overview](../README.en.md)
- [Architecture](ARCHITECTURE.en.md)
- [Best Practices](BEST_PRACTICES.en.md)
- [XNet V2](api/api-xnet-v2.en.md)
- [Future / Task / Promise](api/api-future-task-promise.en.md)
- [Benchmark Methodology](../dev/bench/XRT_FOUNDATION_BENCHMARK_METHOD.md)
- [XNet Comparison Baseline](../dev/bench/XNET_COMPARE_20260314.md)
- [TLS Baseline](../dev/bench/TLS_BENCH_20260315.md)

---

**XRT Performance Notes Version: current mainline rebuild edition**

