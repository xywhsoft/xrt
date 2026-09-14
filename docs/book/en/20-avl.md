---
num: 20
slug: avl
title: AVL Trees: Intrusive and Owning
volume: 卷三 容器与数据结构
type: practice
lead: Two faces of the ordered index — the intrusive xavl hangs objects with zero allocation; the owning xavltree owns its objects.
api: avl
---

## Orientation

The hash map answers "where is this key"; the AVL tree answers "what else is next to this key": in-order iteration is naturally sorted, and range queries ("all sessions with IDs between 20 and 30") walk the tree once. XRT gives it two faces — **intrusive `xavl`** (the node embeds in the business object, insert/delete cost zero allocations, same clan as Chapter 17's linked list) and **owning `xavltree`** (the tree owns the objects; taking one out is a handover). The choice logic between the two faces carries straight on from "value arrays vs pointer arrays" (Chapter 14): object ownership decides the container's form. This chapter teaches the two with a pair of complete examples and thoroughly covers "multiple indexes" (one tree by ID, another by name) — the intrusive form's killer application.

## Introduction

A session manager needs two views: operations need "find this one exactly by session ID" (hashing does that), while auditing needs "export everything in ID order" or "export a slice of the ID range" — hashing can't, sorted arrays insert linearly, lists aren't ordered at all. The AVL tree's answer is a balanced binary search tree: insert, delete, and lookup are all O(log n), **in-order traversal is the sorted sequence**, and range queries walk the tree once.

The second decision follows immediately: does the tree own the objects or vice versa? The session objects already live in a pool (their lifecycle is the pool's business); the tree only needs to "organize them by ID" — intrusive `xavl`, the node is a field of the object, zero allocations. The standalone scenario ("this batch of config items should live and die with the tree") takes the owning `xavltree` — inserting is a handover, taking out is a return. This decision mirrors Chapter 14's exactly; with this chapter, Volume 3's multiple-choice exam gets its last piece.

## Concepts

### Two faces, one table

| Dimension | Intrusive `xavl` | Owning `xavltree` |
| --- | --- | --- |
| Object ownership | caller's (stack/pool/array, anywhere) | the tree's |
| Insert/delete | **zero allocation** (links only) | insertion allocates a node |
| Multiple indexes | one object, several node fields, several trees | one tree, one ownership |
| Comparison function | reverse-looks-up the host from the node (`offsetof`) | compares elements directly |
| Typical scenarios | multi-view indexes over pooled objects | ordered owning of standalone config/data |

### In-order is sorted

```diagram flow
- Insert: the comparison function seats it; imbalance auto-rotates - balance is the tree's private business
- Lookup: O(log n), descending by the comparison function
- In-order iteration: left-root-right visit order = ascending key sequence
- Range query: start at the lower bound, stop at the upper, walking only the interval subtrees
```

AVL's "self-balancing" is a black box to the user: you supply only the three-way comparison function; rotations, heights, and rebalancing are the tree's own maintenance. The comparison signature deserves a look — `(查找键, 树节点, 用户数据)` (lookup key, tree node, user data): the key and the node are **different types** (one is a key, one is a key-bearing node); in the intrusive form the comparison first reverse-looks-up the host via `offsetof` and then compares keys — the same "reverse lookup" idiom as Chapter 17's `XRT_CONTAINER_OF`.

### Intrusive: the zero-allocation index

The `xavl` handle itself is content-free (`Init` only initializes the handle); insertion only rearranges the existing objects' links — no heap allocation anywhere. Where the business objects live is your call: a stack array (example one), an object pool (Chapter 22), even a slot_map — the tree doesn't care, it only links. One object with two node fields can hang on two trees at once: "one by ID, one by name" is the C form of database-style multiple indexes. The price is written in the comparison function: every comparison reverse-looks-up the host; this indirection is the intrusive form's inherent cost.

### Owning: ownership and handover

`xavltree` manages the objects' life and death: inserting is a handover (the tree allocates a node wrapping the object), iteration yields object pointers, and taking (the `Take` family) gives the object back to you and returns the node to the tree. It suits scenes where "the data naturally belongs to this tree" — config items, static dictionaries. When you need neither multiple indexes nor an external pool, the owning form writes half the boilerplate.

### Division of labor with the hash map

AVL and Chapter 18's maps are not competitors but colleagues. For exact single-key lookup both are O(1)/O(log n) grade, and hashing has the smaller constant; but the moment the question becomes "what else is next to this key" — ordered traversal, range queries, predecessor/successor, min/max — hashing is structurally unable, while AVL starts at O(log n) for all of it. In practice a resource manager often keeps both: `xmap` for hot-path location, `xavl` as the ordered view for audits and exports, the same batch of objects under two indexes (one object hanging on two indexes fits the intrusive form exactly). Chapter 23's selection table writes this division as a rule: **lookup only → map; lookup + order → AVL; both → keep both; never force one container to carry two kinds of questions**.

### Duplicate-key semantics

If the tree already holds a node with the same key, `Insert` returns **that existing node** (non-NULL) — duplicates neither error nor overwrite; "what to do" goes back to the caller: replace the value, refuse, or rename. Memorize the return-value semantics: **NULL is the successful insert** (opposite of many libraries); non-NULL means "key collision, here's the old node".

## Examples

### Complete program: intrusive index and ordered iteration

From the repository example `examples/containers/avl/main.c`:

```embed path="examples/containers/avl/main.c" title="examples/containers/avl/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/containers/avl/main.c -lws2_32 -liphlpapi
id=10 name=alpha
id=20 name=beta
id=30 name=gamma
```

**What just happened.** (1) The session array sits on the stack (30, 10, 20 unordered); the `Index` field embeds an `xavlnode` — `xrtAVLNodeInit` zeroes the links before hanging; this step is not optional. (2) The comparison `(iID > pSession->ID) - (iID < pSession->ID)`: reverse-looks-up the host, takes the key, returns three-way; the subtraction form prevents overflow (an old friend from Chapter 14). (3) `xrtAVLInsert` succeeds only by returning NULL — in the loop, the `== NULL` branch is the success branch. (4) **Zero heap allocation** throughout: the tree merely linked three stack objects into an ordered structure. (5) In-order iteration always outputs 10, 20, 30 — the insertion order 30,10,20 is irrelevant to the output; the value of an "index" is decoupling insertion order from the ordered view. (6) `offsetof(examplesession, Index)` reverse lookup: node address minus field offset gives the host address — the intrusive style's companion idiom.

### Complete program: the owning tree and range queries

From `examples/containers/avl_tree/main.c`, demonstrating the owning tree and interval iteration:

```embed path="examples/containers/avl_tree/main.c" title="examples/containers/avl_tree/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/containers/avl_tree/main.c -lws2_32 -liphlpapi
id=20 timeout=2000
range id=20 timeout=2000
range id=30 timeout=3000
```

**What just happened.** (1) Insert-as-handover: once the config objects enter the tree, the tree holds them; iteration yields object pointers. (2) First an exact hit on id=20 — the shape of lookup and hit output. (3) Range iteration walks from the lower bound 20 to before the upper bound 40, outputting 20, 30 — "interval export" is the direct application of range iteration; not even a full-tree scan is needed. (4) The tree manages life and death: at `Unit`/`Destroy` the objects are released with it — the owning semantics save you worry and one layer of control.

### From examples to engineering: the three-tier index structure

A real resource manager is usually a "three-tier index structure": at the bottom, object storage (a pool or slot_map, managing life, death, and stable addresses); in the middle, the location index (`xmap` or `xintmap`, key to handle); on top, the ordered view (intrusive `xavl`, for audit exports and range queries). Each tier does its own job — storage guarantees stable addresses, location carries the hot path, the view carries order — and intrusive AVL exists precisely because the storage tier guarantees the object addresses never move. This three-tier structure appears in full form in Chapter 67's network connection table; when you get there, come back and compare: this chapter's two examples trained the minimal usage of the middle and top tiers respectively.

## Contracts

- **Duplicate keys**: `Insert` returning non-NULL = collision, the return being the existing node; NULL = success. Write the success branch as `== NULL`.
- **Intrusive zero allocation**: `NodeInit` before `Insert`; the object's lifetime must cover its tree period; unlink before destroying the object (same discipline as Chapter 17's linked list).
- **Owning**: insert-as-handover; `Unit`/`Destroy` release the objects themselves.
- **Comparison function**: three-way return; the intrusive form reverse-looks-up the host first; prevent overflow with the two-step subtraction comparison.
- **Iteration**: in-order = ascending; range iteration walks the interval subtrees, never the whole tree.

### Giving the comparison function a checkup

All of AVL's behavioral correctness rides on the comparison function; it deserves to be tested as an independent unit. Three checkup questions: **is it three-way** — equal must return 0; a comparison returning -1 or 1 for equality makes "can't find the key I just inserted"; **is it overflow-safe** — integer comparisons use the two-step subtraction form; **is it consistent** — the same pair of keys must compare complementarily from either direction (a<b implies b>a); an inconsistent comparison function destroys the tree's structural balance entirely. Finish the comparison function, feed it hand-made cases (less/equal/greater/extremes) like a callback would be fed, then hang the tree — this order saves the whole "the tree behaves bizarrely" debugging session later.

## Pitfalls

### Pitfall 1: using Insert's return value as a boolean

Symptom: insertion "always fails" or "always succeeds" — depending on how you wrote the if; on collision the object silently vanishes or the old data gets misread.

Cause: opposite to other libraries, `xrtAVLInsert`'s NULL is success and non-NULL is the collision-returned old node — writing the test on the "non-null is true" reflex inverts the logic wholesale.

```c bad
if ( xrtAVLInsert(&tSessions, &pSessions[i].Index, &pSessions[i].ID,
		exampleCompare, NULL, NULL) ) {
	/* assumed entering here = success; actually entering here = key collision */
}
```

```c good
if ( xrtAVLInsert(&tSessions, &pSessions[i].Index, &pSessions[i].ID,
		exampleCompare, NULL, NULL) == NULL ) {
	/* NULL is the successful insert */
}
```

### Pitfall 2: forgetting NodeInit, or destroying an object still on the tree

Symptom: the former crashes on insert (the links are garbage); the latter is the same family as the list — the tree points at freed memory, and the crash lands far from the scene.

Cause: uninitialized link fields in the intrusive form are undefined state; destroying a tree-period object takes the tree's pointers down with it.

```c bad
examplesession* pDead = find_expired(&tSessions);
xrtFree(pDead);                        /* Index is still on the tree - the tree now dangles */
```

```c good
examplesession* pDead = find_expired(&tSessions);
xrtAVLRemove(&tSessions, &pDead->Index);   /* unlink first */
xrtFree(pDead);                             /* destroy after */
```

## Exercises

### Basic: unordered-insert verification

Reproduce the main example: insert 5 sessions in disorder, assert the in-order output is always ascending; rerun with a different insertion order — the output is unchanged. The tree's internal shape may differ completely between runs while the in-order output agrees — observable proof that "balance is the tree's private business".

### Advanced: double indexes (the intrusive killer app)

Give the session object a second node field; build two trees, "by ID" and "by name lexicographically"; the same batch of objects outputs each tree's in-order separately. Hint: two comparison functions + two rounds of Insert; the objects exist only once.

### Challenge: an interval exporter (range iteration vs brute-force filtering)

Load 1000 random-ID configs into an owning tree; implement `export(minId, maxId)` outputting everything in the interval; compare node-visit counts against "full iteration + manual filtering". Acceptance criteria: with 10 random intervals inside [0,1000], the output matches the brute force line by line; record and report the iteration-count difference between the two methods.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Two forms | intrusive `xavl` — zero allocation, multiple indexes / owning `xavltree` — owns the objects |
| Return value | `Insert` NULL = success; non-NULL = the existing node with the same key |
| Reverse lookup | `offsetof(宿主, 节点字段)`; same stock as `XRT_CONTAINER_OF` |
| Order | in-order iteration = ascending keys; range iteration walks interval subtrees only |
| Tree-hanging discipline | `NodeInit` first; unlink before destroying; lifetime covers the tree period (same as Chapter 17's list) |
| Three-tier structure | storage (pool/slot_map) + location (map) + ordered view (intrusive AVL) — the real resource-manager standard |
| Balance | auto-rotation; the user only supplies a three-way comparison; feed the comparison hand-made cases before hanging |
| Division with maps | lookup only → map; lookup + order → AVL; both needs → both indexes coexist |
| Complexity | lookup/insert/delete O(log n); in-order and range walk only the necessary subtrees; coexisting with a map, each carries its own duty |
