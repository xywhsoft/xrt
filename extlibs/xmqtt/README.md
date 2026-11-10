# xmqtt (developing)

Independent pure-C MQTT 5.0 / 3.1.1 client extension using public XRT APIs.
The independent [in-memory session state](docs/session.md) leaf now implements
bounded QoS state, cross-connection replay decisions, incoming identity retention
and expiry. The optional [Client Session factory](docs/client_session.md) now attaches it
to numeric TCP with explicit reconnect and a shared Inbox. Optional
[TLS Client Session](docs/client_session_tls.md) composes numeric, hostname and
prepared-address TLS, including MQTT5 challenge/final-proof authentication.
[TCP Dial/Auth Session](docs/client_session_dial.md) adds hostname/prepared TCP
recovery and numeric/hostname/prepared enhanced-auth factories. The developing
[automatic reconnect controller](docs/reconnect.md) adds generation leases, bounded
backoff/deadlines and physical cleanup over a borrowed Engine/task pool. Durable
recovery and complete transport/interoperability qualification remain required.
**Wire primitives, bounded framing, MQTT 5.0 properties and full MQTT 5.0/3.1.1
packet codecs and negotiated connection constraints are implemented.** A developing
[bounded TCP client](docs/client.md) now covers CONNECT/CONNACK, basic credentials,
keep-alive, QoS-0/1/2 send/receive with retained acknowledgement metadata,
SUBSCRIBE/UNSUBSCRIBE with owned per-filter reasons, shared outgoing identifiers/flow credit, automatic MQTT5 diagnostic close, bounded application graceful DISCONNECT and physical shutdown.
MQTT5 incoming Topic Aliases are opt-in through CONNECT; outgoing aliases
snapshot logical topics and select safe compact/full frames. See the
[local receive-alias example](examples/receive_alias/README.md).
Optional [MQTT5 enhanced authentication](docs/client_auth.md) now provides
bounded multi-round AUTH, explicit final-proof verification and reauthentication
with concurrent ordinary traffic. Optional [verified TLS](docs/client_tls.md)
provides TLS 1.2/1.3, mutual authentication and the same MQTT operations over
numeric, hostname and prepared-address factories. Optional
[TLS1.3 credential handoff](docs/client_tls_resume.md) supplies a bounded latest
ticket for explicit subsequent connections. Durable sessions,
cross-connection cache management and WebSocket remain pending. Optional [hostname/prepared-address Dial](docs/client_dial.md)
uses bounded Core DNS and candidate racing with the original setup deadline.
Optional [Client Inbox](docs/client_inbox.md) preserves accepted PUBLISH messages
and one shared physical object quota through sequential Client/Engine destruction.
Its ordinary factory preserves application payloads; the optional Client Session
factories additionally restore protocol state over numeric/hostname/prepared TCP or verified TLS.
No broker or production/performance acceptance is claimed.

Run the [offline TCP client example](examples/client/main.c) with the xmqtt
manifest and `--suite xmqtt_client --no-single`. It asserts handshake, QoS-0
send/receive, QoS-1 echo/acknowledgements and retained-message validity after
Client/Engine destruction. The same suite also runs an
[error-disconnect example](examples/error_disconnect/main.c) proving final reason
0x81 and original-error/physical-retirement semantics. A
[graceful shutdown example](examples/graceful/main.c) drains QoS-1, sends the final
normal DISCONNECT and observes peer EOF and physical ownership retirement. The
[QoS-2 sender example](examples/qos2_send/main.c) verifies both PUBREC/PUBCOMP
results, then graceful shutdown and physical ownership retirement. The
[QoS-2 receiver example](examples/qos2_receive/main.c) takes an owned incoming
message, waits for the peer to observe PUBCOMP and verifies retained payload
validity after physical shutdown.

The independent implementation follows the OASIS
[MQTT 5.0](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html) and
[MQTT 3.1.1 with Errata 01](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/errata01/os/mqtt-v3.1.1-errata01-os-complete.html)
standards, using their data representations and fixed-header rules. Sources of
reference client libraries are not embedded/copied into this extension.

Select `XMQTT_MODULE_XMQTT_WIRE` before including `xmqtt.h` (and before core
feature processing). `XMQTT_MODULE_XMQTT` selects all current product leaves;
the product root deliberately remains developing. Link one separate xmqtt and
one XRT per host process. Module selection does not change public type layouts.

`xmqttHeaderRead` inspects at most five bytes, checking version, packet type,
flags, variable-integer form and the **total** packet byte limit before a body
is buffered. PUBLISH checks QoS/duplicate flag combinations; 3.1.1 rejects AUTH.
`xmqttFrameRead` adds the complete-body availability check and returns borrowed
Wire/Body views for one packet. Coalesced bytes remain after Header.PacketBytes.
Body includes the variable header and payload, and is opaque to this leaf.
DONE is a complete frame, not proof of a valid packet body or a permissible
session action. Direction, body-length rules, identifiers/properties/topic
semantics and session ordering remain the packet/runtime layers' responsibility.

Reads are stateless: MORE retains nothing and changes no output. Supply the
complete prefix again, rather than the new suffix alone. This scans at most five
header bytes again; no payload is scanned or copied. The caller owns bounded
receive storage, which must stay alive/unchanged while a returned view is used.
The default total-packet cap is 1 MiB, explicitly configurable up to the protocol
maximum; it never allocates a receive buffer. Header size includes the actual
Remaining Length encoding. A configured cap is a local resource result, distinct
from malformed wire. No transport, timeout, EOF, admission queue or recovery
semantics are implied by this stateless framing API.

Variable integers read up to four bytes and stop at the first terminator; a
fourth continuation bit is immediately malformed. MQTT 5.0 enforces minimal
encoding. The explicit 3.1.1 compatibility policy accepts nonminimal 1..4-byte
forms. Writers always emit minimal encodings. HeaderWrite emits only the fixed
header; a complete packet codec must validate/build the body before transmission.

String/binary primitives use a uint16 big-endian byte length. Binary accepts any
bytes. Strings require scalar UTF-8, reject NUL and preserve BOM. Empty strings
are valid primitives; individual fields can impose further rules. Optional
`XMQTT_UTF8_REJECT_DISALLOWED` rejects controls/noncharacters; flags=0 applies
the mandatory string rules. This API validates MQTT **encoded strings**, not a
PUBLISH payload's separate UTF-8 data format. Data beyond the first field stays
unconsumed. String/binary reads borrow the source, and writes copy once.

Every call is synchronous on the caller thread, retaining no callback/input.
There is no mutable global codec state or successful heap allocation. DONE and
MORE preserve ambient error. Wire/field MORE/ERROR preserve read outputs; all write failures
preserve the wire buffer and Written. Error diagnostics can allocate and fall
back to XERR_MEMORY. Overlapping output/source/config/size objects, null misuse,
unknown flags/configuration and arithmetic-wrapping spans are rejected. Range
checks require actual objects and cannot authenticate arbitrary addresses.

Costs are explicit: one bounded header parse per frame call; one UTF-8 walk per
complete string check/read/write (ASCII handled directly, non-ASCII scalars use
the public XRT decoder); zero payload copies in framing/field reads; one payload
copy for field writes. Integer/header writers encode in 4/5 stack bytes and copy
that small header once. Public span/alias checks and core diagnostic lookups
remain counted work. There is no DOM, receive allocation or new shared runtime;
this does not establish end-to-end zero-copy or comparator performance.

Select `XMQTT_MODULE_XMQTT_PROPERTIES` for the MQTT 5.0 section codec. It covers
all 27 standard property ids and seven data representations, retaining order and
borrowed string/binary/pair views. Packet/Will scope and sender direction are
explicit. It enforces allowed scopes, singleton/repeated rules, numeric domains,
response-topic syntax and section-local authentication requirements. Unknown
ids are rejected. It never applies negotiated session defaults or interprets
an absent abbreviated packet section as an explicit zero-length section.

`xmqttPropertiesRead` publishes Count/Consumed/Raw only on DONE. Caller scratch
is provisional on failure; discard it until complete validation. MORE changes
no scratch, retains no prefix and preserves ambient error. The complete-section
byte cap (including the length integer) is checked before its body is available.
Count and caller-capacity failures are local; malformed and protocol failures
have distinct return statuses that survive diagnostic OOM. Size/Write preflight
independently and preserve all outputs on failure, using caller storage and no
successful heap allocations. Text is UTF-8 checked once per call; writes copy
data once and retain input order. Response topics have an additional wildcard
scan. See [property contracts](docs/properties.md) for ownership, omitted AUTH
forms, default limits and the required connection-level rules still pending.

Select `XMQTT_MODULE_XMQTT_CODEC5` for complete MQTT 5 packet encoding/decoding.
`xmqttPacketRead5` validates all fifteen control types with explicit sender
direction, flags, identifiers, property sections, topic/shared-filter syntax,
subscription options, ordered reason domains and indicated payload format.
Text/binary/payload views borrow wire bytes; property/subscription lists borrow
caller arrays. Packet publishes on DONE only; scratch is provisional on errors.
MORE changes no output/scratch, and independent typed Issue context survives
diagnostic OOM. Size/Write are atomic independent preflights, with one data copy
and no repeated string validation during emission. Encoded strings forbid NUL;
PFI=1 payload character data permits it. MQTT 5 password-only CONNECT is legal.
Negotiated CONNECT/CONNACK capabilities and outbound constraints have a separate
leaf described below. Alias mappings/auth/session/QoS state still belong to the
pending client runtime.
See [packet contracts and costs](docs/codec.md). `XMQTT_MODULE_XMQTT_PACKET`
selects just the shared descriptors/config/initializers and private grammar.

Select `XMQTT_MODULE_XMQTT_CODEC311` for all fourteen MQTT 3.1.1 packet types,
without depending on 5.0 properties or codec code. It shares packet types/config
and the borrowed, atomic, bounded, zero-success-allocation model. It validates
legacy CONNECT flag/ClientId rules, exact acknowledgement lengths, reason-byte
domains, client-only DISCONNECT and legacy subscription option bits. Payload,
Will and password remain opaque binary. Nonminimal Remaining Length forms are
accepted and canonicalized, including the full four-byte range fixed by Errata
01. The descriptor's CleanStart field means Clean Session for this codec. It
rejects 5.0 producer fields instead of discarding them; callers select a negotiated
protocol explicitly. See [3.1.1 contracts](docs/codec311.md) for version differences,
ignored property scratch fields, lifetimes, limits and pending session work.

Select `XMQTT_MODULE_XMQTT_NEGOTIATION` for `xmqttNegotiate` and
`xmqttNegotiatedPacketCheck`. They independently preflight descriptors, enforce
cross-packet session/id/auth/response rules and directional peer limits, and
check client packets against those capabilities. They allocate nothing on valid
acceptance/refusal and retain no packet/property scratch. Result text borrows
the relevant input bytes. This leaf selects both complete codecs; it does not
provide a minimal 3.1.1-only runtime. See [negotiation contracts and costs](docs/negotiation.md)
for version-specific session policy, original expiry, lifetime/alias/OOM rules
and the still-pending transport, credit, alias-map and authentication state.

```sh
python tools/build.py --compiler gcc --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_tests --cflag=-O2
python tools/build.py --compiler gcc --manifest extlibs/xmqtt/config/modules.json --suite xmqtt --no-single --cflag=-O2
python tools/build.py --compiler gcc --manifest extlibs/xmqtt/config/modules.json --suite xmqtt --trim-only
python tools/package.py --compiler gcc --manifest extlibs/xmqtt/config/modules.json --suite xmqtt --kind static --verify
python tools/package.py --compiler gcc --manifest extlibs/xmqtt/config/modules.json --suite xmqtt --kind shared --verify
python tools/measure_performance.py --config extlibs/xmqtt/config/performance_profiles.json --smoke
```

The [offline example](examples/wire/main.c) requires no broker/network or cleanup.
It prints packet types 12 and 13 (two-byte headers, zero-byte bodies), then
`string-bytes=4 consumed=6`, asserting that a BOM survives a field round trip.
See [API inventory](docs/api/reference.md), [coverage](docs/coverage.md) and
[development progress](../../docs/development/service-extensions-progress.md).
The [property example](examples/properties/main.c) prints
`properties=3 bytes=18 subscription=7` for a section round trip. Its data needs
no broker/network or cleanup. The [packet example](examples/codec5/main.c)
round-trips a full QoS-1 PUBLISH with a PFI UTF-8 payload containing NUL. The
[representation example](examples/packet/main.c) initializes PUBREL/config.
The [3.1.1 packet example](examples/codec311/main.c) round-trips binary PUBLISH.
The [negotiation example](examples/negotiation/main.c) decodes literal peer
packets and checks allowed/forbidden publishes, with no broker or cleanup.
The existing workloads separately measure wire primitives, property sections, private
already-UTF8-validated ASCII filter grammar, MQTT 5 PFI/property PUBLISH, and
MQTT 3.1.1 binary PUBLISH decode/encode.
Two negotiation variants measure independent handshake descriptor preflights
plus one outbound check for MQTT 5 and 3.1.1. They do not establish network delivery throughput, tail latency, QoS sessions,
durability or performance parity.
