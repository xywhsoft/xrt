# Asynchronous checkpoint group writer

Open and read the group with CheckpointGroupOpen and CheckpointGroupRead.
Use the recovered view to initialize jobs before transferring the store to
CheckpointWriterCreate. The writer owns the open file and exclusive lock.
The host owns the XRT executor and keeps it live through WriterDestroy.
Choose a storage executor with a bounded queue. A dedicated executor keeps
disk latency out of the execution pool that dispatches jobs.

Submit supplies the current expected generation and immutable records.
The writer holds one operation through collection. Admission copies the
descriptor and borrows the record array. It allocates no per-commit object.
The executor applies its own queue limit. Admission rejection ends without a
ticket or a borrow; inspect the XRT diagnostic for queue pressure or close.

The worker runs the same validated alternate-slot write and native flush as
CheckpointGroupCommit. It publishes the result after it releases use of the
store and records. Collect with zero to poll, a relative millisecond budget,
or -1 to wait. Only successful collection transfers the result and ends the
borrow. A stale ticket leaves the current ticket and output intact. One
collector owns the reply and any error. Reply.Generation is meaningful when
the status is OK. A wait from the storage executor is refused if the ticket
still depends on it.

Closing the executor drains an accepted commit. Cancelling the executor
reports CANCELLED for a discarded queued commit and leaves the store
generation intact. An already running commit finishes with its native store
result. A collection timeout keeps the ticket valid. Destroy requires all
tickets collected and public calls quiescent. It closes the owned store.
After an uncertain result, destroy and reopen the store to discover the
durable generation before retrying.

For a live group, capture and hold the member jobs first, then submit the
captured records to the writer. Keep the jobs held until your policy accepts
the durable outcome. The scheduling owner can still process unrelated work
during the write and flush.
[Runnable live group example](../examples/service_checkpoint_group/README.md).
