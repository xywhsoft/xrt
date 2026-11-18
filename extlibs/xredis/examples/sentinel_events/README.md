# Offline Sentinel event listener

Run the example using the xredis_sentinel_events suite. It starts disposable
local Sentinel and data peers, acquires a lease, then enables the listener.
The existing lease remains held while the peer sends a switch event.
A queued acquire receives a lease from the newly verified generation.
See [listener configuration](../../docs/sentinel_events.md).
