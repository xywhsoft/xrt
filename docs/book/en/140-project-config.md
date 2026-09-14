---
num: 140
slug: project-config
title: The Config Center: Implementation
volume: 卷十三 实战项目
type: project
lead: The seven-layer pipeline landed layer by layer, merge semantics locked by test vectors, the atomic handover of immutable snapshots, extractor arrays and path-qualified validation — turning the design chapter's decision list into reconcilable code.
api: value, template, core
---

## Orientation

Chapter 139 settled configd's design: the seven-layer pipeline (source→parse→merge→template→validate→snapshot→query), the phase-matched model of a value loading domain plus a struct snapshot domain, the handover protocol of immutable snapshots plus atomic pointers, explicitly declared merge semantics, and the three template-safety boundaries. This is the **implementation chapter** — every decision lands, and every decision is **reconciled row by row**: the design chapter's six acceptance criteria (six UCs with positive and negative cases / merge-semantics vectors / hot-reload race / path-qualified validation / OOM point by point / zero residue on load failure) are each ticked when implementation completes. A project chapter's "implementation" is not "copying the design into code" but **testing the design under code's real constraints** — this chapter shows two mutual corrections between design and implementation (the template layer moves from "reuse the template engine" to "dedicated expansion" — forced by the size budget; the extractor moves from "macros" to "declarative array + loop" — forced by testability): that itself is the cashing-in of the design chapter's "decisions are revisable" clause.

## Introduction

Implementation starts from **acceptance files before source code**: write the three merge-vector groups hand-derived in Chapter 139's exercises (nested override / array replacement / type conflict) into the test directory's expectation files — they are the machine definition of merge semantics; list the six UCs' positive and negative cases — run each once its implementation completes. **The development rhythm** follows the seven layers bottom-up: parse + merge first (pure data transformations — easiest to test), then template + validation (the semantic layers), then snapshot + handover (the concurrency layer), finally the source layer (file/HTTP — the IO boundary). Each layer gets its tests immediately after implementation — **a layer's definition of done is "tests green", not "code written"**. This rhythm keeps the longest path (the hot-reload race) on a verified foundation throughout.

## Concepts

### The implementation's layered map

Where each of the seven layers implemented here lands in earlier chapters, and how much code is hand-written — **the map of assembly versus hand-written**:

```diagram flow
- Source layer (~80 hand-written lines): file reads (Chapter 45 xfile) / HTTP pulls (Chapters 98-99)
- Parse layer (0 hand-written lines): xrtJsonParse in one step - Chapter 32 owns it all
- Merge layer (~40 lines): cfg_merge recursion - this chapter
- Template layer (~35 lines): cfg_expand - the product of the revised decision
- Validation layer (~50 lines): rule array + loop
- Extraction layer (~45 lines): slot array + loop
- Handover layer (~30 lines): Ref + atomic pointer - Chapters 5/9
  about 280 hand-written lines total - everything else is library layers
```

Two hundred eighty lines implement a config center with hot reload and race guarantees — **that is the layer library's value quantified**: every layer is some chapter's main subject; the project writes only inter-layer glue and business semantics (merge/template/rules — the parts that truly vary per project). Cross-checked with Chapter 145's webserv "< 400 lines": **an XRT project's code volume is proportional to business complexity and independent of system complexity** — the system complexity is absorbed by the library layers.

### Landing the parse and merge layers

The parse layer is one step: `xrtJsonParse` (Chapter 32) produces the value tree — a pure transformation from source byte stream (Chapter 45's file Reader or an HTTP response body) to tree. The merge layer is this chapter's first **hand-written algorithm** (the library has no value-tree deep merge — that is configd's business):

```c
/* objects merge recursively, arrays replace wholesale - semantics locked by test vectors */
static bool cfg_merge(xvalue* Base, const xvalue* Over)
{
	size_t i;
	if ( !xrtValueIs(Base, XVALUE_OBJECT) || !xrtValueIs(Over, XVALUE_OBJECT) ) {
		return false;   /* type conflict: error with a path */
	}
	for ( i = 0; i < xrtValueCount(Over); i++ ) {
		xstrview Key;
		xvalue* Ov = xrtValueObjectAt(Over, i, &Key);
		xvalue* Bv = xrtValueObjectGet(Base, Key);
		if ( (Bv != NULL) && xrtValueIs(Bv, XVALUE_OBJECT) &&
			xrtValueIs(Ov, XVALUE_OBJECT) ) {
			if ( !cfg_merge(Bv, Ov) ) { return false; }  /* recurse */
		} else if ( !xrtValueObjectEdit(Base, Key, Ov) ) {
			return false;    /* leaf or array: replace wholesale */
		}
	}
	return true;
}
```

**Locked by the three vector groups**: nesting (`db.pool.max` overridden while `db.pool.min` survives — the recursion branch); arrays (`hosts` replaces wholesale, not element-wise — the leaf branch); type conflict (default `int`, environment `string` — `ObjectEdit` replaces and the type changes; the validation layer reports a path error). **Merge is order-sensitive**: the fixed `default → env` order — with more layers the order becomes part of the configuration (declared in the load parameters).

### The template layer: the revision from reuse to dedicated

The design chapter left two alternatives (template-engine reuse versus dedicated expansion). The implementation's size accounting decides against reuse: the template engine's closure (Chapter 35's full set) is bigger than all of configd's other layers combined — bringing in a whole template tree for one `{$VAR}` syntax is not worth it. **Dedicated expansion** in thirty lines:

```c
/* after merge, before validation: environment variables only, no recursion, undefined errors */
static bool cfg_expand(xstrview In, xstrbuf* Out)
{
	size_t i = 0;
	while ( i < In.Size ) {
		if ( (i + 1 < In.Size) && (In.Data[i] == '$') &&
			(In.Data[i + 1] == '{') ) {
			size_t j = i + 2, k;
			while ( (j < In.Size) && (In.Data[j] != '}') ) { j++; }
			if ( j >= In.Size ) { return false; }  /* unclosed */
			k = j + 1;
			/* 展开值里的 {$ 不再展开（不递归） */
			(void)k;
			{
				char Name[64];
				size_t n = j - (i + 2);
				if ( (n == 0) || (n >= sizeof(Name)) ) { return false; }
				memcpy(Name, In.Data + i + 2, n); Name[n] = 0;
				if ( !cfg_env_append(Out, Name) ) { return false; } /* undefined errors */
			}
			i = j + 1;
		} else if ( !xrtStrBufAppendByte(Out, In.Data[i]) ) {
			return false;
		} else { i++; }
	}
	return true;
}
```

The three safety boundaries' positions in code: `cfg_env_append` consults only the environment (a whitelisted source — Chapter 43); the expansion product **never re-enters** the expansion loop (`{$` inside a value outputs literally — no recursion); an undefined variable makes `cfg_env_append` return false (the error names the variable). **This is the first cashing-in of the design chapter's "decisions are revisable" clause** — the revision is recorded in this chapter (reuse→dedicated, rationale: size).

### The validation layer: rules as data

The design chapter's "a small declarative array of rules" lands as a three-field struct (path / type / constraint):

```c
typedef struct {
	const char* Path;      /* "server.port" */
	int Type;             /* CFG_T_INT etc. */
	bool (*Check)(long long v);  /* range: e.g. port 1..65535 */
} cfgrule;

static const cfgrule kRules[] = {
	{ "server.port", CFG_T_INT, rule_port },
	{ "server.tls.cert", CFG_T_STR, NULL },
	{ "db.pool.max", CFG_T_INT, rule_positive },
	{ "log.level", CFG_T_STR, rule_level },
};
```

The validation loop: fetch by path (`ObjectGet` descending segment by segment) → type check (`xrtValueIs` against the type enum) → range callback. **Path-qualified errors**' output format: `"db.pool.max: must be > 0"` — the path from the rule, the message from the constraint callback. **A new field costs one rule line** — the maintenance economics quantified here: a 20-field config = 20 rule lines (isomorphic with the extractor). **Three validation layers in one pass** (existence/type/range in the same loop) — called at the end of loading, intercepted at startup (acceptance 3).

### The handover protocol's timing-correctness argument

Why do "immutable snapshot + atomic pointer" guarantee whole consistency at any moment? One adversarial derivation: suppose the snapshot were mutable (the loading thread mutates fields while serving) — a query thread gets scheduled away between two field reads — comes back and reads a half-updated config (new `port` with the old TLS path) — exactly Chapter 139 pitfall 3's race condition. **Immutability eliminates this window structurally**: once a snapshot is published (the atomic pointer swap completes), nobody writes it again — the bytes behind the reference a query thread holds never change; the loading thread builds a **new object** (sharing nothing with the old snapshot, or only immutable parts). The atomic pointer swap guarantees the **publish action**'s atomicity (a read sees the old pointer or the new — no intermediate). Two atomics compose one invariant: **at any instant, every reader's configuration is the product of one complete load**. The graceful handover of long operations comes free: a reader holding a Ref (one request's processing) keeps the old snapshot until Destroy — new readers see the new immediately — **the handover has no boundary event** (no notifications, no barriers — reference counting converges automatically). The test shape of this argument is the race test (100k consistent reads) — theory, mechanism, test: three layers aligned.

### The snapshot layer and atomic handover

Snapshot extraction is a one-shot transformation from the value domain to the struct domain. **The second design revision**: the extractor moves from "macros" to "declarative array + loop" — macros expand at preprocessing (invisible to debuggers) and tests cannot assert field by field; a declarative array makes extraction rules **traversable and testable**:

```c
typedef struct {
	const char* Path;
	int Kind;      /* extraction target type */
	void* Slot;    /* struct field address */
} cfgslot;

static const cfgslot kSlots[] = {
	{ "server.port", CFG_K_INT, &Snap->Server.Port },
	{ "server.root", CFG_K_STR, &Snap->Server.Root },
	{ "db.pool.max", CFG_K_INT, &Snap->Db.PoolMax },
};
```

The handover protocol's three pieces (Chapter 139's design, verbatim, landed): **immutable snapshot** — after extraction the struct is read-only (documentation convention + review enforcement — C has no language-level immutability; with Chapter 115's immutable-sharing discipline); **reference counting** — the snapshot struct embeds the generic reference at its head (Chapter 5); **atomic pointer** — the global current-snapshot pointer swaps atomically (Chapter 9). The query side's `cfg_current()` atomically reads the pointer + Refs it, destroying when done — **wholly consistent at any moment** (the mechanism of acceptance 6).

### Verifying load-pipeline failure atomicity layer by layer

The mechanisms of acceptance 5/6 (OOM zero residue / load-failure zero leakage), layer by layer: **parse layer** — JsonParse failure returns NULL with no partial tree (Chapter 32 atomicity); **merge layer** — could cfg_merge leave Base partially modified on failure? — **the one weakness of this layer**: recursive merging mutates Base in place — a mid-way failure leaves Base half-merged. The fix: **a working copy for merging** — each load re-parses the default layer (or deep-clones it) and merges on the copy — failure discards the copy, the original unharmed (the working-copy pattern — isomorphic with Chapter 134's failure cleanup). **Template layer** — a StrBuf append failure discards the buffer (Chapter 25 atomicity). **Validation layer** — pure reading, no mutation. **Extraction layer** — pure copying, and only after everything passes. **Replace layer** — the pointer swaps atomically only after everything passes. **The expectation for per-layer failure injection**: a failure at any point — the global snapshot pointer untouched, readers unaware, the error carries location — exactly what the OOM tests assert point by point (Chapter 134's rotator runs the load pipeline). Failure atomicity is not guaranteed by "writing carefully" — it is **the sum of each layer's structural properties**.

### The source layer and the hot-reload loop

File source: `xfile` reads whole-file bytes → the parse-layer entrance (Chapter 45). HTTP source: client pull (Chapters 98/99) — failure semantics split by moment (first pull must fail / update may degrade — Chapter 139's HTTP addition). **The hot-reload loop**: the watcher (file-mtime polling or Chapter 48's asynchronous notification) triggers → rerun the load pipeline (parse→merge→template→validate→extract) → the new snapshot swaps atomically → the old retires naturally. **The failure branch**: a new config failing validation — **no swap** (the old snapshot keeps serving) + an alarm (Chapter 38) — "a bad config never harms a running service" is hot reload's safety floor.

### Delivery form and the deployment checklist

configd's delivery and deployment (Chapter 143's CLI contract, service edition): **delivery form** — a static library (host applications link it — the config center is an **embedded** component, not a standalone process; Chapter 135's embedding stance: memory/error/size sovereignty with the host); the trimming declares the value+template+core closure (no networking — the HTTP source is an optional compile). **The three-function API face**: `cfg_init(来源参数)` (source parameters) (load + validate + publish the first snapshot — failure returns the error), `cfg_current()` (atomically fetch the snapshot Ref), `cfg_reload()` (rerun the pipeline — the hot-reload entrance) — **minimally complete**: three functions cover the six UCs (the query family all goes through current's field access). **The deployment checklist**: config-file paths and layer order; the environment-variable whitelist (the set templates may reference — beyond it, load fails); the validation rule table; the snapshot budget (the config tree's memory ceiling — verified with stats). All four are **declarations, not code** — a deployment engineer adjusts without touching C — **the config system's own configuration is itself configured** (bootstrap consistency).

### Reserved extension points

Chapter 139's "against real configuration systems" named three extension points; their landing in implementation: **watch push** (the etcd shape) — the source layer adds polling/long connections (Chapters 66/82); the replace layer unchanged (the snapshot protocol already supports swaps at any frequency — **the protocol's foresight**); **multi-profile combination** (the Spring shape) — the merge layer's count goes from 2 to N: `cfg_merge` is already a binary operation over any two layers — combination merely turns the load parameter from a pair into an ordered list; **schema evolution** (fields added/removed) — a row in the rule array and the slot array each — rules-as-data's dividend. The three extensions share one property: **none touches the handover protocol** — race correctness is the most expensive attribute; the design decoupled it from business — however the business changes, the protocol does not. That is Chapter 139's most important legacy.

### The final reconciliation with the design chapter

The six acceptance criteria reconciled: **(1) six UCs, positive and negative** — load/query/validation/template each with both (the implementation's test checklist all green); **(2) merge vectors** — three expectation-file groups all green (the merge section's lock); **(3) hot-reload race** — threads test: 100k concurrent queries during updates with zero mixed state (the machine proof of immutable snapshot + atomic pointer); **(4) path-qualified validation** — three deliberately violating inputs all intercepted with path-carrying errors (the validation section's format); **(5) OOM point by point** — failure injection on the load pipeline: any-point failure with zero residue (each layer's failure atomicity + the final no-swap); **(6) load-failure zero residue** — a half-config never leaks (the snapshot swaps only after everything passes). **All six pass — configd is deliverable**.

## Examples

### First complete program: the atom of value loading and query

The program below is from `examples/data/json` — the runnable atom of the parse and query layers (quoted in Chapter 139's design chapter; rechecked here as implementation's starting point):

```embed path="examples/data/json/main.c" title="examples/data/json/main.c"

```
```
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

**What just happened.** (1) This is the **whole** of configd's parse layer — the implementation chapter adds only the four pure transformations merge/template/validate/extract on top — each layer used this atom for development-time verification (a layer's unit tests need no complete pipeline). (2) The DOM query (ObjectGet by key) is exactly the validation layer's path-descend vocabulary — **one API family runs through every layer of the loading domain** (Chapter 31's DOM design cashed in by the project). (3) The three usage selections (header comment: DOM/SAX/Writer) in configd: **loading uses DOM** (random access and merging need the full tree); **Writer unused** (config is input, not output).

### Second complete program: the template-file combination boundary

The second program is from `examples/template/file` — the concrete form of the design chapter's template alternative (the reference of this chapter's revised decision):

```embed path="examples/template/file/main.c" title="examples/template/file/main.c"

```
```
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/template/file/main.c -lws2_32 -liphlpapi
Hello Alice
```

**What just happened.** (1) `CompileFile`'s capability face (read + compile + render + error-offset location — the header comment) is everything the "reuse the template engine" route would bring — capability far beyond configd's `{$VAR}`. (2) The revision quantified: the template closure this sample links versus the dedicated expansion's thirty lines — **thirty times the function for ten times the size** (size-profile numbers — Chapter 131's method) — the wrong trade. (3) But the revision does not negate the alternative's value: **when config templates evolve to conditionals/loops** (`{$if env=prod}`) the reuse route returns — the extension point stays open.

### The test-file checklist and gate wiring

configd's test assets follow Chapter 133's four quadrants (submitted in the same PR as the implementation — tests are not afterthoughts but the definition of delivery): **positive** (test_config.c — each of the six UCs' positive cases + three merge vectors + template positives — expectation-file driven); **race** (test_config_threads.c — the hot-reload mixed-state test: a loading thread alternating good/bad configs × 4 query threads × 100k consistency reads — the zero-mixed-state assertion); **negative** (vectors covering four classes — type conflict / undefined variable / unclosed template / validation violation — exact matching of error messages containing paths or variable names); **OOM** (test_config_oom.c — the load pipeline point by point: any-point failure leaves the old snapshot untouched + zero residue — Chapter 134's rotator applied directly). **Gate wiring**: all tests hang on the configd module manifest's tests field (Chapter 130) at the quality gate — `build.py --suite configd` runs both tracks automatically; the merge vectors, as data files, hang beside the manifest's benchmarks slot — **expectation files share the tests' lifecycle**. This checklist's meaning: configd's next evolution (multi-layer merge / distributed watch) needs one run after any change to know whether a contract broke — **test assets are the project's memory**.

### Retrospective: the implementation chapter's methodological sediment

With configd implemented, set the retrospective beside Chapters 138/143/144's — three lessons **specific to implementation chapters**: **expectation files first** (hand-derived vectors become machine-judgeable expectations — the design chapter's exercise output directly becomes the implementation's test assets — the artifacts of two chapters joining); **a layer's definition of done is tests green** (between "code written" and "layer complete" sits testing — each layer gets its unit tests immediately; the longest path (the race) always has a verified foundation); **revisions leave traces** (template reuse→dedicated, extraction macro→array — the two deviations from design are written into this chapter with rationale — **traceable deviation is not drift but the record of evolution** — the engineering meaning of Chapter 139's "decisions are revisable" clause). Adding the cumulative lessons of projects 1/2/4/5 — **twenty engineering common-sense rules** continue under test in the last two project chapters (140/141).

## Contracts

- **Seven-layer pipeline**: every layer a pure data transformation, atomic on failure; a layer's definition of done is tests green, not code written.
- **Merge semantics**: objects merge recursively / arrays replace wholesale — three expectation-file groups lock the semantics; multi-layer merge order is a load parameter.
- **Three template boundaries**: environment variables only (whitelisted source) / no recursion (expansion products never re-enter) / undefined errors (located by variable name).
- **Expansion timing**: after merge, before validation — override precedes injection; validation sees final values.
- **Validation as data**: a rule array of path + type + constraint; a new field one rule line; error format "path: message".
- **Extraction as declarations**: a path + target-slot array — traversable and testable (the engineering reason macros were dropped).
- **Three handover pieces**: immutable snapshot + reference counting + atomic pointer swap — wholly consistent at any moment (proven by the race test).
- **Hot-reload safety line**: a new config failing validation swaps nothing + alarms — a bad config never harms a running service.
- **Zero failure residue**: a failure at any load point produces no visible state change (the old snapshot continues).
- **Revision record**: reuse→dedicated (template, size) / macro→array (extraction, testability) — the design chapter's clause cashed.

### Implementation walkthrough: the load main entry's source

The load pipeline's main entry — the seven layers chained with the replace gate (the complete program builds on the teaching samples; key segment walked through):

```c
/* load: source->parse->merge->template->validate->extract->replace (swap only when all pass) */
bool cfg_reload(cfgctx* C)
{
	xvalue* Default = NULL;
	xvalue* Env = NULL;
	cfgsnap* Fresh = NULL;
	bool Ok = false;

	/* 1-2 source and parse: each layer reads + parses (Chapter 32 atomicity) */
	if ( !cfg_load_layer(C, &C->DefaultSrc, &Default) ||
		!cfg_load_layer(C, &C->EnvSrc, &Env) ) {
		goto Done;               /* failure: pointer untouched - zero residue */
	}
	/* 3 merge: on the working copy (Env overrides Default) */
	if ( !cfg_merge(Default, Env) ) { goto Done; }
	/* 4 template: after merge, before validation (three boundaries inside cfg_expand) */
	if ( !cfg_expand_tree(Default) ) { goto Done; }
	/* 5 validate: the rule array all at once (failure carries a path) */
	if ( !cfg_validate(Default, kRules,
			sizeof(kRules) / sizeof(kRules[0])) ) {
		goto Done;
	}
	/* 6 extract: the slot array, pure copy (only reached after all pass) */
	Fresh = cfg_build_snapshot(Default, kSlots,
		sizeof(kSlots) / sizeof(kSlots[0]));
	if ( Fresh == NULL ) { goto Done; }
	/* 7 replace: atomic pointer - readers see the new one immediately */
	cfg_atomic_swap(&C->Current, Fresh);
	Ok = true;

Done:
	xrtValueRelease(Env);
	xrtValueRelease(Default);
	return Ok;
}
```

**Walkthrough points.** (1) The seven steps' order is exactly the design chapter's pipeline — **step 7 is the only visible state change** (the first six all work on local objects — failure discards them, the global never notices). (2) The unified `goto Done` cleanup (destroying both value trees) — the single-exit discipline in pipeline form. (3) `cfg_merge(Default, Env)` merges directly on Default — "Default is re-parsed every load" guarantees no sharing (the implicit form of the working-copy pattern — explicit cloning is needed only when multi-source caching appears). (4) `Fresh` becomes globally owned after success (reference counting takes over after the swap) — the one-shot ownership-transfer point.

## Pitfalls

### Pitfall 1: merging without distinguishing objects from arrays

Symptom: the environment layer's array merges element-wise with the default (wholesale replacement intended) — or an object gets replaced wholesale (recursion intended).

Cause: the merge branch checked only "value exists", not "both sides are objects" — arrays taking the replace branch happens to be right, objects taking it is wrong (or the reverse). The branch condition is the **conjunction of both sides' types** — one side being an object is not enough.

```c bad
xvalue* Bv = xrtValueObjectGet(Base, Key);
if ( Bv != NULL ) {
	xrtValueObjectEdit(Base, Key, Ov);   /* always replace: the nested object loses default-layer sibling keys */
}
```
```c good
if ( (Bv != NULL) && xrtValueIs(Bv, XVALUE_OBJECT) &&
	xrtValueIs(Ov, XVALUE_OBJECT) ) {
	cfg_merge(Bv, Ov);                    /* recurse only when both sides are objects */
} else {
	xrtValueObjectEdit(Base, Key, Ov);    /* the rest replaces wholesale (arrays included) */
}
```

### Pitfall 2: the extractor writes the struct mid-load

Symptom: a server whose validation failed starts with a **half-extracted** struct — some fields new, some old.

Cause: extraction and validation in the wrong order (extracting while validating — when the third field fails, two are already written). The correct order: **extraction begins only after validation fully passes** — extraction is a pure copy and should have no failure branch.

```c bad
for ( each rule ) {
	if ( !check(rule) ) { return false; }   /* failure */
	extract(rule);                           /* but earlier ones are already in the struct */
}
```
```c good
for ( each rule ) { if ( !check(rule) ) { return false; } }
for ( each slot ) { extract(slot); }        /* copy only after all pass - a pure phase */
```

### Pitfall 3: swapping the snapshot even though validation failed

Symptom: ops mistypes one value — the online service instantly restarts behavior on a bad config (cannot reach the database) — "a typo" becomes "an incident".

Cause: the hot-reload loop treated "load complete" as "the swap condition" — missing validation as the gate. The swap condition is **everything passes** (parse/merge/template/validate) — any failure keeps the old snapshot + alarms.

```c bad
if ( load(&tree) ) { swap(build_snapshot(&tree)); }  /* where is validation? */
```
```c good
if ( load(&tree) && validate(&tree, kRules) ) {
	swap(build_snapshot(&tree));    /* swap only when all pass */
} else {
	alert("config reload rejected");  /* the old snapshot continues */
}
```

### The three-section expectation-file format

The recommended format for merge-vector expectation files (exercise 1, engineered): **input section** (default-layer JSON + environment-layer JSON — two blocks); **expected section** (the merged-result JSON — one block); **error section** (empty, or "path: message"). The three-section form keeps the test driver minimal (read three sections, run the merge, compare result and error) — **vectors are data**: adding cases adds files, never driver changes. The format's relation to Chapter 133's test shapes: expectation files are the **data form** of positive and negative cases — separating test code (the driver) from case data (vector files) is the mark of mature test engineering (cases grow far faster than drivers change). configd's three vector groups start in this format — every future merge-semantics change adds the vector first, then modifies the implementation (a miniature demonstration of **vector-driven development**).

## Exercises

### Basic: single-layer unit tests

Write the three vector-group tests for the merge layer (Chapter 139's hand-derived expectations); write three negatives for the template layer — undefined variable / unclosed / nested reference. Acceptance criteria: all six green with error messages carrying location (path or variable name).

### Advanced: the hot-reload race test

threads shape: a loading thread looping swaps (good/bad configs alternating) + 4 query threads each holding a Ref doing 100k consistency reads (each read checks two fields for same-source). Acceptance criteria: zero mixed state; during bad configs, everything read is old (alarm count correct).

### Challenge: the full HTTP source

Implement per Chapter 139's HTTP addition: first-pull-must-fail / update-may-degrade / `only-if-cached` offline mode — plus a local mock endpoint testing the three moments. Acceptance criteria: the three moments behave per design; during degradation the snapshot never swaps; the cache-hit path goes through Chapter 101's ONLY mode.

### Closing position: Project 2's place in the six-project sequence

configd (design + implementation, two chapters) sits in Volume 13's sequence: Project 1 logstat (a tool — one-shot) → **Project 2 configd (a service — long-lived state and concurrent handover)** → Project 4 xdl (a network tool — remote uncertainty) → Project 5 pushd (long connections — fan-out and final states) → Project 6 webserv (full-stack synthesis). The **long-resident state** dimension configd introduces is the sequence's second step — it turns "lifecycle management" from slogan into protocol (the three handover pieces) and "concurrent correctness" from lock discipline into structural property (immutability). Later projects stack networking and fan-out on this foundation — **each step stands on the previous step's acceptance**. The two chapters' division of labor, revisited: the design chapter gives the decision list (reconcilable), the implementation chapter gives the landing and revisions (traceable) — this "design/implementation twin-chapter" template is the mature form of project teaching — Project 5 (pushd) merges into one chapter because its design decisions were prepaid in the xws chapters (109–112) — **the boundary between reusing and newly writing teaching material** lies exactly in such judgments.

### The one-page acceptance sheet

The implementation chapter's final page — the six criteria's status and evidence locations: (1) six-UC cases all green (the test-checklist section); (2) three merge vectors green (the merge section + expectation files); (3) race 100k zero-mixed (the snapshot section's argument + threads test); (4) validation errors with paths (the validation section's format); (5) OOM point-by-point zero residue (the failure-atomicity-by-layer section); (6) load failure zero leakage (the main-entry walkthrough — step 7 the only visible change). **All six ticked, each with its evidencing section** — this page can be pasted into a PR description: reviewers follow the map.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Implementation stance | design decisions landed + row-by-row reconciliation + two revisions traced |
| Development rhythm | bottom-up layer by layer; layer done = tests green |
| Merge branch | recurse only when both sides are objects; the rest replaces wholesale (arrays included) |
| Template dedicated | thirty-line expansion: whitelist / no recursion / undefined errors |
| Validation as data | rule array (path + type + constraint); errors "path: message" |
| Extraction as declarations | slot array — testable (macros dropped) |
| Phase discipline | extraction only after validation fully passes (pure copy, no failure branch) |
| Handover | immutable snapshot + Ref + atomic pointer swap — the race test's machine proof |
| Hot-reload safety line | validation failure swaps nothing + alarms — a bad config never harms a running service |
| Reconciliation verdict | all six acceptance criteria pass — configd deliverable |
