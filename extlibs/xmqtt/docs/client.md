# Bounded TCP client: current vertical slice

`xmqtt_client` is developing. It implements one numeric-address TCP connection
on a running XRT Engine, MQTT 5.0 (default) or explicit 3.1.1, copied CONNECT
including basic username/password/Will fields, validated CONNACK negotiation,
QoS-0 PUBLISH send/receive, server keep-alive override, PINGREQ/PINGRESP,
abortive close and physical retirement. There is no protocol fallback/replay.

This stage does **not** complete the planned client. QoS 1/2, subscription and
identifier state, aliases, enhanced authentication, graceful DISCONNECT,
TLS/WebSocket/DNS, reconnect, expiry/recovery and durable sessions remain work.
Enhanced authentication and positive receive Topic Alias Maximum are refused
at creation. Receiving QoS 1/2 currently closes with UNSUPPORTED; it never
pretends to acknowledge or deliver those flows. SessionPresent with no local
session closes with SESSION, also for the explicit 3.1.1 mode.

## Ownership and results

Create a Client with `xmqttClientConnect`; it owns an Engine pin until Destroy.
It has one unique caller owner. Stop concurrent API entries before Destroy.
`Ready` returns an additional Future observer. Read its typed readiness status:
resolved does not imply success. The readiness result retains the CONNACK and
owns original/assigned client identity views. It has no Client/Engine reference.

Async publish first validates, sizes and encodes a caller-independent immutable
packet. ACCEPTED transfers one Future observer to the caller. Ordinary CLOSED
and OVERLOADED leave that output and the ambient error unchanged. A writeresult
OK means every native write-buffer reference retired while the transport was
open. It acknowledges no broker receipt, routing, storage or delivery. Any
prefix accepted by TCP makes delivery uncertain, including timeout/cancellation
results. Cancelling/timing out a request with an accepted prefix retires the
connection; no incomplete frame is followed by another packet or auto-replayed.
Before any accepted prefix, cancelling one request need not close the Client.

Explicit Future cancellation wakes the owner through the public Future V2
producer capability. Raw token requests are sampled on IO/deadline progress.
Dropping a Future observer does not cancel work. Timeout starts at API entry,
including validation/encoding/queueing/handshake, with no deadline extension.
Blocking Publish, Wait and Destroy reject every worker of the same Engine.

Receive messages are immutable retained copies. TakeMessage transfers one queue
reference, leaving its output unchanged when empty. Retained messages survive
Client and Engine destruction. Already queued messages remain available after
Close; Destroy releases any untaken queue references. Server DISCONNECT records
its reason in Info and closes; it is not a PUBLISH queue item.

Close is nonblocking and **abortive**: it sends no MQTT DISCONNECT and therefore
can cause the server to publish the Will. CLOSED is the logical terminal state.
Wait additionally observes producer, event, post, timer, send-buffer and native
stream retirement. It uses the original wait deadline; timeout preserves the
caller owner. Destroy requests Close and waits off-worker before releasing it.
Notifications run on the IO worker, outside the admission gate; they may Take,
submit or Close, and must not block. Notify code/User live through Destroy.

## Resource accounting

Defaults: 128 physical requests / 4 MiB wire; 512 retained objects / 16 MiB;
64 queued messages / 8 MiB; 4 MiB private bytes / 64 KiB startup wire;
1 KiB initial receive storage, 1 MiB codec packet/payload/property caps;
64 KiB read/write work, 16 KiB write chunks, 64 packets/turn; setup and PING
timeouts 5 seconds. Caller configuration may tighten these finite limits.

Requests and outgoing wire remain charged until their **last real reference**
retires, potentially after the result Future resolves. Their bound is
MaxRequests times request metadata plus MaxOutgoingBytes, excluding Core Future
and stream nodes. Independent object quota includes the readiness outcome,
CONNACK, write outcome boxes and all queued/user-retained receive messages.
Retaining results can therefore cause later admission overload or receive LIMIT;
they do not pin a Client or Engine. Queue bytes include owned message metadata,
properties and wire. Object quota uses requested allocation bytes and remains
alive until its last retained object is released.

Private bytes count Client/scratch/input/startup allocations, including the old
and new input allocation during growth. Request storage, object quota and native
stream storage are separate bounds. The one quota header, allocator overhead,
Core timer/post/Future/native queues, socket/kernel buffers and caller copies
are not private bytes. Native queues obey the configured XRT stream limits;
timer count is at most one per Client. The Client creates no thread or Engine.

Startup wire is securely wiped after negotiation/stopping **and** final native
write-reference retirement; this does not erase caller/native/kernel copies.
An advertised MQTT Maximum Packet Size must fit the local codec packet cap.
Peer violations of the advertised cap are PROTOCOL; otherwise valid packets
beyond a local bound are LIMIT. Diagnostic allocation failure does not change
that typed decision. Overfull receive queues close instead of dropping silently.

## Work/copy costs and evidence limits

Admission performs two public descriptor preflights (Size, Write) and one wire
encoding copy. Native sends borrow bounded chunks of that wire via SendRef;
PINGREQ uses a two-byte copied native send, also supporting WriteLimit=1.
Receive copies native bytes into bounded staging, then copies a complete frame
into one message allocation and rebases property/payload views; coalesced tails
can incur memmove and growth can copy staging once. There is one host-worker
protocol owner, a Client admission/snapshot gate, atomic physical references and
a separate object-quota gate. Request/deadline scans are bounded by MaxRequests.
No zero-allocation or zero-copy end-to-end claim is made.

The source fixture, independent stdlib wire peer and example are separate.
Native Windows and WSL sanitizer runs are development evidence; WSL is not the
required native Linux production qualification. Broker/version pinning,
representative throughput/P99 comparisons and native >=72h fault/mixed-load
qualification remain required. Root xmqtt and this client remain developing.

Protocol basis: [OASIS MQTT 5.0](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html)
sections 3.1, 3.2, 3.3, 3.12, 3.13, 3.14; and
[OASIS MQTT 3.1.1](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html).
Implementation is independent; no third-party MQTT source is incorporated.

Run the offline example:

```
python tools/build.py --compiler gcc --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client --no-single
```

Run modular fault/lifetime tests:

```
python tools/build.py --compiler gcc --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_tests --test test_client --no-single --no-examples --cflag=-O2
```
