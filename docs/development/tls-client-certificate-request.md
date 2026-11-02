# TLS client CertificateRequest without a local identity

This is a bounded core prerequisite for xredis mutual TLS, not delivery of
client-certificate signing or any of the four production extensions.

## Reproduced gap and standards

Before this change, an OpenSSL server issuing CertificateRequest caused XRT
TLS 1.2 to fail with `expected an empty ServerHelloDone`, and TLS 1.3 to fail
with `expected Certificate after EncryptedExtensions`. This also prevented
connections to servers that request an optional client certificate.

[RFC 5246 sections 7.4.6-7.4.9](https://www.rfc-editor.org/rfc/rfc5246.html#section-7.4.6)
and [RFC 8446 sections 4.3.2 and 4.4.2](https://www.rfc-editor.org/rfc/rfc8446.html#section-4.3.2)
require an empty Certificate when the client has no suitable certificate.
The server decides whether to allow the connection. No CertificateVerify is
sent for the empty chain.

## Implemented boundary

- The existing `xrtTlsClientCreate` API/configuration layout stays unchanged.
  There is no configured local identity and no new signer or runtime dependency.
- TLS 1.2 accepts one correctly framed request after ServerKeyExchange and
  before ServerHelloDone. Its response contains an empty Certificate followed by
  ClientKeyExchange in one plaintext record, then the existing CCS/Finished.
  EMS includes the certificate and key exchange in the specified order.
- TLS 1.3 accepts one initial request immediately after EncryptedExtensions.
  Its context must be empty. The required signature list and present authority
  lists are validated by the existing codec. Duplicate extensions and known
  extensions confined to other messages fail; unsupported opaque extensions are
  ignored. Client selection/filtering is unnecessary when no chain is sent.
- Only after authenticating the server does TLS 1.3 send an empty Certificate
  and Finished together under the handshake write epoch. Application secrets
  still cover server Finished; client Finished and resumption master include
  the empty Certificate. No post-handshake authentication is advertised or
  accepted. A PSK handshake cannot enter the certificate-request state.
- Rejections send exact fatal alerts: missing mandatory signature algorithms
  uses `missing_extension`, a nonempty initial context or a known extension in
  the wrong message uses `illegal_parameter`, malformed encoding uses
  `decode_error`, and repeated/early/late messages use `unexpected_message`.
  These bytes are checked after decrypting the TLS 1.3 alert on the peer.
- Send-size preflight includes the whole response. Backpressure pauses before
  consuming the final triggering handshake; retry cannot duplicate a certificate,
  transcript update or epoch switch. TLS 1.2 allocation failure leaves the
  response transcript and both record epochs uncommitted.
- A required-certificate peer still rejects this client. TLS 1.3 local READY
  precedes that peer decision; consumers must process subsequent fatal alerts.

## Costs and acceptance

The only new role state is a boolean placed in existing alignment padding.
Windows x64 and WSL x64 GCC layout probes compare the same selected closures:
`tls_client_resume` stays 728 bytes and production `xredis_tls` stays 1240 bytes;
the public client configuration remains 88 bytes. An initial eight-byte growth
was measured and removed before the final validation. These are selected ABI
measurements, not a claim about every architecture. Existing public layouts
and unselected dependency closures stay unchanged. Requested TLS 1.2 adds seven
handshake bytes and TLS 1.3 adds eight; each reuses an existing response record,
with the existing protection and queue submission. No second record or dedicated
allocation call is introduced. Actual native queue block allocation and size-class
rounding can change when these bytes need additional capacity; these costs are
not declared zero. The existing reader can still grow within its HandshakeLimit,
and native queue/crypto allocations remain real. The codec and role policy each
scan the request extension list. Precise alerts use a private failure entry
without changing public diagnostics/layout. The default failure wrapper now
performs one borrowed error-slot read before transferring the same primary
error; that extra cold-path operation is recorded, not measured away.
The extra certificate bytes enter the transcript; TLS 1.3 also takes a transcript
digest before calculating client Finished. A client without a request takes a
conditional branch in cold handshake preparation. There is no new application
message dispatch, lock or signer lookup. This is structural accounting, not
an independently measured zero-overhead or production-performance claim.

Modular tests cover TLS 1.3 request fragmentation across encrypted records,
unknown schemes/extensions, malformed/duplicate/out-of-order requests, exact
certificate/Finished bytes, application-secret and resumption-master derivation,
and atomic send backpressure. TLS 1.2 uses a transcript-assisted native peer:
the fixture inserts a request and checks/removes only the empty certificate;
both peers still verify actual Finished and exchange application data. It also
covers byte fragmentation, unsupported schemes/types, malformed and early
requests, duplicate requests, backpressure and output OOM. The same cases run
as separate single-header programs. The native server itself does not gain
client authentication in this change.

Run the independent in-memory Python ssl/OpenSSL peer with ephemeral certificates:

```text
python tools/test_tls_client_certificate_request.py --report out/tls-request.json
```

Prerequisites are a C compiler and Python `cryptography`. The test loads a
fresh local CA into the default native chain/name verifier; it does not replace
trust with a test-accept callback. It checks TLS 1.2/1.3, AES-128-GCM,
AES-256-GCM/SHA-384 and ChaCha20-Poly1305, optional-certificate success with
binary data/ALPN/close, required-certificate rejection and actual fatal alert
reception, plus TLS 1.3 HRR. Private keys stay in a temporary directory and are
removed on exit; reports contain public certificate hashes and source identities.
Windows and WSL results are local interoperability/sanitizer evidence; native
Linux acceptance, pinned measurements, signing identity integration and the
full long-running extension gates remain required.
