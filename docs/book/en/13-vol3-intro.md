---
num: 13
slug: vol3-intro
title: Volume 3 · Containers and Data Structures: Introduction
volume: 卷三 容器与数据结构
type: intro
lead: Eleven containers, four family conventions, and one selection map — this volume is the physical foundation of all data manipulation in the book.
api: array
---

## Orientation

Volume 3 holds "how to store data". The eleven containers split into five groups: **sequences** (array, buffer, the stack family, linked list) manage element arrangement and flow; **maps and sets** (map, intmap, set) manage "locate by key" and "membership"; **ordered structures** (the two AVL forms) manage "key order and ranges"; **queues** (SPSC/MPSC/MPMC) manage "cross-thread transfer"; **pools** (fixed pool, variable pool) manage "batched object take-and-put". Chapters 14 through 22 cover each closely, and Chapter 23 closes by assembling them into a selection map.

### Four family conventions (learn the conventions first, then the containers)

Every container of Volume 3 shares four conventions, first established in Chapter 14 and referenced directly afterward. **Handle on the stack, memory on the heap**: `Init` opens shop with a stack handle, the container manages its own buffers, `Unit` returns everything exactly; when the container object itself must live on the heap, use `Create`/`Destroy`. **Read via fields, write via functions**: read-only state like `Count`, `Size`, and `Data` are public fields — zero overhead on hot paths; fallible operations go through function return values with Chapter 4's error slot. **No side effects on failure**: a failed edit leaves the container untouched, so cleanup code is always safe. **Three-part iteration**: `Begin`/`Next`/`End` ordered-snapshot iteration — pointers do not outlive the iteration, and inserts/deletes are forbidden during it — map, set, AVL, and pool traversal are all isomorphic. The four conventions make "learning one container" cheaper as the count grows: Chapter 14 is the most expensive, and every later chapter reuses.

### One through-line: ownership and order

Deeper than the four conventions runs **ownership and order**. Ownership asks "who manages this data" — value semantics (the slots of arrays/maps) hand full management to the container, intrusive style (the nodes of lists/AVL) lets you keep the object while the container manages only the links, owning style (the container-form AVL) entrusts life and death to the tree, pools manage take-and-put, arenas manage batch reclamation — Chapter 5's ownership language lands in Volume 3 as concrete container choices. Order asks "in what sequence do I access" — index, insertion order, key order, LIFO/FIFO, priority — each container promises exactly one order: the array's index, the map's insertion order, the AVL's key order, the stack's last-in-first-out, the queue's first-in-first-out. Answer these two questions when selecting a container and the answer is basically out — Chapter 23's selection map unfolds exactly these two questions into decision paths.

### Reading advice

Three kinds of readers, three routes. **Sequential**: Chapters 14 through 23 — conventions established up front, the map closing at the end; suited to first systematic study. **On demand**: jump straight to the container your project needs, after reading this chapter's four conventions and the two through-line questions — the conventions are every chapter's prerequisite. **Review**: read Chapter 23's selection map directly, hopping back from the map to each chapter's contracts — suited to interview prep or a quick pass before a technical review. Whichever route, every chapter's two complete examples deserve a hands-on run: containers are "feel" knowledge — only what you have run truly belongs to you.

### One reminder: this volume does not solve concurrency

Except for the queues, none of this volume's containers are thread-safe — not a defect but a layering decision (every chapter's contracts state it). When containers must be shared across threads, the answer is in Volume 6: reader-writer locks for read-heavy workloads, queues for cross-thread transfer, sharding for high concurrency. Setting this expectation now avoids the detour of "discovering at Volume 6 that the selection must be redone" — a container layer designed under single-thread assumptions is the most expensive part of a concurrency refactor.

### The volume's knowledge map

```diagram flow
- Sequence group: array (index), buffer (byte stream), the stack family (four LIFO variants), linked list and slot_map (intrusion and generation handles)
- Map group: xmap (byte-key insertion order), xintmap (integer-key key order), set (membership and set algebra)
- Ordered group: intrusive AVL (zero allocation, multi-index), container-form AVL (ownership and range queries)
- Transfer group: SPSC/MPSC/MPMC lock-free queues (this volume's only thread-safe containers)
- Object group: fixed pool (equal-size slot LIFO reuse), variable pool (size bucketing)
- Finale: Chapter 23 packs all of the above into one selection decision map
```

### Learning self-checks (before leaving this volume)

With the book closed, answer six questions, each mapping to a group of containers: a set of same-typed records needing frequent indexed access and whole sorting — which? A byte stream needing "accumulate in segments, take whole" — which? An object lifecycle that is tidy and wants reclamation together — what are Chapter 7's answer and this volume's answer, and why don't they conflict? For key-based lookup, which map for "insertion-order export" and which for "key-order export"? Why is hashing powerless for "key order + range queries", and how to choose between the two AVL forms? Why is the take-and-put of a hundred thousand connection objects given to a pool rather than the heap? Answer all six fluently and Volume 3 is truly read — for any that stumble, reread the corresponding chapter's contracts — the six questions point respectively to: array, buffer, the division of labor between arena and pool, the two map families, the two AVL forms, pooled objects.

### A performance view: the battle of constants

Container performance debates are mostly battles of constants; three constant factors recurring in this volume deserve intuition now. **Locality**: contiguous storage (arrays, buffers, map slots) is prefetcher-friendly, while chained structures (lists, trees) make every hop a cache-miss opportunity — across million-scale traversals this often rings louder than algorithmic complexity. **Allocation counts**: value-inlining (arrays, maps) and pooling drop allocation from "per object" to "per container/per page"; intrusive goes to zero — Chapter 6's backing curves are the observable divide between the three strategies. **Synchronization cost**: lock-free queues price by topology, SPSC cheapest — "once the topology is known, pick the specialized one" is the discipline this volume teaches in its only brush with concurrency. The three factors constrain each other at selection time: the list's O(1) insert-delete often loses to the array's shifting in the face of cache locality — Chapter 23's map fixes such "counterintuition" into decision rules, and the intuition to take away now is: **magnitude first, then constants, cleverness last**.

### Links to the volumes before and after

Looking back: Volume 1's memory system (Chapter 5's owning style, Chapter 7's arena) is the foundation of the container ownership language, and Chapter 6's statistics is the tool for verifying container performance. Looking forward: Volume 4's parsers assemble from stacks (frame stacks) and maps (word frequency); Volume 6's concurrency system takes queues and pools as its physical carriers; Volume 7's network engine's connection table is the full bloom of "array + map + slot_map + pool". One can say every container of Volume 3 plays a lead role in some later volume — know them well now, and when you meet them again you only need to focus on the one new variable: the business.
