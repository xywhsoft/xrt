# Persist and restore a scheduler job

Run the checkpoint_store example without arguments to create, commit and
reopen a store in a temporary directory. It removes its own fixture.

To inspect the file across processes, create an empty, trusted directory
and run these commands with that directory as the second argument.

    checkpoint_store init ./state
    checkpoint_store show ./state
    checkpoint_store step ./state
    checkpoint_store show ./state

The first call commits generation 1. The final call reports generation 2
and the next scheduled instant. Run the active action to hold a lease
without committing a new generation. The restart test kills that process
and checks the last synchronized checkpoint. Repeated init calls fail
against an existing file. [Store contract](../../docs/checkpoint_store.md).
