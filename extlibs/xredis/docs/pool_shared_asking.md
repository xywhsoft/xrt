# Shared native ASKING groups

The developing `xredis_pool_shared_asking` leaf executes ASKING plus one ordinary
query on fixed or automatically maintained shared pools. It returns the existing
immutable `xredisaskingresult` from one native request/Future, with separate
control and business outcomes. There is no lease, proxy Future, extra payload
clone or new runtime. Selection uses the same bounded ready-list rotation and
native claim/state admission as SharedPoolCall. Only rejected constructors may
probe another member; accepted business requests never migrate or replay.

The two commands occupy one adjacent native wire. Concurrent ordinary/group
producers cannot interleave between them on the selected connection. Both are
pipelined: the query MAY EXECUTE even when ASKING is denied. Inspect both
outcomes; a control error does not authorize a retry. Known mode/connection/
transaction/subscription controls and CLIENT are refused as group queries.

Call timeout begins at API entry, includes selection/construction/both replies,
and has no lease-hold cap. No eligible capacity returns OVERLOADED immediately;
there is no outage queue. Prefix and aligned request-only state count toward
native/client and pool request/byte limits. There is one physical request slot,
not one per command. Sent timeout/cancel retains native two-reply FIFO, partial
wire and drain obligations. Following ordinary traffic can be admitted while a
group drains, but cannot consume a leftover one-shot flag or its replies. Full
wire/replies drain, or the native member retires. Optional maintenance can repair
that member without replaying the group. Results record immutable control-prefix
knowledge at logical completion and retain neither the pool nor engine.

Sync rejects every worker of the pool engine; async callback reentry is allowed.
Callers retain Pool across each API, bound their own admission policy and use
PoolClose/Closed/Release normally. There is no topology refresh, ASK address
selection, redirect following or replica policy in this execution primitive.
See the [runnable example](../examples/pool_shared_asking/README.md). Full Cluster
routing and production acceptance remain subsequent work.
