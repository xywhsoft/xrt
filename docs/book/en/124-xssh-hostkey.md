---
num: 124
slug: xssh-hostkey
title: SSH (Part 3): Host Keys and known_hosts
volume: 卷十一 其他扩展库
type: practice
lead: The allocation-free host-key format layer, Ed25519 verification, fingerprint computation, known_hosts' trust states and hashed hostnames — every tool for SSH's trust judgment.
api: xssh-ssh_hostkey, xssh-ssh_known_host_db, xssh-ssh_fingerprint
---

## Orientation

Chapter 121's VERIFY_HOST_KEY event handed the trust judgment to the application — this chapter is every tool for that judgment. **Format layer** (`ssh_hostkey`): encoding/decoding of generic public keys/signatures (algorithm name + raw parameter views — new algorithms don't touch the public prefix parser) plus the `ssh-ed25519` fixed format; **verification layer** (`ssh_hostkey_ed25519`): the cryptographic implementation of signature verification; **fingerprints** (`ssh_fingerprint`): the host key's SHA-256 fingerprint — the shape of "humans comparing keys"; **the trust store** (`ssh_known_host`/`_db`/`_hash`): known_hosts-format parsing/matching/writing, multiple trust states (MATCH/NEW/CHANGED/REVOKED/CERT — a change is a changed-key alarm) and hashed hostnames (the privacy shape). known_hosts is the core file of SSH's trust model — the complete TOFU implementation of "note it the first time, compare ever after, shout when it changes".

## Introduction

On first connection to a new server, the client asks "I'm about to note this host key — trust it?" — that is TOFU (Trust On First Use). Every later connection compares the peer's key against the record: identical, pass; no record (a new machine), prompt; **different** — either the server changed keys (reinstall/rotation) or **someone is between you and the server** (a man in the middle). The third case is known_hosts' entire reason for existing — it is the only client-side defense against "network-position hijacking" (the spare-parts edition of the certificate system). `MATCH` passes, `NEW` (no record) asks, `CHANGED` (a record exists but the key differs) **shouts** — the extra "changed" state beyond PuTTY is where the security value lives.

The tool layer's design mirrors Chapter 78's X.509 views: the format layer is allocation-free (parameters keep their raw encoding — algorithm evolution never touches the public parser), the verification layer is independent (its cryptography subject to trimming), and the trust layer is a pure file protocol (touching no network and no connections).

## Concepts

### The generic format: prefix parsing and algorithm extension

`xrtSshPublicKeyRead` reads the algorithm name (string) + the fields after it as a **raw `Parameters` view** — the public prefix parser knows nothing of each algorithm's parameter structure, so adding RSA/ECDSA/secure-key algorithms **requires no change to it**. `xrtSshSignatureRead` strictly reads `string algorithm || string signature` and rejects trailing data; `SignatureWrite` writes directly into the caller's buffer. The Ed25519 fixed format (`ssh_hostkey_ed25519`): public key = 32-byte key (string-wrapped), signature = 64-byte r||s (string-wrapped) — a read/write/verify trio.

### Fingerprints: the shape of human comparison

A host key is dozens of unreadable bytes — the fingerprint (`ssh_fingerprint`) compresses it into a short `SHA256:`-prefixed hexadecimal string (Chapter 73's digest, a protocol application). Two uses: the "eyeball confirmation" of first-time TOFU recording (checked against an out-of-band channel — console output/administrator verification); and the key identifier in logs and alarms (a "key changed" event records the fingerprint, not the full key).

### known_hosts: format and states

```diagram flow
- File line: [|@]marker [|]hostnames keytype base64-key [comment]
  (@ marks revoked; hashed hostname in |1|salt|hash form)
- Query: Check(hostname list, port, key view) -> a trust state
  MATCH = key matches some line / NEW = host has no record (the TOFU entrance) /
  CHANGED = host has a record but a different key (re-keyed or MITM - alarm!)
- Record: a new host's line is written; a revoked line outranks any match
```

`ssh_known_host` is the single-line/in-memory form of parsing and matching; `ssh_known_host_db` is file-level (load/query/append); `ssh_known_host_hash` is the hashed hostname (`|1|salt|hash` — preventing the known_hosts file itself from leaking intranet hostnames; matching recomputes by salt). **Port semantics**: a non-standard port's hostname takes the `[host]:port` form — the same machine on a different port is a different identity.

### Assembling the trust decision

Wiring this chapter's tools to Chapter 121's event: query the store before `HostKeyAccept` — MATCH passes; NEW per policy (interactive prompt/out-of-band fingerprint check then record/configured allowlist); CONFLICT rejects and alarms. The fingerprint is printed in both NEW and CHANGED states — for verification in the former, for reporting in the latter. This assembly has its standard form in Chapter 126's client runtime.

## Examples

### First complete program: writing and reading back an Ed25519 public key

The program below is from `examples/hostkey` — the format layer's round trip:

```embed path="extlibs/xssh/examples/hostkey/main.c" title="extlibs/xssh/examples/hostkey/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xssh/single -include xssh.h impl.c extlibs/xssh/examples/hostkey/main.c -lws2_32 -liphlpapi
（输出 ed25519 公钥尺寸与 blob 尺寸的自检结果）
```

**What just happened.** (1) `xrtSshEd25519PublicKeyWrite` encodes the algorithm name (`ssh-ed25519`) + the 32-byte key into the standard blob — `Writer.Size` is the blob size. (2) `xrtSshEd25519PublicKeyRead` reads it back — the relation between `PublicKey.Size` (32) and the blob size is exactly the wrapping overhead of "algorithm-name string + key string". (3) The round trip matching means format self-consistency — this blob is exactly the shape of `K_S` inside ECDH_REPLY and the content of the base64 in known_hosts: **one encoding runs through handshake, verification, and storage**. Companions: `hostkey_ed25519` (the verification trio), `key_text` (text-form conversion), `private_key`/`private_key_pem` (private-key loading — OpenSSH/PEM formats).

### Second complete program: known_hosts three-state query

The second program is from `examples/known_host_db` — the heart of the trust judgment:

```embed path="extlibs/xssh/examples/known_host_db/main.c" title="extlibs/xssh/examples/known_host_db/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xssh/single -include xssh.h impl.c extlibs/xssh/examples/known_host_db/main.c -lws2_32 -liphlpapi
（输出 trust=MATCH 与记录行号的自检结果）
```

**What just happened.** (1) Query inputs: a hostname list (one line may record several hosts), a port, a key view — `Check.Trust` outputs the three states, `Check.Entry.LineNumber` points at the hit line (locating information for alarms/deletion). (2) This sample hits MATCH (trust=1) + a line number — the pass path's shape; CONFLICT/UNKNOWN are covered in the test vectors (the `known_host`/`known_host_hash` samples verify single-line matching and hashed-hostname matching respectively). (3) The caller's multi-state branches are the substance of Chapter 121's event handling: MATCH→Accept, NEW→policy, CHANGED→Fail+alarm (with fingerprint and line number).

### Versus TLS's certificate system (a design observation)

Chapters 79/80's X.509 and this chapter's known_hosts solve the same problem — "who does this public key belong to" — in nearly opposite shapes. **Certificates**: hierarchical trust (root→intermediate→leaf), third-party vouching, verification is chain building (PathBuild) + policy (Verify layers separated); **known_hosts**: flat trust (one record per host), self-vouching (TOFU or out-of-band), verification is single-point comparison (Check's three states). Costs and benefits differ: the certificate system is heavy to deploy (CA operations/revocation/renewal) but scales well (one trusted root, a million sites); known_hosts has zero deployment but every client maintains its own. The engineering selection is not exclusive — SSH has a certificate form too (`XSSH_KNOWN_HOST_TRUST_CERT_AUTHORITY` is a trust state reserved as the exit for certificate-authority records), and TLS intranets have pinning (Chapter 84). A reader who understands both shapes picks the right tool per scenario: open-internet HTTPS uses certificates, internal automation SSH uses known_hosts, high-sensitivity stacks both with fingerprint pinning.

## Contracts

- **Format layer allocation-free**: parameters as raw-encoding views; the public prefix parser never modified as algorithms evolve; reads via borrowing, writes go direct.
- **Signature strictness**: `algorithm || signature` double string; trailing rejected; Ed25519 fixed 32/64 bytes.
- **Fingerprints**: SHA-256 shape; for human verification and log identity — not a replacement for verification (comparison is still by key).
- **Trust-state semantics**: MATCH/NEW/CHANGED/REVOKED/CERT; CHANGED is a security event (re-key/rotation or MITM — reject by default + alarm).
- **revoked wins**: a hit on an @revoked line rejects — even if another line matches.
- **Hashed hostnames**: the `|1|salt|hash` form; matching recomputes by salt; for privacy scenarios (file leakage exposes no hostnames).
- **Port identity**: the `[host]:port` form — non-standard ports are independent identities.
- **Line locating**: query results carry line numbers — locating for alarms/deletion/audit.
- **File protocol layer**: the known_hosts layer touches no network/connections/key generation — pure files and matching.

## Pitfalls

### Pitfall 1: treating CONFLICT as UNKNOWN

Symptom: on a key change the client asks "record the new key?" — the user hits Enter out of habit, and the man in the middle completes the takeover.

Cause: UNKNOWN (first time) and CONFLICT (recorded but different) are **completely different** security levels: the former is the normal flow; the latter must be rejected by default — alarm and let a human investigate (did the server really re-key? confirm out-of-band, then clear the old record).

```c bad
if ( Check.Trust != XSSH_KNOWN_HOST_TRUST_MATCH ) {
	prompt_and_save(Host, Key);   /* conflict treated as first record: a takeover hole */
}
```

```c good
switch ( Check.Trust ) {
case XSSH_KNOWN_HOST_TRUST_MATCH:
	accept(); break;
case XSSH_KNOWN_HOST_TRUST_NEW:
	if ( confirm_tofu(fingerprint) ) { save_and_accept(); }
	break;
default:  /* CONFLICT */
	alert_key_changed(Host, fingerprint, Check.Entry.LineNumber);
	reject();   /* reject by default - only after human confirmation may the old record be cleared */
}
```

### Pitfall 2: comparing by fingerprint instead of by key

Symptom: matching on the first 8 fingerprint bytes and passing — the collision surface widens, and mismatched fingerprint encodings (base64/hex/with-or-without prefix) misjudge.

Cause: the fingerprint is a **display form**; programmatic comparison goes by key bytes (Check's key-view parameter) — exact matching. Fingerprints belong only to human interfaces and logs.

```c bad
if ( strncmp(fp_text, LineFp, 8) == 0 ) { accept(); }  /* prefix collision + encoding mismatch */
```

```c good
/* the library compares by key */
xrtSshKnownHostDbCheck(Db, Hosts, Port, KeyView, &Check);
if ( Check.Trust == XSSH_KNOWN_HOST_TRUST_MATCH ) { accept(); }
```

### Pitfall 3: known_hosts recording intranet hostnames in plaintext

Symptom: a compliance audit fails — a stolen laptop leaks the entire intranet topology.

Cause: standard known_hosts stores hostnames in plaintext; the hashed form (`known_host_hash`) exists exactly for this — imperceptible on the matching side (recomputed by salt), unreadable on the file side.

```c bad
save_line("buildserver.internal.corp,10.1.2.3 ssh-ed25519 AAAA...");
```

```c good
/* write in hashed form - a leaked file shows only salts and hashes */
save_hashed_line(Host, Key);   /* |1|salt|hash ssh-ed25519 AAAA... */
```

## Exercises

### Basic: the three-state matrix

Construct three known_hosts lines (hosts with a match/a different key/no record), and query with the same key + another key — covering the full MATCH/NEW/CHANGED combination. Acceptance criteria: the six query results match the trust-state semantics; CHANGED's line number points at the existing record.

### Advanced: a TOFU interactive client fragment

Implement `verify_host(主机, 密钥, 库)` (host, key, store): query → branch by state (MATCH silent pass / NEW print fingerprint and prompt / CHANGED alarm and reject) — wired to Chapter 121's event. Acceptance criteria: the three paths behave correctly; the fingerprint output carries the algorithm prefix (SHA256: form).

### Challenge: the key-rotation flow

Simulate a server re-keying: old store CHANGED → alarm → simulated out-of-band confirmation → delete the old line (by line number) → record the new line → reconnect MATCH. Hashed hostnames throughout. Acceptance criteria: pass after rotation; always rejected before confirmation; the file carries no plaintext hostnames at any point.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Format layer | algorithm name + raw parameter views; the public parser untouched by algorithm evolution; allocation-free |
| Ed25519 | public key 32B / signature 64B (string-wrapped); the read/write/verify trio |
| Fingerprint | SHA-256 display form; human checks and logs — programs compare by key |
| Trust states | MATCH passes / NEW prompts (TOFU) / CHANGED alarms and rejects / REVOKED rejects outright |
| revoked | a hit on an @ line rejects — it outranks matches |
| Hashed hostnames | \|1\|salt\|hash; matching recomputes by salt; protects topology from file leaks |
| Port identity | [host]:port — non-standard ports are independent identities |
| Line locating | results carry LineNumber — locating for alarms/deletion/audit |
| Interface with Chapter 118 | VERIFY_HOST_KEY event + this chapter's store query = the complete trust judgment |
