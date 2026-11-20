# Online MQTT store compaction

`xmqttStoreWriteCompactAsync` inserts a maintenance operation into the existing
Store Writer FIFO. The same Writer and Client connection remain active. The
host storage task serializes durable writes while the network engine handles
transport work. A later operation that needs a durable acknowledgement waits
behind the checkpoint in the bounded FIFO.

The selected Journal borrows a data file from the Slot. The compactor measures the replacement
from the in-memory durable model before writing. It skips a generation switch
when the resulting selected file would save too little space. Entry count and
scratch capacity are separately bounded. The scan does not replay old
transactions or allocate a second model. Writing copies the retained packets once
into bounded blocks and uses the ordinary Journal barriers. The APP keeps both
data handles and the control handle open across the switch.

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
result credit. Scratch and the second Journal are additional bounded
allocations. The caller chooses a workload-specific tail-latency budget for the time
other durable work spends behind the checkpoint.

Build the example with

```sh
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_store_compact_online_tests
```

Provision the three empty slot files in a private directory, then run the
example with `create DIRECTORY`, `online DIRECTORY`, and `read DIRECTORY`.
It prints the selected generation and the retained pending record count.

## APP scheduling

Schedule maintenance from the APP. For the owned writer, use
`xmqttStoreWriteInfo` to inspect committed bytes and replay counters at a
known foreground boundary. Reserve headroom for concurrently submitted
work before its next admission. Run `auto DIRECTORY` after `create DIRECTORY`
to let the example compare its selected log size to an APP threshold and
submit one compaction when needed. The capacity benchmark drains its own
foreground requests and compacts at the configured transaction limit.

Storage quota exhaustion completes the original request with
`XMQTT_COMMIT_LIMIT` and `XMQTT_JOURNAL_NOT_COMMITTED`. The library schedules
maintenance only through a separate explicit request. After a successful
compaction, the APP can retry its own definitely uncommitted operation under
its remaining deadline. A definitely uncommitted retirement leaves the record
pending for the APP to retry. If selection is uncertain, reopen and resolve the selected
generation before taking further action.
