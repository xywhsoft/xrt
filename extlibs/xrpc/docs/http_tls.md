# HTTP client TLS

Protocol references: [HTTP certificate identity](https://www.rfc-editor.org/rfc/rfc9110.html#section-4.3.4)
and [ALPN selection](https://www.rfc-editor.org/rfc/rfc7301.html#section-3.2).

Select `XRPC_MODULE_XRPC_HTTP_TLS`. `xrpcHttpTlsConnect`, `xrpcHttpTlsDial` and
`xrpcHttpTlsDialAddresses` require TLS explicitly on numeric, DNS and prepared
endpoints. They reuse Core's bounded resolver/candidate controller, TLS session
and TLS stream. They do not implement cryptography or an independent IO runtime.
An HTTP client retains one optional private secure-state pointer; the ordinary
numeric TCP path does not allocate TLS state or perform TLS IO calls.

Configure an explicit certificate identity with Session.ServerName or VerifyName.
DNS, HTTP authority and TLS identity are separate inputs. IP identities belong
in VerifyName, not SNI. Supply a Core Verifier with explicit trust and bounded
verification policy; the default Core client rejects missing verification.
ResumeOnly is explicit and follows Core ticket/identity rules. Mutual client
authentication, ticket-management APIs and HTTP application authentication are
separate outstanding work. No certificate rejection falls back to cleartext.

The factory snapshots borrowed names/ALPN and retains shared session policy,
verifier and optional resume objects before returning. Verifier callback code
and context follow Core lifetime, including final Engine join after client Closed.
HTTP/1.1 is the default ALPN; unsupported protocol offers are rejected before
admission. Empty ALPN is allowed. Negotiated protocol is checked before readiness.
Metadata is bounded at 64 KiB by default (hard 1 MiB), plaintext send work at
64 KiB per turn (hard 1 MiB); WorkRequests additionally bounds native send calls.
Core context/session/TCP limits still bound record, plaintext and cipher queues.

Dial.Timeout is one absolute monotonic API-entry deadline spanning construction,
owner queueing, DNS, TCP and authenticated TLS readiness. Zero is immediate
asynchronous timeout, -1 disables it. The Core handshake timer is an additional
stage bound. Each queued RPC retains its own deadline/cancellation. The connection
source's callback credit retires physically; the setup timer/source state survive
through the TLS handshake, then are released. Close observes both source callback
retirement and actual transport closure. Resolver ownership stays with the caller
through Closed and it is joined separately.

Readiness publication also compares the absolute deadline directly. A native
verification/IO callback can occupy the same worker until after its timer became
due; a queued timer does not permit publishing an already expired connection.
The test suite deliberately holds the fixture verifier across that boundary and
requires TIMEOUT/NOT_SENT with zero peer RPCs. Production verifier callbacks must
observe Core's worker/fairness contract; the hold is fault injection.

TLS can accept short plaintext prefixes. The client keeps its encoded request
until complete acceptance or retirement, marks MAY_HAVE_EXECUTED at the first
accepted byte, and never replays a partially sent operation. Accepted TLS data is
copied into authenticated record storage; no zero-copy claim is made. An early
valid HTTP response may complete its RPC, but retires the connection if an unsent
request suffix remains. TLS failure/truncation is a transport failure, not HTTP EOF.
Result quotas, notifications, mixed batches and retained snapshots keep their
ordinary-client contracts. Public Close aborts; it is not a graceful close_notify
delivery guarantee.

The runnable example takes runtime trust material:

```sh
python tools/build.py --compiler gcc --manifest extlibs/xrpc/config/modules.json --suite xrpc --no-single --cflag=-O2
out/gcc/native/xrpc/examples_http_tls_main.exe localhost PORT CA.pem rpc.test
```

Use the corresponding platform output path on Linux. The service must accept
JSON-RPC `echo` with params `[7]` at `/rpc`. Success verifies the returned value,
then closes/releases the client, joins the resolver/engine and releases trust.
No args prints usage. Unit tests use a separate bounded TLS 1.3 fixture with a
historical public identity and explicit fixture trust; that trust callback must
not be used for arbitrary remote peers. Independent TLS peers and full native
Windows/Linux performance/72-hour acceptance remain required.
