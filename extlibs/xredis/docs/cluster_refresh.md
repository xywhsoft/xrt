# Coalesced Cluster metadata refresh

The developing xredis_cluster_refresh leaf fetches read-only CLUSTER metadata
through authorized maintained pools. SHARDS is the default; legacy SLOTS is an
explicit configuration, with no command/ACL/transport fallback. Formats follow
the official [SHARDS](https://redis.io/docs/latest/commands/cluster-shards/)
and [SLOTS](https://redis.io/docs/latest/commands/cluster-slots/) documentation.

Create copies bounded explicit host/port/TLS seeds and retains Nodes without
I/O. Start coalesces concurrent callers onto one result Future; fresh !Force
reuses the last result. No per-waiter queue or worker/timer is added. One
caller-serialized Step/Pending/NextWake driver progresses bounded phases and
must keep running to publish timeout/completion. A creation-entry monotonic
deadline covers setup/admission, seed failover, query and parsing. Native calls
inherit the exact Entered/Timeout pair. Waits and accepted metadata attempts
have separate hard caps. Only the fixed read-only commands retry transport
failures; business is never submitted. Denied/server/malformed replies stop and
preserve an existing view. Native handles remain observable when just resolved.

Each immutable Map owns a topology ref and copied query origin. SOURCE uses
that host; SLOTS port meaning follows query transport. Snapshot acquisition
returns one retained Map. MapInfo borrows topology/origin until Map release.
Generations increase on publication. Invalidation advances a local demand
epoch, not a Redis configuration epoch. A refresh racing newer demand can
publish STALE but cannot clear Dirty. OK certifies demand at publication only;
later invalidation can stale it. Failed replacement retains the previous view.
Fully managed MOVED notification wiring, seed selection from refreshed maps,
replica policy and async scheduling remain work.

Controller/seed/result allocation has a control cap, reserving two operation
boxes for the active and last result (including creation overlap). Per-map caps include the
view/host, topology and retained Reply reservation; native parser/request quotas
remain independently configured in the node factory. One active and one previous
result/view can coexist; caller-retained old maps/Futures are caller-owned
storage. Constructor/cohort failure preserves output and ownership. Creation or
publication reentry can return OVERLOADED. Snapshot acquisition allocates no
storage, but takes a gate and Map ref. Refresh allocates a shared Future,
operation box, topology/view and native wire, and copies seeds/origins. These
are real costs; no zero-overhead claim is made.

Close seals, includes a running provider/driver phase, publishes immutable CLOSED
and requests native stop. Physical FIFO duties remain independent; the last
native Future can remain pending. Parent QueryObserved/Query never change
afterwards. Close releases cache/view/result refs and does not close a shared
Nodes cache. Its owner separately closes Nodes and joins the engine before
freeing factory material/unloading code. Retained maps and settled outcomes own no coordinator,
cache or engine; pending native work retains independent physical resources. WaitFor timeouts/cancelled observers do not cancel the cohort.
Full real TLS/auth/trust, upstream/native Linux/fixed-arrival/mixed-load/CPU/RSS/
72h acceptance and the complete managed client remain open.
