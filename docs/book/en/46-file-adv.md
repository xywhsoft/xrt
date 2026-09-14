---
num: 46
slug: file-adv
title: Files (Part 2): Mapping, Locks, and Atomic Writes
volume: 卷五 系统服务
type: practice
lead: Zero-copy reads/writes via memory mapping, range locks for concurrency coordination, temp-file-plus-rename atomic delivery — the advanced trio of file operations.
api: file
---

## Orientation

The middle file chapter covers three advanced capabilities. **Memory mapping** (`xrtFileMap`): map the file into the address space, and a pointer is the file's content — random access to large files, zero-copy and zero system calls. **File locks** (`xrtFileLock` whole-file + `xrtFileLockRange` ranges): shared/exclusive modes coordinating multiple processes reading and writing the same file — two granularity levels, from "whole-file mutual exclusion" down to "byte-range fine grain". **Atomic delivery** (`xrtFileTemp` + rename): write the temp file, then rename atomically — readers always see a complete file; the "half-written" state structurally disappears. Each solves one real concurrency or performance problem.

## Introduction

Three scenes. Scene one: a 2GB index file needs random access — a Read/Seek loop makes every read a system call (kernel copies into the user buffer), and on a hot path of a million accesses the syscall overhead dominates; once mapped into memory, access is an ordinary pointer dereference — the kernel loads pages lazily, zero copies. Scene two: two processes updating the same data file — without coordination they overwrite each other; a whole-file lock is too coarse (read-heavy scenes serialize all readers), while range locks let readers share, writers exclude, and non-overlapping ranges run in parallel. Scene three: a reader happens to be reading the config file during an update — it reads half-written content and the config parse crashes; write a temp file and rename atomically, and the reader sees either the old file or the new one, never an intermediate state.

## Concepts

### Memory mapping: the pointer is the file (random access's home turf)

```diagram flow
- Establish: xrtFileMap(handle, offset, length, flags) - length 0 = map to end of file; the view's Data/Size is the access surface
- Access: dereferencing the pointer reads the file; with the writable flag, writes synchronize back to the file
- Lifecycle: Unmap returns the mapping; when written data reaches disk is governed by the kernel and flush
- Fit: large files with random access (indexes/databases/read-only assets) - sequential streaming is better served by Read
```

Mapping's payoff splits by access pattern: **random access** — each access is a page fault (first time) or a memory read (afterward); once the hot region is resident, speed approaches memory. **Sequential streaming** — mapping has no read-ahead advantage; chunked Read is simpler and more controllable. So "mapping is faster" is half-true — **fast for random, no advantage for sequential**; check the access pattern first (a continuation of Chapter 45's three hosts).

### Range locks: byte-granularity coordination

Two granularity levels: `xrtFileLock(句柄, 共享/独占, 是否等待)` (handle, shared/exclusive, wait?) — the whole-file lock (the standard posture for single-instance guards and task-queue mutual exclusion); `xrtFileLockRange(句柄, 模式, 偏移, 长度, 是否等待)` (handle, mode, offset, length, wait?) — the byte-range lock (record-level coordination); `Unlock`/`UnlockRange` release correspondingly, and a range unlock's parameters must exactly match the lock's. Choosing between them follows the data's shape: the whole-file lock suits "the file is the unit" mutual exclusion (the classic single-instance daemon — held while the process lives, auto-released on exit); the range lock suits record-level parallelism (a reader takes a shared lock on range A, a writer an exclusive lock on range B; non-overlapping means parallel). The semantic discipline of locks: **a lock is a coordination protocol, not magic** — it means something only when every accessing party follows the same lock protocol (one process writing directly without locking is protected by no one's lock); align lock ranges with data-structure boundaries (record-level locks over record ranges).

### Atomic delivery: temp file + atomic rename (the annihilator of intermediate states)

The industrial full form of Chapter 45's "three-step finish": (1) write the temp file (same directory — renaming across file systems fails); (2) Flush semantics ensure the data reaches disk; (3) **rename atomically** to the final name — rename within one file system is atomic, and readers at any moment see a complete file. Beyond the lock sample, the temp and dir_temp samples demonstrate temp-file naming and cleanup coordination (`xrtFileTemp` creates a uniquely named temp file (directory/prefix parameterized), deleted or renamed when done).

### Combining the trio (mix per scene as needed)

Mapping plus locks: a mapping's writers coordinate with range locks — each writer locks its own record range and writes through the mapping. Atomic delivery plus locks: frequently updated files use "versioned file + atomic switch" (write a v2 temp file, rename onto the primary name) to avoid holding locks long. The trio imposes no forced pairing — take what the concurrency scene demands, which is exactly why each has its own section.

## Examples

### Complete program: reading via memory mapping

From the repository sample `examples/file/map/main.c`:

```embed path="examples/file/map/main.c" title="examples/file/map/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/file/map/main.c -lws2_32 -liphlpapi
mapped
```

**What just happened.** (1) The file is written first, then mapped — the parameters (file, offset=0, size=0, READ) with size 0 mean "map to end of file"; mapping creates no file, it maps the file's current bytes. (2) After the mapping is established, content is read through the pointer and prints `mapped` — this read involved no Read call, just memory access. (3) `xrtFileMapData/xrtFileMapSize` fetch the view's pointer and length, and `xrtFileUnmap` tears down the mapping — order discipline: Unmap before Close (never reversed). A read-only mapping's extra dividend: multiple processes share the same physical pages — the loading scheme for dictionaries/indexes/read-only resource files. A writable mapping's (flag-declared) writes are synchronous back to the file — the kernel flushes dirty pages at its choosing; explicit durability needs go with flush semantics.

### Complete program: file locks

From `examples/file/lock/main.c`:

```embed path="examples/file/lock/main.c" title="examples/file/lock/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/file/lock/main.c -lws2_32 -liphlpapi
locked
```

**What just happened.** (1) An exclusive lock is taken on the target range — on success it prints `locked`; while held, other processes' lock requests on the same range will wait or fail (per flags). (2) The sample is a single process demonstrating the API's shape; real multi-process coordination requires **all participants to follow the lock protocol** — the practical face of "coordination protocol". (3) Unlock releases — a lock's lifecycle can also hang on handle close (auto-release at process exit), but explicit Unlock is the disciplined spelling. The link and report samples are left as further reading: symlink operations and file-status reports (size/times/attributes) belong to the file-metadata family, complementary to this chapter's trio.

## Contracts

- **Mapping discipline**: establish before use, Unmap before Close; random access is its home turf, sequential streaming uses Read.
- **Lock protocol**: protection exists only if all parties follow the same protocol; lock ranges align with data boundaries; explicit Unlock.
- **Atomic delivery**: temp file in the same directory, flushed when written, rename switches atomically — the "half-written" state structurally disappears.
- **Temp files**: uniquely named (`xrtFileTemp`); rename on the success path, delete on the failure path — neither ending leaves garbage.
- **Freedom of combination**: combine the trio per concurrency scene; no forced pairing.

### From examples to engineering: three hosts of the trio

**Index and asset loading** (mapping's home turf): dictionaries, large read-only config files, database indexes — map at startup, read-only throughout, Unmap at exit; multiple processes sharing the same physical pages turns "one copy of memory per process" into "one view per process". **Multi-process coordination** (locks' home turf): single-instance daemons guard with whole-file locks (serve only when acquired; the second instance exits); task-queue files and ledger files do record-level updates with range locks — the lock protocol goes into the file's "manual" (which field ranges, what modes, maximum hold time). **Config and state delivery** (atomic delivery's home turf): every file "readers might read at any moment" — configs, manifests, checkpoints. The hosts also combine: a ledger file = range locks (record updates) + atomic delivery (whole-file rotation).

### Crash consistency: atomic delivery's blind spot

Atomic delivery guarantees "readers never see intermediate states", but one blind spot must be acknowledged: **a successful rename does not mean the data is on disk** — rename atomically switches the directory entry, while data blocks may still sit in the page cache; a power loss at that moment leaves the directory pointing at the new name with incomplete data. For crash-consistency-critical scenes (ledgers, checkpoints), the full chain is "write temp file → fsync semantics (data to disk) → rename → fsync the directory (modern file systems)" — XRT's file layer provides the corresponding flush-semantics capability, and the directory fsync after rename is a host file-system detail (omitted in most scenes, added for ledger-grade scenes). Write this chain into your "durability checklist": **rename eliminates intermediate states; fsync eliminates the power-loss window** — two guarantees on different layers; missing either leaves a window.

### A selection reminder: when not to use locks

File locks are not the only concurrency-coordination option; ask three questions first. **Multiple processes on one machine?** Yes → file locks are reasonable; across machines → file locks don't work (lock semantics on network file systems are unreliable); use an application-layer protocol. **Access granularity?** File-level → whole-file lock (simplest); record-level → range locks; finer → consider the single-writer model (one process owns the file, others access through it — Chapter 21's queues passing messages) to sidestep locks. **Contention frequency?** Low-frequency updates → atomic delivery replaces locks outright (version switching, lock-free); high-frequency contention → lock-wait overhead shows; consider shared memory + single writer. Locks are one of the last resorts of coordination, not the first reaction — the same root as Chapter 23's "lock-first-for-safety" counter-question.

## Pitfalls

### Pitfall 1: using mapping for sequential reads

Symptom: scanning a large file byte by byte through a mapping — performance no better, often worse (page-fault overhead + no read-ahead), and the code got harder.

Cause: mapping's payoff model is "random access with hot regions resident" — for sequential streams its page faults hold no advantage over Read's chunking.

```c bad
xfilemap Map = xrtFileMap(File, 0, Size, XFILE_MAP_READ);
for ( uint64 i = 0; i < Size; i++ ) {
	Consume(Map.Data[i]);   /* sequential scan of 2GB - one page fault per page, no read-ahead */
}
```

```c good
/* sequential stream: back to chunked Read (Chapter 45's streaming host) */
char Buffer[65536];
uint64 Done = 0;
while ( Done < Size ) {
	uint64 Chunk = Min(sizeof(Buffer), Size - Done);
	xrtReadFull(File, Buffer, Chunk);
	Consume(Buffer, Chunk);
	Done += Chunk;
}
```

### Pitfall 2: you lock, but others don't

Symptom: data races persist despite locking — corrupted records, half-new-half-old reads; intermittent and hard to reproduce.

Cause: a lock is a **protocol among participants** — one writer bypassing it and writing directly renders every compliant party's lock decorative.

```c bad
/* process A (compliant): lock, then write */
xrtFileLockRange(File, XFILE_LOCK_EXCLUSIVE, Off, Len, true);
WriteRecord(File, Off, Data);
xrtFileUnlockRange(File, Off, Len);

/* process B (legacy code, no lock protocol): writes directly */
WriteRecord(File, Off, OtherData);   /* A's lock constrains unlocked B not at all - the race remains */
```

```c good
/* every accessing party follows the same lock protocol - B is fixed to match A */
xrtFileLockRange(File, XFILE_LOCK_EXCLUSIVE, Off, Len, true);
WriteRecord(File, Off, OtherData);
xrtFileUnlockRange(File, Off, Len);
/* the protocol goes into the module doc: all writes to this file must lock first */
```

## Exercises

### Basic: mapping vs read-back (consistency of two access paths)

Read the same content out through a mapping and through ReadFull — byte-identical; then modify through a writable mapping and read back to verify synchronization.

### Advanced: a two-process lock experiment

Write a "lock holder" and a "lock waiter" program: the former locks and sleeps a few seconds; the latter tries to lock and reports waiting/success — feel the coordination semantics (the waiter tries once with a shared lock and once with an exclusive lock).

### Challenge: an atomic config updater (zero intermediate states for concurrent readers)

Implement `save_config(路径, 内容)` (path, content): write a uniquely named temp file in the same directory → flush → rename atomically; concurrent readers loop reading and parsing the config, verifying that what they read is always complete and legal. Acceptance criteria: zero parse failures across 100 updates; after fault injection (killing the process before rename) the original file is intact; no temp-file residue.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Mapping | `xrtFileMap` — the pointer is the file; random access's home turf, no advantage for sequential streams; Unmap before Close |
| Two lock grades | whole-file Lock (single-instance mutual exclusion) / range LockRange (record-level parallelism); the protocol binds all parties |
| Atomic delivery | same-directory temp file (unique name) → flush → atomic rename; intermediate states structurally gone |
| Temp files | `xrtFileTemp(目录, 前缀, ...)` (directory, prefix) unique naming; rename on success, delete on failure - two endings, zero garbage |
| Combinations | mapping + locks (writer coordination) / atomic switching (spares long-held locks) - take per scene |
| Metadata family | link/report samples: symlinks and file-status reports - companions to the trio |
| Three hosts | index/asset loading (mapping) / multi-process coordination (locks) / anytime-read delivery (atomic rename) |
| Crash blind spot | rename eliminates intermediate states, fsync eliminates the power-loss window - two-layer guarantees; missing either leaves a window |
| Lock's boundary | useless across machines, replaceable by atomic delivery when updates are rare, single-writer when contention is hot - a last resort, not a first reaction |
