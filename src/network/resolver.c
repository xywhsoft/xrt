#include "../internal/xrt_net_resolver.h"



#if defined(XRT_FEATURE_NET_RESOLVER)

#define XRT_NET_RESOLVER_MAX_WORKERS 32u
#define XRT_NET_RESOLVER_DEFAULT_WORKERS 2u
#define XRT_NET_RESOLVER_DEFAULT_REQUESTS 8192u
#define XRT_NET_RESOLVER_DEFAULT_QUERIES 4096u
#define XRT_NET_RESOLVER_DEFAULT_CACHE 256u
#define XRT_NET_RESOLVER_DEFAULT_SUCCESS_TTL UINT64_C(60000000)
#define XRT_NET_RESOLVER_DEFAULT_FAILURE_TTL UINT64_C(5000000)
#define XRT_NET_RESOLVER_DEFAULT_HOST_LIMIT 1024u
#define XRT_NET_RESOLVER_INLINE_HOST 256u



/* 设置异步解析器的稳定网络错误。 */
static void __xrtNetResolverError(
	xerrkind Kind,
	xneterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtNetSetError(Kind, Code, sOperation, sMessage, 0);
}



/* 创建可跨线程保存的解析器错误对象。 */
static xerror* __xrtNetResolverErrorCreate(
	xerrkind Kind,
	xneterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Code = (int32)Code;
	Desc.Domain = "xrt.net";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	return xrtErrorBuild(&Desc);
}



/* 为固定上限选择负载因子不高于二分之一的二次幂哈希桶。 */
static size_t __xrtNetResolverBucketCount(size_t iLimit)
{
	size_t iNeed;
	size_t iCount = 8;

	if ( iLimit > (SIZE_MAX / 2u) ) {
		__xrtErrorSetSizeOverflow();
		return 0;
	}
	iNeed = iLimit * 2u;
	while ( iCount < iNeed ) {
		if ( iCount > (SIZE_MAX / 2u) ) {
			__xrtErrorSetSizeOverflow();
			return 0;
		}
		iCount *= 2u;
	}
	return iCount;
}



/* 执行 DNS 规定的 ASCII 大小写规范化，不改变尾随根点。 */
static void __xrtNetResolverHost(
	str sResult,
	cstr sHost,
	size_t iSize
)
{
	for ( size_t i = 0; i < iSize; i++ ) {
		unsigned char iByte = (unsigned char)sHost[i];

		sResult[i] = (char)(
			(iByte >= 'A') && (iByte <= 'Z') ?
			(iByte + ('a' - 'A')) : iByte
		);
	}
	sResult[iSize] = 0;
}



/* 把规范化主机名与地址族混合成查询和缓存共用的键。 */
static uint64 __xrtNetResolverHash(
	cstr sHost,
	size_t iSize,
	xnetfamily Family
)
{
	return xrtHash64(sHost, iSize) ^
		((uint64)(uint32)Family * UINT64_C(0x9E3779B97F4A7C15));
}



/* 增加 Resolver 内部引用。 */
static bool __xrtNetResolverRetain(xnetresolver* pResolver)
{
	return (pResolver != NULL) &&
		(xrtRefRetain(&pResolver->RefCount) >= 0);
}



/* 最后一个 Resolver 引用负责释放仍需供外部操作查询使用的同步外壳。 */
static void __xrtNetResolverRelease(xnetresolver* pResolver)
{
	if ( (pResolver == NULL) ||
		 (xrtRefRelease(&pResolver->RefCount) != 0) ) {
		return;
	}
	xrtErrorFree(pResolver->CancelError);
	if ( pResolver->ConditionReady ) {
		(void)xrtCondUnit(&pResolver->Condition);
	}
	if ( pResolver->LockReady ) {
		(void)xrtMutexUnit(&pResolver->Lock);
	}
	xrtFree(pResolver);
}



/* 释放解析操作的最后一个引用以及它持有的不可变终态。 */
static void __xrtNetResolveOpRelease(xnetresolveop* pOperation)
{
	if ( (pOperation == NULL) ||
		 (xrtRefRelease(&pOperation->RefCount) != 0) ) {
		return;
	}
	xrtNetAddrListDestroy(pOperation->Addresses);
	xrtErrorFree(pOperation->Error);
	__xrtNetResolverRelease(pOperation->Resolver);
	xrtFree(pOperation);
}



/* Resolver 派发器和上层取消路径只允许一方认领终态回调。 */
bool __xrtNetResolveOpClaimCallback(xnetresolveop* pOperation)
{
	uint32 iExpected = 0;

	return (pOperation != NULL) && xrtAtomic32CompareExchange(
		&pOperation->CallbackClaimed,
		&iExpected,
		1,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	);
}



/* 在活动查询哈希表中查找相同规范化键。 */
static xrt_net_resolve_group* __xrtNetResolverQueryFind(
	xnetresolver* pResolver,
	cstr sHost,
	xnetfamily Family,
	uint64 iHash
)
{
	size_t iBucket = (size_t)iHash & (pResolver->QueryBucketCount - 1u);
	xrt_net_resolve_group* pGroup = pResolver->QueryBuckets[iBucket];

	while ( pGroup != NULL ) {
		if ( (pGroup->Hash == iHash) && (pGroup->Family == Family) &&
			 (strcmp(pGroup->Host, sHost) == 0) ) {
			return pGroup;
		}
		pGroup = pGroup->HashNext;
	}
	return NULL;
}



/* 把新查询组挂入活动查询哈希表。 */
static void __xrtNetResolverQueryInsert(
	xnetresolver* pResolver,
	xrt_net_resolve_group* pGroup
)
{
	size_t iBucket =
		(size_t)pGroup->Hash & (pResolver->QueryBucketCount - 1u);

	pGroup->HashNext = pResolver->QueryBuckets[iBucket];
	pResolver->QueryBuckets[iBucket] = pGroup;
}



/* 从活动查询哈希表摘除一个已完成或已放弃的查询组。 */
static void __xrtNetResolverQueryRemove(
	xnetresolver* pResolver,
	xrt_net_resolve_group* pGroup
)
{
	size_t iBucket =
		(size_t)pGroup->Hash & (pResolver->QueryBucketCount - 1u);
	xrt_net_resolve_group** ppCurrent = &pResolver->QueryBuckets[iBucket];

	while ( *ppCurrent != NULL ) {
		if ( *ppCurrent == pGroup ) {
			*ppCurrent = pGroup->HashNext;
			pGroup->HashNext = NULL;
			return;
		}
		ppCurrent = &(*ppCurrent)->HashNext;
	}
}



/* 把唯一查询追加到受限工作队列。 */
static void __xrtNetResolverQueryQueueAppend(
	xnetresolver* pResolver,
	xrt_net_resolve_group* pGroup
)
{
	pGroup->QueuePrevious = pResolver->QueryTail;
	if ( pResolver->QueryTail != NULL ) {
		pResolver->QueryTail->QueueNext = pGroup;
	} else {
		pResolver->QueryHead = pGroup;
	}
	pResolver->QueryTail = pGroup;
	pResolver->QueuedQueries++;
}



/* 从工作队列中以常数时间摘除指定查询组。 */
static void __xrtNetResolverQueryQueueRemove(
	xnetresolver* pResolver,
	xrt_net_resolve_group* pGroup
)
{
	if ( pGroup->QueuePrevious != NULL ) {
		pGroup->QueuePrevious->QueueNext = pGroup->QueueNext;
	} else {
		pResolver->QueryHead = pGroup->QueueNext;
	}
	if ( pGroup->QueueNext != NULL ) {
		pGroup->QueueNext->QueuePrevious = pGroup->QueuePrevious;
	} else {
		pResolver->QueryTail = pGroup->QueuePrevious;
	}
	pGroup->QueuePrevious = NULL;
	pGroup->QueueNext = NULL;
	pResolver->QueuedQueries--;
}



/* 取出队首唯一查询并标记为运行中。 */
static xrt_net_resolve_group* __xrtNetResolverQueryTake(
	xnetresolver* pResolver
)
{
	xrt_net_resolve_group* pGroup = pResolver->QueryHead;

	if ( pGroup == NULL ) {
		return NULL;
	}
	__xrtNetResolverQueryQueueRemove(pResolver, pGroup);
	pGroup->Running = true;
	pResolver->RunningQueries++;
	pResolver->QueriesStarted++;
	for ( xnetresolveop* pOperation = pGroup->RequestHead;
		pOperation != NULL; pOperation = pOperation->GroupNext ) {
		xrtAtomic32Store(
			&pOperation->State,
			(uint32)XNET_RESOLVE_RUNNING,
			XMEMORY_RELEASE
		);
	}
	return pGroup;
}



/* 把调用方操作追加到查询组订阅链。 */
static void __xrtNetResolverRequestAppend(
	xrt_net_resolve_group* pGroup,
	xnetresolveop* pOperation
)
{
	pOperation->Group = pGroup;
	pOperation->GroupPrevious = pGroup->RequestTail;
	if ( pGroup->RequestTail != NULL ) {
		pGroup->RequestTail->GroupNext = pOperation;
	} else {
		pGroup->RequestHead = pOperation;
	}
	pGroup->RequestTail = pOperation;
}



/* 从查询组中摘除一个已取消或即将完成的调用方操作。 */
static void __xrtNetResolverRequestRemove(
	xrt_net_resolve_group* pGroup,
	xnetresolveop* pOperation
)
{
	if ( pOperation->GroupPrevious != NULL ) {
		pOperation->GroupPrevious->GroupNext = pOperation->GroupNext;
	} else {
		pGroup->RequestHead = pOperation->GroupNext;
	}
	if ( pOperation->GroupNext != NULL ) {
		pOperation->GroupNext->GroupPrevious = pOperation->GroupPrevious;
	} else {
		pGroup->RequestTail = pOperation->GroupPrevious;
	}
	pOperation->Group = NULL;
	pOperation->GroupPrevious = NULL;
	pOperation->GroupNext = NULL;
}



/* 把已进入终态的操作加入 Worker 回调队列。 */
static void __xrtNetResolverReadyAppend(
	xnetresolver* pResolver,
	xnetresolveop* pOperation
)
{
	if ( pResolver->ReadyTail != NULL ) {
		pResolver->ReadyTail->ReadyNext = pOperation;
	} else {
		pResolver->ReadyHead = pOperation;
	}
	pResolver->ReadyTail = pOperation;
	pResolver->ReadyCallbacks++;
}



/* 取出一个待执行回调。 */
static xnetresolveop* __xrtNetResolverReadyTake(xnetresolver* pResolver)
{
	xnetresolveop* pOperation = pResolver->ReadyHead;

	if ( pOperation == NULL ) {
		return NULL;
	}
	pResolver->ReadyHead = pOperation->ReadyNext;
	if ( pResolver->ReadyHead == NULL ) {
		pResolver->ReadyTail = NULL;
	}
	pOperation->ReadyNext = NULL;
	pResolver->ReadyCallbacks--;
	return pOperation;
}



/* 从最近使用链摘除一个缓存项。 */
static void __xrtNetResolverCacheLRURemove(
	xnetresolver* pResolver,
	xrt_net_resolver_cache* pEntry
)
{
	if ( pEntry->LRUPrevious != NULL ) {
		pEntry->LRUPrevious->LRUNext = pEntry->LRUNext;
	} else {
		pResolver->LRUHead = pEntry->LRUNext;
	}
	if ( pEntry->LRUNext != NULL ) {
		pEntry->LRUNext->LRUPrevious = pEntry->LRUPrevious;
	} else {
		pResolver->LRUTail = pEntry->LRUPrevious;
	}
	pEntry->LRUPrevious = NULL;
	pEntry->LRUNext = NULL;
}



/* 把命中或新插入的缓存项移动到最近使用链首部。 */
static void __xrtNetResolverCacheLRUFirst(
	xnetresolver* pResolver,
	xrt_net_resolver_cache* pEntry
)
{
	pEntry->LRUNext = pResolver->LRUHead;
	if ( pResolver->LRUHead != NULL ) {
		pResolver->LRUHead->LRUPrevious = pEntry;
	} else {
		pResolver->LRUTail = pEntry;
	}
	pResolver->LRUHead = pEntry;
}



/* 从缓存哈希表摘除一个项目。 */
static void __xrtNetResolverCacheHashRemove(
	xnetresolver* pResolver,
	xrt_net_resolver_cache* pEntry
)
{
	size_t iBucket =
		(size_t)pEntry->Hash & (pResolver->CacheBucketCount - 1u);
	xrt_net_resolver_cache** ppCurrent = &pResolver->CacheBuckets[iBucket];

	while ( *ppCurrent != NULL ) {
		if ( *ppCurrent == pEntry ) {
			*ppCurrent = pEntry->HashNext;
			pEntry->HashNext = NULL;
			return;
		}
		ppCurrent = &(*ppCurrent)->HashNext;
	}
}



/* 释放一个已经从全部索引摘除的缓存项。 */
static void __xrtNetResolverCacheFree(xrt_net_resolver_cache* pEntry)
{
	if ( pEntry == NULL ) {
		return;
	}
	xrtNetAddrListDestroy(pEntry->Addresses);
	xrtErrorFree(pEntry->Error);
	xrtFree(pEntry);
}



/* 摘除并释放一个缓存项。 */
static void __xrtNetResolverCacheRemove(
	xnetresolver* pResolver,
	xrt_net_resolver_cache* pEntry
)
{
	__xrtNetResolverCacheHashRemove(pResolver, pEntry);
	__xrtNetResolverCacheLRURemove(pResolver, pEntry);
	pResolver->CachedResults--;
	__xrtNetResolverCacheFree(pEntry);
}



/* 清空 Resolver 的全部成功与失败缓存。 */
static void __xrtNetResolverCacheClearLocked(xnetresolver* pResolver)
{
	while ( pResolver->LRUTail != NULL ) {
		__xrtNetResolverCacheRemove(pResolver, pResolver->LRUTail);
	}
}



/* 查找未过期缓存并更新 LRU；过期项在命中路径立即回收。 */
static xrt_net_resolver_cache* __xrtNetResolverCacheFind(
	xnetresolver* pResolver,
	cstr sHost,
	xnetfamily Family,
	uint64 iHash
)
{
	xrt_net_resolver_cache* pEntry;
	size_t iBucket;
	uint64 iNow;

	if ( pResolver->CacheBucketCount == 0 ) {
		return NULL;
	}
	iBucket = (size_t)iHash & (pResolver->CacheBucketCount - 1u);
	pEntry = pResolver->CacheBuckets[iBucket];
	while ( pEntry != NULL ) {
		xrt_net_resolver_cache* pNext = pEntry->HashNext;

		if ( (pEntry->Hash == iHash) && (pEntry->Family == Family) &&
			 (strcmp(pEntry->Host, sHost) == 0) ) {
			iNow = xrtClock();
			if ( pEntry->Expires <= iNow ) {
				__xrtNetResolverCacheRemove(pResolver, pEntry);
				return NULL;
			}
			__xrtNetResolverCacheLRURemove(pResolver, pEntry);
			__xrtNetResolverCacheLRUFirst(pResolver, pEntry);
			return pEntry;
		}
		pEntry = pNext;
	}
	return NULL;
}



/* 计算单调缓存失效时间，溢出时饱和到最大刻度。 */
static uint64 __xrtNetResolverExpires(uint64 iTTL)
{
	uint64 iNow = xrtClock();

	return iTTL > (UINT64_MAX - iNow) ? UINT64_MAX : iNow + iTTL;
}



/* 最佳努力地缓存完整结果；缓存 OOM 不改变已经完成的查询结果。 */
static void __xrtNetResolverCachePut(
	xnetresolver* pResolver,
	const xrt_net_resolve_group* pGroup,
	xnetaddrlist* pAddresses,
	xerror* pError
)
{
	uint64 iTTL = pAddresses != NULL ?
		pResolver->Config.SuccessTTL : pResolver->Config.FailureTTL;
	xrt_net_resolver_cache* pEntry;
	size_t iHostSize;
	size_t iBucket;

	if ( (pResolver->CacheBucketCount == 0) || (iTTL == 0) ) {
		return;
	}
	iHostSize = strlen(pGroup->Host);
	if ( iHostSize > (SIZE_MAX - sizeof(*pEntry) - 1u) ) {
		xrtClearError();
		return;
	}
	pEntry = (xrt_net_resolver_cache*)xrtCalloc(
		1,
		sizeof(*pEntry) + iHostSize + 1u
	);
	if ( pEntry == NULL ) {
		xrtClearError();
		return;
	}
	memcpy(pEntry->Host, pGroup->Host, iHostSize + 1u);
	pEntry->Hash = pGroup->Hash;
	pEntry->Family = pGroup->Family;
	pEntry->Expires = __xrtNetResolverExpires(iTTL);
	pEntry->Addresses = pAddresses != NULL ?
		xrtNetAddrListRef(pAddresses) : NULL;
	pEntry->Error = pError != NULL ? xrtErrorRef(pError) : NULL;

	if ( pResolver->CachedResults >= pResolver->Config.CacheEntries ) {
		__xrtNetResolverCacheRemove(pResolver, pResolver->LRUTail);
	}
	iBucket = (size_t)pEntry->Hash & (pResolver->CacheBucketCount - 1u);
	pEntry->HashNext = pResolver->CacheBuckets[iBucket];
	pResolver->CacheBuckets[iBucket] = pEntry;
	__xrtNetResolverCacheLRUFirst(pResolver, pEntry);
	pResolver->CachedResults++;
}



/* 释放已经离开活动表和工作队列的查询组。 */
static void __xrtNetResolverGroupFree(xrt_net_resolve_group* pGroup)
{
	if ( pGroup == NULL ) {
		return;
	}
	xrtFree(pGroup);
}



/* 在 Resolver 锁内写入唯一终态并排队回调。 */
static void __xrtNetResolverComplete(
	xnetresolver* pResolver,
	xnetresolveop* pOperation,
	xnetresolveopstate State,
	xnetaddrlist* pAddresses,
	xerror* pError
)
{
	pOperation->Addresses = pAddresses != NULL ?
		xrtNetAddrListRef(pAddresses) : NULL;
	pOperation->Error = pError != NULL ? xrtErrorRef(pError) : NULL;
	xrtAtomic32Store(&pOperation->State, (uint32)State, XMEMORY_RELEASE);
	__xrtNetResolverReadyAppend(pResolver, pOperation);
	if ( State == XNET_RESOLVE_RESOLVED ) {
		pResolver->Resolved++;
	} else if ( State == XNET_RESOLVE_FAILED ) {
		pResolver->Failed++;
	} else if ( State == XNET_RESOLVE_CANCELLED ) {
		pResolver->Cancelled++;
	}
}



/* 验证自定义查询过程没有突破端口和地址族契约。 */
static bool __xrtNetResolverResultValid(
	xnetaddrlist* pAddresses,
	xnetfamily Family
)
{
	size_t iCount = xrtNetAddrListCount(pAddresses);

	if ( iCount == 0 ) {
		return false;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		const xnetaddr* pAddress = xrtNetAddrListGet(pAddresses, i);

		if ( (pAddress == NULL) || (pAddress->Port != 0) ||
			 ((Family != XNET_FAMILY_UNSPEC) &&
			  (pAddress->Family != (uint16)Family)) ) {
			xrtClearError();
			return false;
		}
	}
	return true;
}



/* 执行一次可能阻塞的底层查询并向全部订阅者发布共享结果。 */
static void __xrtNetResolverRunQuery(
	xnetresolver* pResolver,
	xrt_net_resolve_group* pGroup
)
{
	xnetaddrlist* pAddresses;
	xerror* pError;
	xnetresolveop* pOperation;

	xrtClearError();
	pAddresses = pResolver->Config.Lookup(
		pGroup->Host,
		pGroup->Family,
		pResolver->Config.LookupData
	);
	if ( (pAddresses != NULL) &&
		 !__xrtNetResolverResultValid(pAddresses, pGroup->Family) ) {
		xrtNetAddrListDestroy(pAddresses);
		pAddresses = NULL;
		__xrtNetResolverError(XERR_VALUE, XNET_ERROR_RESOLVER_QUERY,
			"resolve", "resolver lookup returned an invalid address list");
	}
	if ( pAddresses != NULL ) {
		xrtClearError();
	}
	pError = pAddresses == NULL ? xrtTakeError() : NULL;
	if ( (pAddresses == NULL) && (pError == NULL) ) {
		__xrtNetResolverError(XERR_IO, XNET_ERROR_RESOLVER_QUERY,
			"resolve", "resolver lookup failed without an error");
		pError = xrtTakeError();
	}

	(void)xrtMutexLock(&pResolver->Lock);
	__xrtNetResolverQueryRemove(pResolver, pGroup);
	pResolver->ActiveQueries--;
	pResolver->RunningQueries--;
	__xrtNetResolverCachePut(pResolver, pGroup, pAddresses, pError);
	pOperation = pGroup->RequestHead;
	while ( pOperation != NULL ) {
		xnetresolveop* pNext = pOperation->GroupNext;

		__xrtNetResolverRequestRemove(pGroup, pOperation);
		__xrtNetResolverComplete(
			pResolver,
			pOperation,
			pAddresses != NULL ?
				XNET_RESOLVE_RESOLVED : XNET_RESOLVE_FAILED,
			pAddresses,
			pError
		);
		pOperation = pNext;
	}
	(void)xrtCondBroadcast(&pResolver->Condition);
	(void)xrtMutexUnlock(&pResolver->Lock);

	xrtNetAddrListDestroy(pAddresses);
	xrtErrorFree(pError);
	__xrtNetResolverGroupFree(pGroup);
}



/* 在 Resolver Worker 上执行一次终态回调并释放内部操作引用。 */
static void __xrtNetResolverDispatch(
	xnetresolver* pResolver,
	xnetresolveop* pOperation
)
{
	if ( __xrtNetResolveOpClaimCallback(pOperation) &&
		 (pOperation->Done != NULL) ) {
		pOperation->Done(pOperation, pOperation->Data);
	}
	(void)xrtMutexLock(&pResolver->Lock);
	pResolver->Outstanding--;
	(void)xrtCondBroadcast(&pResolver->Condition);
	(void)xrtMutexUnlock(&pResolver->Lock);
	__xrtNetResolveOpRelease(pOperation);
}



/* Resolver Worker 优先派发轻量终态，再领取唯一阻塞查询。 */
static int32 __xrtNetResolverWorker(ptr pData)
{
	xnetresolver* pResolver = (xnetresolver*)pData;

	for ( ;; ) {
		xnetresolveop* pOperation;
		xrt_net_resolve_group* pGroup;

		(void)xrtMutexLock(&pResolver->Lock);
		while ( (pResolver->ReadyHead == NULL) &&
			 (pResolver->QueryHead == NULL) && !pResolver->Closing ) {
			if ( xrtCondWait(&pResolver->Condition, &pResolver->Lock) !=
				 XWAIT_OK ) {
				pResolver->Closing = true;
				break;
			}
		}
		pOperation = __xrtNetResolverReadyTake(pResolver);
		pGroup = pOperation == NULL ?
			__xrtNetResolverQueryTake(pResolver) : NULL;
		if ( (pOperation == NULL) && (pGroup == NULL) &&
			 pResolver->Closing ) {
			(void)xrtMutexUnlock(&pResolver->Lock);
			break;
		}
		(void)xrtMutexUnlock(&pResolver->Lock);

		if ( pOperation != NULL ) {
			__xrtNetResolverDispatch(pResolver, pOperation);
		} else if ( pGroup != NULL ) {
			__xrtNetResolverRunQuery(pResolver, pGroup);
		}
	}
	return 0;
}



/* 默认查询过程复用同步 DNS 的完整、端口无关结果。 */
static xnetaddrlist* __xrtNetResolverLookupDefault(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	(void)pData;
	return xrtNetLookup(sHost, Family);
}



/* 写入独立解析器的平衡默认值。 */
XRT_API void xrtNetResolverConfigInit(xnetresolverconfig* pConfig)
{
	if ( pConfig == NULL ) {
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Workers = XRT_NET_RESOLVER_DEFAULT_WORKERS;
	pConfig->RequestLimit = XRT_NET_RESOLVER_DEFAULT_REQUESTS;
	pConfig->QueryLimit = XRT_NET_RESOLVER_DEFAULT_QUERIES;
	pConfig->CacheEntries = XRT_NET_RESOLVER_DEFAULT_CACHE;
	pConfig->SuccessTTL = XRT_NET_RESOLVER_DEFAULT_SUCCESS_TTL;
	pConfig->FailureTTL = XRT_NET_RESOLVER_DEFAULT_FAILURE_TTL;
	pConfig->HostLimit = XRT_NET_RESOLVER_DEFAULT_HOST_LIMIT;
}



/* 校验配置中的线程数量和全部硬容量边界。 */
static bool __xrtNetResolverConfigValid(const xnetresolverconfig* pConfig)
{
	if ( (pConfig->Workers == 0) ||
		 (pConfig->Workers > XRT_NET_RESOLVER_MAX_WORKERS) ||
		 (pConfig->RequestLimit == 0) || (pConfig->QueryLimit == 0) ||
		 (pConfig->HostLimit == 0) ) {
		__xrtNetResolverError(XERR_VALUE, XNET_ERROR_RESOLVER_CREATE,
			"create-resolver", "resolver configuration contains a zero or unsupported limit");
		return false;
	}
	return true;
}



/* 创建失败时停止已启动线程并恢复最初的结构化错误。 */
static xnetresolver* __xrtNetResolverCreateFail(xnetresolver* pResolver)
{
	xerror* pError = xrtTakeError();

	if ( pResolver->LockReady ) {
		(void)xrtMutexLock(&pResolver->Lock);
		pResolver->Closing = true;
		if ( pResolver->ConditionReady ) {
			(void)xrtCondBroadcast(&pResolver->Condition);
		}
		(void)xrtMutexUnlock(&pResolver->Lock);
	}
	for ( uint32 i = 0; i < pResolver->StartedThreads; i++ ) {
		(void)xrtThreadWait(pResolver->Threads[i]);
		xrtThreadDestroy(pResolver->Threads[i]);
	}
	xrtFree(pResolver->Threads);
	xrtFree(pResolver->QueryBuckets);
	xrtFree(pResolver->CacheBuckets);
	__xrtNetResolverRelease(pResolver);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
	return NULL;
}



/* 创建并启动独立、受限、可裁剪的 DNS Resolver。 */
XRT_API xnetresolver* xrtNetResolverCreate(
	const xnetresolverconfig* pConfig
)
{
	xnetresolverconfig Config;
	xnetresolver* pResolver;

	xrtNetResolverConfigInit(&Config);
	if ( pConfig != NULL ) {
		Config = *pConfig;
	}
	if ( !__xrtNetResolverConfigValid(&Config) ) {
		return NULL;
	}
	pResolver = (xnetresolver*)xrtCalloc(1, sizeof(*pResolver));
	if ( pResolver == NULL ) {
		return NULL;
	}
	pResolver->RefCount = 1;
	pResolver->Config = Config;
	if ( pResolver->Config.Lookup == NULL ) {
		pResolver->Config.Lookup = __xrtNetResolverLookupDefault;
	}
	if ( !xrtMutexInit(&pResolver->Lock) ) {
		return __xrtNetResolverCreateFail(pResolver);
	}
	pResolver->LockReady = true;
	if ( !xrtCondInit(&pResolver->Condition) ) {
		return __xrtNetResolverCreateFail(pResolver);
	}
	pResolver->ConditionReady = true;
	pResolver->QueryBucketCount =
		__xrtNetResolverBucketCount(Config.QueryLimit);
	if ( pResolver->QueryBucketCount == 0 ) {
		return __xrtNetResolverCreateFail(pResolver);
	}
	pResolver->QueryBuckets = (xrt_net_resolve_group**)xrtCalloc(
		pResolver->QueryBucketCount,
		sizeof(xrt_net_resolve_group*)
	);
	pResolver->Threads = (xthread**)xrtCalloc(
		Config.Workers,
		sizeof(xthread*)
	);
	if ( Config.CacheEntries != 0 ) {
		pResolver->CacheBucketCount =
			__xrtNetResolverBucketCount(Config.CacheEntries);
		if ( pResolver->CacheBucketCount != 0 ) {
			pResolver->CacheBuckets =
				(xrt_net_resolver_cache**)xrtCalloc(
					pResolver->CacheBucketCount,
					sizeof(xrt_net_resolver_cache*)
				);
		}
	}
	pResolver->CancelError = __xrtNetResolverErrorCreate(
		XERR_CANCELLED,
		XNET_ERROR_RESOLVER_QUERY,
		"cancel-resolve",
		"DNS resolve operation was cancelled"
	);
	if ( (pResolver->QueryBuckets == NULL) ||
		 (pResolver->Threads == NULL) ||
		 ((Config.CacheEntries != 0) &&
		  ((pResolver->CacheBucketCount == 0) ||
		   (pResolver->CacheBuckets == NULL))) ||
		 (pResolver->CancelError == NULL) ) {
		return __xrtNetResolverCreateFail(pResolver);
	}
	for ( uint32 i = 0; i < Config.Workers; i++ ) {
		pResolver->Threads[i] = xrtThreadCreate(
			__xrtNetResolverWorker,
			pResolver,
			Config.ThreadStack
		);
		if ( pResolver->Threads[i] == NULL ) {
			return __xrtNetResolverCreateFail(pResolver);
		}
		pResolver->StartedThreads++;
	}
	return pResolver;
}



/* 判断当前线程是否属于 Resolver，防止回调内同步销毁造成自等待。 */
static bool __xrtNetResolverIsWorker(const xnetresolver* pResolver)
{
	uint64 iCurrent = xrtThreadCurrentId();

	for ( uint32 i = 0; i < pResolver->StartedThreads; i++ ) {
		if ( xrtThreadId(pResolver->Threads[i]) == iCurrent ) {
			return true;
		}
	}
	return false;
}



/* 停止接收、排空已受理查询与回调，再释放 Resolver 所有运行资源。 */
XRT_API bool xrtNetResolverDestroy(xnetresolver* pResolver)
{
	bool bSuccess = true;

	if ( pResolver == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtNetResolverIsWorker(pResolver) ) {
		__xrtNetResolverError(XERR_STATE, XNET_ERROR_RESOLVER_CLOSED,
			"destroy-resolver", "resolver cannot be destroyed by its own worker");
		return false;
	}
	(void)xrtMutexLock(&pResolver->Lock);
	if ( pResolver->Closing || pResolver->Destroyed ) {
		(void)xrtMutexUnlock(&pResolver->Lock);
		__xrtNetResolverError(XERR_STATE, XNET_ERROR_RESOLVER_CLOSED,
			"destroy-resolver", "resolver is already closing or destroyed");
		return false;
	}
	pResolver->Closing = true;
	(void)xrtCondBroadcast(&pResolver->Condition);
	(void)xrtMutexUnlock(&pResolver->Lock);

	for ( uint32 i = 0; i < pResolver->StartedThreads; i++ ) {
		if ( xrtThreadWait(pResolver->Threads[i]) != XWAIT_OK ) {
			bSuccess = false;
		}
		xrtThreadDestroy(pResolver->Threads[i]);
	}
	(void)xrtMutexLock(&pResolver->Lock);
	__xrtNetResolverCacheClearLocked(pResolver);
	pResolver->Destroyed = true;
	(void)xrtMutexUnlock(&pResolver->Lock);
	xrtFree(pResolver->Threads);
	xrtFree(pResolver->QueryBuckets);
	xrtFree(pResolver->CacheBuckets);
	pResolver->Threads = NULL;
	pResolver->QueryBuckets = NULL;
	pResolver->CacheBuckets = NULL;
	__xrtNetResolverRelease(pResolver);
	return bSuccess;
}



/* 建立一个持有 Resolver 引用的调用方解析操作。 */
static xnetresolveop* __xrtNetResolveOpCreate(
	xnetresolver* pResolver,
	xnetresolveproc pDone,
	ptr pData
)
{
	xnetresolveop* pOperation =
		(xnetresolveop*)xrtCalloc(1, sizeof(*pOperation));

	if ( pOperation == NULL ) {
		return NULL;
	}
	pOperation->RefCount = 1;
	pOperation->Resolver = pResolver;
	pOperation->Done = pDone;
	pOperation->Data = pData;
	(void)xrtAtomic32Init(&pOperation->State, (uint32)XNET_RESOLVE_PENDING);
	(void)xrtAtomic32Init(&pOperation->CallbackClaimed, 0);
	return pOperation;
}



/* 提交可合并、可缓存、可取消的异步主机查询。 */
XRT_API xnetresolveop* xrtNetResolverResolve(
	xnetresolver* pResolver,
	cstr sHost,
	xnetfamily Family,
	xnetresolveproc pDone,
	ptr pData
)
{
	xnetresolveop* pOperation;
	xrt_net_resolve_group* pGroup;
	xrt_net_resolver_cache* pCache;
	char sInlineHost[XRT_NET_RESOLVER_INLINE_HOST];
	str sAllocatedHost = NULL;
	str sCanonical = sInlineHost;
	size_t iHostSize;
	uint64 iHash;
	bool bAccepted = false;

	if ( (pResolver == NULL) || (sHost == NULL) || (sHost[0] == 0) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( (Family != XNET_FAMILY_UNSPEC) &&
		 (Family != XNET_FAMILY_IPV4) &&
		 (Family != XNET_FAMILY_IPV6) ) {
		__xrtNetResolverError(XERR_VALUE, XNET_ERROR_FAMILY,
			"submit-resolve", "unsupported DNS address family");
		return NULL;
	}
	if ( !__xrtNetResolverRetain(pResolver) ) {
		__xrtNetResolverError(XERR_CLOSED, XNET_ERROR_RESOLVER_CLOSED,
			"submit-resolve", "resolver is closed");
		return NULL;
	}
	iHostSize = strlen(sHost);
	if ( iHostSize > pResolver->Config.HostLimit ) {
		__xrtNetResolverRelease(pResolver);
		__xrtNetResolverError(XERR_RANGE, XNET_ERROR_RESOLVER_SUBMIT,
			"submit-resolve", "host name exceeds the configured limit");
		return NULL;
	}
	if ( iHostSize >
		 (SIZE_MAX - sizeof(xrt_net_resolve_group) - 1u) ) {
		__xrtErrorSetSizeOverflow();
		__xrtNetResolverRelease(pResolver);
		return NULL;
	}
	if ( iHostSize >= sizeof(sInlineHost) ) {
		sAllocatedHost = (str)xrtMalloc(iHostSize + 1u);
		if ( sAllocatedHost == NULL ) {
			__xrtNetResolverRelease(pResolver);
			return NULL;
		}
		sCanonical = sAllocatedHost;
	}
	__xrtNetResolverHost(sCanonical, sHost, iHostSize);
	pOperation = __xrtNetResolveOpCreate(pResolver, pDone, pData);
	if ( pOperation == NULL ) {
		xrtFree(sAllocatedHost);
		__xrtNetResolverRelease(pResolver);
		return NULL;
	}
	iHash = __xrtNetResolverHash(sCanonical, iHostSize, Family);

	(void)xrtMutexLock(&pResolver->Lock);
	if ( pResolver->Closing || pResolver->Destroyed ) {
		pResolver->Rejected++;
		__xrtNetResolverError(XERR_CLOSED, XNET_ERROR_RESOLVER_CLOSED,
			"submit-resolve", "resolver is closing");
	} else if ( pResolver->Outstanding >=
		pResolver->Config.RequestLimit ) {
		pResolver->Rejected++;
		__xrtNetResolverError(XERR_AGAIN, XNET_ERROR_RESOLVER_SUBMIT,
			"submit-resolve", "resolver request limit reached");
	} else if ( (pCache = __xrtNetResolverCacheFind(
		pResolver, sCanonical, Family, iHash)) != NULL ) {
		(void)xrtRefRetain(&pOperation->RefCount);
		pResolver->Submitted++;
		pResolver->CacheHits++;
		pResolver->Outstanding++;
		__xrtNetResolverComplete(
			pResolver,
			pOperation,
			pCache->Addresses != NULL ?
				XNET_RESOLVE_RESOLVED : XNET_RESOLVE_FAILED,
			pCache->Addresses,
			pCache->Error
		);
		bAccepted = true;
	} else if ( (pGroup = __xrtNetResolverQueryFind(
		pResolver, sCanonical, Family, iHash)) != NULL ) {
		(void)xrtRefRetain(&pOperation->RefCount);
		__xrtNetResolverRequestAppend(pGroup, pOperation);
		if ( pGroup->Running ) {
			xrtAtomic32Store(
				&pOperation->State,
				(uint32)XNET_RESOLVE_RUNNING,
				XMEMORY_RELEASE
			);
		}
		pResolver->Submitted++;
		pResolver->CacheMisses++;
		pResolver->Coalesced++;
		pResolver->Outstanding++;
		bAccepted = true;
	} else if ( pResolver->ActiveQueries >=
		pResolver->Config.QueryLimit ) {
		pResolver->Rejected++;
		__xrtNetResolverError(XERR_AGAIN, XNET_ERROR_RESOLVER_SUBMIT,
			"submit-resolve", "resolver unique query limit reached");
	} else {
		pGroup = (xrt_net_resolve_group*)xrtCalloc(
			1,
			sizeof(*pGroup) + iHostSize + 1u
		);
		if ( pGroup != NULL ) {
			pGroup->Hash = iHash;
			pGroup->Family = Family;
			memcpy(pGroup->Host, sCanonical, iHostSize + 1u);
			(void)xrtRefRetain(&pOperation->RefCount);
			__xrtNetResolverRequestAppend(pGroup, pOperation);
			__xrtNetResolverQueryInsert(pResolver, pGroup);
			__xrtNetResolverQueryQueueAppend(pResolver, pGroup);
			pResolver->ActiveQueries++;
			pResolver->Submitted++;
			pResolver->CacheMisses++;
			pResolver->Outstanding++;
			bAccepted = true;
		}
	}
	if ( bAccepted ) {
		(void)xrtCondSignal(&pResolver->Condition);
	}
	(void)xrtMutexUnlock(&pResolver->Lock);
	xrtFree(sAllocatedHost);
	if ( !bAccepted ) {
		__xrtNetResolveOpRelease(pOperation);
		return NULL;
	}
	return pOperation;
}



/* 清空缓存但不影响任何活动查询或已经交付的共享结果。 */
XRT_API bool xrtNetResolverClear(xnetresolver* pResolver)
{
	if ( pResolver == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	(void)xrtMutexLock(&pResolver->Lock);
	if ( pResolver->Closing || pResolver->Destroyed ) {
		(void)xrtMutexUnlock(&pResolver->Lock);
		__xrtNetResolverError(XERR_CLOSED, XNET_ERROR_RESOLVER_CLOSED,
			"clear-resolver", "resolver is closing or destroyed");
		return false;
	}
	__xrtNetResolverCacheClearLocked(pResolver);
	(void)xrtMutexUnlock(&pResolver->Lock);
	return true;
}



/* 在单个临界区内取得计数器与队列深度的一致快照。 */
XRT_API bool xrtNetResolverStats(
	const xnetresolver* pResolver,
	xnetresolverstats* pStats
)
{
	xnetresolver* pMutable = (xnetresolver*)pResolver;

	if ( (pResolver == NULL) || (pStats == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	(void)xrtMutexLock(&pMutable->Lock);
	memset(pStats, 0, sizeof(*pStats));
	pStats->Workers = pResolver->Config.Workers;
	pStats->Submitted = pResolver->Submitted;
	pStats->Rejected = pResolver->Rejected;
	pStats->CacheHits = pResolver->CacheHits;
	pStats->CacheMisses = pResolver->CacheMisses;
	pStats->Coalesced = pResolver->Coalesced;
	pStats->QueriesStarted = pResolver->QueriesStarted;
	pStats->Resolved = pResolver->Resolved;
	pStats->Failed = pResolver->Failed;
	pStats->Cancelled = pResolver->Cancelled;
	pStats->Outstanding = pResolver->Outstanding;
	pStats->ActiveQueries = pResolver->ActiveQueries;
	pStats->QueuedQueries = pResolver->QueuedQueries;
	pStats->RunningQueries = pResolver->RunningQueries;
	pStats->ReadyCallbacks = pResolver->ReadyCallbacks;
	pStats->CachedResults = pResolver->CachedResults;
	(void)xrtMutexUnlock(&pMutable->Lock);
	return true;
}



/* 增加解析操作的调用方引用。 */
XRT_API xnetresolveop* xrtNetResolveOpRef(xnetresolveop* pOperation)
{
	if ( (pOperation == NULL) ||
		 (xrtRefRetain(&pOperation->RefCount) < 0) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return pOperation;
}



/* 释放解析操作的调用方引用。 */
XRT_API void xrtNetResolveOpDestroy(xnetresolveop* pOperation)
{
	__xrtNetResolveOpRelease(pOperation);
}



/* 取消单个订阅者；无订阅者的排队查询会立即从工作队列移除。 */
XRT_API bool xrtNetResolveOpCancel(xnetresolveop* pOperation)
{
	xnetresolver* pResolver;
	xrt_net_resolve_group* pGroup;
	xrt_net_resolve_group* pDiscard = NULL;
	xnetresolveopstate State;

	if ( pOperation == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	State = (xnetresolveopstate)xrtAtomic32Load(
		&pOperation->State,
		XMEMORY_ACQUIRE
	);
	if ( (State != XNET_RESOLVE_PENDING) &&
		 (State != XNET_RESOLVE_RUNNING) ) {
		return false;
	}
	pResolver = pOperation->Resolver;
	(void)xrtMutexLock(&pResolver->Lock);
	State = (xnetresolveopstate)xrtAtomic32Load(
		&pOperation->State,
		XMEMORY_ACQUIRE
	);
	if ( (State != XNET_RESOLVE_PENDING) &&
		 (State != XNET_RESOLVE_RUNNING) ) {
		(void)xrtMutexUnlock(&pResolver->Lock);
		return false;
	}
	pGroup = pOperation->Group;
	if ( pGroup == NULL ) {
		(void)xrtMutexUnlock(&pResolver->Lock);
		return false;
	}
	__xrtNetResolverRequestRemove(pGroup, pOperation);
	__xrtNetResolverComplete(
		pResolver,
		pOperation,
		XNET_RESOLVE_CANCELLED,
		NULL,
		pResolver->CancelError
	);
	if ( (pGroup->RequestHead == NULL) && !pGroup->Running ) {
		__xrtNetResolverQueryQueueRemove(pResolver, pGroup);
		__xrtNetResolverQueryRemove(pResolver, pGroup);
		pResolver->ActiveQueries--;
		pDiscard = pGroup;
	}
	(void)xrtCondSignal(&pResolver->Condition);
	(void)xrtMutexUnlock(&pResolver->Lock);
	__xrtNetResolverGroupFree(pDiscard);
	return true;
}



/* 原子读取解析操作状态。 */
XRT_API xnetresolveopstate xrtNetResolveOpState(
	const xnetresolveop* pOperation
)
{
	if ( pOperation == NULL ) {
		__xrtErrorSetInvalidArgument();
		return XNET_RESOLVE_FAILED;
	}
	return (xnetresolveopstate)xrtAtomic32Load(
		&pOperation->State,
		XMEMORY_ACQUIRE
	);
}



/* 返回解析成功结果的独立引用，并把其他状态翻译到当前错误上下文。 */
XRT_API xnetaddrlist* xrtNetResolveOpResult(
	const xnetresolveop* pOperation
)
{
	xnetresolveopstate State;

	if ( pOperation == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	State = (xnetresolveopstate)xrtAtomic32Load(
		&pOperation->State,
		XMEMORY_ACQUIRE
	);
	if ( State == XNET_RESOLVE_RESOLVED ) {
		return xrtNetAddrListRef(pOperation->Addresses);
	}
	if ( (State == XNET_RESOLVE_FAILED) ||
		 (State == XNET_RESOLVE_CANCELLED) ) {
		xrtSetError(pOperation->Error);
	} else {
		__xrtNetResolverError(XERR_AGAIN, XNET_ERROR_RESOLVER_QUERY,
			"resolve-result", "DNS resolve operation is not complete");
	}
	return NULL;
}



/* 返回失败或取消终态持有的借用错误。 */
XRT_API const xerror* xrtNetResolveOpError(
	const xnetresolveop* pOperation
)
{
	xnetresolveopstate State;

	if ( pOperation == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	State = (xnetresolveopstate)xrtAtomic32Load(
		&pOperation->State,
		XMEMORY_ACQUIRE
	);
	return (State == XNET_RESOLVE_FAILED) ||
		(State == XNET_RESOLVE_CANCELLED) ? pOperation->Error : NULL;
}

#endif
