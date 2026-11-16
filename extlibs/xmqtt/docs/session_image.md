# Session protocol images (developing)

`xmqtt_session_image` is a bounded checkpoint codec over the existing passive
Session. It uses public XRT CRC/memory/synchronization APIs and both MQTT codecs;
it adds no file, thread, timer or shared runtime. This component does not yet
implement the durable Client/store: incoming application payloads, transactional
Inbox recovery, journal ordering, asynchronous sync barriers and crash recovery
must still be completed before claiming durable MQTT delivery.

Supply a nonempty opaque broker/security-principal Binding, explicit monotonic
seconds and trusted nonnegative UTC milliseconds. Config defaults to a16MiB image
cap. A successful restore must match Binding, ClientId and MQTT version exactly.
No credentials or native TLS session are serialized. CRC detects accidental
corruption; it does not authenticate storage or prevent replay of an older image.
Protect files and enforce storage-generation/rollback policy in the future store.

Size/Write require the sole passive protocol owner, reject a live Client claim,
and hold the Session gate for one coherent cut. They make no successful-path heap
allocation. Size scans active metadata; Write copies each canonical packet once
and scans the output for CRC. These are cold operations with real work and gate
duration, not network-callback work or a reservation across calls. Separate Size
and Write observe separate cuts if the owner mutates state between calls.

Only active outgoing QoS1/2 snapshots, original identifiers/attempt flags and
PUBLISH/PUBREL stages, committed incoming QoS2 identifiers, generation and copied
ClientId are saved. Prepared/invisible receives and completed externally held
references are excluded. A restore owns fresh allocations under Session count/
byte/codec limits; it borrows neither the image nor configuration after return.
Physical old references remain tied to their original Session owner.
`xmqttSessionImageConfigInit`, `xmqttSessionImageSize`, `xmqttSessionImageWrite`
and `xmqttSessionImageRestore` expose these operations. Explicit
`xmqttSessionImageRestoreAssigned` additionally accepts a MQTT5 config with empty
ClientId and copies the bound image identity. Normal Restore never adopts it
implicitly. Use `xmqttSessionClientId` after assigned restore for the next CONNECT;
bind each logical storage slot separately. The caller still joins old transports.

Closed finite expiry is rounded down to whole milliseconds and reduced by wall
downtime. Expired input still receives full packet validation before publishing
an empty Session with RESET. Backward wall time is a typed STATE_ERROR. Fractional
remaining seconds are rebased against the new monotonic epoch without subtracting
from a potentially huge clock. The selected private expiry representation is
double; the unselected Session keeps its original integer field. Exact sizes and
actual allocation costs are measured in the stage evidence.

A snapshot taken while connected cannot reveal the later disconnect time.
Restoration retains that candidate locally until a fresh authenticated CONNACK
decides SessionPresent; it does not guess an expiry start from the checkpoint
timestamp. Every restored Session is disconnected and still requires the normal
Open/negotiation/replay process. Clean/nonpersistent sessions serialize no retained
obligations. Incoming IDs alone do not prove payload acceptance survived a crash.

The semantics use [MQTT5 session state and expiry](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html)
and [MQTT3.1.1 state/retry rules](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/errata01/os/mqtt-v3.1.1-errata01-os-complete.html).
The following private image format is independently defined, not an MQTT wire
packet or an interoperable broker storage format.

Version1 uses big-endian integers, an exact64-byte header and a final four-byte
CRC-32/ISO-HDLC over all preceding bytes. Header offsets:

| Offset | Field |
| --- | --- |
|0|8-byte `XMQSIMG\0` magic|
|8,10|uint16 format1, header64|
|12,13,14|MQTT version4/5, flags STATE1/LIVE2/INFINITE4, uint16 reserved0|
|16,24,32,40|uint64 total bytes, saved wall ms, remaining ms (or UINT64_MAX), generation|
|48,52|uint32 outgoing and incoming counts|
|56,58,60,62|uint16 ClientId bytes, Binding bytes, next identifier, reserved0|

Binding then ClientId follow. Each ordered outgoing entry has a12-byte prefix:
stage byte0/1, attempted1/sent2 flags, accepted PUBREC reason, reserved0,
uint32 canonical PUBLISH wire length and uint32 property count; then that wire.
Topic Alias/DUP/non-PUBLISH entries, inconsistent stages/counts, duplicate ids,
bad packets, unsupported versions, trailing data and corrupt CRC are rejected.
Incoming entries are sorted uint16 nonzero id plus state2/3. Boundaries and CRC
are checked before state allocation; all partial construction is freed on failure.
The caller supplies storage for the image in addition to restored Session bytes.

Run separate codec/fault and actual native Client recovery suites:

```text
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_session_image_tests --cflag=-O2
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_session_image_client_tests --cflag=-O2
```

The tests cover live/offline/clean/infinite/finite/expired state, both versions,
all three outgoing ACK cuts, identity/clock/byte/count/codec caps, every truncated
prefix and one-bit corruption per byte, CRC-valid malformed fixtures and10000
seeded mutations, real OOM and budget1 allocation-free Size/Write, property/alias
canonicalization, selected-frame and old-owner lifetimes, and assigned identity.
The image gate refuses a real Client claim even after physical Wait, until
Client Destroy. Public operations remain ordinary Session/Client APIs.

An independent Python struct/zlib oracle validates every field and literal MQTT
wire; actual Windows/WSL modular/single/static/shared processes write equal bytes
and restore opposite-platform images. This file interchange does not prove
transactional sync, crash/power-loss recovery or payload/Inbox durability.
`bench_session_image.c COUNT MODE INSTRUMENT` measures Size0, Write1 and
Restore/Info/Destroy2 over64 snapshots with256-byte payloads. It reports checksum,
throughput, caller-observed percentiles, process CPU and optional XRT heap costs.
Clock calls, CRC, copies, gate duration, decoder work and allocator bookkeeping
remain real; OS/CRT costs are incomplete. Shared-host diagnostics and any quality
failures are distinct from controlled performance/production acceptance.
