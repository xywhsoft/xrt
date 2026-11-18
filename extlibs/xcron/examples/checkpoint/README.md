# Restore an xcron job from a checkpoint

Run the checkpoint example without arguments to save and restore one job in
memory. It prints the first uncommitted scheduled instant.

The file commands demonstrate application-owned persistence. Use fresh files
inside a disposable directory.

    checkpoint save first.bin
    checkpoint inspect first.bin
    checkpoint advance first.bin second.bin
    checkpoint inspect second.bin

The two inspections print NEXT 1000 and NEXT 2000. The command named active
restores and publishes the next lease, then waits for stdin before completing
it. Terminating that process leaves the earlier file untouched. Use the
separate process test to run that check with bounded subprocesses.

The sample writes one file with C stdio. A production host needs a durable
commit protocol for the file or database record. See [checkpoint contract](../../docs/checkpoint.md).
