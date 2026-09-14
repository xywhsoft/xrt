---
num: 83
slug: tls-identity
title: TLS Identity: Certificates, Private Keys, and Signatures
volume: 卷八 安全
type: practice
lead: Packaging "one certificate chain + one private key" into an immutable shared object — the signing engine for the handshake's CertificateVerify and its hardware extension port.
api: tls, tls_identity, crypto
---

## Orientation

Two parties in TLS must "prove themselves": the client proves "whom I trust" (verifier, Chapter 85), the server proves "who I am" (**identity**, this chapter). `xtlsidentity` packages one DER certificate chain and one private key into an **immutable shared object**: at creation it deep-copies all material and completes cross-validation (does the private key really match the certificate's public key? Which TLS schemes can this certificate sign?), after which multiple threads and server configurations share it concurrently, and the handshake uses it to produce CertificateVerify signatures. Four strongly-typed constructors (RSA/P-256/P-384/Ed25519) cover mainstream servers; `xrtTlsIdentityCreate`'s extension interface outsources the signing capability to an HSM, the system key store, or a remote signer. Client-certificate authentication is currently unimplemented (Chapter 82's contract), so identity today is a **server-side** concept — but the object itself is decoupled from the role.

## Introduction

What must you prepare to deploy a TLS server? A certificate chain (leaf + intermediate CA, PEM files) and a private key. The trouble starts at loading: private keys come in several DER formats (PKCS#1, PKCS#8, SEC1 bare scalar, Ed25519 seed), the certificate's public-key algorithm must match the private key, and RSA-PSS certificates carry parameter constraints (digest, MGF1, salt-length floor) — any mismatch surfaces only at handshake-time signing, and then you face a 3 a.m. alert instead of a clear startup error.

`xtlsidentity`'s answer is **full validation at creation**: the constructor parses the private key, cross-checks the certificate SPKI, and verifies the two sides' common signature schemes across TLS versions — "an identity with no common TLS scheme is rejected outright at construction". Blowing up at startup beats blowing up at handshake ten-thousandfold — deployment experience and security experience are the same thing here. The validated identity is immutable: the chain is deep-copied into a single compact allocation (securely zeroed before release), borrowed views staying valid until the last reference drops — multiple Workers share one object, zero locks.

## Concepts

### The four constructors and their private-key dialects

| Constructor | Private-key input forms | Creation-time validation |
| --- | --- | --- |
| `xrtTlsIdentityRsa` | PKCS#1 or unencrypted PKCS#8 DER | Modulus, exponent, all CRT parameters, leaf SPKI, RSA-PSS restrictions |
| `xrtTlsIdentityP256` | 32-byte scalar / SEC1 / unencrypted PKCS#8 | Scalar range, curve OID, optional SEC1 public key, leaf P-256 SPKI |
| `xrtTlsIdentityP384` | 48-byte scalar / SEC1 / unencrypted PKCS#8 | Same as above (P-384) |
| `xrtTlsIdentityEd25519` | 32-byte seed / single-or-double DER OCTET / RFC 8410 PKCS#8 | Algorithm parameters defaulted, public key derived, leaf Ed25519 SPKI |

Three commonalities: **the chain's first certificate must be the leaf**; **the private key is never borrowed** (deep-copied into the identity; the caller's buffer is reusable immediately); **RSA keeps the full CRT view, Ed25519 keeps a once-expanded signing key** — the handshake path never re-derives. PEM/files/system stores are not the identity core: first use Chapter 78's PEM decoding to get DER, then hand it to a constructor — only DER passes between layers.

### CanSign: the triple capability query

`xrtTlsIdentityCanSign(身份, 版本, 方案)` answers "can this identity sign with this scheme" — simultaneously checking TLS version restrictions, the certificate identity type, the standard schemes, and backend compilation. Key rules: TLS 1.3 does not accept RSA-PKCS#1 for CertificateVerify (a hard rule of the modern protocol); ECDSA schemes must match the certificate's curve; `rsaEncryption` and `RSASSA-PSS` certificates enter only the `rsa_pss_rsae_*` and `rsa_pss_pss_*` paths respectively; restricted PSS keys must additionally satisfy parameter constraints on both the certificate and private-key sides. The policy's (Chapter 85) signature whitelist intersected with `CanSign` yields the handshake's actual schemes — this chapter's `exampleSignature` demonstrates picking common schemes by identity type.

### Sign: two-step signing and determinism

`xrtTlsIdentitySign(身份, 版本, 方案, 内容, 输出, 容量, &长度)` receives the **complete TLS content to sign** (the protocol-context header is built by the caller — TLS 1.3's 64-space role prefix is handled at the open-primitive `xrtTls13CertificateVerifySignature` layer). Two-step, consistent with library convention: empty output queries the exact length, insufficient capacity invokes no signer, failure publishes no partial signature. The three backends' randomness policies: RSA-PSS uses a cryptographically secure random salt; ECDSA uses RFC 6979 deterministic low-S signatures (Chapter 76 explained why determinism is a virtue); Ed25519 pure mode is deterministic by nature.

### Create: hardware and remote signers

In real enterprise environments the private key often lives in an HSM or system key store and cannot be exported as DER. `xrtTlsIdentityCreate` is the extension interface: the certificate chain is still deep-copied by XRT, and the signing capability is outsourced via the `Supports`/`Sign` callbacks — both must permit concurrent invocation, `Supports` is an error-free capability predicate, and `Sign` leaves the output unchanged on failure. Without hardware needs, use the built-in strongly-typed constructors — the callback port exists for pluggability, not as the daily path.

### Lifecycle: from loading to sharing

```diagram flow
- Load: PEM/file -> DER (Chapter 78); chain + private key handed to a constructor
- Validate: private-key parse + SPKI cross-check + common-scheme check (failure rejects)
- Share: the immutable object is referenced concurrently by server configs/Workers (`Retain`/`Release`)
- Destroy: last reference released -> all key material securely zeroed -> Release callback exactly once
```

`Retain`/`Release` reference-count; releasing the last reference securely zeroes all key material and invokes the `Release` callback (the Create form). Immutability means: shared across server configurations, concurrent signing across Workers, combinable with any policy/context — **assemble once, share everywhere**, the same philosophy as Chapter 22's "read-only sharing is cheapest", except what's shared here is an immutable value, not a reusable slot.

## Examples

### First complete program: constructing four key forms and querying types

The following program comes from `examples/tls/identity/main.c`; with no arguments it self-checks the four identity constructors with embedded material:

```embed path="examples/tls/identity/main.c" title="examples/tls/identity/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/tls/identity/main.c -lws2_32 -liphlpapi
embedded rsa identity=1 certificates=1
embedded p256 identity=3 certificates=1
embedded p384 identity=4 certificates=1
embedded ed25519 identity=5 certificates=1
```

**What just happened.** (1) Each form goes through one constructor: RSA (1), P-256 (3), P-384 (4), Ed25519 (5) — `IdentityType`'s enum values correspond to `xtlsidentitytype`, stable across platforms. (2) Every construction fully validates: private-key parse, SPKI cross-check, common-scheme check — the embedded material is all legal so construction passes; swap in a certificate/key pair that doesn't match and construction fails outright with a structured error (an identity error code + the DER/X.509/crypto cause chain). (3) `CertificateCount` confirms the chain length (one leaf per form here); real deployments pass the full array including intermediates. (4) With arguments, the program switches to the full path "read DER files → construct → two-step Sign" — `exampleSignature` picks the scheme by type (RSA goes PSS, Ed25519 signs directly), querying length with empty output, allocating, then signing, printing the signature byte count (Ed25519 always 64, RSA equal to the modulus length). (5) Each identity gets `Release` when done — the lifecycle management of an immutable shared object is just this one pair of calls.

### Second complete program: the underlying domain of signature schemes

The second program comes from `examples/crypto/sign_tour/main.c`, demonstrating the underlying scheme family that identity signing depends on — Ed25519's three domains and ECDSA's DER representation:

```embed path="examples/crypto/sign_tour/main.c" title="examples/crypto/sign_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/crypto/sign_tour/main.c -lws2_32 -liphlpapi
sign: ed25519 pure sign->verify ok
sign: ed25519 context + prehash modes ok
sign: ecdsa-p256 sign->verify(der) roundtrip ok
sign: ecdsa-p384 sign->verify(der) roundtrip ok
sign: der r||s <-> der roundtrip ok
```

**What just happened.** (1) **Ed25519's three domains are mutually incompatible**: PURE (sign the message directly), CONTEXT (with a context string — TLS 1.3's CertificateVerify uses exactly this domain-separation idea), PREHASH (signs a 64-byte SHA-512 prehash). Signatures do not verify across domains — this is the cryptographic mechanism of "one algorithm, different uses, never confused"; TLS's role prefix ("TLS 1.3, server CertificateVerify") exists to prevent moving a site's TLS signature to another protocol. (2) ECDSA works through the "receive a digest, not a message" interface (the `Hash` enum declares the digest algorithm), with raw `r||s` fixed-width and DER wire formats interconverting — Chapter 76's ECDSA dual-representation round-trip. (3) Compare with the identity layer: `IdentitySign` internally is exactly these underlying primitives plus TLS context wrapping — understanding the bottom helps diagnose "CanSign says yes but the handshake fails" cross-layer problems (usually the policy whitelist hasn't opened the scheme).

## Contracts

- **Immutable sharing**: immutable after creation; Retain/`Release` reference-counted sharing; borrowed views (certificates/public keys) valid to the last reference; lock-free multi-threaded signing.
- **Creation-time validation**: private-key parse + leaf SPKI cross-check + common TLS-scheme check; no common scheme rejects outright; the chain's first certificate must be the leaf.
- **Private-key deep copy**: constructors never borrow the key input — no borrowing; single compact allocation, securely zeroed before release; RSA keeps the CRT view, Ed25519 the expanded key.
- **CanSign semantics**: version + type + scheme + backend, four-fold check; 1.3 rejects PKCS#1; ECDSA schemes follow the certificate's curve; PSS-restricted keys constrained on both sides.
- **Sign contract**: two-step (empty output queries length); insufficient capacity invokes no signer; failure publishes no partial signature; PSS random salt / ECDSA deterministic low-S / Ed25519 pure mode.
- **Create extension**: `Supports` predicate sets no error; `Sign` concurrent-safe, leaving output unchanged on failure; `Release` exactly once; the chain still deep-copied.
- **Error model**: identity error codes under `xrt.tls`; DER/X.509/crypto failures preserved as Causes — upper layers can map by TLS stage while C callers read the underlying reason.
- **Trimming**: the `TLS_IDENTITY` core + `_RSA/_EC/_P256/_P384/_ED25519` each independent — an Ed25519-only server compiles in no RSA code.

## Pitfalls

### Pitfall 1: certificate and private key mismatched, discovered the night of launch

Symptoms: the service starts fine; client connections fail at the CertificateVerify stage — the log says only "signature invalid", with no hint the material is mismatched.

Cause: many loading paths only use the private key at handshake time; a mismatched certificate (say, renewed with a new certificate while the private key stayed old) surfaces only at the first real connection.

```c bad
/* merely read files, construct no identity: the error defers to the handshake */
load_file("cert.pem", &Cert, &CertSize);
load_file("key.pem", &Key, &KeySize);
/* stored away and used at handshake — the mismatch goes unnoticed */
```

```c good
/* construct the identity at startup: a mismatch fails on the spot */
xtlsidentity* pIdentity = xrtTlsIdentityP256(
	&(xbytesview){ Cert, CertSize }, 1u,
	(xbytesview){ Key, KeySize }
);
if ( pIdentity == NULL ) {
	fprintf(stderr, "identity: %s\n",
		xrtErrorMessage(xrtGetError()));
	return EXIT_FAILURE;   /* exposed at startup, with the cause chain */
}
```

### Pitfall 2: homemade signature-scheme combinations (forcing what CanSign rejected)

Symptoms: an Ed25519 identity configured with an RSA-PSS scheme, or a P-256 certificate with a P-384 scheme — `IdentitySign` fails and the handshake hangs.

Cause: TLS schemes are strongly bound to key types (RFC-mandated): ECDSA schemes carry the curve name, RSA splits into rsae/pss families, Ed25519 has only the pure scheme. This is not a "try it and see" configuration item.

```c bad
/* Ed25519 identity + RSA scheme: certain failure */
xrtTlsIdentitySign(pIdentity, XTLS_VERSION_13,
	XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256, ...);
```

```c good
/* schemes follow the identity type: ask CanSign first */
xtlssignature Scheme = pick_scheme_for(xrtTlsIdentityType(pIdentity));
if ( !xrtTlsIdentityCanSign(pIdentity, XTLS_VERSION_13, Scheme) ) {
	/* configuration error: reported at startup, not at handshake */
}
```

### Pitfall 3: the HSM extension's Sign callback carries shared mutable state

Symptoms: intermittent signing failures or scrambled output during concurrent handshakes — `Create`'s documentation says plainly that Supports/Sign must be concurrent, but the callback implementation uses a non-reentrant handle.

Cause: the identity object is shared concurrently by Workers, and signing callbacks run simultaneously in multiple handshakes. The callback is a lock-free contract — making stateful device access queued or per-thread is your responsibility.

```c bad
static bool hsmSign(..., void* pOutput, size_t* pSize) {
	hsm_begin(pHsmSession);   /* shared session: concurrent handshakes trample each other */
	hsm_op(...);
	hsm_end(pHsmSession);
}
```

```c good
/* an independent session per call (or a session pool); failure alters neither output nor length */
static bool hsmSign(const void* pContent, size_t iSize,
		void* pOutput, size_t iCapacity, size_t* pSize) {
	hsmsession* pS = hsm_session_acquire();   /* pooled */
	bool bOk = hsm_sign(pS, pContent, iSize, pOutput, iCapacity, pSize);
	hsm_session_release(pS);
	return bOk;
}
```

## Exercises

### Basic: four-form self-check with embedded material

Run the identity example's no-argument form, matching the four lines' type enum values against `xtlsidentitytype`. Then generate a self-signed Ed25519 certificate and private key with OpenSSL (DER export), walking the argument path to verify the 64-byte signature. Acceptance: all four forms self-check green; your own material path outputs `identity=5 certificates=1 signature=64 bytes`.

### Advanced: startup-time interception of mismatched material

Deliberately mismatch two pairs (RSA certificate + EC private key; P-256 certificate + P-384 private key), construct identities for each, and print the error messages' cause chains (`xrtErrorMessage` + `xrtErrorCause`). Acceptance: both mismatches fail at construction, not at signing; the error chains distinguish "DER parse failure" from "SPKI mismatch".

### Challenge: a simulated HSM signer

Implement a "mock HSM" with `xrtTlsIdentityCreate`: `Supports` accepts only Ed25519; `Sign` internally uses the real implementation (the seed held in a "hardware" struct, simulating non-exportability) plus an atomic counter tallying calls. Wire it into Chapter 86's server (or test directly), with 8 threads signing concurrently. Acceptance: all signatures succeed and verify; the counter equals the signature count (proving concurrency); the "hardware" seed never leaves the mock struct (confirmed by code review).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| The identity object | chain + private key packaged; immutable, lock-free multi-threaded sharing via reference counting |
| Four constructors | Rsa/P256/P384/Ed25519; private-key dialects differ (PKCS#1/#8/SEC1/seed) |
| Creation-time validation | key parse + SPKI cross-check + common-scheme check; no scheme, rejection |
| Private-key handling | deep copy, never borrowed; single allocation zeroed before release; CRT/expanded key resident |
| CanSign | version + type + scheme + backend; 1.3 rejects PKCS#1; ECDSA follows the curve; PSS both-side constraints |
| Sign | two-step query/sign; no partial on failure; PSS random salt / ECDSA low-S / Ed25519 pure |
| Scheme binding | schemes follow key type; whitelist policy ∩ CanSign = the handshake's schemes |
| Create extension | HSM/system store/remote signing; Supports predicate + concurrent-safe Sign + exactly-once Release |
| Input boundary | PEM/file/system store are not the identity core — get DER first, then construct |
| Trimming | _RSA/_EC/_P256/_P384/_ED25519 independent; single-algorithm servers carry no others |
