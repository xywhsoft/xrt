# Commit a group of scheduler checkpoints

Select XCRON_MODULE_XCRON_CHECKPOINT_GROUP. Run with no arguments to create
a temporary store, commit two jobs, advance both, and restore the result.

To use independent processes, create a private directory and run the
following sequence with the same directory argument.

~~~text
checkpoint_group init DIRECTORY
checkpoint_group advance DIRECTORY
checkpoint_group show DIRECTORY
~~~

The example restores both jobs in the paused state. Advance checks that
the overdue jobs stay held, then resumes both, completes their leases and
commits both successor records with one flush. Show reads the latest
complete generation and prints its two next occurrences.
[Group store contract](../../docs/checkpoint_group.md).
