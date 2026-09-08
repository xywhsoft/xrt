---
num: 11
slug: random
title: Random Numbers and Random Text
volume: 卷二 数学、随机与标识
type: practice
lead: The four-layer randomness system — the secure entropy source, the thread-local global, the fast, and the explicit reproducible instance; the security boundary is this chapter's red line.
api: random
---

## Orientation

XRT's randomness capability splits into four layers, each with its own place: the **Secure** layer goes to the operating-system entropy source (`xrtSecureRandom`, exclusively for keys and tokens); the **global Rand** layer is the thread-local default choice; the **Fast** layer prioritizes speed, not security; the **RNG instance** layer seeds explicitly and is fully reproducible (`xrng`). This chapter makes the four-layer selection clear and pins one red line: **PCG and the Fast series are not cryptographically secure** — tokens, keys, and nonces must go through the Secure layer. Fixed-seed reproducibility is the other theme: test snapshots and replay systems both stand on "same seed, same sequence".

## Introduction

Two seemingly contradictory demands coexist. In games and simulation you want randomness **reproducible** — the same session, the same save loaded, the random event sequence must agree, or replay recordings and synchronous online play all fail; in security you want randomness **unpredictable** — a guessed session token equals a hijacked account. These two must be carried by different mechanisms: the former by an explicitly seeded pseudorandom generator (same seed, same sequence), the latter by the operating-system entropy source (`xrtSecureRandom`).

The common mistake is mixing the two: seeding a fast generator with the time and generating tokens — the time is guessable and the generator predictable, two layers of protection amounting to zero. XRT cuts off this error class with explicit naming: `Rand`/`FastRand`/`Rng` read as statistical at a glance, `SecureRandom`/`SecureText` read as security at a glance, and the module docs state plainly "security boundaries must use Secure". Naming is documentation — during code review you needn't chase into the implementation; the function name alone completes half the security audit.

## Concepts

### The four-layer system and selection

```diagram flow
- Secure: xrtSecureRandom / SecureText, OS entropy source — the only legitimate source for keys, tokens, nonces
- Global Rand: xrtRand64 etc., thread-local default — business sampling, demos, workload simulation
- Fast: xrtFastRandSeed etc., speed-first non-secure — hot-path jitter injection, graphics noise
- RNG instance: xrng + seeding, explicit reproducible state — test snapshots, replay recordings, parallel independent streams
```

Selection asks only two questions: **do you need unpredictability?** Yes → Secure, done. **Do you need reproducibility?** Yes → an RNG instance (explicit state, explicit seed). Neither → the global Rand for convenience, Fast for confirmed performance-critical hot paths. The four layers' function-name prefixes map one-to-one onto those answers — visible at a glance in code review.

Add a feel-list of "which layer for which scenario": generating session tokens → `SecureText`; game drop rolls → an RNG instance (replay requirement); load-testing traffic perturbation → the global Rand; graphics particle noise → Fast; temporary file names → `SecureText` or XID (Chapter 12); test fault-injection points → an RNG instance (the seed is the test case). The list grows per project, but every new entry should answer one of those two questions.

### The RNG instance: explicit state and stream numbers

`xrng` is a state struct you can put anywhere — a stack variable, a struct member, a stretch of bytes in a save file. `xrtRngSeed(&Rng, 种子, 流序号)` seeds (arguments: seed, stream number — verbatim token below): **same seed, different stream number yields mutually independent sequences**. This second parameter solves the classic trap: two generators seeded identically produce fully correlated output — when a server hands each session an RNG, using the session number as the stream number guarantees non-correlation. The generation family covers integers (`Rng32`/`Rng64`), floats (`RngReal`, [0,1)), bounded integers (`RngRangeClosed`, the closed-closed interval — a die roll `1..6` including 6), byte filling (`RngBytes`), and in-place shuffling (`RngShuffle`, Fisher-Yates).

### The global layer and threads

Global functions like `xrtRand64` use **thread-local** state — multi-threaded concurrent calls need no locking and never interfere with each other's sequences. This layer suits "just give me a random number"; when a sequence must be consistent across calls (the same logic rerun wanting the same result), return to the RNG instance. The Fast series (`xrtFastRandSeed` to seed, `xrtFastRand32` to draw, etc.) is a speed-first implementation without security guarantees, for confirmed extreme-performance non-security scenarios.

### Random text and alphabets

`RandText` / `RandStringFrom` / `SecureText` generate random strings: you can specify the character set (say only the unambiguous `23456789abcdefghjkmnpqrstuvwxyz`) to generate invitation codes, temporary directory names, and session tokens. The security counterparts are `SecureText` and `SecureStringFrom` — built atop `xrtSecureRandom`, the random text's unpredictability identical to the byte version's.

A custom alphabet is not just cosmetics: excluding lookalikes like `0O1lI` keeps invitation codes from being miscopied by users; restricting to the URL-safe set lets tokens go straight into links and QR codes. The generation length is caller-specified, and failure (entropy source unavailable) is likewise declared by return value — failures on the security path are never silently swallowed.

### Engineering uses of reproducibility

Reproducible randomness has three high-frequency landing points. **Test snapshots**: write the seed into the test fixture, assert the generated sequence's hash (Chapter 12) equals the recorded fingerprint — regression testing of random logic goes from "luck" to exact assertion. **Replay recordings**: store only the seed and the operation sequence; on replay, re-run the generation calls, random events frame-identical. **Fault injection**: use an RNG to decide injection points (combined with Chapter 6's FailAfter for mixed fault modes), the seed being the test-case number — a failing rerun doesn't change the result. The shared discipline of the three: **the order of generation calls is also state** — skip one `Rng32` during reproduction and everything after shifts.

### Reading the three assertion classes of random tests

Before running the examples, be clear how random code is tested — both examples' assertions come from these three classes. **Reproducibility assertions**: generate twice with the same seed, results item-by-item equal (or hash fingerprints equal). **Bounded assertions**: generated values fall in the declared interval, including both endpoints of the closed interval each hit at least once. **Distribution assertions**: over large samples the frequencies approach expectation (judged with Chapter 10's `Near` on relative deviation). Three classes each at their post: reproducibility guarantees determinism, bounds guarantee semantics, distribution guarantees quality — writing only one leaves the corresponding dimension's defects uncaught.

## Examples

### Complete program: the explicit RNG full family

From the repository example `examples/math/random/main.c` — fixed seed, no global state, fully reproducible output:

```embed path="examples/math/random/main.c" title="examples/math/random/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/math/random/main.c -lws2_32 -liphlpapi
explicit: 4207372542
dice    : 6
real    : 0.219369470187
bytes   : 8641
shuffle : 2 1 6
```

**What just happened.** (1) `xrng Rng` is local state on the stack — whoever declares it holds it; putting it in a struct, one per session, or storing it in a replay save are all the same shape. (2) `xrtRngSeed(&Rng, 2026, 7)`: seed 2026, stream number 7; these fifty-odd bytes of sequence are now fully deterministic — any machine at any time rerunning gives these five output lines — the cornerstone of test snapshots and replay recordings. (3) The five generation functions **advance the same state in sequence**: the integer drawn first influences the die rolled later — when reproducing a sequence, the call order must also agree. (4) `RngRangeClosed(&Rng, 1, 6)` is the closed-closed interval; a die roll no longer needs `1 + x % 6` (the double trap of a half-open interval plus modulo bias). (5) `RngShuffle` is an in-place shuffle — the array is still that array, its order uniformly permuted by Fisher-Yates; "uniform" is not rhetoric — every permutation equally probable, the object of the distribution check in this chapter's exercises.

### Complete program: a full tour of the four-layer interface

From `examples/math/random_tour/main.c`, walking each of the four layers and asserting reproducibility and boundedness:

```embed path="examples/math/random_tour/main.c" title="examples/math/random_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/math/random_tour/main.c -lws2_32 -liphlpapi
random: global rand64/below/range/bytes/shuffle ok
random: fast (seeded) 32/64/below/range/real/shuffle ok
random: rng instance ready=1 below32/64/range/text ok
random: secure text from alphabet len=8
```

**What just happened.** (1) The global layer's five in a row: direct calls within a thread, no state declaration. (2) The Fast layer seeds with `xrtFastRandSeed` first, then draws — fast but non-secure, the header comment pinning the boundary. (3) The RNG-instance layer verifies `RngReady` and the bounded versions; (4) the Secure layer generates an 8-character token from a custom alphabet — the length assertion passing completes it (the content of course differs every time). The whole program verifies reproducibility with "same seed, two generations, identical results" and boundedness with range assertions — the two assertion classes your own random tests should copy.

## Contracts

- **Security red line**: keys, tokens, passwords, nonces, temporary resource names, and every security boundary **must** use `xrtSecureRandom` / `SecureText`; `Rng`/`Rand`/`FastRand` are all statistical — declared doubly by documentation and naming.
- **Reproducibility contract**: same seed (stream number included) and same call sequence → same output, stable across platforms and versions (the consistency of `xrtMath` supports it underneath).
- **Independent streams**: multiple instances seeded with different stream numbers are mutually independent; same seed, same stream = a fully correlated copy — check the seeding lines before parallel simulations.
- **Threading**: the global Rand is thread-local, lock-free concurrency; an RNG instance belongs to the scope that declared it, and cross-thread sharing needs your own synchronization.
- **Interval semantics**: `RngRangeClosed` is closed-closed (both endpoints included); a hand-written `x % n` carries modulo bias — don't.
- **Order is state**: when reproducing a sequence, the kinds and ordering of generation calls must exactly match the recording.

## Pitfalls

### Pitfall 1: generating security tokens with statistical randomness

Symptoms: the security audit flags tokens as predictable; in principle an attacker observing a few outputs can reconstruct the internal state and compute every later token.

Cause: PCG and the Fast series are statistical — reproducibility and unpredictability are opposed by design; the former dooms the latter.

```c bad
xrng Rng;
xrtRngSeed(&Rng, (uint64)time(NULL), 1);   /* the time is guessable */
xrtRngBytes(&Rng, Token, sizeof(Token));   /* PCG is predictable — the token is decoration */
```

```c good
uint8 Token[16];
if ( !xrtSecureRandom(Token, sizeof(Token)) ) {
	return false;                            /* entropy failure treated as failure */
}
/* xrtSecureZero(Token, sizeof(Token)) when done (Chapter 5) */
```

### Pitfall 2: two RNGs seeded identically

Symptoms: multiple "independent" random streams highly correlated in a parallel simulation — identical deck orders, synchronized load patterns, distorted statistical conclusions.

Cause: same seed, same stream number produces **exactly the same** sequence; "multiple instances" does not imply "multiple sequences".

```c bad
for ( i = 0; i < 4; ++i ) {
	xrtRngSeed(&Workers[i].Rng, 2026, 1);  /* four workers, one sequence */
}
```

```c good
for ( i = 0; i < 4; ++i ) {
	xrtRngSeed(&Workers[i].Rng, 2026, (uint64)i + 1u);  /* stream numbers split the flows */
}
```

## Exercises

### Basic: reproduction snapshot

Draw ten consecutive integers with a fixed seed and print them; rerun with a different stream number and compare — the two sequences differ; rerun with the original seed, asserting item-by-item equality with the first run. Finally, fingerprint the ten-element sequence with Chapter 12's `Hash64` — future regression of this test compares a single 64-bit value.

### Advanced: a dice distribution check

Roll `RngRangeClosed(&Rng, 1, 6)` a hundred thousand times, tally the frequencies of 1–6, and use Chapter 10's `Near` to verify each face's relative deviation from expectation (a hundred thousand divided by six) is under one percent. Then a control: deliberately generate with `1 + (int)(xrtRng32(&Rng) % 6)` and observe whether the deviation is still acceptable — appreciate whether "modulo bias" is actually a problem in small ranges, and write down your conclusion.

### Challenge: a replay-system skeleton

Design a minimal session replay: store the opening seed (stream number included) and the player's operation sequence in a save; on "replay", re-run the seed and the operations, asserting every random event (every drop, every roll) matches the live recording item by item. Acceptance: saves from two different seeds replay self-consistently and differently from each other; the same save replayed twice gives item-by-item identical results.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Four layers | Secure (entropy source) / global Rand (thread-local) / Fast (speed) / RNG instance (reproducible) |
| Security red line | tokens, keys, nonces only via `SecureRandom` / `SecureText` |
| Seeding | `RngSeed(&Rng, 种子, 流序号)`; stream numbers split the flows |
| Interval | `RngRangeClosed` closed-closed with endpoints; `x % n` rejected |
| Reproduction | same seed, same call order → same output; call order is also state |
| Text | `SecureStringFrom` generates tokens from a custom alphabet; excluding lookalikes prevents miscopying |
