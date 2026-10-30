# HTTP TLS server

Select `XRPC_MODULE_XRPC_HTTP_SERVER_TLS` and create with
`xrpcHttpServerTlsCreate`. The ordinary Start/Local/Info/Close/Wait/Destroy API
then applies. Construction copies TLS metadata and retains native context and
identity objects; no listener is opened before Start. Core supplies cryptography,
certificate selection, resumption and TLS stream ownership. There is no separate
network runtime or cleartext fallback. Mutual client and HTTP application
authentication remain separate work.

The TCP listener enforces connection/private-buffer and concurrent handshake
limits before allocating a native TLS session. Default MaxHandshakes is 128,
independent of the HTTP connection bound. Native stream handshake time starts at
TCP admission; zero explicitly disables it. Readiness rechecks the absolute
deadline, including native selector/signing work. HTTP idle waiting starts at
readiness and request time at the first plaintext input. Opaque owners carry real
references; Close/Wait observes native session and callback-data retirement.

HTTP/1.1 ALPN is the default; missing ALPN is allowed unless RequireProtocol is
configured. Other protocol offers are rejected before construction. Absolute
request targets use https; plaintext server targets keep http. Protocol parsing,
notification/batch semantics, request/result quotas and the separate bounded
business executor are shared with the ordinary HTTP server.

MaxQueuedBytes reserves native application-record wire bounds before plaintext
acceptance. Short native sends retain conservative credit until complete drain;
closed sessions refund only after native session/transport ownership is destroyed.
Handshake/control bytes, native buffers and allocator metadata remain separately
bounded by native context/stream policies and connection/handshake counts. This is
not total process RSS or an end-to-end zero-copy claim: TLS copies plaintext into
authenticated record storage. Network callbacks perform no business work.

HandshakeError borrows its immutable cause for the call, runs on the peer worker
without the server gate, and must not block. It may inspect/Close the server;
Wait/Destroy from network workers is rejected. Native selectors/resume callbacks
and application context remain borrowed through Destroy and must support concurrent
workers. Configure runtime identities and trust; fixture identities used by tests
and benchmarks are public historical data, not production credentials.

The accepted TCP WriteLimit must cover the selected native context's SendLimit,
as required by Core TLS. An incompatible native policy fails the handshake with
its original cause. MaxQueuedBytes is a separate application-record budget; it
can be smaller and is validated against one minimum record at TLS readiness.

The [example](../examples/http_server_tls/main.c) loads one DER leaf certificate
and a native-supported private key at runtime; it supports rsa, p256, p384 and
ed25519, including PKCS#8. It wipes its key buffer, releases caller identity
references after construction, prints the bound endpoint and runs for a bounded
duration. Its final stats require physical Close/Wait before Destroy. It does
not install trust, authentication or certificate renewal for a deployment.

For a local P-256 demonstration, use OpenSSL and curl from a shell with those
tools available. Keep the temporary key directory outside version control:

```sh
openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes -days 1 -subj /CN=localhost -addext subjectAltName=DNS:localhost -keyout key.pem -out cert.pem
openssl x509 -in cert.pem -outform DER -out cert.der
openssl pkcs8 -topk8 -nocrypt -in key.pem -outform DER -out key.der
python tools/build.py --compiler gcc --arch native --manifest extlibs/xrpc/config/modules.json --suite xrpc --no-single --cflag=-O2
out/gcc/native/xrpc/examples_http_server_tls_main cert.der key.der 8443 60000 p256
```

Use `--arch x64` and `out/gcc/x64/...` on Linux; Windows adds `.exe`.
In a second shell, the certificate is explicitly trusted for this local request:

```sh
curl --cacert cert.pem -H 'Content-Type: application/json' --data '{"jsonrpc":"2.0","id":1,"method":"echo","params":[7]}' https://localhost:8443/rpc
```

Expected JSON is `{"jsonrpc":"2.0","id":1,"result":[7]}`. After the duration,
the example exits 0 with zero connections, queued bytes and active handshakes.
Remove the local key/certificate files after the example exits. The independent
peer generates and removes its own temporary keys:

```sh
python extlibs/xrpc/tests/interop/http_server_tls.py
python tools/build.py --compiler gcc --manifest extlibs/xrpc/config/modules.json --suite xrpc_http_server_tls_tests --no-single --cflag=-O2
python tools/measure_performance.py --config extlibs/xrpc/config/performance_profiles.json --profiles xrpc_http_server_tls --smoke
```

The dedicated C suite exercises shared wire credit, short sends, policy snapshots,
pre-session admission, stalled/plaintext handshakes, trust/ALPN rejection, idle and
partial-request deadlines, interim replies, chunk/trailer/pipeline framing,
noncooperative shutdown, constructor quotas and actual peer-worker allocation
quotas. Quota thresholds observe failures/success and zero remaining charged
owners after full join; this is not exhaustive allocation-site fault coverage.
The separate continue-credit probe compiles the real production source and
injects real credit return at gate release; the pinned predecessor must miss the
wakeup and current code must post it. No production test hook is added.

Final native Linux, broader fault/performance and 72-hour qualification
remain required. The full four-product contract stays in force.
