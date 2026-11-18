# Checkpoint a live cron service

The optional service checkpoint module supports actual and selected live
services. Create a service with its checkpointed constructor. Its arena
includes one 180-byte record and key for each configured command slot.
Ordinary construction leaves that reservation disabled. Checkpoint commands
share the existing FIFO, work limit, reply credits and command generations.
Admission copies a restore record and retains the config zone. Collection
transfers the original Data duty after a failed restore, the live Job ID
after a successful restore, or the captured record after a successful save.

The owner saves only quiescent jobs and validates only the one saved
occurrence when restoring. An active lease or unfinished calculation can
return kernel BUSY. Save captures the state at command execution. The
service can advance again before the caller receives or commits that state.
Selected service restores the delivery policy and local metadata from the
saved record. The caller must reconstruct the correct configuration and
manage one live owner for each persistent identity.

The caller can use the [checkpoint store](checkpoint_store.md) to read
before restore and commit after capture. File synchronization runs on the
calling or separate storage thread. Use one ordered producer for a key
when publishing generations. The application owns the order between
business effects, snapshot requests and durable commits. After a process
interruption, execution can repeat work following the last committed
snapshot. The service can run again as soon as a restore commits on its
owner thread. Paused dispatch, automatic snapshots and batching are
outside the scope of these commands.
