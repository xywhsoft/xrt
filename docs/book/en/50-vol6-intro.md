---
num: 50
slug: vol6-intro
title: Volume 6 · Processes and Concurrency: Introduction
volume: 卷六 进程与并发
type: intro
lead: Three concurrency granularities from processes to coroutines, cancellation-threaded unified waiting, and the "primitives-cooperation-structured" three-layer assembly line.
api: thread, coroutine, channel, future
---

## Orientation

Volume 6 answers "how do multiple flows of execution cooperate". It has one clear three-layer assembly line: the **primitives layer** (Chapter 52's threads and the synchronous four-piece set — the physical rules of the concurrent world), the **cooperation layer** (Chapters 53~58: cancellation tokens, coroutines ×2, Channels, Futures, executors — wrapping the physical rules into engineering tools), and the **structured layer** (Chapter 59's task groups and the endgame of cancellation — assembling the tools into concurrent programs with lifecycle boundaries, shutdown, and auditability). One independent granularity sits at the open: Chapter 51's processes (the coarsest isolation unit — the sandbox for running untrusted code) opens the volume.

### This volume's knowledge map

```diagram flow
- Opening: processes (51) - coarsest granularity, isolated execution of external code
- Primitives layer: threads and the synchronization four-piece set (52) - the physical rules of mutex/condition/quota/read-write lock
- Cooperation layer: cancellation token (53) -> coroutine primitives (54) -> scheduler (55) -> Channel (56) -> Future (57) -> executor (58)
- Finale: task groups (59) - structured concurrency, assembling 53~58 into bounded concurrent programs
```

### One main line: cancellation running through

More important than any single tool is the main line of **cancellation running through the whole concurrency system**. Chapter 53 lays the foundation (the token trio and the propagation tree); Chapter 54's coroutines sense cancellation at yield points (the cleanup stack guarantees finishing); Chapter 55's scheduler Close means "cancel everything"; Chapter 56's channel waits are all token-configurable; Chapter 57's Future four-state endgame makes cancellation a first-class citizen; Chapter 58's tasks accept tokens and set checkpoints; Chapter 59's task groups regularize the cancellation tree into scopes. While reading, keep an eye on this line: **every chapter's cancellation interface is a dialect of Chapter 53's token** — once the token is understood, each chapter's cancellation posture costs zero to learn. The line's engineering meaning is graceful shutdown (Chapter 48's signals, second half) — concurrency's value is not just "it runs" but "it can stop".

### Three mental models

Volume 6 offers three reusable mental models (they apply to any concurrency system). **"Shared vs message"**: inside a data structure use locks/atomics (Chapters 52/10), between modules use messages (Channels) — tight inside, clear outside; the two camps are not faiths but layers. **"The cost ladder of waiting"**: threads waiting (1MB stack × concurrency) → coroutines waiting (tens of KB × concurrency) → Futures/callbacks waiting (zero stack) — choose the tool by the amount of waiting; "waiting a lot" steps down. **"Three granularities"**: processes (isolate untrusted code), threads (true parallelism for computation), coroutines (high concurrency for waiting) — the three are not replacements but partners. Use the three models in combination when selecting — Chapter 59's closing selection map stacks them into one decision diagram. Their value exceeds XRT itself — when evaluating any concurrency library, or deciding whether to build your own, interrogate with these three models first.

### Connections to the volumes before and after (this volume is the book's hub)

Looking back: Volume 5 stocked the raw materials — the first glimpse of Futures (Chapter 47's async files), the task-pool prototype, signals' shutdown trigger, and the observation pipeline (Chapter 49's extension into a concurrent context is cited throughout this volume). Looking forward: Volume 7's network engine is Volume 6's biggest customer — one coroutine per connection (54/55), the event loop as the scheduler pump, sends and receives through Channels, results through Futures; Volume 9's HTTP and Volume 10's xhttp build on that; Volume 13's chat/downloader projects are the proving ground for concurrent composition. **Volume 6 is the book's hub** — the first five volumes' capabilities are concurrentized here, and the last four volumes' systems stand on it.

### Three kinds of readers, three routes

**Server-side developers** (the main force): read 52→58 in order (one line from primitives to executors) — Chapter 59's structured concurrency is your direct template for writing "request handling"; the process chapter (51) is for on-demand lookup (read when invoking external tools). **Embedded/resource-constrained developers**: the primitives chapter (52) is your main battlefield (coroutine stack memory and the scheduler trimming per hardware); Channels/Futures weighed against the connection budget; Chapter 59's pool-depth economics matter to you especially. **Developers arriving from Go/Erlang**: good news — the Channel semantics (including the closed three-party rule) align with Go, and structured concurrency resembles Erlang's supervision trees; what needs adjusting is "coroutines bound to threads" (Go's goroutines migrate — XRT's have fixed ownership) and "shared memory legally exists" (messages are not the only choice; use them layered). All three kinds share the same cancellation main line — it is the shortest path to understanding XRT concurrency.

### A reminder: concurrency is a leverage on correctness and on complexity

Every tool in Volume 6 amplifies something — throughput (parallelism), connection count (coroutines), responsiveness (asynchronous IO). But leverage amplifies **everything**: the correct parts are amplified, and the erroneous parts (race condition pitfalls, deadlocks, leaks, uncleaned cancellations) are amplified too — and amplified errors are harder to reproduce (timing-dependent), harder to localize (Chapter 49's pipeline with concurrency corrections), harder to fix (fixing one spot may introduce a timing change elsewhere). Three damage-control habits: **minimal concurrency** — write the single-threaded version correctly first, then concurrentize (concurrency's correctness is benchmarked against the serial version); **explicit boundaries** — what is shared, who waits for whom, how the cancellation tree grows — all written into comments and documentation (implicit conventions explode in concurrency); **observation first** — wire up observation (Chapter 49's pipeline) before adopting concurrency tools, so the first race already has curves to read. Hand the lever to a prepared hand.

### A study check (self-test before leaving this volume)

Close the book and answer eight questions: the semantics of each piece of the four-piece set, and why Condition must pair with a Mutex? What do the cancellation token's "one-way propagation" and "checkpoints" each solve? What do the coroutine's four states, three final states, and cleanup stack guarantee? Why are the scheduler's pump and Post thread-safe across threads? How do the Channel's closed three-party rules guarantee zero-loss shutdown? The Future's four-state endgame and continuation failure forwarding? Why must the executor's shutdown protocol put Close before Wait? Last: given a workload of "read a lot, compute little, wait even more", how do you combine this volume's tools? Eight smooth answers plus an architecture sketch for the last — Volume 6 passed; hazy answers send you back to the relevant chapters' contracts.
