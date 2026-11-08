# Redis TLS client authentication

Select `xredis_tls_auth` to use `xredisClientTlsAuthDialV1`,
`xredisClientTlsAuthDialAddressesV1` or `xredisClientTlsAuthConnectV1` with the
existing `xredisclienttlsconfig` and a native `xtlsclientauthconfigv1`.
The ordinary TLS configuration ABI is unchanged. Signing backends are explicit
separate modules; no private signing implementation is pulled into this module.

The native session retains the immutable client identity and snapshots its
limits before asynchronous DNS/TCP work. Caller config/name/ALPN memory can be
recycled after successful admission, and caller identity/context/verifier
references can be released. Resolver and callback code/context follow the
existing TLS/close/engine-join contract. Signing runs synchronously on the owning
connection worker and must not block it; final identity-context cleanup follows
the last-reference thread contract.

`xrtTlsClientAuthConfigInitV1` defaults to requiring a compatible nonempty
certificate request. Explicitly setting `RequireCertificate=false` permits
handshakes without local proof. A server's certificate trust decision is
separate: local TLS 1.3 READY/queued proof alone does not publish successful
managed Redis readiness. The configured Redis initialization must succeed.
The transport never falls back to cleartext or replays uncertain operations.

Only initial full TLS 1.2/1.3 certificate handshakes are available here. Native
auth rejects explicit Resume/ResumeOnly and disables ticket retention until
credentials are bound to the local identity. Native chain/transcript limits and
the existing TLS/Redis queue/deadline/close limits all apply. See
[native auth](../../../docs/development/tls-client-auth.md) and
[TLS transport](tls.md).

This module shares the existing TLS stream and request data path. The cold dial
factory forwards a nullable auth pointer and chooses the native factory once;
no new per-command dispatch, auth-config allocation or descriptor copy is added.
Native auth metadata, signing/crypto, references and optional TLS 1.2 raw tape
remain real costs. Production performance and native long-run acceptance remain
pending.

## Examples and verification

```text
python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_tls_auth_tests
python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_tls_auth_examples
```

The [RSA example](../examples/tls_auth/main.c) accepts a host, port, explicit
server trust PEM, client certificate-chain PEM, matching unencrypted RSA key
PEM and optional password-environment-variable name. File and aggregate DER
limits apply; key/DER scratch is wiped before release. The native identity
deep-copies those inputs. With no arguments it prints usage without contacting
a service. After admission it immediately drops its own identity/verifier
references, then requires successful managed initialization and PONG.

The disposable real Redis runner sets `tls-auth-clients yes`, uses separate
server/client CAs and checks RSA/P-256/P-384/Ed25519, both versions, no-password/
password/ACL setup, RESP2/RESP3/auto, all three factories and binary pipeline
traffic. Missing/untrusted/corrupt client proofs and a memory error reported by
the signer cannot publish successful managed readiness or send queued business.
Wrong server identity/trust invokes no private signing call. Every connection
checks one signing attempt, and all external identity contexts retire. Passwords
are fixed public fixture values; temporary private keys are never reported.

```text
python extlibs/xredis/tests/interop/run_redis_tls_auth.py --build --report out/redis-mtls.json
```

Requires local `redis-server` with TLS and `openssl`. A Windows client can be
built with `--build --build-only --report out/client-build.json`, then exercised
against the local WSL Redis from the same runner using `--client .../client.exe`.
The runner translates fixture file paths for the native executable and verifies
its exact build receipt, core/extension private and public header inputs,
helpers and binary hash before and after the run. Prebuilt clients require that
receipt. Reports retain the actual client's compiler and invalidate any old
success before starting a new run. For native Windows clients, the pinned
Windows Python from the build receipt independently verifies TLS/PING through
the Windows loopback endpoint before launching the C case; a WSL-only startup
probe cannot prove that forwarding endpoint is ready. A failed C case is never
retried by this startup check. This validates a native Windows client with a
WSL peer, not native Linux acceptance.

## Measurement boundary

`xredis_tls_auth` has a separate size profile forbidding private signing
constructors, process launchers and unrelated HTTP/WebSocket/task APIs.
Applications explicitly select a signing backend in their core build. Existing
plain/Dial/TLS profiles remain separately measurable.

```text
python tools/measure_size.py --config extlibs/xredis/config/size_profiles.json --profiles xredis_tls_auth --kind static
python tools/measure_performance.py --config extlibs/xredis/config/performance_profiles.json --profiles xredis_tls_auth_cold --smoke
```

The cold benchmark's measured process is C. A separate Python/OpenSSL fixture
uses `cryptography` to issue a temporary leaf from the public example CA,
then requires and verifies that client certificate before responding to
HELLO/PING. No production library depends on Python/process APIs.
The benchmark reuses identity/trust/context/engine, performs a new full TLS 1.3
handshake for each sequential cycle, checks PONG and waits for physical client
close. Key import, fixture startup and engine startup are outside the cycle;
peer work and first-cycle OS/crypto effects are inside. It does not retain
resumption tickets. Workload code, peer and fixed certificate source are all
explicit fingerprint inputs. The fixed test CA key is public; only this fixture
shares the temporary leaf/key between roles. Neither is a deployment credential.

The reported cycle quantiles describe this sequential connection workload.
Two-cycle smoke values only verify execution/accounting. Pinned matched
steady-request throughput/tail-latency, comprehensive allocation/copy/call/
synchronization attribution and native Windows/Linux 72-hour mixed/fault
acceptance remain required by the overall development contract.

[Redis TLS documentation](https://redis.io/docs/latest/management/security/encryption/)
describes its client certificate requirement. Native details and costs remain
in the linked core contract.
