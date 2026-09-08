---
num: 42
slug: env
title: Environment Variables and System Info
volume: 卷五 系统服务
type: practice
lead: Environment variable reads/writes with existence semantics, plus system info queries — the outermost config layer and the first entry into the host.
api: environment
---

## Orientation

The env module is thin but critical (five functions in the whole API; an hour to learn, a lifetime to use): it is the **outermost config layer** (environment variables — the first config entrance of the container era: 12-Factor apps live on them) and the **first query point for host info** (system name, version, machine identity). Reading has two entrances with distinct semantics: **`xrtEnvGet`** returns an owning copy (NULL = doesn't exist — the copy isolates you from later environment changes, safer than bare getenv); **`xrtEnvLookup`** is dual-channel (returns bool + carries out an owning copy via the out-param) — with one crucial subdivision: **only false is a real failure** (an encoding problem, say); "variable doesn't exist" is a success state with the output emptied; Get's NULL cannot distinguish the two. Together the entrances answer the three-state question: **exists with a value / exists empty / doesn't exist** — "doesn't exist" is not an error. Beyond the APIs, this chapter focuses on the fitness boundary of environment variables as a config layer — part of a configuration-architecture decision — which config belongs in variables and which firmly does not.

## Introduction

Deploying the same image to dev/staging/production — how does configuration differ? File paths differ per environment; hard-coding at build time means one package per environment. Environment variables are the answer: `DATABASE_URL`, `LOG_LEVEL`, `LISTEN_PORT` are injected per environment at the orchestration layer (compose/k8s), and the image itself carries zero environment difference — the config pillar of "build once, run anywhere".

But their boundary must also be seen clearly: they are a **flat string map** — cramming structured config (nested objects, arrays) into variables means inventing encoding conventions (`A_B_C=1`-style flattening), and conventions multiply into disaster; length limits (legacy baggage on some platforms) and the all-strings typing also burden complex config. The industry's consensus division (where 12-Factor meets most engineering practice): **the few switches of environment difference** (addresses, levels, on/off) go into variables; **the bulk of business config** (complex structures, frequent changes) goes into config files (Chapter 32's JSON/XSON) — the standard shape is a "two-layer config" where startup lets environment variables override file defaults (Chapter 37's "read log level from the environment" is exactly this pattern).

## Concepts

### Reading and the three-state semantics

The core question of reading environment variables is the **three states**: exists with a value / exists but empty / doesn't exist. Get's NULL and Lookup's "success + empty output" are both explicit answers for "missing" — never disguise "doesn't exist" as an empty string (an empty string may be a legal config value). The variants sample's `missing=ok` assertion demonstrates: missing takes the default-value path, and both it and "read a value" are verified paths.

### Writing and the process environment

`xrtEnvSet` modifies **this process's** environment block — writes take real effect (not an in-process simulation table), so **child processes created afterward inherit it** (the environment-inheritance mechanism of Chapter 52's process creation); `xrtEnvRemove` deletes. Writing environment variables is a global-affecting operation — environment-block operations are not synchronous under multithreading (POSIX's historical baggage); the discipline is **write only in the single-threaded startup phase**, read-only afterward. Set/Remove return bool to report success — the putenv semantic differences between Windows and POSIX have been smoothed over by the implementation.

### System info queries (the runtime platform channel)

The host-info family: OS name and version (diagnostic log headers, runtime branches for platform checks — compile-time branches use Chapter 8's `XRT_FEATURE_` macros; things known only at runtime use this), CPU/memory overview (a self-check report for capacity planning), machine identity (reporting "who am I" at distributed node registration). The shared shape of these queries: read-only, queried once at startup and cached (except dynamic info like remaining memory, re-queried on demand).

### The place in configuration layering

```diagram flow
- Layer 1 environment variables: environment-difference switches (address/level/port) - injected by orchestration, earliest readable by the process
- Layer 2 config files: the business bulk (structured, versioned) - parsed into a value tree by Chapter 32
- Merge policy: file defaults + environment overrides (the flat-key special case of Chapter 31's ObjectMerge)
- Layer 3 command-line arguments: one-shot overrides for this run - highest priority of the three
```

Three layers, three posts: variables manage "what this deployment environment is like", config files manage "how this business is configured", the command line manages "what to change temporarily this run". Priority from low to high: file < environment < command line — startup assembly code overrides layer by layer in this order.

## Examples

### Complete program: three-state reads and default values

From the repository sample `examples/environment/variants/main.c`:

```embed path="examples/environment/variants/main.c" title="examples/environment/variants/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/environment/variants/main.c -lws2_32 -liphlpapi
$ ./a.exe
missing=ok
present=hello
```

**What just happened.** (1) Reading an unset variable name: returns false → the program takes the default-value path and prints `missing=ok` — missing is not an error; it is one of the three states. (2) Reading a set variable: a view comes back, parsed and used — `present=hello`. (3) This sample demonstrates the owning path: Get returns a copy, freed with `xrtFree` when done — even if the environment changes afterward, this string is unaffected; for "existence first" judgments use Lookup — the two entrances chosen by calling posture. (4) Running without injection also passes all checks: the program is robust to both "configured" and "not configured" — the point of a default-value strategy.

### Complete program: a system info report

From `examples/system/environment/main.c`:

```embed path="examples/system/environment/main.c" title="examples/system/environment/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/system/environment/main.c -lws2_32 -liphlpapi
$ ./a.exe
value=hello
```

**What just happened.** (1) `xrtEnvSet` writes the real process environment — **child processes inherit it** (Chapter 52); the putenv semantic differences between Windows and POSIX are smoothed over by the implementation (a returned bool reports success uniformly). (2) `xrtEnvGet` reads back an **owning copy** — even if the environment changes afterward, this string is unaffected; free with `xrtFree` when done. (3) `xrtEnvRemove` deletes — the three-step closed loop (Set/Get/Remove) is the minimal complete set of environment management. The system-info queries (system name/version/machine identity mentioned in Concepts) live in the same header family, used for diagnostic headers and node-registration reports.

## Contracts

- **Three-state semantics**: exists with a value / exists empty / doesn't exist — reading APIs return dual-channel; missing goes through explicit defaults.
- **Write timing**: environment writes only in the single-threaded startup phase; read-only at runtime (both thread safety and view stability demand it).
- **Child inheritance**: this process's environment changes affect child processes created afterward (Chapter 52's inheritance semantics).
- **Config layering**: file defaults < environment overrides < command line highest — variables carry only "environment-difference switches".
- **Dual platform channels**: compile-time differences via feature macros (Chapter 8), runtime differences via this module's queries — never mixed.

### From examples to engineering: three hosts of environment variables

**Containerized deployment** (the home turf): the image carries zero environment difference; `DATABASE_URL`/`LOG_LEVEL`/`PORT` are injected per environment by orchestration — 12-Factor's config principle landed; health-check addresses and secret-reference paths often ride here too. **Development debugging**: when running a service locally, temporarily override with a variable (`LOG_LEVEL=DEBUG ./app`) — faster than editing a config file and leaves no trace; the shell's temporary-prefix syntax was born for exactly this. **Child environment construction**: before creating a child process (Chapter 52), `xrtEnvSet` tailors the child's environment (its own config view) — the parent changes the environment, the child inherits the effect. The three hosts confirm the positioning "environment variables = environment-difference switches" — any config need beyond that range belongs in the file layer.

### An operations view: the environment is an interface

The deployment description (the env section of compose/k8s) is the application's **outward interface** — it declares "which environment-difference inputs this service needs". Interfaces deserve documentation and validation: at startup, enumerate all expected variables and print the assembly result (`LOG_LEVEL=INFO (env)` versus `(default)`) — the "configuration assembly log" is troubleshooting's first stop; a missing required variable (no `DATABASE_URL` in production, say) should fail fast with a clear error, not run on empty values and blow up on the first request. These two, plus the pitfalls' "no runtime writes", are the three steps from "works" to "operable".

### The complete picture of config assembly

Placing this chapter into the whole: startup assembly reads the three layers (file defaults < environment overrides < command line highest), the assembled result enters a value tree (Chapter 31) for the whole program to consume; the assembly log prints final values and their sources; required items are validated with fast failure. This assembler is the full form of Chapter 35's pipeline "config station" — exercise two's challenge version is exactly it. Assemble once, read-only globally: hot config updates (runtime changes) take the "new value tree atomically swapped in + notification" path (Chapter 31's three hosts), never runtime writes to the environment — the two disciplines echo again.

## Pitfalls

### Pitfall 1: stuffing business config into environment variables

Symptom: the variable list runs to dozens and still grows; `SERVICE_A_TIMEOUT_MS`, `SERVICE_A_RETRY_COUNT`... the naming conventions become a system of their own; every config change means editing the orchestration system's deployment description.

Cause: the flat string shape was treated as a universal config layer — structured config force-flattened, convention complexity growing exponentially.

```c bad
/* a dozen-plus business parameters all through environment variables - orchestration becomes a degraded config file */
int timeout = env_int("SVC_TIMEOUT_MS", 3000);
int retry = env_int("SVC_RETRY", 3);
str endpoint = env_str("SVC_ENDPOINT", "...");
/* ... twenty more ... */
```

```c good
/* config bulk in a file (structured value tree), environment keeps only environment switches */
xvalue* pCfg = xrtJsonParse(read_file("config.json"), NULL);
int timeout = cfg_int(pCfg, "timeout", 3000);           /* file default */
if ( env_has("SVC_TIMEOUT_MS") ) {                       /* environment override */
	timeout = env_int_override("SVC_TIMEOUT_MS");
}
```

### Pitfall 2: writing environment variables at runtime

Symptom: occasional crashes in unrelated code; the race condition of environment-block operations under POSIX (unsetenv concurrent with getenv is UB) surfaces under load.

Cause: the environment block is global mutable state with no synchronization guarantees in the legacy APIs — runtime writes pull the rug out from under every reading thread.

```c bad
void handle_config_reload(void)
{
	xrtEnvSet(XRT_STR_LITERAL("APP_MODE"), ...);   /* runtime write - concurrent readers are UB */
}
```

```c good
/* environment writes concentrated at the top of main (single-threaded phase) */
int main(void)
{
	apply_env_overrides();          /* the only write point */
	/* read-only from here on */
}
```

## Exercises

### Basic: verify the three states (assert on paper first, then run and compare)

For one unset, one set-empty, and one set-with-value variable, read through both Get and Lookup and print the six results — write the two entrances' differing presentation of the three states into your conclusions.

### Advanced: two-layer config assembly (three-layer priority in practice)

Implement config loading of "JSON file defaults + environment overrides": the file holds 5 keys, 2 of them overridable (an `APP_` prefix mapping); after overrides, print the final config. Complete the third priority layer (file < environment < command line) with a simple argv parse.

### Challenge: node registration info (a module-cooperation report body)

Implement `node_info()`: system name/version (this module) + machine identity + process start time (Chapter 41) + version number (Chapter 8), packed into a JSON value tree (Chapter 31) — the report body of distributed node registration. Acceptance criteria: run twice — static fields stable, the start-time field changes; the JSON parses under a standard parser; full-type fields (time as the time type — Chapter 33).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Two entrances | Get copy (NULL = absent; two failures indistinguishable) / Lookup (absent = success + empty out; only false is a real failure) |
| Write discipline | write only in the single-threaded startup phase; read-only at runtime; Set/Remove return bool |
| Layering | config file < environment < command line; the environment holds difference switches, never the business bulk |
| Inheritance | this process's environment writes take real effect; child processes created later inherit |
| Dual platform channels | compile-time differences via feature macros (Chapter 8) / runtime differences via this module - never mixed |
| Node reporting | system info + machine identity + start time + version - the standard four of a registration body |
| Three operational steps | assembly log with sources / required items fail fast / runtime read-only - from works to operable |
| Place in the whole | this chapter is the foundation of Chapter 35's pipeline config station; hot updates go through value-tree swap, never the environment |
