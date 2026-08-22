#include "../internal/xrt_http_cors_client.h"
#include "../internal/xrt_http_origin.h"

#include <xrt/hash.h>
#include <xrt/http_cors_cache.h>
#include <xrt/map.h>
#include <xrt/sync.h>
#include <xrt/time.h>



#if defined(XRT_FEATURE_HTTP_CORS_CACHE)

/* 每个 Node 对应 Fetch 预检缓存中的一个方法或字段权限。 */
typedef enum xrt_http_cors_cache_kind {
	XRT_HTTP_CORS_CACHE_METHOD = 1,
	XRT_HTTP_CORS_CACHE_HEADER = 2
} xrt_http_cors_cache_kind;



/* 删除原因分别进入到期、淘汰和主动删除统计。 */
typedef enum xrt_http_cors_cache_remove {
	XRT_HTTP_CORS_CACHE_REMOVE_EXPIRED = 0,
	XRT_HTTP_CORS_CACHE_REMOVE_EVICTED,
	XRT_HTTP_CORS_CACHE_REMOVE_EXPLICIT
} xrt_http_cors_cache_remove;



typedef struct xrt_http_cors_cache_node {
	struct xrt_http_cors_cache_node* BucketPrevious;
	struct xrt_http_cors_cache_node* BucketNext;
	struct xrt_http_cors_cache_node* LRUPrevious;
	struct xrt_http_cors_cache_node* LRUNext;
	struct xrt_http_cors_cache_node* WorkNext;
	xhttporigin Origin;
	xstrview URL;
	xstrview Partition;
	xstrview Name;
	uint64 Hash;
	uint64 Expires;
	uint8 Kind;
	bool Credentials;
} xrt_http_cors_cache_node;



/* Cache 的索引、LRU、容量和统计由同一 Mutex 保护。 */
struct xhttpcorscache {
	volatile int32 References;
	xmutex Lock;
	xmap Index;
	xhttpcorscacheconfig Config;
	xrt_http_cors_cache_node* LRUHead;
	xrt_http_cors_cache_node* LRUTail;
	xhttpcorscachestats Stats;
	bool LockReady;
	bool IndexReady;
};



/* 单调统计达到上限后保持饱和。 */
static void __xrtHttpCorsCacheCounterAdd(
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



/* 普通缓存键文本按字节精确比较。 */
static bool __xrtHttpCorsCacheViewSame(
	xstrview Left,
	xstrview Right
)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 判断缓存键中的两个 Origin 是否属于同一三元组。 */
static bool __xrtHttpCorsCacheOriginSame(
	const xhttporigin* pLeft,
	const xhttporigin* pRight
)
{
	bool bLeftNull = (pLeft->Flags &
		XHTTP_ORIGIN_NULL) != 0;
	bool bRightNull = (pRight->Flags &
		XHTTP_ORIGIN_NULL) != 0;

	if ( bLeftNull || bRightNull ) {
		return bLeftNull && bRightNull;
	}
	return __xrtHttpOriginTupleSame(pLeft, pRight);
}



/* 把大小写不敏感的视图规范为小写后加入一级哈希。 */
static uint64 __xrtHttpCorsCacheHashCaseView(
	uint64 iHash,
	xstrview View
)
{
	size_t i;

	iHash = xrtHash64Seed(
		&View.Size, sizeof(View.Size), iHash
	);
	for ( i = 0; i < View.Size; i++ ) {
		uint8 iByte = __xrtHttpAsciiLower(
			(uint8)View.Data[i]
		);

		iHash = xrtHash64Seed(&iByte, 1u, iHash);
	}
	return iHash;
}



/* 取得与 Origin 三元组比较规则一致的有效端口。 */
static uint16 __xrtHttpCorsCacheOriginPort(
	const xhttporigin* pOrigin
)
{
	bool bExplicit = (pOrigin->Url.Flags & (
		XURL_HAS_PORT | XURL_PORT_EMPTY
	)) == XURL_HAS_PORT;

	return bExplicit ? pOrigin->Url.Port :
		xrtUrlDefaultPort(pOrigin->Url.Scheme);
}



/* 从可未对齐描述符读取并验证缓存键。 */
static bool __xrtHttpCorsCacheKeyResolve(
	const xhttpcorscachekey* pInput,
	xhttpcorscachekey* pKey
)
{
	if ( !__xrtRangeValid(pInput, sizeof(*pKey)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pKey, pInput, sizeof(*pKey));
	if ( !__xrtHttpOriginValueValid(&pKey->Origin) ||
		!__xrtHttpViewValid(pKey->URL) ||
		!__xrtHttpViewValid(pKey->Partition) ||
		(pKey->URL.Size == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 判断一段输出是否覆盖缓存键借用的任何内容。 */
static bool __xrtHttpCorsCacheKeyOverlap(
	const xhttpcorscachekey* pKey,
	const void* pOutput,
	size_t iSize
)
{
	return __xrtHttpOriginOverlap(
		&pKey->Origin, pOutput, iSize
	) || __xrtRangesOverlap(
		pKey->URL.Data, pKey->URL.Size,
		pOutput, iSize
	) || __xrtRangesOverlap(
		pKey->Partition.Data, pKey->Partition.Size,
		pOutput, iSize
	);
}



/* 分区、URL 与规范 Origin 共同组成一级桶。 */
static uint64 __xrtHttpCorsCacheHash(
	const xhttpcorscachekey* pKey
)
{
	uint8 iNull = (pKey->Origin.Flags &
		XHTTP_ORIGIN_NULL) != 0;
	uint16 iPort;
	uint64 iHash;

	iHash = xrtHash64(
		&pKey->Partition.Size,
		sizeof(pKey->Partition.Size)
	);
	if ( pKey->Partition.Size != 0 ) {
		iHash = xrtHash64Seed(
			pKey->Partition.Data,
			pKey->Partition.Size,
			iHash
		);
	}
	iHash = xrtHash64Seed(
		&pKey->URL.Size,
		sizeof(pKey->URL.Size),
		iHash
	);
	iHash = xrtHash64Seed(
		pKey->URL.Data, pKey->URL.Size, iHash
	);
	iHash = xrtHash64Seed(&iNull, sizeof(iNull), iHash);
	if ( iNull != 0 ) {
		return iHash;
	}
	iHash = __xrtHttpCorsCacheHashCaseView(
		iHash, pKey->Origin.Url.Scheme
	);
	iHash = __xrtHttpCorsCacheHashCaseView(
		iHash, pKey->Origin.Url.Host
	);
	iPort = __xrtHttpCorsCacheOriginPort(&pKey->Origin);
	return xrtHash64Seed(&iPort, sizeof(iPort), iHash);
}



/* 把 64 位一级哈希转换为 Map 字节键。 */
static xbytesview __xrtHttpCorsCacheMapKey(
	const uint64* pHash
)
{
	return (xbytesview){
		(cbytes)pHash, sizeof(*pHash)
	};
}



/* 返回指定一级哈希的桶头槽。 */
static xrt_http_cors_cache_node** __xrtHttpCorsCacheBucket(
	xhttpcorscache* pCache,
	const uint64* pHash
)
{
	return (xrt_http_cors_cache_node**)xrtMapGet(
		&pCache->Index,
		__xrtHttpCorsCacheMapKey(pHash)
	);
}



/* 比较 Node 与完整分区、Origin 和 URL 键。 */
static bool __xrtHttpCorsCacheNodeKeySame(
	const xrt_http_cors_cache_node* pNode,
	const xhttpcorscachekey* pKey
)
{
	return __xrtHttpCorsCacheViewSame(
		pNode->Partition, pKey->Partition
	) && __xrtHttpCorsCacheViewSame(
		pNode->URL, pKey->URL
	) && __xrtHttpCorsCacheOriginSame(
		&pNode->Origin, &pKey->Origin
	);
}



/* 按方法大小写敏感、字段名大小写不敏感的规则比较权限名。 */
static bool __xrtHttpCorsCacheNameSame(
	uint8 iKind,
	xstrview Left,
	xstrview Right
)
{
	if ( iKind == XRT_HTTP_CORS_CACHE_HEADER ) {
		return xrtHttpFieldNameEqual(Left, Right);
	}
	return xrtHttpMethodEqual(Left, Right);
}



/* 判断权限名是否为单字节通配符。 */
static bool __xrtHttpCorsCacheWildcard(xstrview Name)
{
	return (Name.Size == 1u) && (Name.Data[0] == '*');
}



/* 判断缓存项是否覆盖当前方法或字段名。 */
static bool __xrtHttpCorsCachePermissionMatch(
	const xrt_http_cors_cache_node* pNode,
	uint8 iKind,
	xstrview Name
)
{
	if ( pNode->Kind != iKind ) {
		return false;
	}
	if ( __xrtHttpCorsCacheNameSame(
		iKind, pNode->Name, Name
	) ) {
		return true;
	}
	if ( !__xrtHttpCorsCacheWildcard(pNode->Name) ) {
		return false;
	}
	return (iKind == XRT_HTTP_CORS_CACHE_METHOD) ||
		!xrtHttpCorsRequestHeaderNonWildcard(Name);
}



/* Fetch 凭据匹配允许 true 项覆盖无凭据请求，反向不成立。 */
static bool __xrtHttpCorsCacheCredentialsMatch(
	const xrt_http_cors_cache_node* pNode,
	bool bCredentials
)
{
	return pNode->Credentials || !bCredentials;
}



/* 释放 WorkNext 连接的临时或退休 Node。 */
static void __xrtHttpCorsCacheWorkRelease(
	xrt_http_cors_cache_node* pNode
)
{
	while ( pNode != NULL ) {
		xrt_http_cors_cache_node* pNext =
			pNode->WorkNext;

		xrtFree(pNode);
		pNode = pNext;
	}
}



/* 释放 LRUNext 连接的全部活动 Node。 */
static void __xrtHttpCorsCacheLRURelease(
	xrt_http_cors_cache_node* pNode
)
{
	while ( pNode != NULL ) {
		xrt_http_cors_cache_node* pNext =
			pNode->LRUNext;

		xrtFree(pNode);
		pNode = pNext;
	}
}



/* 为一个权限创建同时拥有键文本与权限名的紧凑 Node。 */
static xrt_http_cors_cache_node* __xrtHttpCorsCacheNodeCreate(
	const xhttpcorscachekey* pKey,
	uint64 iHash,
	uint8 iKind,
	xstrview Name,
	bool bCredentials
)
{
	xrt_http_cors_cache_node* pNode;
	char* pText;
	size_t iSize = sizeof(*pNode);

	if ( !__xrtHttpSizeAdd(&iSize, pKey->Partition.Size) ||
		!__xrtHttpSizeAdd(&iSize, pKey->URL.Size) ||
		!__xrtHttpSizeAdd(
			&iSize, pKey->Origin.Url.Scheme.Size
		) || !__xrtHttpSizeAdd(
			&iSize, pKey->Origin.Url.Host.Size
		) || !__xrtHttpSizeAdd(&iSize, Name.Size) ) {
		return NULL;
	}
	pNode = (xrt_http_cors_cache_node*)xrtCalloc(
		1, iSize
	);
	if ( pNode == NULL ) {
		return NULL;
	}
	pNode->Hash = iHash;
	pNode->Kind = iKind;
	pNode->Credentials = bCredentials;
	pNode->Origin.Flags = pKey->Origin.Flags;
	pNode->Origin.Url.Flags = pKey->Origin.Url.Flags &
		(XURL_HAS_SCHEME | XURL_HAS_AUTHORITY |
		 XURL_HAS_HOST | XURL_HAS_PORT |
		 XURL_PORT_EMPTY | XURL_PORT_VALUE);
	pNode->Origin.Url.Port = pKey->Origin.Url.Port;
	pText = (char*)(pNode + 1);

	if ( pKey->Partition.Size != 0 ) {
		memcpy(
			pText,
			pKey->Partition.Data,
			pKey->Partition.Size
		);
		pNode->Partition = (xstrview){
			pText, pKey->Partition.Size
		};
		pText += pKey->Partition.Size;
	}
	memcpy(pText, pKey->URL.Data, pKey->URL.Size);
	pNode->URL = (xstrview){ pText, pKey->URL.Size };
	pText += pKey->URL.Size;
	if ( pKey->Origin.Url.Scheme.Size != 0 ) {
		memcpy(
			pText,
			pKey->Origin.Url.Scheme.Data,
			pKey->Origin.Url.Scheme.Size
		);
		pNode->Origin.Url.Scheme = (xstrview){
			pText, pKey->Origin.Url.Scheme.Size
		};
		pText += pKey->Origin.Url.Scheme.Size;
	}
	if ( pKey->Origin.Url.Host.Size != 0 ) {
		memcpy(
			pText,
			pKey->Origin.Url.Host.Data,
			pKey->Origin.Url.Host.Size
		);
		pNode->Origin.Url.Host = (xstrview){
			pText, pKey->Origin.Url.Host.Size
		};
		pText += pKey->Origin.Url.Host.Size;
	}
	memcpy(pText, Name.Data, Name.Size);
	pNode->Name = (xstrview){ pText, Name.Size };
	return pNode;
}



/* 判断临时权限列表是否已经包含同类同名项。 */
static bool __xrtHttpCorsCachePreparedHas(
	const xrt_http_cors_cache_node* pNode,
	uint8 iKind,
	xstrview Name
)
{
	while ( pNode != NULL ) {
		if ( (pNode->Kind == iKind) &&
			__xrtHttpCorsCacheNameSame(
				iKind, pNode->Name, Name
			) ) {
			return true;
		}
		pNode = pNode->WorkNext;
	}
	return false;
}



/* 向临时列表增加一个去重后的拥有式权限。 */
static bool __xrtHttpCorsCachePreparedAdd(
	xrt_http_cors_cache_node** ppNodes,
	const xhttpcorscachekey* pKey,
	uint64 iHash,
	uint8 iKind,
	xstrview Name,
	bool bCredentials,
	size_t iLimit,
	size_t* pCount
)
{
	xrt_http_cors_cache_node* pNode;

	if ( __xrtHttpCorsCachePreparedHas(
		*ppNodes, iKind, Name
	) ) {
		return true;
	}
	if ( *pCount >= iLimit ) {
		return true;
	}
	pNode = __xrtHttpCorsCacheNodeCreate(
		pKey, iHash, iKind, Name, bCredentials
	);
	if ( pNode == NULL ) {
		return false;
	}
	pNode->WorkNext = *ppNodes;
	*ppNodes = pNode;
	(*pCount)++;
	return true;
}



/* 完整验证响应列表并预先创建一次原子更新需要的全部权限。 */
static bool __xrtHttpCorsCachePrepare(
	const xhttpcorscachekey* pKey,
	uint64 iHash,
	xstrview Method,
	bool bForce,
	bool bCredentials,
	const xhttpfield* pFields,
	size_t iCount,
	size_t iLimit,
	xrt_http_cors_cache_node** ppNodes
)
{
	xrt_http_cors_cache_node* pNodes = NULL;
	xhttpcorscursor Cursor;
	xstrview Name;
	xhttpnext Next;
	size_t iMethodFields;
	size_t iPrepared = 0;

	xrtHttpCorsCursorInit(&Cursor);
	while ( (Next = xrtHttpCorsAllowMethodNext(
		pFields, iCount, &Cursor, &Name
	)) == XHTTP_NEXT_ITEM ) {
		if ( !__xrtHttpCorsCachePreparedAdd(
			&pNodes,
			pKey,
			iHash,
			XRT_HTTP_CORS_CACHE_METHOD,
			Name,
			bCredentials,
			iLimit,
			&iPrepared
		) ) {
			goto fail;
		}
	}
	if ( Next == XHTTP_NEXT_ERROR ) {
		goto fail;
	}
	iMethodFields = xrtHttpFieldCount(
		pFields,
		iCount,
		XRT_STR_LITERAL("Access-Control-Allow-Methods")
	);
	if ( bForce && (iMethodFields == 0) &&
		!__xrtHttpCorsCachePreparedAdd(
			&pNodes,
			pKey,
			iHash,
			XRT_HTTP_CORS_CACHE_METHOD,
			Method,
			bCredentials,
			iLimit,
			&iPrepared
		) ) {
		goto fail;
	}

	xrtHttpCorsCursorInit(&Cursor);
	while ( (Next = xrtHttpCorsAllowHeaderNext(
		pFields, iCount, &Cursor, &Name
	)) == XHTTP_NEXT_ITEM ) {
		if ( !__xrtHttpCorsCachePreparedAdd(
			&pNodes,
			pKey,
			iHash,
			XRT_HTTP_CORS_CACHE_HEADER,
			Name,
			bCredentials,
			iLimit,
			&iPrepared
		) ) {
			goto fail;
		}
	}
	if ( Next == XHTTP_NEXT_ERROR ) {
		goto fail;
	}
	*ppNodes = pNodes;
	return true;

fail:
	__xrtHttpCorsCacheWorkRelease(pNodes);
	return false;
}



/* 完整验证响应中的方法与字段权限，并统计方法字段行。 */
static bool __xrtHttpCorsCachePermissionsValid(
	const xhttpfield* pFields,
	size_t iCount,
	size_t* pMethodFields
)
{
	xhttpcorscursor Cursor;
	xstrview Name;
	xhttpnext Next;

	xrtHttpCorsCursorInit(&Cursor);
	while ( (Next = xrtHttpCorsAllowMethodNext(
		pFields, iCount, &Cursor, &Name
	)) == XHTTP_NEXT_ITEM ) {
		(void)Name;
	}
	if ( Next == XHTTP_NEXT_ERROR ) {
		return false;
	}

	xrtHttpCorsCursorInit(&Cursor);
	while ( (Next = xrtHttpCorsAllowHeaderNext(
		pFields, iCount, &Cursor, &Name
	)) == XHTTP_NEXT_ITEM ) {
		(void)Name;
	}
	if ( Next == XHTTP_NEXT_ERROR ) {
		return false;
	}
	*pMethodFields = xrtHttpFieldCount(
		pFields,
		iCount,
		XRT_STR_LITERAL("Access-Control-Allow-Methods")
	);
	return true;
}



/* 从 LRU 双向链摘除 Node。 */
static void __xrtHttpCorsCacheLRURemove(
	xhttpcorscache* pCache,
	xrt_http_cors_cache_node* pNode
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
static void __xrtHttpCorsCacheLRUFirst(
	xhttpcorscache* pCache,
	xrt_http_cors_cache_node* pNode
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



/* 把命中的 Node 移到 LRU 最新端。 */
static void __xrtHttpCorsCacheLRUTouch(
	xhttpcorscache* pCache,
	xrt_http_cors_cache_node* pNode
)
{
	if ( pCache->LRUHead == pNode ) {
		return;
	}
	__xrtHttpCorsCacheLRURemove(pCache, pNode);
	__xrtHttpCorsCacheLRUFirst(pCache, pNode);
}



/* 按原因累计一次 Node 删除。 */
static void __xrtHttpCorsCacheRemoveCount(
	xhttpcorscache* pCache,
	xrt_http_cors_cache_remove Reason
)
{
	if ( Reason == XRT_HTTP_CORS_CACHE_REMOVE_EXPIRED ) {
		__xrtHttpCorsCacheCounterAdd(
			&pCache->Stats.Expired, 1u
		);
	} else if ( Reason ==
		XRT_HTTP_CORS_CACHE_REMOVE_EVICTED ) {
		__xrtHttpCorsCacheCounterAdd(
			&pCache->Stats.Evictions, 1u
		);
	} else {
		__xrtHttpCorsCacheCounterAdd(
			&pCache->Stats.Removals, 1u
		);
	}
}



/* 从一级桶和 LRU 同时删除 Node，并移交到锁外释放链。 */
static bool __xrtHttpCorsCacheNodeDetach(
	xhttpcorscache* pCache,
	xrt_http_cors_cache_node* pNode,
	xrt_http_cors_cache_remove Reason,
	xrt_http_cors_cache_node** ppRetired
)
{
	xrt_http_cors_cache_node** ppHead =
		__xrtHttpCorsCacheBucket(pCache, &pNode->Hash);

	if ( ppHead == NULL ) {
		__xrtErrorSetInternal();
		return false;
	}
	if ( pNode->BucketPrevious != NULL ) {
		pNode->BucketPrevious->BucketNext =
			pNode->BucketNext;
	} else if ( *ppHead == pNode ) {
		*ppHead = pNode->BucketNext;
	} else {
		__xrtErrorSetInternal();
		return false;
	}
	if ( pNode->BucketNext != NULL ) {
		pNode->BucketNext->BucketPrevious =
			pNode->BucketPrevious;
	}
	__xrtHttpCorsCacheLRURemove(pCache, pNode);
	if ( pCache->Stats.Entries == 0 ) {
		__xrtErrorSetInternal();
		return false;
	}
	pCache->Stats.Entries--;
	__xrtHttpCorsCacheRemoveCount(pCache, Reason);
	pNode->BucketPrevious = NULL;
	pNode->BucketNext = NULL;
	pNode->WorkNext = *ppRetired;
	*ppRetired = pNode;
	if ( *ppHead == NULL ) {
		(void)xrtMapRemove(
			&pCache->Index,
			__xrtHttpCorsCacheMapKey(&pNode->Hash)
		);
	}
	return true;
}



/* 删除指定一级桶中已经到期的全部 Node。 */
static bool __xrtHttpCorsCacheBucketPurge(
	xhttpcorscache* pCache,
	uint64 iHash,
	uint64 iNow,
	xrt_http_cors_cache_node** ppRetired,
	size_t* pRemoved
)
{
	xrt_http_cors_cache_node** ppHead =
		__xrtHttpCorsCacheBucket(pCache, &iHash);
	xrt_http_cors_cache_node* pNode =
		ppHead != NULL ? *ppHead : NULL;

	while ( pNode != NULL ) {
		xrt_http_cors_cache_node* pNext =
			pNode->BucketNext;

		if ( pNode->Expires <= iNow ) {
			if ( !__xrtHttpCorsCacheNodeDetach(
				pCache,
				pNode,
				XRT_HTTP_CORS_CACHE_REMOVE_EXPIRED,
				ppRetired
			) ) {
				return false;
			}
			(*pRemoved)++;
		}
		pNode = pNext;
	}
	return true;
}



/* 删除一级桶中覆盖指定方法或字段权限的全部 Node。 */
static bool __xrtHttpCorsCachePermissionRemove(
	xhttpcorscache* pCache,
	uint64 iHash,
	const xhttpcorscachekey* pKey,
	bool bCredentials,
	uint8 iKind,
	xstrview Name,
	xrt_http_cors_cache_node** ppRetired,
	size_t* pRemoved
)
{
	xrt_http_cors_cache_node** ppHead =
		__xrtHttpCorsCacheBucket(pCache, &iHash);
	xrt_http_cors_cache_node* pNode =
		ppHead != NULL ? *ppHead : NULL;

	while ( pNode != NULL ) {
		xrt_http_cors_cache_node* pNext = pNode->BucketNext;

		if ( __xrtHttpCorsCacheNodeKeySame(
			pNode, pKey
		) && __xrtHttpCorsCacheCredentialsMatch(
			pNode, bCredentials
		) && __xrtHttpCorsCachePermissionMatch(
			pNode, iKind, Name
		) ) {
			if ( !__xrtHttpCorsCacheNodeDetach(
				pCache,
				pNode,
				XRT_HTTP_CORS_CACHE_REMOVE_EXPLICIT,
				ppRetired
			) ) {
				return false;
			}
			(*pRemoved)++;
		}
		pNode = pNext;
	}
	return true;
}



/* 判断一级桶中是否存在覆盖当前权限的有效项。 */
static bool __xrtHttpCorsCachePermissionHas(
	xhttpcorscache* pCache,
	uint64 iHash,
	const xhttpcorscachekey* pKey,
	bool bCredentials,
	uint8 iKind,
	xstrview Name
)
{
	xrt_http_cors_cache_node** ppHead =
		__xrtHttpCorsCacheBucket(pCache, &iHash);
	xrt_http_cors_cache_node* pNode =
		ppHead != NULL ? *ppHead : NULL;

	while ( pNode != NULL ) {
		if ( __xrtHttpCorsCacheNodeKeySame(
			pNode, pKey
		) && __xrtHttpCorsCacheCredentialsMatch(
			pNode, bCredentials
		) && __xrtHttpCorsCachePermissionMatch(
			pNode, iKind, Name
		) ) {
			__xrtHttpCorsCacheLRUTouch(pCache, pNode);
			return true;
		}
		pNode = pNode->BucketNext;
	}
	return false;
}



/* 在锁内插入一个预先分配的权限 Node。 */
static void __xrtHttpCorsCacheNodeInsert(
	xhttpcorscache* pCache,
	xrt_http_cors_cache_node** ppHead,
	xrt_http_cors_cache_node* pNode
)
{
	pNode->BucketPrevious = NULL;
	pNode->BucketNext = *ppHead;
	if ( *ppHead != NULL ) {
		(*ppHead)->BucketPrevious = pNode;
	}
	*ppHead = pNode;
	__xrtHttpCorsCacheLRUFirst(pCache, pNode);
	pCache->Stats.Entries++;
	__xrtHttpCorsCacheCounterAdd(
		&pCache->Stats.Stores, 1u
	);
}



/* 淘汰最旧 Node，直到重新满足硬条目上限。 */
static bool __xrtHttpCorsCacheLimit(
	xhttpcorscache* pCache,
	xrt_http_cors_cache_node** ppRetired
)
{
	while ( pCache->Stats.Entries >
		pCache->Config.MaxEntries ) {
		if ( (pCache->LRUTail == NULL) ||
			!__xrtHttpCorsCacheNodeDetach(
				pCache,
				pCache->LRUTail,
				XRT_HTTP_CORS_CACHE_REMOVE_EVICTED,
				ppRetired
			) ) {
			return false;
		}
	}
	return true;
}



/* 把秒数转换为钳制且不回绕的单调时钟到期点。 */
static uint64 __xrtHttpCorsCacheExpires(
	uint64 iNow,
	uint64 iSeconds
)
{
	const uint64 iScale = UINT64_C(1000000);

	if ( iSeconds > ((UINT64_MAX - iNow) / iScale) ) {
		return UINT64_MAX;
	}
	return iNow + (iSeconds * iScale);
}



/* 无分配撤销零寿命响应列出的全部权限。 */
static bool __xrtHttpCorsCacheRevoke(
	xhttpcorscache* pCache,
	const xhttpcorscachekey* pKey,
	uint64 iHash,
	xstrview Method,
	bool bForce,
	bool bCredentials,
	const xhttpfield* pFields,
	size_t iCount,
	size_t* pUpdated
)
{
	xrt_http_cors_cache_node* pRetired = NULL;
	xhttpcorscursor Cursor;
	xstrview Name;
	xhttpnext Next;
	uint64 iNow;
	size_t iMethodFields;
	size_t iUpdated = 0;
	size_t iExpired = 0;

	if ( !__xrtHttpCorsCachePermissionsValid(
		pFields, iCount, &iMethodFields
	) ) {
		return false;
	}
	iNow = xrtClock();
	if ( !xrtMutexLock(&pCache->Lock) ) {
		return false;
	}
	if ( !__xrtHttpCorsCacheBucketPurge(
		pCache,
		iHash,
		iNow,
		&pRetired,
		&iExpired
	) ) {
		goto fail;
	}

	xrtHttpCorsCursorInit(&Cursor);
	while ( (Next = xrtHttpCorsAllowMethodNext(
		pFields, iCount, &Cursor, &Name
	)) == XHTTP_NEXT_ITEM ) {
		if ( !__xrtHttpCorsCachePermissionRemove(
			pCache,
			iHash,
			pKey,
			bCredentials,
			XRT_HTTP_CORS_CACHE_METHOD,
			Name,
			&pRetired,
			&iUpdated
		) ) {
			goto fail;
		}
	}
	if ( Next == XHTTP_NEXT_ERROR ) {
		goto fail;
	}
	if ( bForce && (iMethodFields == 0) &&
		!__xrtHttpCorsCachePermissionRemove(
			pCache,
			iHash,
			pKey,
			bCredentials,
			XRT_HTTP_CORS_CACHE_METHOD,
			Method,
			&pRetired,
			&iUpdated
		) ) {
		goto fail;
	}

	xrtHttpCorsCursorInit(&Cursor);
	while ( (Next = xrtHttpCorsAllowHeaderNext(
		pFields, iCount, &Cursor, &Name
	)) == XHTTP_NEXT_ITEM ) {
		if ( !__xrtHttpCorsCachePermissionRemove(
			pCache,
			iHash,
			pKey,
			bCredentials,
			XRT_HTTP_CORS_CACHE_HEADER,
			Name,
			&pRetired,
			&iUpdated
		) ) {
			goto fail;
		}
	}
	if ( Next == XHTTP_NEXT_ERROR ) {
		goto fail;
	}
	(void)xrtMutexUnlock(&pCache->Lock);
	__xrtHttpCorsCacheWorkRelease(pRetired);
	if ( pUpdated != NULL ) {
		memcpy(pUpdated, &iUpdated, sizeof(iUpdated));
	}
	return true;

fail:
	(void)xrtMutexUnlock(&pCache->Lock);
	__xrtHttpCorsCacheWorkRelease(pRetired);
	return false;
}



/* 解析并验证 Cache 配置。 */
static bool __xrtHttpCorsCacheConfigResolve(
	const xhttpcorscacheconfig* pInput,
	xhttpcorscacheconfig* pConfig
)
{
	xrtHttpCorsCacheConfigInit(pConfig);
	if ( pInput != NULL ) {
		if ( !__xrtRangeValid(pInput, sizeof(*pConfig)) ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
		memcpy(pConfig, pInput, sizeof(*pConfig));
	}
	if ( (pConfig->MaxEntries == 0) ||
		(pConfig->MaxAge == 0) ||
		(pConfig->InitialEntries > pConfig->MaxEntries) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 写入默认预检缓存配置。 */
XRT_API void xrtHttpCorsCacheConfigInit(
	xhttpcorscacheconfig* pConfig
)
{
	xhttpcorscacheconfig Config;

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	Config.InitialEntries =
		XHTTP_CORS_CACHE_INITIAL_DEFAULT;
	Config.MaxEntries = XHTTP_CORS_CACHE_ENTRIES_DEFAULT;
	Config.MaxAge = XHTTP_CORS_CACHE_MAX_AGE_DEFAULT;
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 初始化不带网络分区的缓存键。 */
XRT_API bool xrtHttpCorsCacheKeyInit(
	xhttpcorscachekey* pOutput,
	const xhttporigin* pOrigin,
	xstrview URL
)
{
	xhttpcorscachekey Output;
	xhttporigin Origin;

	if ( !__xrtRangeValid(pOutput, sizeof(Output)) ||
		!__xrtRangeValid(pOrigin, sizeof(Origin)) ||
		!__xrtHttpViewValid(URL) || (URL.Size == 0) ||
		__xrtRangesOverlap(
			pOutput, sizeof(Output),
			pOrigin, sizeof(Origin)
		) || __xrtRangesOverlap(
			pOutput, sizeof(Output),
			URL.Data, URL.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Origin, pOrigin, sizeof(Origin));
	memset(&Output, 0, sizeof(Output));
	memcpy(pOutput, &Output, sizeof(Output));
	if ( !__xrtHttpOriginValueValid(&Origin) ||
		__xrtHttpOriginOverlap(
			&Origin, pOutput, sizeof(Output)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Output.Origin = Origin;
	Output.URL = URL;
	memcpy(pOutput, &Output, sizeof(Output));
	return true;
}



/* 创建默认或显式配置的线程安全预检缓存。 */
XRT_API xhttpcorscache* xrtHttpCorsCacheCreate(
	const xhttpcorscacheconfig* pConfig
)
{
	xhttpcorscacheconfig Config;
	xhttpcorscache* pCache;

	if ( !__xrtHttpCorsCacheConfigResolve(
		pConfig, &Config
	) ) {
		return NULL;
	}
	pCache = (xhttpcorscache*)xrtCalloc(
		1, sizeof(*pCache)
	);
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



/* 增加 Cache 引用并返回原指针。 */
XRT_API xhttpcorscache* xrtHttpCorsCacheRetain(
	const xhttpcorscache* pCache
)
{
	if ( (pCache == NULL) ||
		(xrtRefRetain(
			&((xhttpcorscache*)pCache)->References
		) < 0) ) {
		return NULL;
	}
	return (xhttpcorscache*)pCache;
}



/* 释放 Cache 最后一个引用及其全部权限。 */
XRT_API void xrtHttpCorsCacheRelease(xhttpcorscache* pCache)
{
	xrt_http_cors_cache_node* pNodes;

	if ( (pCache == NULL) ||
		(xrtRefRelease(&pCache->References) != 0) ) {
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
	__xrtHttpCorsCacheLRURelease(pNodes);
	memset(pCache, 0, sizeof(*pCache));
	xrtFree(pCache);
}



/* 规划请求并查询方法与全部非安全字段权限。 */
XRT_API bool xrtHttpCorsCachePlan(
	xhttpcorscache* pCache,
	const xhttpcorscachekey* pKey,
	xstrview Method,
	const xhttpfield* pFields,
	size_t iCount,
	bool bCredentials,
	bool bForce,
	xhttpcorspreflightplan* pOutput
)
{
	xhttpcorspreflightplan Output = { 0 };
	xhttpcorscachekey Key;
	xrt_http_cors_request_info Info;
	xrt_http_cors_cache_node* pRetired = NULL;
	xstrview Name;
	uint64 iHash;
	uint64 iNow;
	size_t iOffset = 0;
	size_t iExpired = 0;
	bool bCached = true;

	if ( (pCache == NULL) ||
		!__xrtRangeValid(pKey, sizeof(Key)) ||
		!__xrtHttpViewValid(Method) ||
		!__xrtHttpFieldArrayValid(pFields, iCount) ||
		!__xrtRangeValid(pOutput, sizeof(Output)) ||
		__xrtRangesOverlap(
			pKey, sizeof(Key), pOutput, sizeof(Output)
		) || __xrtRangesOverlap(
			Method.Data, Method.Size,
			pOutput, sizeof(Output)
		) || __xrtHttpFieldArrayOverlap(
			pFields, iCount, pOutput, sizeof(Output)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpCorsCacheKeyResolve(pKey, &Key) ||
		__xrtHttpCorsCacheKeyOverlap(
			&Key, pOutput, sizeof(Output)
		) ) {
		if ( xrtGetError() == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	memcpy(pOutput, &Output, sizeof(Output));
	if ( !xrtHttpTokenValid(Method) ) {
		__xrtErrorSetValue();
		return false;
	}
	if ( !__xrtHttpCorsRequestInspect(
		pFields, iCount, &Info
	) ) {
		return false;
	}
	__xrtHttpCorsPreflightMake(
		Method, bForce, &Info, &Output
	);
	if ( (Output.Flags &
		XHTTP_CORS_PREFLIGHT_REQUIRED) == 0 ) {
		memcpy(pOutput, &Output, sizeof(Output));
		return true;
	}
	iHash = __xrtHttpCorsCacheHash(&Key);
	iNow = xrtClock();
	if ( !xrtMutexLock(&pCache->Lock) ) {
		return false;
	}
	__xrtHttpCorsCacheCounterAdd(
		&pCache->Stats.Lookups, 1u
	);
	if ( !__xrtHttpCorsCacheBucketPurge(
		pCache,
		iHash,
		iNow,
		&pRetired,
		&iExpired
	) ) {
		(void)xrtMutexUnlock(&pCache->Lock);
		__xrtHttpCorsCacheWorkRelease(pRetired);
		return false;
	}
	if ( bForce || !xrtHttpCorsMethodSafelisted(Method) ) {
		bCached = __xrtHttpCorsCachePermissionHas(
			pCache,
			iHash,
			&Key,
			bCredentials,
			XRT_HTTP_CORS_CACHE_METHOD,
			Method
		);
	}
	while ( bCached &&
		(__xrtHttpCorsUnsafeNameNext(
			pFields,
			iCount,
			&Info,
			&iOffset,
			&Name
		) == XHTTP_NEXT_ITEM) ) {
		bCached = __xrtHttpCorsCachePermissionHas(
			pCache,
			iHash,
			&Key,
			bCredentials,
			XRT_HTTP_CORS_CACHE_HEADER,
			Name
		);
	}
	if ( bCached ) {
		Output.Flags |= XHTTP_CORS_PREFLIGHT_CACHED;
		__xrtHttpCorsCacheCounterAdd(
			&pCache->Stats.Hits, 1u
		);
	} else {
		__xrtHttpCorsCacheCounterAdd(
			&pCache->Stats.Misses, 1u
		);
	}
	(void)xrtMutexUnlock(&pCache->Lock);
	__xrtHttpCorsCacheWorkRelease(pRetired);
	memcpy(pOutput, &Output, sizeof(Output));
	return true;
}



/* 刷新已有权限或插入新的方法与字段权限。 */
XRT_API bool xrtHttpCorsCacheUpdate(
	xhttpcorscache* pCache,
	const xhttpcorscachekey* pKey,
	xstrview Method,
	bool bForce,
	bool bCredentials,
	const xhttpfield* pResponseFields,
	size_t iResponseFieldCount,
	const xhttpcorsclientresult* pResult,
	size_t* pUpdated
)
{
	xhttpcorscachekey Key;
	xhttpcorsclientresult Result;
	xrt_http_cors_cache_node* pPrepared = NULL;
	xrt_http_cors_cache_node* pRetired = NULL;
	xrt_http_cors_cache_node** ppHead;
	uint64 iHash;
	uint64 iNow;
	uint64 iMaxAge;
	uint64 iExpires;
	size_t iUpdated = 0;
	size_t iExpired = 0;
	bool bNew = false;

	if ( (pUpdated != NULL) &&
		!__xrtRangeValid(pUpdated, sizeof(iUpdated)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pCache == NULL) ||
		!__xrtRangeValid(pKey, sizeof(Key)) ||
		!__xrtHttpViewValid(Method) ||
		!__xrtHttpFieldArrayValid(
			pResponseFields, iResponseFieldCount
		) || !__xrtRangeValid(pResult, sizeof(Result)) ||
		((pUpdated != NULL) &&
		 (__xrtRangesOverlap(
			pKey, sizeof(Key),
			pUpdated, sizeof(iUpdated)
		 ) || __xrtRangesOverlap(
			Method.Data, Method.Size,
			pUpdated, sizeof(iUpdated)
		 ) || __xrtHttpFieldArrayOverlap(
			pResponseFields,
			iResponseFieldCount,
			pUpdated,
			sizeof(iUpdated)
		 ) || __xrtRangesOverlap(
			pResult, sizeof(Result),
			pUpdated, sizeof(iUpdated)
		 ))) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Result, pResult, sizeof(Result));
	if ( !__xrtHttpCorsCacheKeyResolve(pKey, &Key) ||
		((pUpdated != NULL) &&
		 __xrtHttpCorsCacheKeyOverlap(
			&Key, pUpdated, sizeof(iUpdated)
		 )) ) {
		if ( xrtGetError() == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	if ( pUpdated != NULL ) {
		memcpy(pUpdated, &iUpdated, sizeof(iUpdated));
	}
	if ( !xrtHttpTokenValid(Method) ) {
		__xrtErrorSetValue();
		return false;
	}
	if ( (Result.Reject != XHTTP_CORS_CLIENT_REJECT_NONE) ||
		(Result.Flags != (
			XHTTP_CORS_CLIENT_ALLOW |
			XHTTP_CORS_CLIENT_PREFLIGHT
		)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	iHash = __xrtHttpCorsCacheHash(&Key);
	iMaxAge = Result.MaxAge < pCache->Config.MaxAge ?
		Result.MaxAge : pCache->Config.MaxAge;
	if ( iMaxAge == 0 ) {
		return __xrtHttpCorsCacheRevoke(
			pCache,
			&Key,
			iHash,
			Method,
			bForce,
			bCredentials,
			pResponseFields,
			iResponseFieldCount,
			pUpdated
		);
	}
	if ( !__xrtHttpCorsCachePrepare(
		&Key,
		iHash,
		Method,
		bForce,
		bCredentials,
		pResponseFields,
		iResponseFieldCount,
		pCache->Config.MaxEntries,
		&pPrepared
	) ) {
		return false;
	}
	if ( pPrepared == NULL ) {
		return true;
	}
	iNow = xrtClock();
	iExpires = __xrtHttpCorsCacheExpires(iNow, iMaxAge);
	if ( !xrtMutexLock(&pCache->Lock) ) {
		__xrtHttpCorsCacheWorkRelease(pPrepared);
		return false;
	}
	if ( !__xrtHttpCorsCacheBucketPurge(
		pCache,
		iHash,
		iNow,
		&pRetired,
		&iExpired
	) ) {
		(void)xrtMutexUnlock(&pCache->Lock);
		__xrtHttpCorsCacheWorkRelease(pPrepared);
		__xrtHttpCorsCacheWorkRelease(pRetired);
		return false;
	}
	ppHead = __xrtHttpCorsCacheBucket(pCache, &iHash);
	if ( ppHead == NULL ) {
		ppHead = (xrt_http_cors_cache_node**)
			xrtMapGetOrAdd(
				&pCache->Index,
				__xrtHttpCorsCacheMapKey(&iHash),
				&bNew
			);
		if ( ppHead == NULL ) {
			(void)xrtMutexUnlock(&pCache->Lock);
			__xrtHttpCorsCacheWorkRelease(pPrepared);
			__xrtHttpCorsCacheWorkRelease(pRetired);
			return false;
		}
		(void)bNew;
	}
	while ( pPrepared != NULL ) {
		xrt_http_cors_cache_node* pPermission =
			pPrepared;
		xrt_http_cors_cache_node* pNode;
		xrt_http_cors_cache_node* pNext;
		bool bFound = false;

		pPrepared = pPrepared->WorkNext;
		pPermission->WorkNext = NULL;
		ppHead = __xrtHttpCorsCacheBucket(
			pCache, &iHash
		);
		pNode = ppHead != NULL ? *ppHead : NULL;
		while ( pNode != NULL ) {
			pNext = pNode->BucketNext;
			if ( __xrtHttpCorsCacheNodeKeySame(
				pNode, &Key
			) && __xrtHttpCorsCacheCredentialsMatch(
				pNode, bCredentials
			) && __xrtHttpCorsCachePermissionMatch(
				pNode,
				pPermission->Kind,
				pPermission->Name
			) ) {
				bFound = true;
				iUpdated++;
				pNode->Expires = iExpires;
				__xrtHttpCorsCacheLRUTouch(
					pCache, pNode
				);
				__xrtHttpCorsCacheCounterAdd(
					&pCache->Stats.Replacements,
					1u
				);
			}
			pNode = pNext;
		}
		if ( !bFound ) {
			ppHead = __xrtHttpCorsCacheBucket(
				pCache, &iHash
			);
			if ( ppHead == NULL ) {
				__xrtErrorSetInternal();
				(void)xrtMutexUnlock(&pCache->Lock);
				xrtFree(pPermission);
				__xrtHttpCorsCacheWorkRelease(pPrepared);
				__xrtHttpCorsCacheWorkRelease(pRetired);
				return false;
			}
			pPermission->Expires = iExpires;
			__xrtHttpCorsCacheNodeInsert(
				pCache, ppHead, pPermission
			);
			pPermission = NULL;
			iUpdated++;
		}
		xrtFree(pPermission);
	}
	if ( !__xrtHttpCorsCacheLimit(
		pCache, &pRetired
	) ) {
		(void)xrtMutexUnlock(&pCache->Lock);
		__xrtHttpCorsCacheWorkRelease(pRetired);
		return false;
	}
	(void)xrtMutexUnlock(&pCache->Lock);
	__xrtHttpCorsCacheWorkRelease(pRetired);
	if ( pUpdated != NULL ) {
		memcpy(pUpdated, &iUpdated, sizeof(iUpdated));
	}
	return true;
}



/* 删除一个完整预检缓存键下的全部权限。 */
XRT_API bool xrtHttpCorsCacheRemove(
	xhttpcorscache* pCache,
	const xhttpcorscachekey* pKey,
	size_t* pRemoved
)
{
	xhttpcorscachekey Key;
	xrt_http_cors_cache_node* pRetired = NULL;
	xrt_http_cors_cache_node** ppHead;
	xrt_http_cors_cache_node* pNode;
	uint64 iHash;
	size_t iRemoved = 0;

	if ( (pRemoved != NULL) &&
		!__xrtRangeValid(pRemoved, sizeof(iRemoved)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pCache == NULL) ||
		!__xrtRangeValid(pKey, sizeof(Key)) ||
		((pRemoved != NULL) &&
		 __xrtRangesOverlap(
			pKey, sizeof(Key),
			pRemoved, sizeof(iRemoved)
		 )) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpCorsCacheKeyResolve(pKey, &Key) ||
		((pRemoved != NULL) &&
		 __xrtHttpCorsCacheKeyOverlap(
			&Key, pRemoved, sizeof(iRemoved)
		 )) ) {
		if ( xrtGetError() == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	if ( pRemoved != NULL ) {
		memcpy(pRemoved, &iRemoved, sizeof(iRemoved));
	}
	iHash = __xrtHttpCorsCacheHash(&Key);
	if ( !xrtMutexLock(&pCache->Lock) ) {
		return false;
	}
	ppHead = __xrtHttpCorsCacheBucket(pCache, &iHash);
	pNode = ppHead != NULL ? *ppHead : NULL;
	while ( pNode != NULL ) {
		xrt_http_cors_cache_node* pNext =
			pNode->BucketNext;

		if ( __xrtHttpCorsCacheNodeKeySame(
			pNode, &Key
		) ) {
			if ( !__xrtHttpCorsCacheNodeDetach(
				pCache,
				pNode,
				XRT_HTTP_CORS_CACHE_REMOVE_EXPLICIT,
				&pRetired
			) ) {
				(void)xrtMutexUnlock(&pCache->Lock);
				__xrtHttpCorsCacheWorkRelease(pRetired);
				return false;
			}
			iRemoved++;
		}
		pNode = pNext;
	}
	(void)xrtMutexUnlock(&pCache->Lock);
	__xrtHttpCorsCacheWorkRelease(pRetired);
	if ( pRemoved != NULL ) {
		memcpy(pRemoved, &iRemoved, sizeof(iRemoved));
	}
	return true;
}



/* 删除全部已经达到单调时钟截止点的权限。 */
XRT_API bool xrtHttpCorsCachePurge(
	xhttpcorscache* pCache,
	size_t* pRemoved
)
{
	xrt_http_cors_cache_node* pRetired = NULL;
	xrt_http_cors_cache_node* pNode;
	uint64 iNow;
	size_t iRemoved = 0;

	if ( (pCache == NULL) ||
		((pRemoved != NULL) &&
		 !__xrtRangeValid(pRemoved, sizeof(iRemoved))) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pRemoved != NULL ) {
		memcpy(pRemoved, &iRemoved, sizeof(iRemoved));
	}
	iNow = xrtClock();
	if ( !xrtMutexLock(&pCache->Lock) ) {
		return false;
	}
	pNode = pCache->LRUHead;
	while ( pNode != NULL ) {
		xrt_http_cors_cache_node* pNext = pNode->LRUNext;

		if ( pNode->Expires <= iNow ) {
			if ( !__xrtHttpCorsCacheNodeDetach(
				pCache,
				pNode,
				XRT_HTTP_CORS_CACHE_REMOVE_EXPIRED,
				&pRetired
			) ) {
				(void)xrtMutexUnlock(&pCache->Lock);
				__xrtHttpCorsCacheWorkRelease(pRetired);
				return false;
			}
			iRemoved++;
		}
		pNode = pNext;
	}
	(void)xrtMutexUnlock(&pCache->Lock);
	__xrtHttpCorsCacheWorkRelease(pRetired);
	if ( pRemoved != NULL ) {
		memcpy(pRemoved, &iRemoved, sizeof(iRemoved));
	}
	return true;
}



/* 清空全部权限并保留 Map 容量。 */
XRT_API bool xrtHttpCorsCacheClear(xhttpcorscache* pCache)
{
	xrt_http_cors_cache_node* pNodes;
	size_t iRemoved;

	if ( pCache == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
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
	__xrtHttpCorsCacheCounterAdd(
		&pCache->Stats.Removals,
		(uint64)iRemoved
	);
	(void)xrtMutexUnlock(&pCache->Lock);
	__xrtHttpCorsCacheLRURelease(pNodes);
	return true;
}



/* 取得锁内一致的缓存统计快照。 */
XRT_API bool xrtHttpCorsCacheStats(
	xhttpcorscache* pCache,
	xhttpcorscachestats* pOutput
)
{
	xhttpcorscachestats Output = { 0 };

	if ( (pCache == NULL) ||
		!__xrtRangeValid(pOutput, sizeof(Output)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pOutput, &Output, sizeof(Output));
	if ( !xrtMutexLock(&pCache->Lock) ) {
		return false;
	}
	Output = pCache->Stats;
	(void)xrtMutexUnlock(&pCache->Lock);
	memcpy(pOutput, &Output, sizeof(Output));
	return true;
}

#endif
