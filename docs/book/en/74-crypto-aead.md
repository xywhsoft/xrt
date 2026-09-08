---
num: 74
slug: crypto-aead
title: Cryptography (Part 2): AEAD Encryption
volume: 卷八 安全
type: practice
lead: AES-GCM and ChaCha20-Poly1305 — confidentiality, integrity, and authentication in one package, plus the nonce discipline that is the lifeline.
api: crypto, random
---

## Orientation

The last chapter settled "content unmodified" and "where keys come from"; this one settles "content invisible" — and packages three security properties at once. AEAD (Authenticated Encryption with Associated Data) is the only recommended shape of modern symmetric encryption: it computes an authentication tag while encrypting, and verifies the tag before decrypting — on mismatch it rejects wholesale, never emitting "possibly tampered plaintext". XRT provides two peer implementations: AES-GCM (first choice on platforms with AES-NI hardware) and ChaCha20-Poly1305 (the constant-time first choice without hardware acceleration). This chapter covers both families' state-machine and stateless API shapes, AAD's purpose binding, and AEAD's lifeline — **nonce management**: repeat a nonce once under the same key, and confidentiality and integrity collapse together. Chapter 86's TLS record layer is a term-by-term application of this chapter's primitives.

## Introduction

Imagine designing a configuration-sync service: the client puts encrypted configuration into untrusted object storage; the server fetches and decrypts. Version one is naive — AES-encrypt the plaintext, store the ciphertext, decrypt on retrieval. Before launch, the security review asks one question: **the attacker flips one byte of the ciphertext — what does your decryption do?** The answer: AES decryption "succeeds" and returns garbage — your program now holds data it never wrote, yet it passed verification. This is the classic bit-flipping attack: **encryption provides no integrity**; bare modes like CBC/CTR/ECB are all alike.

So you attach an HMAC to the ciphertext — Encrypt-then-MAC? The wrong order invites length-extension-class flaws; MAC-then-Encrypt? Theoretically safe but extremely easy to get wrong in implementation. A decade of stumbles led cryptography to one conclusion: **do not compose encryption and authentication yourself — use AEAD directly**. Encryption produces "ciphertext + authentication tag"; decryption verifies the tag first, plaintext second; one primitive, three guarantees (confidentiality, integrity, authenticity) — ordering and boundaries are the implementation's job, which is the entire point of this chapter's two families.

## Concepts

### The AEAD trio: one call, three guarantees

The fastest way to understand AEAD is its record format:

```diagram flow
- Input: plaintext + AAD (additional authenticated data, authenticated but not encrypted) + nonce + key
- Encrypt (Seal/Encrypt): plaintext -> ciphertext (same length), plus a 16-byte authentication tag
- Transmission: ciphertext + tag (+ public AAD and nonce) arrive together at the peer
- Decrypt (Open/Decrypt): verify the tag first — on mismatch, fail wholesale; never output plaintext
```

Three points. (1) **The tag is the fingerprint of the whole record**: ciphertext, AAD, nonce, and length all participate — flip any bit and verification fails. (2) **AAD is authenticated but not encrypted**: version headers, record sequence numbers, context identifiers — fields that "must be visible but need no secrecy" — are bound into the authentication scope via AAD; an attacker changing the version number likewise fails verification; this is "purpose binding". (3) **Failure is atomic**: when `Open` returns `false`, the output buffer is undefined; a correct program discards it wholesale rather than "giving it a try".

### AES-GCM: the state-machine shape

`XRT_FEATURE_CRYPTO_AES_GCM` provides the stateful `xaesgcm`:

- `xrtAesGcmInit(状态, 密钥, 密钥长, 标签长)`: **fixes and binds** an AES-128/192/256 key and tag length (4..16 bytes, default 16) at initialization — the key schedule is expanded once and never repeated per message; a fixed tag length per key also matches protocol contracts.
- `xrtAesGcmEncrypt / Decrypt`: the detached-tag shape — ciphertext and tag go to two buffers; the protocol layer decides their layout.
- `xrtAesGcmSeal / Open`: the contiguous shape — the tag is appended directly after the ciphertext; buffer length = plaintext + tag length; most convenient for network packets.
- `xrtAesGcmClear`: wipes the key schedule from the state; clear when done.
- Nonce recommendation is 12 bytes (`XRT_AES_GCM_NONCE_DEFAULT_SIZE`); the implementation automatically takes the AES-NI hardware path.

### ChaCha20-Poly1305: the stateless convenience layer

`XRT_FEATURE_CRYPTO_CHACHA20_POLY1305` implements RFC 8439: 32-byte key, 12-byte nonce, 16-byte tag — a fixed trio (no tag-length choice — the protocol says 16). The API likewise has detached (`Encrypt/Decrypt`) and contiguous (`Seal/Open`) forms, but is **stateless**: each call takes the key directly, maintaining no external key schedule. The trade-off: no state management, at the cost of reprocessing the key per message — high-frequency same-key scenarios (like the TLS record layer) internally use the stateful variant; occasional encryption (config files, tokens) uses the convenience layer most directly. ChaCha20 is a software stream cipher, **constant-time and table-free** — on ARM devices and IoT targets without AES hardware it is both faster than software AES and side-channel resistant. `XRT_CHACHA20_POLY1305_OVERHEAD` gives the fixed overhead of ciphertext over plaintext (the 16-byte tag); write macros, not numbers, in buffer calculations.

### Nonce discipline: AEAD's lifeline

**Under the same key, a nonce must never repeat** — this is not a style suggestion but a mathematical boundary: one repeated GCM nonce lets the authentication key be recovered, exposing every record that key ever protected (including past ones). Two safe management schemes:

- **Counter**: each record's nonce = random starting value + incrementing sequence; the sender guarantees monotone non-reuse — best performance, requires single-point ordering (the key is used on exactly one send path).
- **Random**: a 96-bit random nonce per record — under the birthday bound the repeat probability becomes significant after about 2^32 records under one key; suits short-lived keys (one session).

Chapter 76's session example uses the combination "AAD binds the record sequence number + counter". Key rotation itself (discard at session end) is the last line of insurance — an ephemeral key paired with a counter nonce is TLS 1.3's standard posture.

### The relation to bare ChaCha20 stream cipher

`XRT_FEATURE_CRYPTO_CHACHA20` also provides the bare ChaCha20 transform (no authentication) — it exists only as the foundation of the Poly1305 composition and as an interop piece for special protocols. The discipline is isomorphic to MD5's: **application-layer encryption always goes AEAD; bare encryption appears only where a protocol explicitly requires it**. Likewise for AES: the `CRYPTO_AES` block-cipher layer is depended on by GCM and never touched directly by application code.

## Examples

### First complete program: AES-GCM seal and open

The following program comes from `examples/crypto/aes_gcm/main.c`, encrypting in place then decrypting a message under a fixed key, with AAD binding a version header:

```embed path="examples/crypto/aes_gcm/main.c" title="examples/crypto/aes_gcm/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/crypto/aes_gcm/main.c -lws2_32 -liphlpapi
hello from aes-gcm
```

**What just happened.** (1) Key and nonce buffers are declared with `XRT_AES256_KEY_SIZE`/`XRT_AES_GCM_NONCE_DEFAULT_SIZE` — lengths always from macros; the all-zero key is for reproducibility only; production keys come from a KDF or the randomness source. (2) After `Init` binds the key and 16-byte tag, `Seal` encrypts in place: the output buffer is the input buffer (overlap allowed), writing 19 bytes of ciphertext plus the appended 16-byte tag, 35 bytes total — capacity 64 is ample. (3) `Open` takes the 35 bytes of "ciphertext+tag", verifies the tag, and decrypts in place; a mismatched tag returns `false` with the buffer untrusted. (4) The AAD `"message-v1"` is passed identically on both sides — it never enters the ciphertext, but it enters the tag: changing the version number makes opening fail; this is "visible but untouchable". (5) The trailing `Clear` wipes the key schedule — the state struct holds the expanded key, treated exactly like the plaintext key.

### Second complete program: the ChaCha20-Poly1305 convenience layer

The second program comes from `examples/crypto/chacha20_poly1305/main.c`, one stateless pair of entries doing authenticated in-place encryption/decryption:

```embed path="examples/crypto/chacha20_poly1305/main.c" title="examples/crypto/chacha20_poly1305/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/crypto/chacha20_poly1305/main.c -lws2_32 -liphlpapi
authenticated message
```

**What just happened.** (1) No state struct — the key is used and gone: `Seal` directly takes Key/Nonce/AAD/plaintext and outputs ciphertext + 16-byte tag (`iSealedSize = 明文长 + XRT_CHACHA20_POLY1305_OVERHEAD`). (2) The key is filled with the `0x40+i` pattern and the nonce increments byte-wise — fixed material reproduces the output; the header comment explicitly reminds "in production, nonces under the same key must be unique". (3) The AAD `"header"` demonstrates protocol-header binding; both calls share one buffer in place. (4) The trailing `xrtSecureZero(Key, ...)` wipes the key — the convenience layer has no state to Clear; the key array itself is the sensitive material.

### Extra: tampering means failure (verify it yourself once)

Both examples walk the "encrypt → decrypt" success path. The key moment in understanding AEAD is **seeing failure with your own eyes**: add one line `Buffer[0] ^= 1;` after `Seal` in the `aes_gcm` example (flipping the first ciphertext bit), then call `Open` — it returns `false` and the program takes the error branch. Switch the tampering to the tag's last byte, or the AAD to `"message-v2"` — failure again. **Flip any bit of any of the three inputs and verification refuses** — that is the weight of the words "authenticated encryption", and the first redemption of this volume's "failure must be single" discipline.

## Contracts

- **AES-GCM state**: `Init` fixes the key (128/192/256) and tag length (4..16, default 16); the key schedule expands once and is reused across messages; `Clear` wipes the state; `TagSize` queries the bound value.
- **Two call shapes**: `Encrypt/Decrypt` detach ciphertext and tag (protocol defines layout); `Seal/Open` keep the tag right after the ciphertext (best for packets); both share the same key schedule and security.
- **ChaCha20-Poly1305**: RFC 8439 fixed parameters (32 key/12 nonce/16 tag); the stateless convenience layer processes the key per message; TLS internally uses the stateful variant; constant-time software implementation.
- **Nonce**: uniqueness under the same key is a mathematical boundary — counter (monotone, single-point ordering) or 96-bit random (short-lived keys); the nonce may transit in the clear; it is the key that is secret, not the nonce.
- **AAD**: authenticated but not encrypted; version headers/sequence numbers/context are bound via AAD; both ends must agree byte for byte.
- **Failure semantics**: `Open/Decrypt` on tag mismatch fails wholesale with an undefined output buffer — a correct program discards the whole record; there is no "partial decryption".
- **Overlap**: `Seal/Open` support in-place (input and output the same buffer); the detached shape's output and tag buffers must not overlap the input (checked by contract).
- **Errors**: parameter/capacity errors return before the first output write; illegal key lengths (not 128/192/256) are refused outright.
- **Trimming**: AES, AES-GCM, ChaCha20, Poly1305, and the composition layer are all separate feature macros — an IoT target carrying only ChaCha20-Poly1305 ships no AES code at all.

## Pitfalls

### Pitfall 1: nonce reuse

Symptoms: the security audit fails outright; worse, nobody tells you — a repeated GCM nonce can recover the authentication key, giving the attacker the ability to **forge arbitrary records**, undetectable from the ciphertext afterwards.

Cause: GCM's security proof rests on "no nonce repeats under one key". Random 12-byte nonces with a fixed long-term key over massive record counts grow repeat probability quadratically — significant after 2^32 records.

```c bad
static const uint8 Nonce[12] = { 0 };  /* fixed nonce */
for ( each record ) {
	xrtAesGcmSeal(&State, Nonce, sizeof(Nonce),
		NULL, 0, Plain, iSize, Out, sizeof(Out));
	/* from the second call on: the authentication key progressively leaks */
}
```

```c good
uint64 iCounter = 0;
uint8 Nonce[XRT_AES_GCM_NONCE_DEFAULT_SIZE];
for ( each record ) {
	memset(Nonce, 0, sizeof(Nonce));
	/* counter nonce: single-point monotone increment under one key — never repeats */
	memcpy(Nonce + 4, &iCounter, sizeof(iCounter));
	iCounter++;
	xrtAesGcmSeal(&State, Nonce, sizeof(Nonce),
		NULL, 0, Plain, iSize, Out, sizeof(Out));
}
```

### Pitfall 2: using the output buffer after Open fails

Symptoms: garbage "plaintext" appears in logs; the downstream parser receives data never written; or more insidiously — the error branch forgets the return and keeps running with the polluted buffer.

Cause: on tag mismatch `Open` **fails wholesale with undefined output** — possibly partially written, possibly stale. Treating "there may be something in the buffer on failure" as "the buffer holds the original on failure" is a fatal misunderstanding.

```c bad
uint8 Plain[64];
if ( !xrtAesGcmOpen(&State, Nonce, 12, pAad, iAad,
		Cipher, iCipherSize, Plain, sizeof(Plain)) ) {
	/* forgot the return: continuing with undefined Plain */
}
parse(Plain);   /* processing data under attacker influence */
```

```c good
if ( !xrtAesGcmOpen(&State, Nonce, 12, pAad, iAad,
		Cipher, iCipherSize, Plain, sizeof(Plain)) ) {
	return false;   /* the whole record is void: this is the only correct action */
}
parse(Plain);
```

### Pitfall 3: hand-composing "AES encryption + HMAC authentication"

Symptoms: the composition seems to run but review refuses it; or it really gets attacked — Encrypt-then-MAC is theoretically correct, but any hand-made slip in length boundaries, nonce binding, or comparison timing directly breaks security.

Cause: a decade of cryptographic-engineering lessons says composition errs easily — ordering, coverage, constant-time comparison, key separation (one key doing both encryption and MAC is disaster); each has famous failure cases. AEAD internalizes all these decisions.

```c bad
/* bare block/stream encryption for ciphertext, then a hand-added HMAC */
encrypt_only(Key1, Nonce, Plain, iSize, Cipher);
xrtHmacSha256(Key2, ..., Cipher, iSize, Mac);
/* ordering, coverage, key separation... every step is attack surface awaiting review */
```

```c good
xrtAesGcmSeal(&State, Nonce, 12, pAad, iAad,
	Plain, iSize, Out, sizeof(Out));
/* or ChaCha20-Poly1305: one call — confidentiality + integrity + authentication */
```

## Exercises

### Basic: round-trip and the tampering triple

After running the `aes_gcm` example, do three experiments: flip one ciphertext byte, flip one tag byte, change one AAD character — `Open` must fail in all three. Turn the three failure paths into printing `rejected` and returning non-zero. Acceptance: all three failures print identically (you cannot tell which tampering it was — that is a feature, not a defect).

### Advanced: a configuration encrypter with version migration

Implement `seal_config(密钥, 版本号, JSON, 输出)` and `open_config`: AAD binds `"config-v1"` and the version number (the version goes into the AAD string); `Seal/Open` use the ChaCha20-Poly1305 convenience layer. Deliberately open a v2 record with the v1 key — it must fail. Hint: build the AAD from the version string via one shared function on both sides, avoiding hand-copied divergence.

### Challenge: an encrypted record-stream channel

Over one TCP connection (Chapter 66's synchronous face suffices) implement a simple encryption protocol: handshake with a fixed test key, then each record = 8-byte sequence number (cleartext) + Seal's ciphertext and tag; the nonce is filled from the sequence number (counter method), and the sequence number also goes into the AAD against reordering. The receiver verifies strictly increasing sequence numbers and disconnects on disorder. Acceptance: split records with Chapter 71's framer; attack experiments — replay of old records, reordering, tampering — all three disconnect; 2^16 records with no memory growth (Chapter 6's stats).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| AEAD's three guarantees | confidentiality + integrity + authentication in one call; atomic failure (no suspicious plaintext emitted) |
| AES-GCM state machine | `Init` binds key and tag length (4..16, default 16) → key schedule expanded once; `Clear` wipes state |
| Two shapes | `Encrypt/Decrypt` detach the tag; `Seal/Open` append it after the ciphertext (+16 bytes) |
| ChaCha20-Poly1305 | RFC 8439 fixed 32/12/16; stateless convenience layer; constant-time, first choice without AES hardware |
| Selection | AES-NI available → AES-GCM; ARM/IoT → ChaCha20-Poly1305; the two are peers and interchangeable |
| Nonce discipline | uniqueness under one key is a mathematical boundary; counter (single-point monotone) or 96-bit random (short-lived keys) |
| AAD | authenticated but not encrypted; version/sequence/context binding; byte-for-byte agreement between ends |
| Failure semantics | `Open` failure leaves output undefined; discarding the whole record is the only correct action |
| In-place | `Seal/Open` support the same input/output buffer; the detached shape's tag buffer is separate |
| Trimming | AES/AES-GCM/ChaCha20/Poly1305/composition layer all separate macros; bare transforms only for protocol interop |
