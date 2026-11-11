#ifndef XREDIS_CLUSTER_ROUTE_H
#define XREDIS_CLUSTER_ROUTE_H
#include <xredis/cluster_topology.h>
#include <xredis/pool_shared_asking.h>
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_ROUTE) && \
    (!defined(XREDIS_FEATURE_XREDIS_CLUSTER_TOPOLOGY) || !defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_ASKING))
#error "Cluster routes require topology and native shared ASKING"
#endif
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_ROUTE)
typedef struct xredisclusterroute xredisclusterroute;
typedef enum xredisclusterpoolstatus {
    XREDIS_CLUSTER_POOL_READY, XREDIS_CLUSTER_POOL_WAIT, XREDIS_CLUSTER_POOL_DENIED,
    XREDIS_CLUSTER_POOL_CLOSED, XREDIS_CLUSTER_POOL_ERROR, XREDIS_CLUSTER_POOL_MEMORY
} xredisclusterpoolstatus;
typedef enum xredisclusterroutestatus {
    XREDIS_CLUSTER_ROUTE_PENDING, XREDIS_CLUSTER_ROUTE_COMPLETE,
    XREDIS_CLUSTER_ROUTE_TIMEOUT, XREDIS_CLUSTER_ROUTE_CANCELLED,
    XREDIS_CLUSTER_ROUTE_NO_ROUTE, XREDIS_CLUSTER_ROUTE_POLICY,
    XREDIS_CLUSTER_ROUTE_LIMIT, XREDIS_CLUSTER_ROUTE_OVERLOADED,
    XREDIS_CLUSTER_ROUTE_CLOSED, XREDIS_CLUSTER_ROUTE_PROVIDER_ERROR,
    XREDIS_CLUSTER_ROUTE_ADMISSION_ERROR, XREDIS_CLUSTER_ROUTE_PROTOCOL,
    XREDIS_CLUSTER_ROUTE_MEMORY
} xredisclusterroutestatus;
typedef enum xredisclusterroutestep {
    XREDIS_CLUSTER_ROUTE_STEP_ERROR=-1, XREDIS_CLUSTER_ROUTE_WAIT=0,
    XREDIS_CLUSTER_ROUTE_MORE=1, XREDIS_CLUSTER_ROUTE_DONE=2
} xredisclusterroutestep;
typedef struct xredisclusteraddress {
    xbytesview Host; uint16 Port, Slot; bool Tls;
    /* Zero for the initial topology choice, otherwise validated MOVED/ASK.
     * ASK applies only to this route and must not change a shared slot map. */
    xredisclusterredirectkind Reason;
} xredisclusteraddress;
/* Runs on the Step caller, outside all library gates. Nonblocking; no reentry
 * on this Route. Authorize the exact address/transport/profile before returning
 * its fixed or maintained SHARED pool. READY transfers one retained Pool ref;
 * every other status leaves output unchanged and transfers nothing. Endpoint
 * input borrows only this invocation. WAIT requests bounded delayed admission,
 * not another business attempt. Provider owns node caches/refresh/coalescing
 * and pool/context close duties independently. Never return a pool for a
 * different endpoint/credentials/transport. No implicit DNS/port/TLS fallback. */
typedef xredisclusterpoolstatus (*xredisclusterpoolproc)(const xredisclusteraddress* Address,
    int64 Remaining, ptr Data, xredispool** Pool);
typedef struct xredisclusterrouteconfig {
    size_t Size, MaxRedirects, MaxWaits, MaxArguments, MaxCommandBytes, MaxHostBytes, MaxStorageBytes;
    int64 Timeout, RetryDelay; bool Tls;
    xredisclusterpoolproc GetPool; ptr Data;
} xredisclusterrouteconfig;
/* Views borrow Route until the next Step, or Destroy at DONE. Native
 * Query/Control/Cause additionally borrow the current
 * native Future: retain ResultFuture before escaping Route's lifetime.
 * RequestObserved is a snapshot at DONE, even if a stopped native Future later
 * resolves. COMPLETE means a terminal native outcome, including a server error
 * or uncertain transport failure; inspect Query.Status/Delivery. A parent stop
 * can coexist with an already observed native outcome. No result authorizes
 * replay after timeout/cancel/disconnect or denied ASKING control. */
typedef struct xredisclusterrouteresult {
    size_t Size; xredisclusterroutestatus Status;
    xredisclusteraddress Address, AttemptAddress;
    size_t Attempts, Redirects, Moved, Asks, Waits, ProviderCalls;
    /* Controller requested storage only; native Future/reply/physical duties
     * remain bounded by their independent pool/client limits. */
    size_t StorageBytes;
    xredisclusterpoolstatus Provider; xredisadmission Admission;
    bool AdmissionObserved, RequestObserved, UsedAsking, ControlReceived;
    xredisrequestresult Query, Control;
} xredisclusterrouteresult;
XRT_EXTERN_C_BEGIN
/* Defaults: 5s total, 5ms admission wait, 8 redirects, 1024 waits/arguments,
 * 1MiB encoded command, 1024-byte host, 2MiB controller storage. GetPool required.
 * Timeout0 is due; maximum24h, delay1..60000ms, redirects<=1024/waits<=65536,
 * arguments1..65536, host1..65536, command<=64MiB/storage<=128MiB. */
XREDIS_API bool xredisClusterRouteConfigInit(xredisclusterrouteconfig* Config);
/* Nonblocking construction. Slot is explicit: caller computes it and ensures
 * all command keys belong to that slot. Master ONLINE/UNREPORTED only; no replica
 * fallback. SHARDS chooses the explicit TCP/TLS port; SLOTS uses its query's
 * transport-specific Port. SOURCE uses OriginHost from that topology query;
 * UNKNOWN, unavailable port or missing slot finishes NO_ROUTE without a provider.
 * One bounded allocation copies descriptors/payload and owns current/last-attempt
 * host buffers sized for the initial endpoint. A longer redirect host grows
 * them once, bounded by MaxHostBytes/MaxStorageBytes; that cold allocation can
 * finish MEMORY/LIMIT without another business attempt. Address is the current
 * candidate; AttemptAddress belongs to the
 * last accepted native attempt and its observed Query/Control outcomes.
 * Topology/config/input need live only for this API; Data/code until DONE or
 * Destroy. Copies add real cost versus direct native calls. No worker, timer,
 * proxy Future or engine pin. Output changes only on ACCEPTED, including a
 * locally terminal NO_ROUTE/TIMEOUT. Failure consumes nothing. */
XREDIS_API xredisadmission xredisClusterRouteCreate(const xredisclustertopology* Topology,
    xbytesview OriginHost, uint16 Slot, const xbytesview* Arguments, size_t Count,
    const xredisclusterrouteconfig* Config, xredisclusterroute** Route);
/* Unique caller serialization for every API; never block a worker. Each Step
 * advances one bounded phase. MORE schedules another turn; WAIT observes Pending
 * or NextWake, whichever is earlier. Do not busy-poll. One original monotonic
 * deadline includes construction, caller scheduling, provider/admission waits
 * and every redirected native query. Native attempts inherit that exact deadline.
 * Only a well-formed matching-slot MOVED/ASK server reply can trigger another
 * attempt; ASK uses adjacent native ASKING/query. Control must succeed before
 * following another redirect. Other server/transport outcomes are final.
 * A MOVED can inform provider cache/refresh policy; this controller never mutates
 * its input snapshot. RetryDelay/MaxWaits bound nonaccepted provider/overload wait.
 * Caller-owned pools keep their independent maintenance lifetime. */
XREDIS_API xredisclusterroutestep xredisClusterRouteStep(xredisclusterroute* Route);
/* Retain the current native wait handle in the NATIVE phase, even if it has
 * just resolved. This closes the completion window between WAIT/NextWake and
 * subscribing to that handle. NULL during provider/backoff or after DONE. */
XREDIS_API xfuture* xredisClusterRoutePending(xredisclusterroute* Route);
/* Absolute xrtTimer seconds; zero at DONE. Pending completion may wake earlier. */
XREDIS_API double xredisClusterRouteNextWake(xredisclusterroute* Route);
XREDIS_API bool xredisClusterRouteResult(xredisclusterroute* Route, xredisclusterrouteresult* Result);
/* At DONE, retain the last accepted native attempt, or NULL if none. It can
 * still be pending after a parent stop. It is not a proxy success Future. */
XREDIS_API xfuture* xredisClusterRouteResultFuture(xredisclusterroute* Route);
/* Immediate nonblocking logical stop, asks the current native operation to
 * cancel. Independent native physical FIFO/storage duties survive this object.
 * Retained DONE metadata holds no pool/engine/context reference. */
XREDIS_API bool xredisClusterRouteCancel(xredisclusterroute* Route);
XREDIS_API void xredisClusterRouteDestroy(xredisclusterroute* Route);
XRT_EXTERN_C_END
#endif
#endif
