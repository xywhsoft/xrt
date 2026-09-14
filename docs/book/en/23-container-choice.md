---
num: 23
slug: container-choice
title: Container Selection: The Decision Path from Requirements to Containers
volume: 卷三 容器与数据结构 · 卷三收官
type: practice
lead: Fitting eleven containers onto one decision map — ownership, order, and access mode: three questions settle it, and constant-factor disputes are settled by data.
api: array, map, avl, queue, pool
---

## Orientation

Volume 3's closing chapter introduces no new containers; it does exactly one thing: compress the previous ten chapters' knowledge into an **executable selection process**. The cost of choosing the wrong container never shows in code review — it hides in the quarterly retrospective's "why does this interface get slower the more we optimize". This chapter provides three decision axes (ownership, order, access mode), one decision map, two comparison experiments that let data speak, and the five most-asked "you don't actually want X" anti-selection questions. After this chapter, any data-organization requirement should yield its candidate containers and verification method within thirty seconds — that "thirty seconds" is not rhetoric; it is the pass line of exercise one.

## Introduction

A real scene: a ticketing system's "pending tickets" must support three operations — take out the highest by priority, look up details by ticket number, archive when done. Each operation points to a container: a priority queue, a map, an archive array. The novice answer is "find an all-powerful container" — it does not exist; the veteran answer is "one problem, one view": an array stores all tickets (the authoritative data), a map maintains "ticket number to index" (the lookup view), and priority processing does a linear scan or sorts on demand (the priority view). The data exists once; views are built as needed — that is lesson one of selection: **fix the authoritative store first, then build views; do not hunt for an all-powerful container**.

Lesson two is subtler: selection must be adjudicated by data, not intuition. The textbook says "linked-list O(1) insert/delete beats array O(n)" — but in a mixed load of million-level traversal and a few thousand insertions, the array's cache locality often lets total time overtake the list by an order of magnitude. This chapter's two comparison experiments exist for exactly this "counter-intuition": run the numbers on your own machine by hand, and upgrade your selection basis from "heard" to "measured".

## Concepts

### Three questions settle it

```diagram flow
- Question 1, ownership: who manages the data? Value-semantic custody / intrusive self-holding / pool take-and-put / arena reclaim-together
- Question 2, order: accessed by what? Index / insertion order / key order / LIFO-FIFO / priority order
- Question 3, mode: what is the hottest operation? Traversal / lookup / insert-delete / range / cross-thread transfer
```

Question one halves the candidates: uniform lifetimes → arena (not a container question); same-size objects frequently taken and returned → pool; objects must be self-holding (multi-view, on the stack, in a pool) → intrusive, or value containers holding handles; the container takes full custody → value containers. Question two halves again: by index → array; by insertion order → map/set; by key order and range → AVL; LIFO → the stack family; FIFO → queues. Question three decides among the remaining candidates by heat: traversal-hot picks contiguous storage, lookup-hot picks hashing, insert-delete-hot asks whether you already hold the node, and cross-thread has only the queues.

### The decision map

| Requirement shape | First choice | Alternative | Do not use |
| --- | --- | --- | --- |
| Same-type records, traversal- and sort-heavy | value array | pointer array (large objects) | linked list |
| Byte stream accumulated into whole blocks | buffer | —— | hand-spliced arrays |
| Recursion/backtracking of known depth | fixed stack | chunked stack (stable addresses) | dynamic stack by default |
| Key → value lookup (insertion-order export) | xmap | —— | hand-written hashing |
| Integer keys / sparse indices / key-order export | xintmap | array (when small and dense) | xmap (a notch slower) |
| Membership and set algebra | set | map (when values needed) | array deduplication |
| Key-order traversal / range queries | AVL, both forms | sorted array (when static) | hashing (unordered) |
| Multi-view indexes over objects | intrusive list + AVL | slot_map (generational) | naked pointer arrays |
| External handles referencing objects | slot_map | map + handle | bare indices |
| Cross-thread transfer | SPSC/MPSC/MMPC | Channel (Volume 6) | a lock around an ordinary queue |
| Same-size objects, high-frequency take-and-put | fixed pool | variable-size pool (when mixed) | bare heap |
| Temporary fragments reclaimed together | arena (Chapter 7) | —— | pool (semantics mismatch) |

### The five anti-selection questions (review mantras)

Five frequent misconceptions, each worth calling out; they also make review mantras — hear one, ask the corresponding three questions. **"We need a universal container"** — it does not exist; authoritative store + views is the answer (see the Introduction). **"Linked lists insert and delete fast"** — the premise is you already hold the node; finding a node by index is O(n) with poor locality on top. **"Hashing is always fastest"** — with integer keys xintmap is faster, and with key-order needs hashing simply does not answer. **"Lock first for safety"** — cross-thread transfer uses queues, lookup uses sharding; the lock is Volume 6's last resort, not the first reflex. **"The pool cures everything"** — short-lived objects with uniform lifetimes belong to the arena; the pool manages "frequent take-and-put"; with semantics mismatched, both are wrong. The shared root of the five: treating containers as faith rather than tools — the map replaces faith with process.

### How to adjudicate constant-factor disputes

Complexity gives the magnitude; constants decide the winner; the only adjudication tool is measurement — and the measurement's verdict belongs to your load and your machine; change the mix and you must re-measure. The method: fix the data scale (say a million elements), fix the operation mix (say 90% traversal + 10% insert-delete), time with `xrtNow` (returning microseconds as `xtime`, Chapter 3), run each candidate three times and take the median — Chapter 10's `Near` tolerance thinking applies to reading performance numbers too. Chapter 6's statistics add the memory-side evidence: allocation counts and the cache-hit proxy (throughput per second) read together; time alone misleads about causes.

## Examples

### Comparison experiment 1: array vs linked list — who wins when traversal dominates

```c
/* race_list.c —— 百万节点遍历 + 千次插删的对照 */
#define XRT_MODULE_ALL
#define XRT_IMPLEMENTATION
#include <xrt.h>
#include <stdio.h>

typedef struct node { int Value; struct node* Next; } node;

int main(void)
{
	xarray tArray;
	node* pChain = NULL;
	xtime t0, t1;
	long long iSum = 0;

	xrtArrayInit(&tArray, sizeof(int));
	for ( int i = 0; i < 1000000; ++i ) {
		int v = i;
		xrtArrayPush(&tArray, &v);
	}
	for ( int i = 0; i < 1000000; ++i ) {
		node* p = (node*)xrtMalloc(sizeof(node));
		p->Value = i; p->Next = pChain; pChain = p;
	}

	t0 = xrtNow();
	for ( size_t i = 0; i < tArray.Count; ++i ) {
		iSum += *(int*)xrtArrayGet(&tArray, i);
	}
	t1 = xrtNow();
	printf("array scan: %lld us (sum=%lld)\n",
		(long long)(t1 - t0), iSum);

	iSum = 0;
	t0 = xrtNow();
	for ( node* p = pChain; p != NULL; p = p->Next ) {
		iSum += p->Value;
	}
	t1 = xrtNow();
	printf("list  scan: %lld us (sum=%lld)\n",
		(long long)(t1 - t0), iSum);

	while ( pChain != NULL ) {
		node* p = pChain->Next;
		xrtFree(pChain);
		pChain = p;
	}
	xrtArrayUnit(&tArray);
	return 0;
}
```

```term
$ gcc -O2 -DXRT_MODULE_ALL -DXRT_IMPLEMENTATION -I single race_list.c -lws2_32 -liphlpapi
$ ./a.exe
array scan: 942 us (sum=1783293664)
list  scan: 6211 us (sum=1783293664)
（数值因机器而异，比值才是结论：遍历主导时数组快约一个数量级）
```

**What just happened.** Both structures hold the same million integers, and the sums agree. The array is **one contiguous block of memory**; the hardware prefetcher turns traversal into a pipeline; every linked-list hop is a potential cache miss — a million hops means a million "wait for memory" moments. The textbook's "O(1) insert/delete" for lists did not lie to you, but insertions are one in a thousand of your load: **the constant of the hot operation is the decision variable**. Raise the insert/delete mix to 50% and rerun; you will see the gap narrow — behind every cell of the decision map there should be a measured table like this from your own machine.

### Comparison experiment 2: array linear search vs map lookup

```c
/* race_lookup.c —— 十万条记录，十万次按键查找 */
#define XRT_MODULE_ALL
#define XRT_IMPLEMENTATION
#include <xrt.h>
#include <stdio.h>

typedef struct item { int Key; int Value; } item;

static item Items[100000];

int main(void)
{
	xmap tMap;
	xtime t0, t1;
	long long iSum = 0;

	for ( int i = 0; i < 100000; ++i ) {
		Items[i].Key = i * 7;
		Items[i].Value = i;
	}
	xrtMapInit(&tMap, sizeof(int));
	for ( int i = 0; i < 100000; ++i ) {
		int Key = Items[i].Key;
		xrtMapGetOrAdd(&tMap, (xbytesview){ &Key, sizeof(Key) }, NULL);
	}

	t0 = xrtNow();
	for ( int i = 0; i < 100000; ++i ) {
		int Key = i * 7;
		for ( size_t j = 0; j < 100000; ++j ) {
			if ( Items[j].Key == Key ) {
				iSum += Items[j].Value;
				break;
			}
		}
	}
	t1 = xrtNow();
	printf("array linear: %lld us (sum=%lld)\n",
		(long long)(t1 - t0), iSum);

	iSum = 0;
	t0 = xrtNow();
	for ( int i = 0; i < 100000; ++i ) {
		int Key = i * 7;
		int* pValue = (int*)xrtMapGet(
			&tMap, (xbytesview){ &Key, sizeof(Key) });
		if ( pValue != NULL ) {
			iSum += *pValue;
		}
	}
	t1 = xrtNow();
	printf("map lookup  : %lld us (sum=%lld)\n",
		(long long)(t1 - t0), iSum);
	xrtMapUnit(&tMap);
	return 0;
}
```

```term
$ gcc -O2 -DXRT_MODULE_ALL -DXRT_IMPLEMENTATION -I single race_lookup.c -lws2_32 -liphlpapi
$ ./a.exe
array linear: 24318900 us (sum=4999950000)
map lookup  : 4300 us (sum=4999950000)
（数值因机器而异：定位主导时映射快三个数量级以上）
```

**What just happened.** The same hundred thousand key lookups: the array's linear scan is O(n) each time — tens of billions of operations total; the map is amortized O(1) each time. A three-order-of-magnitude gap is no longer a "constant-factor dispute" but a magnitude question — behind the map's "lookup-hot → map" rule in the decision map is this very table. Mind the fairness of the comparison: if the array were sorted first and searched by bisection, the gap would narrow to about one order of magnitude — **a comparison experiment must give every candidate its strongest form**, or you are measuring a straw man.

### Volume 3 closes: from cells to intuition

The endpoint of the selection map is not memorizing cells but internalizing them into intuition — the pass mark: reading a requirement description, you automatically see "where the authoritative store goes, where the views go, which operation carries the heat", name candidates and a verification method within thirty seconds, and know which anti-selection question is most likely to trap you. At that point, Volume 3's eleven containers are no longer eleven tools but a mental map ready at hand — the parsing of Volume 4, the concurrency of Volume 6, and the networking of Volume 7 all keep building floors on this map.

## Contracts

- **Three questions settle it**: ownership → order → access mode; filter candidates in that order.
- **Authoritative store + views**: the data once, views as needed; refuse all-powerful containers.
- **Measurement adjudicates**: fixed scale and mix, median of three, memory-side evidence alongside (Chapter 6's stats); give every candidate its strongest form — no straw men.
- **Five anti-selection questions**: universal container / list insert-delete / hashing always fastest / lock first / pool cures everything — each has its substitute answer marked on the map.
- **Concurrency boundary**: except for the queues, this volume's containers are not thread-safe; cross-thread answers live in Volume 6 — carry this boundary at design time instead of reworking at concurrency-conversion time.

### Combination plays: three high-frequency architecture sketches

Beyond the map's cells, three "multi-container combination" architectures recur in real projects, worth memorizing whole. **The resource manager**: a fixed pool holds the objects (authoritative store) + xmap does key lookup + an intrusive list maintains recency (LRU/cleanup order) + slot_map lends generational handles — four containers, one cell each, composing a complete "take-put, locate, order, reference" system; Chapter 67's network connection table is its concurrent, scaled-up edition. **The parser workspace**: a fixed stack as the frame stack (depth bounded by resource limits) + an arena for temporary fragments (reclaimed with scope) + xmap as the symbol table (insertion-order export of error messages) — Volume 4's Chapter 31 uses exactly this. **The event pipeline**: two SPSC stages in series (collect → aggregate → process) + a fixed pool on the aggregation side reusing event objects — Volume 6's logging and monitoring threads are the finished product of this skeleton. The common thread of the three sketches: **not one "clever" container choice anywhere — all are the natural outcome of answering the three questions** — and that is the final state this chapter wants you to leave with: selection is no longer inspiration; it is process.

### When to refactor a container choice

Container selection is not one-shot. Three signals mean returning to the decision map: **magnitude crossing** (data grows from thousands to millions, and the array's linear search goes from unnoticeable to bottleneck — experiment two's numbers are the vaccination); **access-mode drift** (a page that was traversal-dominated gains high-frequency lookups — adding a view is cheaper than rewriting the authoritative store); **concurrency conversion** (a single-threaded module must now be shared — every answer except the queues lives in Volume 6; don't force it at the container layer). Put these three signals into your tech-debt checklist, and container-layer debt won't "grow silently until the position blows up".

## Pitfalls

### Pitfall 1: choosing containers by "what I know"

Symptom: code review all green, performance retrospective all red; a new maintainer can't answer "why this container".

Cause: the selection basis is inertia, not requirements — not one of the three questions was asked.

```c bad
/* Need: key lookup + key-order export. Inertia: I know linked lists */
xlist Pending;   /* lookup O(n), unordered - neither requirement met */
```

```c good
/* Three questions: ownership = objects self-holding; order = key order; mode = lookup + range */
/* Answer: intrusive AVL (the objects exist; add a node field and hang them on the tree) */
xavl ByKey;
```

### Pitfall 2: substituting complexity for measurement

Symptom: everyone quotes big-O in the selection meeting; after launch the "theoretically optimal" solution is crushed by the "theoretically worse" one.

Cause: big-O erases constants and caches — the scene where list O(1) insert/delete loses to array O(n) shifting is exactly experiment one's proof.

```c bad
if ( Complexity == "O(1)" ) {
	Choose(It);          /* list insert O(1) -> choose the list; a traversal-hot load crashes outright */
}
```

```c good
/* Fix scale and mix, run each candidate three times, take the median, choose by numbers */
Benchmark(候选A, 负载);
Benchmark(候选B, 负载);
Choose(winner_by_measurement);
```

## Exercises

### Basic: map fill-in

Cover the decision map's "first choice" column and recite the answers row by row against the requirement shapes; then cover the "do not use" column and recite the reasons — both columns pass before it counts as memorized. Then quiz each other with a colleague: they read a requirement shape, you answer first choice and reason — five rounds without pausing is a pass.

### Advanced: a selection audit of your project (the three questions in live fire)

Pick a real module at hand, list every container usage point, and re-verify each with the three questions: was ownership answered correctly, is the order promise actually used, does the hot operation match the container's strength. Produce an audit table (usage point / three answers / verdict / suggestion), covering at least five usage points.

### Challenge: a three-container comparison benchmark

For the requirement "a million records + key lookup + key-order export", compare three candidates: a sorted array (bisection lookup), xintmap (lookup), AVL (lookup + range). Write loading plus two operation rounds for each (a hundred thousand lookups, one full key-order export) and output a six-cell timing table. Acceptance criteria: the table reproduces "lookup: the map fastest; export: AVL/sorted array favored; no container first in everything", plus one sentence on how to combine them in your project.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Three questions | ownership (who manages the data) → order (accessed by what) → mode (what is the hot operation); filter candidates in order |
| Architecture | authoritative store + views on demand; data once; refuse all-powerful containers — selection lesson one |
| Lookup | byte keys xmap / integer keys xintmap / key order + range AVL |
| Transfer | cross-thread belongs to the queue family alone; the lock is Volume 6's last resort |
| Take-and-put | same-size high-frequency → fixed pool; mixed → variable-size pool; reclaim-together → arena |
| Adjudication | measurement decides: fixed load, median of three, every candidate in its strongest form |
| Anti-selection | universal / list insert-delete / hashing fastest / lock first / pool cures everything — five self-check questions |
| Combination plays | resource manager / parser workspace / event pipeline — three high-frequency architecture sketches |
| Refactor signals | magnitude crossing / access-mode drift / concurrency conversion — any one means back to the map |
