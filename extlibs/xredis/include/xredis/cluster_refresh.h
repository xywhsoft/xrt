#ifndef XREDIS_CLUSTER_REFRESH_H
#define XREDIS_CLUSTER_REFRESH_H
#include <xredis/cluster_nodes.h>
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_REFRESH) && !defined(XREDIS_FEATURE_XREDIS_CLUSTER_NODES)
#error "Cluster refresh requires maintained node pools"
#endif
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_REFRESH)
typedef struct xredisclusterrefresh xredisclusterrefresh;
typedef struct xredisclustermap xredisclustermap;
typedef enum xredisclusterrefreshstatus {
    XREDIS_REFRESH_OK,XREDIS_REFRESH_STALE,XREDIS_REFRESH_TIMEOUT,XREDIS_REFRESH_CLOSED,
    XREDIS_REFRESH_POLICY,XREDIS_REFRESH_OVERLOADED,XREDIS_REFRESH_TRANSPORT,
    XREDIS_REFRESH_SERVER,XREDIS_REFRESH_PROTOCOL,XREDIS_REFRESH_LIMIT,XREDIS_REFRESH_MEMORY
} xredisclusterrefreshstatus;
typedef enum xredisclusterrefreshstep {
    XREDIS_REFRESH_STEP_ERROR=-1,XREDIS_REFRESH_IDLE,XREDIS_REFRESH_WAIT,
    XREDIS_REFRESH_MORE,XREDIS_REFRESH_DONE,XREDIS_REFRESH_BUSY
} xredisclusterrefreshstep;
typedef struct xredisclusterrefreshconfig {
    size_t Size,MaxSeeds,MaxHostBytes,MaxControlBytes,MaxSnapshotBytes,MaxAttempts,MaxWaits;
    int64 Timeout,RetryDelay;xredisclusterformat Format;xredisclusterlimits Limits;
} xredisclusterrefreshconfig;
/* All views borrow a live Map reference. Origin is the copied query endpoint;
 * its transport defines legacy SLOTS port interpretation and SOURCE fallback.
 * Epoch is a local invalidation counter, not a Redis configuration epoch. */
typedef struct xredisclustermapinfo {
    size_t Size;uint64 Generation,Epoch;xredisclusteraddress Origin;
    xredisclustertopology* Topology;size_t StorageBytes;
} xredisclustermapinfo;
/* Immutable Future value. Native/Map and Query reply/cause borrow that Future;
 * retain either object before escaping. Native can remain pending after stop.
 * QueryObserved never changes after publication. STALE means invalidation raced
 * refresh, so publication does not certify current demand. OK only certifies
 * that demand at publication; later invalidation can stale the same Map. */
typedef struct xredisclusterrefreshresult {
    size_t Size;xredisclusterrefreshstatus Status;uint64 Epoch,Generation;
    size_t Attempts,Waits,ProviderCalls;bool Published,StaleAtPublish,QueryObserved;
    xredisclusterpoolstatus Provider;xredisadmission Admission;xredisclusterstatus Parse;
    xredisclusteraddress Origin;xredisrequestresult Query;xfuture* Native;xredisclustermap* Map;
} xredisclusterrefreshresult;
typedef struct xredisclusterrefreshinfo {
    size_t Size;bool Closing,Closed,Active,Busy,Dirty;
    uint64 DesiredEpoch,PublishedEpoch,Generation,Started,Coalesced,CacheHits,Completed;
    xredisclusterrefreshstatus LastStatus;size_t Seeds,ControlBytes;
} xredisclusterrefreshinfo;
XRT_EXTERN_C_BEGIN
/* Defaults:16 seeds,1024-byte host,1MiB control,32MiB per retained snapshot,
 * 8 native metadata attempts,1024 waits,5s total/5ms delay,SHARDS/full coverage.
 * Bounds:seeds<=1024,host<=65536,control/snapshot<=128MiB,attempts<=1024,
 * waits<=65536,timeout0..24h,delay1..60000ms. Explicit SLOTS is supported;
 * no implicit command/ACL/transport fallback. */
XREDIS_API bool xredisClusterRefreshConfigInit(xredisclusterrefreshconfig* Config);
/* Copies bounded explicit host/port/TLS seeds; Slot/Reason must be zero. Retains
 * Nodes on success; never closes it. No policy/I/O/callback runs during creation.
 * Failure preserves output and ownership. Close before final Release. */
XREDIS_API xredisadmission xredisClusterRefreshCreate(xredisclusternodes* Nodes,
    const xredisclusteraddress* Seeds,size_t Count,const xredisclusterrefreshconfig* Config,
    xredisclusterrefresh** Refresh);
XREDIS_API bool xredisClusterRefreshRetain(xredisclusterrefresh* Refresh);
XREDIS_API void xredisClusterRefreshRelease(xredisclusterrefresh* Refresh);
/* Concurrent callers share one active result Future. Fresh !Force reuses the
 * completed result. Force starts a new local epoch when otherwise fresh; it
 * does not alter an already active cohort. Invalidate explicitly records newer
 * demand. One creation-entry deadline covers setup/admission/retry/query/parse.
 * Joining or cancelling an observer never extends/cancels the cohort. A waiter
 * can use its own WaitFor timeout. Start performs no policy/I/O; a serialized
 * driver must keep stepping. During cohort creation or result-publication reentry Start can return
 * OVERLOADED. Output changes only on ACCEPTED. */
XREDIS_API xredisadmission xredisClusterRefreshStart(xredisclusterrefresh* Refresh,bool Force,xfuture** Future);
XREDIS_API bool xredisClusterRefreshInvalidate(xredisclusterrefresh* Refresh);
/* Replace future query candidates with up to MaxSeeds distinct eligible
 * masters from Map, then its actual query origin and distinct original explicit
 * seeds as fallback (total <=2*MaxSeeds+1). SHARDS uses Map.Origin.Tls and its explicit port; SLOTS keeps
 * its query transport. SOURCE uses the copied query host. Unknown/unhealthy/
 * unavailable/oversize endpoints are skipped. Every later lookup still runs
 * Nodes authorization. No policy/I/O/allocation here; bounded preallocated
 * hash/plan storage and two passes avoid partial replacement.
 * Requires no active cohort/creation/driver phase; otherwise OVERLOADED.
 * Concurrent Close is included in Busy and can return CLOSED. Does not change
 * topology, generation or demand epoch; existing outcomes remain immutable.
 * A failed cohort advances the next start candidate; successful query origin
 * is preferred when it remains an eligible discovered master. */
XREDIS_API xredisadmission xredisClusterRefreshDiscover(xredisclusterrefresh* Refresh,
    const xredisclustermap* Map);
/* Start/Invalidate/Snapshot/Info/Close are thread-safe. Step/Pending/NextWake
 * have ONE caller-serialized driver; no worker blocking. Step makes one bounded
 * phase. BUSY reports another active Step/publication, not permission to spin.
 * WAIT observes Pending (including just-resolved native handles) or NextWake.
 * Only these fixed read-only metadata commands retry transport failures; no
 * business is submitted/replayed. Denied/server/malformed replies stop without
 * replacing an existing view. Original deadlines survive seed failover. */
XREDIS_API xredisclusterrefreshstep xredisClusterRefreshStep(xredisclusterrefresh* Refresh);
XREDIS_API xfuture* xredisClusterRefreshPending(xredisclusterrefresh* Refresh);
XREDIS_API double xredisClusterRefreshNextWake(xredisclusterrefresh* Refresh);
XREDIS_API xredisclusterstatus xredisClusterRefreshSnapshot(xredisclusterrefresh* Refresh,xredisclustermap** Map,bool* Stale);
XREDIS_API bool xredisClusterRefreshInfo(xredisclusterrefresh* Refresh,xredisclusterrefreshinfo* Info);
/* Nonblocking seal. Includes any running provider/driver phase, stops the last
 * native operation and publishes typed CLOSED. Its physical native duties are
 * independent. Closed releases coordinator-owned Nodes/view/result refs. Retained maps and
 * settled outcomes own no coordinator/cache/engine; pending native work keeps
 * independent physical resources. Close does not certify physical engine
 * shutdown: the owner separately closes Nodes and joins its engine. */
XREDIS_API bool xredisClusterRefreshClose(xredisclusterrefresh* Refresh);
XREDIS_API xfuture* xredisClusterRefreshClosed(xredisclusterrefresh* Refresh);
XREDIS_API bool xredisClusterMapRetain(xredisclustermap* Map);
XREDIS_API void xredisClusterMapRelease(xredisclustermap* Map);
XREDIS_API bool xredisClusterMapInfo(const xredisclustermap* Map,xredisclustermapinfo* Info);
XRT_EXTERN_C_END
#endif
#endif
