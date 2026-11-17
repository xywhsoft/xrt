# Restore committed obligations into an independent Session

Build/run the `xmqtt_store_session_tests` suite. The example creates, syncs and
reopens a real temporary typed journal, recovers a QoS2 obligation already at
PUBREL, imports an ordinary disconnected Session, destroys the source State,
and inspects the independent owner. An optional file argument is initialized
when empty or reopened with the exact demo binding, identity and generation.
The default deletes only its own temporary file.

The demo uses fixed historical wall milliseconds for reproducible offline
expiry arithmetic. Live code must supply trusted actual wall time and a current
monotonic clock. Join and destroy the old transport owner before replacing its
Session; import creates an independent owner and cannot fence an old Client.
Attach the resulting Session to an existing Client Session factory, send its
bound ClientId, and wait for fresh authenticated CONNACK before replay. Storage
generation/epoch are distinct from the new transport generation.

This passive example sends no network ACK and commits no completion. The file
will recover the obligation again until a completion is durably recorded.
Keep pending State Inbox payloads until a durable application ACK; Session
import does not move them into the volatile Client Inbox. Namespace durability,
leases, live commit ordering and compaction remain separate work.

`tests/interop/run_store_session.py` drives an actual Client and Engine against
an independent literal Python TCP peer for both MQTT versions, full and
fragmented IO, including PUBREL-stage and DUP/no-DUP outgoing replay and old
QoS2 receive deduplication. See [contracts](../../docs/store_session.md).
