---
num: 10
slug: math
title: The Math Toolbox: The xrtMath Family
volume: 卷二 数学、随机与标识
type: practice
lead: Relations, classification, exponentials and logs, and "near" judgments — the cross-platform-consistent math complement, with floating-point comparison explained thoroughly.
api: math
---

## Orientation

Volume 2 starts with mathematics. The `xrtMath` family fills the "engineering gaps" of the C standard math library: generic relational operations like `Min`/`Max`/`Sign`, the NaN/Inf/finite three-way classification, numerically stable versions like `Log1p`/`Expm1`, and the floating-point "near" judgment `xrtMathNear` used throughout the book. Beyond walking the function family, this chapter thoroughly explains the two most common engineering traps: **why floating-point cannot be compared with `==`** and **the semantics of modulo on negatives** — they are the foundation of every later chapter on numbers, sampling, and comparison.

## Introduction

Three snippets, three memories. First: the assertion `0.1 + 0.2 == 0.3` fails — floating-point representation error is common knowledge, yet every project steps on it anew. Second: in graphics code `fmod(-7, 3)` returns `-1` instead of the expected `2` — C's modulo follows the dividend's sign (truncated semantics), differing from Python and others (floor semantics); cross-language teams have written real direction bugs here. Third: wanting to judge "do two points coincide" on coordinates at the `1e9` scale, an absolute tolerance of `1e-9` never matches, while a relative tolerance collapses near zero — a single tolerance pleases neither end.

These three memories map to `xrtMath`'s three core designs: `Near`'s **combined tolerance**, `Mod`'s **explicit truncation semantics** (documented and pinned by tests, not guessed), and the classification family's **three-way completeness** (NaN/Inf/finite mutually exclusive and covering). The math library is small, but every piece answers a real engineering wound.

One easily overlooked value: **these functions reappear in every later volume**. Tolerance comparison is the assertion basis of Chapter 11's distribution checks and Volume 12's performance benchmarks; three-way classification is the first gate of data parsing (Volume 4) cleaning illegal floats; `Log2`/`Exp2` are the mathematical foundation of container capacity growth and bitmap indexing. Get fluent with `Near` and the classification family, and no later numeric assertion will need a second thought.

## Concepts

### The function-family map

| Family | Functions | One-liner |
| --- | --- | --- |
| Relations | `xrtMathMin` / `Max` / `Sign` / `Trunc` / `Mod` / `Rad` | Min/max, three-state sign, truncation, truncated modulo, degrees to radians |
| Classification | `xrtMathIsNaN` / `IsInf` / `IsFinite` | NaN / infinity / finite — three ways, mutually exclusive and complete |
| Exp/Log | `xrtMathLog2` / `Exp2` / `Log1p` / `Expm1` | Base-2 log and exponential; `Log1p`/`Expm1` numerically stable near zero |
| Roots | `xrtMathCbrt` | Cube root, **correct on negatives** (`pow(x, 1/3)` is a trap there) |
| Nearness | `xrtMathNear` / `xrtMathIntNear` | Combined-tolerance float comparison; the integer "near" judgment |

`Sign` returns three states `-1/0/1` (not a boolean) — it serves sorting and direction judgments, where zero has its own meaning. `Log1p(x)` computes `ln(1+x)`: with tiny `x`, computing `log(1+x)` directly rounds `1+x` to `1` and yields zero — `Log1p` specifically bypasses this precision trap; `Expm1` is its inverse. The pair is standard equipment in interest rates, probability, and incremental statistics.

`Rad` converts degrees to radians — trigonometric functions eat radians while physics and configuration carry degrees, and this one conversion is the easiest to get backwards (multiply by `π/180` or divide). `Trunc` truncates toward zero (`2.7 → 2`, `-2.7 → -2`); note it differs from "floor" (`-2.7 → -3`) — compose your own when floor semantics are needed; don't treat `Trunc` as universal rounding.

### Floating-point comparison: the combined tolerance

`xrtMathNear(a, b, abs, rel)` judges `|a-b| ≤ max(abs, rel×|a|)` — absolute and relative tolerances, **whichever is larger** applies:

```diagram flow
- Near zero: the relative tolerance fails (0 and 1e-10 differ tenfold yet both should count as zero) -> pass an absolute tolerance
- Large values: the absolute tolerance fails (a 0.05 gap at 1e9 is meaningless in precision) -> pass a relative tolerance
- Combined: pass both, Near takes the max — both scenarios each covered
- Integers: IntNear uses a single tolerance (integers carry no representation error)
```

A practical rule for choosing tolerances: **the absolute tolerance = the magnitude you consider "physically identical"** (coordinates, 1e-9 meters); **the relative tolerance = the rounding fraction you accept** (one thousandth → pass 0.001). Passing both is always the safest form.

### A comprehensive example: coordinate alignment

Chaining this chapter's tools into one real judgment: "do the coordinates from two laser rangefinder runs point at the same spot?" Assume meter-scale magnitudes, instrument precision 0.01 millimeters, and a 3-millisecond clock skew between runs. The correct judgment takes three steps: first `IsFinite` filters out the NaN/Inf produced by failed measurements (classification first — every later comparison is meaningless otherwise); coordinate deltas use `Near(dx, 0, 1e-5, 1e-9)` near zero and `Near(dx, x, 0, 1e-6)` at large values — the combined tolerance open on both sides; the time delta uses `IntNear(t1, t2, 5000)` with a microsecond tolerance. This three-step pattern (classify → combined tolerance → integer tolerance) recurs in sensor fusion, sample alignment, and regression-test assertions — worth burning into muscle memory.

### Modulo and sign semantics

`xrtMathMod(7, 3) = 1`, `xrtMathMod(-7, 3) = -1` — **the result follows the dividend's sign** (the truncated semantics of C's `fmod`). This differs from the intuition "modulo is a cyclic mapping" (`-7 mod 3 = 2`, floor semantics). Why truncation? Because it agrees with the C standard, with hardware instructions, and `Mod(a,b)` satisfies the identity `Trunc(a/b)*b + Mod(a,b) == a` — self-consistent and derivable. For an "always non-negative" cyclic index, express the idiom `((a % n) + n) % n` explicitly; don't make the library guess for you.

### The link to Chapter 11: the foundation of numeric determinism

The `xrtMath` family shares the "deterministic and reproducible" foundation with Chapter 11's randomness system: the reproducibility assertions of the random module rely on this chapter's `Near` for numeric comparison. Together the two chapters form Volume 2's "numeric determinism" base — the later containers (Volume 3) and parsing (Volume 4) come back to these two tools in their tests.

### Cross-platform consistency

`xrtMath`'s other layer of value is **the same input yielding comparable results across compilers** — classification, relations, and `Near` are all exact semantics; the exp/log family keeps a consistent precision policy on common platforms. Test snapshots (like Chapter 11's fixed-seed random sequences) can therefore be compared across different CI machines.

## Examples

### Complete program: two-way assertions over the function family

From the repository example `examples/math/tour/main.c`, every function asserted in both directions against known constants:

```embed path="examples/math/tour/main.c" title="examples/math/tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/math/tour/main.c -lws2_32 -liphlpapi
math: min/max/sign/trunc/mod/rad ok
math: isnan/isinf/isfinite = 1/1/1
math: log2/exp2/log1p/expm1 ok
math: cbrt(-27) = -3
```

**What just happened.** (1) The relation family verified both ways: `Min(2,3)` and `Min(3,2)` both give 2 — argument order does not affect semantics; such "symmetric assertions" are the standard test for relational functions. (2) `Mod(-7,3)` asserted as `-1`, pinning the truncated semantics into the test — if your code depends on other semantics, it exposes itself at the assertion rather than becoming a direction bug after launch. (3) The three classification functions verified mutually exclusively with the three samples `0.0/0.0` (NaN), `1.0/0.0` (Inf), and `1.5` (finite) — NaN is not even "equal to itself"; the classification functions are the only reliable discriminator. (4) `Cbrt(-27) = -3`: the cube root is defined and correct on negatives — the key difference from `pow`.

### Complete program: the three scenarios of combined tolerance

From `examples/math/near/main.c`, walking all three tolerance choices at once:

```embed path="examples/math/near/main.c" title="examples/math/near/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/math/near/main.c -lws2_32 -liphlpapi
relative: near
absolute: near
integer : near
```

**What just happened.** (1) `100` vs `100.05` judged near with relative tolerance `0.001`: the gap 0.05 ≤ 0.1, passing — large values call for relative. (2) `0` vs `1e-10` judged near with absolute tolerance `1e-9` — the relative tolerance here would demand zero difference (any multiple of 0 is 0), failing utterly. (3) The integer version `IntNear(1000, 1003, 5)`: judgments like "a few units of jitter or clock skew don't matter" don't need the floating-point tolerance apparatus. Note that the example's two `Near` calls each pass one zero tolerance — the combined tolerance lets you open one side, but engineering practice more commonly gives both: the absolute side catches the near-zero region, the relative side the large values, one line of judgment covering the full range.

Putting `Near` into an assertion macro is many teams' standard move: `ASSERT_NEAR(got, want)` expands to a `Near` call — tests neither false-fail from representation error nor false-pass from over-wide tolerance. When writing a test, first think clearly "at what magnitude does this assertion work", then choose the tolerance — that matters more than memorizing numbers.

## Contracts

- **Near combined**: `|a-b| ≤ max(abs, rel×|a|)`; a zero tolerance disables that side; passing both is safest; assertion macros should build directly on `Near`.
- **Mod truncates**: the result follows the dividend's sign; the identity `Trunc(a/b)*b + Mod(a,b) == a` holds; normalize yourself for non-negative indices.
- **Trunc toward zero**: `-2.7 → -2`; different from floor — never mix the two semantics.
- **Sign three states**: `-1/0/1`; zero is neither positive nor negative.
- **Classification three ways**: NaN/Inf/finite mutually exclusive and complete; judging NaN only via `IsNaN` — every comparison operator is unreliable on NaN.
- **Rad direction**: degrees → radians, multiply by `π/180`; getting it backwards breaks every trigonometric function — worth pinning with an assertion.
- **Cbrt on negatives**: cube root correct on negatives; never substitute `pow(x, 1.0/3.0)`.

## Pitfalls

### Pitfall 1: comparing floats with ==

Symptoms: assertions failing sporadically, graphics alignment randomly not passing, platform-dependent behavior; the most insidious form is "passes in debug, fails in release".

Cause: floating-point rounding paths depend on compiler optimization and intermediate precision; the binary result of `0.1+0.2` is simply a different number than `0.3`.

```c bad
double fSum = 0.1 + 0.2;
if ( fSum == 0.3 ) {          /* always false: different binary representations */
	accept();
}
```

```c good
double fSum = 0.1 + 0.2;
if ( xrtMathNear(fSum, 0.3, 1e-12, 1e-9) ) {   /* combined tolerance */
	accept();
}
```

### Pitfall 2: assuming modulo is always non-negative

Symptoms: ring-buffer indices and hash bucketing out of bounds or flipping direction on negative input; high incidence in code migrated from Python/Go.

Cause: C's `%` and `xrtMathMod` both use truncated semantics — a negative dividend gives a negative result; "modulo is non-negative" is the semantics of some other languages.

```c bad
int iIndex = (int)xrtMathMod(iOffset, 3.0);   /* iOffset=-7 -> -1 */
Touch(Buffer[iIndex]);                        /* out of bounds: negative index */
```

```c good
int iIndex = ((int)iOffset % 3 + 3) % 3;      /* explicitly normalized into [0,3) */
Touch(Buffer[iIndex]);
```

## Exercises

### Basic: three classification samples

Write a program constructing NaN (`0.0/0.0`), Inf (`1.0/0.0`), and finite (`1.5`), printing each value's three classification results (three rows, nine booleans), verifying mutual exclusion and completeness; then compare NaN with itself using `==`, print the result, and explain why classification cannot rely on comparison.

### Advanced: probing the tolerance boundary

For `Near(100.0, x, 0.0, 0.001)`, binary-search the `x` boundary of nearness and compare with the hand-computed `100×(1±0.001)`. Hint: one `ulp` (the smallest representation unit) beyond the boundary should flip to "not near"; test both sides to confirm the relative tolerance applies symmetrically.

### Challenge: a numerical-stability comparison

For ten points of `x` from `1e-15` to `1e-1`, compute the error of `log(1+x)` and of `xrtMathLog1p(x)` against a high-precision reference (a `long double` version approximates it), tabulating the comparison. Acceptance: the table shows `log(1+x)`'s error exploding at small `x` while `Log1p` stays stable, with your own explanation of the cause (hint: the rounding of `1+x`).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Comparison | `Near(a,b,abs,rel)` takes `max(abs, rel×|a|)`; integers use `IntNear` |
| Modulo | truncated semantics following the dividend's sign; non-negative indices via your own `((x%n)+n)%n` |
| Sign | `Sign` three states `-1/0/1` |
| Classification | `IsNaN` / `IsInf` / `IsFinite` mutually exclusive and complete; NaN unreliable under every comparison |
| Stable functions | near zero use `Log1p` / `Expm1`, never `log(1+x)` |
| Negative roots | cube root via `Cbrt`; `pow(x,1/3)` unusable on negatives |
