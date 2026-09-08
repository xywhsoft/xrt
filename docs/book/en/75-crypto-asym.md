---
num: 75
slug: crypto-asym
title: Cryptography (Part 3): RSA, ECDSA, Ed25519, and X25519
volume: 卷八 安全
type: practice
lead: Two independent main lines: key exchange and signatures — x25519/ECDH let strangers share a secret; Ed25519/ECDSA/RSA make claims verifiable.
api: crypto, random
---

## Orientation

The previous two chapters' primitives are all **symmetric** — both sides use the same key. But the very first sentence jams: how do you hand the symmetric key to the other side? Sending it in the clear over an untrusted channel equals publishing it; meeting in advance to exchange requires a secure channel that already exists. Asymmetric cryptography dissolves the deadlock: keys come in pairs — the public one distributes freely, the private one never leaves hand. This chapter covers two mutually independent main lines — **key exchange** (ECDH on x25519/x448 and P-256/P-384: each side holds a private key, exchanges public keys, computes the same secret) and **digital signatures** (Ed25519/ECDSA/RSA-PSS: private key signs, public key verifies, claims become non-repudiable). They are the raw material of Chapter 77's certificates (a certificate is just "a signature over a public key") and the engine of Chapter 81's TLS handshake. After this chapter you should answer for any protocol need "which curve, which signature scheme, and why".

## Introduction

Two scenarios. Scenario one: your client must establish an encrypted channel with a server it has never met — over a fully public channel each side says "here is my public key", then each computes with its own private key and the other's public key, arriving at a **completely identical** 32-byte secret; no eavesdropper can compute it from all the public messages. This is the DH (Diffie-Hellman) exchange; x25519 is its 2026 modern form. Scenario two: a software publisher must prove "this update really comes from me" — sign the package digest with the privately held key, and anyone in the world verifies with the publicly distributed public key. The difference between signatures and MACs (Chapter 73) is the trust model: MAC verifiers must hold the key (symmetric); signature verifiers need only the public key (public) — which lets them support "one-to-many" claims: one publisher, a million verifiers.

The two main lines share the stage in the TLS 1.3 handshake: x25519 exchanges the shared secret, and the server signs the handshake transcript with the certificate's private key (RSA-PSS or ECDSA) to prove identity. This chapter's four examples correspond to these roles.

## Concepts

### Key exchange: ECDH's symmetry

Diffie-Hellman's magic can be drawn as one symmetric diagram:

```diagram flow
- Setup: A generates private key sA and public key pubA=sA·G; B likewise gets sB, pubB
- Exchange: public keys cross the public channel (eavesdroppers see everything)
- A computes: sharedA = sA · pubB = sA·sB·G
- B computes: sharedB = sB · pubA = sB·sA·G
- Result: sharedA == sharedB — only the private-key holders can compute this
```

The associativity of multiplication guarantees the two products are equal, while the eavesdropper has only ever seen two public keys — recovering sA from pubA is the elliptic-curve discrete logarithm problem, and that is the whole security. XRT covers three families:

- **x25519** (32-byte private/public/shared): RFC 7748, TLS 1.3's default group, no randomness-source dependency; `xrtX25519Public` (private → public), `xrtX25519` (bare scalar multiplication), `xrtX25519Shared` (the **safe shared-secret entry** — additionally handles malicious public-key inputs like all-zero and low-order points), `xrtX25519KeyPair` (a convenience combination depending on the secure randomness source).
- **x448** (56-byte form): an isomorphic, larger-security-margin version whose API mirrors x25519 exactly (`xrtX448Public/Shared/KeyPair`).
- **P-256/P-384**: NIST curves (65/97-byte uncompressed points); besides ECDH they also expose point arithmetic (`Valid` point validation, `Add` point addition, `Multiply` scalar multiplication) — some protocols (EC signature implementations, zero-knowledge proofs) need direct point manipulation.

**The shared discipline**: the computed shared secret is **never used directly as a symmetric key** — its shape is unfixed and possibly distinguishable; it must pass through Chapter 73's HKDF to become "key material" (Chapter 76's session example is the standard chain). An x25519 private key is just 32 random bytes (`xrtX25519KeyPair` does the clamping internally); a P-curve scalar is a big-endian integer — fixed material serves only test reproducibility; production private keys always come from the secure randomness source.

### Signature line one: Ed25519 (the modern default)

Ed25519 represents the "new generation" of signatures: 64-byte signatures, 32-byte public keys/seeds, **deterministic signing** (same message + same key always yields the same signature — no randomness involved, eliminating from the root the whole class of "poor randomness leaks the private key" incidents; Sony's 2010 ECDSA incident is the cautionary tale). The API splits in two layers: the per-call entry `xrtEd25519Sign(种子, 消息, 签名)` and the stateful `xrtEd25519KeyInit/SignKey/KeyClear` — the latter pre-expands the signing intermediates, clearly faster for same-key multi-signing (certificate issuance, batch authorization); `KeyClear` wipes the sensitive intermediate state when done. Verification `xrtEd25519Verify(公钥, 消息, 签名)` is a purely public operation with no key to clear. Hitting the RFC 8032 test vectors is interop proof — signatures XRT produces, OpenSSL verifies, and vice versa.

### Signature line two: ECDSA (P-256/P-384)

ECDSA is the workhorse of TLS certificates and the existing PKI ecosystem. XRT's implementation trims in four layers: `ECDSA_VERIFY`/`ECDSA_SIGN` (curve-independent core) → `ECDSA_P256/P384` (raw verification: 64-byte `r||s` fixed-length concatenation) → `ECDSA_DER` (the conversion layer between raw and canonical DER) → curve-specific DER convenience layers. Signing uses **deterministic RFC 6979** — like Ed25519, no external randomness. The trade-off between raw and DER representations: DER is the wire format of certificates and existing protocols (variable-length, TLV-encoded); raw is the shape of internal computation and compact protocols (fixed length) — XRT isolates the conversion into a layer so both worlds are reachable.

### Signature line three: RSA (the incumbent giant)

RSA's security rests on the hardness of factoring large integers; keys are big (2048 bits minimum) and operations slow, but the installed base of certificates is the widest. XRT's RSA layering best displays the boundary between "algorithm and protocol":

- `xrtRsaPublic`/`xrtRsaPrivate`: **raw modular exponentiation** — the mathematical foundation; plaintext must never be "encrypted" this way directly (no padding, no security; the example uses it only to prove the foundation's self-consistency).
- **RSA-PSS** (`xrtRsaPssSign/PssVerify`): the modern standard and TLS 1.3's only RSA signature mode. The signed object is a **message digest** (two hash parameters: the content digest and the MGF1 mask generator). Salt is optional: explicit salt `PssSignSalt` (for test vectors and deterministic protocols), the random-salt convenience entry (default), and `XRT_RSA_PSS_SALT_ANY` for verifiers that impose no salt-length constraint.
- **RSA-PKCS1** v1.5 (`xrtRsaPkcs1Verify` etc.): the historical-protocol compatibility layer, canonical EMSA encoding.

Every private-key operation's result is **cross-checked** with the public exponent — any internal misstep surfaces rather than silently producing a wrong signature; this is a model of defensive implementation.

### Selection quick-judgment

New systems sign with **Ed25519 first** (fast, small, deterministic, no side-channel habits); to fit into an existing PKI use ECDSA P-256; RSA-PSS only for talking to legacy; key exchange always x25519 (x448 when a higher margin is demanded), P-256/P-384 only when government/compliance mandates NIST curves. This priority table matches TLS 1.3's group negotiation order — Chapter 81 shows it landing.

## Examples

### First complete program: ECDH symmetry and curve-arithmetic self-check

The following program comes from `examples/crypto/ecdh_tour/main.c`, verifying in one run that both parties' x25519/x448 shared secrets agree and that P-curve arithmetic is self-consistent:

```embed path="examples/crypto/ecdh_tour/main.c" title="examples/crypto/ecdh_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/crypto/ecdh_tour/main.c -lws2_32 -liphlpapi
ecdh: x25519 public + shared-secret symmetric ok
ecdh: x448 public + shared-secret symmetric ok
ecdh: p256 public/valid + add == multiply-by-2 ok
ecdh: p384 public/valid + add == multiply-by-2 ok
```

**What just happened.** (1) Both parties' private keys use fixed sequences (1..56 and reversed) — reproducible; in production, entries like `xrtX25519KeyPair` sample from the secure randomness source. (2) The symmetry verification is ECDH's core assertion: `xrtX25519(sA, pubB)` and `xrtX25519(sB, pubA)` are byte-for-byte equal — mathematically both are sA·sB·G. (3) The P-curve section does two self-checks: `Valid` accepts the legitimate point exported from the public key and **rejects the all-0xFF pseudo-point** (point validation is part of ECDH security — unvalidated points admit small-subgroup attacks); `Add(P,P) == Multiply(2,P)` verifies arithmetic consistency without preset base-point constants. (4) Note NIST points are big-endian encoded with curve-dependent lengths (65/97 bytes) while x25519 is a fixed 32 bytes — keep the two families' byte-shape difference in mind during protocol design.

### Second complete program: stateful Ed25519 signing

The second program comes from `examples/crypto/ed25519_sign/main.c`, signing a message on an expanded key state:

```embed path="examples/crypto/ed25519_sign/main.c" title="examples/crypto/ed25519_sign/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/crypto/ed25519_sign/main.c -lws2_32 -liphlpapi
signed: yes
```

**What just happened.** (1) `xrtEd25519KeyInit` pre-expands the signing intermediates from a 32-byte seed — the all-zero seed is for reproducibility only; in same-key multi-signing scenarios (a CA batch-signing certificates, a token service) this state reuse is the performance key. (2) The signature is deterministic: repeated runs output the identical value, no randomness involved — which also means the signature can serve as a test vector. (3) `KeyClear` immediately wipes the expanded state (it holds material from which the private key is derivable), and only then prints the result — **zeroing happens on the success path too**, not just the error path. The companion `examples/crypto/ed25519_verify` validates the public-key signature with the RFC 8032 §7.1 empty-message vector — hitting the standard vector is interop proof.

### Third complete program: RSA in three layers — foundation, explicit salt, random salt

The third program comes from `examples/crypto/rsa_pss/main.c`, walking from raw modular exponentiation to PSS with a random salt in one pass:

```embed path="examples/crypto/rsa_pss/main.c" title="examples/crypto/rsa_pss/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/crypto/rsa_pss/main.c -lws2_32 -liphlpapi
RSA-PSS: valid
```

**What just happened.** (1) The first section deliberately demonstrates **raw modular exponentiation** and round-trips to prove the mathematics — header and code comments both stress "ordinary messages must never be encrypted this way": RSA without padding is the textbook example solvable by direct mathematical attack; this section exists so you can see the distance between foundation and secure protocol. (2) PSS signs twice: explicit salt `PssSignSalt` (same input yields a reproducible signature — the shape of protocol testing) and random salt `PssSign` (different every time — the production default); verification uses the same `PssVerify`, the second time passing `XRT_RSA_PSS_SALT_ANY` to impose no salt-length constraint — **verifiers should generally not assume the signer's salt policy**. (3) The signed object is `Fixture.Hash` (a digest), not the raw message — RSA always signs digests; the two `XCRYPTO_HASH_SHA256` parameters are the content digest and MGF1's mask hash. (4) The cleanup section `xrtSecureZero`s the key struct, plaintext buffers, and signature — an RSA private-key struct holds CRT parameters and primes, a far larger sensitive surface than a symmetric key.

## Contracts

- **Exchange family**: x25519/x448/P-256/P-384 all support private → public export and two-party shared secrets; `Shared`/the safe entries handle malicious public keys (low-order points, all-zero outputs), while the bare scalar-multiplication entries (`xrtX25519`) do no input validation — protocol layers use the `Shared` shape directly; point arithmetic (Valid/Add/Multiply) is exposed only for P curves.
- **Randomness-source boundary**: the bare exchange and signing cores **depend on no randomness source** (an x25519 private key is just a random number; Ed25519/ECDSA are deterministic); only the `KeyPair` convenience layers and PSS's random salt depend on `RANDOM_SECURE` — in trimming, the randomness source enters only with those entries.
- **Signature representations**: Ed25519 fixed 64 bytes; ECDSA raw is fixed-length r||s, DER is the certificate wire format, with the conversion layer independent; RSA-PSS/PKCS1 signature length equals the modulus length (2048 bits → 256 bytes).
- **RSA layering**: raw modular exponentiation demonstrates the layer only — no padding, no security; PSS is the modern standard (TLS 1.3's only RSA mode); explicit salt for reproducibility, random salt by default, verification may use `XRT_RSA_PSS_SALT_ANY`; private-key results are cross-checked with the public exponent.
- **Deterministic signing**: Ed25519 and ECDSA (RFC 6979) signatures depend on no external randomness — repeated signing of the same message is constant and usable as test vectors.
- **Sensitive-material cleanup**: `xed25519key` gets `KeyClear` after use; RSA private-key structs, seeds, and shared secrets get `xrtSecureZero`; verification is a public operation with no key to clear.
- **Errors**: failures declared by return value; illegal parameter lengths/point encodings are refused outright; point validation (Valid) rejects off-curve points and pseudo-encodings.

## Pitfalls

### Pitfall 1: encrypting messages with raw RSA modular exponentiation

Symptoms: the security review vetoes at a glance; under serious attack, unpadded RSA plaintext is recoverable by classic techniques like low-exponent and chosen-ciphertext — textbook RSA is not an encryption scheme.

Cause: `xrtRsaPublic`/`xrtRsaPrivate` are the mathematical foundation (modular exponentiation) with no padding or randomization. Security level zero: the same plaintext always yields the same ciphertext, and plaintext and ciphertext are algebraically manipulable against each other.

```c bad
uint8 Cipher[128];
xrtRsaPublic(&Key.Public, SecretMessage, 128, Cipher);
send(Cipher);   /* unpadded RSA: the plaintext falls to direct mathematical attack */
```

```c good
/* The modern way has exactly two roads:
   1) Encryption: do not encrypt data with RSA — use ECDH exchange + AEAD (Chapters 74/76)
   2) Signatures: xrtRsaPssSign (TLS 1.3's only RSA signature mode) */
uint8 Signature[128];
xrtRsaPssSign(&Key, XCRYPTO_HASH_SHA256, XCRYPTO_HASH_SHA256,
	Digest, Signature);
```

### Pitfall 2: using the ECDH shared secret directly as a symmetric key

Symptoms: the security review fails; in some scenarios the shared secret's low bits have statistical bias, and feeding it straight into AES's key schedule weakens security; the protocol also cannot derive multiple purpose-bound keys.

Cause: DH output is a "mathematical artifact", not "qualified key material" — shape, distribution, and purpose binding all fall short. HKDF exists precisely for this conversion.

```c bad
uint8 Shared[32];
xrtX25519Shared(sA, pubB, Shared);
xaesgcm State;
xrtAesGcmInit(&State, Shared, 32, 16);  /* the raw secret directly as key */
```

```c good
uint8 Shared[32], SessionKey[32];
xrtX25519Shared(sA, pubB, Shared);
xrtHkdfSha256(Salt, iSalt, Shared, sizeof(Shared),
	Transcript, iTranscript, SessionKey, sizeof(SessionKey));
xrtSecureZero(Shared, sizeof(Shared));  /* the intermediate secret is discarded at once */
xrtAesGcmInit(&State, SessionKey, 32, 16);
```

### Pitfall 3: handling verification with memcmp reflexes or ignoring the return value

Symptoms: the verification function clearly returned `false`, yet the code continues down the success branch; or it tries to "compare" the signature against an expected value — signatures contain a random salt (PSS), or though deterministic (Ed25519), the code logic wrote "verify" as "compare".

Cause: verification is a **predicate function** (inputs public key/message/signature, output bool), not "compute an expected value then compare". PSS's random salt means two signatures of the same message under the same key legitimately differ — writing verification as "re-sign then compare" always breaks on random-salt signatures.

```c bad
uint8 Expect[128];
xrtRsaPssSign(&Key, XCRYPTO_HASH_SHA256, XCRYPTO_HASH_SHA256,
	Digest, Expect);                       /* re-sign */
if ( memcmp(Expect, Received, 128) == 0 ) { /* never equal under random salt */
	accept();
}
```

```c good
if ( xrtRsaPssVerify(&Key.Public, XCRYPTO_HASH_SHA256,
		XCRYPTO_HASH_SHA256, XRT_RSA_PSS_SALT_ANY, Digest,
		Received, 128) ) {
	accept();   /* verification is a predicate: the answer in one step */
}
```

## Exercises

### Basic: reproduce the x25519 three-step handshake

Fix both parties' private keys (say 32 bytes of 0x01 and 0x02), walk `Public → 互发 → Shared` (mutual exchange) completely, print both shared secrets in hex, and confirm they match. Then swap the private-key roles and rerun, confirming the symmetry doesn't depend on "who initiates". Acceptance: two runs output bit-for-bit identical values (determinism).

### Advanced: a deterministic-signature test-vector generator

Sign three different messages with Ed25519's stateful entries, outputting a vector table of "message hex + signature hex"; then re-sign with the per-call entry `xrtEd25519Sign` and verify bit-for-bit agreement with the stateful results. Hint: fixed seed for reproducibility; these are interop test vectors for colleagues — any RFC 8032 implementation (OpenSSL, say) should verify them all.

### Challenge: a mini key-agreement protocol

Over loopback TCP implement a two-party handshake: each side generates an ephemeral key with `xrtX25519KeyPair`, exchanges public keys (plain JSON suffices), both derive the session key via `Shared` + HKDF (info includes "mini protocol v1" and both public keys), then exchange one encrypted message with ChaCha20-Poly1305 to verify. Attack experiment: a man-in-the-middle swaps public keys — explain why bare negotiation resists no MITM (the answer is Chapter 78's certificates), and write the "certificates needed" conclusion into your README. Acceptance: all key material (private keys/shared secret/session key) `xrtSecureZero`d after use; zero leaks by Chapter 6's stats.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Two main lines | exchange (x25519/x448/P-256/P-384 ECDH) and signatures (Ed25519/ECDSA/RSA) are mutually independent, trimmed per need |
| ECDH symmetry | sA·pubB == sB·pubA == sA·sB·G; eavesdroppers see only public keys and cannot compute the secret |
| Exchange entries | bare scalar multiply (no validation) vs `Shared` safe entry (handles malicious public keys) vs `KeyPair` convenience (includes randomness) |
| Ed25519 | 32 seed/32 public/64 signature; deterministic; `KeyInit/SignKey/KeyClear` stateful acceleration for multi-signing |
| ECDSA | P-256/P-384; raw fixed-length r‖s and DER certificate wire format, independent conversion layer; RFC 6979 deterministic |
| RSA layering | raw modular exponentiation (foundation only) → PSS (TLS 1.3's only RSA mode) → PKCS1 (compat); results cross-checked with the public exponent |
| PSS salt | explicit salt reproducible, random salt default, verification `XRT_RSA_PSS_SALT_ANY` unconstrained |
| Selection | new systems Ed25519 + x25519; fitting a PKI uses ECDSA P-256; legacy only then RSA-PSS |
| Iron rules | shared secrets must pass HKDF; RSA never encrypts data directly; verification is a predicate, not a comparison |
| Cleanup | expanded keys `KeyClear`; seeds/private keys/shared secrets `xrtSecureZero`; verification has no key to clear |
