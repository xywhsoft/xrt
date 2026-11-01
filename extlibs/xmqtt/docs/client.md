# Bounded TCP client: current vertical slice

`xmqtt_client` is developing. It implements one numeric-address TCP connection
on a running XRT Engine, MQTT 5.0 (default) or explicit 3.1.1, copied CONNECT
including basic username/password/Will fields, validated CONNACK negotiation,
QoS-0/1 PUBLISH send/receive, correlated publishing PUBACK results,
server keep-alive override, PINGREQ/PINGRESP,
abortive close and physical retirement. There is no protocol fallback/replay.

This stage does **not** complete the planned client. QoS 2, subscription and
shared outgoing identifier state, aliases, enhanced authentication, graceful DISCONNECT,
TLS/WebSocket/DNS, reconnect, expiry/recovery and durable sessions remain work.
Enhanced authentication and positive receive Topic Alias Maximum are refused
at creation. Receiving QoS 2 currently closes with UNSUPPORTED; it never
pretends to acknowledge or deliver that flow. SessionPresent with no local
session closes with SESSION, also for the explicit 3.1.1 mode.

## Ownership and results

Create a Client with `xmqttClientConnect`; it owns an Engine pin until Destroy.
It has one unique caller owner. Stop concurrent API entries before Destroy.
`Ready` returns an additional Future observer. Read its typed readiness status:
resolved does not imply success. The readiness result retains the CONNACK and
owns original/assigned client identity views. It has no Client/Engine reference.

Async publish first validates, sizes and encodes a caller-independent immutable
packet. ACCEPTED transfers one Future observer to the caller. Ordinary CLOSED
and OVERLOADED leave that output and the ambient error unchanged. A QoS-0 result
OK means every native write-buffer reference retired while the transport was
open. It acknowledges no broker receipt, routing, storage or delivery. QoS-1
completion and decision metadata follow the sender contract below. Any
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

## QoS-1 sender and acknowledgement ownership

Publish accepts fresh QoS-0/1 descriptors (DUP=0). For QoS 1, Identifier=0
assigns an unused nonzero id; an explicit id reserves that value. Concurrent
attempts to reserve the same id return normal OVERLOADED, preserving the Future
output and ambient error. No caller packet is modified. An id remains reserved
until its real request references retire, possibly after Future completion.
No id is silently reused for a still-unacknowledged publication.

One bitmap/hash table is allocated lazily on the first QoS-1 attempt, charged
to private bytes and retained through Destroy. A later allocation/admission
failure can leave that cold table cached. The bitmap is 8192 bytes; bucket
count is the next power of two at least twice MaxRequests. Worker-owned hash
chains correlate ACKs, and a doubly linked active list removes completions
without an additional request scan. Lookup collisions remain bounded by the
configured request count; deadline and send scans are still bounded scans.
QoS-0 allocates no table and performs no extra descriptor copy. QoS-1 copies
one descriptor to substitute its assigned id, retaining the two independent
public Size/Write preflights and one wire encoding copy.

MQTT 5 Receive Maximum limits started, unacknowledged QoS-1 publications.
Only the first successfully accepted native prefix consumes credit. A matching
PUBACK returns it, including negative reason codes. Queued QoS-1 packets wait
at zero credit; QoS-0 and PING traffic may proceed. A partial frame is completed
before another control packet is inserted. MQTT 3.1.1 uses local request bounds
without a negotiated Receive Maximum. No retransmission occurs on the same
connection; reconnect/session/durable replay is separate unfinished work.

QoS-1 success waits for a matching PUBACK, not native write retirement. The
result appends Identifier/Reason/Puback after its original four-field prefix;
the latter's offsets are unchanged. PEER_ACCEPTED indicates a reason below
0x80 (including no matching subscribers); PEER_REJECTED indicates a negative
decision and Status=REJECTED. Neither promises subscriber delivery or durability.
The immutable ACK retains all reason/User Properties and may be separately
retained through the message API. Results/ACKs remain independent of Client
and Engine. If ACK metadata cannot fit the object budget, close with LIMIT or
MEMORY while retaining its scalar reason and accepted/rejected decision.

CONNECT Request Problem Information=0 is retained as connection state after
startup wire retirement. PUBACK Reason String/User Properties then cause a
protocol close; a bare PUBACK remains valid. CONNACK, PUBLISH and DISCONNECT
retain their permitted diagnostics behavior.

Unknown, duplicate, unsent or wrong-kind acknowledgements on this connection
close with PROTOCOL; they cannot complete an unrelated Future or mint credit.
Timeout/cancellation while awaiting PUBACK closes the connection with uncertain
delivery. Existing native-write, cancellation, quota and physical-close rules
still apply. Incoming QoS-1 PUBLISH follows the receiver contract below.

## QoS-1 receiver and ordered acknowledgements

A validated incoming QoS-1 PUBLISH is accepted only after an immutable owned
message enters the finite local receive queue. The client then queues a minimal
four-byte success PUBACK with the same id. ACKs retain arrival order, independent
of application Take/Retain/Release. Success means volatile local acceptance;
it does not prove application processing, durable storage or exactly-once effects.
A failed queue/object/private allocation produces no success ACK for that frame.
Accepted queued messages remain available after transport failure, including a
failure before their ACK could reach the peer. Such messages may be redelivered.

A lazy private ring stores at most min(MaxMessages, CONNECT Receive Maximum)
identifiers (3.1.1 uses the default 65535). Its requested bytes are one header
plus two bytes per entry, charged to MaxPrivateBytes through Destroy, including
cached empty storage. No receive-id bitmap or per-ACK library object is allocated.
QoS-0 receive creates no ring. The native bounded copied-send queue separately
owns accepted ACK chunks; it obeys Stream write limits and physical retirement.

A local full ACK ring pauses parsing at the already buffered next QoS-1 frame;
its bytes are not discarded or acknowledged. ACK progress wakes parsing without
a busy repost while native writes are blocked. Exceeding the advertised MQTT 5
receive credit closes with PROTOCOL. The credit is returned only once all four
PUBACK bytes are accepted into the ordered native stream. Error exits currently
remain abortive: MQTT 5 diagnostic DISCONNECT (including reason 0x93), graceful
close/drain and application-controlled/durable acknowledgements remain pending.
These are explicit gaps in full client qualification, not protocol-complete
production acceptance.

A partial outgoing PUBLISH, PING or PUBACK is finished before another packet can
start. A pending receive ACK takes priority over new publications, including
when outgoing QoS-1 send credit is exhausted. Send and receive identifiers are
independent: an incoming id can equal a pending outgoing id. QoS-1 does not promise
duplicate suppression. A PUBLISH after the earlier PUBACK is a new queue item,
even with the same identifier and DUP=1; original flags/properties remain visible.

The offline example asserts both directions and waits until its fixture observes
the receive ACK before closing. The sender-only benchmark leaves fixture echo
mode disabled. `bench_client_receive.c` separately measures a combined QoS-1
publish/echo exchange: both directions' ACKs, one local fixture, two IO workers,
one observer, window32/payload32, and first cold sender/receiver allocations.
Latency ends at application observation of both the outgoing PUBACK result and
the corresponding incoming message; throughput additionally waits for the peer
to observe every receiver PUBACK. It includes polling, scheduling and fixture
costs. Instrumented variants include client AND peer XRT block traffic, not all
system allocations or RSS. It is not a receiver-only benchmark or broker parity.

`bench_client_qos1.c` supplies a separate loopback workload: one client, a local
healthy peer on another host worker, window 32, 32-byte payload, application
observation of PUBACK completion, P50/P99/max latency and throughput. Timing
starts after CONNECT/CONNACK and stops before teardown; the first lazy id-table
allocation is included. Allocation variants instrument the client **and peer**,
reporting XRT block counts/bytes rather than RSS or all system allocations.
Uninstrumented and instrumented variants are separate. Host polling and timer
sampling, peer decoding/ACK copies and scheduling are included. This fixture
is neither an independent broker nor a pinned comparator; no performance
acceptance is inferred from one successful run or a smoke result.

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
