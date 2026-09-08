---
num: 49
slug: debug-pipeline
title: Debugging and Diagnostics Composition: Logging, Statistics, and Fault Injection
volume: 卷五 系统服务 · 卷五收官
type: composition
lead: Stringing the observation trio into a battle pipeline — fault injection manufactures extremes, statistics locate magnitudes, logging reconstructs the scene.
api: memory_debug, memory_stats, logger, error
---

## Orientation

Volume 5's closing chapter (composition type) teaches no new module — it strings the observation tools scattered across three chapters into a **debugging battle pipeline**: **fault injection** (Chapter 6) actively manufactures extreme conditions (OOM, failure paths), **memory statistics** (Chapter 6) supply the numeric evidence of magnitude and trend, and **logging** (Chapters 37/38) reconstructs the scene and the causal chain. Each has its post: injection makes the "illness", statistics locate "which layer it is in", logging answers "what happened then". The composition chapter's thesis: **debugging capability is not a pile of tools but the collaborative design between tools** — where to place injection points, how to window the statistics, which fields the log records: these "how to string" decisions deserve more design than the tools themselves.

## Introduction

A real troubleshooting scene: a service in production leaks memory upward intermittently, restarted weekly to stay alive. Without an observability system, the path is: guess ("maybe connections aren't closed?") → add printf → restart and watch → guess again — looping for weeks. With the battle pipeline: first open a **statistics window** (before/after snapshot comparison confirms "what magnitude, what type is growing") → after locking the suspicious path, enable the **debug heap** (VisitLive finds unreleased allocations and their birthplaces) → incidentally run **fault injection** to verify failure paths (inject OOM and watch rollback complete — leaks and incomplete failure paths are frequent companions) → the **log's** levels and fields were deployed structured from the start, the timeline directly readable. Same problem, weeks become hours — the gap isn't cleverness but the pipeline's three links feeding each other.

The second scene is preventive: a newly written resource-management module needs acceptance. Injection scans every allocation failure point (Chapter 6's OOM skeleton), statistics verify live counts reaching zero, logging records every failure path's error chain — **the pre-release checkup** and **the post-mortem autopsy** use the same pipeline, only in different order (checkup: inject first, observe after; autopsy: observe first, inject after).

## Concepts

### The battle pipeline panorama

```diagram flow
- Link 1 statistics: open window (Reset) -> run load -> Get readings - "reconnaissance" of magnitude and trend
- Link 2 debug heap: Enabled -> reproduce -> VisitLive finds unreleased allocations and birthplaces - "localization"
- Link 3 injection: FailAfter(N) manufactures extremes -> assert rollback complete -> scan all N - "stress test"
- Link 4 logging: structured fields (correlation ID/error chain/magnitude) throughout - "timeline"
- Loop: each link's output is the next link's input (statistics find magnitude -> debugging locates source -> injection verifies hypothesis -> logging confirms causality)
```

### Injection-point selection: not scanning everything

The brute-force answer to fault injection is "inject every allocation point" (Chapter 6's exercise approach — correct but slow). Production-grade selection strategy: **boundaries first** — the failure rollback of resource-acquisition points (open/allocate/connect) is the most complex, scan them first; **new code first** — new code's failure paths have never been production-verified, must-scan before release; **changed code first** — refactored paths rescanned. Three priorities spend the scanning budget (time) on the blade — Chapter 26's "boundary values before coverage" testing view, projected onto the resource dimension.

### The joint design of statistics windows and log fields

The observability tools' **field linkage** is the composition-design core lesson. The statistics window's boundaries (the two instants Reset/Get) should align with log timestamps — "what happened inside the window" is answered by the log's timeline; the log's **structured fields** should include the dimensions statistics care about — every request log carrying an allocation-magnitude field (`alloc_bytes`), so a statistical anomaly can be back-traced from the logs to which request class contributed; an injection test's assertion output should go through the (structured) log, not printf — test reports become searchable and comparable. The three tools' field design is **planned together before hands-on work** — this is why "composition chapters" exist: single chapters teach you to use tools; composition chapters teach you to design their interfaces.

### The debug heap's ethics: overhead and timing

The debug heap's (memory_debug) detailed records carry real overhead — leaving it on in production long-term is a luxury. Timing strategy: **open on demand** (on alert or during investigation, open a window), **reproduce inside the window** (run the triggering load with the heap on), **close promptly** (once evidence is taken, close it; don't let it pollute performance measurement). The statistics module is the opposite — nearly zero overhead, production-always-on (the trend-monitoring sentinel). The two cooperate: statistics always-on as the "is there a problem" sentinel, the debug heap on demand as the "where is the problem" magnifying glass — **the cheap always on, the expensive on demand**: the economics of observability.

### The error chain's place in diagnosis

Chapter 4's error chain (cause chain) has a dedicated place in the diagnostics pipeline: the log's **error field** (`xrtLogFieldError`) carries the complete chain — `资源释放失败 → 底层 IO 错误 → 系统错误码` (resource release failed → underlying IO error → system error code) — a three-link chain seen in one view. When troubleshooting, the "surface error" (release failed) and the "root cause" (disk full) live in one object — no assembling causality across log lines. **The error chain's layering design** (Chapter 4's "boundary wrapping" discipline) directly determines the log's diagnostic value — wrap well, and one log line sees the whole case; wrap poorly, and three lines can't assemble the causality.

## Examples

### Complete program: a three-link checkup

The checkup pipeline's minimal implementation — statistics window, failure injection, log records, three links in one pass:

```c
/* health_check.c - pre-release checkup: injection -> statistics -> logging in concert */
#define XRT_MODULE_ALL
#define XRT_IMPLEMENTATION
#include <xrt.h>
#include <stdio.h>

static xlogger* gLog;

static bool businessRun(void)
{
	/* business under test: allocate two blocks; the normal path frees one, the failure path must free all */
	ptr pA = xrtMalloc(64);
	ptr pB = xrtMalloc(128);
	bool bOk = false;

	if ( (pA != NULL) && (pB != NULL) ) {
		bOk = true;
	}
	xrtFree(pA);
	xrtFree(pB);
	return bOk;
}

int main(void)
{
	xmemstats Stats;
	int iPass = 0;
	int iFail = 0;

	gLog = xrtLogCreate(XRT_STR_LITERAL("health"), XLOG_DEBUG);
	if ( gLog == NULL ) {
		return 1;
	}

	/* link 1: open the statistics window */
	xrtMemStatsEnable();
	xrtMemStatsReset();

	/* link 2: injection scan (first N succeed, the N+1-th fails) */
	for ( uint64 N = 0; N < 3; ++N ) {
		xrtMemDebugEnable(true);
		xrtMemDebugFailAfter(N);
		if ( businessRun() ) {
			++iPass;
		} else {
			++iFail;
			/* link 4: failures go through structured logs (error chain along) */
			xrtLog(gLog, XLOG_WARN, XRT_STR_LITERAL("inject point failed"));
		}
		xrtMemDebugFailClear();
		xrtMemDebugEnable(false);
	}

	/* link 3: window reading - live should be zero (failure-path rollback complete) */
	xrtMemStatsGet(&Stats);
	printf("pass=%d fail=%d live=%llu\n",
		iPass, iFail,
		(unsigned long long)(Stats.MallocBytes - Stats.FreeBytes));
	xrtLogFree(gLog);
	return (Stats.MallocBytes - Stats.FreeBytes) == 0 ? 0 : 1;
}
```

```term
$ gcc -O2 -DXRT_MODULE_ALL -DXRT_IMPLEMENTATION -I single health_check.c -lws2_32 -liphlpapi
$ ./a.exe
pass=1 fail=2 live=0
（三个注入点：首点通过、后两点按预期触发失败路径；live=0 证明失败路径回滚完整——零泄漏）
```

**Where this code stands.** The three links in compact union: statistics' Reset/Get frame the measurement window (before and after the business run); injection's FailAfter(N) loop scans the first three allocation points (businessRun happens to allocate twice — the third injection point has nothing to hit and passes naturally); the log leaves a structured record on the failure branch (in real projects this carries the error-chain field). `live=0` is the whole program's core assertion: **under any injection point, after the business fails, all resources are returned** — the machine testimony of "failure-path rollback complete". Swap this skeleton's business function for yours, and you have a module-level pre-release checkup.

### Complete program: a leak localizer

The autopsy pipeline — after statistics spot the anomaly, the debug heap locates the leak's source:

```c
/* leak_hunt.c - VisitLive finds unreleased allocations and their birthplaces */
#define XRT_MODULE_ALL
#define XRT_IMPLEMENTATION
#include <xrt.h>
#include <stdio.h>

static bool reportLeak(const xmemdebugallocation* pAlloc, ptr pData)
{
	size_t* pCount = (size_t*)pData;

	++(*pCount);
	printf("leak: %zu bytes at %s:%d\n",
		pAlloc->Size, pAlloc->File, (unsigned)pAlloc->Line);
	return true;   /* keep walking */
}

int main(void)
{
	size_t iLeaks = 0;

	xrtMemDebugEnable(true);

	/* simulated business: one block deliberately not freed */
	(void)xrtMalloc(256);

	iLeaks = xrtMemDebugVisitLive(reportLeak, &iLeaks);
	printf("total leaks=%zu\n", iLeaks);
	xrtMemDebugEnable(false);
	return 0;
}
```

```term
$ gcc -O2 -DXRT_MODULE_ALL -DXRT_IMPLEMENTATION -I single leak_hunt.c -lws2_32 -liphlpapi
$ ./a.exe
leak: 256 bytes at leak_hunt.c:20
total leaks=1
```

**Where this code stands.** The autopsy's core link: `xrtMemDebugVisitLive` walks the allocations **still live** — each with size and **birthplace** (file:line, credit to Chapter 5's At family); the callback prints the list and counts. In real use, replace the "simulated business" with the reproducing load, and the leak list is the repair roadmap. Cooperation with the checkup pipeline: statistics find `live` not reaching zero (sentinel sounds) → enable the debug heap and reproduce (magnifying glass) → VisitLive produces the list (evidence chain) → after the fix, return to the checkup (verification). **The two programs together form the complete prevent-diagnose-treat loop** — the pre-release checkup blocks new illness, the runtime sentinel reports anomalies, on-demand localization produces the list.

### Four checkpoints of link design (Chapter 35's observability edition)

Chapter 35 set four checkpoints for data pipelines; the observability pipeline has its mirror edition. **Checkpoint 1: is the observation question explicit** — each tool answers one clear question (statistics answer "whether/how much", debugging answers "where", injection answers "what under extremes", logging answers "when/why") — no tool answers two questions, and a need for two observations shouldn't be crammed into one. **Checkpoint 2: are fields planned** — window boundaries, correlation IDs, magnitude fields, error chains unified at design time (Pitfall 1's enforcement target). **Checkpoint 3: is overhead paid in the right place** — sentinel always on, magnifier on demand (Pitfall 2's enforcement target). **Checkpoint 4: can assertions be mechanized** — the checkup's conclusions (live=0, the injection-point list) are grep-able structured output, not reports for human eyes. With the four passed, the observability system is "operable", not merely "runnable".

### Looking back at Volume 5 from the composition chapter: the observability face of system services

As Volume 5 closes, looking back reveals the debugging pipeline as the whole volume condensed: the two logging chapters gave the timeline and structured fields, Chapter 6 (Volume 1) gave injection and statistics, Chapter 4 gave the causal chain, the file family gave the carrier for "window snapshots to disk for diffing" (writing statistics-window data to files for before/after comparison), and async and signals gave the "observe without stopping service" deployment form (reload observability config via the SIGHUP path). **Every module of system services supplies ammunition for observability** — this is the concrete meaning of "observability is not a module but an architectural property". With this lens, revisit Volume 5's chapters' contracts: at least half the clauses serve diagnosis.

### A work view: observation is paid insurance

Investment in observability is like insurance: pure cost when nothing happens (overhead, field-design time, checkup-script maintenance), total payoff when something does (hour-level localization versus week-level guessing). Three underwriting suggestions: **buy the basic policy** — statistics sentinel always on + structured logs with correlation IDs, lowest cost, widest coverage; **extra coverage for disaster zones** — resource-intensive modules (pools/connections/caches) get checkup scripts and leak-localization drills; **rehearse the claims process** — the alert-triggered enable-reproduce-evidence path is rehearsed quarterly (unrehearsed insurance equals uninsured). The insurance view lifts observation from "technical decision" to "risk decision" — when budget tightens, which coverage to cut and which to keep answers like actuarial science.

## Contracts

- **Pipeline order**: reconnaissance (statistics) → localization (debug heap) → verification (injection) → timeline (logging) — each link's output is the next link's input.
- **Injection budget**: boundaries first, new code first, changed code first — scanning spent on the blade, not full traversal.
- **Field linkage**: window boundaries aligned to log timestamps, logs carry statistics-dimension fields, assertion output goes through structured logs — planned together before hands-on work.
- **Observability economics**: statistics always on (sentinel), debug heap on demand (magnifier) — the cheap always on, the expensive on demand.
- **Error chain position**: the log's error field carries the full cause chain — wrapping depth determines diagnostic value (Chapter 4's boundary discipline).
- **Checkup and autopsy are isomorphic**: one pipeline, two orders — nothing new to learn between pre-release and runtime.

### Evolution path: building observability from nothing

An observability system isn't built in a day; a four-level evolution with distinct returns. **Level 1 (usable)**: printf swapped for structured logs + statistics sentinel always on — a week of work, covering "when something happens, at least there's a timeline". **Level 2 (diagnosable)**: correlation IDs in all log fields + error chains wrapped to convention + statistics windows routinized — the timeline can assemble causality. **Level 3 (verifiable)**: core modules get pre-release checkups (injection scan + zero-live assertion) + leak-localization drills — new illness blocked before release. **Level 4 (self-healing)**: alert-driven automatic window-opening and evidence collection + hot observability-config reload (the SIGHUP path) + checkup regression in CI — observation itself becomes part of the service. Most teams stop at level one-and-a-half; level three is the cost-effectiveness inflection (the payoff of blocking new illness starts exceeding the investment); level four suits scaled services. Which level is your team on, and what is the next milestone — a question the technical lead should answer.

### Station close-up: the practical shape of injection scanning

The "scan one by one" of Chapter 6's OOM skeleton deserves a full unfolding in practice. **Scope measurement**: first run a statistics window to get the allocation total (say 200), then trim the scan budget by the injection priorities (boundaries first, the first 50 resource-acquisition points). **The assertion trio**: under each injection point — the failure happened as expected (injection effective), the error chain complete (log checkable), live count zero (rollback complete). **False-positive screening**: some "failures" are normal business degradation (retry succeeded) — assertions must distinguish "failure handled correctly" from "failure swallowed wrongly"; the former passes, the latter fails. **Artifact archiving**: scan results written to file (injection-point list + trio results), compared on regression — Chapter 45's atomic delivery keeps the archive itself intact. This shape upgrades "injection" from a toy into a regression-able test asset.

### Station close-up: the diagnostic dictionary of log fields

Structured logging's field design is the observability system's "data model"; a diagnostic dictionary closes this out: **correlation fields** — request_id (full request path), trace_id (cross-service), session_id (user session) — the primary keys of timeline assembly; **magnitude fields** — alloc_bytes/duration_us/count — the back-trace dimensions for statistical anomalies; **state fields** — level, error (the error chain), result — the statistical source of distributions and failure rates; **context fields** — module, source (code location), endpoint — the landing points of localization. Field names are stable (logs are downstream dependencies; renaming is a breaking change); the field set is versioned (new fields may be added, old fields never change meaning); the dictionary itself goes into documentation — new members can read the logs before using the timeline. This dictionary shares the spirit of Chapter 32's JSON Schema: **the data model precedes data accumulation**.

### Three high-frequency diagnostic scripts

Three scripts land the pipeline on concrete symptoms. **Script one: memory creeping upward** — statistics sentinel confirms (window comparison: is the growth pooled or direct) → investigate by growth type (pooled growth: check pool leaks; direct growth: check bare allocations) → debug heap opens a window and reproduces → VisitLive produces the list → after the fix, injection regression. **Script two: intermittent irregular failures** — filter logs by error category (Chapter 37's level strategy) → correlation ID assembles the failed request's full timeline → localize the shared state → inject concurrent pressure to reproduce (race class) or inject IO failure to reproduce (environment class). **Script three: crash at startup** — inject the earliest allocation points (startup-path OOM is often untested) → read the first link of the error chain → after the fix, scan the whole startup path. The three scripts share one structure: **symptom → pipeline entrance choice → link order → evidence form** — copy them into the team's troubleshooting manual, and the new on-call member's first-night panic at least halves.

### An anti-pattern warning: observability-driven design

A final warning balances this chapter's stance: observability matters, but **observation cannot replace design**. The anti-pattern's shape: sloppy error handling ("the log will show it anyway"), loose resource discipline ("statistics will find it anyway"), skipped failure paths ("injection will catch it anyway") — treating observation as a compensator for design flaws. The correct division: **design guarantees correctness; observation verifies correctness** — only when Chapter 4's error chain is wrapped per the boundary discipline can the log hold a readable chain; only when Chapter 5's ownership is balanced can statistics reach zero; only when failure paths are written complete can injection pass. The observability system's value stands on design quality — the foundation is design, the insurance is observation, and the order cannot be reversed. Leave Volume 5 with this warning, and in Volume 6 you will meet it in stronger form: concurrency correctness comes from design (lock protocols/message passing); observation can only find violations.

## Pitfalls

### Pitfall 1: each observability tool fighting its own war

Symptom: statistics show an anomaly but the logs have no records for that window; injection results scattered in printf, unalignable with the log timeline; every troubleshooting session "improvises" the tool combination on the spot.

Cause: tools deployed per single chapter, fields and windows never designed in linkage — the three instruments each recording in their own language.

```c bad
/* statistics window opened casually, injection results printf'd, logs patched in when remembered */
xrtMemStatsReset();
RunLoad();
printf("done\n");                    /* injection/results via printf - outside the timeline */
```

```c good
/* window boundaries, log fields, and assertion output aligned at design time */
xrtLog(gLog, XLOG_INFO, XRT_STR_LITERAL("window start"));   /* window boundary into the log */
xrtMemStatsReset();
RunLoad();
xrtMemStatsGet(&Stats);
xrtLogFieldInt(gLog, XLOG_INFO, XRT_STR_LITERAL("live_bytes"),
	(int64)(Stats.MallocBytes - Stats.FreeBytes));   /* readings into structured logs - timeline checkable */
```

### Pitfall 2: the debug heap always on in production

Symptom: overall service performance down twenty percent; performance optimization goes slower the more you tune (the tuning target is the debug heap's overhead, not the business).

Cause: using the "magnifying glass" as the "sentinel" — detailed-record costs paid long-term; the correct economics is statistics always on, debugging on demand.

```c bad
int main(void)
{
	xrtMemDebugEnable(true);   /* on from startup - paying detail overhead the whole run */
	/* ... months of production ... */
}
```

```c good
int main(void)
{
	xrtMemStatsEnable();        /* sentinel always on: near-zero overhead */
	/* ... production ... */
}
void onMemoryAlert(void)        /* when the alert triggers */
{
	xrtMemDebugEnable(true);    /* magnifier on demand: open window, reproduce, collect */
	RunRepro();
	xrtMemDebugVisitLive(...);  /* close right after evidence */
	xrtMemDebugEnable(false);
}
```

### Volume 5 closes: from tools to capability

This chapter closes Volume 5 and is the composition chapter's second appearance (Chapter 35 was the data-processing domain). What it wants to leave behind is not "how to use three tools" (single chapters taught that) but **the assembly capability of stringing tools into a system** — the design awareness of field linkage, the overhead allocation of observability economics, the isomorphic reuse of checkup and autopsy, the insurance-style investment decision. These "how to string" capabilities will be named again in Volume 12 (Chapter 132's testing system) and Volume 13 (real-project operational acceptance) — and looking back then with this chapter's pipeline thinking, you will find that testing and operations are the pipeline's two extensions: forward into the release process (checkup), backward into the running scene (autopsy). The next volume enters concurrency — and the observability system is precisely the concurrent program's "night vision": reproducing races, locating deadlocks, measuring throughput — all rely on this volume's pipeline operating in a concurrent context.

### A preview: observation is harder in the concurrent world

One fact stated up front: observation in the concurrent world is harder than this volume describes. Statistics windows under concurrency must account for "allocations in flight"; injection-point scans under multithreading must prevent one thread's injection from disturbing another thread's assertions; log ordering interleaves under multithreading (timestamps need microsecond precision to reconstruct the true order). Volume 6 will re-invoke this volume's tools in the concurrent context — Chapter 54's thread-pool statistics, Chapter 56's channel backpressure observation, Chapter 59's task-group cancellation auditing — each invocation annotated with "differences from single-threaded usage". **This volume's pipeline is the foundation; Volume 6 adds concurrency corrections** — the house on a weak foundation is the first to fall in a concurrency storm.

## Exercises

### Basic: reproduce the three-link skeleton

Reproduce the checkup sample: scan three injection points, verify live-zero in the statistics window, leave a log on the failure branch — replace businessRun with your own "two allocations, one may fail" function.

### Advanced: a window-log aligner

For any load, run "open window - load - read" three steps, with window boundaries and readings all into structured logs (timestamp + fields); run three rounds and reconstruct each round's window and readings from the log timeline — field linkage's minimal proof.

### Challenge: a module pre-release checker

Build a pre-release checkup for Chapter 22's pool module: injection-scan all allocation points of pool create/acquire-release/destroy (budget per "boundaries first"), statistics verify live-zero under any injection, every failure path's error chain into the logs. Acceptance criteria: the checkup report (structured log) directly greps into the failed-injection-point list; the checkup script rerunnable (for regression); zero false positives during the checkup.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Pipeline order | statistics reconnoiter → debug localize → inject verify → log timeline - each link feeds the next |
| Injection budget | boundaries/new/changed, three priorities - not full traversal |
| Field linkage | windows aligned to timestamps, logs carry statistics dimensions, assertions structured - unified at design time |
| Observability economics | statistics sentinel always on, debug heap magnifier on demand - cheap always on, expensive on demand |
| Error chain | the log's error field carries the full chain - Chapter 4's wrapping depth determines diagnostic value |
| Checkup/autopsy | one pipeline, two orders - block new illness pre-release, diagnose old illness at runtime |
| Zero-leak testimony | live=0 under any injection point - machine proof of complete failure-path rollback |
