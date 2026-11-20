# APP-driven online MQTT store compaction

The [APP compaction helper](../examples/common/store_compact_online.h)
queues an explicit operation on the Store Writer FIFO with
`xmqttStoreWriteCheckpointAsync`. The callback runs on the storage task at
a committed boundary. The same Writer and Client connection remain active,
and later durable work waits behind the callback in the bounded FIFO.

The APP compares the selected size to its recovery threshold, then calls
`xmqttStoreCheckpointMeasure` to obtain the projected framed size. If the
estimated reduction is sufficient, it calls `xmqttStoreCheckpointStage` with
an APP callback that opens the replacement Journal. It then calls
`xmqttStoreCheckpointPublish` with an APP callback that persists selection
and reports the durable effect. The included APP uses the two-generation
selector. The APP can leave the original
generation selected by returning before publication. The checkpoint cut
lives only for the callback and cannot be used by another task.

The checkpoint uses the in-memory durable model to bound measurement and
scratch by the request limits. It copies retained packets into bounded
blocks and uses the ordinary Journal barriers. The APP keeps both data
handles and the control handle open across the switch. The Writer uses the
reported effect to adopt the candidate or fence the current Journal.

After selector sync the Writer swaps its Journal and continues with the new
generation's physical sequence. Previous results retain their original values.
An inactive-file write failure leaves the old Writer usable. Unknown publication
effect or failed selector witness repair fences it. Retire the Client and
Writer, then reopen the slot to resolve
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
`XMQTT_COMMIT_LIMIT` and `XMQTT_JOURNAL_NOT_COMMITTED`. The APP submits
maintenance through a separate explicit request. After a successful
compaction, the APP can retry its own definitely uncommitted operation under
its remaining deadline. A definitely uncommitted retirement leaves the record
pending for the APP to retry. If selection is uncertain, reopen and resolve the selected
generation before taking further action.
