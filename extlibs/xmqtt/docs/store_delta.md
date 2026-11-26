# Typed MQTT store deltas

Status: **developing**. This pure codec adds MQTT-aware transaction payloads to
the existing opaque journal. It validates every record's representation and
native PUBLISH semantics. It is not the state applier, asynchronous commit queue,
durable Inbox/Client or a proof that any network ACK is ordered correctly.
Those remain required in the [full store plan](../../../docs/development/xmqtt-store-design.md).
The separate [recovery applier](store_state.md) now validates cross-record order
and reconstructs immutable protocol/Inbox views from the complete committed
prefix. Live transaction scheduling, Session import and ACK/delivery integration
remain required; this codec's own no-state contract is unchanged.

## API, bounds and lifetime

`xmqttStoreDeltaConfigInit` defaults to MQTT5, the native codec limits,
128 operations and1MiB per encoded batch. MQTT3.1.1 is also supported.
Zero count/byte limits are real limits. An empty batch is32 bytes. Set journal
`PayloadSchema=XMQTT_STORE_DELTA_SCHEMA` (`0x584d4401`), matching MQTT level,
exact broker/principal/slot binding and storage generation.

`xmqttStoreDeltaSize` and `xmqttStoreDeltaWrite` independently validate immutable
operations and all active wire spans. Write preflights every record before
touching its wire output/count, copies each body once, and never serializes
unused C padding. Native packet decoding validates complete PUBLISH, identifiers,
QoS/fixed flags, property rules, UTF-8/PFI and resource limits. Full logical topic
is mandatory; connection-only Topic Alias is forbidden. Outgoing wire retains
QoS1/2 with DUP0 plus separate attempted/sent metadata. Incoming QoS0/1/2 retains
its valid received wire, including DUP when legal. CONNECT/AUTH and other
control-packet types are rejected as bodies. Application payload bytes remain
opaque; this is not a content or secret filter.

`xmqttStoreDeltaRead` requires exactly one complete version1 batch, not a partial
stream. Document publishes only on DONE. Operation/property scratch is provisional
on any failure and must not be used until DONE. Returned bodies borrow Input;
operations borrow the caller's operation array. Property scratch is reused for
one packet at a time and is not retained by returned operations. Keep Input and
the operation array alive/unchanged while consuming Document. With journal Next,
finish consuming/copying the borrow before any subsequent journal operation.

All writable spans are mutually disjoint and disjoint from input/config/vector/
every body. Validate actual objects and nonwrapping sizes. Count and byte caps
bound work; configured packet limits still apply inside each body. Local count
overload can refuse before traversing deep body views. Output capacity/property
scratch are independent caller limits. DONE/LIMIT preserve ambient errors;
writer wire/count and reader Document are unchanged on any failure. Native
decoder capacity/limit diagnostics may allocate and notify their configured
handler transiently before being discarded on LIMIT. Restoring the existing
ambient error uses checked `xrtTrySwapErrorTake` and never re-notifies it. Native
error-slot failure returns ERROR before writer output/reader Document publish.
Error/OOM cannot change confirmed typed failures. Success
attempts no XRT allocation. Caller arrays, stack, CRT/kernel storage are additional.

No callback, lock, thread, Engine or file is owned by this codec. Size/Write each
perform a native parse per PUBLISH, including string/property/PFI scans. Write
adds one body copy; Read copies no body. Moving/restoring the ambient error around
native decoder refusal adds bookkeeping. There is no extra CRC scan here:
integrity/authentication/rollback policy belongs to the journal and protected
storage. A valid altered payload can remain a valid batch without the enclosing
journal checksum; this format is not a standalone integrity envelope.

## Record meaning

Every record has nonzero logical broker-session `Epoch`, distinct from transport
generations. `Key` names a persistent nonwrapping flow/delivery identity within
the storage generation. The future applier must allocate/validate keys, correlate
stages and prevent stale completion from removing a reused MQTT identifier.
All unused fields are zero. Wall milliseconds are<=INT64_MAX; trusted clock
ordering, actual expiry, authenticated broker proof and epoch changes require
state and are not inferred by a syntax codec.

| Type | Fields and meaning |
| --- | --- |
|BIND|Data=nonempty valid UTF-8 ClientId, wall; other fields0. Covers explicit assigned-id binding after real authenticated acceptance.|
|OPEN|clean-start/session-present flag bits1/2, negotiated effective Expiry seconds, wall. Both flag bits together invalid.311 clean=>0 expiry, persistent=>UINT32_MAX.|
|END|flag1 attests to a complete client DISCONNECT and records its reason, otherwise reason0. Records effective Expiry and the actual disconnect wall. Abnormal END preserves OPEN expiry. A graceful MQTT 5 END can carry a valid expiry override, checked against the original CONNECT by the live Client. MQTT 3.1.1 preserves OPEN expiry.|
|CLEAR|reason0 admin/1 expiry/2 broker reset, wall. Clears protocol state in the future applier; it must not discard already accepted Inbox deliveries.|
|OUT_PUT|Key/id, canonical QoS1/2 wire, attempted/sent flag bits1/2. Sent implies attempted.|
|OUT_ATTEMPT|Key/id; store before the first possible native output when required.|
|OUT_PUBREL|Key/id, successful PUBREC reason0/0x10 (311 only0); retain PUBREL stage before output.|
|OUT_COMPLETE|Key/id, Flags=PUBACK/PUBREC/PUBCOMP packet type and valid reason. PUBREC here must be negative; successful PUBREC uses OUT_PUBREL.|
|IN_ACCEPT|Key, matching id or0 for QoS0, canonical PUBLISH wire. One record represents accepted payload/delivery and QoS2 identity together.|
|IN_PUBREL|Key/id, durable received-PUBREL stage.|
|IN_COMPLETE|Key/id, protocol identity completion only. Pending application payload remains independent.|
|INBOX_ACK|Key and its original acceptance Epoch, including after protocol reset. This is a durable application-completion intent, not plain memory Release.|
|IN_TRACK|Key, Epoch and the nonzero identifier of an accepted QoS 2 PUBLISH. It records receive identity without storing a delivery or any wire bytes.|

These are representations, not authorization to apply an arbitrary transition.
The future store must reject invalid epoch/key/stage/identity/clock order and
publish recovered application state only after the whole committed prefix is
verified. Subscription intent, application outbox policy, cancellation/physical
retirement and application delivery leases remain explicit integration work.

## Version1 binary layout

All integers are big-endian. A32-byte header starts with `XMQDSET\0`, u16 format1
at8, MQTT level byte4/5 at10, zero at11, u32 operation count at12, u64 total batch
bytes at16 and8 zero bytes at24. Unknown tags, future format/version, reserved
fields, truncated bodies and trailing bytes fail closed.

Each record has48 bytes followed by exact Data bytes: tag/flags/reason/zero at
0..3; u16 identifier at4; u16 header bytes=48 at6; u32 Expiry at8; u32 zero at12;
u64 Epoch at16; u64 Key at24; u64 wall at32; u64 Data length at40. The format
contains no pointers, padding, native endian integers or property descriptors.

A whole encoded batch is one journal transaction. This allows later bounded
count/byte/deadline group-commit policy without copying the entire Session.
It does **not** allow coalescing across a dependency that needs a prior durable
ACK boundary: e.g. the driver must commit accepted receive state before PUBREC,
and cannot delay that sync until a later PUBREL. The16-record complete-history
fixture tests representation/atomicity, not one-sync live QoS2 handshakes.

Packet/handshake semantics are checked against the existing native codecs and
[MQTT5.0](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html) and
[MQTT3.1.1](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html).
Protocol delivery guarantees do not attest application business effects.

## Validation

Run `python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite
xmqtt_store_delta_tests`. Separate fault/OOM/alias tests cover both protocol
levels, every fixture truncation, seeded body/header mutations and property/PFI
rules. Journal fault integration verifies zero-or-all16 operation visibility
across partial-write/sync/root publication models, wrong schema and CRC-valid
semantic damage. Native child-process cuts, an independent Python struct/packet
fixture oracle and package consumers add separate evidence. Process kill is not
physical powerloss, and WSL is not native Linux production qualification.

The [example](../examples/store_delta/README.md) builds a3-record receive-admission
transaction and optionally synchronizes/recovers a pre-published file. Pure
Size/Write/Read microbenchmarks report64 independent incoming records per batch;
they include native validation and real body copies/scans but no file/network/
queue delay. Controlled fixed-arrival/comparator and native >=72h acceptance
remain outstanding.
