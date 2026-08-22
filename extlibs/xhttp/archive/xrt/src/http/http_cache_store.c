#include "../internal/xrt_http_cache_store.h"



#if defined(XRT_FEATURE_HTTP_CACHE_STORE)

/* Node 只保存索引、LRU 和 Record 引用，不复制响应内容。 */
typedef struct xrt_http_cache_node {
	xhttpcacherecord* Record;
	struct xrt_http_cache_node* BucketNext;
	struct xrt_http_cache_node* LRUPrevious;
	struct xrt_http_cache_node* LRUNext;
	struct xrt_http_cache_node* RetireNext;
	uint64 Sequence;
} xrt_http_cache_node;



/* Cache 的 Map、LRU、限额和统计全部由同一 Mutex 保护。 */
struct xhttpcache {
	volatile int32 References;
	xhttpcacheops Ops;
	ptr Context;
	xmutex Lock;
	xmap Index;
	xhttpcacheconfig Config;
	xrt_http_cache_node* LRUHead;
	xrt_http_cache_node* LRUTail;
	xhttpcachestats Stats;
	uint64 Sequence;
	bool Custom;
	bool LockReady;
	bool IndexReady;
};



/* 内部状态检查区分逻辑损坏、容量拒绝和可提交。 */
typedef enum xrt_http_cache_fit {
	XRT_HTTP_CACHE_FIT_ERROR = -1,
	XRT_HTTP_CACHE_FIT_REJECTED = 0,
	XRT_HTTP_CACHE_FIT_READY = 1
} xrt_http_cache_fit;



/* 把文本 URI 转换为 Map 使用的只读字节键。 */
static xbytesview __xrtHttpCacheMapKey(xstrview URI)
{
	return (xbytesview){ (cbytes)URI.Data, URI.Size };
}



/* 判断两个普通视图是否逐字节相同。 */
static bool __xrtHttpCacheStoreViewEqual(
	xstrview Left,
	xstrview Right
)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 单调统计达到上限后保持饱和。 */
static void __xrtHttpCacheCounterAdd(
	uint64* pCounter,
	uint64 iValue
)
{
	if ( *pCounter > (UINT64_MAX - iValue) ) {
		*pCounter = UINT64_MAX;
	} else {
		*pCounter += iValue;
	}
}



/*
	验证一次原子状态变更可以由 size_t 精确表示。
	容量超限随后由 LRU 处理；这里仅阻止计数回绕后绕过硬限额。
*/
static xrt_http_cache_fit __xrtHttpCacheStateFits(
	const xhttpcache* pCache,
	size_t iRemoveEntries,
	size_t iRemoveBytes,
	size_t iAddEntries,
	size_t iAddBytes
)
{
	size_t iEntries;
	size_t iBytes;

	if ( (iRemoveEntries > pCache->Stats.Entries) ||
		(iRemoveBytes > pCache->Stats.Bytes) ) {
		__xrtErrorSetInternal();
		return XRT_HTTP_CACHE_FIT_ERROR;
	}
	iEntries = pCache->Stats.Entries - iRemoveEntries;
	iBytes = pCache->Stats.Bytes - iRemoveBytes;
	if ( (iAddEntries > (SIZE_MAX - iEntries)) ||
		(iAddBytes > (SIZE_MAX - iBytes)) ) {
		__xrtErrorSetRange();
		return XRT_HTTP_CACHE_FIT_REJECTED;
	}
	return XRT_HTTP_CACHE_FIT_READY;
}



/* 为新写入分配一个单调序号，极端溢出后保持饱和。 */
static uint64 __xrtHttpCacheSequence(xhttpcache* pCache)
{
	if ( pCache->Sequence != UINT64_MAX ) {
		pCache->Sequence++;
	}
	return pCache->Sequence;
}



/* 解析并验证 Cache 配置。 */
static bool __xrtHttpCacheConfigResolve(
	const xhttpcacheconfig* pInput,
	xhttpcacheconfig* pConfig
)
{
	xrtHttpCacheConfigInit(pConfig);
	if ( pInput != NULL ) {
		if ( !__xrtRangeValid(pInput, sizeof(*pConfig)) ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
		memcpy(pConfig, pInput, sizeof(*pConfig));
	}
	if ( (pConfig->MaxEntries == 0) ||
		(pConfig->MaxBytes == 0) ||
		(pConfig->MaxEntryBytes == 0) ||
		(pConfig->InitialEntries > pConfig->MaxEntries) ||
		(pConfig->MaxEntryBytes > pConfig->MaxBytes) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 从 LRU 双向链摘除 Node。 */
static void __xrtHttpCacheLRURemove(
	xhttpcache* pCache,
	xrt_http_cache_node* pNode
)
{
	if ( pNode->LRUPrevious != NULL ) {
		pNode->LRUPrevious->LRUNext = pNode->LRUNext;
	} else {
		pCache->LRUHead = pNode->LRUNext;
	}
	if ( pNode->LRUNext != NULL ) {
		pNode->LRUNext->LRUPrevious =
			pNode->LRUPrevious;
	} else {
		pCache->LRUTail = pNode->LRUPrevious;
	}
	pNode->LRUPrevious = NULL;
	pNode->LRUNext = NULL;
}



/* 把 Node 放到 LRU 最新端。 */
static void __xrtHttpCacheLRUFirst(
	xhttpcache* pCache,
	xrt_http_cache_node* pNode
)
{
	pNode->LRUPrevious = NULL;
	pNode->LRUNext = pCache->LRUHead;
	if ( pCache->LRUHead != NULL ) {
		pCache->LRUHead->LRUPrevious = pNode;
	} else {
		pCache->LRUTail = pNode;
	}
	pCache->LRUHead = pNode;
}



/* 把已存在 Node 移到 LRU 最新端。 */
static void __xrtHttpCacheLRUTouch(
	xhttpcache* pCache,
	xrt_http_cache_node* pNode
)
{
	if ( pCache->LRUHead == pNode ) {
		return;
	}
	__xrtHttpCacheLRURemove(pCache, pNode);
	__xrtHttpCacheLRUFirst(pCache, pNode);
}



/* 把摘除 Node 链接到锁外回收列表。 */
static void __xrtHttpCacheRetire(
	xrt_http_cache_node** ppRetired,
	xrt_http_cache_node* pNode
)
{
	pNode->RetireNext = *ppRetired;
	*ppRetired = pNode;
}



/* 在锁外释放一组已经从索引和 LRU 摘除的 Node。 */
static void __xrtHttpCacheRetiredRelease(
	xrt_http_cache_node* pRetired
)
{
	while ( pRetired != NULL ) {
		xrt_http_cache_node* pNext = pRetired->RetireNext;

		xrtHttpCacheRecordRelease(pRetired->Record);
		memset(pRetired, 0, sizeof(*pRetired));
		xrtFree(pRetired);
		pRetired = pNext;
	}
}



/* 在锁外按原 LRU 链释放全部 Node。 */
static void __xrtHttpCacheLRURelease(
	xrt_http_cache_node* pNode
)
{
	while ( pNode != NULL ) {
		xrt_http_cache_node* pNext = pNode->LRUNext;

		xrtHttpCacheRecordRelease(pNode->Record);
		memset(pNode, 0, sizeof(*pNode));
		xrtFree(pNode);
		pNode = pNext;
	}
}



/* 返回 URI 对应的可写 Bucket 头槽。 */
static xrt_http_cache_node** __xrtHttpCacheBucket(
	xhttpcache* pCache,
	xstrview URI
)
{
	return (xrt_http_cache_node**)xrtMapGet(
		&pCache->Index,
		__xrtHttpCacheMapKey(URI)
	);
}



/* 从 URI Bucket 摘除指定 Node，并在 Bucket 变空时删除 Map 键。 */
static bool __xrtHttpCacheBucketRemove(
	xhttpcache* pCache,
	xrt_http_cache_node* pNode
)
{
	const xhttpcachekey* pKey =
		xrtHttpCacheRecordKey(pNode->Record);
	xbytesview MapKey = __xrtHttpCacheMapKey(pKey->URI);
	xrt_http_cache_node** ppHead =
		(xrt_http_cache_node**)xrtMapGet(
			&pCache->Index, MapKey
		);
	xrt_http_cache_node** ppNode;

	if ( ppHead == NULL ) {
		return false;
	}
	ppNode = ppHead;
	while ( (*ppNode != NULL) && (*ppNode != pNode) ) {
		ppNode = &(*ppNode)->BucketNext;
	}
	if ( *ppNode == NULL ) {
		return false;
	}
	*ppNode = pNode->BucketNext;
	pNode->BucketNext = NULL;
	if ( *ppHead == NULL ) {
		(void)xrtMapRemove(&pCache->Index, MapKey);
	}
	return true;
}



/* 从 Cache 计数、索引和 LRU 摘除一个 Node。 */
static bool __xrtHttpCacheNodeRemove(
	xhttpcache* pCache,
	xrt_http_cache_node* pNode,
	xrt_http_cache_node** ppRetired
)
{
	size_t iCharge = xrtHttpCacheRecordCharge(pNode->Record);

	if ( !__xrtHttpCacheBucketRemove(pCache, pNode) ) {
		return false;
	}
	__xrtHttpCacheLRURemove(pCache, pNode);
	pCache->Stats.Entries--;
	pCache->Stats.Bytes -= iCharge;
	__xrtHttpCacheRetire(ppRetired, pNode);
	return true;
}



/* 淘汰最旧 Node 直到条目数和 Record 字节数同时满足硬限额。 */
static void __xrtHttpCacheLimit(
	xhttpcache* pCache,
	xrt_http_cache_node** ppRetired
)
{
	while ( (pCache->Stats.Entries >
			pCache->Config.MaxEntries) ||
		(pCache->Stats.Bytes > pCache->Config.MaxBytes) ) {
		xrt_http_cache_node* pNode = pCache->LRUTail;

		if ( (pNode == NULL) ||
			!__xrtHttpCacheNodeRemove(
				pCache, pNode, ppRetired
			) ) {
			break;
		}
		__xrtHttpCacheCounterAdd(
			&pCache->Stats.Evictions, 1
		);
	}
}



/* 在 URI Bucket 中查找相同主键和 Vary 变体。 */
static xrt_http_cache_node* __xrtHttpCacheVariant(
	xrt_http_cache_node* pNode,
	const xhttpcacherecord* pRecord,
	xrt_http_cache_node** ppPrevious
)
{
	xrt_http_cache_node* pPrevious = NULL;

	while ( pNode != NULL ) {
		if ( __xrtHttpCacheRecordVariantEqual(
			pNode->Record, pRecord
		) ) {
			if ( ppPrevious != NULL ) {
				*ppPrevious = pPrevious;
			}
			return pNode;
		}
		pPrevious = pNode;
		pNode = pNode->BucketNext;
	}
	if ( ppPrevious != NULL ) {
		*ppPrevious = NULL;
	}
	return NULL;
}



/* 把 Bucket 中的已有 Node 移到最新端，保持相同时间下的确定选择。 */
static void __xrtHttpCacheBucketFirst(
	xrt_http_cache_node** ppHead,
	xrt_http_cache_node* pNode,
	xrt_http_cache_node* pPrevious
)
{
	if ( pPrevious == NULL ) {
		return;
	}
	pPrevious->BucketNext = pNode->BucketNext;
	pNode->BucketNext = *ppHead;
	*ppHead = pNode;
}



/* 判断候选是否比当前选择更新。 */
static bool __xrtHttpCacheNewer(
	const xrt_http_cache_node* pCandidate,
	const xrt_http_cache_node* pSelected
)
{
	xtime iCandidate;
	xtime iSelected;

	if ( pSelected == NULL ) {
		return true;
	}
	iCandidate = __xrtHttpCacheRecordSelectionTime(
		pCandidate->Record
	);
	iSelected = __xrtHttpCacheRecordSelectionTime(
		pSelected->Record
	);
	if ( iCandidate != iSelected ) {
		return iCandidate > iSelected;
	}
	return pCandidate->Sequence > pSelected->Sequence;
}



/* 创建线程安全的有界内存缓存。 */
XRT_API xhttpcache* xrtHttpCacheCreate(
	const xhttpcacheconfig* pConfig
)
{
	xhttpcacheconfig Config;
	xhttpcache* pCache;

	if ( !__xrtHttpCacheConfigResolve(pConfig, &Config) ) {
		return NULL;
	}
	pCache = (xhttpcache*)xrtCalloc(1, sizeof(*pCache));
	if ( pCache == NULL ) {
		return NULL;
	}
	pCache->References = 1;
	pCache->Config = Config;
	if ( !xrtMutexInit(&pCache->Lock) ) {
		xrtFree(pCache);
		return NULL;
	}
	pCache->LockReady = true;
	if ( !xrtMapInit(&pCache->Index, sizeof(ptr)) ) {
		(void)xrtMutexUnit(&pCache->Lock);
		xrtFree(pCache);
		return NULL;
	}
	pCache->IndexReady = true;
	if ( (Config.InitialEntries != 0) &&
		!xrtMapReserve(
			&pCache->Index, Config.InitialEntries
		) ) {
		xrtMapUnit(&pCache->Index);
		(void)xrtMutexUnit(&pCache->Lock);
		xrtFree(pCache);
		return NULL;
	}
	return pCache;
}



/* 校验自定义后端必须实现的完整操作集合。 */
static bool __xrtHttpCacheOpsValid(const xhttpcacheops* pOps)
{
	if ( (pOps == NULL) ||
		(pOps->Get == NULL) ||
		(pOps->Put == NULL) ||
		(pOps->Insert == NULL) ||
		(pOps->Replace == NULL) ||
		(pOps->RemoveRecord == NULL) ||
		(pOps->Remove == NULL) ||
		(pOps->RemoveURI == NULL) ||
		(pOps->Clear == NULL) ||
		(pOps->Stats == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 创建委托给自定义线程安全后端的统一 Cache 句柄。 */
XRT_API xhttpcache* xrtHttpCacheOpen(
	const xhttpcacheops* pOps,
	ptr pContext
)
{
	xhttpcacheops Ops;
	xhttpcache* pCache;

	if ( !__xrtRangeValid(pOps, sizeof(Ops)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	memcpy(&Ops, pOps, sizeof(Ops));
	if ( !__xrtHttpCacheOpsValid(&Ops) ) {
		return NULL;
	}
	pCache = (xhttpcache*)xrtCalloc(1, sizeof(*pCache));
	if ( pCache == NULL ) {
		return NULL;
	}
	pCache->References = 1;
	pCache->Ops = Ops;
	pCache->Context = pContext;
	pCache->Custom = true;
	return pCache;
}



/* 增加 Cache 引用并返回原指针。 */
XRT_API xhttpcache* xrtHttpCacheRetain(const xhttpcache* pCache)
{
	if ( (pCache == NULL) ||
		(xrtRefRetain(
			&((xhttpcache*)pCache)->References
		) < 0) ) {
		return NULL;
	}
	return (xhttpcache*)pCache;
}



/* 释放 Cache 最后一个引用及其全部 Node。 */
XRT_API void xrtHttpCacheRelease(xhttpcache* pCache)
{
	xrt_http_cache_node* pNodes;

	if ( (pCache == NULL) ||
		(xrtRefRelease(&pCache->References) != 0) ) {
		return;
	}
	if ( pCache->Custom ) {
		if ( pCache->Ops.Close != NULL ) {
			pCache->Ops.Close(pCache->Context);
		}
		memset(pCache, 0, sizeof(*pCache));
		xrtFree(pCache);
		return;
	}
	pNodes = pCache->LRUHead;
	pCache->LRUHead = NULL;
	pCache->LRUTail = NULL;
	if ( pCache->IndexReady ) {
		xrtMapUnit(&pCache->Index);
		pCache->IndexReady = false;
	}
	if ( pCache->LockReady ) {
		(void)xrtMutexUnit(&pCache->Lock);
		pCache->LockReady = false;
	}
	__xrtHttpCacheLRURelease(pNodes);
	memset(pCache, 0, sizeof(*pCache));
	xrtFree(pCache);
}



/* 查找最新匹配记录并返回独立引用。 */
XRT_API xhttpcachelookup xrtHttpCacheGet(
	xhttpcache* pCache,
	const xhttpcachekey* pKey,
	xhttpcacherecord** ppRecord
)
{
	xhttpcachekey Key;
	xrt_http_cache_node* pNode;
	xrt_http_cache_node* pSelected = NULL;
	xhttpcacherecord* pRecord = NULL;

	if ( (pCache == NULL) ||
		!__xrtRangeValid(ppRecord, sizeof(pRecord)) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_CACHE_LOOKUP_ERROR;
	}
	if ( !__xrtRangeValid(pKey, sizeof(Key)) ) {
		memcpy(ppRecord, &pRecord, sizeof(pRecord));
		__xrtErrorSetInvalidArgument();
		return XHTTP_CACHE_LOOKUP_ERROR;
	}
	if ( __xrtRangesOverlap(
		pKey, sizeof(Key), ppRecord, sizeof(pRecord)
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_CACHE_LOOKUP_ERROR;
	}
	if ( !__xrtHttpCacheKeyResolve(pKey, &Key) ) {
		memcpy(ppRecord, &pRecord, sizeof(pRecord));
		return XHTTP_CACHE_LOOKUP_ERROR;
	}
	if ( __xrtHttpCacheKeyOverlap(
		&Key, ppRecord, sizeof(pRecord)
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_CACHE_LOOKUP_ERROR;
	}
	memcpy(ppRecord, &pRecord, sizeof(pRecord));
	if ( pCache->Custom ) {
		xhttpcachelookup Result = pCache->Ops.Get(
			pCache->Context,
			&Key,
			&pRecord
		);

		if ( (Result < XHTTP_CACHE_LOOKUP_ERROR) ||
			(Result > XHTTP_CACHE_LOOKUP_HIT) ||
			((Result == XHTTP_CACHE_LOOKUP_HIT) &&
			 (pRecord == NULL)) ||
			((Result == XHTTP_CACHE_LOOKUP_HIT) &&
			 !__xrtHttpCacheRecordMatchesValid(
				pRecord,
				&Key
			 )) ||
			((Result != XHTTP_CACHE_LOOKUP_HIT) &&
			 (pRecord != NULL)) ) {
			xrtHttpCacheRecordRelease(pRecord);
			pRecord = NULL;
			memcpy(ppRecord, &pRecord, sizeof(pRecord));
			__xrtErrorSetInternal();
			return XHTTP_CACHE_LOOKUP_ERROR;
		}
		memcpy(ppRecord, &pRecord, sizeof(pRecord));
		return Result;
	}
	if ( !xrtMutexLock(&pCache->Lock) ) {
		return XHTTP_CACHE_LOOKUP_ERROR;
	}
	__xrtHttpCacheCounterAdd(&pCache->Stats.Lookups, 1);
	{
		xrt_http_cache_node** ppHead =
			__xrtHttpCacheBucket(pCache, Key.URI);

		pNode = ppHead != NULL ? *ppHead : NULL;
	}
	while ( pNode != NULL ) {
		if ( __xrtHttpCacheRecordMatchesValid(
			pNode->Record, &Key
		) && __xrtHttpCacheNewer(
			pNode, pSelected
		) ) {
			pSelected = pNode;
		}
		pNode = pNode->BucketNext;
	}
	if ( pSelected == NULL ) {
		__xrtHttpCacheCounterAdd(
			&pCache->Stats.Misses, 1
		);
		(void)xrtMutexUnlock(&pCache->Lock);
		return XHTTP_CACHE_LOOKUP_MISS;
	}
	pRecord = xrtHttpCacheRecordRetain(pSelected->Record);
	if ( pRecord == NULL ) {
		(void)xrtMutexUnlock(&pCache->Lock);
		return XHTTP_CACHE_LOOKUP_ERROR;
	}
	__xrtHttpCacheLRUTouch(pCache, pSelected);
	__xrtHttpCacheCounterAdd(&pCache->Stats.Hits, 1);
	(void)xrtMutexUnlock(&pCache->Lock);
	memcpy(ppRecord, &pRecord, sizeof(pRecord));
	return XHTTP_CACHE_LOOKUP_HIT;
}



/* 保存首次出现的 Vary 变体。 */
static xhttpcacheput __xrtHttpCacheNodeInsert(
	xhttpcache* pCache,
	xhttpcacherecord* pRecord,
	xrt_http_cache_node** ppRetired
)
{
	const xhttpcachekey* pKey =
		xrtHttpCacheRecordKey(pRecord);
	xrt_http_cache_node* pNode;
	xrt_http_cache_node** ppHead;
	xhttpcacherecord* pHeld;
	bool bNewKey = false;

	pNode = (xrt_http_cache_node*)xrtCalloc(
		1, sizeof(*pNode)
	);
	if ( pNode == NULL ) {
		return XHTTP_CACHE_PUT_ERROR;
	}
	pHeld = xrtHttpCacheRecordRetain(pRecord);
	if ( pHeld == NULL ) {
		xrtFree(pNode);
		return XHTTP_CACHE_PUT_ERROR;
	}
	ppHead = (xrt_http_cache_node**)xrtMapGetOrAdd(
		&pCache->Index,
		__xrtHttpCacheMapKey(pKey->URI),
		&bNewKey
	);
	if ( ppHead == NULL ) {
		xrtHttpCacheRecordRelease(pHeld);
		xrtFree(pNode);
		return XHTTP_CACHE_PUT_ERROR;
	}
	(void)bNewKey;
	pNode->Record = pHeld;
	pNode->Sequence = __xrtHttpCacheSequence(pCache);
	pNode->BucketNext = *ppHead;
	*ppHead = pNode;
	__xrtHttpCacheLRUFirst(pCache, pNode);
	pCache->Stats.Entries++;
	pCache->Stats.Bytes += xrtHttpCacheRecordCharge(pHeld);
	__xrtHttpCacheCounterAdd(&pCache->Stats.Stores, 1);
	__xrtHttpCacheLimit(pCache, ppRetired);
	return XHTTP_CACHE_PUT_STORED;
}



/* 用已经持有的 Record 无失败地替换一个定位完成的 Node。 */
static void __xrtHttpCacheNodeReplaceHeld(
	xhttpcache* pCache,
	xrt_http_cache_node** ppHead,
	xrt_http_cache_node* pNode,
	xrt_http_cache_node* pPrevious,
	xhttpcacherecord* pHeld,
	xhttpcacherecord** ppOld
)
{
	size_t iOld;
	size_t iNew;

	iOld = xrtHttpCacheRecordCharge(pNode->Record);
	iNew = xrtHttpCacheRecordCharge(pHeld);
	*ppOld = pNode->Record;
	pNode->Record = pHeld;
	pNode->Sequence = __xrtHttpCacheSequence(pCache);
	pCache->Stats.Bytes = pCache->Stats.Bytes - iOld + iNew;
	__xrtHttpCacheBucketFirst(
		ppHead, pNode, pPrevious
	);
	__xrtHttpCacheLRUTouch(pCache, pNode);
	__xrtHttpCacheCounterAdd(&pCache->Stats.Stores, 1);
	__xrtHttpCacheCounterAdd(
		&pCache->Stats.Replacements, 1
	);
}



/* 持有新 Record 后替换一个已经定位的 Node。 */
static xhttpcacheput __xrtHttpCacheNodeReplace(
	xhttpcache* pCache,
	xrt_http_cache_node** ppHead,
	xrt_http_cache_node* pNode,
	xrt_http_cache_node* pPrevious,
	xhttpcacherecord* pRecord,
	xhttpcacherecord** ppOld
)
{
	xhttpcacherecord* pHeld =
		xrtHttpCacheRecordRetain(pRecord);

	if ( pHeld == NULL ) {
		return XHTTP_CACHE_PUT_ERROR;
	}
	__xrtHttpCacheNodeReplaceHeld(
		pCache,
		ppHead,
		pNode,
		pPrevious,
		pHeld,
		ppOld
	);
	return XHTTP_CACHE_PUT_REPLACED;
}



/* 保存 Record 的独立引用，并在提交后锁外回收旧内容。 */
XRT_API xhttpcacheput xrtHttpCachePut(
	xhttpcache* pCache,
	xhttpcacherecord* pRecord
)
{
	const xhttpcachekey* pKey;
	xrt_http_cache_node** ppHead;
	xrt_http_cache_node* pNode;
	xrt_http_cache_node* pPrevious;
	xrt_http_cache_node* pRetired = NULL;
	xhttpcacherecord* pOld = NULL;
	xhttpcacheput Result;
	xrt_http_cache_fit Fit;
	size_t iCharge;

	if ( (pCache == NULL) || (pRecord == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_CACHE_PUT_ERROR;
	}
	if ( pCache->Custom ) {
		xhttpcacheput Result = pCache->Ops.Put(
			pCache->Context,
			pRecord
		);

		if ( (Result != XHTTP_CACHE_PUT_ERROR) &&
			(Result != XHTTP_CACHE_PUT_REJECTED) &&
			(Result != XHTTP_CACHE_PUT_STORED) &&
			(Result != XHTTP_CACHE_PUT_REPLACED) ) {
			__xrtErrorSetInternal();
			return XHTTP_CACHE_PUT_ERROR;
		}
		return Result;
	}
	pKey = xrtHttpCacheRecordKey(pRecord);
	iCharge = xrtHttpCacheRecordCharge(pRecord);
	if ( !xrtMutexLock(&pCache->Lock) ) {
		return XHTTP_CACHE_PUT_ERROR;
	}
	if ( (iCharge > pCache->Config.MaxEntryBytes) ||
		(iCharge > pCache->Config.MaxBytes) ) {
		__xrtHttpCacheCounterAdd(
			&pCache->Stats.Rejected, 1
		);
		(void)xrtMutexUnlock(&pCache->Lock);
		return XHTTP_CACHE_PUT_REJECTED;
	}
	ppHead = __xrtHttpCacheBucket(pCache, pKey->URI);
	pNode = __xrtHttpCacheVariant(
		ppHead != NULL ? *ppHead : NULL,
		pRecord,
		&pPrevious
	);
	Fit = __xrtHttpCacheStateFits(
		pCache,
		pNode != NULL ? 1u : 0u,
		pNode != NULL ?
			xrtHttpCacheRecordCharge(pNode->Record) :
			0,
		1u,
		iCharge
	);
	if ( Fit != XRT_HTTP_CACHE_FIT_READY ) {
		if ( Fit == XRT_HTTP_CACHE_FIT_ERROR ) {
			(void)xrtMutexUnlock(&pCache->Lock);
			return XHTTP_CACHE_PUT_ERROR;
		}
		__xrtHttpCacheCounterAdd(
			&pCache->Stats.Rejected, 1
		);
		(void)xrtMutexUnlock(&pCache->Lock);
		return XHTTP_CACHE_PUT_REJECTED;
	}
	if ( pNode != NULL ) {
		Result = __xrtHttpCacheNodeReplace(
			pCache,
			ppHead,
			pNode,
			pPrevious,
			pRecord,
			&pOld
		);
		if ( Result == XHTTP_CACHE_PUT_REPLACED ) {
			__xrtHttpCacheLimit(
				pCache,
				&pRetired
			);
		}
	} else {
		Result = __xrtHttpCacheNodeInsert(
			pCache, pRecord, &pRetired
		);
	}
	(void)xrtMutexUnlock(&pCache->Lock);
	xrtHttpCacheRecordRelease(pOld);
	__xrtHttpCacheRetiredRelease(pRetired);
	return Result;
}



/* 仅在相同 Vary 变体不存在时保存 Record。 */
XRT_API xhttpcacheput xrtHttpCacheInsert(
	xhttpcache* pCache,
	xhttpcacherecord* pRecord
)
{
	const xhttpcachekey* pKey;
	xrt_http_cache_node** ppHead;
	xrt_http_cache_node* pNode;
	xrt_http_cache_node* pRetired = NULL;
	xhttpcacheput Result;
	xrt_http_cache_fit Fit;
	size_t iCharge;

	if ( (pCache == NULL) || (pRecord == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_CACHE_PUT_ERROR;
	}
	if ( pCache->Custom ) {
		Result = pCache->Ops.Insert(
			pCache->Context,
			pRecord
		);
		if ( (Result != XHTTP_CACHE_PUT_ERROR) &&
			(Result != XHTTP_CACHE_PUT_CONFLICT) &&
			(Result != XHTTP_CACHE_PUT_REJECTED) &&
			(Result != XHTTP_CACHE_PUT_STORED) ) {
			__xrtErrorSetInternal();
			return XHTTP_CACHE_PUT_ERROR;
		}
		return Result;
	}
	pKey = xrtHttpCacheRecordKey(pRecord);
	iCharge = xrtHttpCacheRecordCharge(pRecord);
	if ( !xrtMutexLock(&pCache->Lock) ) {
		return XHTTP_CACHE_PUT_ERROR;
	}
	if ( (iCharge > pCache->Config.MaxEntryBytes) ||
		(iCharge > pCache->Config.MaxBytes) ) {
		__xrtHttpCacheCounterAdd(
			&pCache->Stats.Rejected, 1
		);
		(void)xrtMutexUnlock(&pCache->Lock);
		return XHTTP_CACHE_PUT_REJECTED;
	}
	ppHead = __xrtHttpCacheBucket(pCache, pKey->URI);
	pNode = __xrtHttpCacheVariant(
		ppHead != NULL ? *ppHead : NULL,
		pRecord,
		NULL
	);
	if ( pNode != NULL ) {
		__xrtHttpCacheCounterAdd(
			&pCache->Stats.Conflicts, 1
		);
		(void)xrtMutexUnlock(&pCache->Lock);
		return XHTTP_CACHE_PUT_CONFLICT;
	}
	Fit = __xrtHttpCacheStateFits(
		pCache,
		0,
		0,
		1u,
		iCharge
	);
	if ( Fit != XRT_HTTP_CACHE_FIT_READY ) {
		if ( Fit == XRT_HTTP_CACHE_FIT_ERROR ) {
			(void)xrtMutexUnlock(&pCache->Lock);
			return XHTTP_CACHE_PUT_ERROR;
		}
		__xrtHttpCacheCounterAdd(
			&pCache->Stats.Rejected, 1
		);
		(void)xrtMutexUnlock(&pCache->Lock);
		return XHTTP_CACHE_PUT_REJECTED;
	}
	Result = __xrtHttpCacheNodeInsert(
		pCache,
		pRecord,
		&pRetired
	);
	(void)xrtMutexUnlock(&pCache->Lock);
	__xrtHttpCacheRetiredRelease(pRetired);
	return Result;
}



/* 判断两个 Record 是否属于同一方法、URI 和 Partition。 */
static bool __xrtHttpCacheRecordBaseEqual(
	const xhttpcacherecord* pLeft,
	const xhttpcacherecord* pRight
)
{
	const xhttpcachekey* pLeftKey =
		xrtHttpCacheRecordKey(pLeft);
	const xhttpcachekey* pRightKey =
		xrtHttpCacheRecordKey(pRight);

	return (pLeftKey != NULL) &&
		(pRightKey != NULL) &&
		__xrtHttpCacheStoreViewEqual(
			pLeftKey->Method,
			pRightKey->Method
		) &&
		__xrtHttpCacheStoreViewEqual(
			pLeftKey->URI,
			pRightKey->URI
		) &&
		__xrtHttpCacheStoreViewEqual(
			pLeftKey->Partition,
			pRightKey->Partition
		);
}



/* 在 URI Bucket 中按不可变 Record 身份定位当前 Node。 */
static xrt_http_cache_node* __xrtHttpCacheRecordNode(
	xrt_http_cache_node* pNode,
	const xhttpcacherecord* pRecord,
	xrt_http_cache_node** ppPrevious
)
{
	xrt_http_cache_node* pPrevious = NULL;

	while ( pNode != NULL ) {
		if ( pNode->Record == pRecord ) {
			if ( ppPrevious != NULL ) {
				*ppPrevious = pPrevious;
			}
			return pNode;
		}
		pPrevious = pNode;
		pNode = pNode->BucketNext;
	}
	if ( ppPrevious != NULL ) {
		*ppPrevious = NULL;
	}
	return NULL;
}



/*
	仅在 Expected 仍是当前 Record 时提交 Replacement。
	若 Vary 变化后撞上另一变体，保留新响应并在同一锁内淘汰旧的重复 Node。
*/
XRT_API xhttpcacheput xrtHttpCacheReplace(
	xhttpcache* pCache,
	const xhttpcacherecord* pExpected,
	xhttpcacherecord* pReplacement
)
{
	const xhttpcachekey* pKey;
	xrt_http_cache_node** ppHead;
	xrt_http_cache_node* pNode;
	xrt_http_cache_node* pPrevious;
	xrt_http_cache_node* pDuplicate;
	xrt_http_cache_node* pRetired = NULL;
	xhttpcacherecord* pOld = NULL;
	xhttpcacherecord* pHeld = NULL;
	xhttpcacheput Result;
	xrt_http_cache_fit Fit;
	size_t iCharge;
	size_t iRemoveBytes;
	size_t iRemoveEntries;

	if ( (pCache == NULL) || (pExpected == NULL) ||
		(pReplacement == NULL) ||
		!__xrtHttpCacheRecordBaseEqual(
			pExpected,
			pReplacement
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_CACHE_PUT_ERROR;
	}
	if ( pCache->Custom ) {
		Result = pCache->Ops.Replace(
			pCache->Context,
			pExpected,
			pReplacement
		);
		if ( (Result != XHTTP_CACHE_PUT_ERROR) &&
			(Result != XHTTP_CACHE_PUT_CONFLICT) &&
			(Result != XHTTP_CACHE_PUT_REJECTED) &&
			(Result != XHTTP_CACHE_PUT_REPLACED) ) {
			__xrtErrorSetInternal();
			return XHTTP_CACHE_PUT_ERROR;
		}
		return Result;
	}
	pKey = xrtHttpCacheRecordKey(pExpected);
	iCharge = xrtHttpCacheRecordCharge(pReplacement);
	if ( !xrtMutexLock(&pCache->Lock) ) {
		return XHTTP_CACHE_PUT_ERROR;
	}
	if ( (iCharge > pCache->Config.MaxEntryBytes) ||
		(iCharge > pCache->Config.MaxBytes) ) {
		__xrtHttpCacheCounterAdd(
			&pCache->Stats.Rejected, 1
		);
		(void)xrtMutexUnlock(&pCache->Lock);
		return XHTTP_CACHE_PUT_REJECTED;
	}
	ppHead = __xrtHttpCacheBucket(pCache, pKey->URI);
	pNode = __xrtHttpCacheRecordNode(
		ppHead != NULL ? *ppHead : NULL,
		pExpected,
		&pPrevious
	);
	if ( pNode == NULL ) {
		__xrtHttpCacheCounterAdd(
			&pCache->Stats.Conflicts, 1
		);
		(void)xrtMutexUnlock(&pCache->Lock);
		return XHTTP_CACHE_PUT_CONFLICT;
	}

	pDuplicate = __xrtHttpCacheVariant(
		ppHead != NULL ? *ppHead : NULL,
		pReplacement,
		NULL
	);
	if ( pDuplicate == pNode ) {
		pDuplicate = NULL;
	}
	iRemoveEntries = 1u;
	iRemoveBytes = xrtHttpCacheRecordCharge(
		pNode->Record
	);
	if ( pDuplicate != NULL ) {
		size_t iDuplicateCharge =
			xrtHttpCacheRecordCharge(
				pDuplicate->Record
			);

		if ( iDuplicateCharge >
			(pCache->Stats.Bytes - iRemoveBytes) ) {
			__xrtErrorSetInternal();
			(void)xrtMutexUnlock(&pCache->Lock);
			return XHTTP_CACHE_PUT_ERROR;
		}
		iRemoveEntries++;
		iRemoveBytes += iDuplicateCharge;
	}
	Fit = __xrtHttpCacheStateFits(
		pCache,
		iRemoveEntries,
		iRemoveBytes,
		1u,
		iCharge
	);
	if ( Fit != XRT_HTTP_CACHE_FIT_READY ) {
		if ( Fit == XRT_HTTP_CACHE_FIT_ERROR ) {
			(void)xrtMutexUnlock(&pCache->Lock);
			return XHTTP_CACHE_PUT_ERROR;
		}
		__xrtHttpCacheCounterAdd(
			&pCache->Stats.Rejected, 1
		);
		(void)xrtMutexUnlock(&pCache->Lock);
		return XHTTP_CACHE_PUT_REJECTED;
	}
	pHeld = xrtHttpCacheRecordRetain(pReplacement);
	if ( pHeld == NULL ) {
		(void)xrtMutexUnlock(&pCache->Lock);
		return XHTTP_CACHE_PUT_ERROR;
	}
	if ( (pDuplicate != NULL) &&
		!__xrtHttpCacheNodeRemove(
			pCache,
			pDuplicate,
			&pRetired
		) ) {
		xrtHttpCacheRecordRelease(pHeld);
		__xrtErrorSetInternal();
		(void)xrtMutexUnlock(&pCache->Lock);
		return XHTTP_CACHE_PUT_ERROR;
	}
	if ( pDuplicate != NULL ) {
		pNode = __xrtHttpCacheRecordNode(
			ppHead != NULL ? *ppHead : NULL,
			pExpected,
			&pPrevious
		);
		if ( pNode == NULL ) {
			xrtHttpCacheRecordRelease(pHeld);
			__xrtErrorSetInternal();
			Result = XHTTP_CACHE_PUT_ERROR;
			goto finish;
		}
	}
	__xrtHttpCacheNodeReplaceHeld(
		pCache,
		ppHead,
		pNode,
		pPrevious,
		pHeld,
		&pOld
	);
	Result = XHTTP_CACHE_PUT_REPLACED;
	__xrtHttpCacheLimit(
		pCache,
		&pRetired
	);

finish:
	(void)xrtMutexUnlock(&pCache->Lock);
	xrtHttpCacheRecordRelease(pOld);
	__xrtHttpCacheRetiredRelease(pRetired);
	return Result;
}



/* 仅在 Expected 仍是当前 Record 时删除对应 Node。 */
XRT_API xhttpcachechange xrtHttpCacheRemoveRecord(
	xhttpcache* pCache,
	const xhttpcacherecord* pExpected
)
{
	const xhttpcachekey* pKey;
	xrt_http_cache_node** ppHead;
	xrt_http_cache_node* pNode;
	xrt_http_cache_node* pRetired = NULL;
	xhttpcachechange Result;

	if ( (pCache == NULL) || (pExpected == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_CACHE_CHANGE_ERROR;
	}
	if ( pCache->Custom ) {
		Result = pCache->Ops.RemoveRecord(
			pCache->Context,
			pExpected
		);
		if ( (Result < XHTTP_CACHE_CHANGE_ERROR) ||
			(Result > XHTTP_CACHE_CHANGE_APPLIED) ) {
			__xrtErrorSetInternal();
			return XHTTP_CACHE_CHANGE_ERROR;
		}
		return Result;
	}
	pKey = xrtHttpCacheRecordKey(pExpected);
	if ( !xrtMutexLock(&pCache->Lock) ) {
		return XHTTP_CACHE_CHANGE_ERROR;
	}
	ppHead = __xrtHttpCacheBucket(pCache, pKey->URI);
	pNode = __xrtHttpCacheRecordNode(
		ppHead != NULL ? *ppHead : NULL,
		pExpected,
		NULL
	);
	if ( pNode == NULL ) {
		__xrtHttpCacheCounterAdd(
			&pCache->Stats.Conflicts, 1
		);
		Result = XHTTP_CACHE_CHANGE_CONFLICT;
	} else if ( !__xrtHttpCacheNodeRemove(
		pCache,
		pNode,
		&pRetired
	) ) {
		__xrtErrorSetInternal();
		Result = XHTTP_CACHE_CHANGE_ERROR;
	} else {
		__xrtHttpCacheCounterAdd(
			&pCache->Stats.Removals, 1
		);
		Result = XHTTP_CACHE_CHANGE_APPLIED;
	}
	(void)xrtMutexUnlock(&pCache->Lock);
	__xrtHttpCacheRetiredRelease(pRetired);
	return Result;
}



/* 删除与完整 Key 和 Vary 选择匹配的全部记录。 */
XRT_API bool xrtHttpCacheRemove(
	xhttpcache* pCache,
	const xhttpcachekey* pKey,
	size_t* pRemoved
)
{
	xhttpcachekey Key;
	xrt_http_cache_node* pRetired = NULL;
	xrt_http_cache_node** ppHead;
	xrt_http_cache_node** ppNode;
	size_t iRemoved = 0;

	if ( (pRemoved != NULL) &&
		!__xrtRangeValid(pRemoved, sizeof(iRemoved)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pCache == NULL ) {
		if ( pRemoved != NULL ) {
			memcpy(pRemoved, &iRemoved, sizeof(iRemoved));
		}
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtRangeValid(pKey, sizeof(Key)) ) {
		if ( pRemoved != NULL ) {
			memcpy(pRemoved, &iRemoved, sizeof(iRemoved));
		}
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pRemoved != NULL) && __xrtRangesOverlap(
		pKey, sizeof(Key), pRemoved, sizeof(iRemoved)
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpCacheKeyResolve(pKey, &Key) ) {
		if ( pRemoved != NULL ) {
			memcpy(pRemoved, &iRemoved, sizeof(iRemoved));
		}
		return false;
	}
	if ( (pRemoved != NULL) && __xrtHttpCacheKeyOverlap(
		&Key, pRemoved, sizeof(iRemoved)
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pRemoved != NULL ) {
		memcpy(pRemoved, &iRemoved, sizeof(iRemoved));
	}
	if ( pCache->Custom ) {
		if ( !pCache->Ops.Remove(
			pCache->Context,
			&Key,
			&iRemoved
		) ) {
			return false;
		}
		if ( pRemoved != NULL ) {
			memcpy(pRemoved, &iRemoved, sizeof(iRemoved));
		}
		return true;
	}
	if ( !xrtMutexLock(&pCache->Lock) ) {
		return false;
	}
	ppHead = __xrtHttpCacheBucket(pCache, Key.URI);
	ppNode = ppHead;
	while ( (ppNode != NULL) && (*ppNode != NULL) ) {
		xrt_http_cache_node* pNode = *ppNode;

		if ( !__xrtHttpCacheRecordMatchesValid(
			pNode->Record, &Key
		) ) {
			ppNode = &pNode->BucketNext;
			continue;
		}
		*ppNode = pNode->BucketNext;
		pNode->BucketNext = NULL;
		__xrtHttpCacheLRURemove(pCache, pNode);
		pCache->Stats.Entries--;
		pCache->Stats.Bytes -=
			xrtHttpCacheRecordCharge(pNode->Record);
		__xrtHttpCacheRetire(&pRetired, pNode);
		iRemoved++;
	}
	if ( (ppHead != NULL) && (*ppHead == NULL) ) {
		(void)xrtMapRemove(
			&pCache->Index,
			__xrtHttpCacheMapKey(Key.URI)
		);
	}
	__xrtHttpCacheCounterAdd(
		&pCache->Stats.Removals, (uint64)iRemoved
	);
	(void)xrtMutexUnlock(&pCache->Lock);
	__xrtHttpCacheRetiredRelease(pRetired);
	if ( pRemoved != NULL ) {
		memcpy(pRemoved, &iRemoved, sizeof(iRemoved));
	}
	return true;
}



/* 删除 URI 和 Partition 下的全部方法与 Vary 变体。 */
XRT_API bool xrtHttpCacheRemoveURI(
	xhttpcache* pCache,
	xstrview URI,
	xstrview Partition,
	size_t* pRemoved
)
{
	xrt_http_cache_node* pRetired = NULL;
	xrt_http_cache_node** ppHead;
	xrt_http_cache_node** ppNode;
	size_t iRemoved = 0;
	xhttpcachekey Key;
	xhttpcachekey Resolved;

	if ( (pRemoved != NULL) &&
		!__xrtRangeValid(pRemoved, sizeof(iRemoved)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&Key, 0, sizeof(Key));
	Key.Method = XRT_STR_LITERAL("GET");
	Key.URI = URI;
	Key.Partition = Partition;
	if ( pCache == NULL ) {
		if ( pRemoved != NULL ) {
			memcpy(pRemoved, &iRemoved, sizeof(iRemoved));
		}
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpCacheKeyResolve(&Key, &Resolved) ) {
		if ( pRemoved != NULL ) {
			memcpy(pRemoved, &iRemoved, sizeof(iRemoved));
		}
		return false;
	}
	if ( (pRemoved != NULL) && __xrtHttpCacheKeyOverlap(
		&Resolved, pRemoved, sizeof(iRemoved)
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pRemoved != NULL ) {
		memcpy(pRemoved, &iRemoved, sizeof(iRemoved));
	}
	if ( pCache->Custom ) {
		if ( !pCache->Ops.RemoveURI(
			pCache->Context,
			URI,
			Partition,
			&iRemoved
		) ) {
			return false;
		}
		if ( pRemoved != NULL ) {
			memcpy(pRemoved, &iRemoved, sizeof(iRemoved));
		}
		return true;
	}
	if ( !xrtMutexLock(&pCache->Lock) ) {
		return false;
	}
	ppHead = __xrtHttpCacheBucket(pCache, URI);
	ppNode = ppHead;
	while ( (ppNode != NULL) && (*ppNode != NULL) ) {
		xrt_http_cache_node* pNode = *ppNode;
		const xhttpcachekey* pStored =
			xrtHttpCacheRecordKey(pNode->Record);

		if ( !__xrtHttpCacheStoreViewEqual(
			pStored->Partition, Partition
		) ) {
			ppNode = &pNode->BucketNext;
			continue;
		}
		*ppNode = pNode->BucketNext;
		pNode->BucketNext = NULL;
		__xrtHttpCacheLRURemove(pCache, pNode);
		pCache->Stats.Entries--;
		pCache->Stats.Bytes -=
			xrtHttpCacheRecordCharge(pNode->Record);
		__xrtHttpCacheRetire(&pRetired, pNode);
		iRemoved++;
	}
	if ( (ppHead != NULL) && (*ppHead == NULL) ) {
		(void)xrtMapRemove(
			&pCache->Index,
			__xrtHttpCacheMapKey(URI)
		);
	}
	__xrtHttpCacheCounterAdd(
		&pCache->Stats.Removals, (uint64)iRemoved
	);
	(void)xrtMutexUnlock(&pCache->Lock);
	__xrtHttpCacheRetiredRelease(pRetired);
	if ( pRemoved != NULL ) {
		memcpy(pRemoved, &iRemoved, sizeof(iRemoved));
	}
	return true;
}



/* 删除全部记录并保留 Map 桶容量。 */
XRT_API bool xrtHttpCacheClear(xhttpcache* pCache)
{
	xrt_http_cache_node* pNodes;
	size_t iRemoved;

	if ( pCache == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pCache->Custom ) {
		return pCache->Ops.Clear(pCache->Context);
	}
	if ( !xrtMutexLock(&pCache->Lock) ) {
		return false;
	}
	pNodes = pCache->LRUHead;
	iRemoved = pCache->Stats.Entries;
	pCache->LRUHead = NULL;
	pCache->LRUTail = NULL;
	xrtMapClear(&pCache->Index);
	pCache->Stats.Entries = 0;
	pCache->Stats.Bytes = 0;
	__xrtHttpCacheCounterAdd(
		&pCache->Stats.Removals, (uint64)iRemoved
	);
	(void)xrtMutexUnlock(&pCache->Lock);
	__xrtHttpCacheLRURelease(pNodes);
	return true;
}



/* 取得条目、字节和累计行为的一致快照。 */
XRT_API bool xrtHttpCacheStats(
	xhttpcache* pCache,
	xhttpcachestats* pStats
)
{
	xhttpcachestats Stats = { 0 };

	if ( (pCache == NULL) ||
		!__xrtRangeValid(pStats, sizeof(Stats)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pStats, &Stats, sizeof(Stats));
	if ( pCache->Custom ) {
		if ( !pCache->Ops.Stats(
			pCache->Context,
			&Stats
		) ) {
			memset(&Stats, 0, sizeof(Stats));
			memcpy(pStats, &Stats, sizeof(Stats));
			return false;
		}
		memcpy(pStats, &Stats, sizeof(Stats));
		return true;
	}
	if ( !xrtMutexLock(&pCache->Lock) ) {
		return false;
	}
	Stats = pCache->Stats;
	(void)xrtMutexUnlock(&pCache->Lock);
	memcpy(pStats, &Stats, sizeof(Stats));
	return true;
}

#endif
