# Bounded TCP client: current vertical slice

`xmqtt_client` is developing. It implements one numeric-address TCP connection
on a running XRT Engine, MQTT 5.0 (default) or explicit 3.1.1, copied CONNECT
including basic username/password/Will fields, validated CONNACK negotiation,
QoS-0/1/2 PUBLISH send/receive, SUBSCRIBE/UNSUBSCRIBE, correlated typed ACK results,
server keep-alive override, PINGREQ/PINGRESP,
application graceful DISCONNECT, abortive Close, bounded automatic MQTT5 error DISCONNECT and physical
retirement. There is no protocol fallback/replay.

This stage does **not** complete the planned client. Subscription recovery,
WebSocket, reconnect, expiry/recovery and durable sessions remain work.
Optional [Dial](client_dial.md) and [TLS](client_tls.md) leaves provide hostname/
prepared-address setup and verified TLS on the same bounded protocol owner.
The base factory refuses enhanced authentication; use [client_auth](client_auth.md)
for the optional authenticated factory. Positive receive Topic Alias
Maximum enables bounded incoming aliases. QoS-2 receives use the volatile current-connection contract below.
SessionPresent with no local
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

`xmqttClientDisconnectAsync(Client, Packet, Timeout, &Future)` requests normal
application shutdown after an accepted CONNACK; NULL Packet means reason 0.
`xmqttClientDisconnect` is the blocking form and rejects every same-Engine worker.
A caller may submit the asynchronous form from Notify. Accepted shutdown stops
new PUBLISH/disconnect admissions immediately. It drains prior accepted PUBLISH
frames, QoS-0 native write completion, QoS-1 PUBACK/QoS-2 PUBCOMP outcomes and partial control
frames before appending the final DISCONNECT to the native queue. No MQTT packet
follows it, and the client initiates TCP close. Queued receive messages remain
available until Take/Destroy. It does not wait for a broker acknowledgement of
DISCONNECT: none exists.

Each prior PUBLISH keeps its own deadline, cancellation and capability outcome.
An unsent request may expire/be rejected without making the connection unhealthy;
shutdown can then complete normally. Inspect each PUBLISH Future: graceful OK
does not mean all prior publishes succeeded or were delivered.

The descriptor and properties are encoded/copied before return. MQTT 5 reason,
Reason String, repeated User Properties and Session Expiry obey the existing
codec and negotiated limits. In particular an originally zero CONNECT expiry
cannot become nonzero at DISCONNECT, including after a CONNACK override. MQTT
3.1.1 uses the two-byte normal packet and rejects v5 properties/nonzero reason.
Oversized packets are rejected rather than silently stripping caller properties.
The finite Timeout (>=0 milliseconds) starts at entry, includes validation,
encoding, prior-request drain and native queue close, and never restarts. Zero
is an immediately expired operation. Its explicit Future cancellation, raw-token
sampling, explicit Close, first protocol/transport/resource error or deadline
interrupts shutdown and aborts; it does not reopen admissions. No incomplete
frame is followed by a diagnostic or another MQTT packet. Bytes already accepted
into native output cannot be retracted.

One shutdown uses one slot and its wire in the existing request/outgoing quotas,
plus one typed-result box in the shared object quota. A full quota can return
OVERLOADED while leaving the connection open; the caller may retry after work
retires or choose abortive Close. Invalid arguments, pre-CONNACK state and peer
limits return ERROR; outputs change only on ACCEPTED. Duplicate/closed admission
is CLOSED and normal CLOSED/OVERLOADED preserve the ambient error. Accepted and
Completed counters include the shutdown request and its physical reservation.

Its retained `xmqttwriteresult` has Identifier=0, Puback=NULL and Reason equal to
the requested reason. OK means the complete native write queue drained and TCP
closed normally. Delivery remains MAY_HAVE_REACHED_PEER for any accepted prefix,
even on OK; neither OK nor DisconnectBytes proves broker processing, delivery or
Will suppression. NOT_SENT means no native prefix was accepted. Status/Cause
preserve the first actual failure; a normal shutdown intent is not a failure.
While State=CLOSING, CloseStatus=OK is provisional. The Future is the typed local
outcome; WaitFor also observes every actual producer/native/callback tail.
Config/user code must stay alive through Destroy and all concurrent API entries
and wait borrowers must finish before destroying the caller owner.

The protocol basis is OASIS [MQTT 5.0 section 3.14](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html)
and [MQTT 3.1.1 section 3.14](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html).
Only receipt of a normal DISCONNECT gives the server the specified Will behavior;
reason 0x04 in MQTT 5 explicitly requests disconnection with the Will instead.
Independent wire fixtures verify the bytes/order and client EOF; pinned broker
Will/session interoperability remains separate required qualification.

Application Close is nonblocking and **abortive**: it initiates no MQTT DISCONNECT
and can cause the server to publish the Will. It also interrupts an automatic
error drain; bytes already accepted by TCP cannot be retracted. CLOSED is the
logical terminal state.
Wait additionally observes producer, event, post, timer, send-buffer and native
stream retirement. It uses completion notifications without polling, waiter-node
allocation or ownership graph inspection. The Client and native phases share the
remaining budget from one original monotonic deadline; the native phase rounds
that remainder to the public millisecond unit. Timeout preserves the caller
owner. Multiple waiters borrow that same stable owner: stop all API entries and
finish all borrowers before Destroy. Destroy requests Close and waits off-worker
before releasing it. Repeated Close on CLOSED posts no additional work.

`xmqttClientPollRetired` is the nonwaiting query for an IO-driven controller.
It may run on either Engine worker. OK observes the same Client/reference/native
completion boundary; TIMEOUT means active references, incomplete logical closure
or an immediately busy predicate mutex. ERROR reports invalid input or a real
synchronization failure. The query never closes or consumes its unique owner.
It tries the existing Client retirement mutex, releases it before trying the
admission gate, then queries the native Stream. A cancelled Dial that never
created a Stream needs only the Client predicates. This includes the final
reference notification tails, rather than inferring completion from CLOSED.

Normal OK/TIMEOUT allocate no requested heap, timer or waiter and preserve the
exact ambient error without redelivery. This is a query, not a lock-free or
real-time destructor. Keep a stable owner throughout each call and exclude other
API entries before treating OK as final. Wait and Destroy still reject all
same-Engine workers; transfer cleanup to an external owner after readiness.
No Client fields, send-path work or reference-drop operations are added.

Notifications run on the IO worker, outside the admission gate; they may Take,
submit or Close, and must not block. Notify code/User live through Destroy.

The client selects Core `net_tcp_retire` and owns a separate embedded mutex and
condition for its actual reference retirement. The admission gate is never taken
under that wait lock, since a failed post can drop its reference under the gate.
Ordinary drops above two use a load/CAS loop; the terminal 2->1 publication locks
and broadcasts before any waiting owner can release the Client. The native phase
uses `xrtNetStreamWaitRetired`, including callback tails after logical CLOSED.
The tests deliberately hold a CLOSED notification and borrow one owner from
multiple threads; no waiter may return until that callback actually exits.

## Resource accounting

Retirement synchronization adds 40 bytes to the private Client on tested native
Windows x64 (1088 -> 1128) and 176 bytes on tested WSL x64 (1160 -> 1336). Selecting
the Core module also adds 40/176 bytes per native Stream. Client storage is
charged to MaxPrivateBytes; Core storage is accounted separately. Public
config/info/ready/packet layouts stay 272/160/136/168 bytes; the sender stage
expanded write results to 64 bytes. Mutex and
condition initialization/destruction, terminal locking/broadcasts, and ordinary
reference load/CAS work are real costs. These changes establish neither zero
overhead nor steady-message throughput parity; those require measurements.

Application shutdown adds one private pointer and four flags (16 bytes on tested
Windows/WSL x64): Client sizeof became 1144/1352, charged to MaxPrivateBytes.
The QoS-2 receiver adds another private pointer (current Client 1152/1360).
The request metadata is reused, without another runtime or per-message object.
Each admitted shutdown still allocates its owned request/wire, Future/token and
quota result, and uses existing Core timer/send nodes. Ordinary workloads also
execute the added state/deadline branches; no zero-cost or throughput parity
claim is established by the functional tests.

Defaults: 128 physical requests / 4 MiB wire; 512 retained objects / 16 MiB;
64 queued messages / 8 MiB; 4 MiB private bytes / 64 KiB startup wire;
1 KiB initial receive storage, 1 MiB codec packet/payload/property caps;
64 KiB read/write work, 16 KiB write chunks, 64 packets/turn; setup and PING
timeouts 5 seconds, including positive CloseTimeout for diagnostic queue drain.
Caller configuration may tighten these finite limits.

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

Publish accepts fresh QoS-0/1/2 descriptors (DUP=0). For QoS 1 or 2, Identifier=0
assigns an unused nonzero id; an explicit id reserves that value. Concurrent
attempts to reserve the same id return normal OVERLOADED, preserving the Future
output and ambient error. No caller packet is modified. An id remains reserved
until its real request references retire, possibly after Future completion.
No id is silently reused for a still-unacknowledged publication.

One bitmap/hash table is allocated lazily on the first QoS-1/2 attempt, charged
to private bytes and retained through Destroy. A later allocation/admission
failure can leave that cold table cached. The bitmap is 8192 bytes; bucket
count is the next power of two at least twice MaxRequests. Worker-owned hash
chains correlate ACKs, and a doubly linked active list removes completions
without an additional request scan. Lookup collisions remain bounded by the
configured request count; deadline and send scans are still bounded scans.
QoS-0 allocates no table and performs no extra descriptor copy. QoS-1/2 copies
one descriptor to substitute its assigned id, retaining the two independent
public Size/Write preflights and one wire encoding copy.

MQTT 5 Receive Maximum limits started, unacknowledged QoS-1/2 publications.
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

Unknown, duplicate, unsent or wrong-kind PUBACKs on this connection
close with PROTOCOL; they cannot complete an unrelated Future or mint credit.
Timeout/cancellation while awaiting PUBACK closes the connection with uncertain
delivery. Existing native-write, cancellation, quota and physical-close rules
still apply. Incoming QoS-1 PUBLISH follows the receiver contract below.

## QoS-2 sender on the current connection

A fresh QoS-2 PUBLISH uses the same finite request/wire quota, lazy identifier
bitmap/hash table, immutable encoded input and physical id retirement as QoS 1.
A successful PUBREC retains its reason/properties, then schedules the four-byte
PUBREL with the same id. The sender holds Receive Maximum credit until PUBCOMP
or a negative PUBREC; positive PUBREC alone cannot start another credit-limited
publication. QoS 0 and bounded control traffic can continue. MQTT 3.1.1 uses the
same four-phase handshake without MQTT 5 reason/properties or negotiated credit.

Completion requires a corresponding PUBCOMP after a complete PUBREL. A negative
PUBREC terminates with REJECTED and sends no new PUBREL. A negative PUBCOMP also
returns REJECTED: id-not-found (0x92) describes a protocol/state mismatch and
cannot prove the message was never delivered. PEER_ACCEPTED denotes final
protocol success, not subscriber processing, durable storage or recovery after
crashes. Durable/reconnected sessions remain unsupported.

Duplicate successful PUBREC coalesces while PUBREL is unsent/partial; after a
complete PUBREL it schedules the same immutable frame again. It never replays
the original PUBLISH and introduces no timer-based retransmission. If a repeated
PUBREL is partial when a terminal ACK arrives, its remaining bytes are finished
before another frame or graceful DISCONNECT. Native references retain its
request/inline frame; physical id reuse waits for all actual references.
Unknown PUBREC, an ACK for an unsent flow and wrong-kind ACKs close with PROTOCOL.
Unknown/repeated PUBCOMP or PUBCOMP before that flow's first complete PUBREL is
ignored, without completing a Future or returning credit. There is no stored
session from which to recover an unknown PUBREC. These are current-connection
policies; they do not establish session recovery.

`xmqttwriteresult` keeps its original 40-byte prefix (including Puback at offset
32) and appends Pubrec/Pubcomp plus PubrecReason/PubrecReceived/PubcompReceived.
Its tested x64 size becomes 64 bytes; callers must rebuild. Puback remains NULL
for QoS 2. Pubrec is the first successful or terminating negative receipt;
Reason is the final ACK reason when available. Both immutable message owners
may be retained separately and survive Client/Engine destruction. Scalar flags
and reasons are stored before copying metadata: a LIMIT/MEMORY outcome may
therefore retain a completed protocol decision even when a message pointer is
NULL. Request Problem Information=0 rejects forbidden ACK properties.
AcceptedBytes still counts only the original PUBLISH, excluding PUBREL bytes.
Timeout/cancellation/Close after any accepted prefix retires the connection;
receipt alone leaves delivery uncertain. Graceful shutdown waits for the final
ACK (or each request's terminal failure), then its own final packet/native close.

The inline release frame/state increases private request metadata for every
request, including QoS 0/1 and shutdown. Appended result fields add 24 requested
bytes to every write-result box, also for those existing operations; allocator
bucket/RSS effects are not inferred from struct sizes. Successful PUBREC and
PUBCOMP each consume one owned message in the existing object budget.
Duplicate positive receipts create no new ACK copy. The original PUBLISH wire
remains charged until real request retirement, even after PUBREC, so no payload
memory reduction or zero-cost/performance claim is made for this stage.

The [offline QoS-2 example](../examples/qos2_send/main.c) runs in the normal
`xmqtt_client` example suite. It checks both ACK owners, graceful final bytes/EOF
and physical cleanup. The separate stdlib peer covers both protocol versions,
window/negative reasons, duplicate/late/premature ACKs, byte-sized writes, phase
timeout/cancellation/close, metadata quotas and retained properties. Reproduce
its single implementation on Windows GCC (use `-lm -pthread` instead of
`-lws2_32` on Linux):

```text
gcc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude -Iextlibs/xmqtt/include extlibs/xmqtt/tests/interop/qos2_single_probe.c -lws2_32 -o out/qos2_probe.exe
python extlibs/xmqtt/tests/interop/run_qos2_wire.py --probe out/qos2_probe.exe --report out/qos2-wire.json
```

Create `out/` first if absent. The peer binds ephemeral loopback ports only;
success prints each case and writes `complete=true`. It closes sockets, joins
threads and needs no credentials/broker. Generated executable/report files can
be removed after inspection. This is functional evidence, not a benchmark.
The protocol basis is OASIS MQTT 5.0/3.1.1 sections 4.3.3 and MQTT 5.0 section 4.9.

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
cached empty storage. The QoS-1 ring allocates no receive-id bitmap or per-ACK library object.
QoS-0 receive creates no ring. The native bounded copied-send queue separately
owns accepted ACK chunks; it obeys Stream write limits and physical retirement.

A local full ACK ring pauses parsing at the already buffered next QoS-1 frame;
its bytes are not discarded or acknowledged. ACK progress wakes parsing without
a busy repost while native writes are blocked. Exceeding the advertised MQTT 5
receive credit closes with PROTOCOL. The credit is returned only once all four
PUBACK bytes are accepted into the ordered native stream. MQTT5 protocol errors after accepted negotiation now follow the bounded
diagnostic-close contract below, including Receive Maximum reason 0x93. Application-controlled/durable acknowledgements remain
pending; full client/production acceptance is not claimed.

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

## QoS-2 receiver on the current connection

A new incoming QoS-2 PUBLISH is copied into one owned message and enters the
bounded local queue before success PUBREC is queued. This accepts volatile
ownership: Notify/Take may expose it before PUBREL, independent of application
processing. Its incoming id stays active through the complete PUBCOMP frame
being accepted into ordered native output. Application Take/Release does not
return that protocol credit. QoS-1 pending PUBACKs and active QoS-2 ids share
CONNECT Receive Maximum. Exceeding it closes with PROTOCOL/reason 0x93. Incoming
and outgoing identifier sets are independent, including identical active ids.

While that id awaits PUBREL, a duplicate PUBLISH repeats/coalesces PUBREC without
another message allocation, queue insertion or Received increment. DUP=0 versus
DUP=1 does not determine duplicate suppression. The original owned packet keeps
its flags/payload/properties. A valid PUBREL queues PUBCOMP behind earlier ACKs;
even an early release cannot interleave or discard a pending PUBREC. No second
message is delivered at PUBREL. Queued duplicate ACKs coalesce. Repeated/unknown
PUBREL receives PUBCOMP with reason 0x92 in MQTT 5, or minimal success in 3.1.1,
without any application delivery or extra receive credit. A known id releases
normally even if PUBREL carries reason 0x92; an already exposed volatile message
cannot be retracted. Forbidden PUBREL properties under Request Problem
Information=0 close with PROTOCOL.

Once PUBCOMP is fully accepted, a subsequent PUBLISH with that id is a new
exchange, including DUP=1. Reusing an active QoS-2 id for QoS 1, or submitting
another PUBLISH after its PUBREL but before completion, closes with PROTOCOL.
No session restoration is attempted. These rules establish one local enqueue
per accepted QoS-2 exchange on this connection. They do not establish durable
or business exactly-once effects, delivery across crashes or deduplication across
connections. Retained messages still survive Client/Engine destruction.

The first QoS-2 PUBLISH or unknown PUBREL lazily allocates a private table. It
contains a 65536-byte direct-index state array, fixed bookkeeping and
2*min(MaxMessages,ReceiveMaximum) four-byte ACK slots. Requested bytes on tested
x64 are 65576 + 8*min(MaxMessages,ReceiveMaximum), charged to MaxPrivateBytes
and retained through Destroy, even when empty. A small private quota can reject
this first use with LIMIT. There are no per-id state/message references in this
table: incoming flow count is bounded by Receive Maximum (at most 65535), while
the application queue and retained objects keep their existing independent
count/byte limits. Therefore taking messages can allow more active exchanges
than MaxMessages, without unbounded payload retention by the flow state.

State lookup and duplicate coalescing use direct id indexing. A new QoS-2 id
also scans the bounded pending QoS-1 ACK ring to reject a cross-QoS id collision.
At complete frame boundaries, ready QoS-1 and QoS-2 ACK classes alternate;
a replenished QoS-1 ring cannot monopolize a small write budget. Control queue
exhaustion pauses at the buffered packet and resumes on ACK progress; it never waits for an application Take to release protocol credit.
PUBREC/PUBCOMP copies into the existing native bounded send queue, which may
allocate buffers; duplicate handling is not a claim of allocation-free native
IO. Byte/chunk/packet work budgets bound each turn. All partial PUBLISH/PUBREL/
PING/PUBACK/PUBREC/PUBCOMP frames finish before another frame starts. Partial
receive ACKs also prevent appending a diagnostic inside a packet.

Accepted application DISCONNECT waits for currently accepted incoming exchanges
to reach PUBCOMP and for queued control frames, with the same finite overall
shutdown deadline. Missing PUBREL can time out that shutdown; cancel/Close aborts
it. The failure path collects already accepted submissions again before retiring
the shutdown request, including submissions made during the current worker
turn. A gated concurrency test verifies their Futures cannot be stranded.
This does not impose a retransmission timer or silently drop live flow state
while the connection remains open. Messages accepted before an interrupted ACK
remain available, so reconnect redelivery is still possible in future session
modes. Persistence and application-controlled ACK policies remain separate work.

The [offline receiver example](../examples/qos2_receive/main.c) runs with the
ordinary client suite. It verifies an owned incoming message, the peer's PUBCOMP
observation, graceful EOF, physical cleanup and retained payload validity. The
separate independent wire peer covers 66 cases across both protocol versions,
including direct-id boundaries, duplicates/reuse, early/unknown/negative release,
shared windows, control backpressure, partial/duplex traffic, limits, reentrant
Close and graceful interruption. Unit tests additionally inject real allocation
failures before state allocation and between state/message allocation. Run the
single implementation on Windows GCC (substitute `-lm -pthread` on Linux):

```text
gcc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude -Iextlibs/xmqtt/include extlibs/xmqtt/tests/interop/qos2_receive_single_probe.c -lws2_32 -o out/qos2_receive_probe.exe
python extlibs/xmqtt/tests/interop/run_qos2_receive_wire.py --probe out/qos2_receive_probe.exe --report out/qos2-receive-wire.json
```

Create `out/` if absent. No broker/credentials are required; ephemeral loopback
sockets and threads are closed/joined. A successful report has `complete=true`.
Generated binary/report files can be removed after inspection. Functional cases
and struct sizes do not establish throughput/P99, pinned-broker interoperability,
native Linux qualification or >=72h fault/mixed-load acceptance.
The protocol basis is OASIS MQTT 5.0/3.1.1 section 4.3.3, MQTT 5.0 sections 3.7,
4.6 and 4.9, and their Packet Identifier/Request Problem Information rules.

## Automatic MQTT5 diagnostic close

After accepted MQTT5 negotiation, protocol failures select a final minimal
three-byte DISCONNECT: malformed framing/codec input 0x81, unexpected protocol
state 0x82, receive-window violation 0x93, invalid receive alias 0x94, or an
advertised packet-size violation 0x95. No Reason String/User Property, session
mutation or extra retained message is allocated for this diagnostic. MQTT 3.1.1,
pre-readiness failures, server DISCONNECT, local resource/transport failure and
unsupported unfinished flows retain abortive behavior. No server-only reason
code is emitted by this client. This does not implement normal reason-0 graceful
application disconnection or complete all remaining protocol state.

The first typed CloseStatus/Cause wins. New admissions stop immediately. Fully
accepted native output frames may drain ahead of the final diagnostic; a partial
PUBLISH, PING or PUBACK instead forces abort, preventing control-byte insertion
inside that frame. The peer packet cap must allow all three bytes. One-byte
write limits and the configured per-turn byte/chunk limits are respected. Native
queue pressure waits for real low-water/drain events; no busy repost at zero
writability is needed. The copied-send API owns each accepted diagnostic chunk.
After the complete diagnostic is queued, Core Close stops reading and drains its
output; no further MQTT packet is sent.

A single host timer bounds this phase by positive CloseTimeout, default 5000ms,
from the diagnostic-close attempt through native queue retirement. The deadline
is monotonic and never extended by progress. Timer or copied-send allocation
failure, peer EOF/reset, explicit application Close or deadline expiry falls back
to abort without changing the original typed cause. Close timeout is evaluated
on owner progress; it cannot preempt a blocked user callback or OS scheduling
stall. Callbacks remain required to be nonblocking. CLOSING is notified once on
initial stop, outside the gate, so a reentrant Close can interrupt before further
diagnostic output. CLOSED still follows terminal request work, and Wait checks
actual timer/post/event/send/native ownership retirement.

Info.LocalReason is the intended local diagnostic reason; zero means no such
attempt was selected. DrainStarted means the finite drain timer was installed.
DisconnectBytes counts only bytes accepted into native output (0..3), never peer
receipt. DrainTimedOut records deadline fallback independently of CloseStatus.
A native write result may already be OK before an unread peer protocol error is
processed; that OK observes local buffer retirement and does not establish peer
receipt or business success. These guarantees apply equally when the kernel has
buffered output beyond the library's native queue.

This developing API appends CloseTimeout and diagnostic Info fields. On the
current Windows/Linux x64 layouts config grows 264 to 272 and Info 144 to 160
bytes; original prefixes remain at their prior offsets. Callers must rebuild:
this is not binary compatibility with the older structure sizes. Feature/module
selection keeps each new public layout identical. Ready/write/packet layouts are
unchanged. The diagnostic uses stack bytes plus existing Core timer/send nodes,
with no new worker or shared runtime; those Core allocations are outside private
bytes and may fail normally. Fault tests disable the public Engine node cache to
exercise cold timer allocation as well as copied-send allocation failure.

The separate error example demonstrates peer-observed reason 0x81 and full
cleanup. `bench_client_disconnect.c` measures complete connection/error/physical
retirement cycles on a persistent local Engine: client construction, handshake,
malformed peer header, diagnostic (MQTT5) or abort (311), observer waits, client
and peer stream destruction. Per-cycle latency stops after physical Client Wait;
cycle throughput also includes destruction/reuse. Engine creation/listener setup,
latency-array allocation/sorting and final Engine teardown are excluded. First
cache use and client AND fixture XRT allocations are included. Public millisecond
observer waits affect these timings. This is not isolated DISCONNECT speed,
receiver throughput, a pinned broker comparator or production acceptance.

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


The same offline command runs `examples/graceful/main.c`, which sends a QoS-1
message, observes its PUBACK, requests a normal DISCONNECT, verifies the peer's
final bytes/EOF and releases all Client/Stream/Listener/Engine owners. The typed
shutdown result remains valid after Client and Engine destruction.


To reproduce the independent graceful wire cases with a single implementation
(Windows GCC; replace `-lws2_32` by `-lm -pthread` on Linux):

```text
python -c "from pathlib import Path; Path('out').mkdir(exist_ok=True)"
gcc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude -Iextlibs/xmqtt/include extlibs/xmqtt/tests/interop/graceful_single_probe.c -lws2_32 -o out/graceful_probe.exe
python extlibs/xmqtt/tests/interop/run_graceful_wire.py --probe out/graceful_probe.exe --report out/graceful-wire.json
```

The fixture binds only an ephemeral loopback port, starts one client per case,
checks exact final frames and EOF, joins its threads and closes every socket.
The successful report has `complete=true`; stdout lists the passing cases.
The generated executable/report under `out/` may be removed after inspection.


## QoS-2 performance workload

The separate [QoS workload](../bench/README.md) measures complete sender and
duplex echo exchanges with exact completion/cold/hot/instrumentation facts.
It includes the in-process peer and observer; it supplies measured process CPU,
latency/throughput, connection setup/teardown and XRT heap traffic without claiming
client-only cost or production acceptance. Its independent sequence checks and
physical request/owner cleanup are validated separately from timing/noise gates.


## Current-connection subscriptions

`xmqttClientSubscribeAsync` / `xmqttClientUnsubscribeAsync` accept a codec
SUBSCRIBE / UNSUBSCRIBE descriptor. Blocking `Subscribe` / `Unsubscribe`
wrappers use the same deadline and reject every worker of the same Engine.
They return a Future containing **xmqttsubscriptionresult**, not writeresult.
The entire wire packet is validated, encoded and copied before admission
returns; filters/options/properties need no later caller lifetime. Input vectors
are limited by `Codec.MaxTopics` (default 1024), packet bytes and request/wire
quotas. A zero identifier allocates one from the same outgoing bitmap/hash table
as QoS1/2 publications. An explicit collision is normal OVERLOADED and leaves
the output unchanged; its reservation lasts through actual native retirement.
Retaining the completed Future does not keep an identifier or Client alive.

Admission can precede CONNACK. Nothing is sent before accepted negotiation;
packet size and MQTT5 wildcard/shared/subscription-identifier availability are
checked before the first byte. UNSUBSCRIBE is allowed even if those subscription
capabilities are disabled. Requested subscription QoS may exceed the peer's
Maximum QoS for client publications. Subscription controls consume no publishing
Receive Maximum credit, allowing them to proceed when publications are blocked.
Partial frames still finish before another packet is inserted. There is no
count-based batching delay, retry timer, automatic resubscription or registry of
active filters in this stage. A subscription identifier on SUBSCRIBE is copied;
incoming PUBLISH subscription-id properties are retained with each message.

`Requested` counts input filters. `Acknowledged` requires the correct packet
kind, reserved id, fully accepted request, exact reason count and grants no higher
than each requested QoS. `Granted` counts successful filter outcomes (including
MQTT5 UNSUBACK 0x11, no subscription existed); `Rejected` counts negative ones.
Any rejection yields REJECTED, **including mixed success**: no rollback is
implied. Reason bytes retain the input filter order. MQTT3.1.1 UNSUBACK has no
reason payload and acknowledges all requested removals. With no acknowledgement,
nonzero `AcceptedBytes` means broker effects are uncertain, including after
cancellation, timeout or transport loss. Incoming publications may precede
SUBACK or continue after UNSUBACK; these enter the ordinary bounded owned queue.

`Ack` is an optional owned message with ordered `Body.Reasons` and the full
Reason String/repeated User Properties. Its bytes are rebased into its immutable
owned wire, surviving later input-buffer reuse, Client and Engine destruction.
Use `xmqttMessageRetain` to retain it beyond its result Future. Scalar counts
and acknowledgement are committed before metadata allocation; a local LIMIT or
MEMORY result can therefore have a completed broker decision and `Ack == NULL`.
Request Problem Information=0 prohibits diagnostic properties on these ACKs too.
Wrong ids/kinds, excessive grants, bad domains/counts and unsolicited/duplicate
subscription ACKs cause protocol close. No uncertain operation is replayed.

Cancellation/timeout before any byte finishes only that request. After an
accepted prefix it retires the connection, settling all other accepted requests
without stale id reuse. Graceful application DISCONNECT waits for accepted
subscription operations under its original finite deadline. Close remains
abortive and every accepted control Future is settled before physical retirement.

The shared private Request remains 104 bytes on tested x64 builds; a tagged
union reuses the QoS2 inline-release storage for the filter count. Publish result
and public Client config/info layouts are unchanged. Each control has one
request-plus-wire allocation, one typed result box, and existing Promise/Future/
cancel producer costs. ACK metadata allocates one owned message. The lazy shared
id table retains its previous size/ownership. A subscribe capability scan reads
filter bytes once at admission; ACK validation skips the immutable encoded filter
bytes and reads each requested options byte with no extra descriptor allocation.
These branches/scans and owned copies are real costs; no speedup or zero-overhead
claim follows from the unchanged object sizes. Dedicated externally paced
subscription benchmarks and reference-broker parity remain pending.

See [the runnable offline example](../examples/subscriptions/README.md),
`tests/client_subscriptions.h` for OOM/lifetime/terminal tests, and
`tests/interop/run_subscription_wire.py` with `subscription_probe.c` for an
independent byte peer (including fragmented/reordered ACKs and host-worker rules).
Rules follow the [MQTT5 OASIS standard](https://docs.oasis-open.org/mqtt/mqtt/v5.0/mqtt-v5.0.html)
sections 2.2.1, 3.8-3.11 and the equivalent
[MQTT3.1.1 sections](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/mqtt-v3.1.1.html).
Subscription/session recovery, durable state, expiry,
DNS/TLS/WebSocket and full performance/production qualification are incomplete.


Reproduce the independent subscription peer from the root (Windows GCC):

```powershell
gcc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude -Iextlibs/xmqtt/include extlibs/xmqtt/tests/interop/single_subscription_probe.c -lws2_32 -o out/subscription_probe.exe
python extlibs/xmqtt/tests/interop/run_subscription_wire.py --probe out/subscription_probe.exe --report out/subscription-wire.json
```

On Linux, replace `-lws2_32` with `-lm -pthread`, use an executable path without
`.exe`, and optionally add `-fsanitize=address,undefined -fno-omit-frame-pointer`
to compilation and `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` /
`UBSAN_OPTIONS=halt_on_error=1` at execution. `--case NAME` selects the same
named case for applicable protocol versions. The full default run records 107
cases. Outputs remain under ignored `out/`; normal process cleanup closes each
loopback socket and joins the peer thread.


## MQTT5 incoming Topic Alias

CONNECT `Topic Alias Maximum` opts in; absent/zero retains the no-alias default.
The client accepts every id 1..advertised maximum, including 65535. The server's
CONNACK alias maximum describes the opposite direction and cannot change this
receive limit. A full-topic PUBLISH binds/replaces that id, and a subsequent
empty-topic PUBLISH resolves it. Unaliased full topics do not create or replace
mappings. The table belongs only to one Client/connection and is not carried to
another connection, even with the same ClientId or retained messages.

Only the host protocol worker reads/updates the table; lookup adds no mutex.
The first valid full-topic alias lazily allocates a direct pointer table of
`ReceiveAliasMaximum * sizeof(pointer)` bytes. Each bound id owns an entry with
two size fields plus the exact initial topic bytes. Same/shorter remaps reuse
that capacity; larger remaps charge both old and new entries until the new copy
is installed, then free/refund the old one. These capacities count in
`Info.PrivateBytes` / `MaxPrivateBytes`, remain cached through Destroy and are
separate from owned-message quotas. Destroy scans all advertised pointer slots
and frees their entries. No table is allocated for disabled or unused aliases. The private Client gains one pointer; public config/info,
result, packet, codec and property layouts do not change.

For compact packets, `xmqttMessagePacket(M)->Body.Publish.Topic` is an owned
**logical resolved topic**, while `Wire` preserves the actual empty-topic frame.
The resolved bytes are appended to the message's existing allocation, counted
in `MaxObjectBytes`, queued `MessageBytes` and `MaxMessageBytes`. Remapping,
releasing the original bind message and destroying Client/Engine never invalidate
any retained message. Re-decoding Wire naturally exposes its original empty
topic. An outgoing encoder does not silently strip or reinterpret that view;
outgoing admission now follows the logical snapshot contract below.

Alias processing occurs before active QoS2 duplicate suppression. A duplicate
can therefore update a binding without a second application delivery. Normal
QoS1/2 ACK credit/order, duplex publication and graceful incoming-flow drain
remain in force. Queued aliases/message limits fail with typed local LIMIT;
allocation failure closes with MEMORY without publishing a partial message.
A longer rebind can fail at its documented temporary old/new peak. Close and
physical retirement do not release caller-held message objects.

The decoder now preserves allocation-independent alias zero/repetition context
through the existing private properties boundary. New appended codec error
constants distinguish zero (`XMQTT_CODEC_TOPIC_ALIAS`) and repeated properties
(`XMQTT_CODEC_TOPIC_ALIAS_REPEAT`). Public property statuses/outputs and existing
codec enum values are unchanged. Runtime attempts DISCONNECT 0x94 for zero or
above-maximum aliases, 0x82 for unbound compact references or repeated aliases;
the existing finite diagnostic/abort fallback still applies. Other malformed
fields keep their prior diagnostics. See [OASIS MQTT5 sections 3.3.2.3.4 and 3.3.4](https://docs.oasis-open.org/mqtt/mqtt/v5.0/mqtt-v5.0.html).

Costs are explicit: first-use table/entry allocation, cache copies on full-topic
bind/remap, a direct lookup on compact references, and a resolved-topic copy in
one existing message allocation. Compact wire saves topic bytes; it does not
eliminate retained-topic storage or copying. The property reader's internal
context and clone's optional resolved-view branch are additional instructions;
no zero-overhead or performance acceptance is claimed. Subscription/alias
workloads, pinned reference/broker comparisons, native Linux and >=72h native
production qualification remain required.

The [offline example](../examples/receive_alias/README.md),
`tests/client_aliases.h`, alias codec diagnostic-OOM tests and the independent
`tests/interop/run_alias_wire.py` / `alias_probe.c` cover mapping lifetimes,
remaps, max ids/topics, QoS1/2, duplicate updates, raw/logical views, directional
limits, exact error reasons, bounds, OOM and destruction. Reproduce the peer:

```powershell
gcc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude -Iextlibs/xmqtt/include extlibs/xmqtt/tests/interop/single_alias_probe.c -lws2_32 -o out/alias_probe.exe
python extlibs/xmqtt/tests/interop/run_alias_wire.py --probe out/alias_probe.exe --report out/alias-wire.json
```

Linux replaces `-lws2_32` with `-lm -pthread`, optionally adding
`-fsanitize=address,undefined -fno-omit-frame-pointer` and the usual ASan/UBSan
halt/leak options. The peer has 39 distinct cases and supports `--case NAME`;
JSON/stdout/stderr stay under ignored out/. It closes local sockets and joins
its thread each time. There are no external service or credential prerequisites.


## MQTT5 outgoing Topic Alias

A PUBLISH Topic Alias opts in. Both full-topic and explicit empty-topic inputs
are supported. Admission fixes a logical topic: a full topic updates that alias's
latest successfully admitted intent; an empty topic captures that intent, or
returns ADMISSION_ERROR with unchanged Future if it has no admitted binding.
Subsequent input changes, later rebinds, cancellation or timeout of an earlier
operation cannot retarget an already admitted publication. Concurrent callers
linearize when accepted into the submission queue, as for other publications.
No cache intent is changed by failed/overloaded/closed admission.

Before its first byte, the sole worker compares the captured topic with its
actual outgoing wire mapping. A matching mapping allows an empty wire topic;
otherwise the publication carries its captured full topic and establishes that
binding. Thus a QoS0 reference can safely proceed while an earlier QoS1 binding
is blocked by Receive Maximum, or after that unsent binding was cancelled. It
cannot accidentally use an older broker topic. A delayed older publication also
rechecks the mapping before sending. This changes the frame representation of
the requested operation, without replaying an earlier operation. A complete
packet accepted into TCP establishes ordered mapping state; it does not prove
broker receipt or message acceptance. QoS1/2 confirmation rules are unchanged.

The alias must fit the server's CONNACK maximum (absent/zero disables it),
independently of incoming alias capacity. Packet caps apply to the **actual
selected frame**: peer overflow returns RESTRICTED; local wire overflow returns
LIMIT before sending and keeps the connection open. Snapshot encoding allows
full-topic bytes above the local wire cap when a compact selected frame fits;
payload/property/UTF-8/protocol limits and MaxOutgoingBytes still bound retained
input. `AcceptedBytes` reports actual PUBLISH bytes and can be larger or smaller
than the original descriptor encoding. `OutgoingBytes` charges that retained
encoding, while the optional metadata/topic caches count in PrivateBytes. Native
queued expanded bytes remain separately bounded by Stream.WriteLimit.

Ordinary requests retain their 104-byte private shape and original raw send
loop. Aliased requests add an aligned 48-byte tail (plus 0..7 alignment bytes),
charged to MaxPrivateBytes until physical retirement, with no separate metadata
allocation. A lazy 2048-byte x64 root and requested 4096-byte pages provide
constant-time slots for ids through 65535. Each immutable topic has a 16-byte
header plus its bytes. Planned, actual and in-flight requests hold explicit
references under the existing Client Gate. Same-topic planned bindings reuse
storage after an exact byte comparison; obsolete topics free when their last
actual reference retires. Cached slots/intents remain charged through Destroy;
Destroy scans allocated pages. The private Client gains one pointer. Public
config/info/result and codec/packet/property layouts stay unchanged.

Matching explicit empty references and first full-topic binds use their original
single raw buffer. Compaction of full-topic input gathers an inline prefix and
owned original tail; repairing an empty reference gathers prefix, captured topic
and tail. XRT SendRefs accepts each bounded group atomically. Every nonempty
native segment holds a real request reference and contributes to NativeRefs,
so id, metadata and topic retirement cannot precede any release callback. Headers
are immutable after the first accepted byte and partial frames preserve packet
ordering. No extra payload copy or revalidation scan is added at send time.

Costs are explicit: optional metadata, cold pages/topics, retained snapshot and
cache copies, same-topic comparison at admission, mapping comparison/short header
encoding, and up to three native spans per alias chunk. Gather cases can add
native nodes, reference operations and release callbacks even with one API call;
this is not a zero-overhead claim. Default raw requests avoid the gather stack
and loops, but tagged wire-length/admission checks add instructions. Detailed
throughput/tail/CPU/copy/allocation and pinned-reference measurements remain
required; the earlier failed QoS2 quality gates are unchanged.

The [offline send-alias example](../examples/send_alias/README.md) binds a topic,
sends a compact QoS1 reference and checks actual 23/14-byte outcomes after owner
destruction. `tests/client_send_aliases.h` adds deterministic queued cancellation/
zero timeout, flow and allocation-budget rollback. The independent byte peer
covers 30 cases, including logical snapshot repair, post-flow latest intent,
peer rejection/restriction, QoS1/2, 65535 id/topic bounds, Remaining Length width
changes, compressed local/peer limits, partial cancellation/deadline/EOF, normal
private-cap overload, invalid/unknown aliases, duplex and graceful drain.

```powershell
gcc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude -Iextlibs/xmqtt/include extlibs/xmqtt/tests/interop/single_send_alias_probe.c -lws2_32 -o out/send_alias_probe.exe
python extlibs/xmqtt/tests/interop/run_send_alias_wire.py --probe out/send_alias_probe.exe --report out/send-alias-wire.json
```

Linux replaces `-lws2_32` with `-lm -pthread` and can add ASan/UBSan/halt/leak
options. `--case NAME` selects a case. JSON includes independently observed wire
lengths checked against every corresponding result's AcceptedBytes; maximum
topics are recorded by length/hash rather than repeated large strings. Local
peer sockets/threads and library owners are closed each run. Mappings remain
connection-local; session/reconnect/store/expiry/transports and production
qualification are still pending. Wire semantics follow
[OASIS MQTT5 section 3.3.2.3.4](https://docs.oasis-open.org/mqtt/mqtt/v5.0/mqtt-v5.0.html).


Optional MQTT5 enhanced authentication is available through the separate
[client_auth leaf](client_auth.md), factory and unique cross-thread tickets.
The base factory continues to reject Authentication Method in CONNECT.
