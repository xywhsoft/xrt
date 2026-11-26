# Moving pending publications into an APP inbox

Run from the repository root:

    python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_store_pending_tests

Create a dedicated directory and pre-provision two distinct empty files named
`session.journal` and `inbox.journal`. Run these commands using the built
example. On Windows use the `.exe` suffix. Use `311` throughout for MQTT 3.1.1.

```text
examples_store_inbox_main seed DIRECTORY 5
examples_store_inbox_main migrate DIRECTORY 5
examples_store_inbox_main consume DIRECTORY 5
examples_store_inbox_main migrate DIRECTORY 5
```

`seed` creates a legacy fixture containing three accepted publications, with
the middle publication already confirmed. `migrate` imports the other two
from the fully recovered protocol journal. The separate APP journal stores the
original epoch, key and publication bytes. After processing each record,
`consume` writes an APP confirmation. Repeating migration after processing
imports zero records. The APP journal retains the highest imported key even
if there are no pending deliveries.

Run migration with the old Client stopped and hold both files exclusively.
The legacy journal is recovered normally, including removal of any torn,
uncommitted suffix. Migration does not append a confirmation to it. Keep the
source until the target has been recovered and verified. The example uses a
fixed broker/client binding and generation. Adapt the configuration for an
actual source, and use a separate APP inbox for each original key namespace.

If a write reports an uncertain outcome, reopen and replay before retrying.
A crash after the business effect but before the APP confirmation can repeat
the effect. Use the original journal namespace, epoch and key to make the
business operation idempotent. The example prints the effect and uses a
bounded FIFO in the APP. The [legacy processing contract](../../docs/store_inbox.md)
describes the source format.
