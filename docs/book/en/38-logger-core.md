---
num: 38
slug: logger-core
title: Logging (Part 1): Records, Levels, and Structured Fields
volume: 卷五 系统服务
type: practice
lead: The Logger/Sink separation, custom output destinations, structured fields delivered intact — the foundation of the logging system.
api: logger, error
---

## Orientation

Logging is a service's first observability. The core of XRT's logging model is a pair kept separate: the **Logger** (`xrtLogCreate`, naming and the level threshold) and the **Sink** (`xrtLogSinkCreate`, the output destination) — one Logger can carry multiple Sinks, and one Sink can be reused by multiple Loggers; records pass the Logger's level filter and broadcast to every Sink, and **structured fields** (`xrtLogFieldInt` and friends) travel with the record straight to the Sink, which consumes them as it sees fit. This chapter makes the pair's separation and the complete recipe for a custom Sink clear — the foundation for understanding the next chapter's five built-in Sinks.

## Introduction

Three plain needs puncture printf logging's ceiling. Need one: in development, DEBUG floods the screen; in production, only WARN and above — level filtering cannot rely on commenting out printfs and recompiling. Need two: the same record should go to the console and to a file (with different formats) — the output destination cannot be welded into the call site. Need three: the `request complete` message should carry `request_id=42` — when troubleshooting, "which request" is worth more than "what message", and printf can only hand-concatenate strings.

The three needs point at one structure: **level filtering** (the Logger's threshold), **output separation** (the Sink's broadcast), and **structured fields** (fields travel with the record; the Sink decides presentation — a text Sink prints them, a JSON Sink collects them into a fields object). The Logger/Sink separation's extra dividends unfold over the next two chapters: five built-in Sink types work out of the box (Chapter 39), and the async Sink moves disk writes off the hot path (Chapter 39).

## Concepts

### The division of labor between Logger and Sink

```diagram flow
- Logger: name (http/db/auth) + level threshold (records below it are dropped right here)
- Sink: name + its own level threshold + write function + user data
- Composition: xrtLogAttach hangs the Sink into the Logger - one carrier, many; one Sink, reused
- Submit: xrtLog(x) / xrtLogFields(with fields) -> filtered, then broadcast to every Sink
```

The two thresholds each mind their own post: the Logger's threshold is **module policy** (DEBUG on for the db module, INFO for the rest), the Sink's is **destination policy** (console takes only ERROR, the file takes everything). A record must pass both gates before reaching disk — and `xrtLogSinkSetLevel` adjusts at runtime (an operations topic in Chapter 39).

### A custom Sink: four elements and three disciplines

The four elements of `xlogsinkconfig`: a **name** (operations-readable), a **level threshold**, a **write function** (the `xlogresult (const xlogrecord*, ptr)` signature), and **user data** (the carrier for the output target — FILE*, a socket, anything). Three disciplines for the write function: **the record is borrowed** — no keeping references past the callback (the record's memory belongs to the submit path and expires on return); **return-value semantics** — `XLOG_RESULT_WRITTEN`/`_ERROR` are aggregated and reported by the Logger (one Sink's failure doesn't drag down the others); **zero intermediate allocation** — a hot-path write function formats and writes directly, never allocating per record (the sample's direct fprintf is the model).

### Structured fields: values travel with the record

| Constructor | Field type |
| --- | --- |
| `xrtLogFieldInt` / `Float` | numeric fields |
| `xrtLogFieldString` | string field (view) |
| `xrtLogFieldTime` | time field (microsecond value) |
| `xrtLogFieldError` | **error field** — Chapter 4's xerror delivered straight into the log |
| `xrtLogFieldNull` | null sentinel |

The value of fields lies in **consumer-side freedom**: a text Sink may ignore fields or print key-value pairs, a JSON Sink collects them into a `fields` object (the output shape of Chapter 39's json sample), and your custom Sink may cherry-pick the fields it cares about for alerting. `xrtLogFieldError` deserves a name check — an error object enters the log with its cause chain (Chapter 4) preserved whole, so troubleshooting sees "what is the error + why" in one view.

### The level system

Five levels, `XLOG_TRACE/DEBUG/INFO/WARN/ERROR` (numerically increasing). The semantic conventions: TRACE is the debugging blueprint (down to "embarrassingly detailed"), DEBUG is development diagnostics, INFO is a business milestone, WARN is an automatically recoverable anomaly, ERROR is a fault needing human intervention. Threshold-setting experience: **production Loggers stay at INFO, with DEBUG opened per module** — global DEBUG is a log flood; targeted modules are diagnosis.

## Examples

### Complete program: a custom Sink and structured fields

From the repository sample `examples/logging/core/main.c` — about 60 lines through the whole Logger/Sink/fields flow:

```embed path="examples/logging/core/main.c" title="examples/logging/core/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/logging/core/main.c -lws2_32 -liphlpapi
[INFO] example: request complete
```

**What just happened.** (1) `exampleWrite` is the entirety of a custom Sink: fprintf writes directly in the `[级别] 名字: 消息` ([level] name: message) format, `xrtLogLevelName` turns the enum into a readable name, and `ferror` decides between WRITTEN and ERROR — all three disciplines (borrowing/return value/zero allocation) honored. Note that it **does not consume fields** — request_id is ignored by this Sink, exactly the point of "each Sink decides its own field consumption". (2) `Config.UserData = stdout` — the same write function with stderr or a file pointer is another destination; user data is the Sink's universal slot. (3) `xrtLogCreate("example", XLOG_DEBUG)` builds the Logger, `xrtLogSinkCreate` the Sink, `xrtLogAttach` composes them — a three-step assembly. (4) `xrtLogFields` submits a record carrying one integer field, and the return value is checked for WRITTEN — the submit path's failures must also be checked (a full or closed Sink). (5) Teardown order: `SinkFree` before `LogFree` — the Sink is a referenced resource and can be reclaimed independently before the Logger's release.

### Complete program: the printf convenience layer

From `examples/logging/printf/main.c`, the convenient form of formatted submission:

```embed path="examples/logging/printf/main.c" title="examples/logging/printf/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/logging/printf/main.c -lws2_32 -liphlpapi
request=42 status=200
```

**What just happened.** The printf-style submission (the `xrtLogPrintf` family; the format string takes C strings, not views) goes through Chapter 25's formatting internally — `"request=%u status=%u"` and arguments are joined into message text in one step at the submit gate. **Choosing between the convenience layer and structured fields**: fields meant for machine consumption always go through `LogFields` (fields keep their types; a JSON Sink can collect them into an object); concatenated messages for human eyes use the printf layer — when mixing, remember that "a field merged into message text is structured data no longer".

## Contracts

- **Borrowed records**: the Sink's write function keeps no record references; field views likewise expire with the callback.
- **Return values**: WRITTEN/ERROR aggregated by the Logger; one Sink's failure leaves the others untouched.
- **Zero allocation**: the write function writes out directly, never allocating per record.
- **Two thresholds**: the Logger's threshold is module policy, the Sink's is destination policy; a record passes both gates before reaching disk.
- **Typed fields**: structured fields go through `LogFields`; the printf layer only assembles display text.
- **Error fields**: `xrtLogFieldError` carries Chapter 4's error object — the whole cause chain enters the log.

### From examples to engineering: three hosts of logging

**Library code** (modules written for others): no global Logger — accept a caller-injected Logger or build an independent instance per module name (`xrt/db`, `xrt/http` prefixes partition naturally); a library never touches Sinks directly (output policy is the application's privilege). **Application services**: assemble at startup — build the Logger tree and Sink set from config (Chapter 39's five built-ins plus a custom alerting Sink), read levels from environment variables or config, Flush on graceful shutdown before releasing (the async Sink's teardown discipline is in the next chapter). **Utility programs**: one console Logger plus one text Sink suffices — printf, upgraded; five minutes of assembly, lifelong benefit. The shared discipline of the three hosts: **logging assembly concentrates near main** — Logger creation scattered through business code is where configuration drift begins.

### One habit: think retrieval before you write the log

A log's first consumer is "you, woken at 3 a.m. three days later". Before writing each record, mutter three questions: **can it locate the problem** (is there a correlation field like request_id); **is the level right** (is this a milestone INFO or a fault ERROR); **is the volume right** (how many per second at peak; can the disk take it). Records that fail the three questions don't get written — a log, once written, is a liability; a retrievable one is an asset. Structured fields are this habit's technical support: correlation fields into LogFields, levels chosen by the semantic table, volume observed during load tests — Chapter 39's statistics and rotation are the habit's operational extension.

## Pitfalls

### Pitfall 1: keeping the record pointer inside the Sink callback

Symptom: later use of the saved record or field pointers reads dangling content — the same family as the iteration-borrowing pits of Chapters 18/32.

Cause: the record lives on the submit stack and is reclaimed on callback return; saving a reference treats it as an owning object.

```c bad
static xlogresult slowWrite(const xlogrecord* pRecord, ptr pData)
{
	gPending = pRecord;          /* stored globally "for later" - dangling on return */
	return XLOG_RESULT_WRITTEN;
}
```

```c good
static xlogresult slowWrite(const xlogrecord* pRecord, ptr pData)
{
	queue_push(pData, pRecord);  /* the queue deep-copies: fields and message copied before enqueue */
	return XLOG_RESULT_WRITTEN;
}
```

### Pitfall 2: global DEBUG in production

Symptom: a log flood — disk filled, real ERRORs drowned; performance slides with volume.

Cause: the level threshold is policy, not default — global DEBUG turned "development diagnostics" into "permanent state".

```c bad
xlogger* gLog = xrtLogCreate(XRT_STR_LITERAL("app"), XLOG_DEBUG);
/* DEBUG for every module: every SQL statement, every cache hit hits the disk */
```

```c good
xlogger* gLog = xrtLogCreate(XRT_STR_LITERAL("app"), XLOG_INFO);
xlogger* gDbLog = xrtLogCreate(XRT_STR_LITERAL("db"), XLOG_DEBUG);
/* only the db module gets targeted DEBUG - precise diagnosis, controlled volume */
```

## Exercises

### Basic: verify the double threshold

Build a DEBUG Logger plus a WARN-threshold Sink; submit TRACE/DEBUG/INFO/WARN/ERROR in order — count how many actually reach the write function.

### Advanced: an alerting Sink

Write a custom Sink that cares only about ERROR-level records carrying an error field; on a hit, extract the fields and send them to your "alert channel" (a printf simulation is fine); all other records return WRITTEN without writing — a demonstration of field-consumption choice.

### Challenge: an in-memory ring Sink

Implement a Ring Sink: a ring buffer of a fixed N records, Write copies the record (message + fields deep-copied, memory from Chapter 22's pool or arena); expose `dump()` to export the most recent N as text. Acceptance criteria: after 1000 submissions, dump is exactly the last N; zero leaks (verified by Chapter 6's statistics); the write function itself allocates nothing (copy memory is prepared at Sink creation).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Division | Logger (name + threshold) / Sink (destination + threshold); one carrier many, one Sink reused |
| Four elements | name / level / write function / user data — the entirety of a custom Sink |
| Three disciplines | records borrowed, never kept / return-value semantics / zero allocation in the write function |
| Field family | Int/Float/String/Time/Error/Null — typed, delivered straight to the Sink |
| Convenience layer | printf-style submission; machine-bound fields always go LogFields |
| Level semantics | TRACE blueprint / DEBUG diagnostics / INFO milestone / WARN recoverable / ERROR human intervention |
| Threshold policy | production Loggers at INFO + module-targeted DEBUG |
