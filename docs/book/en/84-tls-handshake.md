---
num: 84
slug: tls-handshake
title: The Handshake Timeline Diagrammed: From ClientHello to Finished
volume: 卷八 安全
type: practice
lead: TLS 1.3's four core messages, the HKDF chain of the key schedule, HelloRetryRequest and version negotiation — turning the handshake from a black box into a readable timeline.
api: tls, crypto
---

## Orientation

In Chapter 82 you pressed one function and the handshake ran by itself; this chapter opens the black box. The TLS 1.3 handshake really has only four core messages: ClientHello, ServerHello, (EncryptedExtensions, Certificate, CertificateVerify,) Finished, plus the client's own Finished — each message's "who sent it, what it carries, what it verifies" is laid out, together with the **key-schedule chain** running throughout (transcript digest → handshake secret → bidirectional traffic secrets → application secrets — all HKDF-driven, where Chapters 74/77's primitives converge). Then three engineering branches: HelloRetryRequest (the server switches groups), version and suite negotiation (1.2/1.3 dual stack), and KeyUpdate (long-connection rekeying). This chapter's examples use two independent entries — the key-exchange primitives and certificate-message encode/decode — to split the handshake's two core mechanisms into unit-testable pieces — Chapter 77's session skeleton fully unfolded at the protocol layer.

## Introduction

When debugging TLS failures, "the handshake hung" carries zero information. Was it the ClientHello's group unsupported (a HelloRetryRequest loop)? Certificate verification failing after ServerHello? The CertificateVerify signature not holding? Or a Finished digest mismatch (a middlebox altered transcript bytes)? Each failure class has different handling — and someone who doesn't understand the timeline can only restart and hope.

The key to understanding the timeline is one insight: **every step of the TLS 1.3 handshake accumulates a transcript** — both sides feed every handshake message sent and received into the same running digest; keys derive from the transcript, Finished verifies the transcript, resumption tickets bind the transcript. Change one byte of any message and every later key changes — that is the mechanism of "handshake tamper-proofing" itself, not magic but a hash chain. Read the four messages with this lens and every field answers "why it must be here".

## Concepts

### The four core messages' timeline

```diagram flow
- ClientHello (cleartext): highest supported version + random + session ID + suite list + group list + signature scheme list + key share (preferred group's public key) + SNI/ALPN/PSK
- ServerHello (cleartext): chosen version + suite + random + echoed session ID + chosen key share — both sides compute the handshake secret here
- [server ciphertext begins] EncryptedExtensions: SNI confirmation, ALPN chosen, group-negotiation extras
- Certificate: leaf + intermediate chain (entry-based, each entry optionally with an OCSP extension)
- CertificateVerify: signature by the certificate's private key over "role prefix + transcript digest" — the identity proof itself
- Finished: an HMAC over "the transcript before adding this message" — the tamper-proof proof itself
- [client verifies chain and digest] client Finished (protected by the server write key): after mutual confirmation, switch to application keys
```

Three reading lines. The **plaintext/ciphertext boundary** sits after ServerHello — the server's EE/Cert/CV/Finished all undergo encryption; eavesdroppers see certificate lengths but no contents; **what each message verifies**: EE verifies negotiation consistency, CV verifies identity (Chapters 80/81's verification triggers here), Finished verifies global integrity; **key-switch points** number two — after ServerHello both sides derive handshake keys (EE onward encrypted), and after the client's Finished the application keys take over.

### The key schedule: one HKDF chain

```diagram flow
- ECDHE: ClientHello's and ServerHello's key shares -> shared secret (Chapter 76's ECDH)
- Early Secret = HKDF-Extract(0, PSK or 0)
- Handshake Secret = HKDF-Extract(Early, ECDHE shared secret)
- bidirectional handshake traffic secrets = HKDF-Expand(Handshake, "c hs traffic"/"s hs traffic", transcript digest)
- Master Secret = HKDF-Extract(Handshake, 0)
- bidirectional application traffic secrets = Expand(Master, "c ap traffic"/"s ap traffic", fresh transcript digest)
- each direction's secret -> HKDF-Expand -> the record layer's key and IV (Chapter 75's AEAD slots in)
```

Compare with Chapter 77's session example: the same "ECDH → HKDF → AEAD" skeleton, which TLS builds into a multi-stage ladder — every Extract/Expand binds **the transcript digest at that moment** into the key, so keys lock not just the secret but "everything we've said". Each direction has its own secret (client-write/server-read share one, and vice versa) — one-directional rekeying (KeyUpdate) becomes possible because of this. The client's ephemeral private key is **erased immediately** after the handshake-key switch succeeds — forward secrecy's landing point.

### HelloRetryRequest: the server says "switch groups and come again"

When the client's preferred group (say x25519) is unsupported by the server, the server replies HRR asking to switch groups and resend the ClientHello. XRT's client and server support a single HRR — the resent ClientHello switches to the server's designated group (say P-256) and carries the `h2` retry marker. Fully transparent to the application; important for diagnostics: "two ClientHellos" in connection logs is not a reconnect, it's HRR.

### Version and suite negotiation

Dual-stack (1.2/1.3) negotiation rules: peer's offered list ∩ local support → **highest version**; suites then pass a triple filter — peer's offered order + local support + **identity-type compatibility** (an RSA identity excludes pure-ECDSA suites). The local policy array (Chapter 85) is the input baseline for both steps; the server runs them on every connection. `xrtTlsVersionSelect/CipherSelect` factor negotiation out into unit-testable pure functions — this chapter's example demonstrates the full flow. TLS 1.2's requirements: EMS (extended master secret) and the RFC 5746 empty initial binding mandatory, renegotiation not offered — the old protocol's known weaknesses closed outright by engineering decision.

### KeyUpdate: the long connection's rekeying rhythm

When application traffic reaches a threshold, the record layer rekeys automatically (transparent to the application); you can also trigger `xrtTlsClientKeyUpdate(方向)` manually. Rules: a KeyUpdate message occupies its own record; `update_not_requested` rekeys only the read direction; `update_requested` queues the answer under the old write key first, then switches — **no AGAIN interruption ever produces a half-updated state** (old keys, sequence numbers, and queues preserved as-is; retry after the output drains). The epoch order of queued data is guaranteed by the wire queue — Chapter 75's "nonce counter" discipline automated at the protocol layer.

### The certificate message: the entry model

TLS 1.3's Certificate is organized by **entries** (each entry = a DER certificate + optional extensions like OCSP status); 1.2 is a bare list — `xrtTlsCertificateEncode/Parse/Entries` handles both versions uniformly (the first parameter gives the version). Entries are DER views, with parsing delegated to Chapter 79's `xrtX509Parse`. The client's state machine has strict rules for entry extensions: unrequested per-entry extensions are rejected outright.

## Examples

### First complete program: the metadata-driven bidirectional key-exchange loop

The following program comes from `examples/tls/key_exchange/main.c` — a standalone unit test of the handshake's core mechanism (key-share generation and derivation):

```embed path="examples/tls/key_exchange/main.c" title="examples/tls/key_exchange/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/tls/key_exchange/main.c -lws2_32 -liphlpapi
group=29 private=32 public=32 shared=32
```

**What just happened.** (1) `exampleGroup` searches in x25519 → P-256 → x448 → P-384 order for a group **available in the current build** (`xrtTlsGroupAvailable` reflects trimming and backend capabilities) — group=29 is x25519's TLS group number. (2) `xtlsgroupinfo` gives the three buffers' exact sizes — private 32/public 32/shared 32 (x25519 all 32; P-384 would be 48/97/48): **buffers sized from metadata, no guessed constants**, the same code runs any group. (3) After the bidirectional `Derive`, memcmp equal — ECDH symmetry (Chapter 76) verified under the TLS group wrapper. This is all the mathematics of the key-share extension in ClientHello/ServerHello: Generate twice, exchange public keys, Derive the ECDHE shared secret — the first link of the key-schedule chain.

### Second complete program: encoding and entry traversal of the Certificate message

The second program comes from `examples/tls/messages/main.c`, the full round trip of chain → message body → entry cursor:

```embed path="examples/tls/messages/main.c" title="examples/tls/messages/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/tls/messages/main.c -lws2_32 -liphlpapi
certificate[0]: 3 bytes
certificate[1]: 3 bytes
```

**What just happened.** (1) Two-pass encoding: `CertificateSize` measures the body first (the empty `request_context` view means the client direction; the server direction's CV context is non-empty), then `CertificateEncode` writes — a two-entry chain (leaf + issuer, here 3-byte toy DERs) encoded into TLS 1.3 entry format. (2) `CertificateParse` produces the message view, `Entries` builds the cursor, `Read` walks three-state — the same vocabulary as the DER/PEM cursors (Chapter 78). (3) Each `Entry.Data` is a **DER view** — the real parsing goes to Chapter 79; the two layers each mind their own: the TLS layer manages entry structure, the X.509 layer certificate semantics. Proxies, sniffers, and certificate-transparency logs all use this pair of entries to handle wire certificates.

## Contracts

- **Protocol scope**: TLS 1.2/1.3 dual stack; 1.2 requires EMS and the empty initial binding, no renegotiation; Ed448 unpublished; P-521/SHA-512 excluded from the 1.3 offer.
- **Transcript unbypassable**: all handshake messages enter the digest; key derivation and Finished both bind the transcript; CertificateVerify's signed object is "role prefix + the digest before adding CV".
- **Key-switch atomicity**: any step failing enters FAILED, publishing no half-set of keys; the client's ephemeral private key is erased immediately after a successful switch (forward secrecy).
- **HRR**: client and server support a single retry; the resent ClientHello switches groups and carries the marker; transparent to the application.
- **Negotiation rules**: version takes the intersection's highest; suites pass the offered-order + local-support + identity-compatibility triple filter; the policy array is the baseline (Chapter 85).
- **KeyUpdate**: occupies its own record; two request modes; the answer under the old write key; AGAIN has no half-updated state; the queue guarantees epoch order.
- **Certificate entries**: 1.3 entry-based (extensions allowed), 1.2 bare list, unified Encode/Parse entries; entry DER views go to the X.509 layer; unrequested entry extensions rejected.
- **Error mapping**: failures enter `XTLS_STATE_FAILED` with structured errors; `XTLS_AGAIN` is merely "more input/output space needed".

## Pitfalls

### Pitfall 1: taking HRR as a failed-connection retry

Symptoms: monitoring sees two ClientHellos on one connection and misjudges "the client keeps reconnecting", triggering alert storms; or a hand-written proxy treats HRR as an error and disconnects.

Cause: HelloRetryRequest is a normal in-protocol branch — the server asking to switch groups and resend. 1.3 allows it (XRT supports once); more than once is the anomaly.

```c bad
/* at the capture/log layer: alert on seeing a second ClientHello */
if ( hello_count(client) > 1 ) {
	alert("client stuck in reconnect loop");
}
```

```c good
/* HRR has clear signatures: the ServerHello's special random value + the h2 retry marker.
   The session state machine folds it into a normal handshake — log by state, not by packet count */
if ( tls_state(session) == XTLS_STATE_FAILED ) {
	alert(failure_reason(session));
}
```

### Pitfall 2: "helpfully" optimizing the transcript

Symptoms: a hand-written middle layer (proxy, logger, accelerator) alters handshake-record boundaries — merging or splitting records — and Finished never verifies again.

Cause: the transcript digest's object is the **handshake messages**, not records, but message integrity depends on byte-faithful records; any "optimization" of wire bytes (reassembly, re-encoding) changes the transcript and Finished's HMAC mismatches — precisely the tampering the protocol exists to detect.

```c bad
/* a middle layer "tidies" record boundaries before forwarding */
merge_small_records(&Inbound);   /* the transcript changed */
```

```c good
/* forward raw bytes transparently; to parse, use the record layer's read-only entries */
/* (Chapter 87's read-only record entries exist exactly for such tools) */
```

### Pitfall 3: stuffing TLS-1.2-only schemes into a 1.3 policy

Symptoms: `xrtTlsPolicyValid` rejects the configuration — "the signature list contains a 1.2-only scheme"; or conversely, a pure-1.2 deployment stuffed with 1.3-only suites.

Cause: policy validation keeps every list self-consistent with the enabled versions: every suite corresponds to an enabled version, and a non-empty signature list is usable by at least one enabled version — a 1.2 PKCS#1 scheme mixed into a pure-1.3 policy gets intercepted. This is the policy layer's value: catching configuration errors at startup.

```c bad
static const xtlsversion Versions[] = { XTLS_VERSION_13 };
Policy.Versions = Versions;
Policy.VersionCount = 1;
/* SignatureCount unchanged: still the full default family (including 1.2-only) -> Valid rejects */
```

```c good
static const xtlsversion Versions[] = { XTLS_VERSION_13 };
static const xtlssignature Sigs13[] = { /* only 1.3 schemes */ };
Policy.Versions = Versions;
Policy.VersionCount = 1;
Policy.Signatures = Sigs13;
Policy.SignatureCount = /* consistent with the array */;
if ( !xrtTlsPolicyValid(&Policy) ) { /* report the configuration error at startup */ }
```

## Exercises

### Basic: run two groups of key exchange

Change `exampleGroup`'s order to prefer P-256 and rerun key_exchange — observe the group number and the three sizes changing (a 97-byte public key). Then comment out every group and verify `Available`'s rejection path. Acceptance: both groups close the loop; the size changes match the `xtlsgroupinfo` documentation.

### Advanced: certificate-message version comparison

Encode the same entry chain with `xrtTlsCertificateEncode/Parse` under `XTLS_VERSION_12` and `XTLS_VERSION_13`; dump both bodies' hex and compare the structural difference (1.2 has no request_context or entry extensions). Acceptance: both bodies walkable by their version's `Parse+Entries`; you can point out the fields 1.3 adds.

### Challenge: a handshake-timeline observer

With the record-parsing layer (Chapter 87) or a raw socket, capture one complete handshake (a local `dial` to any site) and annotate per this chapter's timeline: each record's type, the cleartext/ciphertext boundary, the handshake-message type sequence. Acceptance: the annotated sequence corresponds exactly to this chapter's timeline (including a possible single HRR); you can name what you can no longer see after ServerHello (the ciphertext begins).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Four messages | ClientHello / ServerHello / (EE+Cert+CV) / Finished + the client's Finished |
| Clear/cipher boundary | after ServerHello all server messages encrypted; eavesdroppers see lengths only |
| Transcript | the running digest of all handshake messages; keys and Finished both bind it |
| Key schedule | ECDHE → Extract chain (Early/Handshake/Master) → bidirectional traffic secrets → key/IV |
| Direction independence | each direction has its own secret; one-directional rekeying (KeyUpdate) follows |
| Forward secrecy | the client's ephemeral private key erased right after the handshake-key switch |
| HRR | the server's group-switch request; once; the new ClientHello carries the marker; transparent |
| Negotiation | version takes the intersection's highest; suites pass offered + local + identity-compatibility |
| KeyUpdate | own record; the answer under the old write key; AGAIN never half-updates; epoch order guaranteed |
| Certificate entries | 1.3 entry-based / 1.2 bare list unified entries; entry DER views go to the X.509 layer |
| 1.2 boundary | EMS mandatory, no renegotiation; 1.2-only schemes cannot enter a pure-1.3 policy |
