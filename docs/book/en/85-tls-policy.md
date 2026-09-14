---
num: 85
slug: tls-policy
title: Verification Policy: Protocol Whitelists and Trust Decisions
volume: 卷八 安全
type: practice
lead: The policy object defines "which conversations are allowed"; the verifier defines "who may be trusted" — two configurations defined once, referenced everywhere — the security baseline.
api: tls, tls_verify, x509
---

## Orientation

All of TLS's security decisions condense into two configuration objects. The **policy** (`xtlspolicy`): which versions, suites, groups, and signature schemes are allowed — the protocol whitelist, the baseline of Chapter 84's negotiation; the defaults are the current security baseline (2 versions/9 suites/4 groups/14 schemes, no weak algorithms), and tightening is just swapping arrays. The **verifier** (`xtlsverifier`): trust decisions — the default path goes "trust-store chain building + purpose checks + name matching" (the composition of Chapters 80/81's primitives); a custom callback hands the decision entirely to the application (certificate pinning, embedded private CAs, audit logging), and a policy callback can additionally compose CRL/OCSP. Both are immutable, shareable concurrently via reference counting across sessions — "defined once, referenced everywhere". Chapter 82's client configuration completes its full picture here.

## Introduction

Three real needs. One: a compliance audit requires your service to "allow only TLS 1.3 and two or three specific suites" — you want one change to take effect on every connection, not hard-coded values scattered across every client/server config. Two: your client connects only to your own backends, and the public CAs' trust surface is too broad — you want to pin to one specific certificate (pinning), which Chapter 81's trust store cannot do as "single certificate as anchor + fingerprint comparison + audit log". Three: an experimental environment wants to add a temporary "revocation check" (Chapter 81's CRL query) without rewriting the chain-building logic.

The three needs map to this chapter's two layers: the policy object manages the protocol face (need one), and the verifier's three-layer extension points manage the trust face (needs two and three). The key design is **decision-execution separation**: the verifier manages only "trust decisions"; the protocol layer's signature verification and Finished checking are still done by XRT and **no callback can bypass them** — what you take over is "trust or not", not "is the protocol correct".

## Concepts

### The policy object: the single source of truth for protocol preferences

`xtlspolicy` is a borrowing-style configuration: default initialization points at process-lifetime read-only constants (zero allocation); tightening swaps in your own arrays (whose lifecycle covers context creation):

```diagram flow
- Default baseline: xrtTlsPolicyInit -> read-only constants (2 versions/9 suites/4 groups/14 schemes, all current)
- Tightening: replace the Versions/Ciphers/Groups/Signatures array pointers and counts
- Self-consistency check: xrtTlsPolicyValid — empty sets / version-suite mismatch / a 1.2 scheme mixed into 1.3 gets rejected
- Referencing: Context/client/server configs share the same policy snapshot
```

Validation rules (catching configuration errors at startup): versions and suites must be non-empty; list pointers/counts consistent, elements unique, belonging to built-in capabilities; every suite corresponds to an enabled version and every version has at least one suite; a non-empty signature list must be usable by at least one enabled version — a 1.2-only PKCS#1 scheme cannot enter a pure-1.3 policy. Two boundaries: the policy describes only **preferences** and promises nothing about the current build's trimming containing the corresponding backends (session creation combines `xrtTlsGroupAvailable` etc. to generate the actual execution path); `KeySharePolicy` can override the local key-share selection mode (e.g. `XTLS_KEY_SHARE_PREFER_GROUP`) — which group's public key the ClientHello carries is decided by it.

### The verifier: three layers of trust decisions

`xrtTlsVerifierCreate(配置)` deep-copies the optional trust store — after creation the source store may be freed or modified; without a custom callback you **must** provide at least one trust anchor, and an empty store fails at creation (the same root as Chapter 82's "no verification is not an option"). The three-layer decision structure:

- **Default path** (`xrtTlsPeerVerify`): path validation at the explicit time (Chapter 81's chain building) + role-based purpose checks (`digitalSignature` KeyUsage, `serverAuth`/`clientAuth` EKU) + the server role matching the requested name per RFC 9525 (Chapter 80). The wire chain runs leaf to root; anchors come from the store and are not required to appear in the peer's chain.
- **Verify callback** (complete takeover): `XTLS_VERIFY_ACCEPT` takes over trust / `REJECT` refuses / `DEFAULT` falls back to the immutable store / `ERROR` reports a structured error. What the callback receives is the **cryptographically validated peer structure** (signatures and Finished guaranteed by the protocol layer) — you only decide trust. An accept still cannot skip CertificateVerify and Finished — "taking over trust" is not "taking over the protocol".
- **Policy callback** (stacked checks): fires after all default checks succeed, reading the `xtlsverifiedpeer` (`Path` leaf-to-root without the anchor; all views borrowed only during the callback) — composing CRL (Chapter 81), OCSP, CT, pinning, enterprise rules **without re-building the chain**. Returning false may set an error, wrapped as the cause of `XTLS_ERROR_VERIFY`.

The time source is injectable (`xtlsverifytimeproc`): the path and the additional policy share one deterministic time — tests reproducible, caches decidable; omit it to use `xrtNow()`.

### Boundary: the TLS core never sneaks online

The TLS core does not implicitly load system roots, download CRLs, start OCSP, or submit CT queries — data loading, encoding, and networking belong to separate composition layers. The base client never suddenly blocks on file or system APIs during verification (Chapter 77's "system-layer failure" preventive design). Want CRL? Call Chapter 81's APIs yourself in the Policy callback — soft or hard fail is your policy decision; the verifier preserves unambiguous results for you.

### Assembly shape: policy + verifier + session

The complete client assembly (the exploded view of Chapter 82's example):

```diagram flow
- Policy: tighten xtlspolicy to the target baseline -> PolicyValid -> attach to the shared context config
- Trust: StoreSystem (or explicit Add) -> VerifierCreate deep-copies -> the Store may be freed
- Extensions: Verify/Policy/Time callbacks injected as needed (pinning, CRL, deterministic time)
- Session: ClientConfig{Verifier, ServerName, Protocols} -> the session holds references -> `Release` your own
```

## Examples

### First complete program: the policy baseline and tightening validation

The following program comes from `examples/tls/policy/main.c`, printing the default baseline and overriding the key-share mode:

```embed path="examples/tls/policy/main.c" title="examples/tls/policy/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/tls/policy/main.c -lws2_32 -liphlpapi
versions=2 ciphers=9 groups=4 signatures=14
```

**What just happened.** (1) The default baseline — **output as evidence**: 2 versions (1.2/1.3), 9 suites, 4 named groups, 14 signature schemes — no weak algorithms; this is the "safe even unconfigured" floor. (2) `KeySharePolicy = XTLS_KEY_SHARE_PREFER_GROUP` overrides the local key-share selection — the ClientHello carries the preferred group's public key first, reducing HRR round trips (Chapter 84). (3) `PolicyValid` is the configuration's gatekeeper — swapping Versions to 1.3-only while Ciphers still holds 1.2-only suites gets rejected; the tightening example (only 1.3 + the ChaCha20/AES-GCM two suites) swaps arrays and counts together, then validates once more. Once a policy snapshot is referenced by a context it is never modified — the same configuration serves differing hardware capabilities (backend inclusion decided at session creation).

### Second complete program: a custom verifier taking over trust

The second program comes from `examples/tls/verify/main.c`, handing the certificate decision to the application via callback:

```embed path="examples/tls/verify/main.c" title="examples/tls/verify/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/tls/verify/main.c -lws2_32 -liphlpapi
TLS verifier is ready
```

**What just happened.** (1) `VerifyConfig.Verify = exampleTlsVerify` injects the callback — this form **needs no store** (decisions all inside the callback). (2) The callback receives the `xtlspeer` (peer info: certificate-chain views, negotiated parameters) and returns a decision enum — this example is the minimal policy "any certificate is accepted" (interface demonstration only, not a copyable production policy). (3) The verifier is shareable across clients — create once, inject into every `ClientConfig.Verifier`, release your own reference as you go. Production forms: certificate pinning (compare the leaf's SHA-256 against a preset fingerprint, REJECT on mismatch), embedded private CA (store holding only your own root + DEFAULT fallback), audit logging (a Policy callback recording each verification's peer and path before admitting) — each with its place in the three-layer decision structure.

## Contracts

- **Policy validation**: non-empty versions and suites; pointers/counts consistent, elements unique, within built-in capabilities; suite↔version bidirectional coverage; the signature list usable by at least one enabled version; validation modifies no input and allocates nothing.
- **Policy boundary**: describes preferences, promises no backend inclusion; session creation combines `GroupAvailable`/AEAD/identity/verification backends to build the actual path; the raw policy snapshot is immutable.
- **KeySharePolicy**: the local key-share selection mode (e.g. `XTLS_KEY_SHARE_PREFER_GROUP`) — affects which group the ClientHello carries, reducing HRR.
- **Verifier mandate**: without a Verify callback, at least one trust anchor is required; an empty store fails at creation; after the deep copy, the source store may be freed/modified.
- **Three-layer decisions**: default path (chain + purpose + name) / Verify callback (complete takeover, four-state decisions) / Policy callback (stacked after all defaults pass; views borrowed only during the callback).
- **Protocol unbypassable**: an accept decision cannot skip CertificateVerify and Finished — signatures and integrity are guaranteed by the protocol layer.
- **Time injection**: `xtlsverifytimeproc` provides one deterministic time for path and policy; omit for `xrtNow()`.
- **Concurrency**: the verifier is immutable and reference-counted; Verify/Policy/Time callbacks may be invoked concurrently — they must not rely on thread-local mutable sharing.
- **No implicit networking**: no loading system roots, no downloading CRLs, no sending OCSP, no querying CT — the composition layer's and policy callback's business.
- **Error wrapping**: a callback returning false with an error set → `XTLS_ERROR_VERIFY` + cause; without an error → an explicit `XERR_PERMISSION` rejection.

## Pitfalls

### Pitfall 1: treating the policy as a capability declaration (configured but not compiled)

Symptoms: the policy configures a ChaCha20 suite but the firmware build compiled no ChaCha20 backend — runtime negotiation fails, connections fail or suffer degradation to other suites, contradicting the configuration's intent.

Cause: the policy is preference, not promise — validation only checks "belongs to built-in session capabilities", not the current build's backend inclusion. The actual execution path is generated at session creation combining `GroupAvailable` etc.

```c bad
/* assume configured means usable */
Policy.Ciphers = ChaChaFirst;
Policy.CipherCount = 1;
/* built with --feature lacking CHACHA20: every negotiation fails on the target */
```

```c good
/* verify capabilities before deployment: groups via GroupAvailable, suites per the build manifest */
if ( !xrtTlsGroupAvailable(XTLS_GROUP_X25519) ) {
	/* report the capability gap at startup, not as online connection failures */
}
/* run negotiation smoke tests in CI against the target build — policy and capability verified together */
```

### Pitfall 2: redoing cryptographic checks in the Verify callback

Symptoms: the callback re-verifies signatures and compares Finished — bloated code that may verify wrongly; or conversely, believing "taking over verification" skips the protocol checks and dropping vigilance.

Cause: not understanding "decision-execution separation": the callback receives the peer structure **already cryptographically validated** (signatures and Finished guaranteed and unbypassable at the protocol layer); your duty is "trust or not" (chain, pinning, audit), not "re-verify the protocol".

```c bad
static xtlsverifydecision myVerify(const xtlspeer* pPeer, ptr pCtx) {
	/* redoing signature verification: duplicate, error-prone, and already guaranteed */
	if ( !self_verify_signature(pPeer) ) { return XTLS_VERIFY_REJECT; }
	return XTLS_VERIFY_ACCEPT;
}
```

```c good
static xtlsverifydecision myVerify(const xtlspeer* pPeer, ptr pCtx) {
	/* trust decisions only: e.g. certificate pinning */
	if ( !pin_matches(pPeer /* leaf digest comparison */ ) ) {
		return XTLS_VERIFY_REJECT;
	}
	return XTLS_VERIFY_ACCEPT;   /* protocol checks guaranteed by XRT */
}
```

### Pitfall 3: holding borrowed views past the Policy callback

Symptoms: accessing `xtlsverifiedpeer.Path` views after the callback returns — crash or garbage.

Cause: all the Policy callback's views (path, anchor, certificates) are **borrowed only during the callback** — invalid the moment it returns. Long-term retention requires a deep copy (copying the DER bytes).

```c bad
static bool myPolicy(const xtlsverifiedpeer* pPeer, ptr pCtx) {
	g_SavedCert = pPeer->Path[0];   /* saving a borrowed view: dangles on return */
	return true;
}
```

```c good
static bool myPolicy(const xtlsverifiedpeer* pPeer, ptr pCtx) {
	audit_log(pPeer);   /* read and record inside the callback */
	/* for long-term retention: copy the DER bytes here and manage the lifetime yourself */
	return true;
}
```

## Exercises

### Basic: baseline snapshot and tightening drill

Run the policy example and record the default baseline; then tighten to "TLS 1.3 only + two suites" (array replacement + `PolicyValid`), printing the new counts. Acceptance: after tightening, Valid passes; stuffing a 1.2-only signature scheme into a 1.3-only policy, Valid clearly rejects — the error message names the offending list.

### Advanced: a certificate-pinning verifier

Implement a pinning verifier: the `Verify` callback compares the leaf certificate's SHA-256 digest against a preset fingerprint array (Chapter 74's streaming digest + `ConstTimeEqual`), ACCEPT on hit, REJECT on miss. Verify with a self-signed certificate via a local variant of Chapter 82's dial. Acceptance: the right certificate passes; swapping in another rejects immediately; the fingerprint comparison is constant-time (explain why).

### Challenge: a three-layer composition with CRL

Assemble a "system trust + Policy-callback CRL check" verifier: the default path builds the chain (StoreSystem); the Policy callback runs Chapter 81's `CrlValidate + CrlCheck` on the leaf — REVOKED rejects, DONE goes soft-fail (log and admit), GOOD admits. Verify with a test CA generating a revoked certificate. Acceptance: the three revocation verdicts each take their path; the CRL is read only when the callback fires (proving "no implicit networking" — GOOD/REVOKED determinable without connecting anywhere); with a fixed injected time, the results are reproducible.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| The policy object | version/suite/group/signature four lists + KeySharePolicy; the default is the security baseline (2/9/4/14) |
| Policy validation | PolicyValid: non-empty, unique, suite↔version bidirectional coverage; 1.2 schemes cannot enter pure 1.3 |
| Policy boundary | preference ≠ capability; backend inclusion decided at session creation; snapshots immutable, shareable across machines |
| Verifier creation | deep-copies the store (freeable afterwards); no callback requires an anchor; empty store rejected |
| Three-layer decisions | default path (chain + purpose + name) / Verify callback (four-state takeover) / Policy callback (stacking CRL/OCSP/pinning) |
| Decision-execution separation | callbacks decide trust only; CertificateVerify/Finished guaranteed unbypassably by the protocol layer |
| Four-state decisions | ACCEPT takeover / REJECT refuse / DEFAULT fall back to store / ERROR with structured error |
| Time injection | timeproc unifies one deterministic time (tests/caches reproducible); omit for xrtNow() |
| Borrowing boundary | Policy-callback views valid only during the callback; long-term retention requires deep copy |
| No implicit networking | no auto system roots/CRL/OCSP/CT — fetching and soft-fail policy are yours |
