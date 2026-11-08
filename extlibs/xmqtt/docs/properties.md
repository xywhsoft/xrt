# MQTT 5.0 property sections

This module implements an explicit Property Length and its ordered properties,
using the [OASIS MQTT 5.0 standard](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html).
It is not a complete packet/client or a 3.1.1 property extension. All 27 standard
identifiers, seven representations and packet/Will scopes have concrete code,
fixtures and scope/direction tests. Unsupported identifiers are malformed;
they are not skipped based on a guessed future type.

Descriptors have an explicit standard Id/Type and active union member. Byte,
uint16, uint32 and variable integers use Number. Text, Binary and Pair use their
matching members. Strings exclude the uint16 length prefix and borrow the source;
there is no decoded/owned text object. Pair order, repeated names/values, repeated
equal PUBLISH subscription identifiers and raw UTF-8/binary bytes are preserved.
Property order is retained rather than sorted or normalized. No defaults are
inserted. Missing and explicitly present zero-valued properties remain distinct.

The copied config defaults to a 1 MiB complete section, 256 properties and
mandatory MQTT string rules. Optional disallowed-character policy is inherited
from the wire leaf. A zero property-count cap permits empty sections only. The
section byte cap includes the Property Length integer; it is checked as soon as
that integer is available, before a body/scratch item is read. Packet-wide size,
queues and aggregate connection resources still need the packet/runtime gates.

The reader is stateless and caller-thread synchronous. MORE requires the full
prefix again and modifies no output. All negative statuses leave Document
unchanged, but property scratch may contain provisional entries and must not be
used. DONE commits Count, Consumed and the borrowed whole-section Raw span. Any
following packet payload/coalesced data stays unconsumed. Individual fields and
integers cannot cross the declared section boundary. No callbacks are retained.

Malformed wire includes bad integer forms, unknown/out-of-scope ids, truncated
typed fields and invalid UTF-8. Protocol errors include singleton duplicates,
numeric-domain violations, wildcard/empty response topics, client PUBLISH
subscription ids, server DISCONNECT session expiry, CONNECT auth data without a
method, and explicit AUTH sections lacking a method. User Properties repeat in
all their allowed scopes. Subscription Identifier repeats only in server
PUBLISH, and is singleton in SUBSCRIBE. These return classifications survive
diagnostic allocation failure. API/config/span failures, count/byte limits and
scratch exhaustion return LOCAL_ERROR; they never become peer protocol failures.
Cold XRT diagnostics may allocate/fall back to XERR_MEMORY; existing wire
diagnostics can be retained. DONE/MORE preserve ambient diagnostic identity.

Size/Write independently preflight the complete vector, every active view,
semantic rule and bound before publication. Failures preserve wire and size
outputs. Written is optional, no NUL is emitted, and wire/size outputs cannot
overlap vector/config/active input spans or each other. Inputs must stay alive
and unchanged through a call. Only the active union member is meaningful.

Costs: one scan of a complete property section, one UTF-8 check per string/pair
component, an additional wildcard walk for response topics, constant indexed id
lookup and a 64-bit duplicate set. Reads copy descriptors but borrow data.
Writes preflight text once, then copy each data field once, writing fixed-width
integers directly and using the public variable-integer writer. A separate Size
call adds its own preflight. Public wire span/alias checks remain real work;
there is no successful heap allocation, DOM, ownership/refcount result layer,
or new shared runtime. No zero-overhead or end-to-end performance claim follows.

Connection-level checks remain mandatory: negotiated auth-method equality and
authentication sequence, request-problem/response-information policy, topic-alias
maxima/mappings, subscription provenance/capabilities, expiry changes against
the original CONNECT, property-dependent payload format, and packet-specific
reason codes/omitted sections. In particular an omitted AUTH short form is a
packet-level decision, distinct from an explicit property section. The full
scope in the [development contract](../../../docs/development/service-extensions-plan.md)
and the [current evidence ledger](../../../docs/development/service-extensions-progress.md)
is unchanged.
