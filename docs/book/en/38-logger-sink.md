---
num: 38
slug: logger-sink
title: Logging (Part 2): Five Sinks, Rotation, and Async
volume: 卷五 系统服务
type: practice
lead: Five built-in Sinks — console/file/text/JSON/async — runtime rotation and reopen, and moving hot-path disk writes off the submit thread.
api: logger, io
---

## Orientation

The previous chapter wrote every part of a custom Sink; this chapter assembles the whole machine: **five built-in Sinks** (console, file, text formatting, JSON formatting, async wrapping) cover the vast majority of output needs, all in a uniform configurator style (`ConfigInit` + customization + creation). The operations trio — **rotation** (`xrtLogFileRotate`), **reopen** (`xrtLogFileReopen`, recovering after external truncation), and **statistics** (`xrtLogFileStats`) — adapts the file Sinks to long runs. The **async Sink** is the hot path's key: the submitting thread only enqueues, a dedicated thread writes to disk — logging never slows the business down again.

## Introduction

Three hurdles when logging goes live. Hurdle one: logs must reach the collection pipeline as JSON (ELK/Loki eat structured data) — hand-writing JSON assembly in a custom Sink? Field escaping, time formats, nested error chains — pits everywhere. Hurdle two: log files grow without bound — the disk fills in three months; you need rotation by size, compressed archival, and the ability to resume writing after logrotate truncates externally. Hurdle three: at peak, tens of thousands of records per second, and the disk-write IO latency lands directly on request latency — logging becomes a performance tax.

The three hurdles map to three answer sets: the JSON Sink works out of the box (fields auto-collected into the object, error chains auto-expanded, microsecond timestamps); the file family's Rotate/Reopen/Stats (built-in rotation semantics + external-truncation recovery); and the async Sink moving disk writes off the submit path (queue buffering + a dedicated flushing thread). Their shared precondition is Chapter 37's foundation — every Sink implements the same write-function contract.

## Concepts

### The five built-in Sinks in one table

| Sink | Creation entry | Typical use |
| --- | --- | --- |
| Console | `xrtLogConsoleConfigInit` + customize | development terminal, container stdout |
| File | `xrtLogTextFile` / `xrtLogJsonFile` | on-disk archival, chosen by format |
| Text formatting | the `xrtLogTextWrite` family | custom text layouts (reusing the formatting engine) |
| JSON formatting | the json configurator | collection pipelines, structured retrieval |
| Async wrapping | the async configurator | hot-path submission without blocking |

The configurator style is uniform library-wide: `ConfigInit` fills defaults → change fields as needed → create in one step. The JSON Sink's output shape (visible in the json sample): top-level `time` (microseconds)/`level`/`logger`/`message` plus a `fields` object collecting structured fields — names and types preserved as-is, zero parsing cost for the pipeline.

### The file-operations trio

```diagram flow
- Rotate: rotation triggered immediately - the current file is renamed to archive, a new file opens at the original path and writing continues
- Reopen: reopen after external truncation (logrotate move) - handles invalidated, automatically recovered
- Stats: cumulative written/dropped counts and other runtime statistics - the data source for capacity planning and alerting
```

Rotation triggers come in a built-in policy (automatic by size) and manual firing (`xrtLogFileRotate`, the operations interface — time-based rotation is just cron calling it); `xrtLogFilePath` queries the current file path — after rotation the path changes, and monitoring needs to know where writing is happening now.

### The async Sink: queue + dedicated thread

The truth of the submit path: with a synchronous file Sink, every record's IO latency (milliseconds) lands directly on the business call. The async Sink's structure: the **submitting thread** only does "format into the queue" (microseconds; the lock-free queue's batch interface — Chapter 21's MPMC is exactly its foundation); a **dedicated thread** dequeues in batches and writes (amortized efficiency at batch size 32). The costs and disciplines: shutdown must Flush (`xrtLogFlush` — a clean exit means the queued records have reached disk; what you lose is the last few hundred milliseconds before a crash); queue capacity is the backpressure valve (when full, drop or block per policy — drop counts go into Stats; "a log flood sacrificing logs" is the correct trade).

### Formatting reuse: TextWrite and field printf

The `xrtLogTextWrite` family lets a custom destination reuse the text formatting engine (your Sink only decides "write where"; formatting is the library's job); the `xrtLogFieldsPrintf` family printf-assembles field values (a convenience layer; Chapter 37's selection rule still applies). Validators (`xrtLogTextConfigValidate` / `JsonConfigValidate` / `RecordValidate`) stand guard at the config-loading gate — a wrong format string in config surfaces at startup, not as a runtime landmine.

## Examples

### Complete program: structured JSON output

From the repository sample `examples/logging/json/main.c` — fields auto-collected into the object, error chains expanded:

```embed path="examples/logging/json/main.c" title="examples/logging/json/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/logging/json/main.c -lws2_32 -liphlpapi
{"time":1788575322193322,"level":"INFO","logger":"http",
 "message":"request completed","fields":{"request_id":42,"cached":false}}
```

**What just happened.** (1) One `LogFields` submission (integer field 42, boolean field false) — the JSON Sink automatically collects them into the `fields` object: **fields delivered typed and intact** (42 is a number, not `"42"`; false is a boolean), zero parsing cost for the pipeline. (2) `time` is Unix microseconds (Chapter 3's xtime measure) — machine-sortable, human-formattable; the format choice belongs to the consumer. (3) message and fields are separate in the output — the human-facing message and the machine-searchable fields each in their place; that is the very meaning of "structured logging". Contrast Chapter 37's text Sink (which ignored fields and printed only the message): hang both Sinks on one Logger — text to the console, JSON to the pipeline — dual-format in parallel is precisely this model's value.

### Complete program: the async Sink

From `examples/logging/async/main.c` — zero blocking on the submit thread, a dedicated thread flushing to disk:

```embed path="examples/logging/async/main.c" title="examples/logging/async/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/logging/async/main.c -lws2_32 -liphlpapi
asynchronous sink ready
```

**What just happened.** (1) The async Sink wraps a real destination (a file) — to the Logger it is an ordinary Sink; the submission interface is unchanged. (2) The main thread's submissions only enqueue (`asynchronous sink ready` prints immediately — submission was not blocked by disk writes); the dedicated thread consumes in batches in the background. (3) The shutdown path's Flush: queue drained, file handle closed — a clean exit. The sink_tour sample is left as the full-form reading of the operations interfaces: it walks the Ref/Name/Level/SetLevel/Count/Stats/Detach family and the file-specific family (Path/Rotate/Reopen/Stats) end to end — nine oks for nine interface groups.

## Contracts

- **Uniform configurators**: `ConfigInit` → customize → create; validators guard the config gate.
- **JSON shape**: time in microseconds / level / logger / message + the fields object — fields typed, time machine-ordered.
- **Rotation semantics**: Rotate renames to archive and opens a new file; automatic by size plus manual triggers; Path queries the current path.
- **Truncation recovery**: Reopen reopens after an external move/truncate — built-in support for logrotate cooperation.
- **Async discipline**: Flush at shutdown; queue capacity is the backpressure valve, drop counts go into Stats; submission in microseconds, flushing amortized in batches.
- **Formatting reuse**: custom destinations use the TextWrite family; machine-bound fields still go LogFields.

### From examples to engineering: three Sink-assembly patterns

**Development environment**: one console Sink (text format, DEBUG threshold) — instantly readable in the terminal, five-minute assembly. **Production standard**: a JSON file Sink (INFO threshold, rotation by size) + an async wrapper (queue backpressure) + a console ERROR Sink (the stderr alert channel in container environments) — three Sinks, each at its post, assembled once. **Collection pipeline**: JSON format straight to stdout (container stdout collection) or rotating files plus a Filebeat-class agent — the format is already uniform at the Sink layer, zero adaptation in the pipeline. The three patterns share a shape: assembly code concentrates in the startup function (Chapter 37's discipline), configuration comes from a Chapter 32 JSON config file (Sink type, path, level — all configurable) — "the logging system's own log config" is the standard exercise in config-driven assembly.

### An operations view: logs are a data stream

Treating logging as "printing" is the development view; in production it is a **data stream** — with a source (business code), a pipeline (Sinks and queues), a destination (files/collection systems), a capacity (disk and queue), and a lifecycle (rotation and archival). Make operations decisions with the data-stream lens: capacity planning reads Stats' write rate × retention period; flood protection relies on the queue's drop policy plus dynamic level lowering (`SetLevel` adjusts at runtime); failure recovery rehearses Reopen (drill the external truncation); quality monitoring reconciles "submitted vs written" (async drops within tolerance). This view also explains why Chapter 35's pipeline thinking applies again — the logging system is itself a "record → format → queue → disk" pipeline, and the assembly skills you learned in Volume 4 go on duty here directly.

## Pitfalls

### Pitfall 1: shutting down the async Sink without Flushing

Symptom: when reproducing a crash, "the last stretch of logs is gone" — exactly the few hundred milliseconds you need most; the graceful path loses nothing, but kill -9 always loses.

Cause: the process exits while the async queue's records are still unwritten — the queue is volatile, and Flush is the promise of durability.

```c bad
/* shutdown */
xrtLogSinkFree(pAsyncSink);   /* freed directly - queue contents dropped */
```

```c good
/* shutdown: flush the queue first, then free */
xrtLogFlush(pLogger);          /* or the async Sink's dedicated Flush interface */
xrtLogSinkFree(pAsyncSink);
```

### Pitfall 2: one file Sink per log call

Symptom: file-handle counts explode; rotations trample each other; process handle-limit alarms.

Cause: "the place that logs" was mistaken for "the place that builds Sinks" — a Sink is a globally assembled resource, not a disposable object created on a whim.

```c bad
void handle_request(void)
{
	xlogsink* pSink = create_file_sink("app.log");   /* one per request */
	xrtLogAttach(gLog, pSink);
	xrtLog(gLog, XLOG_INFO, ...);
	xrtLogSinkFree(pSink);                            /* build, tear down, build again */
}
```

```c good
/* assemble once at startup, reuse globally (Chapter 37's "concentrated assembly" discipline) */
static xlogsink* gFileSink;
void init_logging(void) { gFileSink = create_file_sink("app.log"); xrtLogAttach(gLog, gFileSink); }
void handle_request(void) { xrtLog(gLog, XLOG_INFO, ...); }   /* submit only, never assemble */
```

## Exercises

### Basic: dual-format in parallel (one record, two presentations)

Hang a text Sink (console) + a JSON Sink (file) on one Logger; submit the same LogFields record — verify the text line's and the JSON line's respective presentations of the fields.

### Advanced: a timed rotator

Implement "rotate hourly" with `xrtLogFileRotate`: a background timer fires, the archived file is gzipped after rotation (Chapter 29's one-shot compression), and `xrtLogFilePath` prints the current path. Hint: the timer can be a simple polling thread (Volume 6 has proper timers).

### Challenge: a log-flood experiment

Construct a load of a hundred thousand submissions per second: run a synchronous file Sink and an async Sink for one minute each, measuring three comparison groups — average submit-side latency, drop counts (Stats), and total bytes written. Acceptance criteria: the async version's submit-side latency is at least two orders of magnitude below the synchronous one; under the drop policy the flood never blocks business; the report contains the three groups of numbers plus analysis.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Five Sinks | console / file (text/JSON) / formatting reuse / async wrapping |
| JSON shape | time microseconds + level + logger + message + fields object (fields typed) |
| Operations trio | Rotate / Reopen after truncation / Stats; Path queries the current path |
| Async discipline | Flush at shutdown; queue = backpressure valve; submit in microseconds, flush in batches |
| Assembly discipline | Sinks assembled once at startup, reused globally; business code only submits |
| Validators | guarding the config-loading gate - format errors surface at startup |
