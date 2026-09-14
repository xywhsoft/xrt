---
num: 77
slug: crypto-discipline
title: Cryptographic Engineering Discipline: From Algorithms to Systems
volume: 卷八 安全
type: practice
lead: Constant time, key lifecycle, randomness sources, and composition discipline — security is decided not by algorithm choice but by every call at the system layer.
api: crypto, random
---

## Orientation

Chapters 74 through 75 gave you every algorithmic primitive — but the systems broken in the real world mostly were not broken at the algorithm; they were **misused at the system layer**: keys in logs, comparison functions leaking timing, wrong randomness sources, homemade protocol compositions. This chapter is the user manual for the preceding three, and the discipline summary before Volume 8 moves from primitives to certificates and TLS. Four themes: **constant-time discipline** (why MAC comparison must not exit early), **key lifecycle** (the rules and APIs of each of the four stages generate–use–store–destroy), **randomness-source discipline** (the boundary between secure and ordinary randomness), and **composition discipline** (no inventing protocols, single failure, context binding). The finale example `crypto/session` assembles the three chapters' primitives into one complete ephemeral session — it is a miniature of the TLS 1.3 skeleton; Chapter 82's handshake merely adds certificates and a state machine on top of it.

## Introduction

Consider three abstractions of real incidents. One: a service's session-token check used `memcmp` — the attacker measured response times and probed the valid token byte by byte: a correct first byte makes the response some tens of nanoseconds slower, and a million requests "feel out" the whole token. Two: a developer cleared a key with `memset` before freeing the memory — the compiler judged that memory "never read again" and deleted the zeroing entirely, leaving the plaintext key in the heap for later allocations to pick up. Three: two modules each generated "random" challenge values from a predictable ordinary randomness source — the attacker predicted the outputs and authentication was decoration.

The common thread: **the algorithms were all correct, the calls all wrong**. A cryptographic API's security holds only in the context of correct use — `xrtConstTimeEqual`, `xrtSecureZero`, and `xrtSecureRandom`, three unassuming functions, are XRT's exits prepared for these three incident classes. This chapter makes them, and the larger composition picture, clear.

## Concepts

### The three-layer failure model: which layer does misuse live on

Think of a security system as three layers; the attacker always picks the softest:

```diagram flow
- Algorithm layer: the mathematical strength of AES/SHA/curves — guaranteed by this volume's primitives, least likely to be the weak link
- Protocol layer: field order, state machines, context binding, failure paths — the high-incidence zone of homemade protocols
- System layer: key storage, randomness provenance, comparison method, log contents — this chapter's main battlefield
```

A practical self-check: **for every line of crypto-related code you write, ask "which layer does this line break"**. Comparing ciphertexts with `memcmp` breaks the system layer; "verification failed but continue processing" breaks the protocol layer; choosing MD5 breaks the algorithm layer. Of the three, the algorithm layer is least likely to err (using the right API suffices), so an engineer's attention should be allocated in reverse — most on the system layer, then the protocol layer, least on the algorithm layer. This chapter's pitfall list comes entirely from the system layer.

### Constant-time discipline: comparison is attack surface too

`memcmp(Left, Right, N)` **returns early at the first differing byte** — that "speed" is the vulnerability: comparison time is proportional to the length of the common prefix, and an attacker timing remotely recovers the secret (token, MAC, key) byte by byte. `xrtConstTimeEqual(Left, Right, N)` accumulates differences bit by bit and judges at the end — **running time depends only on N**. Two precise boundaries: it does not hide N itself (length is public); nor is it full side-channel protection (power and cache are another dimension) — but on "remote timing", the most realistic attack surface, it is a necessity. Applies to: MAC/tag comparisons, token comparisons, shared-secret comparisons, ciphertext-fingerprint comparisons — **any comparison where one side is a secret**. Comparing data public on both sides (file checksums, cache keys) can use `memcmp`; constant time is not free performance.

### The four stages of the key lifecycle

```diagram state
generate -> use: compliant source (randomness/KDF/exchange), never hard-coded
use -> store: minimal residence (stack/dedicated struct), never into logs or dumps
store -> destroy: clear immediately upon leaving scope
destroy -> final: xrtSecureZero or the dedicated Clear, covering every copy
```

- **Generate**: symmetric keys and private keys always come from `xrtSecureRandom` (next section) or KDF derivation; fixed test-vector keys must be marked "reproduction only" as the examples do.
- **Use**: keys live only in stack buffers or dedicated state structs (`xaesgcm`'s key schedule, `xed25519key`'s expanded state); **never copy them to other buffers** — every extra copy widens the destruction surface.
- **Store**: keys **never** enter logs (Chapter 38's Logger included), error messages, or persistent structures that reach swap files (this exceeds `xrtSecureZero`'s power — its contract explicitly covers only the range handed to the call; compiler register copies, crash dumps, and OS swapping need system-level measures: mlock, disabled dumps, full-disk encryption).
- **Destroy**: `xrtSecureZero` zeroes with volatile writes; plain `memset` may be deleted by the compiler as a dead store (the second opening incident); stateful objects use their dedicated `Clear`/`KeyClear` (it knows every sensitive corner of the struct). **Clear on the success path as well as the error path** — this chapter's examples' cleanup sections are the template.

### Randomness-source discipline: two randomnesses must not be mixed

XRT splits randomness into two worlds: ordinary randomness (Chapter 11, sequences seeded by `xrtFastRandSeed` — fast, seedable, reproducible) for test data, load distribution, and shuffling; **secure randomness** (`xrtSecureRandom`) for every key, token, salt, and nonce — the platform CSPRNG (Windows CNG, Linux getrandom), unseedable and unpredictable. The touchstone of the boundary: **would predicting this value have security consequences?** If yes, use secure randomness — even if it is two orders of magnitude slower. The reverse rule matters too: secure randomness is not for reproducible scenarios (test vectors, benchmark data) — not for performance, but because deterministic output is simply not its promise. The crypto module depends on the randomness source through the separate `RANDOM_SECURE` feature (Chapter 74's trimming table), so file and network modules reusing basic randomness never drag in cryptographic algorithms — this boundary lets "randomness seriousness" and "module lightness" coexist.

### Composition discipline: don't invent protocols

With primitives in hand, the greatest temptation is "assemble my own encryption protocol" — four alternatives and one iron rule:

- **Find an existing protocol first**: transport security goes directly to Chapter 82's TLS; tokens use HMAC or signatures; file encryption uses single-record AEAD. Every decision of a homemade protocol (field order, handshake state machine, failure semantics) re-treads pits the industry already fell into.
- **Failure must be single**: a verification failure is a failure — no "degrade and continue", no "warn but admit". Downgrade attacks (forcing both sides back to weak parameters) exploit exactly the "continue after failure" path.
- **Context binding**: derived keys bind purpose with `info` (Chapter 74); encrypted records bind sequence numbers and versions with AAD (Chapter 75); the handshake transcript binds all handshake messages (Chapter 82) — **every layer must write "who I am, which record" into its authentication scope**, so an attacker cannot move a legitimate ciphertext elsewhere for replay.
- **Prefer ephemeral keys**: use session-ephemeral keys over long-term ones whenever possible — the leak radius determines the incident's size. TLS 1.3's forward secrecy is this discipline's extreme: a fresh x25519 key pair per connection, the long-term private key only signing the handshake and never encrypting.

## Examples

### First complete program: the entries of two iron rules

The following program comes from `examples/crypto/core/main.c` — digest metadata, constant-time comparison, and secure zeroing, three pieces in one demonstration:

```embed path="examples/crypto/core/main.c" title="examples/crypto/core/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/crypto/core/main.c -lws2_32 -liphlpapi
equal: yes
```

**What just happened.** (1) `xrtCryptoHashSize` is pure metadata — querying SHA-256's length does not require compiling in SHA-256; capacity calculations decouple from build configuration. (2) Two digest buffers compare with `xrtConstTimeEqual` — here it demonstrates "identical contents"; production uses it for "the MAC I computed vs the MAC I received": even if an attacker's forged MAC differs from the true value only in the last byte, the comparison time is identical, leaving timing attacks nowhere to start. (3) Immediately after comparing, both buffers get `xrtSecureZero` — they simulate key material; volatile writes guarantee the zeroing never disappears. The whole program is under 20 lines, yet these are the three most frequent functions of the volume.

### Second complete program: one complete ephemeral session (a miniature TLS 1.3 skeleton)

The second program comes from `examples/crypto/session/main.c`, chaining Chapters 74/75/76's primitives into a real protocol shape:

```embed path="examples/crypto/session/main.c" title="examples/crypto/session/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/crypto/session/main.c -lws2_32 -liphlpapi
session round trip: valid
```

**What just happened.** Three security design points, each mapping to a discipline of this chapter. (1) **Both parties' key pairs**: `xrtX25519KeyPair` samples from the secure randomness source — each party's ephemeral private key never leaves home; the shared secret is computed with `xrtX25519Shared` (the safe entry, resistant to malicious public keys) and cross-checked with `xrtConstTimeEqual`. (2) **Derivation binds identity**: the raw shared secret **never goes directly into AEAD** (the positive demonstration of Chapter 76's Pitfall 2) — HKDF's salt is the protocol version string, and info is the **Transcript of both public keys concatenated**: the derived session key simultaneously binds "both identities of this negotiation"; a man-in-the-middle swapping either public key makes the two ends derive different keys, and subsequent messages fail mutual verification (the engineering form of unknown-key-share defense). (3) **Records bind sequence numbers**: the AAD is `"record:0"` — record zero first, incrementing afterwards; replaying an old record or reordering both get rejected by `Open` on AAD mismatch. The closing cleanup section `xrtSecureZero`s **five classes of sensitive material** (both private keys, both shared secrets, the session key, plaintext buffers) one by one — this is the template code of the key lifecycle's fourth stage. Compare with Chapter 82: what TLS 1.3's handshake adds on top is only certificate verification and a fuller state machine — **you already own the entire skeleton**.

## Contracts

- **Constant time**: `xrtConstTimeEqual`'s running time depends only on the length parameter; it hides neither length nor all side channels; mandatory for every comparison where one side is a secret.
- **Secure zeroing**: `xrtSecureZero` uses volatile writes and survives dead-store elimination; it covers only the range handed to the call — compiler register copies, logs, swap files, and crash dumps need system-level measures; stateful objects use their dedicated `Clear`/`KeyClear`.
- **Randomness source**: secure uses (keys/salts/nonces/tokens) always `xrtSecureRandom`; reproducible uses (tests/benchmarks) always ordinary randomness; the crypto module depends on secure randomness through the separate `RANDOM_SECURE` feature.
- **Keys enter no non-volatile records**: logs, error messages, exception reports, debug prints carry zero key content — the "debugging convenience" of hex-dumping a key is a leak incident.
- **Single failure**: a verification failure is a failure, no degraded continuation; the error path performs the same sensitive-material cleanup as the success path.
- **Context binding**: the KDF's info, AEAD's AAD, and the handshake's Transcript each bind purpose/sequence/identity — ciphertext is inseparable from its context.
- **Ephemeral keys**: a fresh key pair per session (forward secrecy); long-term keys only sign, never encrypt data.
- **Composition priority**: existing protocol > standard primitive composition > homemade protocol; every step of a homemade protocol needs security review.

## Pitfalls

### Pitfall 1: comparing secrets with memcmp

Symptoms: a remote timing attack recovers the token/MAC byte by byte — response time varies with the common-prefix length; the attacker locks in each byte with a few thousand requests.

Cause: `memcmp` returns early at the first differing byte. Functional tests pass completely (the result is right); the security property is completely absent (timing leaks).

```c bad
uint8 Mac[XRT_SHA256_SIZE];
xrtHmacSha256(Key, sizeof(Key), Body, iSize, Mac);
if ( memcmp(Mac, SentMac, sizeof(Mac)) == 0 ) {
	accept();   /* timing side channel: a correct prefix returns slower */
}
```

```c good
uint8 Mac[XRT_SHA256_SIZE];
xrtHmacSha256(Key, sizeof(Key), Body, iSize, Mac);
if ( xrtConstTimeEqual(Mac, SentMac, sizeof(Mac)) ) {
	accept();   /* time depends only on length — no prefix information */
}
```

### Pitfall 2: clearing keys with memset

Symptoms: a memory audit finds plaintext keys lingering in freed memory; disassembly shows the zeroing code never existed.

Cause: the compiler sees "no read after memset" and deletes the zeroing as dead-store optimization — permitted by the C standard, since the post-memset value is indeed unused on the abstract machine.

```c bad
void handle_key(uint8* pKey, size_t iSize) {
	use(pKey, iSize);
	memset(pKey, 0, iSize);   /* deleted by DSE */
}
```

```c good
void handle_key(uint8* pKey, size_t iSize) {
	use(pKey, iSize);
	xrtSecureZero(pKey, iSize);   /* volatile writes — the optimizer cannot delete */
}
```

### Pitfall 3: debug logs printing keys/tokens

Symptoms: the log platform (and all its downstreams: indexing, backups, third-party analytics) now holds production keys; the emergency key-rotation response takes a whole night — logs are irrevocable.

Cause: the "just peek at the key" debug print from development entered the default log level, and nobody remembered to remove it at release. Once a key enters the logging system, its uncontrolled spread is complete.

```c bad
/* the "convenient one-liner" while troubleshooting */
log_debug("key=%02x%02x... nonce=%02x%02x...",
	Key[0], Key[1], Nonce[0], Nonce[1]);
```

```c good
/* print only the key's existence and length, never its content */
log_debug("key ready: %zu bytes, nonce ready: %zu bytes",
	iKeySize, iNonceSize);
/* when you truly must verify, print the key's digest fingerprint
   (e.g. the first 8 bytes of SHA-256), not the key itself */
```

## Exercises

### Basic: a timing-comparison experiment

Build two pairs of 32-byte buffers: one pair identical, one differing only in the last byte. Loop-compare each pair ten million times with both `memcmp` and `xrtConstTimeEqual`, timing all four. Acceptance: `memcmp` on "first 31 bytes equal" is clearly faster than "differing at the first byte"; `xrtConstTimeEqual`'s two timings are statistically indistinguishable.

### Advanced: a key audit of your existing code

Return to the record-stream channel you wrote for Chapter 75's exercise; annotate line by line the generation, use, and cleanup of each class of sensitive material (keys/nonces/plaintexts), checking: any secure value generated from ordinary randomness? any secret compared with memcmp? does cleanup cover the success path? Fix everything and record the changes. Acceptance: the audit checklist covers every system-layer item of the three-layer failure model.

### Challenge: upgrading the session example to bidirectional multi-record

Extend `crypto/session`: support any number of records in both directions — maintain a 64-bit record counter, fill the nonce from it (counter method), bind the AAD to "direction + sequence number" (each direction with its own counter); after the last message, both sides actively destroy the session (clear keys and print `session destroyed`). Attack experiments: intercept and replay record 3 — `Open` must fail; out-of-order delivery — must fail. Acceptance: 10 records each way all round-trip; all sensitive material zeroed in the destruction section (cross-check with Chapter 6's stats).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Three-layer failure | algorithm (hardest to break) < protocol < system (most often broken) — allocate attention in reverse |
| Constant time | `xrtConstTimeEqual` depends only on length; mandatory for secret comparisons; hides neither length nor all side channels |
| Secure zeroing | `xrtSecureZero` volatile writes defeat DSE; covers only the given range; stateful objects use `Clear`/`KeyClear` |
| Randomness boundary | secure values (keys/salts/nonces/tokens) → `xrtSecureRandom`; reproducible values → ordinary randomness |
| Key lifecycle | generate (compliant source) → use (minimal residence) → store (never logs/dumps) → destroy (SecureZero) |
| Single failure | a verification failure is a failure; no degraded continuation; the error path cleans up too |
| Context binding | KDF info binds purpose / AAD binds sequence and version / Transcript binds identity |
| Ephemeral keys | a fresh key pair per session; forward secrecy; long-term keys sign but never encrypt |
| Composition priority | existing protocol > standard composition > homemade; the session example is the standard-composition template |
| Session skeleton | KeyPair → Shared → HKDF(Transcript) → AEAD(AAD=sequence) → SecureZero everything |
