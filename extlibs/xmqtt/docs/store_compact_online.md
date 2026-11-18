# Online MQTT store compaction

`xmqttStoreWriteCompactAsync` inserts a maintenance operation into the existing
Store Writer FIFO. The caller can request it directly or install an automatic policy. The same Writer,
Client connection and application Inbox leases remain active. The host storage
task serializes the durable writes while the network engine continues handling
transport work. A later operation that needs a durable acknowledgement waits
behind the checkpoint in the bounded FIFO.

A native slot owns the selected Journal. The compactor measures the replacement
from the in-memory durable model before writing. It skips a generation switch
when the resulting selected file would save too little space. Entry count and
scratch capacity are separately bounded. The scan does not replay old
transactions or allocate a second model. Writing copies the retained packets once
into bounded blocks and uses the ordinary Journal barriers. Both data files can
be open briefly while the control lock continues to exclude other owners.

After selector sync the Writer swaps its Journal and continues with the new
generation's physical sequence. Previous results retain their original values.
An inactive-file write failure leaves the old Writer usable. Unknown publication
effect fences it. Retire the Client and Writer, then reopen the slot to resolve
the selected generation. When recovery dropped expired protocol state locally,
a durable reset must commit before an online checkpoint can replace that
history. The offline recovery path preserves the raw expiry state.

Cancellation, close and timeout are checked between blocks and before selector
publication. In-flight native I/O retires physically. A late stop can accompany
a committed generation. The selected-file byte figures describe the log to
replay. Filesystem usage still includes the inactive file until reuse.
The maintenance operation occupies one FIFO request, plus its normal retained
result credit. Scratch and the second native Journal are additional bounded
allocations. The caller chooses a workload-specific tail-latency budget for the time
other durable work spends behind the checkpoint.

Build the example with

```sh
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_store_compact_online_tests
```

Provision the three empty slot files in a private directory, then run the
example with `create DIRECTORY`, `online DIRECTORY`, and `read DIRECTORY`.
It prints the selected generation and the retained Inbox count.

## Automatic policy

Initialize `xmqttstoreautocompactconfig` and call
`xmqttStoreWriteSetAutoCompact` when the native-slot Writer is idle.
Pass NULL to disable it. Each pump gives queued foreground work priority.
At an idle boundary it considers one checkpoint when the selected file meets
TriggerBytes. An append that is valid but reaches the physical file cap can
also run one checkpoint and retry on the selected generation. The original
deadline, semantic validation and Inbox acknowledgement reservation still apply.
Typed operation and replay limits are independent.

After a nonpublishing attempt, continued foreground commits can grow the
selected file by RetryGrowthBytes to rearm compaction. Definite inactive I/O
failures, temporary contention and local allocation failures also start the
configured RetryDelayMs cooldown. A later foreground arrival can then retry
even when the file has no room to grow. A manual successful rotation clears
both gates. Inadequate space savings use the byte threshold alone. The growth
floor and checkpoint limits are explicit. The policy creates no timer, extra task or
maintenance Future. A committed foreground Future can finish before automatic
maintenance. `xmqttStoreWriteAutoCompactInfo` reports the separate outcome.
With uncertain selection, the Writer stops. Reopen the slot to resolve which
generation survived. The foreground result still reports only its own effect.

The example also accepts `auto DIRECTORY` after `create DIRECTORY`. It sets a
small trigger and issues one foreground transaction to drive the policy.
