# Redis Tracking and ordered observations

`xredis_tracking` adds owned receive-order metadata to the existing Client and
server-assisted caching protocol controls. It supplies no local cache, replacement
connection or extra worker. TCP, Dial and TLS use the same opt-in on the returned
Client. The complete four-product plan remains active.

## Connection and control

Use managed strict RESP3 setup, wait for readiness, then enable observations with
`xredisClientTrackingObserve`. Its byte limit is separate from the Client limits;
zero selects 64 KiB. The allocation is a 16-byte header plus `MaxPushes` eight-byte
slots on x64. Concurrent constructors reserve this storage before allocation;
only one can allocate, and normal overload preserves ambient error. Info reports
live/reserved bytes, preparation and counted refusals. Once enabled it is never
reset or disabled; configuration OFF leaves observation intact. The block belongs
to Client until final release.

Initialize `xredistrackingconfig`, then submit
`xredisClientTrackingConfigureAsync`. Binary PREFIX values are snapshotted by the
ordinary command writer. BCAST, OPTIN, OPTOUT, NOLOOP and REDIRECT are explicit;
invalid combinations and excessive prefixes fail before publication. OFF cannot
hide active options. Direct invalidations require known managed RESP3 (strict
RESP3 may queue during setup; AUTO must first select3). RESP2 senders need a
positive REDIRECT. The recipient identity and permissions remain server decisions.
A successful configuration validates the specified simple OK; server errors stay
observable. Unexpected non-error responses retire the connection as PROTOCOL,
including when the caller already cancelled the control.

Clear the application cache before changing configuration and resume it only
after a successful barrier. Sent timeout/cancellation can leave the server mode
uncertain. Existing FIFO obligations drain under the original Client grace; no
control is replayed automatically. For concurrent OPTIN/OPTOUT producers use
[`xredisClientCachingCallAsync`](caching.md): it places CLIENT CACHING and its
query in one adjacent request, and independently returns both outcomes. Two
separate ordinary submissions still require producer synchronization.

## Receive order and ownership

Observed calls return `xredistrackingresult`: the unchanged ordinary Request prefix
and one uint64 Sequence. `xredisClientTrackingTake` transfers a reply reference and
its received Sequence from the existing push queue. It also returns unrelated
pushes; callers must route rather than silently discard them. Do not mix ordinary
and ordered push consumers. Both result/push scalar metadata and retained replies
can outlive all clients, their Engine and Resolver.

Every fully parsed frame after activation increments one owner-worker counter,
including ordinary and abandoned replies and initialization traffic. Gaps are
valid. Queued pre-activation pushes have zero and precede every newly admitted
observed call; start activation with an empty application cache. Local outcomes
without a published reply have zero. A completed Future is immutable even when
its abandoned frame subsequently drains. UINT64_MAX is usable once; exhaustion
closes with LIMIT rather than wrapping. Info.Sequence is parser progress, not a
barrier proving that application queues are drained.

For the same connection, retain the latest invalidation sequence per relevant
key/cache epoch. Store a query result only when its sequence is newer. A delayed
older invalidation must not evict a newer read. This distinguishes both wire
orders even when application code collects Futures before pushes. Cross-connection
REDIRECT sequences cannot be compared; use application generation/placeholders
and invalidate work in progress. A single cache owner must process notifications
and local reads consistently. Flush/stop serving on loss of the invalidation
connection, push overflow, malformed tracking events or broken redirection;
heartbeat/liveness and application data eviction are caller policies. NOLOOP also
requires invalidating the application's own writes.

## Event views

`xredisTrackingEventRead` fully validates recognized RESP3 invalidate and
tracking-redir-broken events and the legacy RESP2 `__redis__:invalidate` Pub/Sub
envelope. It preserves binary/empty keys, null flush, attributes in the original
reply, and the failed redirect ID. OTHER and MALFORMED preserve output and ambient
error. Invalid API use reports ERROR and does not write into any reply allocation.
Walk Keys sibling nodes from FirstKey using `xredisReplyGet`/Next; this is one linear
walk with borrowed views and no copy/allocation. Keep a reply reference while
using these views. The decoder does not add RESP2 subscribed transport mode to
the ordinary one-reply Client; that dedicated runtime remains separate work.

## Costs and verification

Opting in allocates once, then reuses the bounded sequence ring. Observed results
enlarge the existing result allocation by eight x64 bytes; no extra Future,
request allocation or command payload copy is introduced. The selected Client
has private observation bookkeeping. An unobserved selected connection checks a
null atomic pointer per parsed frame; it does not update a counter or allocate
observation storage. Feature-excluded builds omit this path. The decoded views
allocate nothing. The benchmark includes the entire loopback process, immutable
reply construction, queued pushes and caller collection. Setup/physical shutdown
are outside its timed region; offer-to-collection latency includes pipeline wait
and event decoding. It is not a fixed-arrival or external Redis performance gate.

The runnable example demonstrates delayed collection and retained ownership.
Unit assets separately exercise wire order, copied inputs, invalid flags, aliases,
malformed controls, existing queues, cancellation/drain, concurrent producers and
allocation budgets. `tests/interop/tracking_boundaries.py` compiles the actual
production Client TU with test-only allocation/free wrappers, holds real physical
close barriers, forces scoped OOM, checks private output aliases and starts the
counter near exhaustion with its writer held. No test hook is installed in the
production library. Native Redis matrix assets cover direct/redirected modes and
TCP/TLS/Dial; actual accepted runs and limitations belong in the stage evidence.

References: [CLIENT TRACKING](https://redis.io/docs/latest/commands/client-tracking/),
[client-side caching](https://redis.io/docs/latest/develop/clients/client-side-caching/).
