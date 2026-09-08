---
num: 72
slug: vol8-intro
title: Volume 8 · Security: Introduction
volume: 卷八 安全
type: intro
lead: From "data delivered" to "data trustworthy" — the trust pyramid of hashes, AEAD, asymmetric crypto, and certificates up to TLS, plus the key discipline running through the whole volume.
api: crypto, hash, tls
---

## Orientation

At the end of Volume 7 your program could deliver bytes reliably to any corner of the world — but every transmission on the Internet crosses backbone networks, data centers, and proxies you cannot audit. The question Volume 8 answers is: **how to establish trust over untrusted channels**. This volume is the complete map of XRT's in-house cryptographic stack: from the smallest primitives (hashes, HMAC, AEAD, RSA/ECDSA/Ed25519/x25519) through certificate parsing and verification (DER/PEM, X.509, trust chains), finally assembling every primitive into a transport layer of "one handshake, mutual trust, per-record encryption" in the TLS client and server.

The volume unfolds as a **trust pyramid**, each layer depending only on those below:

```diagram flow
- Foundation (73-76): hash/HMAC/KDF provide integrity and key derivation; AEAD provides confidentiality + integrity; asymmetric crypto provides key exchange and signatures; the discipline chapter binds the three into a system
- Certificates (77-79): DER/PEM encoding -> X.509 certificate parsing -> trust chain and revocation verification — turning "which public key belongs to whom" into a verifiable assertion
- Transport (80-82): TLS client handshake and identity verification -> TLS server -> TLS streams — trust returns to Volume 7's xnetstream, encrypted per record
```

Why build an in-house cryptographic stack in 2026 instead of calling OpenSSL? XRT's answer is written in the trimming table: the cryptographic foundation is split by **minimal primitives** — a program using only WebSocket's SHA-1 or token randomness need not carry full TLS; an embedded target can take just ChaCha20-Poly1305 without RSA; every feature macro is independently trimmable. This splitting also gives the tutorial the chance to explain "why each layer exists" — most crypto-library tutorials can only teach "how to call", because the inside is a blob. XRT's layering is explicit: atop `CRYPTO_CORE` (digest metadata, secure zeroing, constant-time comparison), digests, HMAC/HKDF, AEAD, key exchange, and signatures each form independent layers, with X.509 and TLS composing layer by layer.

**Two cross-cutting disciplines** run through the volume; stated here first, redeemed in every chapter:

- **Key lifecycle**: where keys come from (randomness source, passphrase derivation, DH negotiation), how long they live (session, process, persistent), how they die (`xrtSecureZero`, `Clear`) — every link has an explicit API. Keys are not ordinary data: they can leave traces in logs, swap files, and crash dumps; the discipline chapter (76) is devoted to this.
- **Failure must be single**: a verification failure is a failure — AEAD's `Open` with a mismatched tag rejects wholesale, never returning "possibly tampered plaintext"; a certificate verification failure is a failure — there is no "continue but warn". Cryptography's security boundary is held up by these iron rules; any "fault tolerance" is attack surface.

**Links fore and aft**: Volume 7's `xnetstream` is this volume's TLS carrier (Chapter 86 directly produces encrypted streams); Volume 9's HTTP/HTTPS and Volume 10's xhttp stand on TLS; and Chapter 12's randomness source (`xrtSecureRandom`) is the entropy starting point of every key in this volume — the crypto modules explicitly depend on the separate `RANDOM_SECURE` feature, so file and network facilities need not carry cryptographic algorithms for it. In reverse, the "integrity thinking" learned here (digests, MACs, AAD binding) reappears in Volume 9's framing and upgrade protocols.

**Three kinds of readers, three routes**. **Application developers** (goal: HTTPS): go straight to Chapter 81's TLS client, then circle back to Chapter 79's verification policy as needed — but at least read Chapter 76's discipline; wrong use is more dangerous than no use. **Protocol/middleware developers** (custom protocols, tokens, cache signatures): read Chapters 73-76 closely — hash/KDF/AEAD/asymmetric are the direct toolbox; certificates and TLS can wait. **Audit/security engineers**: read the whole volume in order, focusing on 76 (discipline) and 79 (trust chains and revocation) — these two chapters concentrate the pattern library of "how security systems fail".

### How to read Volume 8: read the prohibitions first, then the interfaces

Every chapter's "Contracts" section in Volume 8 shares one structure: state **what must not be done** first, then what may — the MD5 chapter first declares "no collision resistance", the AEAD chapter first declares "on tag mismatch the output must not be used", the certificate chapter first declares "on verification failure do not continue". This is the reverse of the first seven volumes' "contracts state guarantees", because cryptographic failure modes are **invisible**: a repeated nonce raises no error, constant-time comparison or not doesn't affect functional tests passing, a wrong verification policy still connects to the server — the defect only becomes visible when an attacker exploits it, and by then it is too late. So the correct way to read this volume is to memorize every prohibition as part of the API: **prohibitions matter as much as function signatures**; code violating a prohibition is as wrong as calling a nonexistent function. The companion study habit is "watch it fail with your own hands": each chapter's pitfalls and exercises stage destructive experiments (tampering, replay, reordering) — one run through a failure path impresses more than ten readings of the success path.

### One reminder: cryptography is not all of security

What this volume gives you are **cryptographic primitives** — they solve "correctness at the algorithm layer"; real system security also depends on two more layers: **protocol-layer correctness** (field order, state machines, downgrade protection — in Chapters 81-83's TLS implementation you will see protocol correctness errs more easily than algorithm choice) and **system-layer correctness** (key storage, permissions, log hygiene, update channels — Chapter 76's territory). A common illusion is "we use AES-256, therefore secure" — algorithm strength is the layer least likely to be the weak link of the three; attackers always go for the other two first. Read Volume 8 with this layered lens: for every primitive you learn, ask "where does its key come from, where is it stored, who can touch it" — that question points your attention at the real weak link.

**Learning self-checks** (before leaving this volume):

- Given any piece of data, can you say "hash, HMAC, or signature" — what are the three key models and trust assumptions?
- Can you explain why AEAD nonces "must be unique under the same key", and the trade-off between the counter and randomness management schemes?
- Can you hand-draw TLS 1.3's four handshake messages (ClientHello, ServerHello, Finished, Finished) — what each carries and verifies?
- Given one DER and one PEM, can you state their relationship and conversion direction?
- How many pieces of key material does your program hold right now? Can you answer for each: its source, storage, and erasure path?

Where you cannot, return to the corresponding chapter — the chapters of this volume stack strictly like building blocks, and vagueness above almost always traces back to some chapter below.
