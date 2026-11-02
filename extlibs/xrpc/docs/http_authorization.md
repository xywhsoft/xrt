# Asynchronous HTTP authorization

Set `xrpchttpserverconfig.Authorize` and `AuthorizationUser` before factory/Start.
The transport validates the head/framing/route, rejects duplicate Authorization,
then seals a copied head snapshot. One bounded task runs on the existing server
executor, sharing its queue, MaxOperations and request-byte admission. It creates
no Future or dispatch scratch. The network owner stops body processing and sends
no 100 Continue or business dispatch before the decision. Already-received bytes
may occupy bounded HTTP/native receive storage; this is not a guarantee that no
body byte has arrived from the network.

The callback borrows immutable Head/Remote/Secure, its own child Cancel token and
the original absolute request deadline. Remote comes from the accepted transport,
not forwarded headers. Secure denotes TLS transport, not authenticated client
identity. Poll cancellation/deadline; do not destroy/request the token or retain
borrowed views. Code/User remain alive through successful server Destroy and can
be entered concurrently. Noncooperative callbacks delay physical shutdown. Own
callback Wait/Destroy is rejected by the existing executor identity.

ALLOW publishes up to MaxAuthorizationContextBytes opaque identity bytes into
every business handler's copied Context->Data. Set ContextBytes within the given
capacity, initially zero; use memcpy for structured data rather than retaining
external pointers. Default identity capacity is 256 bytes, bounded by
Execution.MaxContextBytes. Context ownership and erasure are described in
[server context](server_context.md). No scheme parser, password database, OAuth
refresh, automatic challenge retry/replay, proxy trust or mTLS identity is added.
Applications implement credential policy using the immutable request facts.

UNAUTHORIZED returns empty 401 with the configured copied WWW-Authenticate value;
FORBIDDEN returns empty 403; callback ERROR/cancellation returns 503, while invalid
return values/output lengths yield 500. Rejections close the connection and do
not drain/reinterpret a pipelined unauthorized body. UnauthorizedChallenge defaults
to Bearer and is limited to 128 bytes. Native HTTP field-value validation prevents
injection; the caller supplies a semantically valid RFC challenge for its scheme.
This follows [RFC 9110 challenge semantics](https://www.rfc-editor.org/rfc/rfc9110.html#section-11.6.1)
and [Expect handling](https://www.rfc-editor.org/rfc/rfc9110.html#section-10.1.1).

MaxAuthorizations defaults to 128 and includes queued/running and retained
head/identity snapshots during body wait. Snapshot headers, exact field arrays,
raw head, alignment padding and output capacity count against MaxBufferBytes.
The executor reserves the same work footprint while queued/running; these are
independent logical limits, not additive process RSS figures. Auxiliary admission
credit returns before notifying the transport so a one-operation executor can
admit its now-authorized document. Core drop/join and retained transport references
remain the physical lifetime barrier. Execution AuxiliaryAccepted/Completed are
separate from JSON document Accepted/Completed.

Snapshots and child tokens retain real peer/server duties. Callback return precedes
release/acquire decision publication. Disconnect/deadline/Close requests the child
token; the snapshot remains until all real owners retire. Snapshot storage is
securely zeroed immediately before free. With authorization enabled, HTTP receive
buffers are copied into newly reserved storage on growth, and old storage is
wiped before free; compaction wipes removed tails and final retirement wipes
capacity. Parent native TCP/TLS buffers, caller copies, pointed-to objects,
business results and dumps are outside this erasure contract. Peak growth credit
covers old plus new buffers. This adds explicit copies/wipes and allocations, not
zero-overhead or complete system-wide secret erasure.

Disabled authorization allocates no per-peer auth sidecar or snapshot and creates
no child/task; config/info owners and additional presence checks still have costs.
The response field scratch retains its prior size. Recompile consumers for public
config/info and nested execution-info layout changes. No Core source or shared
runtime is added.

The HTTPS example accepts an optional runtime authorization file after its key
kind. It uses the native constant-time byte comparison for equal-length fixture
credentials, returns identity [42], does not log values, and wipes its input after
physical server destruction. Its exact-value policy demonstrates transport; it is
not a credential database or token lifecycle service. Caller files remain owned
by the caller.

```sh
python tools/build.py --compiler gcc --arch native --manifest extlibs/xrpc/config/modules.json --suite xrpc --no-single --cflag=-O2
out/gcc/native/xrpc/examples_http_server_tls_main CERT.der KEY.der 8443 60000 rsa AUTHORIZATION_FILE
python extlibs/xrpc/tests/interop/http_auth_tls.py --auth-mode bearer
python extlibs/xrpc/tests/interop/http_auth_tls.py --auth-mode basic
```

On Linux select --arch x64 and the corresponding output directory. Independent
peers create runtime CA/key files and public Basic/Bearer fixtures, verify TLS
1.3/1.2, identity data, batches/notifications, large heads, Continue ordering,
duplicate credentials and pipelined refusals. The pre-free probe checks actual
production storage, held-callback lifetime, snapshot OOM and child-token budget
failures, without shipped hooks or freed-memory reads.

Matched off/on authorization profiles keep TCP/TLS, credential headers, workers,
rate, payload and persistent connection identical. They return no identity and
set output capacity to zero, excluding default 256-byte capacity and nonempty
identity copy/erasure costs. Terminal-watch latency and
process CPU retain their existing observation/collector boundaries; native
retirement/wipe tails can follow the window. No stable baseline, saturation or
production/native long-run acceptance follows from these profiles. Managed
tickets, mTLS, negotiated retries and broader transport metadata remain pending.
