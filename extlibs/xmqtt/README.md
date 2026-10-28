# xmqtt (developing)

Independent pure-C MQTT 5.0 / 3.1.1 client extension using public XRT APIs.
**Only wire primitives and bounded framing are currently implemented.** Complete
packet/property codecs, topic/filter rules, client/session/QoS state machines,
authentication, reconnect, durable storage and TCP/TLS/WebSocket are pending.
No broker or production/performance acceptance is claimed.

The independent implementation follows the OASIS
[MQTT 5.0](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html) and
[MQTT 3.1.1](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html)
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
MORE preserve ambient error. MORE/ERROR preserve read outputs; all write failures
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
The workload measures framing, a topic-string primitive and header writing only,
not complete packet validation, delivery throughput, QoS or durable sessions.
