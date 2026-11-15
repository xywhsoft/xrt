# WebSocket URI input (developing)

`xrpc_ws_uri_parse` provides `xrpcWsUriParse` without a transport/runtime. Select
`xrpc_ws_uri` for `xrpcWsConnectUri` or `xrpc_ws_uri_tls` for
`xrpcWsConnectUriTls`. Each connection returns the ordinary `xrpcws` with the
existing [dial/retirement contract](ws_dial.md). Plain and secure factories require
the matching ws/wss scheme; no fallback, retry, redirect or userinfo authentication.

The parser returns checked-Size borrowed `xrpcwsuri` component views, secure flag,
network port, host kind, decoded-route size and resource size. Scheme is ASCII
case-insensitive, default ports80/443, absent/empty port uses default, explicit
port must be1..65535. Empty path becomes `/`; `?` is emitted only for a nonempty
query per RFC6455, while HasQuery preserves a syntactically empty query. Path/query
keep exact encoded octets; no dot-segment cleanup, percent normalization, decoding
or sorting changes the resource. Userinfo, fragments, raw non-ASCII and malformed
percent triplets fail. Valid IPvFuture is inspectable but routing returns
UNSUPPORTED; unsupported literal mechanisms are never guessed as DNS.

Hard caps:8192 URI bytes,1024 authority and4096 resource bytes. Valid parsing has
no heap allocation. Failure preserves every output byte. All views require the
original URI alive and unchanged. IPv4 recognition is strict; IPv6 excludes zone
identifiers under the referenced baseline URI syntax.

URI factories own HTTP Host/Target, replacing those two config views. Other HTTP
policies apply normally. Names are decoded exactly once, checked as UTF-8 and
reject controls/routing delimiters; path/query remain encoded. Strict numeric IP
bypasses Resolver, names use a borrowed host Resolver. No implicit IRI/IDNA
conversion is supplied: name service owns its mapping policy, and native TLS
identity policy must use names supported by Core (ASCII A-labels work directly).
Empty TLS VerifyName derives from URI host; empty SNI derives only for a name.
Explicit nonempty verification/SNI values remain caller policy. Native client
trust is mandatory; URI input does not bypass it.

Factories capture the setup clock before parsing/assembly and use the existing
native dial constructor with that same timestamp. Inputs are snapshotted before
return. Common route and resource views are borrowed during construction; escaped
host uses at most1025 stack bytes and empty-path/nonempty-query assembly at most4096.
No extra URI heap owner or shared runtime is added. Ordinary native session/dial/
HTTP allocations, copied setup data and TLS/masking costs remain real. Throughput,
tail latency, broader faults/fuzz/interop and native72h qualification remain open.
Multiple simultaneous peers also require host admission policy, including the
per-IP CONNECTING serialization of RFC6455 section4.1. This leaf does not add a
global per-IP coordinator or claim full client conformance qualification.

Normative sources: [RFC6455 section3](https://www.rfc-editor.org/rfc/rfc6455.html#section-3)
and [RFC3986](https://www.rfc-editor.org/rfc/rfc3986.html).
