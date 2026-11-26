# Migrate pending publications from older journals

New Client deliveries use the bounded message queue. The application owns the
payloads it persists and their processing confirmations.

Run the [APP migration example](../examples/store_inbox/README.md) with exclusive
access to the old protocol journal and a separate APP journal. The example
recovers the last committed state with xmqttStoreStateRecover and enumerates
pending records with xmqttStoreStateView and XMQTT_STORE_INBOX. It imports them
in key order and keeps the source namespace, epoch and key as the delivery
identity. Repeating migration skips keys already imported, including deliveries
the APP has confirmed.

The APP writes its confirmations to its own journal. The source protocol
journal supplies historical data and remains available for session recovery.
Keep the original delivery identity with any external effect that must be
deduplicated after a crash.
