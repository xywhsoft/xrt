# Immutable HTTP request headers

`xrpchttpconfig.RequestHeaders` / `RequestHeaderCount` configure a fixed-origin
client header snapshot for every call, notification and batch. Numeric, DNS,
prepared-address and verified TLS factories all seal the same snapshot before
returning. Inputs may then be overwritten/freed. Credential rotation uses an
explicitly created client epoch; no mutable cross-thread credential setter,
challenge negotiation, refresh, redirect, proxy forwarding or replay is added.
Server-side asynchronous authorization and request metadata remain pending.

At most 64 case-insensitively unique names are accepted. The native-encoded
custom block, including its final blank line, defaults to 16 KiB with a hard
1 MiB cap. A zero byte cap disables custom fields. Descriptors may be unaligned.
Names/values are checked using public Core HTTP APIs and bounded before scanning.
Invalid token names, CR/LF/NUL/other prohibited controls, descriptor/value/output
aliases, duplicate names, count/byte overflow and forbidden overrides preserve
the factory output and are rejected before network construction. This follows
[RFC 9110 field syntax](https://www.rfc-editor.org/rfc/rfc9110.html#section-5.5)
and keeps [RFC 9112 framing](https://www.rfc-editor.org/rfc/rfc9112.html#section-6.3)
under transport control. Rejecting every duplicate custom name is an explicit
local policy, including where HTTP permits combinable repeated field lines.

Transport-owned fields cannot be supplied: Host, Content-Length,
Transfer-Encoding, Content-Type, Content-Encoding, Accept-Encoding, Connection,
Keep-Alive, Proxy-Connection, Proxy-Authorization, TE, Trailer, Upgrade,
HTTP2-Settings and Expect. Other values, including Authorization and Cookie, are
opaque HTTP field values. This does not validate Basic/Bearer credentials or
establish application authentication. Use the explicit verified TLS factory
when credentials require encryption. The origin client does not send proxy
credentials to an origin server.

The client allocation contains host, target and the pre-encoded custom block;
there is no additional header heap allocation per request. Each request uses the
existing four native fields, then replaces their final CRLF with the immutable
custom block. This copies constant header bytes once per wire construction and
avoids rescanning all configured field values for each call. Extra bytes count
against per-request/aggregate wire limits. Descriptors are not retained and the
copied config clears its borrowed header pointer. The no-header path keeps the
native four-field writer, with new presence checks and larger client/config
layouts; no zero-overhead claim is made. Recompile all consumers for the config
layout change, independent of compile-time module selection.

All custom fields are treated as potentially sensitive. At final client release,
the extension wipes its flat text snapshot. At final request-wire release, it
wipes the whole encoded request buffer when custom headers were present,
including send/cancel/error paths. The real final references determine retirement;
completion alone is not permission to wipe bytes still borrowed by native TCP.
Caller buffers, native TCP/TLS/internal buffers, OS memory and dumps are outside
this erasure contract. Diagnostics never include supplied field values. This is
not secure erasure of every copy in the process or system.

The TLS example accepts an optional `AUTHORIZATION_FILE` after its CA/identity
arguments. It reads a bounded runtime value, permits one terminal CRLF/LF from a
text file, creates the immutable client snapshot, and wipes its own input buffer.
It does not read credentials from ambient environment variables or print them.
An explicitly supplied caller file remains caller-owned and must be removed or
managed by the caller after the example exits. Without the optional argument,
the previous TLS echo usage is unchanged:

```sh
python tools/build.py --compiler gcc --arch native --manifest extlibs/xrpc/config/modules.json --suite xrpc --no-single --cflag=-O2
out/gcc/native/xrpc/examples_http_tls_main localhost 8443 CA.pem rpc.test AUTHORIZATION_FILE
python extlibs/xrpc/tests/interop/http_headers_tls.py --auth-mode bearer
python extlibs/xrpc/tests/interop/http_headers_tls.py --auth-mode basic
```

On Linux use `--arch x64` and the matching output path. Independent peers create
runtime CA/key material and fake public Basic/Bearer fixtures, check exactly one
Authorization field, and verify no RPC/credential header reaches a rejected TLS
identity. These are client credential-transport tests; the independent Python
peer is not an implemented xrpc authorization server.

The dedicated C cases cover copied/overwritten inputs, ordinary/notification/
batch traffic, all 64 fields, exact byte bounds, unaligned descriptors, duplicate/
reserved/control rejection, aliases and constructor allocation quotas. TLS cases
exercise DNS, numeric and prepared factories. Fixture expectations are published
with release/acquire ordering and checked counts. The separate pre-free probe
compiles the actual production TU, inspects bytes immediately before real frees
and checks successful/cancelled/dropped wires and partially constructed snapshots
under OOM. It does not read freed storage or alter the shipped allocator.

Matched header profiles compare no-header/two-header TCP and TLS traffic with
the same compiled closure, generator, workers, payload and arrival rate. The
values are public fixtures. Snapshot construction and shutdown are outside
timing. Wire construction and observation/generator work inside the collector
window contribute to process
CPU. A final native callback and its deferred wire wipe may retire after that
window; this metric does not prove complete per-request erasure/retirement CPU
cost. Snapshot erasure during shutdown is also excluded. Standard noise,
production/native Linux and 72-hour acceptance remain required.
