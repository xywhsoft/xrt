# MQTT WS/WSS URI connections (developing)

Select `xmqtt_ws_uri_parse` for `xmqttWsUriParse`, `xmqtt_client_ws_uri` for
`xmqttClientWsConnectUri`, or `xmqtt_client_ws_uri_tls` for the additional
`xmqttClientWssConnectUri`. The product root selects all three. Native connection
setup and the ordinary Client remain those of [client_ws_connect](client_ws_connect.md).
No Core production change or new shared runtime is needed.

## URI and routing contract

Parsing follows [RFC6455 section3](https://www.rfc-editor.org/rfc/rfc6455.html#section-3)
and [RFC3986](https://www.rfc-editor.org/rfc/rfc3986.html). Absolute ws/wss scheme
comparison is ASCII case-insensitive. The counted URI is at most8192 bytes,
authority1024 and eventual resource-name4096. Port is positive and fits uint16;
absent/empty port defaults80/443. Userinfo, fragments, raw non-ASCII, malformed
percent triplets and scoped IPv6 syntax are rejected. No IRI/IDNA conversion is
performed. DNS A-labels work directly; the host Resolver owns name-service policy.

The parser returns borrowed raw authority/host/path/query views, strict IP kind,
decoded host-octet count and eventual resource-name size. Empty path means /;
only a nonempty query adds ?. HasQuery separately preserves an explicit empty
query. Valid IPvFuture is inspectable but factories return UNSUPPORTED without
routing it as DNS. Checked Size/ranges/disjoint output apply; failure changes no
output bytes. A valid parse makes no heap allocation; diagnostics may allocate.

Factories percent-decode the separated host once, require valid UTF-8 and reject
ASCII controls and routing delimiters (including backslash). Encoded path/query
are preserved without decoding, normalization or second interpretation. Strict
IPv4/IPv6, including an escaped strict IPv4 host, bypass Resolver. Names borrow
Resolver under the existing physical Client lifetime contract. The raw authority
becomes HTTP Host, including brackets and any explicit port. URI replaces the
policy's Host/Target views; NULL policy selects defaults, and all other bounds,
Origin/extra fields, native WS/Dial/Auth/Session/Inbox policies retain their contracts.

Explicit plain/secure factories require their matching scheme and never downgrade.
WSS requires an explicit native TLS trust policy. With ServerNameFromHost true,
empty VerifyName derives from the decoded URI host independently of an explicit
SNI override; empty SNI derives only for a name. IP literals therefore use IP
verification without IP SNI, following [RFC6066 section3](https://www.rfc-editor.org/rfc/rfc6066.html#section-3).
Nonempty verification/SNI overrides remain intentional caller policy. With the
flag false, caller supplies native identity. This is not automatic IDNA handling.
Supplied Resume follows native identity/trust policy. Select the separate
[WSS credential handoff](client_ws_tls_resume.md) leaf to capture new credentials
after WS event handoff. Without that leaf, ResumeLimit must be zero.

## Clocks, ownership and costs

The original Client.SetupTimeout begins at URI API entry, covering bounded
validation/decoding/assembly plus all ordinary DNS/candidate/TCP/TLS/HTTP/MQTT
setup and local proof. Queued application deadlines also retain their own API
entry. No MQTT bytes precede validated101/Accept/exact mqtt. There are no implicit
redirects, credentials from userinfo, retry, replay or automatic reconnect here.

URI, policy, CONNECT/auth and TLS names snapshot before successful return. No
caller URI/policy view is retained. False preserves output and consumes no caller
resources. Success returns one ordinary Client; typed Ready may still fail.
Close/Wait/Destroy preserve all pre/post-Upgrade native aliases and original TLS
configuration-owner duties. Separately destroy Resolver and join Engine/code.

Adaptation uses bounded stack host1024/resource4096 scratch and parser/config
locals, with no URI heap owner or new Client field. It adds actual cold work:
Core target/authority parsing, scheme/port/bound/IP checks, percent search and
conditional decode/UTF-8 walk, possible resource assembly and native IP parsing.
The existing HTTP writer, network/name snapshots, TLS/masking and ordinary MQTT
copies/allocations remain. No zero-CPU-overhead or end-to-end zero-copy claim is made.

## Validation and remaining work

Run parser and independent functional/fault/state suites:

```text
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_ws_uri_parse_tests
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client_ws_uri_tests
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client_ws_uri_tls_tests
```

The [native CLI](../examples/ws_uri/README.md) accepts a URI, optional MQTT version
and required real CA file for WSS. Its DATA and physical JOIN lines are separate;
incomplete application Disconnect remains an exit failure.

The held-owner, same-input plain numeric constructor control measures equal XRT
allocation calls/bytes for the direct and URI factories; it does not establish
TLS or end-to-end zero overhead.

The [source-bound stage report](../../../docs/development/xmqtt-websocket-uri-20261116.json)
records actual final checks, source cohorts, compressed copies of original
binaries, raw attempts and acceptance gaps. Diagnostic parser/cold-setup profiles
and actual plain/TLS object sizes are distinct from controlled performance
acceptance. Previous Mosquitto clean-close failures remain unresolved and are not
relabeled successful by this URI stage.

All URI connection leaves remain developing. The later [automatic recovery
stage](reconnect_ws.md) qualifies existing controller/factory composition. Remaining:
post-Upgrade credential capture, durable state, broader async-OOM/partial-write/
load and broker-close qualification, controlled throughput/tails/CPU and native
Windows/Linux >=72h soak remain required, along with the other three products.

For a URI connection owning a durable Session and Inbox Writer, use the
[durable WebSocket adapter](client_store_ws.md).
