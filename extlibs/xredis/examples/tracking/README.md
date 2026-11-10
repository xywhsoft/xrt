# Ordered Redis invalidation example

Build/run the `xredis_tracking` suite using the root build tool. No Redis process
or credentials are needed. The local literal peer emits an invalidation followed
by a query reply, while the application deliberately collects the query first.
The program checks the shared wire sequence, borrowed binary-key view and outputs
retained after Client/Engine shutdown. It prints
`reply_sequence=2 invalidation_sequence=1 keys=1`.

The peer is a protocol fixture, not a Redis server or performance comparator.
For a real connection, choose strict RESP3 initialization, enable observation,
configure Tracking and verify its successful result before caching query data.
See [contracts](../../docs/tracking.md) for ordering, failure and mode boundaries.
