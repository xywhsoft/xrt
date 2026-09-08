---
num: 12
slug: hash-xid
title: Hashes and the XID Time-Ordered Identifier
volume: 卷二 数学、随机与标识
type: practice
lead: Deterministic hashing, keyed SipHash, and the 192-bit time-ordered identifier — bucketing verification, collision and precomputation defense, and sortable primary keys, all explained at once.
api: hash, xid
---

## Orientation

Volume 2's closing chapter puts two "identity" topics together: **hashing** folds arbitrary bytes into a fixed-width fingerprint, and **XID** generates a time-ordered unique identifier for every record. They share one keyword — **determinism**: a hash's determinism makes verification and bucketing reproducible; XID's determinism makes "lexicographic order = generation-time order". The chapter also draws the security boundary: keyless hashes resist no collision attack, and the keyed SipHash is the right choice for externally visible keys (like hash-table keys); "storing user passwords" is answered by neither — that is the territory of Volume 8's crypto module. After this chapter you will pick the right tool for three frequent problems: data-consistency verification, external hash-table keys, and distributed primary keys.

## Introduction

Three scenarios. One: syncing gigabytes of index files daily, synchronously, and wanting to know "did anything change" — shipping the whole file over for comparison is too expensive; compute a fingerprint and send 8 bytes. Two: a server uses request-header fields as hash-table keys, and an attacker crafts colliding inputs, degrading the hash map into a linked list — that is hash-collision DoS, and the answer is giving the hash a key only you know. Three: choosing a primary key for a distributed system — auto-increment integers need a coordination center, fully random UUID v4 fragments database indexes, while a "time-first" identifier naturally clusters by creation order.

The three scenarios map to `xrtHash32/64` (deterministic fingerprints), `xrtSipHash` (keyed, collision-resistant), and XID (the time-ordered unique identifier). None is a cryptographic primitive — the usage boundary is pinned in the contracts. This chapter also closes Volume 2: Chapter 10's deterministic numbers, Chapter 11's deterministic random sequences, and this chapter's deterministic fingerprints together form the three cornerstones of "engineering determinism" — everything written into test assertions must be reproducible across platforms and across time.

## Concepts

### Deterministic hashing: same input, always the same output

`xrtHash32(pData, iSize)` and `xrtHash64` use a fixed default seed — **the same bytes produce the same value at any time, in any process**. That is the precondition for checksums, bucketing, and cache keys. When you need cross-process consistency but "don't let an adversary precompute", use the explicit-seed versions `xrtHash32Seed` / `xrtHash64Seed`: multi-process sharding keeps one seed for consistency; external scenarios switch seeds to scramble precomputation tables. Note the other face of "determinism": **a keyless hash is computable by anyone** — it proves "the data is this", not "the data came from whom".

### Keyed hashing: SipHash-2-4

`xrtSipHash(pData, iSize, Key)` uses an `xsipkey` (a 128-bit key, constructed via `xrtSipKey(低64, 高64)`). Without the key, collisions cannot be constructed — hash-map keys over untrusted input must use it. The streaming interface `xrtSipHashInit` / `Update` / `Final` handles chunk-arriving data (network streams, large files) and is **result-equivalent** to the one-shot version — the same key and data give the same 64-bit value either way. The key's lifecycle follows Chapter 5's sensitive-data discipline: `xrtSecureZero` when done.

### XID: the 192-bit identifier with time in front

```diagram flow
- Structure: 64-bit microsecond timestamp + random and sequence bits, 24 bytes total
- Text form: fixed-length 32 characters (base62-style encoding), goes straight into URLs / logs / primary keys
- Ordering: time in front => lexicographic order = generation-time order; index-friendly, sortable logs
- Zero allocation: binary path is value semantics; the text path writes the caller's buffer
```

XID resolves the trilemma of "distributed primary keys": no coordination center needed (time + random + sequence generated locally), no fragmentation (the time prefix clusters), readable (32-character text). `xrtXidMake` generates, `xrtXidWrite` writes into the caller's buffer (capacity `XID_TEXT_CAPACITY`, canonical length `XID_TEXT_SIZE` without the trailing zero), `xrtXidParse` strictly parses back to binary (any deviation of length or character set fails whole and reports the **first illegal byte's position** — `xrtXidErrorOffset`), and `xrtXidTime` extracts the generation moment with O(1) bit operations. Batch generation uses `xrtXidMakeMany` (drawing a whole batch of random bytes at once, cutting syscall overhead); `xrtXidCompare` provides a stable total order — the base primitive for sorting and deduplication. Compare the division of labor with Chapter 11's four randomness layers: XID internally uses the secure randomness source for uniqueness, but the XID itself is not a "secret" — it may appear in URLs, logs, and databases without harm; when you need an unpredictable identifier (a capability token, say), use Chapter 11's `SecureText`.

### A selection table

| Need | Tool | Not to be used |
| --- | --- | --- |
| File/data consistency verification | `xrtHash64` | —— |
| Multi-process-consistent sharding | `Hash64Seed` with one seed | —— |
| External untrusted-input table keys | `xrtSipHash` (key kept secret) | keyless hashes |
| Precomputation-proof fingerprints | `Hash64Seed` with rotating seeds | the default seed |
| Distributed primary keys / log correlation | XID (`Make`/`MakeMany` + textual form) | auto-increment integers, random UUIDs |
| Storing user passwords | Volume 8 crypto's password hashing | any function of this chapter |

### A prior reminder: a fingerprint is not encryption

Before running the examples, pin down a frequent misunderstanding: hashing is a **fingerprint**, not **encryption**. Encryption is reversible (with the key you can restore); a fingerprint is not (you cannot restore an 8-GB file from 8 bytes); encryption keeps content secret, a fingerprint publicizes the content's existence. "Hash the file then delete the original" is an unrecoverable mistake — a fingerprint serves **comparison**, never **storage**. With this boundary clear, the three example groups' outputs get their correct reading.

## Examples

### Complete program: deterministic hashing and the explicit seed

From the repository example `examples/hash/hash32/main.c`:

```embed path="examples/hash/hash32/main.c" title="examples/hash/hash32/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/hash/hash32/main.c -lws2_32 -liphlpapi
default: 9590B597
seeded : 064A210B
```

**What just happened.** (1) `sizeof(sKey)-1` explicitly excludes the trailing zero — a hash's input is a binary-safe byte string, and zero termination is merely the C-string convention; one extra zero is different data and a different fingerprint. (2) The default seed outputs `9590B597` — stable across platforms and versions; what you compute today matches yesterday's CI. (3) The same data with a different seed gives a completely different fingerprint — the seed participates in the same deterministic function, and "precomputation defense" is precisely that the adversary does not know your function variant.

### Complete program: SipHash one-shot versus streaming

From `examples/hash/variants/main.c`, verifying the two postures' result equivalence:

```embed path="examples/hash/variants/main.c" title="examples/hash/variants/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/hash/variants/main.c -lws2_32 -liphlpapi
seed64: 1E3C0D5C77AC4C64
sip:    21D9F0C356D09D0A
stream-equal=yes
```

**What just happened.** (1) The `xsipkey` is built from two 64-bit words — a 128-bit key space, the key coming from secure randomness (Chapter 11), not hard-coded. (2) One-shot `xrtSipHash` over the eighteen-byte input gives `21D9F0C356D09D0A`. (3) The streaming version feeds the same data **in two chunks** (`Update` twice), and `Final`'s result is identical to the one-shot — the third line `stream-equal=yes` is this chapter's most important assertion: chunking does not affect the fingerprint, so network streaming and whole-block computation can cross-verify each other. (4) The `Hash64Seed` line shows the typical use of the seeded deterministic hash: the fingerprint of `"user:42"` with seed `0x1234` is stable — multi-process sharding routes by this value modulo, and as long as the seed doesn't change, routing doesn't change through scale-up or scale-down.

### Complete program: XID generation, textualization, parsing, time extraction

From `examples/id/xid/main.c`:

```embed path="examples/id/xid/main.c" title="examples/id/xid/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/id/xid/main.c -lws2_32 -liphlpapi
XID: V-OPfAm6...（32 字符定长文本，示例）
Unix microseconds: 17...(微秒时间戳)
```

**What just happened.** (1) The full chain: `Make` generates → `Write` textualizes → `Parse` parses back to binary → `Time` extracts the moment — four functions forming XID's read/write loop. (2) `Parse`'s input view length is `XID_TEXT_SIZE` (no trailing zero): any length or character deviation fails whole, and `xrtXidErrorOffset` extracts the first illegal byte's position from the error — errors carry their own location, no error-message parsing needed. (3) The output timestamp varies with the run's moment, but the **prefix-increasing order** property never changes: the earlier-printed XID is lexicographically smaller than the later-printed one.

## Contracts

- **Determinism**: `Hash32/64` and SipHash with the same input and key always give the same output, stable across platforms and versions (chunking does not affect it).
- **Security boundary**: keyless hashes resist no collision attack; external hash-map keys use SipHash with the key kept secret; **password storage does not belong to this chapter** (Volume 8's password hashing).
- **Key discipline**: the `xsipkey` is sensitive data — generated from secure randomness, `xrtSecureZero` when done.
- **XID ordering**: lexicographic order = generation-time order; `Compare` provides a stable total order; the **single-comparison direction** of rapidly consecutive generations is not a promise (the microsecond clock decides).
- **Parsing strictness**: `Parse` accepts only the fixed-length legal text; on failure nothing is written, and the error offset is readable from the thread error.
- **Batch generation**: `MakeMany` generates a contiguously stored batch at once, cutting syscall counts.

## Pitfalls

### Pitfall 1: keyless hashing as external hash-map keys

Symptoms: the server's hash table collapses in performance under specially crafted requests (hash-collision DoS); the longer the table, the worse the degradation.

Cause: the default-seed hash function is public — an attacker can offline-craft masses of same-fingerprint inputs, degrading your table into a linked list.

```c bad
uint64 iKey = xrtHash64(Header.Value, Header.Size);  /* public function */
Bucket = TableFind(iKey);                             /* collisions craftable */
```

```c good
uint64 iKey = xrtSipHash(Header.Value, Header.Size, gTableKey);  /* key kept secret */
Bucket = TableFind(iKey);
```

### Pitfall 2: XID parsing counting the trailing zero in the length

Symptoms: parsing fails when reading an XID back from text (file, URL parameter), the error offset pointing past the last cell; single-stepping in a debugger shows "the string looks fine".

Cause: `Parse`'s input is a view of **exactly 32 characters** (`XID_TEXT_SIZE`); passing `strlen+1` or using `XID_TEXT_CAPACITY` as the length treats the trailing zero as a 33rd character.

```c bad
char arrText[XID_TEXT_CAPACITY];
load(arrText);
if ( !xrtXidParse((xstrview){ arrText, XID_TEXT_CAPACITY }, &Value) ) {
	/* length 33: certain failure — the trailing zero is not part of the text */
}
```

```c good
char arrText[XID_TEXT_CAPACITY];
load(arrText);
if ( !xrtXidParse((xstrview){ arrText, XID_TEXT_SIZE }, &Value) ) {
	size_t iOffset = 0;
	xrtXidErrorOffset(xrtGetError(), &iOffset);   /* first illegal byte's position */
}
```

## Exercises

### Basic: fingerprint cross-check

Compute `Hash32` and `Hash32Seed(…, 0x12345678)` over the same stretch of text, printing both values; change one byte and recompute, verifying the fingerprint completely changes (the avalanche effect experienced firsthand). Also compute the difference between the two changed values — on average about half the bits flip.

### Advanced: a streaming verifier

Write `hash_stream(path)`: read the file in chunks (4 KB each) into the SipHash streaming interface and return the fingerprint; cross-check the same file with the one-shot interface reading it whole. Hint: the two postures must agree — that is your confidence source for writing incremental verification.

### Challenge: log correlation and sorting

Write a small tool: generate 100 XID values (`MakeMany` batch), store them shuffled into an array, sort by `Compare`, then extract times one by one asserting monotonic non-decrease; simulate two logs correlating through the same XID text (`Parse` back to binary, then `Equal`). Acceptance: all post-sort assertions pass; the same text's two parsed binaries test `Equal` true, and against another XID false.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Deterministic fingerprints | `Hash32` / `Hash64` default seed; stable across platforms |
| Explicit seeds | `Hash32Seed` / `Hash64Seed`: consistent sharding or precomputation defense |
| Collision resistance | `SipHash` + a secret key; mandatory for external table keys |
| Streaming | `SipHashInit/Update/Final` equivalent to one-shot |
| XID structure | 24 bytes = time in front; 32-character fixed-length text |
| The XID loop | `Make/Write/Parse/Time`; parse errors carry an offset |
| Ordering | lexicographic = generation order; `Compare` a stable total order |
| Password storage | none of this chapter's functions — that is Volume 8 crypto's password-hashing territory |
