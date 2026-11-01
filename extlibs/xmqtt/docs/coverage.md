# Wire contracts and pending scope

The full [service-extension target](../../../docs/development/service-extensions-plan.md)
remains required. The wire leaf implements data representations and fixed-header
framing; the properties leaf implements explicit MQTT 5.0 sections. The packet
and codec5/codec311 leaves cover shared descriptors/grammar and all MQTT 5/3.1.1
packet bodies. A separate negotiation leaf checks CONNECT/CONNACK relationships
and client per-packet capabilities; its [contracts](negotiation.md) distinguish
those pure checks from negotiated inbound/runtime state. The developing
[TCP client](client.md) integrates handshake, basic credentials, keep-alive,
QoS-0 send/receive and physical shutdown. QoS 1/2, session/auth/alias/expiry/
subscription state, reconnect, persistence and TLS/WebSocket remain pending.
There is no pinned-broker or final performance/production acceptance yet.

| Contract | Verification |
| --- | --- |
| Variable integer | Canonical boundary fixtures, every 2-byte prefix for both versions, exhaustive 0..65535 round trips, nonminimal compatibility, fourth continuation error |
| Fixed header | Independent literal valid-first-byte matrix across all 256 bytes/both versions, AUTH compatibility and PUBLISH flag combinations |
| Total packet bounds | All integer-length transitions through the protocol maximum, exact cap and cap-minus-one, malformed header rejection before body buffering |
| Complete frame | Every prefix of 0/1/2/127/128/16383/16384-byte opaque bodies, borrowed views, coalesced suffix and 3.1.1 nonminimal header byte accounting |
| UTF-8 strings | Every Unicode scalar generated with an independent encoder; all 65536 two-byte patterns; controls/noncharacters, NUL, overlong/surrogate/out-of-range/truncated sequences and error offsets |
| Field representations | Maximum 65535-byte string and every input prefix; arbitrary binary, empty fields and coalesced suffix; borrowed pointers and retained BOM |
| Atomicity/API | Capacity below required size, protocol overflow, unknown config/flags, wrapped/null spans, input/config/output/size aliases and unchanged outputs |
| Adversarial | 20,000 deterministic arbitrary frames, header/frame status agreement, bounded borrowed spans and publication invariants |
| Allocation/fault | 10,000 warm primitive cycles preserve allocation counters/ambient error; successful operation under one-byte quota; diagnostic OOM leaves output unchanged and no live budget bytes |
| Distribution | Modular/single tests, real offline example, independent package consumer and dedicated performance/size profiles |

Reads publish only DONE; MORE/ERROR leave all output values unchanged. A frame's
opaque body may still be an invalid MQTT packet. Read views require the caller's
original bytes to remain alive/unchanged. No partial prefix is retained, no body
buffer is allocated, and no business callback or network action occurs here.

Success allocation counters do not claim zero CPU/scan/copy costs. The README
lists actual header re-parsing, UTF-8 decoder calls, span/alias checks and small
header/field copies. Workload smoke does not prove performance parity, queueing
latency, TLS/QoS/durability equivalence or production stability. Current platform
evidence is in the [progress ledger](../../../docs/development/service-extensions-progress.md).

The [property section contracts](properties.md) cover all standard ids/types,
scope/direction/repetition rules, local-vs-peer status, section publication,
text/data lifetimes and remaining connection-level requirements. Property tests
use literal wire fixtures and a complete scope/direction matrix, count/byte/field
boundaries, duplicate/numeric/UTF-8 failures, exact ordered mutation round trips,
atomic writes and diagnostic-OOM-independent classifications.

The [full MQTT 5 codec](codec.md) adds literal fixtures for every packet type,
all prefixes and coalesced suffixes, complete CONNECT optionals/Will/auth fields,
every CONNECT flag byte, reason byte/sender combinations and subscription option
bytes. Shared filter levels, empty-topic aliases, PFI payload NUL versus encoded-
string NUL, malformed scalar sequences, mandatory/abbreviated sections and late
failures are covered. Packet/payload/property/topic caps, CONNECT+Will aggregate
limits, maximum uint16 fields, VBI transitions, API/alias errors, all short write
capacities, stable canonical forms for 30,000 raw/mutated packets, 10,000 zero-
allocation warm cycles and own/nested diagnostic OOM are tested. Modular/single
tests, offline examples, trimming and independent static/shared consumers include
the new APIs. Packet validation itself is stateless; client interoperability,
session/QoS recovery and production performance acceptance remain required.

The [3.1.1 codec](codec311.md) has separate fourteen-type literal fixtures with
all 1..4-byte header widths, every prefix, coalescing and canonical output. All
CONNECT flag bytes, CONNACK flag/return-byte combinations, SUBACK codes and
subscription option bytes are checked. Exact fixed-body lengths reject before
buffering; filters, empty persistent ClientId, credential dependencies, reserved
AUTH, client-only DISCONNECT and rejected 5.0 producer fields are covered.
Borrowed views stay in the first packet/caller arrays. Opaque data includes NUL,
invalid UTF-8 and v5-looking bytes. Tests exercise zero/property-independent
budgets, caller capacity, uint16 field maxima, >2 MiB complete packets at the
Errata 01 four-byte transition, protocol maximum header-only bounds, wrapped
spans/aliases, atomic failures, 30,000 raw/mutated canonical forms, 10,000 warm
allocation/error-identity cycles and quota/diagnostic OOM. The single trim asserts
no properties/codec5 feature; root package consumers exercise both versions.

## Connection negotiation constraints

The negotiation leaf checks cross-packet CONNECT/CONNACK consistency and client
per-packet capabilities. Tests cover literal wire peers, every version/clean/
session-present/local-state combination, client-id assignment/lifetimes,
successful/refused authentication (including present empty methods), directional
packet/receive/alias bounds, server keep-alive/expiry overrides and response-info
request rules. All 32 capability combinations exercise independent publish/
subscribe expectations. Requested subscription QoS and unsubscribe cleanup retain
their distinct rules. Original CONNECT expiry still constrains DISCONNECT after
a server override. Output/source/config aliases, wrapped spans, unchanged outputs,
10,000 allocation/error-identity cycles, one-byte budgets and typed semantic
errors under diagnostic OOM are covered. It provides no flow-credit/alias map,
authentication sequence, session store, transport or broker interoperability.
