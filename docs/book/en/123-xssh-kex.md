---
num: 123
slug: xssh-kex
title: SSH (Part 2): Key Exchange
volume: 卷十一 其他扩展库
type: practice
lead: KEXINIT negotiation, ECDH messages, the SHA-256 transcript and A-F derivation, host-signature verification and the NEWKEYS switch — one complete SSH handshake.
api: xssh-ssh_kex_session, xssh-ssh_kex_exchange, xssh-ssh_kexinit
---

## Orientation

The transport layer (Chapter 120) provides an encrypted packet pipe — but where do the keys come from? **KEX (key exchange)** answers that — it is SSH's edition of the "TLS handshake" (Chapter 83), but in a more staged form. `ssh_kex_session` composes both sides' KEXINIT, Curve25519, the SHA-256 exchange hash, Ed25519 host-signature verification, A–D key derivation, and the NEWKEYS direction switch into a **deterministic state machine** — creating no socket/Engine/task/wait objects and storing no large KEXINIT buffers. Underneath sit four independent modules: ECDH messages (message-number + SSH string encoding/decoding), the SHA-256 transcript (streaming exchange hash and A–F key extension), the Curve25519 primitive (pure math, no randomness dependency), and the secure-random key pair (an independent trim layer). After this chapter you can read every message of a real SSH handshake.

## Introduction

SSH's handshake versus TLS 1.3 — alike and different. **Alike**: ECDHE (x25519 curve) + transcript hash + derivation chain + Finished/NEWKEYS switch — the same idea as Chapter 83's "keys derive from the transcript; tamper with any message and everything changes". **Different**: SSH's host-signature verification and key trust are **two steps** — the KEX session first does the cryptographic signature check (was this signature really made by this host key), then emits the `VERIFY_HOST_KEY` event for **the application** to make the trust judgment (is this host key one I know — known_hosts, Chapter 122); TLS fuses both steps into certificate validation. The staging lets SSH run without a certificate system — trust comes from "you noted this key on first connection" (TOFU) or pre-distribution.

The staging has a third beneficiary: **tests and dedicated devices**. `xrtSshKexSessionBeginWithPrivate` injects an ephemeral private key explicitly — a deterministic core that is testable; the random convenience layer `Begin` is its own module (bringing in only the system secure-random source) — standard-vector tests and dedicated key devices need not drag a randomness dependency. This is "deterministic core + convenience layer separation" carried through at KEX.

## Concepts

### KEXINIT and algorithm negotiation

Once the connection is established (after the version-string exchange) both sides send KEXINIT: each side's supported lists of kex algorithms/encryption/MAC/compression/host-key algorithms (in priority order). The negotiation rule differs from TLS — **SSH takes the first entry of the client's list that the server also supports** (client priority, the same shape as Chapter 93's subprotocol negotiation). The `ssh_kexinit` module provides building (`xrtSshKexInitWrite` — writing the negotiation lists) and parsing of the KEXINIT message. The session object absorbs both KEXINITs and fixes this round's algorithm combination.

### ECDH messages and Curve25519

```diagram flow
- C->S ECDH_INIT: Q_C (the client's ephemeral public key, 32B)
- S->C ECDH_REPLY: K_S (host public key blob) || Q_S (server ephemeral public key) || signature(exchange hash)
- Both: SharedSecret = x25519 scalar multiply (own private key, peer public key) - the server rejects the all-zero result of a low-order public key
```

The message layer (`xrtSshEcdhInitWrite/Read`, `EcdhReplyWrite/Read`) handles only message numbers and SSH string encoding — results borrow the original payload; wrong message numbers/truncation/trailing data rejected. The Curve25519 primitive has three entrances: `xrtSshCurve25519Public` (private→public), `Curve25519Shared` (the shared secret, rejecting the all-zero of a low-order public key — Chapter 75's malicious-public-key defense), `Curve25519KeyPair` (a random key pair — independently trimmed). **Private-key cleanup**: after transport use, `xrtSecureZero` is mandatory.

### The exchange hash and A–F derivation

`xrtSshKexHashSha256` writes the transcript **streaming** (building no intermediate big buffer): `V_C || V_S || I_C || I_S || K_S || Q_C || Q_S || K` — version strings, both KEXINITs, the host public key, both ephemeral public keys, the shared secret; the first seven items SSH-string-encoded, K encoded as a **canonical non-negative mpint** (an RFC 8731 detail: x25519's 32-byte output is interpreted directly as a network-order integer, not byte-reversed before mpint encoding — contract-literal precision). `xrtSshKexDeriveSha256` implements RFC 4253's **A–F key extension** (initial IVs ×2, C2S/S2C encryption keys, MAC keys ×2): the `HASH(K || H || char || session_id)` chain extension, supporting outputs beyond 32 bytes; the output must not overlap the input, and the shared secret must be nonzero.

### The host signature: verification and trust in two steps

Server side: `xrtSshKexSessionExchangeHash` takes the digest → a local private key/HSM signs → the signature blob goes to `EcdhReplyPrepare` — **which re-verifies the signature with the public host key**; a bad signature never reaches the wire (server self-check). Client side: after ECDH_REPLY's signature check passes, the host public key is **copied into caller storage** and a `XSSH_KEX_EVENT_VERIFY_HOST_KEY` event is raised — if known_hosts/certificates/application policy accept, `xrtSshKexSessionHostKeyAccept`; if not, close the transport and `xrtSshKexSessionFail`. **The meaning of the two steps**: cryptographic verification (library) separated from the trust judgment (policy) — the policy may be TOFU, known_hosts matching, a certificate chain (expanded in Chapters 119/120).

### Transactions and the direction switch

The same transaction discipline as the transport core: ECDH_INIT/ECDH_REPLY/NEWKEYS all run `Prepare` first — only after the transport core commits the packet does `WriteCommit` run (cancellation takes `WriteAbort`); receiving authenticates at the core first, then `ReadPrepare`, committing core and session in the same order; `ReadAbort` **terminates the session** (authenticated input cannot be rolled back — the session-layer echo of Chapter 120's ReadAbort-closes-core). **The key switch is per-direction**: after this side's NEWKEYS commits, `ActivateWrite` (write-direction switch); after the peer's authenticates, `ActivateRead` — the functions select C2S/S2C material by role, and **once the core takes over, the session's key copies are cleared** (minimal residency — Chapter 76's discipline). The transcript's four-segment views are valid only within this KEX round — to escape the network input's lifecycle, first copy exactly with `KexTranscriptMeasure/Write`.

## Examples

### First complete program: the session object's resource shape

The program below is from `examples/kex_session` — the KEX session's zero-burden statement:

```embed path="extlibs/xssh/examples/kex_session/main.c" title="extlibs/xssh/examples/kex_session/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xssh/single -include xssh.h impl.c extlibs/xssh/examples/kex_session/main.c -lws2_32 -liphlpapi
（输出 KEX 会话结构尺寸的自检结果）
```

**What just happened.** (1) `xrtSshKexSessionInit(&Session, XSSH_ROLE_CLIENT)` builds a client session on the stack — the same statement as Chapter 120's transport core: the sizeof output is the documentation of "not a heap monster". (2) The header comment points out: "creates no network, tasks, or large fixed buffers" — all of KEX's state (negotiation results, transcript context, derivation intermediates) lives in this stack object. (3) The real driving sequence (Begin→two KEXINITs→ECDH round trip→host-verification event→NEWKEYS→both Activates) is assembled by the upper client — Chapter 126's runtime wires this state machine to a socket; what this sample confirms is "the parts themselves are predictable".

### Second complete program: building KEXINIT

The second program is from `examples/kexinit` — writing out the negotiation lists:

```embed path="extlibs/xssh/examples/kexinit/main.c" title="extlibs/xssh/examples/kexinit/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xssh/single -include xssh.h impl.c extlibs/xssh/examples/kexinit/main.c -lws2_32 -liphlpapi
（输出 KEXINIT 报文尺寸与所选 kex 算法名的自检结果）
```

**What just happened.** (1) `xrtSshKexInitWrite` encodes the algorithm lists (comma-separated names of kex/encryption/host-key/compression/MAC) into an SSH message — `Writer.Size` is the message size, and the printed algorithm name comes from the post-build selection or first preference. (2) KEXINIT is **the handshake's identity statement**: list order is preference order (negotiation takes the client's order) — configuring it is configuring the client's algorithm stance (the SSH counterpart of Chapter 84's policy). (3) The companion family is testable layer by layer: `kex_curve25519` (pure primitive + standard vectors), `kex_ecdh` (message layer), `kex_sha256` (transcript and derivation vectors), `kex_exchange` (a static round trip of the four layers), the `*_random` family (deterministic contrast for the random convenience layer) — "an independent sample per layer" is the acceptance shape of layered design.

## Contracts

- **Deterministic core**: the session creates no socket/Engine/task/wait objects; the random convenience layer is its own module (bringing in only the system secure-random source).
- **Negotiation rule**: client list order ∩ server support, first by client order; KEXINIT encoded/decoded independently by `ssh_kexinit`.
- **ECDH messages**: message numbers and strings only; borrowing the payload; wrong numbers/truncation/trailing rejected.
- **Curve25519**: Shared rejects the all-zero of low-order public keys; KeyPair's random layer independently trimmed; private keys SecureZero'd after use.
- **Exchange hash**: eight transcript segments streamed (V_C..K); K a canonical non-negative mpint (x25519 output not reversed, directly mpint); shared secret nonzero.
- **A–F derivation**: the RFC 4253 extension chain; outputs beyond 32B supported; output must not overlap input.
- **Two-step verification**: the server self-verifies before Reply; the client raises VERIFY_HOST_KEY after the signature check — the trust judgment belongs to the application (Accept/Fail).
- **Transaction discipline**: Prepare→core commit→Commit/Abort; ReadAbort terminates the session (authenticated input cannot be rolled back).
- **Direction switch**: ActivateWrite/Read independent; material selected by role; the core's takeover clears the session's key copies.
- **Transcript lifecycle**: four-segment views valid this round; escaping requires exact copying first (Measure/Write).

## Pitfalls

### Pitfall 1: skipping the VERIFY_HOST_KEY event and Accepting outright

Symptom: for convenience the event callback calls `HostKeyAccept` unconditionally — man-in-the-middle attacks sail through (every host key is "trusted").

Cause: of the two steps — signature check (cryptography) and trust (policy) — the second is the one that defends against MITM; signature verification only proves "the key matches the signature", not "this is the server you meant to reach".

```c bad
case XSSH_KEX_EVENT_VERIFY_HOST_KEY:
	xrtSshKexSessionHostKeyAccept(&Session);  /* unconditional trust: MITM sails through */
	break;
```

```c good
case XSSH_KEX_EVENT_VERIFY_HOST_KEY:
	if ( known_hosts_check(Host) == TRUST ) {   /* Chapter 122's judgment */
		xrtSshKexSessionHostKeyAccept(&Session);
	} else {
		xrtSshKexSessionFail(&Session, REJECT);
	}
```

### Pitfall 2: transcript views crossing KEX rounds

Symptom: the second rekey round hashes garbage — the first round's KEXINIT views expired long ago.

Cause: the transcript's four-segment views borrow the network input buffer, **valid within this KEX round**; a rekey's new round has a new transcript.

```c bad
/* round one's I_C view saved and still used at round two's Activate */
```

```c good
/* to keep across rounds: copy exactly first */
xrtSshKexTranscriptMeasure(...);
xrtSshKexTranscriptWrite(...);   /* copy into the session workspace */
```

### Pitfall 3: forgetting to zero the ephemeral private key

Symptom: a memory audit finds x25519 private-key remnants — key-material lifecycle violation.

Cause: the KEX ephemeral private key is sensitive material (though short-lived); the contract says plainly "xrtSecureZero is mandatory after transport use ends".

```c bad
/* KEX done, the private-key array leaves scope naturally - the content still sits on the stack */
```

```c good
xrtSshKexSessionActivateRead(&Session, &Core);
/* after the core takes over it clears the session's key copies (automatic); zero your own ephemeral private key by hand: */
xrtSecureZero(Private, sizeof(Private));
```

## Exercises

### Basic: a four-layer static round trip

Run the `kex_curve25519`, `kex_sha256`, `kex_exchange` samples and check them against RFC 7748/8731 test vectors (private-key injection form). Acceptance criteria: the shared secret and exchange hash both hit the standard vectors.

### Advanced: policy injection for the host-verification event

Deterministically drive one full KEX round (injected private keys), injecting both policies — "trust" and "reject" — at the VERIFY_HOST_KEY event; observe the NEWKEYS flow after Accept and the transport close after Fail. Acceptance criteria: both paths' final `KexComplete`/closed states match the contract.

### Challenge: a rekey loop

After the first round completes, trigger a rekey (Chapter 120's rekey budget): new KEXINIT→new ECDH→both Activates — three rounds in a row. Acceptance criteria: keys change every round (compare derivation outputs); old key copies cleared each round (verified by memory scan); the transcript independent per round.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Four-layer split | ECDH messages / SHA-256 transcript / Curve25519 primitive / random key pair (independent trim) |
| Negotiation | client list order ∩ server support; KEXINIT independently encoded/decoded |
| exchange hash | eight segments streamed (V_C..K); K a canonical non-negative mpint; x25519 output not reversed |
| A–F derivation | RFC 4253 extension chain; initial IVs ×2 + encryption ×2 + MAC ×2; beyond-32B supported |
| Two-step verification | the library checks cryptography; the application judges trust (VERIFY_HOST_KEY event) |
| Server self-check | EcdhReplyPrepare re-verifies with the public key — bad signatures never go online |
| Low-order defense | Curve25519Shared rejects the all-zero result (malicious public keys) |
| Transactions | Prepare→core commit→Commit/Abort; ReadAbort terminates the session |
| Direction switch | ActivateWrite/Read independent; takeover clears copies; both live before complete |
| Randomness separated | BeginWithPrivate deterministic / Begin's random convenience layer its own module |
