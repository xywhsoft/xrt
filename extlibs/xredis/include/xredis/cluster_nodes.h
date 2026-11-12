#ifndef XREDIS_CLUSTER_NODES_H
#define XREDIS_CLUSTER_NODES_H
#include <xredis/cluster_route.h>
#include <xredis/pool_shared_connect.h>
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_NODES) && \
    (!defined(XREDIS_FEATURE_XREDIS_CLUSTER_ROUTE) || !defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_CONNECT))
#error "Cluster nodes require routes and maintained shared pools"
#endif
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_NODES)
typedef struct xredisclusternodes xredisclusternodes;
typedef enum xredisclusternodesstate {XREDIS_CLUSTER_NODES_OPEN,XREDIS_CLUSTER_NODES_CLOSING,XREDIS_CLUSTER_NODES_CLOSED} xredisclusternodesstate;
/* Runs on each lookup caller outside gates, including cache hits. Nonblocking,
 * thread-safe; immutable profile/authority for this cache. May inspect/close
 * Cache, but must not recursively call GetPool. No ownership transfer. */
typedef bool (*xredisclusterauthorizeproc)(const xredisclusteraddress* Address,ptr Data);
/* Runs on each node's pool worker; different nodes may call concurrently.
 * Same managed-client factory rules as PoolConnect: private reference transfers
 * on success only. Address borrows a stable copied key; Slot/Reason are zero.
 * Honor exact transport/endpoint/credentials/session profile and total per-
 * attempt Timeout; never block. Client setup may outlive one caller's route. */
typedef bool (*xredisclusterconnectproc)(const xredisclusteraddress* Address,
    xnetengine* Engine,size_t Slot,uint64 Attempt,int64 Timeout,ptr Data,xredisclient** Client);
typedef struct xredisclusternodesconfig {
    size_t Size,MaxNodes,MaxHostBytes,MaxStorageBytes,ConnectionsPerNode,MaxConnections,MaxLookups;
    xredispoolconfig Pool;
    xredisprotocol Protocol;size_t MaxConnecting;
    int64 AttemptTimeout,RetryMin,RetryMax;
    xredisclusterauthorizeproc Authorize;xredisclusterconnectproc Connect;
    xredispoolconnectdrop Drop;ptr Data;
} xredisclusternodesconfig;
typedef struct xredisclusternodesinfo {
    size_t Size;xredisclusternodesstate State;
    size_t Capacity,Nodes,Opening,APIs,StorageBytes,ConnectionBound,ConnectingBound,LookupBound;
    uint64 RequestBound,CommandBytesBound,Hits,Misses,Waits,Denied,Created,Retired,Failures;
} xredisclusternodesinfo;
XRT_EXTERN_C_BEGIN
/* Defaults:64 nodes,1024-byte hosts,1MiB controller storage,1 connection/node,
 * 64 aggregate connections,1024 concurrent lookups; native pool defaults; RESP3,4 attempts/node,
 * 5s setup and100ms..10s retry. Authorize/Connect mandatory. Fixed limits:
 * nodes/connections/lookups<=65536,host<=65536,storage<=128MiB. Aggregate native quota
 * bounds are MaxNodes times the per-node Pool limits, with checked arithmetic. */
XREDIS_API bool xredisClusterNodesConfigInit(xredisclusternodesconfig* Config);
/* One preallocation holds hash buckets/slots/copied hosts. Does not start
 * connections or call callbacks. ACCEPTED consumes Data/Drop; failure preserves
 * them and output. Cache pins Engine until Closed; close before releasing.
 * All APIs are thread-safe, each caller holds a retained cache reference. */
XREDIS_API xredisadmission xredisClusterNodesCreate(xnetengine* Engine,uint64 Affinity,
    const xredisclusternodesconfig* Config,xredisclusternodes** Cache);
XREDIS_API bool xredisClusterNodesRetain(xredisclusternodes* Cache);
XREDIS_API void xredisClusterNodesRelease(xredisclusternodes* Cache);
/* Direct Route.GetPool adapter (Data=Cache). Exact copied host/port/TLS key;
 * Slot/Reason do not create duplicate node pools. Authorization runs on every
 * call. One concurrent creator per key; others WAIT. Hits allocate nothing and
 * transfer one pool ref on READY; all other outcomes preserve output. READY
 * means a maintained pool exists, not that its members are already ready.
 * Native overload waits are bounded by the route's original deadline.
 * Remaining0 performs no setup. Shared maintenance has independent bounded
 * attempts and lasts until node retirement/cache close, not caller timeout.
 * Capacity includes opening/retiring nodes; MaxLookups bounds concurrent
 * authorizations/constructors (Close can add one mechanical API duty). Full
 * capacity/concurrency returns WAIT without invoking policy/setup; no LRU
 * aborts active business. Retired slots reuse only after physical PoolClosed.
 * Endpoint policy/factory code/data live until Drop. No hidden DNS/TLS fallback. */
XREDIS_API xredisclusterpoolstatus xredisClusterNodesGetPool(const xredisclusteraddress* Address,
    int64 Remaining,ptr Cache,xredispool** Pool);
/* Explicit endpoint fence, idempotent if absent. Stops new lookups for this key
 * until its physical close retires; closes accepted native work with the usual
 * typed/possibly uncertain outcomes. Retained selected pools may reject later
 * admissions CLOSED. Retirement never replays accepted business. */
XREDIS_API bool xredisClusterNodesRetire(xredisclusternodes* Cache,const xredisclusteraddress* Address);
XREDIS_API bool xredisClusterNodesInfo(xredisclusternodes* Cache,xredisclusternodesinfo* Info);
/* Nonblocking idempotent seal; closes every current/constructing node. Closed
 * resolves after constructors/authorization calls and all native pool close
 * notifications retire, Drop runs once outside gates, and Engine pin is gone.
 * Retained closed metadata/Future pins no context/pool/engine. Join Engine before
 * unloading callback code. Release alone does not close an open cache. */
XREDIS_API bool xredisClusterNodesClose(xredisclusternodes* Cache);
XREDIS_API xfuture* xredisClusterNodesClosed(xredisclusternodes* Cache);
XRT_EXTERN_C_END
#endif
#endif
