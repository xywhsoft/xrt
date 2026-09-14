---
num: 81
slug: cert-chain
title: Trust Chains and Revocation: From Single Certificates to PKI
volume: 卷八 安全
type: practice
lead: Certification paths, trust anchors, and CRLs — answering verification's third question, "is this issuer worth trusting", completing the last piece of PKI.
api: x509, crypto
---

## Orientation

Chapter 80 ended on "verification passed ≠ trusted" — this chapter supplies the missing third question. Three parts arrive in order: the **certification path** (the signature relay of leaf → intermediate CA → trust anchor, `xrtX509PathValidate` checking strictly link by link, `xrtX509PathBuild` automatically finding the chain from unordered candidates); the **trust store** (an owning anchor set — explicit addition, PEM files, and system roots, three loading forms via the `xrtX509StoreCreate/Add/AddPem/AddSystem` family); and the **CRL revocation check** (`xrtX509CrlValidate` verifying once + `xrtX509CrlCheck` lightly querying N times, a two-layer design). Assembled, the three form complete PKI client verification — Chapter 82's TLS client certificate checking is a direct composition of exactly these parts. This chapter also delineates what this layer does not manage: network fetching, caching, and soft-fail policy belong above — the policy APIs preserve unambiguous results so you can choose your own fetching and failure rules.

## Introduction

Back to Chapter 80's attacker: he self-signs a certificate, CN/SAN all carrying the target domain, signed with his own key — signature verification passes, identity matching passes. He lacks exactly one thing: **his public key is not in your trust anchors**. A trust anchor is a root certificate you pre-approve and store locally — operating systems and browsers preinstall a hundred-odd CA roots, and your corporate intranet may add its own. Verification's essence is: **can you start from the certificate at hand and walk "who signed for whom" all the way to some locally trusted anchor**.

Real-world chains almost always have intermediates: `网站证书 ← 中间CA ← 根证书` (site ← intermediate ← root). At TLS handshake the peer sends leaf and intermediates together (order not even guaranteed); the root is not sent — the root is on your machine. So the complete verification algorithm is: parse all certificates → automatically build the chain from the leaf to some anchor → verify signatures link by link, checking constraints link by link (does this intermediate CA really have authority to sign certificates? Is the path depth over limit? Do name constraints allow it?) → all pass means trusted. And still not enough: a certificate legal in every respect may have been **revoked by its issuer** (private key leaked, domain transferred) — the last question takes the issuer's CRL (Certificate Revocation List) to look up the serial. This chapter's three examples correspond to the three steps "build and verify the chain, load anchors, check revocation".

## Concepts

### The certification path: the signature relay and its rules

A certification path is an array of certificates with "the target first, intermediate CAs approaching the anchor in turn". **The trust anchor is verification input and belongs to no path** — `xrtX509Anchor(&根证书, &锚)` extracts the Subject, public key, and optional NameConstraints from a trusted certificate; the same anchor certificate appearing again inside a path is rejected (against "stuffing a peer-sent root into the path to impersonate an anchor").

`xrtX509PathValidate(路径, 数量, &锚, &配置)` checks link by link at the exact time given by the configuration; the rejection list includes: duplicate certificates, broken issuer chain, signature failure, unhandled critical extensions; intermediate CAs must have a non-empty Subject and a **critical** `BasicConstraints(cA=TRUE)`; if KeyUsage is present it must allow `keyCertSign`, and when `pathLenConstraint` appears, that KeyUsage must be explicitly carried; NameConstraints intersect level by level and apply to the target and all non-self-issued intermediates below it; an intermediate CA's ExtendedKeyUsage, if present, must also allow the requested purpose. **The verification time is explicitly provided by the caller** (`config.Time`) — tests are reproducible, caches decidable, replays checkable; this is the design divide from "implicitly take the current time". `X509_PATH_REQUIRE_KEY_USAGE/PURPOSE` can force the target certificate to carry explicit usage declarations.

### PathBuild: automatically finding the chain from a pile of certificates

`xrtX509PathValidate` requires the path already ordered; `xrtX509PathBuild(目标, &候选源, &配置, 输出路径, 容量, &结果)` accepts **unordered candidates** and searches automatically: at each level it first tries to terminate directly at the anchor, then filters candidates in order by Issuer/Subject and AKI/SKI (`xrtX509IssuerMatch`), deterministic depth-first with backtracking; **any complete path found is always handed to the same PathValidate for strict verification** — the builder has no lenient-verification back door. The output path never contains the anchor certificate; insufficient capacity returns `XERR_RANGE`, no path returns `XERR_NOT_FOUND`, verification failures remain in the `X509_ERROR_PATH_BUILD` cause chain. With capacity ≤16 the search allocates zero heap. This is the standard processing posture of a TLS client after receiving the peer chain.

### The trust store: the anchor's home

`xx509store` is an owning set of trust anchors, three loading forms combined per need:

- **Add one by one**: `xrtX509StoreAdd(库, DER, 长度)` copies and strictly parses — the store keeps exactly one DER per entry, with views all borrowing it; an exactly duplicate certificate returns `X509_DONE` (idempotent).
- **PEM text**: `xrtX509StoreAddPem` walks every block in the text, importing only blocks labeled `CERTIFICATE` — **transactional**: if any block fails, everything added this round rolls back.
- **Files**: `xrtX509StoreAddFile` (independently trimmable) auto-detects single-DER or multi-block PEM files.
- **System roots**: `xrtX509StoreSystem()` produces an **independent snapshot** in one call — Windows enumerates the ROOT logical store, macOS reads the keychain, Unix reads `SSL_CERT_FILE/DIR` or the mainstream bundles. The snapshot is independent of the system (later system changes never affect a built store), modifies no platform settings, and never silently accepts partial results (enumeration failure rolls back the whole). The default trust basis of an HTTPS client is exactly this.

`xrtX509StoreSource(库, 外部中间证书数组, 数量, &候选源)` combines "anchors in the store + intermediates sent by the peer" into PathBuild's input — the store is where anchors come from, handshake material is candidates, each in its place. One crucial invalidation rule: **continued writes may move the internal structure**, invalidating existing pointers and already-constructed `xx509pathsource` — finish loading before building sources, and never write after building. The store itself does not judge "should this be trusted" for you: explicit addition is precisely the granting of anchor status.

### CRL: verify once, query N times

Revocation checking splits into two layers matching two usage frequencies. The **re-verification layer** `xrtX509CrlValidate(CRL, 签发者证书, 配置, &有效视图)`: verifies the CRL signature, Issuer match, time window, issuer's `cRLSign` usage, AKI, CRL Number, unknown critical extensions — producing the borrowed `xx509crlvalid` view (the CRL and issuer must stay alive). The **light query layer** `xrtX509CrlCheck(&有效视图, 证书, &结论)`: merely looks up whether the serial is in the revocation list — verify once, query repeatedly is exactly its design scenario (one CA's CRL verified once, a whole batch of certificates queried). One-shot paths use the combined entry `xrtX509CrlStatus`.

The query's three states must be understood precisely: `X509_ERROR` input/protocol error; `X509_DONE` this CRL **does not apply** to the certificate (or a delta CRL has no new records) — not "not revoked"; only `X509_VALUE` publishes a verdict (`X509_REVOCATION_GOOD/REVOKED` etc.). One subtle trap: a **reason-partitioned CRL**'s `GOOD` only proves the reasons covered by `CoveredReasons` are unrevoked — multiple partitioned CRLs should be aggregated in turn with `xrtX509RevocationUpdate`, with a final verdict only when the full set is covered or any partition confirms revocation. Complete/delta combinations use `xrtX509CrlSetInit/Check`. **What this layer does not manage**: network fetching of CRLs, caching, soft fail (admit when the CRL can't be fetched) — these are upper-layer policy decisions; the policy APIs preserve unambiguous results precisely to leave the choice to you.

### Assembly: the shape of complete client verification

```diagram flow
- Load: StoreSystem (or explicit Add) builds the anchor store; handshake material parsed as candidates
- Build: StoreSource combines the source -> PathBuild finds the path to an anchor
- Verify: PathValidate already executed inside Build (time/constraints/signatures checked link by link)
- Identity: MatchHost matches the hostname (Chapter 80)
- Revocation: CrlValidate + CrlCheck look up the serial (or upper-layer policy decides soft fail)
```

Chapter 82 will show the TLS client wrapping these steps into context configuration — but every step's behavioral boundary is exactly what this chapter defines.

## Examples

### First complete program: automatically building the chain from unordered candidates

The following program comes from `examples/x509/path_build/main.c`, three certificates (leaf/intermediate/root) parsed, then automatically constructing and verifying a path from "a pile of candidates + one anchor":

```embed path="examples/x509/path_build/main.c" title="examples/x509/path_build/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -I . -include xrt.h impl.c examples/x509/path_build/main.c -lws2_32 -liphlpapi
certificate path contains 2 certificate(s)
```

**What just happened.** (1) The three certificates each `xrtX509Parse` into views; **only the root is turned into an anchor by `xrtX509Anchor`** — anchors come from local judgment (hand-picked here); this is where "trust" is injected. (2) The candidate source `xx509pathsource` lists root and intermediate both as Issuers, with the anchor separate — note the root is both a candidate and the anchor: Build first tries anchor termination at each level, then filters candidates in order; the two roles never blur. (3) `PathBuild` outputs `2` — the path holds the leaf and intermediate CA, **the anchor not counted**; this output path has already passed every per-link check (signatures, time explicitly via `Config.Time = Leaf.NotBefore`, BasicConstraints, KeyUsage). (4) The output buffer `Path` is caller-provided, capacity 3 doubling as the max depth — pinning capacity small when building chains from untrusted material is a defensive posture (with a large candidate pool there are depth/breadth constraints). Compare the division: the `examples/x509/path` example demonstrates the caller **ordering the chain themselves** before `PathValidate` — both entries share the same strict verifier.

### Second complete program: one-call loading of the system trust store

The second program comes from `examples/x509/store_system/main.c`, one line producing the current platform's independent anchor snapshot:

```embed path="examples/x509/store_system/main.c" title="examples/x509/store_system/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/x509/store_system/main.c -lws2_32 -liphlpapi
system anchors=33
```

**What just happened.** (1) `xrtX509StoreSystem()` returns an independent snapshot — the anchor count varies with the platform's roots (33 on this machine, likely different on yours); the snapshot uses no global cache, and later system root changes never affect a built store. (2) After counting, `StoreFree` releases — the snapshot is an owning object, the same "one allocation, returned whole" shape as Chapter 71's interface snapshot. (3) The real usage is `StoreSource` turning it into a build source: the HTTPS client's default trust basis is exactly this pair of functions. To tighten the trust surface (trust only specific CAs), switch to `StoreCreate` + `Add`/`AddFile` loading anchors explicitly — the store does not decide whom you should trust; addition is trust.

### Third complete program: the lightweight path of revocation queries

The third program comes from `examples/x509/crl_policy/main.c`, hand-constructing an already-validated view to focus on `CrlCheck`'s query semantics:

```embed path="examples/x509/crl_policy/main.c" title="examples/x509/crl_policy/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/x509/crl_policy/main.c -lws2_32 -liphlpapi
revocation state: 1
```

**What just happened.** (1) In real programs `xx509crlvalid` **should only ever be produced by `xrtX509CrlValidate`** — this example hand-assembles it to focus the query interface (the CRL view hung with revocation entries, the issuer hung with its Subject). (2) The revocation list holds serial `0x2A`; the queried certificate's Serial is exactly `0x2A` with matching Issuer — `CrlCheck` returns `X509_VALUE` and publishes `State=1` (`X509_REVOCATION_REVOKED`). (3) The serial is an **arbitrary-precision byte view** throughout (Chapter 79's original-bytes Serial semantics aligned on the CRL side — exact byte matching, no truncating conversion). The companion `examples/x509/crl` example demonstrates `CrlParse` + entry-cursor traversal: with an empty list, `Read` immediately returns DONE — "structure complete to the end" and "interrupted by error" are two different endings, and code must distinguish them.

## Contracts

- **Anchor semantics**: the anchor is verification input belonging to no path; `xrtX509Anchor` extracts Subject/public key/constraints; the anchor certificate appearing in the path itself is rejected; peer-sent certificates are forever only candidates, never anchors.
- **Verification time**: `config.Time` must be provided explicitly; verification checks every certificate at the given time — reproducible, cacheable, replay-checkable.
- **Path rules**: intermediate CAs need a non-empty Subject + critical BasicConstraints(cA=TRUE); KeyUsage/pathLen/NameConstraints/EKU checked link by link; length counts only non-self-issued intermediate CAs; NameConstraints carried by the target are rejected.
- **Builder boundary**: reads no files, system stores, networks, or AIA, never expands candidates without bound; complete paths must pass PathValidate; capacity ≤16 zero heap; `XERR_NOT_FOUND` no path, `XERR_RANGE` insufficient capacity, failure cause chain in `X509_ERROR_PATH_BUILD`.
- **Trust-store transactionality**: AddPem/AddFile/AddSystem roll back wholly on any block failure; duplicate certificates idempotent (`DONE`); one DER per entry, views borrowing the store's storage.
- **Invalidation rule**: writes may move the structure — take pointers/build sources only after loading completes; once built, read-only is multi-thread concurrent; writes and frees must not run concurrent with reads.
- **CRL layering**: Validate (heavy) produces the `xx509crlvalid` borrowed view (CRL and issuer must stay alive); Check (light) queries many; one-shot uses CrlStatus; configuration strict by default (requires nextUpdate/CRL Number/AKI/KeyUsage).
- **Revocation three states**: ERROR bad input / DONE not-applicable (not "not revoked") / VALUE alone has the verdict; a partitioned CRL's GOOD is limited to CoveredReasons, multiple aggregated via `xrtX509RevocationUpdate`.
- **Policy ownership**: network fetching, caching, soft/hard fail, OCSP composition belong above; this layer preserves unambiguous results.
- **Trimming**: store_file, store_system, crl_policy are all independent feature macros; the path layer has independent RSA/ECDSA/Ed25519 backends.

## Pitfalls

### Pitfall 1: taking the peer-sent "root certificate" as a trust anchor

Symptoms: a man-in-the-middle builds an entire "root + intermediate + leaf" chain and sends it; the client takes the chain top as anchor and verification passes — trust is entirely the peer's to define.

Cause: an anchor's status can only come from **local loading** (system store, config file, explicit Add). Every peer-sent certificate is candidate material awaiting verification — including the self-signed one that looks like a root. PathBuild's design keeps the two apart exactly: the anchor is passed separately and never enters the path.

```c bad
/* take the handshake material's last certificate directly as the anchor */
xrtX509Anchor(&PeerChain[n - 1], &Anchor);
xrtX509PathValidate(Path, n, &Anchor, &Config);
/* the peer controls the chain's contents = the peer controls trust */
```

```c good
/* anchors come only from the local store; the whole peer chain goes to Build as candidates */
xx509pathsource Source;
xrtX509StoreSource(pLocalStore, Issuers, n, &Source);
if ( !xrtX509PathBuild(&Leaf, &Source, &Config,
		Path, 4u, &Result) ) {
	reject();   /* cannot reach a local anchor: untrusted */
}
```

### Pitfall 2: treating CrlCheck's DONE as GOOD

Symptoms: the certificate may well be revoked, yet the code admits because `CrlCheck` returned non-ERROR — DONE (this CRL doesn't apply to the certificate) and GOOD (queried, not revoked) get conflated.

Cause: of the three states, DONE is "no verdict" — the CRL's issuer not matching the certificate's, or a delta CRL with no records, all return it. Treating "not found" as "no problem" makes the revocation check decorative.

```c bad
if ( xrtX509CrlCheck(&Valid, &Cert, &Status) != X509_ERROR ) {
	accept();   /* DONE slips in too: not-applicable treated as not-revoked */
}
```

```c good
switch ( xrtX509CrlCheck(&Valid, &Cert, &Status) ) {
case X509_VALUE:
	if ( Status.State == X509_REVOCATION_REVOKED ) {
		reject("revoked");
	} else {
		accept();   /* queried and not revoked — the only admission */
	}
	break;
default:
	/* DONE=not applicable / ERROR=failed: handle by policy —
	   strict policy rejects; soft-fail policy records then admits */
	handle_no_verdict();
	break;
}
```

### Pitfall 3: using old pointers/old sources after writing to the trust store

Symptoms: occasional dangling views or scrambled build results — `StoreSource` built a source, then mid-way `StoreAdd` added a certificate.

Cause: the store is array-backed internally, and writes may move the certificate and anchor structures — every existing view pointer and already-constructed `xx509pathsource` is invalidated. This is the standard trap of the "owning collection + borrowed views" combination (Chapter 18 covered the same phenomenon).

```c bad
xrtX509StoreSource(pStore, Issuers, n, &Source);
xrtX509StoreAdd(pStore, NewDer, iSize);  /* may move the internal structure */
xrtX509PathBuild(&Leaf, &Source, ...);   /* Source dangles */
```

```c good
/* loading phase: all Add/AddPem/AddFile/AddSystem complete */
load_all_anchors(pStore);
/* usage phase: take pointers, build the source, build the chain — write no more */
xrtX509StoreSource(pStore, Issuers, n, &Source);
xrtX509PathBuild(&Leaf, &Source, &Config, Path, 4u, &Result);
```

## Exercises

### Basic: a three-certificate chain, two ways

Build your own "root → intermediate → leaf" chain with OpenSSL (one each), and verify via both `PathValidate` (ordering the path yourself) and `PathBuild` (unordered candidates). Acceptance: both routes agree; deliberately shuffling candidate order, Build still finds the same path; swapping the anchor for another root not locally installed, both routes fail.

### Advanced: an HTTPS verifier with a tightened trust surface

`StoreCreate` + `AddFile` loading only your self-built root, run "build chain + MatchHost + ValidAt" complete verification on the leaf certificate above. Then compare the same leaf's verification result under a `StoreSystem` snapshot. Acceptance: under the tightened store verification passes (your root is in the store); a chain whose root is absent fails immediately; you can state the deployment scenarios suiting each store.

### Challenge: a batch revocation auditor

Given a set of certificates and the corresponding issuers' CRL files: `CrlValidate` once, loop `CrlCheck` outputting each certificate's verdict (REVOKED/GOOD/not-applicable, three classes); for "not-applicable" certificates report the reason (issuer mismatch etc.). Then wrap the query loop with timing and compare "re-validating each via CrlStatus" against "verify once, query N". Acceptance: the three classes agree with the `openssl crl` tool; the timing difference between modes explainable (verification cost amortized).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Three-question puzzle | signature (79) + identity (79) + chain to a local anchor (this chapter) + revocation (this chapter) = complete PKI verification |
| Path model | leaf → intermediate → ...; the anchor is verification input belonging to no path; the anchor appearing in the path is rejected |
| PathValidate | caller orders the chain: explicit time, per-link signature/BasicConstraints/KeyUsage/NameConstraints |
| PathBuild | unordered candidates, automatic chaining: anchor tried first, AKI/SKI filtering, backtracking; complete paths always strictly verified |
| Result semantics | `XERR_NOT_FOUND` no path / `XERR_RANGE` insufficient capacity / cause chain `X509_ERROR_PATH_BUILD` |
| Trust store | Add/AddPem/AddFile/AddSystem transactional loading; one DER per entry; explicit addition = granting trust |
| System snapshot | `StoreSystem` independent snapshot: platform enumeration failure rolls back wholly; no platform settings modified |
| Invalidation rule | build sources only after loading completes; writes invalidate pointers/sources; once built, read-only concurrent |
| CRL two layers | Validate once (signature/time/usage) → crlvalid; Check lightly queries N serials |
| Revocation three states | ERROR bad / DONE not-applicable (not GOOD) / VALUE has the verdict; partitioned CRLs use the aggregator |
| Policy ownership | fetching/caching/soft-fail belong above; this layer preserves unambiguous results for your decisions |
