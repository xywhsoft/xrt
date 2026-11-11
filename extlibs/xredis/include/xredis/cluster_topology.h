#ifndef XREDIS_CLUSTER_TOPOLOGY_H
#define XREDIS_CLUSTER_TOPOLOGY_H
#include <xredis/reply.h>
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_TOPOLOGY) && !defined(XREDIS_FEATURE_XREDIS_REPLY)
#error "Cluster topology requires immutable replies"
#endif
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_TOPOLOGY)
#define XREDIS_CLUSTER_SLOTS 16384u
typedef struct xredisclustertopology xredisclustertopology;
typedef enum xredisclusterformat {XREDIS_CLUSTER_SHARDS=1,XREDIS_CLUSTER_LEGACY_SLOTS} xredisclusterformat;
typedef enum xredisclusterstatus {
    XREDIS_CLUSTER_ERROR=-1,XREDIS_CLUSTER_MALFORMED=-2,XREDIS_CLUSTER_LIMIT=-3,
    XREDIS_CLUSTER_MEMORY=-4,XREDIS_CLUSTER_SERVER_ERROR=-5,
    XREDIS_CLUSTER_ABSENT=0,XREDIS_CLUSTER_VALUE=1
} xredisclusterstatus;
typedef enum xredisclusterendpoint {XREDIS_CLUSTER_ENDPOINT_HOST=1,XREDIS_CLUSTER_ENDPOINT_SOURCE,XREDIS_CLUSTER_ENDPOINT_UNKNOWN} xredisclusterendpoint;
typedef enum xredisclusterrole {XREDIS_CLUSTER_ROLE_OTHER=0,XREDIS_CLUSTER_MASTER,XREDIS_CLUSTER_REPLICA} xredisclusterrole;
typedef enum xredisclusterhealth {XREDIS_CLUSTER_HEALTH_OTHER=0,XREDIS_CLUSTER_ONLINE,XREDIS_CLUSTER_FAILED,XREDIS_CLUSTER_LOADING,XREDIS_CLUSTER_UNREPORTED} xredisclusterhealth;
typedef enum xredisclusterredirectkind {XREDIS_CLUSTER_MOVED=1,XREDIS_CLUSTER_ASK} xredisclusterredirectkind;
typedef struct xredisclusterlimits {
    size_t Size,MaxShards,MaxNodes,MaxRanges,MaxReplyNodes,MaxPayloadBytes,MaxEndpointBytes,MaxOwnedBytes;
    bool RequireFullCoverage;
} xredisclusterlimits;
/* Views are owned by a live topology reference; no NUL terminator. SOURCE means
 * use the actual host used for the topology query. UNKNOWN ('?') must not use
 * that fallback. SHARDS has separate TCP/TLS ports (0 means unavailable);
 * legacy SLOTS Port belongs to the transport used for that query. */
typedef struct xredisclusternode {
    xbytesview Id,Host,Ip,Hostname,RoleText,HealthText;
    size_t Shard;int64 ReplicationOffset;
    uint16 Port,TlsPort;xredisclusterendpoint Endpoint;
    xredisclusterrole Role;xredisclusterhealth Health;
} xredisclusternode;
typedef struct xredisclusterrange {uint16 First,Last;size_t Master,Shard;} xredisclusterrange;
typedef struct xredisclustertopologyinfo {
    size_t Size,Shards,Nodes,Ranges,AssignedSlots,StorageBytes,RetainedReplyBytes;
    xredisclusterformat Format;
} xredisclustertopologyinfo;
/* Borrowed from the existing Reply reference. ASK targets only the current
 * query and requires exclusive ASKING+query execution, not a permanent map edit.
 * Empty Host uses the original request's host; IPv6 brackets are stripped. */
typedef struct xredisclusterredirect {xredisclusterredirectkind Kind;uint16 Slot,Port;xbytesview Host;xredisclusterendpoint Endpoint;} xredisclusterredirect;
XRT_EXTERN_C_BEGIN
/* Binary-safe CRC16/XMODEM with first-pair nonempty hash tags. O(key bytes),
 * no allocation or ownership. Empty keys are valid. Output must be disjoint. */
XREDIS_API bool xredisClusterKeySlot(xbytesview Key,uint16* Slot);
/* Classify SIMPLE_ERROR/BULK_ERROR only. MaxEndpointBytes is a real cap,
 * including brackets; slot/port decimal fields have fixed maximum lengths.
 * VALUE alone writes Redirect. Non-values preserve output and ambient error.
 * Input must remain owned; never mutates a topology or executes/replays calls. */
XREDIS_API xredisclusterstatus xredisClusterRedirectRead(const xredisreply* Reply,size_t MaxEndpointBytes,xredisclusterredirect* Redirect);
/* Defaults:4096 shards/nodes,16384 ranges,262144 reply nodes,4MiB payload,
 * 1024 endpoint bytes,32MiB total requested retained+snapshot storage, partial
 * coverage allowed. Zero is a limit. MaxNodes<=65535; shards/ranges<=16384. */
XREDIS_API bool xredisClusterLimitsInit(xredisclusterlimits* Limits);
/* Complete two-pass shape validation; detects overlapping/out-of-range slots,
 * duplicate known map fields and missing/multiple masters. SHARDS accepts RESP2
 * alternating arrays or RESP3 maps, preserves unknown fields in the owner,
 * and preserves unknown role/health text. SLOTS accepts old 2-field endpoints,
 * optional node IDs and metadata; duplicate per-range node tuples remain so.
 * Empty shard slot lists and partial maps are valid unless full coverage is
 * required. Bounded reader costs remain the caller's responsibility.
 * VALUE transfers one topology ref. One allocation owns the fixed32KiB slot
 * index and node/range descriptors, plus one retained Reply ref (no text copy).
 * MaxOwnedBytes includes that Reply's ReservedBytes. Creation scans shapes
 * twice and fills slots; no hidden runtime, threads, network, refresh or retry.
 * Output remains unchanged on failure. Source/config/output must be disjoint.
 * Invalid args/OOM may set ambient error; protocol/limit/server outcomes don't. */
XREDIS_API xredisclusterstatus xredisClusterTopologyCreate(xredisreply* Reply,xredisclusterformat Format,const xredisclusterlimits* Limits,xredisclustertopology** Topology);
/* Immutable snapshots support synchronized cross-thread handoff. A caller must
 * own a live ref before access/retain; final Release frees on its own thread. */
XREDIS_API bool xredisClusterTopologyRetain(xredisclustertopology* Topology);
XREDIS_API void xredisClusterTopologyRelease(xredisclustertopology* Topology);
XREDIS_API bool xredisClusterTopologyInfo(const xredisclustertopology* Topology,xredisclustertopologyinfo* Info);
/* Constant-time index/slot lookups, no allocations, locks or refcount changes.
 * Out-of-range arguments report ERROR; unassigned slots return ABSENT and leave
 * Node unchanged. Lookup does not filter health/unknown endpoints: caller must
 * choose an eligible node/transport before dialing. Every writable output must
 * be disjoint from the snapshot and all its retained reply allocations. */
XREDIS_API xredisclusterstatus xredisClusterTopologyLookup(const xredisclustertopology* Topology,uint16 Slot,xredisclusternode* Node);
XREDIS_API bool xredisClusterTopologyNode(const xredisclustertopology* Topology,size_t Index,xredisclusternode* Node);
XREDIS_API bool xredisClusterTopologyRange(const xredisclustertopology* Topology,size_t Index,xredisclusterrange* Range);
XRT_EXTERN_C_END
#endif
#endif
