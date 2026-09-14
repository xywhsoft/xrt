---
num: 80
slug: x509-verify
title: X.509 Signature Verification and Service Identity
volume: 卷八 安全
type: practice
lead: Does the signature hold mathematically? Does the subject identity match? — the first two verification questions; the third, "should it be trusted", belongs to the trust chain.
api: x509, crypto
---

## Orientation

Chapter 79's parser told you what the certificate **says**; this chapter's two verifiers answer the first two true-or-false questions: **signature verification** — does this certificate's signature verify with its claimed algorithm and the issuer's public key, mathematically (`xrtX509CertificateVerify`, backed by protocol-dispatching `xrtX509SignatureVerify`); and **service identity** — does the certificate's claimed identity match the hostname you want to reach (RFC 9525's `xrtX509MatchHost/MatchDns`). The third question — "is this issuer worth trusting" — needs chain building and a trust store, Chapter 81's theme; this chapter draws the conceptual boundary. Both verifiers are zero-allocation borrowing paths; backends trim per algorithm family (RSA/ECDSA/Ed25519 each independent), so firmware verifying only ECDSA certificates need not compile in RSA code.

## Introduction

Consider the verification sequence in an HTTPS connection: receive certificate → parse (Chapter 79) → verify signature (this chapter) → match hostname (this chapter) → follow the Issuer to parent certificates, verifying level by level up to a trust anchor (Chapter 81). What if you do only the first two steps? An attacker can **self-sign a certificate**: CN and SAN both reading `bank.example.com`, signed with a key pair they generated themselves — signature verification passes 100% (mathematically entirely valid!), and identity matching passes too (the SAN really does carry the target domain). The missing link is "trust" — a valid signature only proves "someone holding the corresponding private key signed it"; **whether that someone is a CA you recognize**, the signature itself cannot answer.

Conversely, does a trust chain without signature verification work? Even less so — each link of the chain is precisely "verify the child's signature with the parent's public key". So the three questions are gates in series; this chapter implements the first two and delineates the third's interface boundary. Once you understand "which attacks each question blocks", you will never again write the classic vulnerable code "verify passed, therefore accept".

## Concepts

### Signature verification's layering: protocol dispatch + crypto backends

The verification entries split into two layers. The **convenience layer** `xrtX509CertificateVerify(证书, 签发者证书)`: parses the certificate's signature scheme and the issuer's SPKI, calling the lower layer to complete verification — the most common "verify the child with the parent" in one step. The variant `xrtX509CertificateVerifyKey(证书, 公钥)`: accepts an already-extracted public key or one given directly by trusted configuration — verifying an independent trust anchor (-pin-style public key pinning) without forging certificate objects. The **lower layer** `xrtX509SignatureVerify(方案, 内容, 签名, 公钥)`: accepts the open protocol model — universal for certificates, CRLs, OCSP, or your own signature objects.

Backends trim independently per algorithm family: `XRT_FEATURE_X509_VERIFY_RSA` (PKCS#1 v1.5 and PSS, SHA-1/224/256/384/512 all digests), `XRT_FEATURE_X509_VERIFY_ECDSA` (strict DER r,s decoding; P-256/P-384 chosen by the SPKI curve; digests enter the curve base via bits2int — not the rigid "P-256 pairs with SHA-256"), `XRT_FEATURE_X509_VERIFY_ED25519`. **A backend not compiled in returns `XERR_UNSUPPORTED`** — an explicit "I don't support this algorithm", never disguised as "signature mismatch"; already-parsed-but-backend-less P-521/Ed448 behave the same. This distinction matters for both debugging and policy: a mismatch is a security problem; unsupported is a configuration problem.

### What is verified and what is not: Verify's boundary

`CertificateVerify` verifies only the TBSCertificate's **cryptographic signature**. What it explicitly does not do: it does not compare issuer/subject Names (chain matching is Chapter 81's duty), does not check time, does not check BasicConstraints/KeyUsage, does not check unknown critical extensions, does not query revocation, does not judge trust anchors. This boundary lets root-certificate health checks (self-signed certificates self-verified) and CA material audits (offline verification of every certificate's signature integrity) reuse the same entry — they need only mathematics, not policy.

### Service identity: RFC 9525's matching rules

Identity matching answers "is this certificate really for `api.example.test`":

- `xrtX509MatchHost(证书, 主机名, &呈现身份)`: the certificate entry — reads only the **SubjectAltName**'s DNS-IDs and IP-IDs, **never the CN**. Three-state return: `X509_VALUE` (match, optionally producing the actually-hit SAN), `X509_DONE` (reference identity legal but no match — **the TLS layer must treat the handshake as failed**), `X509_ERROR` (reference identity or SAN structure illegal).
- `xrtX509MatchDns(模式, 主机名)`: the pure-string form of DNS matching — wildcard rules built in: **matches exactly one complete leftmost label** (`*.example.test` hits `api.example.test`, not `a.b.example.test`), appears only in the left label, ASCII case-insensitive, trailing root-dot normalization (`example.test.` equals `example.test`). Invalid presented DNS-IDs are ignored rather than erroring.
- IP identity: IPv4/IPv6 text strictly parsed by `xrtNetAddrParse` then compared **exactly** against the iPAddress SAN as 4/16 bytes (bracketed IPv6 common in URLs supported; Scope IDs rejected). Note that "the connection address obtained by DNS resolution" should still match the original DNS-ID, not the resolved IP.

Hand-implementing these rules is a historical vulnerability hotspot — `*.example.test` matching `a.b.example.test`, wildcards in the middle, case-sensitive comparisons causing misses — the built-in entries eliminate the whole class at once.

### The engineering shape of the verification path

One complete "first two questions" verification (the TLS client's minimal actions upon receiving a certificate):

```diagram flow
- Parse: Parse produces the view (structural legality guaranteed by the parser)
- Signature: CertificateVerify(certificate, issuer certificate) — mathematically valid?
- Identity: MatchHost(certificate, expected hostname) == X509_VALUE — name matched?
- Time: ValidAt(certificate, now) — not expired?
- (Chapter 81) Chain: follow the Issuer to a trust anchor + revocation check
```

Note "time" mixed in — it is neither signature nor identity, but as an independent check it always executes at the same moment; Chapter 81's path validation orchestrates these three together with chain building and policy.

## Examples

### First complete program: self-verification of a self-signed certificate

The following program comes from `examples/x509/verify/main.c`, parsing a real self-signed RSA certificate and verifying its signature:

```embed path="examples/x509/verify/main.c" title="examples/x509/verify/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -I . -include xrt.h impl.c examples/x509/verify/main.c -lws2_32 -liphlpapi
certificate signature is valid
```

**What just happened.** (1) The input is real certificate bytes from the test fixtures (`tests/fixtures/x509_legacy_cert.h`) — far larger than inspect's toy certificate, going through full parse validation. (2) The same certificate passes twice: `CertificateVerify(&Cert, &Cert)` — the verifyee and the "issuer" are both itself (a self-signature's definition is "signed by itself, public key included"). A pass outputs a **mathematical fact**: this signature was indeed generated by the private key corresponding to this public key. (3) The failure branch prints `xrtErrorMessage(xrtGetError())` — the failure cause chain (`xrt.x509`'s signature error, `xrt.crypto`'s underlying reason) goes straight to humans; programmatic judgment uses Chapter 4's category queries. (4) Note there is **no** "trust" judgment here — a self-signed certificate passing verification is normal (every root certificate is self-signed); whether it can be trusted is Chapter 81's trust-store business.

### Second complete program: wildcard identity matching

The second program comes from `examples/x509/identity/main.c`, verifying whether `*.example.test` matches `api.example.test`:

```embed path="examples/x509/identity/main.c" title="examples/x509/identity/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/x509/identity/main.c -lws2_32 -liphlpapi
matched=yes
```

**What just happened.** (1) `xrtX509MatchDns` is the pure-function form — both the certificate pattern and the reference hostname are string views, no certificate object needed; the certificate scenario has `MatchHost` internally applying the same rules to each SAN DNS-ID. (2) The hit demonstrates the core wildcard boundary: `*` swallows exactly **one complete leftmost label** — switching to `a.b.example.test` yields `X509_DONE` (legal but unmatched), and `api.example.org` likewise DONE. (3) Case-insensitivity holds automatically here (`API.EXAMPLE.TEST` also hits) — ASCII label-level comparison, no Unicode folding (U-labels should be converted to A-labels by the IDNA layer first). Running all three variant groups is your fastest path to understanding RFC 9525's boundaries.

## Contracts

- **Layered trimming**: the base `x509_verify` pulls in no cryptographic algorithms; `XRT_FEATURE_X509_VERIFY_RSA/ECDSA/ED25519` enabled per certificate family; unenabled algorithms return `XERR_UNSUPPORTED` (never disguised as signature mismatch).
- **Convenience entries**: `CertificateVerify(证书, 签发者证书)` and `CertificateVerifyKey(证书, 公钥)` verify only the TBSCertificate's cryptographic signature — no name matching, time, constraints, revocation, or trust.
- **Lower-layer entry**: `SignatureVerify(方案, 内容, 签名, 公钥)` accepts the open protocol model — universal for certificates/CRL/OCSP/custom objects; the scheme comes from Chapter 79's algorithm translation layer.
- **RSA rules**: PKCS#1 v1.5 and PSS over all digests; id-RSASSA-PSS public keys verify PSS only with agreeing parameters and salt length not below the public key's declared floor; parameterless PSS public keys impose no restriction.
- **ECDSA rules**: strict DER r,s decoding; curve from the SPKI; digest enters the curve base via bits2int.
- **Identity rules**: RFC 9525 — read only SAN's DNS-ID/IP-ID, never CN; wildcard only the single leftmost label; ASCII case-insensitive; IP exact comparison, Scope IDs rejected.
- **Three-state semantics**: `MatchHost`'s `X509_VALUE` match / `X509_DONE` legal-unmatched (TLS must treat as failure) / `X509_ERROR` illegal structure.
- **Failure shape**: verification failure returns `false` and sets an `xrt.x509` error, with the cryptographic reason preserved via `xrtErrorCause` under `xrt.crypto`; the valid path — stack digests + borrowed views — zero heap allocation.

## Pitfalls

### Pitfall 1: trusting because verification passed

Symptoms: code accepts as soon as `CertificateVerify` succeeds — an attacker with a self-signed certificate (CN/SAN all carrying the target domain) passes directly: the signature is mathematically entirely valid.

Cause: signature verification's answer is "a private-key holder signed it", not "a CA you recognize signed it". Trust is the chain's and trust store's judgment (Chapter 81).

```c bad
if ( xrtX509CertificateVerify(&Cert, &Issuer) ) {
	accept_connection();   /* mathematically valid ≠ trusted: self-signed is also valid */
}
```

```c good
/* signature + identity + (Chapter 81) chain to a trust anchor — all three questions pass before accept */
if ( xrtX509CertificateVerify(&Cert, &Issuer) &&
		xrtX509MatchHost(&Cert, XRT_STR_LITERAL(sHost),
			&Presented) == X509_VALUE &&
		xrtX509ValidAt(&Cert, xrtNow()) &&
		trusted_to_anchor(&Cert /* Chapter 81 */) ) {
	accept_connection();
}
```

### Pitfall 2: treating X509_DONE as success (or as a crash)

Symptoms: the hostname didn't match (DONE), but the caller checks only `!= X509_ERROR` and continues — the wrong service is admitted; or an assertion crashes on ERROR — an illegal SAN input becomes an availability fault.

Cause: of the three states, `DONE` means "legal but unmatched" — **for TLS that is identity-verification failure** and must be rejected; `ERROR` means "input structure illegal" — also rejected, but as a reportable protocol error, not a crash.

```c bad
if ( xrtX509MatchHost(&Cert, XRT_STR_LITERAL(sHost),
		&Presented) != X509_ERROR ) {
	accept();   /* DONE slips in too: unmatched identity admitted */
}
```

```c good
switch ( xrtX509MatchHost(&Cert, XRT_STR_LITERAL(sHost),
		&Presented) ) {
case X509_VALUE:
	accept();           /* the only admitting path */
	break;
case X509_DONE:
	reject("identity mismatch");   /* legal, but not this host */
	break;
default:
	reject("malformed identity");  /* illegal structure: report then reject */
	break;
}
```

### Pitfall 3: identity matching falling back to CN

Symptoms: connections to old certificates without SAN (v1/v2 or early v3) still succeed — the code falls back to comparing CN when SAN doesn't match; an attacker passes verification with a certificate whose CN reads the target domain and that has no SAN.

Cause: RFC 9525 explicitly abolished the CN fallback. A certificate without SAN should fail identity in modern TLS — "backward compatibility" here equals opening a forgery channel.

```c bad
if ( xrtX509MatchHost(&Cert, ...) != X509_VALUE ) {
	/* CN fallback: a forged certificate's CN can be any string */
	if ( cn_equals_host(&Cert, sHost) ) { accept(); }
}
```

```c good
/* no SAN: identity verification fails, period.
   The exception is internal systems where you fully control the certificates —
   then use CertificateVerifyKey for public-key pinning, more honest than CN */
if ( xrtX509MatchHost(&Cert, XRT_STR_LITERAL(sHost),
		&Presented) != X509_VALUE ) {
	reject("no SAN match; CN fallback is forbidden");
}
```

## Exercises

### Basic: the three-state matching matrix

Build six input groups for `xrtX509MatchDns`: exact hit, case-variant hit, wildcard single-label hit, wildcard multi-label miss, different-suffix unmatched, trailing root-dot hit. Print the three-state result per group. Acceptance: the six results match hand-derived RFC 9525 rules; understand why "wildcard multi-label" is DONE, not ERROR.

### Advanced: a public-key-pinning verifier

Implement `verify_pinned(证书 DER, 主机名, 钉扎公钥 SPKI)`: parse → `CertificateVerifyKey` verifies the signature with the pinned key → `MatchHost` matches the hostname → `ValidAt` checks time; all three questions pass, return true. Hint: pinning needs no certificate chain — the public key itself is the trust anchor. Acceptance: with a self-generated key pair and self-signed certificate, walk both the positive and negative paths (swapping the pinned key must fail).

### Challenge: a verification-backend probe report

For a batch of certificates (RSA-PKCS1, RSA-PSS, ECDSA P-256, ECDSA P-384, Ed25519 one each, OpenSSL-generated) write a probe tool: parse the algorithm and public-key type, attempt `CertificateVerify` (self-signed self-verify), printing `XERR_UNSUPPORTED` distinctly from signature mismatch. Then run under both "all backends" and "ECDSA only" trims and compare outputs. Acceptance: before and after trimming, supported algorithms' conclusions unchanged, unsupported algorithms go from "valid" to a clear UNSUPPORTED report — proving trimming produces no silent false negatives.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Three-question model | signature (this chapter) → identity (this chapter) → trust chain (Chapter 81); series gates, none omittable |
| Convenience entries | `CertificateVerify(证书, 签发者)` / `VerifyKey(证书, 公钥)` — mathematical signature only |
| Lower-layer entry | `SignatureVerify(方案, 内容, 签名, 公钥)` — universal for certificates/CRL/OCSP/custom |
| Backend trimming | XRT_FEATURE_X509_VERIFY_RSA / _ECDSA / _ED25519 independent; unenabled → UNSUPPORTED |
| RSA rules | v1.5 + PSS over all digests; PSS public keys with agreeing parameters and salt floor; parameterless keys unrestricted |
| ECDSA rules | strict DER r,s; curve from SPKI; digest into the curve via bits2int |
| Identity entries | `MatchHost` (certificate + hostname) three-state / `MatchDns` (pure strings) same rules |
| Wildcards | single leftmost complete label only; ASCII case-insensitive; trailing root-dot normalized |
| Three-state iron rule | only VALUE admits; DONE = identity failure, must reject; ERROR = illegal structure, report then reject |
| CN boundary | never fall back to CN; no SAN means identity failure; internal systems use public-key pinning instead |
