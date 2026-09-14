---
num: 34
slug: jsonl-xsonl
title: JSONL and XSONL: Line-Delimited Data
volume: 卷四 文本与结构化数据
type: practice
lead: The second shape after documents — a sequence of records, one per line: framing semantics, two-level budgets, global error locations, and atomic files.
api: jsonl, xsonl
---

## Orientation

Chapters 32 and 33 solved how "**one document**" moves in and out of the value tree; engineering has an equally common second shape — "**a sequence of records**": server logs with one event per line, bulk imports with one record per line, inter-process exchange with one message per line. **JSONL (JSON Lines) turns each line into one complete JSON value; XSONL turns each line into one complete XSON value** — many lines together form an `xvalue` Array, while each line stays individually parseable, locatable, and skippable. This is not a new parser: per-record encoding rules, strictness, and error domains are **reused from Chapters 32/33 verbatim**; what is added is exactly three things — line framing semantics, cumulative budgets across records, and the translation of per-record error positions into global line numbers and record indices. When to use "one big document" versus "a sequence of records" — this chapter gives the decision rules.

## Introduction

Three scenarios tell the same story: **the natural unit of the data is the line, not the document**. **Server logs**: each request appends one event line — replay and statistics parse line by line, bad lines are skipped with their position recorded, and line 4096's stray comma must never discard the whole file. **Bulk import**: ten million records, one per line — Chapter 32's DOM turns the whole file into one tree (memory multiplied several times over) and SAX streams but has no notion of "record N"; a line-delimited format makes "parse failed at record N" a first-class citizen. **Inter-process exchange**: internal pipes deliver messages line by line — JSON lines interoperate externally, XSON lines (Chapter 33's bytes/time/set/intmap) round-trip full types internally. All three hosts share one API surface: read into one Array, write as a sequence of lines, errors carry line numbers — **framing is the format's business; the semantics remain the value tree's semantics**.

## Concepts

### Line framing: what counts as one record

```diagram flow
- Delimiting: LF separates; CRLF counts as one line (the CR belongs to the line ending, not the record)
- Blank lines: empty lines and lines of only ASCII spaces/Tab/CR → skipped by default (can be turned into errors)
- Final line: the last complete record may omit the newline
- Not separators: a lone CR / Unicode whitespace (NBSP does not make a blank line)
- One line = exactly one complete root value: cross-line values, several roots on one line, BOM, truncated tails → fail
```

The rule worth memorizing is the last one: **one line must be exactly one complete value**. This is where the "record" semantics come from — the line is both the framing boundary and the validation boundary; `{"id":1} {"id":2}` squeezed onto one line is an error (one root value too many), and `{"id":\n1}` spread across lines is an error (not even one complete root value). An escaped `\n` inside a string does not participate in framing — `"{\"a\":1}\n"` is one valid string record.

### Two-level budgets: per-record inheritance, whole-stream accounting

| Layer | Budget | Scope |
| --- | --- | --- |
| Per record (Record) | MaxInputBytes / MaxOutputBytes / depth | Identical field-by-field to Chapters 32/33; excludes CRLF/LF separators |
| Whole input | MaxInputBytes | **Includes** skipped blank lines and all separators |
| Whole output | MaxOutputBytes | **Includes** each appended LF; excludes the trailing NUL |
| Record count | MaxRecords | Non-blank records = upper bound on result Array elements |
| Syntax values | MaxTotalValues | Cumulative across all records; includes values dropped by duplicate-key policy; excludes the synthesized Array |
| Decoded bytes (XSONL only) | MaxTotalDecodedBytes | Cumulative built-in bytes-tag decoding, checked before allocation |

The two scopes that trip people most: the whole-output budget **counts the LF** (with `MaxOutputBytes=1`, the first record `"1"` commits, and its LF triggers LIMIT); the per-record budget **excludes separators** (a 1 MiB-max line is 1 MiB to `MaxInputBytes`). Defaults follow the library-wide convention: 64 MiB overall, 1000000 records and syntax values — tighten before exposing to untrusted input.

### Global error location: three coordinates and one index

A per-record error (Chapters 32/33) carries in-record line/column; the line-delimited format translates it into global coordinates — `xrtJsonlErrorLocation` yields **zero-based byte offset, one-based physical line, one-based byte column, and zero-based record index**. A blank-line error in strict mode points at the index of the "next pending record"; skipped blank lines still count toward the physical line number — `line=2 record=1` means "line 2 is blank and it should have been record 1". The cause chain keeps the specific per-record reason and Kind; the wrapper only adds a shell at RECORD/OUTPUT/IO.

### Writing: two forms — callback and file

Serialization writes each Array element as one compact line: the **callback form** (`xrtJsonlWrite`) hands chunks to a sink, borrowed bytes valid only during the callback — for streaming straight into a network writer; the **file form** (`xrtJsonlWriteFile`) completes the whole serialization in memory first and then **atomically replaces** the target file — on serialization failure the original file remains untouched. Shared semantics of both forms: compact + LF (PRETTY is rejected), the root must be `XVALUE_ARRAY`, and configurations are snapshotted by value (mutating the config inside a callback does not affect the in-flight call).

### JSONL, XSONL, or neither

| Scenario | Choice | Rationale |
| --- | --- | --- |
| External logs / open data | JSONL | Any `jq`/script/heterogeneous client consumes line by line |
| Internal logs and pipes | XSONL | bytes/time/set land on lines as-is; replay with zero conversion |
| One config, loaded whole | Chapters 32/33 documents | Document semantics (duplicate-key policy, full SAX/Writer paths) |
| Single huge records (> a few MiB) | Documents + streaming | Line formats tightened per line get blown up by a single record |

The decision variable is isomorphic to Chapter 33's: **receiver under your control → XSONL for full types; not under control → JSONL for interop**; if the data is really "one configuration", don't split it into lines — and if it is really "a stream of events", don't cram it into one big array.

## Examples

### Complete program: JSONL round trip and error location

From the repository sample `examples/data/jsonl/main.c` — parse, validate, read back, callback writing, atomic file round trip, and blank-line location in one pass:

```embed path="examples/data/jsonl/main.c" title="examples/data/jsonl/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/data/jsonl/main.c -lws2_32 -liphlpapi
JSONL: 3 records, 20 bytes
empty line: line=2 record=1
```

**What just happened.** (1) Framing in evidence: the input mixes blank lines with CRLF/LF — all 3 records (object, array, null) arrive intact, and 20 bytes is the actual compact output size (LF included per record). (2) Blank-line policy: with skipping on by default, `Valid`/`Read` pass; turning on `XJSONL_READ_REJECT_EMPTY_LINES` makes `"{}\n\n"` fail immediately at the blank line before the second record. (3) Global location: the error gives `line=2 record=1` — the blank line is on line 2 and it cuts the position after record 1; the caller learns "which line to fix", not a byte range. (4) File loop: `StringifyFile` atomically replaces → `ParseFile` reads back → `WriteFile` (with config) replaces again → `ReadFile` reads again — any failed step leaves the file in its last complete state.

### Complete program: XSONL line-by-line reading and writing

From `examples/data/xsonl/main.c` — the same framing and budget machinery, with per-record rules swapped to XSON:

```embed path="examples/data/xsonl/main.c" title="examples/data/xsonl/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/data/xsonl/main.c -lws2_32 -liphlpapi
XSONL: 3 records, 20 bytes
empty line: line=2 record=1
```

**What just happened.** The flow is line-for-line isomorphic to the jsonl sample — not laziness but **a design promise made visible**: swap the format and the framing, budget structure, and error location all stay; the API shapes correspond one to one. The difference is per-record semantics: each line here is a complete XSON value, and Chapter 33's four extended types go straight into the line stream —

```c
/* Full-type records in an XSONL line stream: after parsing, each lands in place in the value tree */
xvalue* pLog = xrtXsonlParse(XRT_STR_LITERAL(
    "{\"key\":bytes(\"AAEC/w==\"),\"at\":time(\"2026-07-31T00:00:00Z\")}\n"
    "set[80, 443]\n"));
/* pLog[0] is an object (Bytes/Time fields), pLog[1] is a set container — line-by-line replay with zero conversion */
```

When the built-in bytes tag needs its own total cap, `MaxTotalDecodedBytes` intercepts before buffer allocation (default 64 MiB) — stopping the "a little per line, a mountain in aggregate" decode bomb.

## Contracts

- **One value per line**: each line is exactly one complete root value; escaped newlines inside strings do not frame; the last record may lack a newline.
- **Failure atomicity**: reads never return a partial Array (failures free all partial results); file writes serialize fully, then replace atomically — failures never touch the original.
- **Budget scopes**: whole input/output include separators and blank lines; per-record does not; output includes each LF — all three scopes are promises, not implementation details.
- **Borrowing and ownership**: input text and config are borrowed until the call returns; the result Array is released with `xrtValueRelease`, strings with `xrtFree`; writing never consumes the caller's references (iteration over a backing snapshot).
- **Threading**: no implicit locks; each call owns an independent workspace; the caller synchronizes shared mutable input.
- **Committed bytes are not rolled back**: record bytes already delivered before a synchronous-callback failure cannot be recalled — see Pitfall 2.

### From examples to engineering: three hosts of line-delimited data

**Server logs** (most common): the writer `Stringify`-writes one line per event, appending or streaming via `Write` into a sink; the reader `Parse`-reads into an Array for offline statistics, sending bad lines to an audit log via `ErrorLocation` and carrying on. **Bulk import**: `Read` yields all valid records at once (any failure fails the whole batch, reported by line), or the text is chunked and `Parse`-processed per chunk for "skip bad chunks, keep importing" — both semantics stand on "the line is the boundary". **Inter-process line streams**: isomorphic to Chapter 33's rules — JSONL outside, XSONL inside; line streams naturally fit text pipelines (`grep`, redirection, line-oriented gateways), an operational property document formats cannot offer.

### A contrast: the same batch of events, two ways on disk

A batch of request logs can land as **one big JSON array** (Chapter 32) or as **JSONL**. Array form: appending means read-modify-rewrite (or the fragile trick of patching a trailing `]`); one bad spot makes the whole file unparseable; streaming consumers must wait for the document to close. JSONL form: appending is a plain `AppendFile`; a corrupted line damages only itself; `tail -f` consumes as data arrives. The point of the contrast: **append-only writing and tolerance of partial corruption are the essential advantages of record streams** — as long as the data is an append-only event sequence, the line-delimited format is the right shape; conversely, configurations that need whole-file reading with strongly related fields remain document territory.

## Pitfalls

### Pitfall 1: expecting the SKIP strategy to skip records that cannot be encoded

Symptom: while writing, one record in the batch contains a value JSON cannot express (say, a function handle); you expect "skip this one and keep going", but the whole `Stringify` call fails.

Cause: the SKIP strategy only applies to **members inside a single root value**; an inexpressible root fails the whole record — the write-side mirror of "one line must be exactly one complete value", preventing physically blank lines from breaking the framing.

```c bad
/* A value the format cannot express is mixed into the tree; hoping SKIP skips the whole record */
xrtJsonlStringify(pArray, &Size);   /* whole call fails: an inexpressible root cannot be skipped */
```

```c good
/* Filter before writing: remove inexpressible records, or degrade them to placeholder objects by convention */
/* External channel: all-JSON types; internal channel: just use XSONL — extended types are natively expressible */
str Text = xrtXsonlStringify(pArray, &Size);
```

### Pitfall 2: assuming a failed callback write "rolls back" what was already written

Symptom: `Write`'s callback hits a full target halfway through and returns false; the caller assumes the file/stream is restored to pre-write state and retries in place — and the log now contains half a batch.

Cause: the synchronous callback is **streaming delivery** — bytes are committed to the far end (file, socket, pipe) the moment the callback returns true, and the library has no "recall" channel; this is promised semantics (Contract item 6), not a defect.

```c bad
/* Retry in place; the committed half batch is written again */
while ( !xrtJsonlWrite(pArray, &Write, sink, Ctx) )
    ;   /* committed bytes cannot be recalled — retry = duplicated records */
```

```c good
/* Stop at failure: recover at a record-level checkpoint — track how many records
 * were durable before Write and replay from there; or use the file form
 * (atomic replacement: all-or-nothing by construction) */
if ( !xrtJsonlWriteFile(Path, pArray, &Write) ) { /* original untouched; fix the data and start over */ }
```

## Exercises

### Basic: a bad-line auditor

Read a JSONL file into an Array; print an audit table of syntax-error lines with line number, record index, and byte offset (hint: split the text into lines and `xrtJsonValid` each, or use strict blank-line mode plus `ErrorLocation`). Acceptance: on a 10-line file with 2 planted errors, the report points at exactly those two.

### Advanced: a JSONL ⇄ big-array converter

Implement both directions: a JSONL file → a single JSON array document; a JSON array document → JSONL. Cumulative budgets must bound memory (configurable whole-stream cap), and conversion errors must report by line or element index. Acceptance: `Valid` passes both directions; a round trip is record-for-record equal to the original file.

### Challenge: a streaming importer with checkpointed resume

Build a "skip bad chunks, keep importing" pipeline: `Parse` in N-line chunks; on a bad chunk, degrade to per-line `Parse` to pinpoint and skip the bad line; persist progress (records imported) so a rerun resumes from the checkpoint with zero duplicates. Acceptance: on a hand-made 10,000-line file with 3 scattered errors, one run imports 9,997 records and reports the 3 bad lines; a second run duplicates nothing.

## Cheat Sheet

| Point | Quick reference |
| --- | --- |
| Framing | One value per line; LF/CRLF both fine; blank lines skipped by default; final line may lack a newline; lone CR / Unicode whitespace are not separators |
| Budgets | Whole-stream includes separators and blank lines; output includes LF; per-record excludes separators; defaults 64 MiB / 1,000,000 records |
| Location | Zero-based offset + one-based line/column + zero-based record index; blank-line errors point at the next pending record |
| Reading | All-or-nothing (no partial Array); `Valid` is DOM-free and skips duplicate-key policy |
| Writing | Compact + LF (PRETTY rejected); streaming callback / atomic file replacement; committed bytes cannot be rolled back |
| Selection | Event streams → line-delimited (JSONL outside / XSONL inside); configurations → Chapters 32/33 documents |
| XSONL delta | Per record = XSON rules; `MaxTotalDecodedBytes` caps built-in bytes decoding in aggregate |
