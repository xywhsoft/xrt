# Processing durable pending publications in the APP

Recover the journal state and transfer it to `xmqttStoreWriteCreate`. Use that
Writer for the connected Client or for offline processing. The
[APP example](../examples/store_inbox/README.md) reads, processes and retires a
record, then verifies the result after reopening the journal.

`xmqttStoreWriteReadPending` copies the oldest committed record into a caller
buffer. It reports the required size on LIMIT and leaves the view unchanged.
Peeking leaves the record pending. A single consumer can process records in
order. For concurrent consumption, the APP can assign disjoint record keys and
read each with `xmqttStoreWriteRead`. A key and epoch belong to their journal
namespace. Keep that namespace with them if the APP saves a processing cursor.

After processing, submit `xmqttStoreWriteRetireAsync` with the record's original
epoch and key. The writer serializes it in the existing FIFO. A stale key or
epoch is refused before any append. The persisted delta retains its existing
INBOX_ACK representation. A committed retirement removes the pending record.
MQTT QoS 2 duplicate detection still follows its protocol identity and PUBREL
state.

The APP decides how to handle admission pressure and definite refusals. Confirm
COMMITTED before advancing the processing cursor. For MAY_BE_COMMITTED, stop
processing, reopen the journal and reconcile against the recovered state. An APP
that commits effects outside the journal can use its namespace and delivery key
as an idempotency key when recovering from a crash between the effect and the
retirement.
