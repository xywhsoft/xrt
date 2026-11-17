# Commit a recovered delivery ACK on a host pool

Run the `xmqtt_journal_worker_tests` suite, including this executable. Without
arguments the example creates a real temporary typed journal, syncs one accepted
QoS1 delivery, recovers its independent Inbox, performs the printed demo work,
submits its explicit INBOX_ACK to a borrowed two-thread XRT pool, destroys all
physical worker owners, and reopens/replays the file to show the delivery gone.
An optional existing exclusively owned file is initialized when empty or reopened
with the exact demo identity/binding/generation. The default deletes only its own
temporary file; namespace publication/power-loss guarantees are not supplied.

Fixed historical clocks make this offline example reproducible. Live code needs
trusted wall/monotonic clocks and must finish its actual application effects
before submitting ACK. Arbitrary business effects and the file are not one atomic
transaction. A failed/timed-out/cancelled result may still report COMMITTED or
MAY_BE_COMMITTED: stop, physically join and recover; do not blindly retry.

The worker accepts opaque bytes. The host validates legal MQTT transitions and
owns delivery leasing/deduplication; this example serializes one offline consumer.
The immutable State is a snapshot and does not change after append. A fresh State
proves the persisted change. Network ACK gating/live Session mutation, leases,
namespace/locking/compaction/group commit and production acceptance remain open.
See [worker contracts](../../docs/journal_worker.md) and [journal](../../docs/journal.md).
