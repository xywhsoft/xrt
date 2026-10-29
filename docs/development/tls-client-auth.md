# Native TLS client certificate authentication

The Redis mutual-TLS prototype reproduced a concrete core gap: the existing
client could only send an empty certificate chain. An OpenSSL server requiring
a client certificate rejected both TLS versions. This optional core module
supplies local certificate selection and actual CertificateVerify signing for
full TLS 1.2 and TLS 1.3 handshakes. Redis integration is a separate feature.

## Contract

Select `tls_client_auth` and a concrete signing backend, such as
`tls_identity_rsa`, `tls_identity_p256`, `tls_identity_p384` or
`tls_identity_ed25519`. Record ciphers, key exchange and schedule backends remain
explicit independent selections. A custom `xtlsidentity` can use the existing
synchronous signing interface; it must obey that interface's concurrency and
failure-atomic output contract. It must not reenter or destroy the active
session. An opaque signer is not an ownership-graph leaf.

Initialize `xtlsclientauthconfigv1` with `xrtTlsClientAuthConfigInitV1`, set its
`Identity`, and call `xrtTlsClientCreateAuthV1`. Creation snapshots scalar limits
and retains the immutable identity. It does not copy its certificate DER again.
The caller can immediately release its identity, context and verifier references
after successful creation. The existing `xtlsclientconfig` ABI is unchanged.

The default `RequireCertificate=true` fails if the server does not request a
compatible certificate. With false, the session can complete without sending
an identity, including an empty Certificate when a request has no match. The
peer still decides whether to accept the supplied chain. The query
`xrtTlsClientCertificateQueuedV1` only reports successful local queueing of the
nonempty Certificate/proof flight. It does not prove transport delivery or the
server's authentication decision; TLS 1.3 local READY precedes that decision.

This first authentication contract requires a full certificate handshake:
explicit `Resume` and `ResumeOnly` are rejected, and ticket retention is
disabled. Resumed credentials need a local-identity binding before this path
can safely enable them. Post-handshake authentication is not advertised.

## Selection and limits

Creation verifies the leaf permits digital signatures and client authentication
when KeyUsage/ExtendedKeyUsage are present. It parses each certificate and
records issuer names and certificate signature schemes. Only a terminal
certificate whose self-signature actually verifies gets the self-signed trust
anchor exception. Creation does not decide whether the remote server trusts
the chain or whether its validity dates satisfy that server's policy.

Selection respects local signing preference, the requested signature algorithms,
TLS 1.2 certificate types, certificate signature algorithms, issuer-name hints,
and the TLS 1.3 KeyUsage/ExtendedKeyUsage OID filter rules. Distinguished names
use the existing semantic X.509 name comparison. Unknown OID filters are
ignored; malformed DER, duplicate OIDs and anyExtendedKeyUsage in a requested
EKU filter fail. The two certificate-signature lists have the RFC-defined
fallback relationship. Initial TLS 1.3 request context must be empty, and its
leaf certificate must be X.509 v3.

| Resource | Default | Hard maximum |
| --- | ---: | ---: |
| Certificates in the configured chain | 16 | 256 |
| Aggregate certificate DER bytes | 64 KiB | 16 MiB |
| TLS 1.2 raw signing transcript bytes | 1 MiB | 16 MiB |
| Authority hints in one request | bounded by peer framing | 256 |
| OID filters in one request | bounded by peer framing | 64 |
| EKU values in one filter | bounded by peer framing | 128 |

Context handshake and send limits also apply. The whole local authentication
flight must fit the send limit. Plaintext/protected records split at 16 KiB;
large certificate messages can cross records without changing message framing.

## Ordering, backpressure and failure

TLS 1.2 captures raw handshake messages because the existing signer receives
the full to-be-signed content. This supports different PRF/signature hashes and
pure Ed25519. EMS is derived through ClientKeyExchange, before CertificateVerify;
client Finished includes the proof. The raw transcript is bounded, allocated
only after TLS 1.2 negotiation, and cleared after the client flight is queued.

TLS 1.3 verifies server Finished before invoking the local signer. Its proof
uses the client context string and the transcript through the local Certificate.
Application traffic secrets bind server Finished; client Finished and the
resumption master calculation also include the local Certificate/proof.

The flight is admitted using the key's maximum signature encoding before any
private-key operation. Output backpressure pauses before consuming the trigger
message. Local keys/transcripts and the whole wire flight commit only after a
single successful output ownership transfer. An error after signing is terminal;
it cannot retry the private-key operation. All temporary private material and
plaintext/wire flight allocations are cleared on cleanup. New signer errors
remain failures even if a signer returns success; an older ambient thread error
is isolated and restored on successful selection/signing.

## Reproducible independent peer

```text
python tools/test_tls_client_auth.py --report out/tls-client-auth.json
python tools/build.py --suite tls_client_auth_tests --cflag=-O2
```

The Python runner generates short-lived CA/server/client keys only in temporary
directories and uses Python ssl/OpenSSL as an independent verifier. Its full
matrix includes RSA/P-256/P-384/Ed25519, AES-128-GCM/AES-256-GCM/ChaCha20-Poly1305,
both versions, a certificate exceeding 16 KiB, absent certificate requests and
TLS 1.3 HelloRetryRequest. Success checks actual peer certificate DER, one
nonnull signature call, binary application data, ALPN and close_notify. Reports
pin source/header/helper/manifest and compiler/library hashes. WSL runs are
development checks, not native Linux production acceptance.

## Costs and remaining acceptance

The optional feature adds one private state pointer only to closures selecting
it, including sessions using the old factory in such a closure. It allocates
one state/entry/metadata block for authenticated sessions, retains one identity reference,
parses the chain and performs bounded name/OID/signature-list scans. TLS 1.2
adds geometrically grown raw transcript storage and copies. The TLS 1.3 path
uses its existing transcript hashes and adds no raw transcript buffer. Both
versions build bounded plaintext and a whole wire flight; native output queue
metadata and crypto/signature backend allocations still apply. Large flights
require one AEAD operation per record. Authentication introduces no application
message virtual dispatch, new runtime or worker lock. These are actual structural
costs, not a zero-overhead or pinned throughput/tail-latency result.

Windows/WSL x64 layout probes measure public client config at 88 bytes in all
cases. `tls_client_resume` role state is 728 without / 736 with authentication
selected; production `xredis_tls` is 1240 / 1248. The auth state is 448 bytes,
each certificate entry 32 and metadata 24, giving a 504-byte single-leaf block.
An already-selected auth/test closure with a warmed RSA identity measures two
logical XRT allocations / 2908 live bytes for the old factory and three / 3380
for the auth factory. The net 472-byte difference includes the added 504-byte
block and removal of four eight-byte native ticket slots. Native allocation
sizes are 2657 / 2625 respectively. Destruction adds no allocation and restores
the tracked live count/bytes in this probe. Setup, initial cold OS resources,
handshake signing and allocator overhead outside XRT's logical counters are
outside this particular window. WSL's probe also uses ASan/UBSan.

When authentication is compiled in but unused, cold handshake/cleanup paths
still take conditional checks and calls into the auth transcript/selection
helpers. State-layout growth, these calls, metadata scans, native in-place
encoding checks/calls, raw TLS 1.2 copies, identity reference bookkeeping and
backend temporaries are real costs. No application-message dispatch was added.

## Verified slice (2026-10-29)

- Windows GCC 16.2 O2 and WSL GCC 15.2 O2 ASan/UBSan each pass 13 modular and
  eight single-header programs in the combined client/server/auth suite.
- Each platform's independently rebuilt peer library passes 34 local
  authentication cases and 60 existing-role/empty-certificate regressions.
  Windows uses Python/OpenSSL 3.0.16, WSL OpenSSL 3.5.5. The final common input
  fingerprint is `893cf6c40f236b4875ef3c6071b613a396eee60a8a57dc7752699b090d1c9dd3`.
- The auth tests sweep 96 factory, 128 TLS 1.3 flight and 160 TLS 1.2 flight
  allocation prefixes. Hit failures roll back allocations, transcript and
  record epochs; prefixes beyond a flight's allocations also verify success.
  Other cases check actual RSA proof verification, EMS/Finished ordering,
  sign-once backpressure, checked callback errors, stale diagnostics, bad server
  Finished, retained identity lifetime, opaque ownership and CA/OID selection.
- The three new APIs, one type and eight constants have complete symbol
  documentation. The detailed TLS checker retains exactly the same 77 older
  missing-section/type issues as HEAD; it is not reported as green.
- Windows and WSL static/shared production auth packages pass their stock
  consumer and a separate consumer linking/exercising all three new APIs.
  The factory example passes Windows O2 and WSL O2 ASan/UBSan. Both existing
  Redis TLS modular programs also pass on both platforms (WSL ASan/UBSan).
  Feature trim and full generated-header consistency pass. Core plus the four
  service declaration headers compile together and preserve the caller's
  authentication feature selection.

The factory/ownership example is deliberately a ClientHello-only tour;
`tools/test_tls_client_auth.py --regression --report ...` exercises full real
certificate authentication. The example's shared embedded identity helper
requires `tls_identity_builtin`; this does not expand the production auth module.
All extension declaration headers are regenerated for the new feature restore
guard. Some also reconcile an older missing `NET_FILE_FUTURE` restore guard;
these are generator outputs, not hand-edited implementations.

Identity-bound resumption, Redis mutual-TLS integration, wider adversarial and
platform validation, matched performance and native 72-hour mixed/fault runs
remain part of the larger task. No production-complete claim follows from this
handshake feature alone.

Standards: [RFC 5246](https://www.rfc-editor.org/rfc/rfc5246.html#section-7.4.8),
[RFC 8446](https://www.rfc-editor.org/rfc/rfc8446.html#section-4.4.3).
