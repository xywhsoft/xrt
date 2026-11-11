# Adjacent CACHING/query groups

`xredisClientCachingCallAsync` copies `CLIENT CACHING YES|NO` and one ordinary
query into a single bounded wire allocation. `xredisClientCachingCall` adds a
wait and rejects every worker of the same Engine before admission. A managed
connection and `xredisClientTrackingObserve` are required. RESP2 managed senders
can use this with a separately configured RESP3 redirection recipient.

The two frames are adjacent even with concurrent ordinary/observed producers
and transactions. The Client uses its existing queue, TCP/TLS writer, one request
credit, one deadline, one Future, cancellation watch and drain timer. No earlier
request drain or prefix-ACK round trip is imposed; unrelated requests can still
pipeline before/after the group. Known setup, transaction, CLIENT, QUIT,
READONLY/READWRITE/ASKING and multiple-reply mode controls are refused as queries.
Unknown module commands must obey the existing one-reply contract.

## Independent results

The Future owns an immutable `xrediscachingresult`. Its first member, `Query`,
has the ordinary request prefix plus receive sequence. `Control` independently
records prefix status, delivery, reply/cause and receive sequence.
`ControlReceived` says whether the prefix response was known at logical
completion. A missing reply has sequence zero. Each actual parsed reply/push
advances the existing observer, including late abandoned frames. Pushes do not
consume either of the two reply positions.

This is an adjacent pipeline, not rollback. The query can execute when the
prefix returns SERVER_ERROR. Its own result remains available; neither status
overwrites the other. Require successful Control and Query before using a value
for a cache, then apply the invalidation ordering contract. An unexpected
non-error prefix reply closes as PROTOCOL, including after local abandonment;
it is not published as a valid Control reply. No cache, reconnection or replay is
introduced. Mode changes still need application synchronization.

Before any transport acceptance, cancellation/expiry removes the whole group
and reports NOT_SENT. Once any byte is accepted, the query conservatively reports
MAY_HAVE_EXECUTED even when only part of the prefix was written. Its original
wire group continues under the configured drain grace, so a one-shot flag cannot
be applied to a later unrelated command. The Client closes if that bounded drain
fails. A known query response certifies REPLIED; local cancellation/timeout can
still discard its data and publish sequence zero. The original Future never
changes as physical replies arrive. Close/transport failure retains any already
known Control outcome and reports the remaining uncertainty explicitly.

Both replies/causes are borrowed from the Future and survive Client, Engine and
Resolver destruction. Retain individual replies before destroying the Future.
Dropping an unresolved Future does not eliminate its wire/FIFO obligation.

## Bounds and costs

MaxCommandBytes and MaxCommandBytesTotal charge the combined encoded frames
plus aligned private pair state, as transactions do. Construction reserves the
whole charge before allocation and refunds it after physical storage release.
One group consumes one MaxRequests/MaxInFlight slot. No payload zero pass or
second query encoding is added. The result replaces the existing result
allocation; state occupies the aligned tail of the existing wire allocation.
The existing uint8 observation tag selects the path. Ordinary public results,
configuration and request allocation layout remain unchanged.

See [the runnable example](../examples/caching/README.md). Unit tests include
binary snapshots, concurrent producers and async reentry, both Engine workers,
independent errors, cancellation/deadline/close/drain, all construction quota
prefixes, exact group byte/count bounds, retained ownership, mixed transactions,
and TLS three-byte write turns. The TLS single test inspects actual request state
only while the owning worker is explicitly held, proving partial-prefix and
ACK-during-partial-query cancellation positions. Independent Redis probes check
the complete OPTIN/OPTOUT invalidation sets under four producers and RESP2
redirection. Performance and broader production acceptance remain separate gates.

Protocol sources: [CLIENT CACHING](https://redis.io/docs/latest/commands/client-caching/)
and [client-side caching reference](https://redis.io/docs/latest/develop/reference/client-side-caching/).
