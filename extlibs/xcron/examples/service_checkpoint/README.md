# Restart a live cron service from a checkpoint

Select XCRON_MODULE_XCRON_SERVICE_CHECKPOINT and run the example without
arguments for a temporary store and three independent service lifetimes.
Use the following sequence to run three separate processes in an empty private directory.

```text
service_checkpoint init DIRECTORY
service_checkpoint advance DIRECTORY
service_checkpoint observe DIRECTORY
```

The caller reads the generation and checkpoint from the native store before
queueing a restore. After a completed lease it collects a new record and
commits it from the caller thread. Observe prints the generation, next
instant and restored success count. Interrupt can be run before Advance
to pause with a live lease. The process interruption runner verifies replay
from the last committed generation.

The fixture uses an explicit clock and the UTC zone. Use a fresh directory
for each invocation of Init. [Service checkpoint contract](../../docs/service_checkpoint.md).
