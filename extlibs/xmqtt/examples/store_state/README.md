# Recover accepted payload independently of protocol state

Build with the `xmqtt_store_state_tests` suite, or select `xmqtt_store_state`
plus Core `file_temp` for the default temporary-file path. Run without arguments
to create/sync/reopen a real temporary journal, apply BIND/OPEN/incoming QoS2/END
with zero effective expiry, and read the retained Inbox after native file close.
The example deletes only its own temporary file. An optional existing, exclusively
owned file is initialized when empty or recovered when already populated.

The final state has one pending delivery and no protocol identity/session. No
INBOX_ACK is committed: destroying the memory view does not attest application
completion, and a retained file will recover the delivery again. The packets
illustrate a previously authenticated connection decision; no broker or network
ACK runs here. Temporary-file creation is not a namespace power-loss guarantee.
See [recovery contracts](../../docs/store_state.md) before live integration.
