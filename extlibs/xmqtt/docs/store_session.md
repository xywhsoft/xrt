# Import committed recovery into an ordinary Session

Status: developing. Select `XMQTT_MODULE_XMQTT_STORE_SESSION`. The three public
APIs construct one ordinary disconnected Session from the immutable committed
state recovered by [store_state](store_state.md). There is no whole-image
serialization, CRC pass, temporary image or new worker/runtime. Live durable
Client storage, ACK ordering, delivery leases and commit integration remain open.

Initialize `xmqttstoresessionconfig`, set exact broker/principal/slot Binding,
storage Generation, trusted wall milliseconds and current monotonic seconds.
Normal Restore requires matching MQTT level and explicit ClientId. Assigned
Restore explicitly accepts empty MQTT5 ClientId, then copies only the already
bound identity after exact slot checks. An unbound empty journal cannot provide
an identity; normal Session creation/authenticated binding still comes first.

Outgoing identifiers, acceptance order, PUBLISH/PUBREL stage and attempted/sent/
PUBREC metadata survive. Incoming QoS2 identities preserve accepted-PUBLISH versus
received-PUBREL state. A new connection starts generation1 from an imported0;
selection tokens, aliases, send/receive windows, Engine ownership and old Futures
are never imported. The existing authenticated Open/CONNACK policy decides resume,
reset or311 fail-closed mismatch before any replay. Inbox payloads remain a
separate recovered obligation; constructing/destroying a Session is no application
ACK and does not supply a durable live transport.

Further wall time since recovery consumes closed finite expiry. Backward trusted
wall time fails. A live cut retains its candidate until a broker decision because
actual disconnect time is unknown. Expiry during import returns RESET with an
empty Session, after validating old outgoing packets under the selected codec and
resource caps. The source state/Inbox never changes. Fractional seconds are
rounded conservatively on rebasing; monotonic clock representation/precision
still limits the observable instant of expiry.

SessionConfig.MaxBytes includes fixed Session maps/identity and exact retained
packet/property allocations. MaxOutgoing/MaxIncoming and native codec caps still
apply. Reusable property scratch has a separate MaxScratchBytes cap; zero allows
property-free MQTT5 packets and needs no scratch for311. The already bounded
source State remains additional. Inputs borrow only for the call. Outputs cannot
alias configuration/views or any State allocation. Partial candidates are freed
on errors/LIMIT; checked quiet error restoration precedes output publication.

Join and destroy the previous Client/transport before replacing its Session.
This passive importer cannot fence an old connection or coordinate live commits.

Each outgoing packet is decoded once into reusable scratch, copied once into
its Session packet, and has string/binary/pair/topic/payload views rebased. There
is one Session allocation, at most one scratch allocation and one allocation per
outgoing packet. Incoming ids require no payload copy. Image restore and store
import share the private packet/index/expiry construction; image decoding still
uses its known property count directly. Descriptor copies, rebasing, allocation,
hash/id updates, input/alias checks and native codec scans remain real costs.
No zero-overhead or production-performance claim is made. Protocol rules follow
[MQTT5](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html) and
[MQTT311](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html).
