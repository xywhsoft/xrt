# Durable Inbox delivery leases

Select the developing xmqtt_store_inbox feature. Open a journal with the same
exact slot, storage generation, schema and protocol version used to record it.
Recover the entire committed prefix and pass the recovered State and READY
Journal to xmqttStoreInboxCreate. It transfers both owners under the existing
Writer rules. Keep the host task pool and file alive through physical
Writer destruction.

xmqttStoreInboxTake claims the first ready delivery and copies canonical MQTT
wire bytes into the caller's buffer. Size refusal leaves the delivery ready.
The caller decodes the owned bytes and performs its application effects. The
lease can be copied but accepted completion or release consumes the claim.
Returning a claim without IO via xmqttStoreInboxRelease makes the delivery ready
at the tail. Unacknowledged leases are volatile, so recovery makes their
deliveries ready in persistent acceptance order.

xmqttStoreInboxAckAsync uses the original acceptance epoch and appends one typed
Inbox ACK. Use the returned commit result's actual Effect. COMMITTED consumes
the delivery even when local cancellation, Close or timeout set another status.
An accepted ACK that was definitely not committed consumes the old lease and
makes its delivery ready again if the Writer remains usable. With uncertain
effects or a failed Writer, recover the journal before issuing new claims. A
synchronous admission refusal leaves the original lease valid.

The Inbox queue adds constant work per committed offer, claim and release,
excluding the payload copy and validation of output spans. The existing
ownership overlap check may scan the bounded committed model for outputs whose
addresses fall inside its conservative address envelope. Take allocates
nothing. Ready deliveries, outstanding leases and pending ACKs share the
existing MaxInbox cap. The caller buffer and native executor budgets are
separate. ACK still uses the existing two file syncs and incurs the semantic
worker's native allocations. The Writer reuses the host task pool.

The lease controls application delivery only. QoS 2 receive identities remain
until their independent protocol retirement. The application owns its external
effects. Preserve idempotency or use an application transaction coordinated by
durable slot and delivery key.

See the runnable [native example](../examples/store_inbox/README.md).
