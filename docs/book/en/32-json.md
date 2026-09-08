---
num: 32
slug: json
title: JSON Reading and Writing
volume: 卷四 文本与结构化数据
type: practice
lead: DOM, SAX event streams, and the incremental Writer — three paths for config parsing, large-file scanning, and streaming serialization, each in its place.
api: json, value
---

## Orientation

JSON is the lingua franca of contemporary data exchange, and XRT gives it three complete paths: **DOM** (`xrtJsonParse` into an xvalue tree — the default for configs and small documents), **SAX** (`xrtJsonVisit` — an event-callback streaming traversal, zero intermediate tree for large files), and **Writer** (`xrtJsonWriterCreate` — incremental construction, streaming serialization without assembling a whole tree). The three paths share one strictness kit: duplicate keys rejected, resource limits (Chapter 3's three gates) defending against deep-nesting bombs, and errors carrying line/column positions (`xrtJsonErrorLocation`). This chapter clarifies each path's fitting scenarios and gives the decision rules for choosing among them.

## Introduction

Three scenarios, three paths. **Config loading**: at startup, read a JSON of a few hundred lines — the DOM path is three lines of code (Parse, fetch by name, Release when done), and the value tree goes straight into Chapter 30's merge/read system. **Log scanning**: walk a 2GB JSON Lines log looking for anomaly events — DOM would turn the whole file into a tree (memory explosion), while SAX fires event callbacks one by one, zero intermediate structures, memory independent of file size. **Response serialization**: the server writes query results as a JSON response — the data already lives in business structs, so building a value tree first just to serialize it is waste; Writer writes incrementally: open object, write key-values, close object — output while generating.

The three paths are not a three-way metaphysical choice but answers to **three data-shape questions**: need random access and repeated reads → DOM; single-pass scanning → SAX; output while generating → Writer. All three coexisting in one system is the norm — config by DOM, logs by SAX, responses by Writer, each used where it fits best.

## Concepts

### DOM: parse, access, serialize

```diagram flow
- Parse: xrtJsonParse(text) -> owning root value (released with Release)
- Access: ObjectGet fetches a member by name (borrowing) -> exact read (Chapter 30)
- Serialize: xrtJsonStringify(value, bPretty) -> owning text
- Strictness: duplicate keys rejected; resource-limit three gates; errors carry line/column positions
```

DOM's value is "parse once, access randomly": the value tree sits in memory and jumps anywhere by path. When `xrtJsonParse` fails, the thread error slot holds structured information, and `xrtJsonErrorLocation` extracts the line and column — "missing comma at line 3, column 14" points at the character. Float serialization uses Chapter 26's shortest round-trip — the `0.1` read in writes back as `0.1`, zero drift around the loop.

### SAX: the event-callback stream

`xrtJsonVisit(文本, 回调, 用户数据)` (text, callback, user data) turns parsing into an event sequence: enter object/array, member name, scalar value, container end. The callback receives an `xjsonevent` structure (HasName/Name/type/value) and returns false to abort. **Zero intermediate tree** is its structural advantage: memory usage is the callback's own stack; combined with Chapter 27's streaming decode it achieves "parse while receiving". SAX's disciplines: don't do heavy work in the callback (it sits on the parse path), and don't keep the event's borrowed pointers past the callback (Chapter 30's validity discipline).

### Writer: incremental construction

```c
xjsonwriter* W = xrtJsonWriterCreate(NULL);
xrtJsonWriterObject(W);          /* open object */
xrtJsonWriterName(W, "name");
xrtJsonWriterString(W, ...);
xrtJsonWriterEnd(W);             /* close object */
str s = xrtJsonWriterFinish(W);  /* validate pairing completeness and take the result */
```

Open/close pairing is validated by `Finish` — a missing End surfaces right there instead of producing mangled JSON. The Writer can output to memory (Take an owning string) or write directly to a sink (isomorphic with Chapter 34's Logger sink system) — the latter lets serialization connect straight to files/networks.

### Path-selection rules

| Scenario | Path | Reason |
| --- | --- | --- |
| Config/small documents | DOM | random access, shortest code |
| Large-file single-pass scan | SAX | memory independent of file size |
| Output while generating | Writer | no intermediate tree |
| Modify then re-serialize | DOM | edit on the tree, then Stringify |

### Resource limits and strictness

Three gates for untrusted JSON (Chapter 3's `xrtresourcelimits`): nesting depth, entry count, total bytes — a malicious document is rejected at level 128 instead of eating through the stack. Duplicate keys rejected (a config file's "same-name key overwrites" is an accident, not a feature); number parsing follows Chapter 26's strict semantics. **Parse configuration differs across the trust boundary**: locally generated configs ship with default limits; external input tightens all three gates.

## Examples

### Complete program: all three paths in one pass

From the repository sample `examples/data/json/main.c` — the same data read by DOM, scanned by SAX, written by Writer:

```embed path="examples/data/json/main.c" title="examples/data/json/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/data/json/main.c -lws2_32 -liphlpapi
name = xrt
{
  "name": "xrt",
  "features": [
    "json",
    "http"
  ]
}
member: code
member: ok
{"code":200,"message":"OK"}
```

**What just happened.** (1) DOM segment: `Parse` yields the root value, `ObjectGet("name")` fetches the member by name (borrowing, not ownership transfer), and `GetString` reads out the view and prints — three steps, the everyday shape of config reading. (2) Serialization segment: `Stringify(值, true)` (value, true) pretty-prints — indented format for human eyes; machine exchange uses false (compact) for smaller size. (3) SAX segment: `Visit` fires callbacks event by event, printing `member: code/ok` — every member name in the event stream passed through the callback's hands, no intermediate tree the whole way. (4) The Writer segment is in the file's second half: incremental construction with `Finish` validation — the last line `{"code":200,"message":"OK"}` is the Writer's product; while reading the code, note that every Object/Name/Int/String pairs with an End. (5) The tour sample's fifth line verifies error location: feed input with a missing comma, and `error-location line=1` pinpoints the line — another fulfillment of Chapter 4's "errors carry their own location".

### Complete program: the full-interface tour (files and streaming)

From `examples/data/json_tour/main.c`, covering file parsing, file writing, streaming, and sink writes:

```embed path="examples/data/json_tour/main.c" title="examples/data/json_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/data/json_tour/main.c -lws2_32 -liphlpapi
json: read+valid ok file-parse/read ok
json: write-file/stringify-file(pretty) ok
json: stream write=23 quote=9
json: sink-writer [[1,true,null,2.5,"v"]] ok
json: error-location line=1 ok
```

**What just happened.** (1) File paths: Parse and Stringify interface with files directly — the file shapes of config loading and export. (2) Streaming write: the Writer writes element by element, counting 23 elements and 9 quotes — empirical proof of "output while generating". (3) Sink write: the Writer's output connects straight to the target (buffer/file) — serialization composed with IO, no intermediate string. The four postures cover every meeting point of JSON with the file system (Chapter 44).

## Contracts

- **Three paths**: DOM for random access / SAX for single-pass zero-tree / Writer for incremental output; choose by data shape, not by taste.
- **Strictness**: duplicate keys rejected; numbers follow Chapter 26's strict semantics; errors carry line/column positions (`xrtJsonErrorLocation`).
- **Resource limits**: untrusted input must set the three gates (depth/entries/bytes) — the defense line is at the parse entrance, not the business layer.
- **Round-trip precision**: float serialization uses the shortest round-trip — the read-write loop drifts zero.
- **Ownership**: Parse's root value released wholesale with Release; ObjectGet borrows; Stringify's product freed with `xrtFree`.
- **Writer pairing**: opens and closes validated by Finish; output is takeable (memory) or sink-direct.

### From examples to engineering: three hosts of JSON

**Config layer**: load at startup → DOM value tree → Chapter 30's merge system (defaults + user overrides) → read-only throughout. Three config disciplines: tighten the three gates by business (config files don't nest ten-thousand deep), report both missing fields and type mismatches (Chapter 30's exact reads), and on hot reload Release the old tree and swap in the new one (reference counting guarantees in-flight requests finish reading safely). **Protocol layer**: parsing every request's JSON body — SAX or DOM by body size, resource limits mandatory (external input), error positions written into the 400 response to help clients self-diagnose. **Data-exchange layer**: passing between modules and across processes — hand the value tree over directly (reference-count balancing, see Chapter 30's three hosts) or serialize and go through the channel (shortest round-trip guarantees precision). All three layers share one strictness kit; only the gate widths and the error reports' destination differ.

### A comparison: the code shapes of the three paths

The same need — "fetch the user's name field" — under all three paths is worth reading side by side. DOM: `Get(Get(Root,"user"),"name")` — two hops, shortest, repeatedly accessible. SAX: the callback maintains a "currently inside the user object" state machine and records on the name event — longer code but constant memory. Writer: the inverse problem (generate rather than read), Name+String in two steps — there is no "take", only "give". Read the three snippets side by side once, and the intuition for path selection arrives: **the access pattern dictates the code shape; an awkward shape usually means the wrong path was chosen**.

## Pitfalls

### Pitfall 1: no resource limits on external JSON

Symptom: stack overflow or memory ballooning while processing user-uploaded JSON — ten-thousand-deep nested array text eats through the recursive parser's stack.

Cause: default resource limits are loose (aimed at trusted configs); external input must tighten the three gates — the defense line belongs at the parse entrance.

```c bad
xvalue* pRoot = xrtJsonParse(Text, NULL);   /* default limits - ten-thousand-deep nesting blows the stack */
```

```c good
xrtresourcelimits Limits;
xrtResourceLimitsInit(&Limits);
Limits.iMaxDepth = 64;      /* business needs 3 levels at most; 64 is already tenfold headroom */
xvalue* pRoot = xrtJsonParse(Text, &Limits);
if ( pRoot == NULL ) {
	size_t iLine, iColumn;
	xrtJsonErrorLocation(xrtGetError(), &iLine, &iColumn);
	Reject(iLine, iColumn);   /* over the limit means reject - and the error carries a position */
}
```

### Pitfall 2: saving borrowed pointers inside SAX callbacks

Symptom: string pointers saved during the scan are used afterward and read dangling or reused content — the same family as Chapter 18, Pitfall 1.

Cause: the event's strings are borrowed views whose validity ends with the callback — the parser reuses its buffers.

```c bad
static bool onEvent(const xjsonevent* pEvent, ptr pData)
{
	if ( pEvent->Type == XJSON_EVENT_STRING ) {
		gSaved = pEvent->String;   /* borrowed view stored globally - dangling by the next callback */
	}
	return true;
}
```

```c good
static bool onEvent(const xjsonevent* pEvent, ptr pData)
{
	if ( pEvent->Type == XJSON_EVENT_STRING ) {
		Process(pEvent->String);   /* use it up within the callback's lifetime */
		/* to keep it: Dup a copy first, then save */
	}
	return true;
}
```

## Exercises

### Basic: a config reader (the DOM three-step posture)

Read `{ "host": "0.0.0.0", "port": 8080, "debug": true }` via the DOM path: fetch the three fields by name and print them (port and switch through Chapter 30's exact reads).

### Advanced: a JSON Lines scanner

Scan a `.jsonl` file (one JSON object per line) via SAX: parse line by line, count occurrences of each event type in the callback, and find the line numbers of `"level":"error"`. Hint: line separation is just "one Visit call per line"; combine with Chapter 25's line iteration.

### Challenge: a streaming response generator

Implement `write_page(Writer, 页号, 数据数组)` (Writer, page number, data array): the Writer incrementally writes out a paginated response object (metadata + data array), with data elements coming from business structs (no value tree assembled); the output feeds a memory sink and a file sink simultaneously. Acceptance criteria: the produced JSON passes Parse validation (legal round trip); generating a million elements peaks at memory independent of element count (streaming proof); the file and memory products are byte-identical.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Three paths | DOM (Parse/Get/Stringify) / SAX (Visit event stream) / Writer (incremental build) |
| Path rules | random access → DOM / single-pass scan → SAX / output while generating → Writer |
| Strictness | duplicate keys rejected; errors carry line/column (`xrtJsonErrorLocation`) |
| Three gates | depth/entries/bytes — mandatory for external input; over-limit rejects wholesale with a positioned report |
| Floats | shortest round-trip serialization — the read-write loop drifts zero |
| Writer | open/close pairing validated by Finish; Take it away or connect straight to a sink |
| Ownership | the root value's Release frees the whole tree; member access borrows; on hot reload the old tree exits safely |
| Three hosts | config layer (merge system) / protocol layer (three gates + error echo) / data exchange (value trees or serialization) |
| Shape criterion | the access pattern dictates the code shape — an awkward shape usually means the wrong path |
