# Offline MQTT store compaction

After closing, joining and destroying the Client and its Store Writer, call
`xmqttStoreCompact` on the same exclusively owned typed journal slot.
Set the recovery configuration to the selected storage generation and use a
trusted wall clock. The compactor fully validates the selected history, takes
its durable cut under the slot lock, and retires the source file. It writes a
versioned checkpoint and bounded ordered row blocks into the inactive file.
Only after every block commits does it publish the new generation.

The checkpoint keeps the original broker epoch, highest flow/delivery key,
disconnect wall time, negotiated expiry, outgoing PUBLISH and PUBREL stage,
active incoming QoS 2 identity, and pending Inbox wire even across broker-session
resets. It stores the pre-expiry model so future recovery evaluates expiry
at its own wall time. Application ACK and protocol completion remain distinct.
The caller still obtains a fresh broker decision before resuming network replay.
[MQTT 5.0 §4.1](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html) ·
[MQTT 3.1.1 §4.1](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html)

After DONE, read the new generation using `xmqttJournalSlotInfo`, reopen the
selected Journal, and recover it with `xmqttStoreStateRecover`. The ordinary
Store Writer accepts the matching recovered State and Journal. The byte format
of legacy delta-only generations is supported. Deploy updated State readers
before writing checkpoints. On UNCERTAIN or a poisoned slot, destroy and reopen
the slot and accept only its selected generation.

The recovered State uses the configured count/memory limits and the operation,
byte, and transaction replay limits. The compactor additionally allocates one
encoding buffer capped by MaxScratchBytes and both transaction size limits.
Each live wire is copied from the source replay once into the recovered model
and once per output block. Journal framing and CRC scans follow the ordinary
Journal path. There are two frame durability barriers per emitted transaction
plus the slot's selector sync. The inactive file may contain incomplete data
after a refusal or failure. Only the selector makes it current.
The caller schedules the offline pause. Live scheduling, namespace
power-loss qualification and production tail-latency measurement remain open.

Build the example with

```sh
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_store_compact_tests
```

In a private directory, provision the three empty regular files `journal.slot`,
`journal.0` and `journal.1`. Run the example with `create DIRECTORY`,
`compact DIRECTORY`, and `read DIRECTORY`. Each operation reports the
selected generation, committed transaction count, pending Inbox count and
highest key. The illustrative history of four transactions becomes two
while keeping the same pending delivery.
