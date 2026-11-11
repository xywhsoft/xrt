# xredis command feature evidence

The Redis request wire format is an array of bulk strings, shared by RESP2 and
RESP3. The implementation is independent and follows the
[public protocol](https://redis.io/docs/latest/develop/reference/protocol-spec/).
This document concerns command encoding. The incremental response parser has
[separate contracts and evidence](reader.md). The [request runtime](client.md)
and [initialization interoperability](client-init.md) have separate evidence.
Full product and production acceptance remain pending.

## Contract matrix

| Contract | Implementation | Verification |
| --- | --- | --- |
| Exact array and bulk framing | `xredisCommandSize`, `xredisCommandWrite` | Known PING bytes and independent wire reader |
| Binary payload and empty arguments | Explicit xbytesview lengths; no strlen | Embedded NUL, CRLF, 0xff, NULL/0 and 10,000 randomized commands |
| Decimal transitions | Checked size calculation and decimal writer | Argument counts 9..101; payload lengths 0..1001 |
| No output mutation on failure | Complete preflight before first write | Capacity, missing inputs, descriptor/payload/output aliases, overflow cases |
| Bounded arithmetic | INT64_MAX wire lengths, SIZE_MAX sizes, public XRT span checks | Count, encoded size and address-wrap rejection without payload dereference |
| Ownership | Inputs borrowed only during call; caller owns output | Aliasing tests; no retained objects |
| Successful path allocates nothing | Caller-buffer writer, public range helpers | 10,000 successful writes with unchanged memory allocation counters; success under one-byte quota |
| Diagnostics can fail safely | XRT error creation/fallback | Forced diagnostic allocation failure preserves output and reports XERR_MEMORY |
| Success preserves ambient error | No error clearing on success | Exact pre-existing error identity retained |
| Minimal module and real package | command leaf, no network/JSON dependencies | Trim probes, static/shared external consumers, same Core diagnostic observed |

## Verified environments

- Windows x64 GCC 16.2.0: strict C11 warnings, modular/single tests, O2 example,
  module trimming and independent static/DLL consumers.
- Ubuntu under WSL x64 GCC: modular/single tests under O2 ASan+UBSan, independent
  static/shared consumers. WSL evidence is explicitly not bare-metal Linux
  networking or production-performance evidence.
- Root tooling tests: API/document generation and common extension layout.
- Normal product asset maturity passes; strict release correctly refuses the
  still-developing xredis root.

Local raw logs are under `out/service-extensions/evidence/` (ignored build
outputs). The first diagnostic-quota fixture run missed the required Info.Size
initialization; it was corrected and the complete tests rerun. Its failed log
is retained separately, not treated as accepted evidence.

The Windows command performance smoke has a single sample and no external
comparator. It establishes workload execution only, not a stable baseline,
latency target or performance acceptance. The writer copies payloads once and
is not an end-to-end zero-copy transport.

Optional receive notification and batch transfer now have the documented
`xredis_subscriber_notify` leaf; see [contracts](subscriber_notify.md). Shared
readability, physical-close wake, FIFO owned batches, private wait cancellation,
callback reentry, allocation rollback and competing consumers are covered by its
focused modular/single/TLS tests. Desired-state replay and reconnection remain
separate incomplete subscriber work.

`xredis_subscriptions` adds desired-set snapshots and bounded replacement replay;
see [recovery contracts](subscriptions.md). Automatic subscriber maintenance,
backoff, live reconciliation and routed HA/Cluster recovery remain separate work.

The developing subscription-session controller adds actual worker factories, shared
attempt deadlines, physical-retirement-before-retry, bounded jitter, persistent
control claims and live incremental desired-set reconciliation. This does not
complete Sentinel/Cluster, mixed-load or production long-run acceptance.

## Cluster topology

The developing `xredis_cluster_topology` leaf covers RESP2/3 SHARDS, legacy
SLOTS, bounded immutable lifetime, indexed slots and borrowed MOVED/ASK views.
Automatic Cluster connection/routing, ASKING execution, refresh, resharding and
production acceptance remain open. See [contracts](cluster_topology.md).

## Native leased ASKING

`xredis_pool_asking` supplies adjacent native ASKING/query execution, separate
control/business outcomes, bounded physical drain and reuse. See
[contracts](pool_asking.md). Automatic Cluster routing/redirect refresh and
production acceptance remain open.

## Native shared pools

The developing `xredis_pool_shared` leaf adds fixed managed-member multiplexing,
bounded round robin, native physical quotas/stops and immediate overload.
See [contracts](pool_shared.md). Node reconnection, Cluster routing and
production performance acceptance remain open.

## Automatic shared pools

The developing `xredis_pool_shared_connect` leaf combines shared dispatch with
native bounded connection maintenance. Replacement waits for native readiness
and physical retirement; accepted business requests never replay. See
[contracts](pool_shared_connect.md). Cluster routing/refresh and full production
acceptance remain work.

## Shared native ASKING

The developing `xredis_pool_shared_asking` leaf combines native ASKING groups
with shared member selection, physical quotas and concurrent ordinary traffic.
See [contracts](pool_shared_asking.md). It does not follow redirects or complete
Cluster routing/refresh or production acceptance.

## Bounded Cluster routes

The developing `xredis_cluster_route` leaf selects an eligible snapshot master,
follows validated MOVED/ASK, bounds provider/admission waits and redirects, and
retains native outcomes through cancellation and physical teardown. See
[contracts](cluster_route.md). Managed node caches, automatic topology refresh,
bootstrap/replica policy and production performance acceptance remain open.
