# MQTT 3.1.1 packets

`XMQTT_MODULE_XMQTT_CODEC311` selects the full fourteen-type MQTT 3.1.1 packet
codec, shared packet types/grammar, wire primitives and public XRT Unicode. It
does not select the 5.0 property or packet leaves. Public layouts are identical
in every trim. Root `XMQTT_MODULE_XMQTT` includes both versions.

The independent implementation follows [MQTT 3.1.1 with approved Errata 01](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/errata01/os/mqtt-v3.1.1-errata01-os-complete.html).
The [erratum](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/errata01/os/mqtt-v3.1.1-errata01-os.html)
corrects the Remaining Length range algorithm; the codec accepts the full
four-byte range, including complete packets above 2,097,151 remaining bytes.
It uses the wire leaf's explicit compatibility policy accepting nonminimal
1..4-byte integers and always emits shortest integers. No client-library source
is imported and no shared runtime is introduced.

## Version-specific semantics

| Field or packet | 3.1.1 codec |
| --- | --- |
| CONNECT | Protocol name MQTT, level 4; CleanStart descriptor field means Clean Session |
| ClientId | Encoded UTF-8, up to 65535 bytes; zero length requires Clean Session; server acceptance policy is separate |
| Credentials | Password requires username; username is encoded UTF-8, password is binary |
| Will | No property section; nonempty wildcard-free topic and length-prefixed opaque binary message |
| CONNACK | Exactly two body bytes, Session Present/reserved bits, return code 0..5; failures cannot set Session Present |
| PUBLISH | Nonempty wildcard-free topic; nonzero id only for QoS 1/2; remaining bytes are opaque payload |
| PUBACK/PUBREC/PUBREL/PUBCOMP | Exactly two body bytes for a nonzero identifier; no reason/property abbreviation |
| SUBSCRIBE | Nonempty filter list, one QoS byte per filter (0, 1 or 2); all high option bits unavailable |
| SUBACK | Nonempty ordered bytes 0, 1, 2 or 0x80, without properties |
| UNSUBSCRIBE | Nonempty filter list, no options; descriptor Options must be zero |
| UNSUBACK | Exactly two body bytes for the identifier; descriptor reason list must be empty |
| PINGREQ/PINGRESP | Empty body, correct sender role |
| DISCONNECT | Client to server only, empty body; no reason/properties |
| AUTH | Reserved/unavailable |
| `$share/` | Ordinary literal filter prefix; no MQTT 5 shared-group interpretation |

Producers reject 5.0 properties, subscription options and reason fields instead
of silently discarding them. Only active union members are interpreted; unused
common scalar fields must be zero. A payload starting with bytes that resemble
5.0 properties remains binary data in 3.1.1. Callers explicitly select the
negotiated codec; an existing stream cannot be automatically downgraded by
guessing packet contents. Broker-specific 3.1.1 shared-subscription conventions
are not imposed on standard filter validation.

## Ownership, limits and errors

Read/Size/Write use the shared packet/config/issue structures. Read is stateless,
retains no prefix, and requires full-prefix resupply after MORE. Packet publishes
only on DONE. MORE changes neither Packet, Issue nor scratch. Scratch can be
provisional after an error and cannot trigger session/application actions. Input
data/reasons remain borrowed; topic lists borrow caller scratch. Keep used bytes
and arrays alive and unchanged. Returned Wire.Size consumes one actual packet,
including a nonminimal header; coalesced suffixes cannot complete truncated
in-packet fields. No destructor or ownership transfer is needed.

Only topic scratch fields in Buffers are used or validated. Property pointer and
capacity are ignored without dereferencing; this permits one config/metadata
layout across protocol trims. Packet, optional Issue and complete topic scratch
capacity must be mutually disjoint and cannot overlap Input, Config or Buffers
metadata. Metadata/config are copied for the call. Config must have the expected
Size/ranges even for unused property fields. Property budgets consume no bytes or
counts and may be zero. Packet, PUBLISH/Will payload, topic/return-code counts and
topic scratch limits are independent. Zero is a real limit. The total packet cap
is checked from the header before body buffering. Fixed-body length and role
violations can also be rejected immediately. Other field validation requires a
complete bounded packet. String/binary fields have uint16 lengths; PUBLISH data
can use the rest of the protocol-sized body.

Negative statuses distinguish local API/resource failure, malformed encoding,
protocol violation and unsupported CONNECT protocol. Issue records only confirmed
type/flags/identifier and a field start offset, independently of cold diagnostic
allocation. Empty persistent ClientId has CLIENT_ID context. MORE/API misuse
leave Issue unchanged; DONE clears it. These are parsing results, not permission
to act on a partially validated packet. 3.1.1 has no wire error acknowledgements
for most failures; the eventual client runtime applies connection policy.

Size and Write independently preflight immutable inputs. All failures preserve
size outputs and wire bytes. Every active source view/vector, Packet/config and
wire/size output must be disjoint. Wire metadata itself is ignored. Each encoded
string is checked once; emission copies each active field once without rescanning
UTF-8. Success/MORE do not allocate. Reads copy fixed packet/config/metadata and
topic descriptors, while borrowing text/binary/reason data. Opaque payloads have
no UTF-8/content scan. Scalar writes, range/alias checks, grammar walks, public
wire calls and separate Size+Write preflights remain measured work. Cold error
diagnostics can allocate/fall back to XERR_MEMORY. There is no zero-overhead claim.

## Running and remaining scope

`examples/codec311/main.c` round-trips a binary QoS-1 PUBLISH and prints its id,
topic and lengths. It is offline, uses stack/caller data and needs no cleanup.
From the repository root, run `python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_codec311`
for its modular/single test and example, or `xmqtt_tests` for all leaves.
The dedicated performance profile measures local binary QoS-1 PUBLISH decode
and atomic encode. It is not comparable to a different protocol/validation
workload and proves no delivery throughput or tail latency objective.

Full client state remains pending: CONNECT/CONNACK order and Clean Session
consistency, granted QoS/requested SUBACK count, active packet identifiers, QoS
delivery/recovery, keepalive, reconnect, persistence and transports. Packet codecs
do not authenticate peers or establish sessions. Interoperability, performance
baselines and native Windows/Linux production acceptance remain required. Exact
local evidence is in the [progress ledger](../../../docs/development/service-extensions-progress.md).
