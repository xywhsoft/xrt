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
The asynchronous client wires MOVED demand and automatically discovers future
metadata seeds from each published map. Replica policy and final acceptance
remain work.

Discover runs only with no active cohort/phase, otherwise OVERLOADED. One
constructor-preallocated address arena, hash and index plan select at most
MaxSeeds distinct eligible masters, preserve the actual query origin and all
distinct original bootstrap seeds (total<=2*MaxSeeds+1). SHARDS explicitly selects
the query transport's port; SLOTS preserves query transport. SOURCE uses the
copied query host. Unknown/unhealthy/unavailable/oversize candidates are skipped.
Every later lookup still authorizes the exact address. Planning is bounded and
allocates no storage, with no partial replacement or epoch/map mutation. Close
includes the Busy phase. Failed cohorts advance the next start candidate, so a
small MaxAttempts does not permanently starve later seeds. Successful query
origin is preferred when eligible; other discovered masters precede fallback.
This adds bounded cold constructor storage and bounded metadata-phase CPU work.
The host arena reserves (MaxSeeds+1)*MaxHostBytes regardless of initial seed
count (17KiB with defaults), plus address/hash/plan storage and original hosts.
Zero-wait cohorts still record the actual provider address and advance the next
start; native observations retain their last accepted query origin separately.

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
