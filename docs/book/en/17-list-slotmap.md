---
num: 17
slug: list-slotmap
title: Doubly Linked Lists and slot_map
volume: 卷三 容器与数据结构
type: practice
lead: Intrusive lists hang objects with zero allocation and maintain LRU order in O(1); generation-stamped stable indices prevent stale handles from hitting reused slots.
api: list, slot_map
---

## Orientation

This chapter puts two "counter-intuitive but extremely common" containers side by side and closes with a quick sketch of their combination. After it, your toolbox of "positions and identifiers" is complete: array indices, list nodes, and generation handles each in their place. The **intrusive doubly linked list** `xlist` embeds the node inside the host object — hanging on a chain costs zero allocations, one object can hang on several chains, and unlinking never destroys the object; the O(1) order maintenance of an LRU cache is its textbook application. **slot_map** `xslotmap` solves the classic trap of "index handles": after a slot is reused, an old handle wrongly hits the new object — a generation counter makes stale handles fail automatically. Together the two containers answer one question: **when the system can change an object's "position", how does an external reference stay correct** — the list's answer is "the node is part of the object", slot_map's answer is "the handle carries a version number".

## Introduction

Two crash sites. Site one: an LRU cache built as "array + re-sort on every hit" — once the cache grows, sorting becomes the throughput bottleneck; switching to a list revealed that every node move allocates a new node and frees the old one — the allocator got busier than the sort. The intrusive list solves it in one move: the node is a field of the object, moving only rewires two pointers — O(1) and zero allocations. Site two: a connection table hands array indices to the business layer as handles; connection 1 drops, a new connection reuses slot 1, the business layer operates on the old handle and hits someone else's connection — data cross-talk and privilege escalation can grow from exactly here. slot_map's generation handles make old handles fail automatically after slot reuse; the wrong hit is refused at the container layer.

The technical essence of both sites is the same: **an identifier must carry enough information to fight "position reuse"**. The list dodges position via "identity embedded"; slot_map versions position via "index + generation".

Worth spelling out: neither container manages object memory for you — the intrusive list allocates and frees nothing, and slot_map stores only `ptr`. They are providers of "structure and order", not of "ownership"; ownership always belongs to Chapter 5's regime. This division of duty lets them sit without worry on top of any memory strategy (heap, pool, arena), and it also means destruction-path discipline lies entirely with the caller — pitfall 1 of this chapter is that discipline's enforcement scene.

## Concepts

### Intrusive: the node lives inside the object

```diagram flow
- Host object: a business struct embeds one xlistnode field
- Hanging on: PushFront only rewires pointers - zero allocation, zero copy
- Reverse lookup: XRT_CONTAINER_OF subtracts the field offset from the node address to recover the host
- Multiple chains: embed another node field to hang on a second chain simultaneously
- Unlinking: Clear only detaches; the host object's lifecycle belongs entirely to the caller
```

Compared with the traditional list where "the container owns nodes and stores element pointers", the intrusive style inverts the ownership relation: **the container does not own the objects; the objects carry their own hooks**. The cost is that the struct must reserve a field for "might hang on a chain later"; the payoff is zero-cost attachment, one object belonging to several groupings at once, and unlinking that never touches the object's lifecycle.

### LRU: MoveFront is the promotion on hit

Head = most recently used, tail = least recently used. On a hit, `xrtListMoveFront` moves the node to the head in O(1) — only the two directional pointers change, independent of chain length. Evicting the "least recently used" is taking the tail. Paired with Chapter 20's map doing "key → node" lookup, a complete LRU is these two containers plus one MoveFront.

### Generation handles: index + version

First make "why a version" plain: a 32-bit index by itself is a perfect position identifier — one-step location, O(1) access; its only flaw is that it carries no history — after a slot is reused, the old and new indices are indistinguishable. Adding a 32-bit generation completes the puzzle: position answers "where", generation answers "which incumbent". `xslotmap`'s handle `xslot` is a 64-bit pack: **generation << 32 | index**. Each time a slot is reused, the generation increments. `Contains` validates index and generation together — an old handle (generation behind) is judged invalid, even if the index points at a slot now occupied by a new object. `Remove` validates the same way; a generation mismatch refuses the deletion. This is the standard foundation of "connection tables, resource pools, ECS entity management": the outside world holds handles, and the container guarantees the handle's semantic precision.

### When not to use a list

The list's O(1) insert/delete has a precondition: **you already hold that node**. Lookup by index or by value is still O(n) — the very act of "finding an element in a list" eats the O(1) advantage whole. Three anti-rules: need random access → array (Chapter 14); need key lookup → map (Chapter 18); need ordered scanning → sorted array or AVL (Chapter 20). The list's irreplaceable scenarios narrow to: order maintenance (LRU, scheduling queues, free lists) and "the object is known, only its grouping membership changes". The rule of thumb: **first ask "how do I get the node", then ask "how frequent are the insertions and deletions"** — wherever the former has no O(1) answer, a list cannot help you.

### Traversal and the complexity portrait

List traversal walks node by node from head to tail (`First`/`Next`); insertion and deletion are O(1); but there is **no random index access** — finding a node by index is O(n). slot_map is the opposite: the handle is the index, access is O(1), but traversal must skip empty slots. Each one's strength is the other's weakness; combining them (slot_map stores the objects + list maintains the order) is the classic resource-manager architecture — Chapter 67's network connection table is exactly this combination scaled up: O(1) handle location, O(1) event-order maintenance, clear lifecycle ownership.

## Examples

### Complete program: O(1) LRU order maintenance

From the repository example `examples/containers/list/main.c`:

```embed path="examples/containers/list/main.c" title="examples/containers/list/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/containers/list/main.c -lws2_32 -liphlpapi
3=300 2=200 1=100
1=100 3=300 2=200
```

**What just happened.** (1) `XRT_LIST_INIT` and `XRT_LIST_NODE_INIT` static initialization — both list and node start at zero cost, no runtime init calls. (2) Three entries `PushFront` in turn: the later push sits at the head, the output is `3 2 1`. (3) After accessing entry 1, `MoveFront`: the output becomes `1 3 2` — entry 1 went from oldest to newest, and the operation only touched pointers. (4) `XRT_CONTAINER_OF(pNode, cacheitem, Recent)` recovers the host from the node address — the intrusive style's companion idiom, the field offset computed at compile time. (5) `Clear` unlinks all the nodes; `Items` is a stack array, needing and deserving no free — **unlinking does not destroy objects**.

### Complete program: generation vs. mistaken hits

From `examples/containers/slot_map/main.c`:

```embed path="examples/containers/slot_map/main.c" title="examples/containers/slot_map/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/containers/slot_map/main.c -lws2_32 -liphlpapi
old=4294967297 replacement=8589934593 same-index=yes stale-valid=no
```

**What just happened.** (1) Two connections inserted yield two handles, `First = 0x1_00000001` (generation 1, index 1). (2) After deleting First and inserting a replacement, free slot 1 is reused and the new handle is `0x2_00000001`: **same index, generation already incremented from 1 to 2** (`same-index=yes`). (3) The key assertion comes last: the old handle `First`'s `Contains` returns `no` — it can never mistakenly point at the replacement connection. Compare the two handles' decimal values in the output (4294967297 and 8589934593): the difference is exactly 2 to the 32nd power — the generation carried a digit. Rewrite this program with "bare-index handles" and rerun, and `stale-valid` turns `yes` — watching the crash scene with your own eyes sticks better than reading the docs ten times. The rewrite only swaps the handle for a bare index and `Contains` for a bounds check — a change of ten lines or fewer degrades a safe container into an accident scene; this controlled experiment is also the best material for explaining in code review "why the handle must not be taken apart".

### A quick sketch of the combined architecture

The two containers' individual weaknesses (the list can't find, slot_map has no order) cancel each other in the combination — worth pinning the canonical shape with a sketch: connection arrives → `slot_map` insert yields a handle while the node `PushFront`s onto the active chain; event fires → the handle locates the object in O(1), and after processing `MoveFront` promotes the timing; connection closes → unlink first, then `Remove`, the handle goes stale with it (the generation already recorded); the outside operates on an old handle → the container layer refuses, cross-talk cannot happen. Four steps cover a resource manager's daily life; Chapter 67's network connection table scales this up into a concurrent engine, but the skeleton is exactly these lines, no more, no less.

## Contracts

- **Intrusive ownership**: the list never owns objects; `Clear`/unlinking never frees hosts; an object must be unlinked before it is destroyed.
- **Static initialization**: `XRT_LIST_INIT` / `XRT_LIST_NODE_INIT` need no runtime initialization; paired runtime versions also exist.
- **Generation semantics**: slot reuse increments the generation; handle validation requires the "index + generation" double match; `XRT_SLOT_INVALID` is the invalid sentinel; taking a handle apart and keeping only the index equals abandoning the protection.
- **Access complexity**: list by index O(n), insert/delete O(1); slot_map by handle O(1); the combined architecture takes the best of each.
- **Threads**: neither is a thread-safe container — cross-thread access needs external synchronization (Volume 6 covers the right primitives).

## Pitfalls

### Pitfall 1: destroying an object while it is still linked

Symptom: list traversal touches freed memory; or an unlink writes into freed node fields — the crash lands far from the accident site.

Cause: the price of intrusiveness is "the object's lifetime must cover its linked period" — before the host object is destroyed, its embedded node is still on the chain, and the chain's pointers aim at memory about to vanish.

```c bad
cacheitem* pItem = find_expired(&Cache);
xrtFree(pItem);                      /* the node is still on Cache - the chain now dangles */
```

```c good
cacheitem* pItem = find_expired(&Cache);
xrtListRemove(&Cache, &pItem->Recent);   /* unlink first */
xrtFree(pItem);                           /* destroy after */
```

### Pitfall 2: taking a slot_map handle apart and using only the index

Symptom: handle-reuse mistaken hits come back to life — precisely the accident the generation mechanism exists to prevent, returning inside "clever" code.

Cause: `xslot` is a packed value of generation and index; bitwise-extracting the index and reassembling it yourself discards the generation.

```c bad
size_t iIndex = First & 0xFFFFFFFFu;         /* keep only the 32-bit index */
connection* pConn = (connection*)xrtSlotMapGet(&tMap, iIndex);  /* using the index as a handle */
```

```c good
connection* pConn = (connection*)xrtSlotMapGet(&tMap, First);   /* pass the handle as-is */
if ( pConn == NULL ) {
	/* generation mismatch: this handle is stale - treat as a stale reference */
}
```

## Exercises

### Basic: reproduce the chain order

Reproduce the sample's two output lines (derive by hand first, then run and compare); add one step, "evict the tail" (take out the oldest entry and unlink it), and print the chain order after eviction.

### Advanced: a complete LRU

Use slot_map or map to hold "key → cacheitem" with the list maintaining recency; implement `get` (on hit MoveFront, on miss return null) and `put` (evict the tail past capacity). Hint: with capacity 3 and accesses 1/2/3/1/4 in order, the evicted one should be 2 — derive by hand first, then verify in code.

### Challenge: a connection-table prototype (this chapter's comprehensive exam)

Implement a connection table with slot_map: `open` inserts and returns a handle, `close` removes by handle (generation mismatch returns failure), `send` fetches the object by handle. Simulate one "old handle arriving after a new connection reuses the slot" and verify that `send` refuses rather than cross-talks. Acceptance criteria: in the reuse scenario all old-handle operations fail with distinguishable errors; the normal open-close loop succeeds end to end; zero leaks (verified with Chapter 6's stats).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Intrusive | node embedded in the host; zero-allocation linking; unlinking never destroys the object |
| Reverse lookup | `XRT_CONTAINER_OF(节点, 类型, 字段)` recovers the host |
| LRU | head is newest; `MoveFront` promotes in O(1); eviction takes the tail |
| Generation handle | `代际<<32 / 下标`; slot reuse bumps the generation; old handles fail automatically |
| Sentinel | `XRT_SLOT_INVALID`; `Contains`/`Get` double-validate |
| Combined architecture | slot_map locates O(1) + list orders O(1) — the resource-manager standard (Chapter 67's connection table) |
| Threads | not thread-safe; cross-thread needs external synchronization |
| Reverse-lookup formula | `XRT_CONTAINER_OF` computes the field offset at compile time, node back to host at zero cost; as many node fields as you embed, that many groupings the object can belong to |
