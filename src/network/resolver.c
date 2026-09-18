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

static void __xrtNetResolverMutationBegin(xrtownershipscope* pScope)
{ if (!xrtOwnershipMutationBegin(pScope)) abort(); }
static void __xrtNetResolverMutationEnd(xrtownershipscope* pScope)
{ if (!xrtOwnershipScopeEnd(pScope)) abort(); }
static void __xrtNetResolverLock(xnetresolver* pResolver, xrtownershipscope* pScope)
{
	__xrtNetResolverMutationBegin(pScope);
	if (!xrtMutexLock(&pResolver->Lock)) abort();
}
static void __xrtNetResolverUnlock(xnetresolver* pResolver, xrtownershipscope* pScope)
{
	if (!xrtMutexUnlock(&pResolver->Lock)) abort();
	__xrtNetResolverMutationEnd(pScope);
}



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
	xrtownershipscope Mutation = {0};
	__xrtNetResolverMutationBegin(&Mutation);
	if ( (pResolver == NULL) ||
		 (xrtRefRelease(&pResolver->RefCount) != 0) ) {
		__xrtNetResolverMutationEnd(&Mutation);
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
	__xrtNetResolverMutationEnd(&Mutation);
}



/* 释放解析操作的最后一个引用以及它持有的不可变终态。 */
static void __xrtNetResolveOpRelease(xnetresolveop* pOperation)
{
	xrtownershipscope Mutation = {0};
	__xrtNetResolverMutationBegin(&Mutation);
	if ( (pOperation == NULL) ||
		 (xrtRefRelease(&pOperation->RefCount) != 0) ) {
		__xrtNetResolverMutationEnd(&Mutation);
		return;
	}
	/* Accepted callback data is retired by dispatch, never by a plan pin. */
	if (pOperation->OwnershipPolicy) abort();
	xrtNetAddrListDestroy(pOperation->Addresses);
	xrtErrorFree(pOperation->Error);
	__xrtNetResolverRelease(pOperation->Resolver);
	xrtFree(pOperation);
	__xrtNetResolverMutationEnd(&Mutation);
}



/* Resolver 派发器和上层取消路径只允许一方认领终态回调。 */
bool __xrtNetResolveOpClaimCallback(xnetresolveop* pOperation)
{
	uint32 iExpected = 0;
	xrtownershipscope Mutation = {0};
	__xrtNetResolverMutationBegin(&Mutation);

	bool bClaimed = (pOperation != NULL) && xrtAtomic32CompareExchange(
		&pOperation->CallbackClaimed,
		&iExpected,
		1,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	);
	__xrtNetResolverMutationEnd(&Mutation);
	return bClaimed;
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
	pOperation->CallbackActive = true;
	pResolver->ActiveCallbacks++;
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
	if ( ((pAddresses != NULL) && (pEntry->Addresses == NULL)) ||
		((pError != NULL) && (pEntry->Error == NULL)) ) {
		xrtNetAddrListDestroy(pEntry->Addresses);
		xrtErrorFree(pEntry->Error);
		xrtFree(pEntry);
		xrtClearError();
		return;
	}

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
	xrtownershipscope Mutation = {0};

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

	__xrtNetResolverLock(pResolver, &Mutation);
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
	__xrtNetResolverUnlock(pResolver, &Mutation);

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
	xrtownershipscope Mutation = {0};
	const xnetresolveownershipv1* pPolicy;
	ptr pData;
	if ( __xrtNetResolveOpClaimCallback(pOperation) &&
		 (pOperation->Done != NULL) ) {
		pOperation->Done(pOperation, pOperation->Data);
	}
	__xrtNetResolverLock(pResolver, &Mutation);
	pPolicy = pOperation->OwnershipPolicy;
	pData = pOperation->Data;
	pOperation->OwnershipPolicy = NULL;
	pOperation->Data = NULL;
	pOperation->Done = NULL;
	__xrtNetResolverUnlock(pResolver, &Mutation);
	if (pPolicy) pPolicy->Drop(pData);
	__xrtNetResolverLock(pResolver, &Mutation);
	pOperation->CallbackActive = false;
	pOperation->CallbackFinished = true;
	pResolver->ActiveCallbacks--;
	pResolver->Outstanding--;
	(void)xrtCondBroadcast(&pResolver->Condition);
	__xrtNetResolverUnlock(pResolver, &Mutation);
	__xrtNetResolveOpRelease(pOperation);
}



/* Resolver Worker 优先派发轻量终态，再领取唯一阻塞查询。 */
static int32 __xrtNetResolverWorker(ptr pData)
{
	xnetresolver* pResolver = (xnetresolver*)pData;

	for ( ;; ) {
		xnetresolveop* pOperation;
		xrt_net_resolve_group* pGroup;
		xrtownershipscope Mutation = {0};

		__xrtNetResolverLock(pResolver, &Mutation);
		while ( (pResolver->ReadyHead == NULL) &&
			 (pResolver->QueryHead == NULL) && !pResolver->Closing ) {
			/* A condition wake owns Lock before mutation admission. Release
			 * it first; otherwise freeze and this worker can deadlock. */
			pResolver->ParkedWorkers++;
			__xrtNetResolverMutationEnd(&Mutation);
			xwaitresult Wait = xrtCondWait(&pResolver->Condition, &pResolver->Lock);
			(void)xrtMutexUnlock(&pResolver->Lock);
			__xrtNetResolverLock(pResolver, &Mutation);
			pResolver->ParkedWorkers--;
			if ( Wait != XWAIT_OK ) {
				pResolver->Closing = true;
				break;
			}
		}
		pOperation = __xrtNetResolverReadyTake(pResolver);
		pGroup = pOperation == NULL ?
			__xrtNetResolverQueryTake(pResolver) : NULL;
		if ( (pOperation == NULL) && (pGroup == NULL) &&
			 pResolver->Closing ) {
			pResolver->ExitedWorkers++;
			__xrtNetResolverUnlock(pResolver, &Mutation);
			break;
		}
		__xrtNetResolverUnlock(pResolver, &Mutation);

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
	xrtownershipscope Mutation = {0};

	if ( pResolver->LockReady ) {
		__xrtNetResolverLock(pResolver, &Mutation);
		pResolver->Closing = true;
		if ( pResolver->ConditionReady ) {
			(void)xrtCondBroadcast(&pResolver->Condition);
		}
		__xrtNetResolverUnlock(pResolver, &Mutation);
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
static xnetresolver* __xrtNetResolverCreate(
	const xnetresolverconfig* pConfig,
	const xnetresolverlookupownershipv1* pPolicy
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
	xrtownershipscope Mutation = {0};
	__xrtNetResolverLock(pResolver, &Mutation);
	pResolver->LookupPolicy = pPolicy;
	pResolver->Initialized = true;
	pResolver->CreatorOwned = true;
	__xrtNetResolverUnlock(pResolver, &Mutation);
	return pResolver;
}

XRT_API xnetresolver* xrtNetResolverCreate(const xnetresolverconfig* pConfig)
{ return __xrtNetResolverCreate(pConfig, NULL); }

XRT_API xnetresolver* xrtNetResolverCreateOwnedV1(const xnetresolverconfig* pConfig,
	ptr pData, const xnetresolverlookupownershipv1* pPolicy)
{
	xnetresolverconfig Config;
	if (!pData || !pPolicy || pPolicy->size != sizeof(*pPolicy) || !pPolicy->Lookup ||
		!pPolicy->Drop || !pPolicy->Ops || !pPolicy->Ops->Count || !pPolicy->Ops->Trace ||
		(pConfig && (pConfig->Lookup || pConfig->LookupData))) {
		__xrtErrorSetInvalidArgument(); return NULL;
	}
	xrtNetResolverConfigInit(&Config);
	if (pConfig) Config = *pConfig;
	Config.Lookup = pPolicy->Lookup; Config.LookupData = pData;
	return __xrtNetResolverCreate(&Config, pPolicy);
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



/* Joining closes admission but retains EVERY owning slot. Graph preparation
 * and public retirement share this path; only a later commit consumes owners. */
static xnetretireresult __xrtNetResolverJoin(xnetresolver* pResolver, bool bWait,
	const void* pToken)
{
	xrtownershipscope Mutation = {0};
	__xrtNetResolverLock(pResolver, &Mutation);
	bool bWorker = __xrtNetResolverIsWorker(pResolver);
	/* A worker can request closing, but can never join or consume the creator.
	 * A collector's claim must not turn this cooperative BUSY into failure. */
	if (!bWorker && pResolver->OwnershipClaim && !pResolver->OwnershipCleared && pResolver->OwnershipClaim != pToken) {
		__xrtNetResolverUnlock(pResolver, &Mutation);
		__xrtNetResolverError(XERR_STATE, XNET_ERROR_RESOLVER_CLOSED,
			"destroy-resolver", "resolver is claimed by an ownership plan");
		return XNET_RETIRE_ERROR;
	}
	if (pResolver->Joined) { __xrtNetResolverUnlock(pResolver, &Mutation); return XNET_RETIRE_READY; }
	if (pResolver->Joining) { __xrtNetResolverUnlock(pResolver, &Mutation); return XNET_RETIRE_BUSY; }
	pResolver->Closing = true;
	if ( !xrtCondBroadcast(&pResolver->Condition) ) {
		__xrtNetResolverUnlock(pResolver, &Mutation);
		return XNET_RETIRE_ERROR;
	}
	if ( bWorker ) {
		__xrtNetResolverUnlock(pResolver, &Mutation);
		return XNET_RETIRE_BUSY;
	}
	pResolver->Joining = true;
	__xrtNetResolverUnlock(pResolver, &Mutation);
	xnetretireresult Result = XNET_RETIRE_READY;
	/* 未证明所有线程及 TLS 清理结束前，不销毁任何线程句柄或共享存储。 */
	for ( uint32 i = 0; i < pResolver->StartedThreads; i++ ) {
		xwaitresult Wait = bWait ? xrtThreadWait(pResolver->Threads[i]) :
			xrtThreadWaitFor(pResolver->Threads[i], 0);
		if ( Wait != XWAIT_OK ) {
			Result = Wait == XWAIT_TIMEOUT ? XNET_RETIRE_BUSY : XNET_RETIRE_ERROR;
			break;
		}
	}
	__xrtNetResolverLock(pResolver, &Mutation);
	pResolver->Joining = false;
	pResolver->Joined = Result == XNET_RETIRE_READY;
	__xrtNetResolverUnlock(pResolver, &Mutation);
	return Result;
}

/* Physical resources may be retired while real operation/plan owners keep the
 * queryable shell alive. Drop of certified lookup data occurs after native TLS. */
static void __xrtNetResolverRetireResources(xnetresolver* pResolver)
{
	xrtownershipscope Mutation = {0};
	__xrtNetResolverLock(pResolver, &Mutation);
	if (pResolver->Destroyed) { __xrtNetResolverUnlock(pResolver, &Mutation); return; }
	if (!pResolver->Joined || pResolver->Retiring || pResolver->Outstanding || pResolver->ActiveQueries) abort();
	pResolver->Retiring = true;
	__xrtNetResolverCacheClearLocked(pResolver);
	for ( uint32 i = 0; i < pResolver->StartedThreads; i++ ) {
		xrtThreadDestroy(pResolver->Threads[i]);
	}
	pResolver->StartedThreads = 0;
	pResolver->ExitedWorkers = 0;
	xrtFree(pResolver->Threads);
	xrtFree(pResolver->QueryBuckets);
	xrtFree(pResolver->CacheBuckets);
	pResolver->Threads = NULL;
	pResolver->QueryBuckets = NULL;
	pResolver->CacheBuckets = NULL;
	pResolver->QueryBucketCount = 0;
	pResolver->CacheBucketCount = 0;
	const xnetresolverlookupownershipv1* pPolicy = pResolver->LookupPolicy;
	ptr pData = pResolver->Config.LookupData;
	pResolver->LookupPolicy = NULL;
	pResolver->Config.LookupData = NULL;
	pResolver->Config.Lookup = NULL;
	__xrtNetResolverUnlock(pResolver, &Mutation);
	if (pPolicy) pPolicy->Drop(pData);
	__xrtNetResolverLock(pResolver, &Mutation);
	pResolver->Destroyed = true;
	pResolver->Retiring = false;
	__xrtNetResolverUnlock(pResolver, &Mutation);
}

static xnetretireresult __xrtNetResolverRetire(xnetresolver* pResolver, bool bWait)
{
	if (!pResolver) return XNET_RETIRE_READY;
	xrtownershipscope Mutation = {0};
	__xrtNetResolverLock(pResolver, &Mutation);
	bool bCreator = pResolver->CreatorOwned;
	bool bRetiring = pResolver->Retiring;
	__xrtNetResolverUnlock(pResolver, &Mutation);
	if (!bCreator) { __xrtErrorSetInvalidState(); return XNET_RETIRE_ERROR; }
	if (bRetiring) return XNET_RETIRE_BUSY;
	xnetretireresult Result = __xrtNetResolverJoin(pResolver, bWait, NULL);
	if (Result != XNET_RETIRE_READY) return Result;
	__xrtNetResolverRetireResources(pResolver);
	__xrtNetResolverLock(pResolver, &Mutation);
	pResolver->CreatorOwned = false;
	__xrtNetResolverUnlock(pResolver, &Mutation);
	__xrtNetResolverRelease(pResolver);
	return XNET_RETIRE_READY;
}



/* 停止接收、排空已受理查询与回调，再释放 Resolver 所有运行资源。 */
XRT_API bool xrtNetResolverDestroy(xnetresolver* pResolver)
{
	if ( pResolver == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtNetResolverIsWorker(pResolver) ) {
		__xrtNetResolverError(XERR_STATE, XNET_ERROR_RESOLVER_CLOSED,
			"destroy-resolver", "resolver cannot be destroyed by its own worker");
		return false;
	}
	return __xrtNetResolverRetire(pResolver, true) == XNET_RETIRE_READY;
}



/* 可重试的退休步骤；正常等待状态不替换调用方已有的错误。 */
XRT_API xnetretireresult xrtNetResolverTryDestroy(xnetresolver* pResolver)
{
	xerror* pPrevious = __xrtErrorSwapOwned(NULL);
	xnetretireresult Result = __xrtNetResolverRetire(pResolver, false);

	if ( Result != XNET_RETIRE_ERROR ) {
		xrtErrorFree(__xrtErrorSwapOwned(pPrevious));
	} else {
		xrtErrorFree(pPrevious);
	}
	return Result;
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
static xnetresolveop* __xrtNetResolverResolve(
	xnetresolver* pResolver,
	cstr sHost,
	xnetfamily Family,
	xnetresolveproc pDone,
	ptr pData,
	const xnetresolveownershipv1* pPolicy
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
	if ( pResolver->Closing || pResolver->Destroyed || pResolver->OwnershipClaim || pResolver->OwnershipCleared ) {
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
		pOperation->OwnershipPolicy = pPolicy;
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

XRT_API xnetresolveop* xrtNetResolverResolve(xnetresolver* pResolver, cstr sHost,
	xnetfamily Family, xnetresolveproc pDone, ptr pData)
{
	XRT_OWNERSHIP_MUTATION_RETURN(xnetresolveop*, NULL,
		__xrtNetResolverResolve(pResolver, sHost, Family, pDone, pData, NULL));
}
XRT_API xnetresolveop* xrtNetResolverResolveOwnedV1(xnetresolver* pResolver, cstr sHost,
	xnetfamily Family, ptr pData, const xnetresolveownershipv1* pPolicy)
{
	if (!pData || !pPolicy || pPolicy->size != sizeof(*pPolicy) || !pPolicy->Done ||
		!pPolicy->Drop || !pPolicy->Ops || !pPolicy->Ops->Count || !pPolicy->Ops->Trace) {
		__xrtErrorSetInvalidArgument(); return NULL;
	}
	XRT_OWNERSHIP_MUTATION_RETURN(xnetresolveop*, NULL,
		__xrtNetResolverResolve(pResolver, sHost, Family, pPolicy->Done, pData, pPolicy));
}



/* 清空缓存但不影响任何活动查询或已经交付的共享结果。 */
static bool __xrtNetResolverClear(xnetresolver* pResolver)
{
	if ( pResolver == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	(void)xrtMutexLock(&pResolver->Lock);
	if ( pResolver->Closing || pResolver->Destroyed || pResolver->OwnershipClaim || pResolver->OwnershipCleared ) {
		(void)xrtMutexUnlock(&pResolver->Lock);
		__xrtNetResolverError(XERR_CLOSED, XNET_ERROR_RESOLVER_CLOSED,
			"clear-resolver", "resolver is closing or destroyed");
		return false;
	}
	__xrtNetResolverCacheClearLocked(pResolver);
	(void)xrtMutexUnlock(&pResolver->Lock);
	return true;
}

XRT_API bool xrtNetResolverClear(xnetresolver* pResolver)
{ XRT_OWNERSHIP_MUTATION_RETURN(bool, false, __xrtNetResolverClear(pResolver)); }



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
static xnetresolveop* __xrtNetResolveOpRef(xnetresolveop* pOperation)
{
	if ( (pOperation == NULL) || pOperation->OwnershipCleared ||
		 (xrtRefRetain(&pOperation->RefCount) < 0) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return pOperation;
}

XRT_API xnetresolveop* xrtNetResolveOpRef(xnetresolveop* pOperation)
{ XRT_OWNERSHIP_MUTATION_RETURN(xnetresolveop*, NULL, __xrtNetResolveOpRef(pOperation)); }



/* 释放解析操作的调用方引用。 */
XRT_API void xrtNetResolveOpDestroy(xnetresolveop* pOperation)
{
	__xrtNetResolveOpRelease(pOperation);
}



/* 取消单个订阅者；无订阅者的排队查询会立即从工作队列移除。 */
static bool __xrtNetResolveOpCancel(xnetresolveop* pOperation)
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

XRT_API bool xrtNetResolveOpCancel(xnetresolveop* pOperation)
{ XRT_OWNERSHIP_MUTATION_RETURN(bool, false, __xrtNetResolveOpCancel(pOperation)); }



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

/* No snapshots of executing native bodies: their temporary owners and borrowed
 * callbacks are not durable queue slots. Parked workers are physical state,
 * never invented strong references. All accepted queue transitions participate. */
static bool __xrtNetResolverOwnershipCount(const void* pData, size_t* pCount)
{
	const xnetresolver* pResolver = pData;
	if (!pResolver || !pCount || !pResolver->Initialized || pResolver->Joining ||
		pResolver->Retiring || pResolver->OwnershipCleared || pResolver->RunningQueries ||
		pResolver->ActiveCallbacks ||
		pResolver->ParkedWorkers + pResolver->ExitedWorkers != pResolver->StartedThreads ||
		pResolver->ActiveQueries != pResolver->QueuedQueries ||
		(pResolver->Config.Lookup && pResolver->Config.Lookup != __xrtNetResolverLookupDefault && !pResolver->LookupPolicy)) return false;
	/* A prior nonblocking Prepare can have requested exit without observing
	 * completion yet. Requiring Joined here forever prevents a graph planner
	 * from revalidating and calling Prepare again. Observe the actual thread
	 * completion read-only instead: cleanup in progress STILL refuses, and no
	 * state mutex is waited for while frozen. Ready continues to require Join. */
	if (pResolver->ExitedWorkers && !pResolver->Joined) {
		if (pResolver->ExitedWorkers != pResolver->StartedThreads) return false;
		for (uint32 i = 0; i < pResolver->StartedThreads; ++i) {
			xthreadstate State;
			if (!xrtThreadStateTry(pResolver->Threads[i], &State) || State != XTHREAD_FINISHED) return false;
		}
	}
	int32 iReferences = __xrtAtomicRefLoad(&pResolver->RefCount);
	if (iReferences <= 0) return false;
	size_t iGroups = 0, iRequests = 0, iReady = 0, iCaches = 0;
	const xrt_net_resolve_group* pPrevious = NULL;
	for (const xrt_net_resolve_group* pGroup = pResolver->QueryHead; pGroup; pGroup = pGroup->QueueNext) {
		if (iGroups++ == pResolver->QueuedQueries || pGroup->Running || pGroup->QueuePrevious != pPrevious) return false;
		const xnetresolveop* pLast = NULL;
		for (const xnetresolveop* pOperation = pGroup->RequestHead; pOperation; pOperation = pOperation->GroupNext) {
			if (iRequests++ == pResolver->Outstanding || pOperation->Resolver != pResolver ||
				pOperation->Group != pGroup || pOperation->GroupPrevious != pLast || pOperation->CallbackActive) return false;
			pLast = pOperation;
		}
		if (!pLast || pLast != pGroup->RequestTail) return false;
		pPrevious = pGroup;
	}
	if (iGroups != pResolver->QueuedQueries || pPrevious != pResolver->QueryTail) return false;
	const xnetresolveop* pLast = NULL;
	for (const xnetresolveop* pOperation = pResolver->ReadyHead; pOperation; pOperation = pOperation->ReadyNext) {
		if (iReady++ == pResolver->ReadyCallbacks || iRequests++ == pResolver->Outstanding ||
			pOperation->Resolver != pResolver || pOperation->Group || pOperation->CallbackActive) return false;
		pLast = pOperation;
	}
	if (iReady != pResolver->ReadyCallbacks || pLast != pResolver->ReadyTail || iRequests != pResolver->Outstanding) return false;
	const xrt_net_resolver_cache* pCachePrevious = NULL;
	for (const xrt_net_resolver_cache* pCache = pResolver->LRUHead; pCache; pCache = pCache->LRUNext) {
		if (iCaches++ == pResolver->CachedResults || pCache->LRUPrevious != pCachePrevious) return false;
		pCachePrevious = pCache;
	}
	if (iCaches != pResolver->CachedResults || pCachePrevious != pResolver->LRUTail) return false;
	*pCount = (size_t)iReferences; return true;
}
static bool __xrtNetResolverOwnershipTrace(const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{
	const xnetresolver* pResolver = pData; size_t iCount;
	if (!pVisit || !__xrtNetResolverOwnershipCount(pData, &iCount)) return false;
	if (!pVisit(xrtErrorOwnership(pResolver->CancelError), pContext)) return false;
	if (pResolver->LookupPolicy && !pVisit((xrtownershipref){pResolver->Config.LookupData, pResolver->LookupPolicy->Ops}, pContext)) return false;
	for (const xrt_net_resolve_group* pGroup = pResolver->QueryHead; pGroup; pGroup = pGroup->QueueNext)
		for (const xnetresolveop* pOperation = pGroup->RequestHead; pOperation; pOperation = pOperation->GroupNext)
			if (!pVisit(xrtNetResolveOpOwnership(pOperation), pContext)) return false;
	for (const xnetresolveop* pOperation = pResolver->ReadyHead; pOperation; pOperation = pOperation->ReadyNext)
		if (!pVisit(xrtNetResolveOpOwnership(pOperation), pContext)) return false;
	for (const xrt_net_resolver_cache* pCache = pResolver->LRUHead; pCache; pCache = pCache->LRUNext)
		if (!pVisit(xrtNetAddrListOwnership(pCache->Addresses), pContext) || !pVisit(xrtErrorOwnership(pCache->Error), pContext)) return false;
	return true;
}
static const xrtownershipops __xrtNetResolverOwnershipOps = {__xrtNetResolverOwnershipCount, __xrtNetResolverOwnershipTrace};
XRT_API xrtownershipref xrtNetResolverOwnership(const xnetresolver* pResolver)
{ return (xrtownershipref){pResolver, pResolver ? &__xrtNetResolverOwnershipOps : NULL}; }
static bool __xrtNetResolverHold(const void* pData)
{
	xnetresolver* pResolver = (xnetresolver*)pData; xrtownershipscope Mutation = {0};
	__xrtNetResolverLock(pResolver, &Mutation);
	bool bHeld = !pResolver->OwnershipCleared && __xrtNetResolverRetain(pResolver);
	__xrtNetResolverUnlock(pResolver, &Mutation); return bHeld;
}
static void __xrtNetResolverDrop(const void* pData) { __xrtNetResolverRelease((xnetresolver*)pData); }
static bool __xrtNetResolverClaim(const void* pData, const void* pToken)
{
	xnetresolver* pResolver = (xnetresolver*)pData; size_t iCount;
	if (!pToken || !__xrtNetResolverOwnershipCount(pData, &iCount) ||
		(pResolver->OwnershipClaim && pResolver->OwnershipClaim != pToken)) return false;
	pResolver->OwnershipClaim = pToken; return true;
}
static void __xrtNetResolverRestore(const void* pData, const void* pToken)
{
	xnetresolver* pResolver = (xnetresolver*)pData;
	if (!pToken || pResolver->OwnershipClaim != pToken || pResolver->OwnershipCleared) abort();
	pResolver->OwnershipClaim = NULL;
}
static bool __xrtNetResolverPrepared(const void* pData)
{
	const xnetresolver* pResolver = pData;
	return pResolver->Joined && !pResolver->Joining && !pResolver->Retiring && !pResolver->Outstanding &&
		!pResolver->ActiveQueries && !pResolver->ActiveCallbacks && !pResolver->ParkedWorkers;
}
static xrtownershipprepareresult __xrtNetResolverPrepare(const void* pData, const void* pToken)
{
	xnetresolver* pResolver = (xnetresolver*)pData; xrtownershipscope Mutation = {0};
	__xrtNetResolverLock(pResolver, &Mutation);
	if (!pToken || pResolver->OwnershipClaim != pToken || pResolver->OwnershipCleared) abort();
	__xrtNetResolverUnlock(pResolver, &Mutation);
	xnetretireresult Result = __xrtNetResolverJoin(pResolver, false, pToken);
	return Result == XNET_RETIRE_READY ? XRT_OWNERSHIP_PREPARE_READY :
		Result == XNET_RETIRE_BUSY ? XRT_OWNERSHIP_PREPARE_BUSY : XRT_OWNERSHIP_PREPARE_FAILED;
}
static void __xrtNetResolverClearOwnership(const void* pData, const void* pToken)
{
	xnetresolver* pResolver = (xnetresolver*)pData;
	if (!pToken || pResolver->OwnershipClaim != pToken || pResolver->OwnershipCleared || !__xrtNetResolverPrepared(pData)) abort();
	pResolver->OwnershipCleared = true;
}
static bool __xrtNetResolverFinish(const void* pData, const void* pToken)
{
	xnetresolver* pResolver = (xnetresolver*)pData;
	if (!pToken || pResolver->OwnershipClaim != pToken || !pResolver->OwnershipCleared) abort();
	__xrtNetResolverRetireResources(pResolver); return true;
}
XRT_API const xrtownershipadapterv1* xrtNetResolverOwnershipAdapterV1(xrtownershipref Reference,
	const xnetresolverlookupownershipv1* const* pPolicies, size_t iPolicyCount,
	const xrtownershippreparationv1** ppPreparation)
{
	static const xrtownershipadapterv1 Adapter = {sizeof(Adapter), __xrtNetResolverHold, __xrtNetResolverDrop,
		__xrtNetResolverClaim, __xrtNetResolverRestore, NULL, __xrtNetResolverClearOwnership, __xrtNetResolverFinish};
	static const xrtownershippreparationv1 Preparation = {sizeof(Preparation), &Adapter, __xrtNetResolverPrepared, __xrtNetResolverPrepare};
	if (!ppPreparation || Reference.Ops != &__xrtNetResolverOwnershipOps || !Reference.Data || (iPolicyCount && !pPolicies)) return NULL;
	const xnetresolver* pResolver = Reference.Data; size_t iCount;
	if (pResolver->LookupPolicy) {
		bool bKnown = false;
		for (size_t i = 0; i < iPolicyCount; ++i) if (pPolicies[i] && pPolicies[i] == pResolver->LookupPolicy) { bKnown = true; break; }
		if (!bKnown) return NULL;
		const xnetresolverlookupownershipv1* pPolicy = pResolver->LookupPolicy;
		if (pPolicy->size != sizeof(*pPolicy) || pPolicy->Lookup != pResolver->Config.Lookup || !pPolicy->Drop ||
			!pPolicy->Ops || !pPolicy->Ops->Count || !pPolicy->Ops->Trace || !pResolver->Config.LookupData) return NULL;
	}
	if (!__xrtNetResolverOwnershipCount(Reference.Data, &iCount)) return NULL;
	*ppPreparation = &Preparation; return &Adapter;
}

static bool __xrtNetResolveOpOwnershipCount(const void* pData, size_t* pCount)
{
	const xnetresolveop* pOperation = pData;
	if (!pOperation || !pCount || pOperation->OwnershipCleared || pOperation->CallbackActive ||
		(pOperation->Done && !pOperation->OwnershipPolicy) ||
		(xrtAtomic32Load(&pOperation->CallbackClaimed, XMEMORY_ACQUIRE) && !pOperation->CallbackFinished)) return false;
	int32 iCount = __xrtAtomicRefLoad(&pOperation->RefCount);
	if (iCount <= 0) return false;
	*pCount = (size_t)iCount; return true;
}
static bool __xrtNetResolveOpOwnershipTrace(const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{
	const xnetresolveop* pOperation = pData; size_t iCount;
	if (!pVisit || !__xrtNetResolveOpOwnershipCount(pData, &iCount)) return false;
	return pVisit(xrtNetResolverOwnership(pOperation->Resolver), pContext) &&
		pVisit(xrtNetAddrListOwnership(pOperation->Addresses), pContext) && pVisit(xrtErrorOwnership(pOperation->Error), pContext) &&
		(!pOperation->OwnershipPolicy || pVisit((xrtownershipref){pOperation->Data, pOperation->OwnershipPolicy->Ops}, pContext));
}
static const xrtownershipops __xrtNetResolveOpOwnershipOps = {__xrtNetResolveOpOwnershipCount, __xrtNetResolveOpOwnershipTrace};
XRT_API xrtownershipref xrtNetResolveOpOwnership(const xnetresolveop* pOperation)
{ return (xrtownershipref){pOperation, pOperation ? &__xrtNetResolveOpOwnershipOps : NULL}; }
static bool __xrtNetResolveOpHold(const void* pData) { return xrtNetResolveOpRef((xnetresolveop*)pData) != NULL; }
static void __xrtNetResolveOpDrop(const void* pData) { xrtNetResolveOpDestroy((xnetresolveop*)pData); }
static bool __xrtNetResolveOpClaim(const void* pData, const void* pToken)
{
	xnetresolveop* pOperation = (xnetresolveop*)pData; size_t iCount;
	if (!pToken || !__xrtNetResolveOpOwnershipCount(pData, &iCount) ||
		(pOperation->OwnershipClaim && pOperation->OwnershipClaim != pToken)) return false;
	pOperation->OwnershipClaim = pToken; return true;
}
static void __xrtNetResolveOpRestore(const void* pData, const void* pToken)
{
	xnetresolveop* pOperation = (xnetresolveop*)pData;
	if (!pToken || pOperation->OwnershipClaim != pToken || pOperation->OwnershipCleared) abort();
	pOperation->OwnershipClaim = NULL;
}
static bool __xrtNetResolveOpPrepared(const void* pData)
{
	const xnetresolveop* pOperation = pData;
	return pOperation->CallbackFinished && !pOperation->CallbackActive && !pOperation->OwnershipPolicy && !pOperation->Group;
}
static xrtownershipprepareresult __xrtNetResolveOpPrepare(const void* pData, const void* pToken)
{
	const xnetresolveop* pOperation = pData; xrtownershipscope Freeze = {0};
	if (!xrtOwnershipFreezeTryBegin(&Freeze)) return XRT_OWNERSHIP_PREPARE_BUSY;
	if (!pToken || pOperation->OwnershipClaim != pToken || pOperation->OwnershipCleared) abort();
	bool bReady = __xrtNetResolveOpPrepared(pData);
	__xrtNetResolverMutationEnd(&Freeze);
	return bReady ? XRT_OWNERSHIP_PREPARE_READY : XRT_OWNERSHIP_PREPARE_BUSY;
}
static void __xrtNetResolveOpClearOwnership(const void* pData, const void* pToken)
{
	xnetresolveop* pOperation = (xnetresolveop*)pData;
	if (!pToken || pOperation->OwnershipClaim != pToken || pOperation->OwnershipCleared || !__xrtNetResolveOpPrepared(pData)) abort();
	pOperation->OwnershipCleared = true;
}
static bool __xrtNetResolveOpFinish(const void* pData, const void* pToken)
{
	xnetresolveop* pOperation = (xnetresolveop*)pData; xrtownershipscope Mutation = {0};
	__xrtNetResolverMutationBegin(&Mutation);
	if (!pToken || pOperation->OwnershipClaim != pToken || !pOperation->OwnershipCleared) abort();
	xnetresolver* pResolver = pOperation->Resolver; xnetaddrlist* pAddresses = pOperation->Addresses; xerror* pError = pOperation->Error;
	pOperation->Resolver = NULL; pOperation->Addresses = NULL; pOperation->Error = NULL;
	__xrtNetResolverMutationEnd(&Mutation);
	xrtNetAddrListDestroy(pAddresses); xrtErrorFree(pError); __xrtNetResolverRelease(pResolver); return true;
}
XRT_API const xrtownershipadapterv1* xrtNetResolveOpOwnershipAdapterV1(xrtownershipref Reference,
	const xnetresolveownershipv1* const* pPolicies, size_t iPolicyCount,
	const xrtownershippreparationv1** ppPreparation)
{
	static const xrtownershipadapterv1 Adapter = {sizeof(Adapter), __xrtNetResolveOpHold, __xrtNetResolveOpDrop,
		__xrtNetResolveOpClaim, __xrtNetResolveOpRestore, NULL, __xrtNetResolveOpClearOwnership, __xrtNetResolveOpFinish};
	static const xrtownershippreparationv1 Preparation = {sizeof(Preparation), &Adapter, __xrtNetResolveOpPrepared, __xrtNetResolveOpPrepare};
	if (!ppPreparation || Reference.Ops != &__xrtNetResolveOpOwnershipOps || !Reference.Data || (iPolicyCount && !pPolicies)) return NULL;
	const xnetresolveop* pOperation = Reference.Data; size_t iCount;
	if (pOperation->OwnershipPolicy) {
		bool bKnown = false;
		for (size_t i = 0; i < iPolicyCount; ++i) if (pPolicies[i] && pPolicies[i] == pOperation->OwnershipPolicy) { bKnown = true; break; }
		if (!bKnown) return NULL;
		const xnetresolveownershipv1* pPolicy = pOperation->OwnershipPolicy;
		if (pPolicy->size != sizeof(*pPolicy) || pPolicy->Done != pOperation->Done || !pPolicy->Drop ||
			!pPolicy->Ops || !pPolicy->Ops->Count || !pPolicy->Ops->Trace || !pOperation->Data) return NULL;
	}
	if (!__xrtNetResolveOpOwnershipCount(Reference.Data, &iCount)) return NULL;
	*ppPreparation = &Preparation; return &Adapter;
}

#endif
