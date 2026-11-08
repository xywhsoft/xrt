# MQTT packet codecs

`xmqtt_packet` supplies feature-independent borrowed packet descriptors, config
and issue types, initialization and library-private reason/topic grammar helpers.
`xmqtt_codec5` reads and writes all fifteen MQTT 5 control packet types, including
CONNECT/Will, enhanced-auth packet syntax, QoS 0/1/2 PUBLISH and all four publish
acknowledgements, subscriptions and ordered reason lists, PING and DISCONNECT.
The separate [3.1.1 leaf](codec311.md) implements its fourteen packet types without
depending on properties/codec5, reusing public layouts and wire/grammar leaves.

The normative source is the [OASIS MQTT 5.0 standard](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html),
especially sections 1.5, 2, 3 and 4.7. Implementation is independent pure C over
public XRT memory/error/Unicode facilities. There is no upstream client code or
new shared runtime.

## Ownership and publication

`xmqttPacketRead5` is stateless: resupply the entire buffered prefix after MORE.
The total packet cap is enforced from the fixed header, before buffering a body.
MORE changes neither Packet, Issue nor scratch. Complete validation publishes
Packet only on DONE. Any scratch written before a negative status is provisional
and must not trigger session actions or application callbacks. Coalesced suffix
bytes are not consumed and cannot complete truncated in-packet fields.

Packet.Wire, topic, ClientId, username, password, payload and property views borrow
the input bytes. Property lists and subscription lists borrow caller arrays.
Keep both input and used arrays alive and unchanged while using the result.
SUBACK/UNSUBACK reason lists borrow raw bytes, with no per-reason scratch. CONNECT
and Will descriptors share a single property array in wire order. No API takes
ownership, retains prefixes or invokes callbacks. Inputs/config/buffer metadata
must remain immutable for each call; separate buffers/config permit concurrent
calls. Packet objects need no destructor.

Caller outputs and entire scratch capacities must be disjoint from input,
config, Buffers metadata and each other. Write/Size check every active source
view/vector and output alias. All encoding failures preserve output bytes and
Required/Written. Size and Write independently preflight immutable descriptors;
a prior Size does not bypass validation. Unused common descriptor fields must be
zero and inactive union members are ignored. Packet.Wire is decoded metadata and
is ignored by encoding.

## Validation and limits

Sender direction is explicit. Header flags, required nonzero packet identifiers,
CONNECT field presence and protocol version, CONNACK flags, per-packet/direction
reason codes, topic/filter grammar, shared group syntax, subscription option bits
and shared No Local are checked. MQTT 5 permits password without username.
All encoded strings apply the MQTT UTF-8/NUL policy. PFI=1 PUBLISH/Will payloads
are always checked as well-formed UTF-8 character data: NUL is legal there, and
the optional encoded-string control/noncharacter policy does not apply to data.
Opaque PFI=0/absent payload bytes have no text normalization or scan.

Property count and encoded byte caps aggregate CONNECT and Will sections,
including both length prefixes. Payload, packet, topic/reason counts and caller
scratch capacity are bounded separately. Zero is an actual limit; empty mandatory
property sections still need their length byte. Packet cap includes fixed-header
bytes and accepts 2 through the protocol maximum. Fields with uint16 lengths
remain limited to 65535 bytes; PUBLISH payload uses the remaining packet body.

Negative read statuses separate malformed encoding, semantic protocol violations,
unsupported CONNECT protocol, invalid indicated payload format and local API/
resource errors. The optional Issue records a field/section start or exact payload
UTF-8 error offset plus confirmed type/flags/identifier. It remains reliable when
cold diagnostics fail to allocate. API misuse and MORE leave it unchanged; DONE
clears it. Property-section local limits and scratch shortages are reported as
the aggregate LIMIT issue; topic scratch shortage has CAPACITY. Invalid type or
header flags have no confirmed type. Issue metadata never authorizes a partial
packet action. Connection policy decides the appropriate rejection/closure and
must obey MQTT rules for unauthenticated and partially decoded peers.

ACKs accept omitted reason/property-length forms, DISCONNECT accepts empty or
reason-only bodies, and AUTH accepts the explicitly permitted empty success form
from a server. Explicit AUTH property sections require Authentication Method;
reason-only AUTH has no defined abbreviation and is malformed. Write preserves
list/property/data order and emits shortest integers and abbreviated zero-reason
ACK/DISCONNECT/AUTH bodies. It does not preserve redundant empty-property syntax.

## Costs and remaining work

No successful or MORE allocation occurs. Decoder copies only a fixed-size packet
descriptor, config/buffer metadata and each property/topic descriptor into caller
storage. Payload/text/binary remain borrowed. UTF-8 encoded strings are validated
once in a successful call, with additional ASCII topic/filter grammar walks.
PFI=1 scans the payload once. Encoder performs one semantic preflight and global
alias checks, then copies each data field once and writes scalar/length bytes.
The emitter reuses prepared property sizes without a second text validation.
Separate Size then Write repeats preflight. Public wire/property calls still
perform span/alias checks and small config/descriptor copies; private helpers have
hidden shared-library visibility. Cold failure diagnostics can allocate and can
fall back to XERR_MEMORY. This is not a zero-overhead claim.

The packet codec has no network/session side effects. Negotiated aliases and
capabilities, auth method continuity/negotiation, CONNECT/CONNACK sequencing,
requested ACK counts, session-present consistency with clean start, request-
problem/response information, session-expiry changes, identifier ownership,
flow control, QoS state/delivery, expiry, reconnect, durability and transports
remain runtime obligations. AUTH success syntax alone does not authenticate a
peer. Complete client interoperation and production/performance acceptance are
still pending. Offline packet/filter benchmarks measure exactly those local
operations and do not establish network throughput or tail latency.

Runnable examples: `examples/packet/main.c` and `examples/codec5/main.c`.
Run suite `xmqtt_tests` for modular/single protocol, malformed-input, boundary,
mutation, lifetime, atomicity and OOM tests. Current evidence belongs in the
[progress ledger](../../../docs/development/service-extensions-progress.md).
