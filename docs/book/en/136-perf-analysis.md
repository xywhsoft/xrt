---
num: 136
slug: perf-analysis
title: Performance Analysis Methods: Benchmark Design and Measurement
volume: 卷十二 工程实践
type: practice
lead: Questions first, environment baselines and scheduling policy, repeat sampling with dispersion gates, regression thresholds and sweep experiments — turning "feels faster" into defensible numbers.
api: memory_stats, core
---

## Orientation

The first step of tuning is not changing code but **knowing how to measure** — this chapter is performance work's methodology (Chapter 137 is the practice). XRT's measurement infrastructure provides the template: **profiled benchmarks** (`config/performance_profiles.json`: each profile a benchmark source + arguments + sampling policy — "what to run" is a declaration, not improvisation); **environment baselines** (fourteen elements: platform/CPU/affinity/power policy/compiler/optimization level/scheduling policy — numbers compare only within one baseline); **statistical discipline** (repeats sampling + MAD (median absolute deviation) and central-range dual dispersion gates — noisy measurements are rejected, not averaged away); **regression thresholds** (directional higher 10%/lower 20% — slow gets strict, fast gets suspicion); **sweep experiments** (a systematic traversal of parameter space, not single-point spot checks). Plus the **questioning discipline**: a benchmark answers **the question you asked** — with the wrong question, the most accurate numbers are useless.

## Introduction

Three failed openings of performance work: **baseline-free measurement** ("this feels slower" — by how much? Compared to what? On which machine?); **cross-environment comparison** (mixing dev-machine and CI-machine numbers — CPU generation/power policy/background load all differ); **noise taken as signal** (treating a 3% single-sample fluctuation as regression — a wild-goose chase). The common root: treating measurement as "run it and look at the number" instead of **experiment design**. XRT's measurement toolchain (measure_performance/measure_size + profiles + report schema) freezes experiment design in place: the profile declares what and how to run, the baseline records where, the gate decides whether it counts — **humans are responsible only for asking the right question**.

The questioning discipline, expanded: before benchmarking, write one sentence — "I suspect X is slower/faster than Z under Y"? X is the operation (say SPSC single enqueue), Y the scenario (capacity 4096, two threads), Z the comparison (the previous version / another data structure). If you can't write that sentence, it isn't time to measure. Chapter 62's "run with defaults first, observe, tune by data" takes this shape here: run the default profile for a baseline → look at dispersion (the noise level) → design the experiment → numbers answer the question.

## Concepts

### Profiled benchmarks: declarative experiments

```diagram flow
- Profile: each entry of performance_profiles.json
  {name, benchmark source (dev/bench/...), args (full arguments), smoke_args (quick verification), suite}
- Default policy: repeats=5 / warmups=1 / policy=baseline-unpinned / -O2
- Measure: measure_performance.py --profiles selects -> compile -> run sampling -> report
  (report schema 2: environment baseline + per-profile sample series + statistics)
```

**The two speeds of smoke and full**: smoke_args is CI's fast form (seconds to verify "the benchmark still runs"); full args are formal sampling (minutes to statistics). Benchmark sources live in `dev/bench/` (hung on the module manifest's benchmarks field — yet another consumption of Chapter 130's manifest) — **benchmarks are manifest-managed assets too**.

### The environment baseline: fourteen elements

| Group | Elements |
| --- | --- |
| Platform | platform / machine / platform_release |
| Processor | cpu / cpu_count / **affinity** / **power_policy** |
| Build | compiler_family / compiler_version / arch / optimization / cflags / ldflags |
| Policy | policy (e.g. baseline-unpinned) |

The two most-ignored elements: **power_policy** (a laptop's power-saver versus plugged-in performance can differ 2–3× — reports must record it); **affinity** (pinning changes cache and scheduling behavior — the queue latency benchmark has a pinned variant specifically for contrast). **Comparison rule**: trend conclusions hold only within one baseline; across baselines, suspect the environment before the code (Chapter 131's seven size elements, performance edition).

### Statistical discipline: dispersion gates

`max_relative_mad: 0.2` (median absolute deviation relative to the median ≤20%) and `max_relative_central_range: 0.3` (central range ≤30%) — **dual gates**: either exceeded = this round of measurement is **too noisy** — the result is "rejected", not "averaged and carried on". This is the watershed with amateur measurement: amateurs average noise away (a very stable mean that means nothing — it lands somewhere different each time); discipline exposes noise (reject → investigate → interference source / more repeats / different policy). **Median first**: reports take the median (robust to outliers); the sample series is recorded in full (transparent, re-examinable).

### Regression thresholds: directional dual thresholds

`regression: {higher: 0.1, lower: 0.2}` — **10% slower alarms, 20% faster also alarms**. Why watch speedups: the measurement environment changed (a fake improvement), or it was paid for elsewhere (this profile faster, another slower — the full profile set must run). The directional asymmetry is pragmatic: slow is direct harm (strict gate), fast is an indirect signal (loose gate).

### Sweep experiments: sweep

A single-point benchmark answers "how fast is this configuration"; **sweep answers "what does the configuration space look like"** — queue's capacity/batch two-dimensional sweep (`run_queue_bench_sweep`): a matrix of capacity 64..65536 × batch 1..128 — inflection points (returns saturate past batch 32) and plateaus (SPSC degrades at small capacity) at a glance. **Sweeps are where selection evidence comes from**: Chapter 21's "how to choose capacity" and Chapter 68's "how big a batch" — the answers come from sweeps, not gut feeling. Reports (QUEUE_BATCH_SWEEP et al.) are archived under dev/bench — **experiment results are first-class assets**.

## Examples

### First complete program: a memory-view micro-benchmark

The program below is from `examples/memory/stats` — stats readings as the lightest performance observation:

```embed path="examples/memory/stats/main.c" title="examples/memory/stats/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/memory/stats/main.c -lws2_32 -liphlpapi
before=0 after=1
malloc_calls=2 pooled=1 direct=1 backing=3
```

**What just happened.** (1) Performance analysis's first question is often "how much got allocated" — stats' four readings answer it: here 48B hits the pool (zero system calls) while 4096B passes through — **the pool boundary measured live** (faster than reading docs and always consistent with the current build). (2) This is **micro-observation** (counter level) — time-dimension measurement goes through the bench toolchain; but many "performance problems" are answered at the counter layer (backing abnormally high = pool configuration mismatching business sizes). (3) Methodological position: stats is the **zero-cost always-on** observation (Chapter 135's health panel) and also the first stop of performance investigation — look at counters before timing.

### Second complete program: a self-check benchmark with deterministic output

The second program is from `examples/core/version_limits` — the trustworthy form of a benchmark:

```embed path="examples/core/version_limits/main.c" title="examples/core/version_limits/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/core/version_limits/main.c -lws2_32 -liphlpapi
version=2.0.0-dev
limits: depth=128 entries=100000 input=268435456
```

**What just happened.** (1) This sample demonstrates **deterministic output** — contrasted with timing benchmarks' randomness: in benchmark engineering "reproducible" splits in two (deterministic output = same value every time; statistical output = stable distribution). (2) The default resource bounds (128/100000/256MiB) are the performance profiles' hidden parameters — parser benchmarks' input sizes sit behind these gates (oversized-input tests need explicit relaxation — profile cflags can inject). (3) The real benchmarks of dev/bench (queue/coroutine/task families) are bigger in shape (multi-threaded sampling + statistical output), but **methodology's atoms** are all present in this sample: fixed input, explicit output, comparable.

### Anatomy of a benchmark report (QUEUE_BENCH as the template)

`dev/bench/QUEUE_BENCH_20260728_XRT2.md`'s structure is the template: **Sources** (benchmark source + scripts + build shape — the reproducible entry); **Environment** (OS/CPU/compiler/build flags/scheduling policy — the baseline self-reported); **Matrix** (matrix parameters: items per producer / capacity / producer-consumer counts / batch / sample counts — the experiment design visible); **Results** (per-profile sample series + median — transparent raw data); **Interpretation** (reading — the connection between conclusions and numbers: the single-item path in the expected band, batch reservation amortizing effectively, batch as the preferred high-throughput path). Five sections, each with its duty: without Sources it's irreproducible, without Environment incomparable, without Matrix unevaluable, without Results untrustworthy, without Interpretation unusable. Self-check a report against these five sections — this is also the acceptance format of Chapter 136's exercises and Chapter 137's challenge.

## Contracts

- **Profile declarations**: benchmark source + args + smoke two-speeds + policy — what runs is never improvised.
- **Fourteen-element baseline**: platform/CPU/affinity/power/build/policy — trends compare only within one baseline.
- **Dual dispersion gates**: MAD ≤20% + central range ≤30% — over the limit means rejection, not averaging.
- **Median reports**: sample series recorded in full; the median is outlier-robust.
- **Directional thresholds**: 10% slower strict / 20% faster loose — fast is a signal too.
- **Sweep experiments**: systematic traversal of parameter space — where selection evidence is born; reports archived under dev/bench.
- **smoke/full two speeds**: CI seconds-level verification + formal minutes-level sampling.
- **Benchmarks are assets**: benchmarks hang on the manifest — managed with the same lifecycle as sources.
- **Questions first**: if you can't write "suspect X over Z under Y", it isn't time to measure.

## Pitfalls

### Pitfall 1: a single sample as the conclusion

Symptom: "8% faster after optimization" — 8% means nothing inside a 3% single-run noise floor; rerun and the direction may flip.

Cause: a single sample has no distribution — without dispersion there is no resolution. Repeats sampling + gates are the minimal configuration separating signal from noise.

```c bad
run_bench_once();      /* single sample */
if ( faster_by_8pct() ) { merge(); }  /* concluding inside noise */
```

```c good
/* profiled sampling: repeats=5 + dispersion gates; median compared against baseline */
measure_profile("queue", /*repeats*/ 5, /*gates*/ true);
if ( regression_significant(&New, &Base) ) { conclude(); }
```

### Pitfall 2: comparing trends across environments

Symptom: CI numbers always 20% better than local — no idea which to trust.

Cause: different baselines (CPU/power/affinity/load). Align baselines first (same machine, same policy, same build) then compare; across environments compare only structural conclusions (e.g. the shape of the batch curve), never absolute numbers.

```c bad
/* local 9M, CI 11M items/s */
if ( ci_faster() ) { trust_ci(); }   /* different baselines: meaningless */
```

```c good
/* same-machine same-policy A/B: environment identical, only code varies */
measure_ab(same_machine(), build_a(), build_b());
/* across machines compare only sweep curve shapes (structural conclusions) */
```

### Pitfall 3: benchmarks that measure only the path you want to see

Symptom: the optimization made the benchmark 30% faster, production unchanged — the benchmark's hot path differs from real load.

Cause: the benchmark question was wrong (questioning discipline). What share of real load is batch? What's the capacity distribution? Profile the real thing first (stats/counters), then design the benchmark — **benchmark questions come from reality**.

```c bad
/* measuring only the single-item path */
bench_single_enqueue();      /* optimize it */
/* production load is 90% batch calls - the optimization goes unfelt */
```

```c good
/* stats first: production behavior counted -> batch dominates */
/* the benchmark includes batch profiles + sweep fixes the batch - the question comes from reality */
bench_batch_sweep();
```

## Exercises

### Basic: run a benchmark profile

`measure_performance.py --profiles task` (or the corresponding profile) — read the report's baseline and statistics sections. Acceptance criteria: you can name your machine's values among the fourteen elements; whether the dispersion gates pass.

### Advanced: an A/B comparison of two versions

Make a small change to any benchmark source (add a branch, say) — a profiled A/B comparison (same machine, same policy). Acceptance criteria: the report correctly concludes "no significant difference"; you can explain why a small change is invisible under these gates.

### Challenge: a one-dimensional sweep report

Model the queue sweep: sample some parameter (say pool capacity) at 5 points — hand-write the sweep script + a tabular report (Environment/Matrix/Results/Interpretation, four sections — the QUEUE_BENCH report's structure). Acceptance criteria: the inflection point is identifiable; the interpretation gives a selection recommendation; the report is archived dev/bench style.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Method core | experiment design — profile declarations / baseline locking / statistical gates / directional thresholds |
| Profiles | profiles.json: source + args + smoke two-speeds + policy |
| Baseline | fourteen elements; power/affinity easiest to miss; trends compare only within one baseline |
| Dispersion gates | MAD ≤20% + central range ≤30% — over the limit means rejection, not averaging |
| Report | median + full sample record; schema 2 |
| Regression | 10% slower strict / 20% faster loose — fast is a signal too |
| sweep | parameter-space traversal — where selection evidence is born; reports are first-class assets |
| Two speeds | smoke seconds in CI / full minutes of sampling |
| Questioning discipline | "suspect X over Z under Y" — if you can't write it, it isn't time to measure |
| Micro-observation | stats counters first — look at allocations before timing |
