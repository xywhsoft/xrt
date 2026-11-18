# Restart a live cron service from a checkpoint

Select XCRON_MODULE_XCRON_SERVICE_CHECKPOINT and run the example without
arguments for a temporary store and three independent service lifetimes.
Use the following sequence to run three separate processes in an empty private directory.

```text
service_checkpoint init DIRECTORY
service_checkpoint advance DIRECTORY
service_checkpoint observe DIRECTORY
```

The caller reads the generation and checkpoint from the native store,
restores the job in a paused state and releases it explicitly to execute
the held cursor. It pauses new leases before draining the physical result,
then collects the new record and commits it from the caller thread.
Observe prints the generation, next instant and restored success count
with the job still paused. Interrupt can be run before Advance
to pause with a live lease. The process interruption runner verifies replay
from the last committed generation.

The fixture uses an explicit clock and the UTC zone. Use a fresh directory
for each invocation of Init. [Service checkpoint contract](../../docs/service_checkpoint.md).
