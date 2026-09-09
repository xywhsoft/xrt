---
num: 138
slug: project-config-design
title: The Config Center: Design
volume: 卷十三 实战项目
type: concept
lead: Requirements analysis and acceptance criteria, the data-model choice (value or a dedicated struct), hot-reload semantics, layered validation and template defaults — getting the design right before writing code.
api: value, template, core
---

## Orientation

Project 2 is a **config center** (configd): centrally managing a service group's configuration — JSON config served from files or HTTP, per-environment overrides (default/development/production three-layer merge), validation (types and constraints), optional template-variable expansion (`{$env}` injecting environment facts), and hot reload (re-reading on file change). The shape difference from Project 1 (Chapter 137, a one-shot tool) is fundamental: **configuration is long-resident state** — loaded, cached, queried, possibly updated — lifecycle management and concurrent access become main subjects for the first time. This is the **design chapter**: requirements and acceptance, the data-model choice, layering and module selection, hot-reload and concurrency semantics, validation policy and template safety — all settled before the implementation (Chapter 139). The design chapter's methodological stance: **code is the execution of design; most rework originates in design-time ambiguity**.

## Introduction

The config problem looks trivial — "just read a JSON". In real systems configuration's complexity comes from four dimensions: **diverse sources** (files/HTTP interfaces/environment variables/command line — they must compose); **layered overrides** (defaults overridden by the environment layer, the environment layer by the instance layer — the merge semantics must be explicit); **late validation** (a wrong config blows up only at first use — it should be intercepted at startup); **update propagation** (which components reread after a change; what happens to operations mid-flight on old values). Ambiguity in any dimension becomes a production incident: a wrong merge order ships development values to production; missing validation lets port 0 "start successfully"; a hot-reload race condition lets two threads see inconsistent half-configurations.

This chapter turns each dimension into an **explicit design decision** — each with rejected alternatives and rationale (Chapter 137's selection-table methodology extended to architecture level). The next chapter's implementation executes these decisions one by one — **the design chapter's verifiability** lies exactly there: when implementation completes, reconciliation shows every decision either honored or explicitly revised.

## Concepts

### Requirements and acceptance criteria

configd's use cases (user-story form): **UC1 load** — at service startup, pull configuration from `config.json` (or an HTTP endpoint); on failure, report explicitly (silently running on empty config is forbidden). **UC2 layering** — `default.json` → `config.{env}.json` two-layer deep merge (objects recurse, arrays replace wholesale — the replacement semantics explicitly declared); **UC3 query** — path-style lookup (`server.port`, `db.pool.max`) with type expectations (an integer fetch returns an integer; a type mismatch errors); **UC4 validation** — full validation at load time (required keys exist / types correct / value ranges sane), errors carrying the **config path** (`db.pool.max: must be > 0` — Chapter 4's error model applied); **UC5 templates** — string values support `{$ENV_HOME}` expansion (environment facts injected into config); **UC6 hot reload** — on file change, reload; old and new configurations **hand over atomically** (any query at any moment sees either all-old or all-new — never a mix).

**Acceptance criteria** (reconciled one by one in the implementation chapter): (1) each of the six UCs has positive and negative cases; (2) merge semantics have test vectors (nested override / array replacement / type conflict); (3) a hot-reload race test (concurrent-query consistency during updates — Chapter 132's threads shape); (4) every validation error carries a path; (5) OOM point by point (config loading allocates — Chapter 133's discipline); (6) zero residue on load failure (a half-config never leaks — atomicity).

### The data-model choice: dynamic value or a dedicated struct

The first and most important decision — **what configuration is in memory**:

| Option | Shape | Strengths | Weaknesses |
| --- | --- | --- | --- |
| A: dynamic value (Chapter 31 xvalue) | the whole JSON loaded as a value tree | any schema compatible (config evolution costs zero code changes); queries via generic paths; JSON deserialization in one step | slow queries (string-path lookup); no compile-time checks |
| B: dedicated struct | `struct config { int Port; struct db Db; }` | fastest access, type safety | schema changes require editing the struct + loading code; multi-source merges hand-written |
| C: value loading + struct snapshot | load/merge/validate in the value domain; after loading, extract once into the struct | both worlds: flexible loading + fast use | extraction code to maintain (a schema mirror) |

**Decision: C**. Rationale: configuration's **evolution frequency** (schema changes often — the value domain is free) and **read frequency** (the hot path may query per request — the struct is O(1)) differ in character — **match by phase**. B is rejected because merging/templates are too heavy hand-written in the struct domain; A because the query path would enter the hot path. C's mirror cost is controlled by **extractor macros** (one declaration line per field — Chapter 139's implementation).

### Layered design and module selection

```diagram flow
- Source layer: files (Chapter 44 xfile reads) / HTTP (Chapters 97-99 xhttp client)
  - unified behind a "fetch a byte stream" interface: sources are pluggable
- Parse layer: JSON (Chapter 32) -> an xvalue tree; XSON (Chapter 33) reserved as an extension
- Merge layer: deep merge of two value trees (objects recurse / arrays replace - semantics explicit)
- Template layer: environment-variable expansion of string values (Chapter 25 strings + Chapter 42 environment)
- Validation layer: a rule set (required / type / range) -> errors with paths (Chapter 4)
- Snapshot layer: value -> struct extracted once (the atomic handover point)
- Query layer: direct struct access (O(1))
```

Seven layers each corresponding to an earlier chapter's main subject — **module selection comes entirely from existing chapters** (no new dependencies): another validation of the thesis "a project = the reassembly of existing layers". Inter-layer contracts: byte stream → value tree → merged value tree → validated value tree → struct snapshot — **every arrow is a pure data transformation** (no side effects, atomic on failure).

### Hot reload and concurrency: the atomic-handover semantics

Hot reload's core design is the **handover protocol between old and new configurations**. configd's choice: **immutable snapshot + atomic pointer replacement**. The load product is an immutable struct (nobody mutates it after loading — thread-safe by nature, Chapter 114's immutable sharing in a config edition); the query side holds the snapshot via a **reference-counted pointer** (Chapter 5's generic references — `Ref` to take a share, `Destroy` when done); update = build a new snapshot → atomically replace the global pointer (Chapter 9's atomic operation) → the old snapshot frees naturally when its references reach zero. **The semantic guarantee**: any queryer at any moment sees a configuration **wholly consistent** (one of old or new, never a mix); a queryer inside a long operation keeps using the old snapshot until it finishes (a graceful handover — never yanked away). **Rejected alternatives**: a global read-write lock (the query hot path would enter a lock — rejected); in-place mutation of a mutable struct (a concurrency hell — rejected).

### Validation layering: semantic correctness beyond syntactic legality

The JSON parser guarantees **syntax** (Chapter 32); the validation layer owns **semantics**: **existence** (required keys — missing is an error); **type** (the value is the expected type — `server.port` must be an integer); **range** (port 1..65535, `pool.max > 0` — business constraints). All three run once at load time (**intercepted at startup** — "running with a wrong config" is not allowed); the error format `路径: 消息` (path: message) (`db.pool.max: must be > 0`) — **path-qualified errors** spare the troubleshooter from scanning the whole JSON. Rules are expressed as a small declarative array (key path + type + constraint callback) — **rules as data** means a new field costs one rule line (the same maintenance economics as the extractor macros).

### Template-expansion timing and the injection-safety boundary

`{$VAR}` expansion happens **after merge, before validation** — rationale: template output may affect validation (the port after expansion is the port to validate); while merging happens in the raw domain (the default layer's `{$BASE}` is not expanded before the environment layer overrides it — override takes precedence over injection). **The safety boundary**: expand only **environment variables** (a whitelisted source — Chapter 42); no recursive expansion (`{$B}` inside the value of `{$A}` is not expanded — blocking injection chains); an undefined variable errors (never a silent empty string — a config referencing a nonexistent environment is a deployment error). These three are the template layer's safety contract — Chapter 76's password discipline in a config edition: **the injection surface must be defended at every "string becomes config" boundary**.

## Examples

### First complete program: the minimal config-loading loop

The program below is from `examples/data/json` — JSON to value loading and query (the textbook shape of configd's parse layer):

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

**What just happened.** (1) JSON text → value tree → field query → serialization round trip — **the parse layer's complete data flow** in a dozen lines. (2) The value views returned by the query are exactly configd's validation-layer input (type checks run against them). (3) This loop is Chapter 139's starting point: wrapping it with "source layer (file/HTTP) → merge layer → template layer" yields the full load pipeline — **every layer chosen in the design chapter has such a runnable atom** (Chapter 137's "examples are gates", architecture edition: **design is not paper talk — every selection has a verified part**).

### Second complete program: template file loading

The second program is from `examples/template/file` — templates and files combined (the template layer's concrete reference):

```embed path="examples/template/file/main.c" title="examples/template/file/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/template/file/main.c -lws2_32 -liphlpapi
Hello Alice
```

**What just happened.** (1) `CompileFile` = read file + compile in one step — the **combination part of source layer and template layer already exists** (Chapter 34): configd's `{$VAR}` expansion can reuse the template engine (consistent variable-injection semantics) or shrink to a dedicated string replacement (smaller) — **both alternatives are on the table**; the implementation chapter picks by size budget. (2) The template's variable context comes from a value tree (Chapter 34's context injection) — combined with environment variables (Chapter 42), that is the complete shape of "config referencing environment". (3) Design insight: **the expansion-layer choice blocks no other layer** — layering's benefit cashes in again: swapping the expansion implementation is a single-layer replacement.

### The shape of a design document: this chapter as template

What form should a design chapter take? Not big UML diagrams but a **decision list** — five elements per decision: **problem** (what to solve), **options** (at least two alternatives), **decision** (what was chosen), **rationale** (why it wins), **verification** (how we know the decision was executed). This chapter's demonstrations: the data model (one of three, rationale is frequency-phased, verification is the extractor-macro line count); the handover protocol (one of three, rationale is the structure eliminates races, verification is threads tests with no mixed state); merge semantics (explicitly declared, verification is test vectors); template safety (three boundaries, verification is injection cases). **Why not UML**: diagrams carry a large ambiguity space (two readers, two readings) while each decision-list row is reconcilable — the same lineage as BOOK_SPEC's quality-gate philosophy (**machine-judgeable over human-interpretable**). This design is quoted row by row at the head of the implementation chapter (Chapter 139) — the reader will see that the "decision → code" mapping is this chapter's real deliverable: **not a document but a reconcilable constraint set**. Chapter 140's chat-service design chapter follows the same template — design-methodology reuse precedes code reuse.

### From tool to service: replaying Project 1's lessons

The seven lessons of Chapter 137's retrospective replay in configd's context — **the shape changed, the lessons hold**: **acceptance first** (six UCs + six criteria versus five — a service's acceptance must cover concurrency and lifecycle, a tool's need not); **shape decides the container** (a value tree versus a counting array — data "evolvability" is the new dimension: a tool's data shape is fixed, a service's can evolve); **views are use-and-discard** (the validation layer works on value views; only the snapshot layer extracts — no premature freezing); **layered swappability** (the source layer's file/HTTP swappable versus the Reader family — the same discipline upgraded to seven layers); **platform facts enter design** (a service's platform facts are its concurrency model and config-file location conventions); **budget first** (load once + query hot path — the budget names the snapshot layer's necessity); **four-quadrant testing** (a service adds threads and hot-reload-race quadrants). **All seven replay, two upgraded** — that is the project sequence's pedagogy: not different projects, but **the same methodology deepened under different constraints**.

## Contracts

- **Acceptance first**: six user stories and six acceptance criteria — the implementation chapter reconciles row by row; unreconciled means unfinished.
- **The model decision**: value loading domain + struct snapshot domain — phase-matched to evolution frequency and read frequency.
- **Merge semantics**: objects merge recursively, arrays replace wholesale — semantics explicitly declared with test vectors.
- **Atomic handover**: immutable snapshot + reference counting + atomic pointer replacement — wholly consistent at any moment.
- **Graceful handover**: long operations hold the old snapshot until done — never yanked.
- **Three validation layers**: existence / type / range — checked once at load, intercepted at startup.
- **Path-qualified errors**: `db.pool.max: must be > 0` — errors located to config paths.
- **Template safety**: environment variables only, no recursion, undefined errors — the injection surface defended.
- **Expansion timing**: after merge, before validation — override precedes injection, validation sees final values.
- **Parts pre-verified**: every layer selection has a runnable atom (this chapter's two samples) — design is not paper talk.

### The added design of the HTTP source (UC1's network extension)

Beyond files, UC1 allows an HTTP endpoint — three added design points: **client assembly** (Chapter 97's easy layer or Chapter 98's builder: config pulling is low-frequency, easy suffices; a verifier is mandatory — Chapter 84's "not verifying is not an option" is stricter here: **a hijacked config source means every downstream hijacked**); **failure semantics** (a pull failure errors per UC1 — but it **may** be designed as "keep the last snapshot + alarm" — the degradation policy is a deployment decision: the first pull at startup (no old snapshot) must fail; a mid-run update failure can tolerate the old config — two moments, two policies); **cache boundary** (the HTTP response's caching semantics — Chapter 100's `only-if-cached` mode is exactly "use cached config offline" — the four modes mapped precisely onto config). The HTTP source upgrades configd from "a local file tool" to "a network config client" — but only the source layer of the seven-layer pipeline changes (the third cashing-in of Chapter 137's layered-swappability discipline).

## Pitfalls

### Pitfall 1: ambiguous merge semantics (arrays and deep objects)

Symptom: the environment layer means to "override only db.pool's max" but replaces the whole pool object (min falls back to default — production connection counts jump); or the reverse, meaning to replace a whole array but merging it element-wise with the default layer.

Cause: the merge semantics were never **explicitly declared** — "deep merge" defaults differ per library for objects and arrays. The design-time decision must become test vectors: nested objects recurse, arrays replace wholesale — every merge's input and output has an expectation file.

```c bad
/* merge behavior not written into the contract: array merging depends on the implementation's mood */
merge(a, b);   /* element-wise? wholesale? - nobody knows */
```

```c good
/* contract explicit: objects recurse, arrays replace + three test-vector groups locking it */
merge_objects_recursive(); replace_arrays_whole();
```

### Pitfall 2: validating at "first use" instead of load time

Symptom: a wrong config starts successfully — it blows up at 3 a.m. when that feature first triggers (the worst failure time).

Cause: lazy validation defers failure to the hardest-to-debug moment. Full validation at load = **intercepted at startup** — failures surface at deployment (the basis of acceptance criterion 3).

```c bad
int get_port(void) {
	return cfg_get_int("server.port");  /* blows up at first use: 3 a.m. */
}
```

```c good
/* full validation at the end of loading: existence/type/range - startup failure carries a path */
if ( !cfg_validate(&tree, rules) ) { fail_with_path(); }
```

### Pitfall 3: hot-reloading by mutating a mutable struct in place

Symptom: at the update instant, concurrent readers see half-new half-old config — `server.port` is new but `server.tls` still old (new port with old TLS — the handshake fails).

Cause: mutable struct + lock-free in-place writes = a race. The atomic-handover protocol (immutable snapshot + pointer replacement) eliminates the "mixed state" structurally — not by lock discipline but by **impossibility**.

```c bad
struct config g_cfg;            /* global mutable */
void hot_update(struct config* n) {
	g_cfg.port = n->port;        /* half-new half-old window: concurrent readers see a mix */
	g_cfg.tls = n->tls;
}
```

```c good
cfg_snapshot* fresh = build_snapshot(&tree);  /* new snapshot (immutable) */
atomic_swap(&g_current, fresh);               /* atomically swap the pointer */
/* readers: Ref one -> read -> Destroy - wholly consistent */
```

### Against real configuration systems

Comparing configd's design with industry shapes clarifies the trade boundary. **etcd/Consul**: distributed KV + watch push — configd's single-machine snapshot model has no watch or consensus protocol (that is distributed systems' subject — beyond this book, but the interface is reserved: the snapshot pointer's atomic replacement is exactly the simplified form of a "local watch"). **Spring profiles**: multiple activated profile combinations — more flexible and more complex than configd's fixed two-layer merge; configd's two layers cover 90% of scenes (default + environment), more layers left as extension. **Environment-variable derivation** (12-factor): all configuration injected from the environment — configd's template layer is exactly its bridge to file configuration (the hybrid of file skeleton + environment injection — a pragmatic compromise between two worldviews). The takeaway: **configd is "the minimal complete that suffices"** — every real system's complex feature maps onto some extension point of this design (source layer / merge-layer count / template depth): another form of design-completeness acceptance.

## Exercises

### Basic: hand-deriving merge vectors

Write three merge test-vector groups (default layer + environment layer → expected result): partial override of a nested object, wholesale array replacement, type conflict (default int, environment string — expected an error with a path). Acceptance criteria: your hand derivation is the implementation chapter's expectation file.

### Advanced: the handover-protocol timeline

Draw the hot-reload timeline: three interleavings of the loading thread (build new snapshot → atomic replace) with two query threads (Ref → read → Destroy) — what each sees before / during / after the swap. Acceptance criteria: each case's visibility derives from "immutable + atomic pointer" — no mixed state.

### Challenge: designing the rule set

For a realistic configuration (server/db/log sections), write the complete validation rule set (paths + types + range constraints) and three deliberately violating test inputs. Acceptance criteria: the violating inputs are all intercepted at startup with path-qualified errors; the rule line count is of the same order as the config field count (maintenance economics verified).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Design-chapter stance | decisions before code — each with alternatives and rationale |
| The model | value loading domain (evolution) + struct snapshot domain (reads) — phase-matched |
| Seven-layer pipeline | source→parse→merge→template→validate→snapshot→query — pure data transformations |
| Merge semantics | objects recurse / arrays replace — explicitly declared + test vectors |
| Handover protocol | immutable snapshot + reference counting + atomic pointer swap — wholly consistent at any moment; graceful for long operations |
| Three validation layers | existence / type / range — once at load, intercepted at startup |
| Path-qualified errors | path + message — no scanning the whole file |
| Template safety | environment variables only / no recursion / undefined errors |
| Expansion timing | after merge, before validation — override precedes injection; validation sees final values |
| Parts pre-verified | every layer selection has a runnable atom — design grounded |
| HTTP addition | easy layer + mandatory verifier; first-pull failure must report, update failure may degrade |
