---
num: 74
slug: crypto-hash
title: Cryptography (Part 1): Hashes and HMAC
volume: 卷八 安全
type: practice
lead: The digest family, the streaming three-step form, HMAC and the dual KDFs (PBKDF2/HKDF) — the foundation of all integrity and key derivation.
api: crypto, hash, random
---

## Orientation

This volume starts with the oldest and most used primitives. Hash functions compress arbitrary data into fixed-length digests — file checksums, content addressing, and set fingerprints all rest on them; HMAC gives the hash a key, upgrading "integrity" to "authenticated integrity"; KDFs (key derivation functions) turn passphrases or negotiated secrets into real keys. This chapter covers the digest family from MD5 to SHA-512/256, both the streaming three-step and one-shot call forms, HMAC's precomputed state, and the pair of clearly-divided derivation functions PBKDF2 and HKDF. They are the bricks of everything that follows: AEAD keys come from KDFs, TLS's handshake transcript hash is a hash, certificates' signatures sign digests. After this chapter you should answer for any scenario "which digest, with or without a key, derive or use directly".

## Introduction

Three real scenarios. You must generate a checksum for a multi-gigabyte file for downloaders to compare against — the file won't fit in memory. Your service exposes a Webhook callback and must verify "this request really comes from the partner who configured the key" — a bare hash won't do: anyone can hash any content; what you verify is "only key holders can compute it". Your users log in with passphrases and you need a symmetric encryption key from the passphrase — passphrase entropy is low (humans can't remember 256 random bits), and using it directly as a key falls to dictionary attacks instantly.

The three scenarios map to three primitive classes: **digests** (keyless, publicly computable, for fingerprints), **MACs** (keyed, computable only by key holders, for authentication), and **KDFs** (which **derive** suitable key material from low-entropy passphrases or high-entropy negotiated secrets). XRT implements the three in a finely trimmable feature family — SHA-256 and HMAC-SHA-256 are two separate macros, so a checksum-only tool need not compile in MAC code.

## Concepts

### The digest family panorama

XRT ships seven digest algorithms, with length metadata unified by `xrtCryptoHashSize`:

| Algorithm | Digest length | Positioning |
| --- | --- | --- |
| MD5 | 16 bytes | **Historical protocol interop only** (HTTP Digest etc.); collision resistance already broken |
| SHA-1 | 20 bytes | WebSocket and historical protocols; **must not be used in new signature designs** |
| SHA-224 | 28 bytes | Shares the compression implementation with SHA-256 |
| SHA-256 | 32 bytes | The modern default — the workhorse of integrity, HMAC, and signature digests |
| SHA-384 | 48 bytes | Shares the compression core with SHA-512 |
| SHA-512 | 64 bytes | Long digest for 64-bit platforms |
| SHA-512/256 | 32 bytes | A 256-bit variant reusing SHA-512's compression core, resistant to length extension |

First instinct for choosing: **new designs start at SHA-256, always**; touch MD5/SHA-1 only to talk to old protocols, and only for the fields the protocol requires, never for any security decision. `xrtCryptoHashSize(XCRYPTO_HASH_SHA256)` returns 32 — note this is a metadata query, independent of whether the algorithm is compiled in; capacity calculations need not depend on build configuration.

### The streaming three-step and one-shot forms

Data does not always sit whole in memory — multi-gigabyte files and chunked network arrivals all need to **feed a little, hash a little**. The whole digest and HMAC family shares one state machine:

```diagram state
uninitialized -> initialized: Init (writes the digest's initial state)
initialized -> initialized: Update x N (any number of times, any lengths)
initialized -> done: Final (writes out the digest; state consumed)
```

`Init` establishes state; `Update` may be called any number of times at any length — internally it accumulates by block, bit-for-bit identical to a one-shot feed; `Final` writes the fixed-length digest. The one-shot entry `xrtSha256(数据, 长度, 输出)` is the "three steps merged into one" convenience, semantically equivalent. The state structs (`xsha256` etc.) are value objects on the caller's stack — no heap allocation, no shared state, any number of concurrent instances.

### HMAC: giving the hash a key

`xrtHmacSha256(密钥, 密钥长, 数据, 数据长, 输出)` differs from the bare hash signature by only one pair of key parameters, yet the semantics leap in kind: **only key holders can compute the same MAC**. `HmacSha256Init` precomputes the inner/outer digest states — the key is processed once, and no later `Update` ever touches it again. This is the standard practice for "API signing, Webhook verification, session tokens": the sender attaches `HMAC(密钥, 内容)` with the content, the receiver recomputes and compares — with `xrtConstTimeEqual`, not `memcmp` (Chapter 77 explains why).

The HMAC family trims independently per underlying digest: the SHA-256 version depends only on SHA-256; SHA-384/512 share an implementation. Key length is arbitrary (overlong keys are pre-hashed); 32 random bytes are the recommendation.

### The KDF twins: PBKDF2 and HKDF

Two derivation functions solving opposite problems, with surprisingly symmetric signatures — remember the division and you won't confuse them:

- **PBKDF2 (passphrase → key)**: the input is a **low-entropy passphrase**, and the **work factor** (iteration count) raises the attacker's cost — every passphrase guess must also pay the same hundred thousand iterations. `xrtPbkdf2Sha256(口令, salt, 迭代数, 输出)`. The salt must be random per user and stored with the ciphertext — defeating rainbow tables and preventing identical passphrases from deriving identical keys. The derivation performs zero heap allocations and securely clears all intermediate state before returning.
- **HKDF (high entropy → key)**: the input is **already high-entropy material** (a DH shared secret, a master key) needing no slowness — what it solves is **shape and purpose**: turning arbitrary-length entropy into arbitrary-length keys, with the `info` parameter binding purpose. `xrtHkdfSha256(salt, IKM, info, 输出)`. From the same IKM, `info` values `"session"` and `"cookie"` derive unrelated keys — the standard practice of **same-origin, different-purpose isolation**; all of TLS 1.3's key schedule is HKDF-driven.

HKDF opens in three layers, shallow to deep: `Extract` (salt+IKM → fixed-length PRK), `Expand` (PRK+info → arbitrary-length OKM), and a combined function (keeping PRK on the stack, done in one call) — most scenarios use the combined function; only streaming handshakes (like TLS) need the split.

### The boundary with Chapter 12's hash library

XRT also has a non-cryptographic hash library (Chapter 12, the SipHash-family non-crypto hashes) — much faster, but **with no security guarantees**, used only for hash tables, dedup fingerprints, and cache keys. The boundary in one sentence: **anything where attacker-controlled input participates and the result feeds a security decision uses this chapter's cryptographic digests**; pure performance scenarios use Chapter 12.

## Examples

### First complete program: the SHA-256 streaming three-step

The following program comes from `examples/crypto/sha256/main.c`, hashing the two input blocks `"hello "` + `"world"` — SHA-256 of "hello world" is the most-used self-test vector:

```embed path="examples/crypto/sha256/main.c" title="examples/crypto/sha256/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/crypto/sha256/main.c -lws2_32 -liphlpapi
b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9
```

**What just happened.** (1) The `xsha256` state sits on the stack, with the `XRT_SHA256_SIZE` constant declaring the output buffer — lengths always come from macros, never hand-written numbers. (2) Two `Update` calls yield a result bit-for-bit identical to a one-shot `xrtSha256("hello world", 11, ...)` — this is the redemption of "chunked arrivals need no whole buffer": in Chapter 45's file streams and Chapter 67's receive callbacks, digests accumulate exactly this way. (3) The output is raw bytes; the example prints them as 64 hex characters with a `%02x` loop — digest APIs produce bytes only; textualization is the caller's business (Chapter 28's HEX codec fits).

### Second complete program: HMAC — only key holders can compute it

The second program comes from `examples/crypto/hmac_sha256/main.c`, the standard MAC of key `"secret"` over message `"message"`:

```embed path="examples/crypto/hmac_sha256/main.c" title="examples/crypto/hmac_sha256/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/crypto/hmac_sha256/main.c -lws2_32 -liphlpapi
8b5f48702995c1598c573db1e21866a9b825d4a794d169d7060a03605796360b
```

**What just happened.** Read side by side with the first program, only two differences: `Init` takes the key parameter, and the output length is still 32 — but the semantics are a different world. Anyone can compute SHA-256 of `"message"`, so a bare hash cannot prove "who the message came from"; only a party knowing `"secret"` can compute this MAC. In real systems the key comes from configuration or negotiation and uses a full 32 random bytes; the verifying side compares with `xrtConstTimeEqual`. Note HMAC also has the streaming three-step (`HmacSha256Init/Update/Final`); after `Init` precomputes the key state, multi-message scenarios need only one Update per message — `examples/crypto/hash_tour` demonstrates the streaming and one-shot self-consistency of every variant.

### Third complete program: PBKDF2 — making keys from passphrases safely

The third program comes from `examples/crypto/pbkdf2_sha256/main.c`, deriving a 32-byte key from a passphrase with a hundred thousand iterations:

```embed path="examples/crypto/pbkdf2_sha256/main.c" title="examples/crypto/pbkdf2_sha256/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/crypto/pbkdf2_sha256/main.c -lws2_32 -liphlpapi
ecfde924b9512da31933191fd1d31754e252fce15eae769808c3cdda9702cbd5
```

**What just happened.** (1) The parameter order is (passphrase, salt, iterations, output) — 100000 iterations is the work factor: a legitimate user pays a hundred thousand iterations per login (milliseconds, acceptable), and an attacker pays the same per passphrase guess, raising dictionary-attack cost a hundred-thousand-fold. (2) The salt is an explicit 16-byte constant — fixed in the example for reproducibility; production must **generate it randomly per user** and store it with the ciphertext (see Pitfall 3). (3) The ending `xrtSecureZero(arrKey, ...)` wipes the derived key from the stack — key material is cleared the moment it's used up, a fixed gesture throughout this volume. May passphrase and salt overlap? No — output must not overlap input; the contract validates all parameters before the first write.

### Fourth complete program: HKDF — same-origin keys isolated by purpose

The fourth program comes from `examples/crypto/hkdf_sha256/main.c`, one call turning three pieces of material into a session key:

```embed path="examples/crypto/hkdf_sha256/main.c" title="examples/crypto/hkdf_sha256/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/crypto/hkdf_sha256/main.c -lws2_32 -liphlpapi
184de99cd5c9f1af2dee024de950759b818bce38644013e38c890f4745a9ff8e
```

**What just happened.** The three parameters each play their part: `salt` is a public random value (change the salt and it's a whole new key), IKM is the raw entropy (a demo string here; real scenarios use a DH shared secret — Chapter 76), and `info` is the purpose binding. Change `info` to `"cookie"` and run again — the output is completely different: this is "one entropy derives many keys, keys never affecting each other", the purpose isolation. Chapter 77's session example will put HKDF into a complete chain: x25519 negotiation → HKDF derivation → AEAD encryption.

## Contracts

- **Digest family**: seven algorithms (MD5/SHA-1/SHA-224/256/384/512/512_256); `xrtCryptoHashSize` gives length metadata (independent of compilation); streaming three-step and one-shot are bit-for-bit identical; states are stack value objects, no heap allocation, arbitrary concurrency across instances.
- **MD5/SHA-1 boundary**: historical protocol interop only (HTTP Digest, WebSocket); must not be used in new designs for signatures, certificates, content trustworthiness, or passphrase storage.
- **HMAC**: Init precomputes the key state; Update never touches the key again; SHA-256 trims independently, SHA-384/512 share; key length arbitrary, 32 random bytes recommended.
- **PBKDF2**: for low-entropy passphrases only; iterations and output length must exceed zero; output must not overlap passphrase/salt; zero heap allocations, HMAC state and intermediates cleared before returning; maximum output `UINT32_MAX × HashLen` (block counter is 32-bit).
- **HKDF**: for high-entropy material only; three entry layers (Extract/Expand/combined); info binds purpose for same-origin isolation; the combined function keeps PRK on the stack.
- **Core tools**: `xrtConstTimeEqual` constant-time comparison (no early exit on a differing first byte); `xrtSecureZero` volatile zeroing (defeating dead-store elimination); the randomness source comes from the separate `RANDOM_SECURE` feature (Chapter 12) — crypto_core carries no randomness.
- **Errors**: failures declared by return value with details in the thread error slot; parameter/range/block-limit errors all return **before the first output write** — failure never pollutes the output buffer.

## Pitfalls

### Pitfall 1: choosing MD5 or SHA-1 for a new design

Symptoms: failing audits, integration refused by dependencies; in worse luck, an attacker constructs two contents with the same digest (MD5 collisions have long been a seconds-scale operation).

Cause: MD5's and SHA-1's collision resistance is broken. XRT compiles them in because HTTP Digest and the WebSocket protocol still require them — interop need is not security usability.

```c bad
uint8 Digest[16];
xrtMd5(Content, iSize, Digest);   /* "integrity check" of a new design */
store(Digest);                     /* an attacker can craft colliding content */
```

```c good
uint8 Digest[XRT_SHA256_SIZE];
xrtSha256(Content, iSize, Digest);  /* new designs default to SHA-256 */
store(Digest);
/* use MD5 only for fields a protocol explicitly requires (e.g. Digest auth),
   and never feed its result into any security decision */
```

### Pitfall 2: using a bare hash for "authentication"

Symptoms: forged requests pass freely — "I hashed and it matched" stops no attacker, because the attacker can compute the same hash.

Cause: hashes are keyless and computable by anyone. Integrity checking (against transmission errors) and authenticated integrity (against malicious construction) are two things, separated by a key.

```c bad
/* request carries ?sig=sha256(body); the server recomputes and compares */
uint8 Digest[XRT_SHA256_SIZE];
xrtSha256(Body, iSize, Digest);
if ( memcmp(Digest, SentSig, sizeof(Digest)) == 0 ) {
	accept();   /* the attacker edits body, recomputes sig, and forges away */
}
```

```c good
/* HMAC: only key holders can produce a valid MAC */
uint8 Mac[XRT_SHA256_SIZE];
xrtHmacSha256(ApiKey, sizeof(ApiKey), Body, iSize, Mac);
if ( xrtConstTimeEqual(Mac, SentMac, sizeof(Mac)) ) {
	accept();
}
```

### Pitfall 3: PBKDF2 salt shared globally or hard-coded

Symptoms: two users share a passphrase → derived keys are identical → one key leak exposes the other's data too; precomputed rainbow tables work against the whole database.

Cause: the salt's mission is "make every derivation input unique". A global salt is no salt — the attacker can precompute one passphrase dictionary for that salt.

```c bad
static const uint8 Salt[16] = { /* hard-coded in the source */ };
xrtPbkdf2Sha256(Password, iLen, Salt, sizeof(Salt),
	100000, Key, sizeof(Key));
```

```c good
uint8 Salt[16];
if ( !xrtSecureRandom(Salt, sizeof(Salt)) ) {  /* random per user */
	return false;
}
xrtPbkdf2Sha256(Password, iLen, Salt, sizeof(Salt),
	100000, Key, sizeof(Key));
save_with_ciphertext(Salt);   /* the salt is not secret; store it with the ciphertext */
```

## Exercises

### Basic: a file digest tool

With Chapter 45's file reading + this chapter's streaming three-step, implement SHA-256 digest printing (64 hex characters) for any path. Acceptance: reading in 1 KiB chunks and in one shot give identical results for the same file; output matches the system tool (`sha256sum` / `certutil -hashfile ... SHA256`).

### Advanced: a Webhook signature verifier

Implement `bool verify_webhook(cstr sBody, const uint8* pKey, size_t iKeySize, const uint8* pSentMac)`: recompute with HMAC-SHA-256 and compare with `xrtConstTimeEqual`. Then deliberately switch the comparison to `memcmp` and think from the timing-attack perspective Chapter 77 will cover: why must this be constant time? Write it into your code comments.

### Challenge: a multi-purpose key manager

Implement `derive_keys(IKM, iSize) → {会话密钥, cookie 密钥, 备份密钥}`: use HKDF-SHA-256 with the same IKM and three different `info` values to derive three 32-byte keys. Then verify isolation: flip one byte of IKM and all three keys change completely; fix IKM and re-derive — results are bit-for-bit identical. Finally `xrtSecureZero` all three keys. Acceptance: zero heap allocations throughout (PBKDF2/HKDF themselves allocate nothing; your wrapper may not either).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Algorithm choice | new designs start at SHA-256; MD5/SHA-1 for protocol interop only; 64-bit long digests use SHA-512 |
| Length metadata | `xrtCryptoHashSize(XCRYPTO_HASH_SHA256)` etc.: 16/20/28/32/48/64, independent of compilation |
| Streaming three-step | `Init → Update×N → Final`; bit-identical to one-shot; stack value states, arbitrary concurrency |
| HMAC | Init precomputes the key state; only key holders can compute; compare with `xrtConstTimeEqual` |
| PBKDF2 | low-entropy passphrase + work factor (iterations); salt random per user, stored with the ciphertext; intermediates cleared before return |
| HKDF | high-entropy material; Extract/Expand/combined three layers; info binds purpose for same-origin isolation; the TLS 1.3 engine |
| Core tools | `xrtConstTimeEqual` constant-time comparison; `xrtSecureZero` volatile zeroing |
| Boundary with Chapter 12 | security decisions use this chapter; hash tables/fingerprints/cache keys use Chapter 12's non-crypto hashes |
| Error contract | parameter errors return before the first output write; output must not overlap input (PBKDF2) |
| Trimming | SHA/HMAC/PBKDF2/HKDF all independent feature macros; digest-only builds carry no MAC |
