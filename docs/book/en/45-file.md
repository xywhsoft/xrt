---
num: 45
slug: file
title: Files (Part 1): Open, Read/Write, and the Cursor
volume: 卷五 系统服务
type: practice
lead: The xrtOpen/Close handle mainline, Full read/write semantics, three-basis positioning and absolute-offset reads — the minimal mental model of file operations.
api: file, io
---

## Orientation

The first of three file chapters covers the **handle mainline**: `xrtOpen` (permissions as flag combinations) → `WriteFull/ReadFull` (read-full/write-full semantics) → `xrtSeek/Tell` (three-basis positioning) → `xrtClose`. The Full-suffix family tucks short-read/short-write retry boilerplate into the library (the same semantics as Chapter 41's IO abstraction — this chapter is its direct form on file handles); `ReadAtFull` reads at an absolute offset without disturbing the cursor — the specialized posture for peeking at file headers. The middle chapter (Chapter 46) covers mapping/locks/atomic writes; the next (Chapter 47) covers directories and traversal — three chapters forming the complete map of file operations.

## Introduction

The C standard library's file API is a layer of historical sediment: `fopen`'s mode strings (who remembers `"rb+"` versus `"w+b"`), `read/write`'s short semantics (the return value demands your own loop), cross-platform behavior differences (the text-mode newline-conversion pit). Every C programmer has written fread's while loop and guessed at fflush timing — not your fault, the interface's age's fault; this chapter's task is to set down that baggage once and for all.

XRT's file layer absorbs fifty years of pits: **permissions as flag combinations** (read/write/create/truncate/append, five flags bitwise-ORed — readable at a glance, never again looking up which of "rb+" and "w+b" is which), **Full semantics built in** (read full or write full or fail — the loop boilerplate vanishes), **platform differences smoothed** (uniform binary semantics, no newline conversion, errors reported through Chapter 4's structured model instead of errno guesswork). `xrtOpen` returns an `xfile` handle — connecting to Chapter 41's IO abstraction (one adapter line turns it into an xreader/xwriter).

## Concepts

### The handle lifecycle (memorize this mainline first)

```diagram flow
- Open: xrtOpen(path, flags) - read/write/create/truncate/append combined bitwise
- Read/write: WriteFull/ReadFull - exactly N bytes or failure; short is failure
- Position: Seek(XSEEK_START/CURRENT/END) + Tell - the three-basis cursor
- Auxiliary: ReadAtFull reads at an absolute offset - cursor untouched, peeking only
- Close: xrtClose - always check the return value (buffer flushing's verdict is settled here)
```

Examples of flag combinations: `XFILE_READ | XFILE_WRITE` (open read-write, no create), `XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE` (rebuild-and-write — the standard posture for temp files). Creation and truncation are explicitly separate — "opened an existing file and accidentally emptied it" cannot happen unintentionally under flag combinations.

### Full semantics, file edition (the end of boilerplate)

Chapter 41's Full semantics take two concrete forms on files: **WriteFull on disk full** — space runs out mid-write, the whole operation fails, and the error slot keeps structured detail (Chapter 4's error object carries diagnostics); **ReadFull on early EOF** — 80 of 100 requested bytes arrive then EOF, a failure ("the file was truncated" is a real fault signal, not "80 will do").

### ReadAtFull: a side-channel read that never disturbs the cursor (peeking only)

While streaming a large file you want to peek at its header (magic-number validation) — a plain Read moves the cursor, and the read-then-Seek-back spelling is both roundabout and error-prone. `ReadAtFull(句柄, 绝对偏移, 缓冲, 长度)` (handle, absolute offset, buffer, length) completes the side read **without moving the cursor** (the sample verifies with Tell that the cursor still sits at 3) — the standard posture for peek-validation and index queries.

### The junction with the IO abstraction (direct or adapted)

`xrtReaderFromFile` adapts an xfile into Chapter 41's unified Reader (the write side has its sibling) — files join the channel of "one body of serialization code, three media" (memory/file/network). Choosing between direct (this chapter's API) and adapted (the IO interface): a one-shot single-file operation is shorter direct; when the same logic must face multiple destinations (memory/file/network), go through the adapter — the choice itself follows Chapter 41's "three hosts" judgment.

## Examples

### Complete program: the handle mainline end to end

From the repository sample `examples/file/basic/main.c` — create, write, position, side read, close, delete:

```embed path="examples/file/basic/main.c" title="examples/file/basic/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/file/basic/main.c -lws2_32 -liphlpapi
xrt file
```

**What just happened.** (1) `xrtOpen(路径, 写|创建|截断)` (path, write|create|truncate) — flag combinations declare "rebuild-and-write", no mode strings — the file module keeps its interfaces explicit. (2) `WriteFull` writes the whole span (8 bytes written exactly — the built-in guarantee that a short write fails). (3) After `Seek` back to the head, `ReadFull` reads back — the write-read loop closes. (4) `ReadAtFull`'s side read verifies: the cursor's Tell still sits at 3 — **peeking doesn't disturb the streaming position**, and this number is the proof. (5) `xrtClose` checks its return value — buffer-flush failures (the final batch on a full disk) surface here, not at crash time. (6) `xrtFileDelete` removes the temp file by path — the handle is closed; path deletion is an independent operation; a two-step cleanup finishes it.

### Complete program: three-media IO adaptation

From `examples/io/file/main.c` — the same serialization logic over memory and file:

```embed path="examples/io/file/main.c" title="examples/io/file/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/io/file/main.c -lws2_32 -liphlpapi
xrt io
```

**What just happened.** (1) The file adapter wraps an xfile into an xwriter — the serialization code (the one written in Chapter 41) drops onto the file unchanged. (2) Running the same logic as with the memory adapter: the products are byte-identical — the file-side verification of the "three media" promise. (3) The adapter's return is cleanly layered: adapter Destroy and file Close each to their own — consistent with Chapter 41's borrowing-based adaptation discipline.

## Contracts

- **Flag combinations**: read/write/create/truncate/append, five flags bitwise-ORed; creation and truncation explicitly separate — no accidental emptying.
- **Full semantics**: exactly N bytes or success; disk-full and early EOF both fail wholesale with structured error detail.
- **Cursor's three bases**: XSEEK_START/CURRENT/END (Chapter 3's xseek enum); Tell reports the current byte position.
- **Side read**: `ReadAtFull` at an absolute offset without moving the cursor — the dedicated channel for peek-validation and index queries.
- **Close verification**: Close's return value is mandatory — buffer-flush problems are settled at close.
- **Explicit deletion**: `xrtFileDelete` is independent of the handle — deleting a closed file by path, unrelated to the handle lifecycle.

### From examples to engineering: three hosts of file handles

**Config and small files**: whole-read/whole-write in one breath — Open/ReadAll or Open/WriteFull/Close, three steps; Chapter 32's JSON file parsing uses it exactly so. **Streaming**: large files in a chunked loop (ReadFull, 4KB a block) — the full form of the chunked-copy exercise; paired with Chapter 41's line reader for line-by-line scanning. **Random access**: ReadAtFull jumping by index (index files, log positioning by offset) — the cursor-untouched side read scales here. The three hosts correspond to file access's three modes (whole-load/sequential stream/random jumps) — think access mode before choosing API, isomorphic with container selection (Chapter 23's three questions).

### Error classification: three identities of file failure (triage for recovery strategy)

File-operation failures have three identities with different handling — mis-triage and both retries and reports go wrong. **Permission class** (missing/no-access/locked): report to the user and ask for correction — retrying is pointless; **space class** (disk full/quota exceeded): report to operations — retryable after cleanup or expansion; **consistency class** (peeking into a truncated file/checksum failure): report to the developer — usually a signal of concurrent modification or data corruption. Chapter 4's error categories mapped to files: XERR_NOT_FOUND/PERMISSION → permission class, XERR_IO's system codes → space class, business validation → consistency class. Classification's meaning is the recovery strategy: blind retry wastes the permission class and masks the consistency class.

### One habit: the three-step finish for files

The three-step finish for writing files deserves to be a habit: **after writing, Flush-semantics verification with Close** (durability settled) → **rename to the final name** (atomically visible — readers see either the old file or the new one; the simplified form of Chapter 46's atomic write) → **on failure, delete the temp file** (no garbage left). The problem with writing the final name directly: a crash mid-write leaves a truncated file that readers take for complete data. The three-step finish yields exactly two states (absent/complete) — the "partially written" state is structurally eliminated. This habit applies to config saves, log switching, and cache updates — Chapter 46 gives its full industrial version (temp file + atomic rename + fsync semantics).

## Pitfalls

### Pitfall 1: not checking Close's return value (the durability verdict)

Symptom: the program exits "successfully" but the file's tail data is lost — the last buffer flush failed (disk full, quota exceeded) and was silently swallowed.

Cause: Close is the data's verdict point — most implementations flush buffers at close; not checking is signing for a package without looking inside.

```c bad
xrtClose(File);   /* return value discarded - flush failure unnoticed */
return 0;
```

```c good
if ( !xrtClose(File) ) {
	ReportIoError(Path);   /* Chapter 4's error slot has the detail: disk full/quota */
	return 1;
}
return 0;
```

### Pitfall 2: Seek round-trips to read a file header (cursor state leaking out)

Symptom: while streaming a large file, it "occasionally continues from the wrong position" — the Seek-back offset was miscalculated (or scrambled by a concurrent branch's read).

Cause: the peek-then-Seek-back spelling (Seek-Read-Seek) exposes cursor state to subsequent logic — one miscalculated offset misaligns everything.

```c bad
xrtSeek(File, 0, XSEEK_START);      /* jump to the head */
xrtReadFull(File, Magic, 4);         /* read the magic */
xrtSeek(File, SavedPos, XSEEK_START); /* jump back - where did SavedPos come from? miscalculation is disaster */
```

```c good
if ( !xrtReadAtFull(File, 0, Magic, 4) ) {   /* absolute-offset side read */
	return false;
}
CheckMagic(Magic);   /* the cursor never moved - the streaming position is safe by construction */
```

## Exercises

### Basic: the flag matrix (a ten-cell behavior table, filled on paper then measured)

With five flag combinations (read-only/read-write/rebuild/append/create-without-truncate), each do an open-operate-close; record each combination's behavior for "file exists" and "file missing" — ten cells, paper first, then measurement.

### Advanced: chunked copy with verification (the streaming host's standard exercise)

Implement `copy(源, 目标)` (source, target): a 4KB chunked loop of ReadFull/WriteFull (the tail block uses the amount actually read — plain Read to finish); fingerprint both source and target with `Hash64` (Chapter 12) and compare. Hint: the final short block is the only place a short read is allowed.

### Challenge: a magic-number sniffer (side reads in practice)

Implement `sniff(路径)` (path): `ReadAtFull` side-reads the first 16 bytes, recognizes the magic numbers of gzip (1F 8B)/PNG/ZIP and prints the verdict; then, unaffected, streams the whole file counting lines (proof the cursor was undisturbed by the peek). Acceptance criteria: one test file per format, all verdicts correct; the line count matches a no-sniff direct-read version.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Open | flags combined bitwise (read/write/create/truncate/append); no mode-string codebook |
| Full family | read full/write full or fail; disk-full and early EOF reported wholesale |
| Positioning | Seek's three bases (START/CURRENT/END, Chapter 3's xseek) + Tell queries |
| Side read | `ReadAtFull` at an absolute offset, cursor unmoved (provable by Tell) - peeking/indexing only |
| Close | Close's return value is mandatory - the durability verdict; failures surface here, not as production crashes |
| Three media | `Reader/WriterFromFile` adapt into Chapter 41's unified interface |
| Three hosts | whole-load (config/small files) / sequential stream (large-file chunks) / random jumps (index offsets) - access mode precedes API |
| Error triage | permission class → report to user / space class → report to ops / consistency class → report to developer - triage before retry |
| Three-step finish | Close settles → rename for atomic visibility → delete the temp file on failure - partial-write states structurally eliminated |
