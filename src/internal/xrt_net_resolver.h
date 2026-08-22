#ifndef XRT_INTERNAL_NET_RESOLVER_H
#define XRT_INTERNAL_NET_RESOLVER_H

#include "xrt_net.h"



#if defined(XRT_FEATURE_NET_RESOLVER)

typedef struct xrt_net_resolve_group xrt_net_resolve_group;
typedef struct xrt_net_resolver_cache xrt_net_resolver_cache;



/* 单个调用方操作拥有独立终态，但可与其他操作共享同一次底层查询。 */
struct xnetresolveop {
	volatile int32 RefCount;
	xatomic32 State;
	xatomic32 CallbackClaimed;
	xnetresolver* Resolver;
	xrt_net_resolve_group* Group;
	xnetresolveop* GroupPrevious;
	xnetresolveop* GroupNext;
	xnetresolveop* ReadyNext;
	xnetaddrlist* Addresses;
	xerror* Error;
	xnetresolveproc Done;
	ptr Data;
};



/* 认领一次唯一终态回调，使派发和分离路径互斥。 */
bool __xrtNetResolveOpClaimCallback(xnetresolveop* pOperation);



/* 查询组按规范化主机名与地址族合并多个调用方操作。 */
struct xrt_net_resolve_group {
	xrt_net_resolve_group* HashNext;
	xrt_net_resolve_group* QueuePrevious;
	xrt_net_resolve_group* QueueNext;
	xnetresolveop* RequestHead;
	xnetresolveop* RequestTail;
	uint64 Hash;
	xnetfamily Family;
	bool Running;
	char Host[];
};



/* 缓存项同时位于哈希桶和最近使用链中，结果与错误均不可变共享。 */
struct xrt_net_resolver_cache {
	xrt_net_resolver_cache* HashNext;
	xrt_net_resolver_cache* LRUPrevious;
	xrt_net_resolver_cache* LRUNext;
	uint64 Hash;
	uint64 Expires;
	xnetfamily Family;
	xnetaddrlist* Addresses;
	xerror* Error;
	char Host[];
};



/* Resolver 的可变队列、缓存和统计全部由同一把短临界区锁保护。 */
struct xnetresolver {
	volatile int32 RefCount;
	xmutex Lock;
	xcond Condition;
	bool LockReady;
	bool ConditionReady;
	bool Closing;
	bool Destroyed;
	xnetresolverconfig Config;
	xthread** Threads;
	uint32 StartedThreads;
	xrt_net_resolve_group** QueryBuckets;
	size_t QueryBucketCount;
	xrt_net_resolve_group* QueryHead;
	xrt_net_resolve_group* QueryTail;
	size_t ActiveQueries;
	size_t QueuedQueries;
	size_t RunningQueries;
	xnetresolveop* ReadyHead;
	xnetresolveop* ReadyTail;
	size_t ReadyCallbacks;
	size_t Outstanding;
	xrt_net_resolver_cache** CacheBuckets;
	size_t CacheBucketCount;
	xrt_net_resolver_cache* LRUHead;
	xrt_net_resolver_cache* LRUTail;
	size_t CachedResults;
	xerror* CancelError;
	uint64 Submitted;
	uint64 Rejected;
	uint64 CacheHits;
	uint64 CacheMisses;
	uint64 Coalesced;
	uint64 QueriesStarted;
	uint64 Resolved;
	uint64 Failed;
	uint64 Cancelled;
};

#endif

#endif
