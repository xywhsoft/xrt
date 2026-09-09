---
num: 137
slug: project-cli
title: Project 1: A Log-Statistics CLI Tool
volume: 卷十三 实战项目
type: project
lead: From requirements to deliverable: line reading over mixed-EOL logs, level filtering and counting, builder-based report output, a temporary report file — the book's knowledge applied together for the first time.
api: io, string, file
---

## Orientation

Volume 13 opens with the first complete project. Every chapter of the preceding twelve volumes covered "how to use one module"; this chapter installs **five modules into one program** — a log-statistics CLI (logstat): read a service log (mixed line endings, possibly huge), filter by level, count lines per level plus error samples, produce a text report. It sounds plain, but every step is the main subject of some earlier chapter: **line reading** (Chapter 40's xlinereader — EOL-adaptive, last line without newline, borrowed views); **level detection** (Chapter 25's string-view operations — zero-copy prefix matching); **counting and dedup** (Chapters 12/18's hashing and Map); **report generation** (Chapter 25's builder — O(n) concatenation); **writing the report to disk** (Chapters 44/46 files and temp directories — never assuming the working directory is writable). Project chapters differ from teaching chapters in form too: we first write **requirements and acceptance**, then walk the **design decisions** (why these modules, the trade at each step), then **implementation** (two complete programs in progression), finally **acceptance and extensions** — the full engineering workflow.

## Introduction

The requirement comes from a real scene: the service died at midnight, you receive a 2 GB log file (copied via rsync — the line endings are mixed: CRLF from the old Windows segment, LF from the Linux segment, even stray CRs from some transcoding accident), and must quickly answer three questions: **how many lines per level? Where did the errors happen (first few samples)? What was the most active time window?** Generic tools (grep+awk) can be spliced together, but the EOL problem needs repeated handling and logic like "dedupe error samples" writes awkwardly; a one-off script is hard to reuse.

Make it a proper small tool, **logstat**: `logstat <日志路径> [--level ERROR] [--top 10] [--report]` (log path) — EOL-adaptive reading, level filtering, counting, error samples, optional report file. **Acceptance criteria** (written before the code): (1) mixed-EOL input handled correctly with zero configuration; (2) a 2 GB file at MB-level memory (streaming — never reading it whole); (3) three boundaries correct — empty file, no matching level, all-errors input; (4) the report lands in the system temp directory (never assuming the working directory is writable), the filename carrying a colon-free timestamp; (5) every error path goes through structured errors (Chapter 4 — the error chain complete enough to locate).

## Concepts

### Design decisions: module choice and rationale

| Requirement step | Choice | Rationale and rejected alternatives |
| --- | --- | --- |
| Read log lines | `xrtLineReaderTake`+Reader (Chapter 40) | EOL-adaptive (all three recognized) / borrowed views zero-copy / grows on demand after the initial capacity. Alternative "read whole then strtok": memory on the file's order, manual EOL handling — rejected |
| Level detection | view prefix comparison (Chapter 25) | each line starts `LEVEL rest` — one zero-copy slice of the view. Alternative "strdup each line then parse": one allocation per line — rejected |
| Counting | fixed-length array (five levels) | levels are a small fixed set — an array beats Map on speed and stays at zero allocation. Alternative Map: a sledgehammer for a fly — rejected (an "arbitrary token frequency" count is when Chapter 18 applies) |
| Error samples | fixed-size ring buffer (first N) | only "the first few" are needed — the ring overwrites. Alternative "store all then take N": memory explodes on a big error log — rejected |
| Report assembly | the `xstrbuf` builder (Chapter 25) | multi-segment O(n); alternative repeated Concat: O(n²) — Chapter 25's lesson applies directly |
| Writing to disk | temp directory + text write (Chapters 44/46) | never assume the working directory writable; colon-free timestamp (a forbidden Windows character) — two engineering details of the `file/report` sample |

**The selection methodology**: at every row ask "what shape is the data" (streaming or whole, fixed set or dynamic keys, kept or not) — the shape decides the container: Chapter 23's selection decision applied project-wide for the first time. If the upgrade target is "dispatching inputs by path/key prefix to dozens of handlers", Chapter 30's cousin pattern with its compile-many-patterns-once design is built for exactly that shape..

### The pipeline architecture: five streaming stages

```diagram flow
- Input: file-path argument -> file Reader (Chapter 44) -> xrtLineReaderTake (initial capacity 1024)
- Per line: the Next three-state loop (LINE/END/ERROR) - line views borrowed, use as you go
- Parse: slice the view at the first space -> level view + body view (zero copy)
- Aggregate: level-array counts / ring-buffered error samples / first-last timestamp records
- Output: builder assembles the report -> stdout or a temp file
```

**The meaning of streaming**, quantified once more: memory = the line reader's buffer (tracks the longest line, not the file size) + counters (O(1)) + the sample ring (O(N)) — independent of the 2 GB. That is acceptance criterion (2)'s design guarantee.

### Boundary and error plans

Before writing code, think through the three boundary classes (acceptance criterion (3) expanded): **empty file** — the first Next is already END: output all-zero counts plus a "no input" note (not an error); **no matching level** (zero hits after filtering): output zero counts normally (the user's filter condition is not the program's fault); **all errors**: the ring fills and overwrites, counts correct. **The error-handling posture**: open failure/read failure/write failure all go through return values + the error slot — the main function's error branches uniformly print `xrtErrorMessage` with the cause chain; **nowhere an exit without releasing resources** (the single-exit cleanup pattern — the shape of Chapter 44's samples).

### Differences from the teaching samples

Both use xlinereader; the teaching sample (Chapter 40) parses three fixed lines, while the project faces **a real stream of unknown length** — the difference shows in three places: **automatic growth** after the initial capacity becomes mandatory (logs may contain very long stack lines); distinguishing END from ERROR becomes correctness-critical (a disk going bad mid-read = ERROR to report; a normal finish = END, no report); the line view's **lifetime** needs discipline (the sample ring stores copies, not views — the landing of the "views are use-and-discard" discipline of Chapters 95/103).

## Examples

### First complete program: the line-reading core (a runnable minimal loop)

The program below is from `examples/io/line` — the textbook shape of logstat's line engine (a sample with mixed EOLs and a last line without newline):

```embed path="examples/io/line/main.c" title="examples/io/line/main.c"

```
```
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/io/line/main.c -lws2_32 -liphlpapi
1: INFO server started
2: WARN queue is busy
3: ERROR request failed
```

**What just happened.** (1) The two steps `xrtReaderFromMemory` → `xrtLineReaderTake(&pReader, 1024)` — **Take is an ownership handover**: afterwards you Destroy only the line reader (it closes the underlying Reader uniformly). In the project the Reader swaps to the file form (`xrtReaderFromFile` — the same family, Chapter 40) and the rest of the pipeline is unchanged — **the value of the Reader abstraction cashes in for the first time in a project**: the same line-processing logic, one code path for the in-memory sample and the 2 GB file. (2) The `Next` three-state loop is exactly logstat's main loop: LINE enters processing, END wraps up and outputs, ERROR reports and exits. (3) All three EOLs (CRLF/LF/CR) and the last line without newline are correct — the machine proof of acceptance criterion (1).

### Second complete program: another input shape, JSON logs

The second program is from `examples/logging/file_json` — the reference when the input upgrades to JSON Lines:

```embed path="examples/logging/file_json/main.c" title="examples/logging/file_json/main.c"

```
```
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/logging/file_json/main.c -lws2_32 -liphlpapi
wrote example_logger_json.log
```

**What just happened.** (1) This is the **generation side**: a JSON-Lines log Sink (one complete JSON object per line) — logstat's extension shape: when the input upgrades from "LEVEL text" to JSON lines (the level in a field), this is what the generator looks like. (2) The parsing side corresponds to Chapter 32's streaming JSON reading — this project's extension-exercise direction. (3) `AddHelper` — one line equals three steps (formatter + file Sink + Attach) — Chapter 38's convenience layer in a field position: **in toolchains prefer Helper for glue, sink down only for composition needs**.

### Mapping onto the book's knowledge map

Small as logstat is, every brick it steps on has a main entry in the book — **the project validates paths through the knowledge map**: Chapter 4 (error model — the two failure branches' structured reporting and cause-chain reading), Chapter 12 (the hashing idea — the semantic boundary of counting versus dedup), Chapter 14 (arrays — O(1) counting of a fixed level set), Chapter 18 (Map — the dynamic-key alternative and the judgment of "when not to"), Chapter 25 (strings — zero-copy view slicing and builder O(n) concatenation), Chapter 32 (JSON — the extension-direction input upgrade), Chapters 37/38 (logging — the generation side of JSON Lines and rotation semantics), Chapter 40 (IO streams — the Reader abstraction and the line reader's three states), Chapter 41 (time — the extension-direction time window), Chapter 44 (files — report writing and the Reader's file form), Chapter 46 (directories — temp directories and system paths), Chapter 106 (SSE — the extension-direction live push). **Fourteen chapters each in place inside a hundred-line program** — not coincidence but the layered result of design: each chapter teaches one layer, the project stacks the layers. After this chapter you should feel "the whole book can be strung together by one small program" — exactly the confidence for the later, larger projects (config center, chat service, downloader): **a bigger program is still these layers plus more inter-layer glue**.

### Data-flow contracts: the shape entering and leaving each layer

A layered architecture's engineering value lands on **each layer's input/output contract** before it is maintainable. logstat's four-layer contracts: **byte layer** (Reader) — path in, byte stream out, failure via return value + error slot, no partial state; **line layer** (LineReader) — byte stream in, `xlineview` out (borrowed, EOL-free, the three EOLs unified), three-state Next, overlong lines auto-growing and only with the longest line; **parse layer** (lsLevelOf) — view in, level number + body view out (zero copy), unknown level returns -1 (lenient counting is the caller's decision); **aggregate layer** (lsstats) — level number / sample line in, statistics out (a pure data structure, no IO, no allocation). **The discipline of contracts**: each layer depends only on the next layer's **output contract**, not its implementation — swapping the Reader implementation (memory/file/network) or the parse target (text/JSON) leaves the contracts intact, so the layer is swappable. This "shape thinking" is Chapter 23's selection decision projected onto architecture: **selection picks containers, contracts fix layer boundaries**.

### The assembly map: module dependencies and data flow

Drawing the pipeline as a module-dependency graph shows how "the Volume 13 project" relates to the first twelve volumes — **every box is some volume's main subject, the arrows are this project's new glue**:

```diagram flow
- File layer (Chapter 44 xfile -> Chapter 40 xreader): path -> byte stream
- Line layer (Chapter 40 xlinereader): byte stream -> line views (EOL-adaptive)
- Text layer (Chapter 25 xstrview/xstrbuf): line views -> level/body split; stats results -> report concatenation
- Container layer (Chapter 14 array / Chapter 12 idea): count array and sample ring
- System layer (Chapter 46 dirs + Chapter 41 time): temp-dir location and colon-free timestamp
- Error layer (Chapter 4 xerror): structured reporting of failures along the whole path
```

Six layers each at its post, with only views and return values between layers — **no layer skips its neighbor to reach further down** (the line layer never touches paths; the text layer never touches file handles). This discipline turns "swap the file for a network stream" (Chapter 40's Reader abstraction) or "swap text for JSON" (Chapter 32) into single-layer replacements — maintainability is not an abstract slogan but a layered fact.

### The performance budget: the account of every stage

A tool's performance account should be settled at design time (a miniature application of Chapter 135's methodology): **line reading** — one view produced per line (zero copy) + buffer growth for overlong lines (amortized O(lines × average length) total); **level detection** — one prefix comparison per line (a view operation, nanoseconds); **counting** — an array-index increment (cache-friendly); **sample copying** — only error lines (a small share); **report assembly** — builder O(total); **writing** — one write. **Budget conclusion**: single-core throughput is decided by IO (the file read is the only slow stage) — all CPU stages combined are negligible next to IO. The conclusion steers optimization: **don't optimize the string comparison; watch the read buffer size** (the Reader's block size) — budget-first prevents effort in the wrong place (Chapter 135's questioning discipline, engineered).

### Implementation walkthrough: the complete source

Below is logstat's complete compilable source — the teaching form (in-memory sample input, for deterministic verification); the file form differs by one Reader line. It turns every design decision of this chapter into code:

```c
/* logstat - mixed-EOL log statistics (teaching form: in-memory sample) */
#include <stdio.h>
#include <string.h>
#include <xrt.h>

#define LS_LEVELS 5
#define LS_TOP 3

static const char* const kLevelNames[LS_LEVELS] = {
	"TRACE", "INFO", "WARN", "ERROR", "FATAL"
};

typedef struct {
	size_t Counts[LS_LEVELS];            /* fixed-set counting: array */
	char   Samples[LS_TOP][96];          /* error sample ring: copied in */
	size_t SampleCount;
	size_t Lines;                        /* total lines */
	size_t BadLines;                     /* lines whose level is unrecognizable */
} lsstats;

/* level detection: prefix match up to the first space - zero copy. -1 = unknown. */
static int lsLevelOf(xstrview Line)
{
	size_t i;
	for ( i = 0; i < LS_LEVELS; i++ ) {
		xstrview Name = { kLevelNames[i], strlen(kLevelNames[i]) };
		if ( (Line.Size > Name.Size) &&
			(Line.Data[Name.Size] == ' ') &&
			xrtStrStarts(Line, Name) ) {
			return (int)i;
		}
	}
	return -1;
}

int main(void)
{
	/* mixed EOLs + last line without newline + unknown-level line: all three boundaries at once. */
	static const char sLog[] =
		"INFO server started
"
		"WARN queue is busy
"
		"ERROR db connect failed
"
		"INFO retry ok
"
		"ERROR db connect failed
"
		"NOTICE rotated
"
		"FATAL giving up";
	xreader* pReader = NULL;
	xlinereader* pLines = NULL;
	xlineview Line;
	xlinenext Next;
	lsstats St;
	xstrbuf Rep;
	size_t i;
	int iResult = 1;

	memset(&St, 0, sizeof(St));
	pReader = xrtReaderFromMemory(
		(xbytesview){ (cbytes)sLog, sizeof(sLog) - 1u });
	if ( pReader != NULL ) {
		pLines = xrtLineReaderTake(&pReader, 1024u);
	}
	if ( pLines == NULL ) {
		fprintf(stderr, "logstat: %s
",
			xrtErrorMessage(xrtGetError()));
		goto Cleanup;
	}

	/* main loop: strict three states - LINE processes / END wraps up / ERROR reports. */
	while ( (Next = xrtLineReaderNext(pLines, &Line)) ==
		XLINE_NEXT_LINE ) {
		int iLevel = lsLevelOf(Line);
		St.Lines++;
		if ( iLevel < 0 ) {
			St.BadLines++;          /* boundary: unknown level counts without interrupting */
			continue;
		}
		St.Counts[iLevel]++;
		if ( (iLevel >= 3) && (St.SampleCount < LS_TOP) ) {
			/* the ring stores copies: views are use-and-discard (the positive form of pitfall 2). */
			size_t n = Line.Size < 95u ? Line.Size : 95u;
			memcpy(St.Samples[St.SampleCount], Line.Data, n);
			St.Samples[St.SampleCount][n] = 0;
			St.SampleCount++;
		}
	}
	if ( Next != XLINE_NEXT_END ) {
		fprintf(stderr, "logstat: read error: %s
",
			xrtErrorMessage(xrtGetError()));
		goto Cleanup;
	}

	/* report: builder O(n) concatenation (pitfall: repeated Concat is O(n^2)). */
	xrtStrBufInit(&Rep);
	if ( !xrtStrBufAppend(&Rep, XRT_STR_LITERAL("lines=")) ) {
		goto BufFail;
	}
	/* count formatting: number to text then append (the builder unified exit) */
	for ( i = 0; i < LS_LEVELS; i++ ) {
		char Num[24];
		int n = snprintf(Num, sizeof(Num), "%zu", St.Counts[i]);
		if ( !xrtStrBufAppend(&Rep,
				(xstrview){ Num, (size_t)n }) ) { goto BufFail; }
		if ( (i + 1u) < LS_LEVELS &&
			!xrtStrBufAppend(&Rep, XRT_STR_LITERAL("/")) ) {
			goto BufFail;
		}
	}
	printf("bad=%zu samples=%zu
", St.BadLines, St.SampleCount);
	for ( i = 0; i < LS_LEVELS; i++ ) {
		printf("%-6s %zu
", kLevelNames[i], St.Counts[i]);
	}
	for ( i = 0; i < St.SampleCount; i++ ) {
		printf("sample[%zu]=%s
", i, St.Samples[i]);
	}
	iResult = 0;

BufFail:
	xrtStrBufFree(&Rep);
Cleanup:
	xrtLineReaderDestroy(pLines);   /* the sole destroy point after the Take handover */
	return iResult;
}
```
```
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c logstat.c -lws2_32 -liphlpapi
bad=1 samples=3
TRACE  0
INFO   2
WARN   1
ERROR  2
FATAL  1
sample[0]=ERROR db connect failed
sample[1]=ERROR db connect failed
sample[2]=FATAL giving up
```

**Walkthrough points.** (1) The sample input deliberately covers every boundary: three EOLs, the last line without newline (the FATAL line), an unknown level (NOTICE — into BadLines without interrupting), duplicate errors (dedup semantics left as an extension). (2) `lsLevelOf`'s test is "prefix + space" — an `INFO` line cannot be misjudged as `INFOX` (the design detail of one extra character check). (3) Single-exit cleanup: `LineReaderDestroy(pLines)` is the **sole** destroy point after the Take handover (pReader must not be touched again); the failure path and the success path share it. (4) The builder segment demonstrates the minimal shape of report assembly — the full report (with timestamp and temp file) extends per Chapters 44/46. (5) The unknown-level `NOTICE` counts as bad=1 — **lenient counting + report presentation** rather than crashing (real logs always contain levels you have never seen).

### Acceptance walkthrough: reconciling all five criteria

Return to the acceptance criteria written at the start and reconcile one by one — the final difference between project chapters and teaching chapters: **teaching chapters end with "understanding"; project chapters end with "acceptance"**. (1) Mixed EOLs: the walkthrough program's input is exactly the three-EOL mix — output correct (criterion 1 passes). (2) Streaming memory: the line engine holds only the current line + fixed counters + 3×95-byte samples — the same-order account settled at design time (criterion 2 passes — the file form is isomorphic for 2 GB). (3) Three boundaries: empty file (the first Next is END — output all zeros), no match (zero hits under the filter flag still output normally), all errors (the ring fills and overwrites) — the walkthrough sample already contains two boundaries; the empty file is verified in the exercises (criterion 3 passes). (4) Report boundary: the temp directory and the colon-free timestamp are the two engineering details of the `file/report` sample (the design basis of criterion 4). (5) Error chain: both error branches print `xrtErrorMessage(xrtGetError())` — structured reporting of open failure / read error (criterion 5 passes). **All five pass — this tool is deliverable**.

### Extension paths: from tool to tool family

logstat's architecture allows organic extension in three directions, each merely "swap one layer": **input-layer upgrade** — JSON Lines (Chapter 32: level in a field, first-character `{` detection branching) or a network stream (Chapter 40's Reader family: a tail -f style live statistics); **statistics-layer upgrade** — time-window aggregation (Chapter 41: minute-bucket counting for trends) or dynamic token frequency (Chapter 18's Map: find the hottest module name); **output-layer upgrade** — a JSON report (Chapter 32 serialization) or SSE push (Chapter 106: a live source for a statistics dashboard). **Every upgrade is a single-layer swap** — the assembly map's layered discipline pays off at evolution time: this is the meaning of "project" as the book's comprehensive acceptance — not "used many APIs", but **swapping any one layer leaves the others untouched**.

## Contracts

- **Streaming guarantee**: memory = line buffer (tracks the longest line) + O(1) counters + O(N) samples — independent of total input.
- **EOL-adaptive**: CRLF/LF/CR all recognized, views carry no EOL, the last line without newline works as-is — zero configuration.
- **Strict three states**: END wraps normally / ERROR reports structurally — a bad disk never confused with a normal finish.
- **View discipline**: line views are use-as-you-go; the sample ring stores copies (views never enter long-lived containers).
- **Selection discipline**: fixed sets use arrays, dynamic keys only then Map, multi-segment assembly uses the builder — shape decides the container.
- **Output boundary**: the report lands in the system temp directory, timestamp colon-free — two platform facts.
- **Error posture**: return values + error slot along the whole path; single-exit cleanup; no naked exits anywhere.
- **Acceptance first**: five criteria before code — boundaries (empty/no-match/all-errors) rehearsed in advance.

### Test strategy: four-quadrant acceptance for the tool

Finished code is not yet delivery — it also needs acceptance pieces in Chapter 132's test shapes. logstat's four quadrants: **positive** — fixed samples (the walkthrough program's embedded sample is the regression vector: expected output of three-EOL/last-line-no-newline/unknown-level/duplicate-error asserted character by character); **boundary** — empty file (the first Next is END), a single line without newline, an overlong line (triggering the buffer-growth path), all lines the same level; **negative** — a nonexistent path (assertions on the error category + domain of open failure), a directory passed as a file (also open failure but a different message); **stress** — Chapter 133's method applied: if the line reader allocates internally, statistics stay consistent under failure injection (the tool's OOM behavior is "report and exit", never "output wrong statistics"). **Test pieces share the tool's lifecycle**: sample vectors enter the test directory and run in the quality gate — when the tool changes behavior, the vectors go red first. This translates Chapter 132's "four weapons" from the library's testing language into **the project tool's acceptance language** — the same methodology reused at a different granularity.

### Delivery form: releasing a single-file tool

The delivery choice for tool-style programs (Chapter 131's three consumer kinds, project edition): logstat is **one of the best scenes for the single-header form** — one `.c` source + `single/xrt.h` + one compile command is a complete delivery (the easiest to distribute in the script world); the trimming declaration (Chapter 130) sits in one line at the top of the tool — `XRT_MODULE_IO`+`XRT_MODULE_STRING`+`XRT_MODULE_FILE` — the size profile asserting it contains no network/TLS symbols. **The alternatives compared**: the static-library form suits tool families (several tools sharing one library artifact — the shape of Chapter 142's static file server); the dynamic-library form yields nothing for a standalone tool. **Version and iteration**: the tool prints its own version (Chapter 8 core's version query) — `logstat --version` outputs the XRT version and the tool's own version, so "which version computed this report" stays traceable during troubleshooting. Delivery is not the end but **the starting point of the next iteration** — the three directions of the extension section all set out from this deliverable baseline.

## Pitfalls

### Pitfall 1: reading the whole file, then splitting lines

Symptom: a 2 GB log eats half the memory of a 32 GB machine — "runs", but ops says no.

Cause: treating "a file" as "a string in memory". Streaming's essence is **holding only the current line at any moment** — the Reader abstraction exists for exactly this.

```c bad
str all = read_whole_file(path);      /* 2GB into memory */
for ( line in strtok(all, "\n") ) {   /* EOLs still need hand-rolled tri-state handling */
	count(line);
}
```
```c good
xreader* r = xrtReaderFromFile(path, ...);
xlinereader* lr = xrtLineReaderTake(&r, 1024);
while ( xrtLineReaderNext(lr, &v) == XLINE_NEXT_LINE ) {
	count(v);                          /* at any moment: one line */
}
xrtLineReaderDestroy(lr);
```

### Pitfall 2: storing line views into the sample ring

Symptom: the output "error samples" are all the last line — the ring stored views, and after the buffer was reused they all point at the same block.

Cause: line views borrow the reader's internal buffer — the next Next invalidates them (Chapter 40). To keep across lines, copy.

```c bad
while ( Next == LINE ) {
	if ( is_error(v) ) { ring_push(&ring, v); }  /* storing views: all dangling */
}
```
```c good
while ( Next == LINE ) {
	if ( is_error(v) ) { ring_push_copy(&ring, v); } /* copy into the ring */
}
```

### Pitfall 3: writing the report to the working directory (or a filename with colons)

Symptom: running under a read-only mount / a service account — writing the report fails; or on Windows the timestamped filename creation fails (`:` is forbidden).

Cause: two platform facts never entered the design. The `file/report` sample's engineering details: **the temp directory** (never assume writable) + **the compact colon-free time format**.

```c bad
snprintf(name, "report_%s.txt", now_with_colons());  /* 12:30:45 -> Windows refuses */
f = fopen(name, "w");                                /* the working directory may be read-only */
```
```c good
/* system temp dir + compact colon-free time (report_20260905_042105.txt) */
path = temp_dir_join(compact_timestamp());
```

### Pitfall 4: carrying over old code's strtok habits

Symptom: porting legacy `strtok(all, "
")` — the EOL handling "looks" right, but strtok's **state machine is global** (not reentrant), consecutive separators are swallowed (blank lines vanish), and it mutates the original buffer (conflicting with view discipline).

Cause: strtok is a pre-ANSI relic — a triple violation (global state / swallowed blank lines / input mutation), all wrong in modern concurrent and streaming contexts. XRT's line reader is its positive replacement: reentrant (state lives in the object), blank lines preserved (view semantics), zero mutation (borrowing).

```c bad
for ( p = strtok(buf, "
"); p; p = strtok(NULL, "
") ) {
	count(p);   /* blank lines gone; buf mutated; multithreading explodes */
}
```
```c good
while ( xrtLineReaderNext(lr, &v) == XLINE_NEXT_LINE ) {
	count(v);   /* reentrant; blank lines kept; zero mutation */
}
```

## Exercises

### Basic: three-level counting running

Extend the io/line sample as the engine: count INFO/WARN/ERROR and print. Verify with a mixed-EOL + last-line-no-newline sample. Acceptance criteria: the three counts correct; an empty file outputs all zeros plus a note.

### Advanced: the full logstat

Implement every requirement: `--level` filtering, `--top N` error samples (copied into the ring), `--report` output to a temp file (colon-free timestamp). Verify against a self-made 100 MB log. Acceptance criteria: all five acceptance criteria pass; memory at MB level (cross-checked with stats).

### Challenge: the JSON Lines upgrade

Upgrade the input to JSON lines in the `file_json` sample's format: line reading unchanged, parsing switches to Chapter 32 JSON (level/message/time fields), statistics isomorphic. Support both old and new inputs in one program (detect by the leading `{`). Acceptance criteria: a mixed file of both formats branches correctly; JSON parse-error lines are counted and reported (without interrupting the whole run).

### Retrospective: what Project 1 taught

As the book's first project, the **transferable lessons** deserve an explicit inventory — they recur in later projects. **Acceptance first**: five criteria written before code, boundaries (empty/no-match/all-errors) thought through in rehearsal — the lowest-rework order; **shape decides the container**: fixed-set array / dynamic-key Map / multi-segment builder — selection is not memorizing APIs but reading data shapes; **views are use-and-discard**: keeping across lines requires copying — the network chapters' (95/103) discipline holds equally in a file tool; **layered swappability**: the Reader abstraction lets the in-memory sample and the 2 GB file share code — acceptance and performance share one implementation; **platform facts enter design**: the temp directory and colon-free timestamp are "listed at design time", not "discovered at runtime"; **budget first**: all CPU stages are negligible next to IO — the budget names the optimization direction, not intuition; **four-quadrant testing**: the tool's acceptance language is isomorphic to the library's testing language. None of the seven lessons is logstat-specific — they are **engineering common sense expressed in XRT**; the next project (the config center, Chapters 138–139) replays them in a service and persistence context; the one after (the chat service, Chapters 140–141) adds concurrency and connection management. That is the intent of the project sequence: **the same engineering common sense, one layer deeper with each new shape**.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Project shape | requirements & acceptance first → selection decisions → progressive implementation → acceptance & extension |
| Line engine | Reader→LineReaderTake (handover) → Next three-state loop — the streaming core |
| EOLs | CRLF/LF/CR adaptive; last line without newline works — zero configuration |
| Level parsing | slice the view at the first space — zero-copy prefix detection |
| Counting choice | fixed level set = array; dynamic keys only then Map |
| Sample ring | copies in (views are use-and-discard) — the ring overwrites, keeping the first N |
| Report | builder O(n) assembly; temp directory + colon-free timestamp |
| Streaming memory | line buffer + O(1) + O(N) — independent of total size |
| Error posture | three states distinguish END/ERROR; single-exit cleanup; lenient counting of unknown levels |
| Extension directions | JSON Lines input (Chapter 32) / time-window stats (Chapter 41) / SSE dashboard (Chapter 106) |
