# Verified TLS transport

`XREDIS_MODULE_XREDIS_TLS` adds encrypted numeric, hostname and prepared-endpoint
connections to the same bounded client and managed RESP2/3 initialization. TLS
failure never falls back to plaintext or replays business commands. The whole
product remains developing; this transport does not complete reconnect, pools,
dedicated Redis modes, HA or production/performance acceptance.

## Policy and borrowed input

Initialize `xredisclienttlsconfig`, provide a shared XRT Verifier and call
`TlsDial`, `TlsDialAddresses` or `TlsConnect`. Native client policy supports
verified TLS 1.2/1.3 with the compiled AES-GCM/ChaCha, SHA-256/SHA-384 and
X25519/P-256/P-384 backends. Applications can further restrict the immutable
XRT Context. No insecure verifier is synthesized. The native explicit
Resume/ResumeOnly contract remains available; certificate policy and its
callback lifetime follow the supplied Verifier, not an invented trust flag.

Names, opaque binary ALPN IDs and ALPN descriptors are borrowed only during the
factory. Before returning, the native TLS session owns copied views and shared
Context/Verifier/Resume references. User callback code/context must satisfy
XRT's retained shared-object lifetime, including Engine join after client Closed.
Destroying the caller's reference does not destroy a still-referenced verifier.
No native session/stream pointer is exposed for concurrent direct IO.

Defaults infer DNS SNI and verification identity from Host when SNI is absent.
IP literals infer VerifyName without sending IP SNI. An explicit VerifyName can
override SNI identity, following XRT. Prepared endpoints and numeric convenience
connections require an explicit ServerName or VerifyName; do not silently use
an unrelated endpoint as a certificate identity. Host copying/list retention
and Resolver lifetime follow the [Dial contract](dial.md).

MaxMetadataBytes defaults to 64 KiB and caps descriptor/name/ALPN input bytes,
with a maximum 1 MiB and 256 ALPN descriptors. Native TLS additionally enforces
wire name/ALPN lengths, resume bounds, cryptographic policy and queue limits.
The metadata cap is not a cap on the entire TLS session. WriteBytes defaults
to 64 KiB per client turn (maximum 1 MiB); WorkRequests also bounds TLS send
calls/cancelled pending work in that turn. Stream handshake/close timeouts are
nonnegative; zero explicitly disables them. Dial.Timeout covers DNS/TCP only.
Init.Timeout starts at public API entry and spans metadata/native-session
creation, worker queue, DNS, TCP, TLS and all Redis setup steps.

The TLS descriptor is required. Invalid arguments, aliasing outputs, metadata
overflow or native session creation failure leave Result unchanged. Invalid
trust configuration fails before dialing. Async handshake/peer verification
failure reports DISCONNECTED readiness with its owned native Cause; handshake
and overall deadline failures report TIMEOUT. Server authentication/setup
errors keep the existing redacted Redis diagnostics.

## Short writes and cancellation

TLS Send accepts a prefix and can fail after accepting plaintext into encrypted
record storage. The client stores the exact accepted prefix and resumes only
the remainder. A zero-byte AGAIN restores the original pending FIFO position;
it remains NOT_SENT. Any accepted prefix creates a sent FIFO obligation and
MAY_HAVE_EXECUTED uncertainty, even if a later ciphertext queue allocation fails
before TCP transmits. An early ordinary reply for a partially encoded command
is a protocol error. Setup commands use the same exact-prefix discipline.

Cancelling/timing out a partially sent command resolves the caller promptly
with sent uncertainty, then finishes that command under the existing drain
grace so subsequent replies retain their FIFO correlation. A late reply drains
the tombstone exactly once. Drain failure retires the connection; no prefix or
command is replayed. Close stops admission and aborts outstanding transport
duties. This Close is an abort barrier; it does not promise a graceful TLS
close_notify exchange. Authenticated peer End and unauthenticated/truncated EOF
remain distinguished by native TLS, with bounded Redis EOF parsing.

Plaintext uses the TLS read-only block view and TLS Consume API. The client never
mutates TLS's buffer through raw NetBufConsume, Pause or Resume. ReadBytes and
ReadFrames retain fair, bounded parsing and resume through queued client work.
The native TLS Close callback marks physical transport retirement. Client Closed
also waits its pending Dial callback, requests, setup wire, timers and internal
posts. Join the Engine before unloading callback code; separately join the shared
Resolver. Retained ready/reply results can outlive these objects.

## Actual costs and compile boundary

TLS session creation happens once during the factory so borrowed configuration
cannot escape. It performs native view copies, shared references, cryptographic
setup and session allocations even if a later client allocation fails. One
cold Dial context owns that session until successful attachment, then the TLS
Stream owns it. Only existing XRT NetDial, TLS session and TLS Stream are used;
there is no extra TLS-Dial/Future stack or shared protocol runtime.

Metadata validation scans bounded names for NUL. Automatic identity inference
uses the public numeric-address parser and restores the caller's ambient error;
a nonnumeric hostname can create and release a temporary parse diagnostic.
That is an additional cold scan/allocation cost, outside the warmed ECHO window.

Selecting TLS adds private client cursor/state and conditional transport branches.
The plain numeric/Dial leaves compile these out and continue to forbid TLS
symbols. TLS writes encrypt accepted plaintext into native record storage;
those copies/allocations, callbacks, cipher/TCP queue metadata, references and
parse/Consume calls are real costs. The owned initialization wire is wiped at
its final reference. This does not claim erasure of caller environment data,
all native scratch/peer buffers or retained response payloads. No zero-overhead
or production tail/throughput claim follows from this integration.

Resumption credential input follows XRT; ticket harvesting/cache management,
mutual client-certificate authentication, more certificate/algorithm/OS matrices
and final pinned performance/native long-run acceptance remain follow-up work.
The implemented transport's scope is server-authenticated TLS plus Redis setup.

## Reproduction and measurement

```sh
python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_tls_tests --no-examples --cflag=-O2
python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_tests --cflag=-O2
python tools/measure_performance.py --config extlibs/xredis/config/performance_profiles.json --profiles xredis_tls_openloop,xredis_tls_openloop_stats,xredis_tls_openloop_stall,xredis_tls_openloop_hold --smoke --repeats 1 --report out/service-extensions/redis-tls-openloop.json
```

The dedicated test suite adds a fixture RSA signer only to test dependencies.
Production TLS does not depend on the fixture signer. The historical public
identity/custom fixture trust verifies handshake signatures and controlled
failures; it is not proof of default CA/hostname validation. Cases include
binary ALPN/name snapshot lifetime, host/numeric/refused-endpoint fallback,
3-byte setup/business write budgets, 32-call binary pipeline, certificate
rejection, handshake/overall deadlines, close during blocked DNS, 1-byte writes
interrupted by cancellation/timeout/close, constructor quota thresholds, worker
allocation failure and retained results after teardown.

The TLS benchmark wraps the existing absolute-arrival ECHO model, preserving
all offer/outcome/lag/deadline, observer-retirement and successful/all-latency
accounting. It uses TLS 1.3 AES-128-GCM/X25519 and explicit custom fixture trust,
with a separate instrumented process-window profile and controlled stall/held
replies. Handshake/warmup precede the measured window; fixture/crypto teardown
is included in CPU/allocation counters. Peak RSS and handle/descriptor samples
are whole-process observations. TLS fixture/native test helper bytes are explicit
benchmark fingerprint inputs. Local TLS workloads reject external ports without
trust configuration; they never reinterpret them as plaintext. These smokes
exercise methodology and are not a pinned performance baseline or client parity.

The [client-only example](../examples/tls/main.c) takes HOST PORT CA.pem and an
optional password environment-variable name. It uses the default bounded native
chain/name verifier, reports typed success, closes the client and joins services.
No arguments print usage and contact no service; that usage check alone is not
TLS interoperability evidence. Compile it against independent packages and run
the opt-in disposable real-service supervisor (Linux paths):

```sh
python tools/package.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis --kind static --verify
gcc -std=c11 -Wall -Wextra -Werror -O2 -DINTEROP_TLS -pthread -Iinclude -Iextlibs/xredis/include extlibs/xredis/tests/interop/redis_init.c release/gnu/native/xredis/static/libxredis.a release/gnu/native/xredis/static/libxrt.a -lm -o out/service-extensions/redis-tls-interop
gcc -std=c11 -Wall -Wextra -Werror -O2 -DXREDIS_MODULE_XREDIS_TLS -pthread -Iinclude -Iextlibs/xredis/include extlibs/xredis/examples/tls/main.c release/gnu/native/xredis/static/libxredis.a release/gnu/native/xredis/static/libxrt.a -lm -o out/service-extensions/redis-tls-example
python extlibs/xredis/tests/interop/run_redis_tls.py --client out/service-extensions/redis-tls-interop --example out/service-extensions/redis-tls-example --report out/service-extensions/evidence/redis-tls-interop.json
```

The supervisor requires installed TLS-enabled Redis and OpenSSL executables.
It generates ephemeral CA/server identities and a distinct wrong CA, binds new
loopback-only instances with plaintext/persistence disabled, forces TLS 1.2 then
1.3 and tests no-auth/password/ACL cases. Default native validation must reject
wrong name and wrong trust before business data; the real client-only example
also runs with both TLS versions. All owned processes/temporary key files are
removed. Reports pin binaries, public certificate hashes, sources/helpers and
expanded arguments. No private key bytes are recorded or shipped in production.
WSL evidence remains distinct from native Linux/Windows production acceptance.

Current stage evidence and unpassed modes are recorded in the
[development ledger](../../../docs/development/service-extensions-progress.md).
