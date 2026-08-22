#include "../internal/xrt_http.h"

#include <xrt/http_auth.h>



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_REPLAY)

#define XRT_HTTP_DIGEST_REPLAY_GUARD UINT32_C(0x4852504C)
#define XRT_HTTP_DIGEST_REPLAY_DEFAULT_SHARDS 16u
#define XRT_HTTP_DIGEST_REPLAY_DEFAULT_ENTRIES 1024u
#define XRT_HTTP_DIGEST_REPLAY_DEFAULT_LIFETIME 300
#define XRT_HTTP_DIGEST_REPLAY_SHARDS_MAX 256u
#define XRT_HTTP_DIGEST_REPLAY_MAX_MESSAGE (UINT64_MAX >> 3u)



/* 单条记录只保存不可逆键、最大 nc 和固定过期时间。 */
typedef struct xrt_http_digest_replay_entry {
	xhttpdigestreplaykey Key;
	uint32 NonceCount;
	int64 ExpiresSeconds;
	size_t HeapIndex;
	bool Occupied;
} xrt_http_digest_replay_entry;



/* 每个分片独立持有固定哈希槽、过期小根堆和策略统计。 */
typedef struct xrt_http_digest_replay_shard {
	xmutex Lock;
	xrt_http_digest_replay_entry* Slots;
	xrt_http_digest_replay_entry** Heap;
	size_t EntryCount;
	size_t HeapCount;
	size_t Capacity;
	size_t SlotCapacity;
	uint64 Accepted;
	uint64 Replayed;
	uint64 Expired;
	uint64 Full;
	uint64 Purged;
	bool LockReady;
} xrt_http_digest_replay_shard;



/* 所有槽和堆在创建时一次性分配，锁内检查路径不再分配。 */
struct xhttpdigestreplay {
	uint32 Guard;
	int64 LifetimeSeconds;
	size_t ShardCount;
	size_t TotalCapacity;
	size_t TotalSlots;
	xrt_http_digest_replay_entry* SlotStorage;
	xrt_http_digest_replay_entry** HeapStorage;
	xrt_http_digest_replay_shard Shards[];
};



static const uint8 __xrtHttpDigestReplayDomain[] =
	"xrt-http-digest-replay-v1";



/* 写出无符号 64 位大端长度，避免不同字段拼接产生歧义。 */
static void __xrtHttpDigestReplayStore64(uint8* pOutput, uint64 iValue)
{
	for ( size_t i = 0; i < 8u; i++ ) {
		pOutput[7u - i] = (uint8)(iValue >> (i * 8u));
	}
}



/* 验证重放表对象并拒绝明显失效的对象。 */
static bool __xrtHttpDigestReplayValid(const xhttpdigestreplay* pReplay)
{
	if ( (pReplay == NULL) ||
		!__xrtRangeValid(pReplay, sizeof(*pReplay)) ||
		(pReplay->Guard != XRT_HTTP_DIGEST_REPLAY_GUARD) ||
		(pReplay->ShardCount == 0) ||
		(pReplay->ShardCount > XRT_HTTP_DIGEST_REPLAY_SHARDS_MAX) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 饱和增加统计，避免诊断计数在长期服务中回绕。 */
static void __xrtHttpDigestReplayCounterAdd(
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



/* 从固定键前八字节读取均匀分布的表索引哈希。 */
static uint64 __xrtHttpDigestReplayHash(
	const xhttpdigestreplaykey* pKey
)
{
	uint64 iHash;

	memcpy(&iHash, pKey->Bytes, sizeof(iHash));
	return iHash;
}



/* 返回环形开放寻址中的下一个槽。 */
static size_t __xrtHttpDigestReplayNext(size_t iIndex, size_t iCapacity)
{
	iIndex++;
	return iIndex == iCapacity ? 0 : iIndex;
}



/* 计算某槽沿线性探测路径距离理想槽的步数。 */
static size_t __xrtHttpDigestReplayDistance(
	size_t iHome,
	size_t iIndex,
	size_t iCapacity
)
{
	return iIndex >= iHome ?
		(iIndex - iHome) : (iCapacity - iHome + iIndex);
}



/* 查找固定键，并按需返回首次空槽供新记录使用。 */
static xrt_http_digest_replay_entry* __xrtHttpDigestReplayFind(
	xrt_http_digest_replay_shard* pShard,
	const xhttpdigestreplaykey* pKey,
	xrt_http_digest_replay_entry** ppEmpty
)
{
	size_t iIndex = (size_t)(
		__xrtHttpDigestReplayHash(pKey) %
		(uint64)pShard->SlotCapacity
	);

	if ( ppEmpty != NULL ) {
		*ppEmpty = NULL;
	}
	for ( size_t i = 0; i < pShard->SlotCapacity; i++ ) {
		xrt_http_digest_replay_entry* pEntry = &pShard->Slots[iIndex];

		if ( !pEntry->Occupied ) {
			if ( ppEmpty != NULL ) {
				*ppEmpty = pEntry;
			}
			return NULL;
		}
		if ( memcmp(
			pEntry->Key.Bytes,
			pKey->Bytes,
			XRT_HTTP_DIGEST_REPLAY_KEY_SIZE
		) == 0 ) {
			return pEntry;
		}
		iIndex = __xrtHttpDigestReplayNext(
			iIndex, pShard->SlotCapacity
		);
	}
	return NULL;
}



/* 比较过期时间和键，使堆顺序在相同时间下仍完全确定。 */
static bool __xrtHttpDigestReplayEntryLess(
	const xrt_http_digest_replay_entry* pLeft,
	const xrt_http_digest_replay_entry* pRight
)
{
	if ( pLeft->ExpiresSeconds != pRight->ExpiresSeconds ) {
		return pLeft->ExpiresSeconds < pRight->ExpiresSeconds;
	}
	return memcmp(
		pLeft->Key.Bytes,
		pRight->Key.Bytes,
		XRT_HTTP_DIGEST_REPLAY_KEY_SIZE
	) < 0;
}



/* 向容量已经确认的分片过期堆加入一条记录。 */
static void __xrtHttpDigestReplayHeapPush(
	xrt_http_digest_replay_shard* pShard,
	xrt_http_digest_replay_entry* pEntry
)
{
	size_t iIndex = pShard->HeapCount++;

	while ( iIndex != 0 ) {
		size_t iParent = (iIndex - 1u) / 2u;
		xrt_http_digest_replay_entry* pParent = pShard->Heap[iParent];

		if ( !__xrtHttpDigestReplayEntryLess(pEntry, pParent) ) {
			break;
		}
		pShard->Heap[iIndex] = pParent;
		pParent->HeapIndex = iIndex;
		iIndex = iParent;
	}
	pShard->Heap[iIndex] = pEntry;
	pEntry->HeapIndex = iIndex;
}



/* 从过期堆删除根，但暂不修改哈希槽。 */
static xrt_http_digest_replay_entry* __xrtHttpDigestReplayHeapPop(
	xrt_http_digest_replay_shard* pShard
)
{
	xrt_http_digest_replay_entry* pRemoved = pShard->Heap[0];
	xrt_http_digest_replay_entry* pLast =
		pShard->Heap[pShard->HeapCount - 1u];
	size_t iIndex = 0;

	pShard->HeapCount--;
	if ( pShard->HeapCount != 0 ) {
		while ( true ) {
			size_t iLeft = (iIndex * 2u) + 1u;
			size_t iRight = iLeft + 1u;
			size_t iChild;
			xrt_http_digest_replay_entry* pChild;

			if ( iLeft >= pShard->HeapCount ) {
				break;
			}
			iChild = ((iRight < pShard->HeapCount) &&
				__xrtHttpDigestReplayEntryLess(
					pShard->Heap[iRight], pShard->Heap[iLeft]
				)) ? iRight : iLeft;
			pChild = pShard->Heap[iChild];
			if ( !__xrtHttpDigestReplayEntryLess(pChild, pLast) ) {
				break;
			}
			pShard->Heap[iIndex] = pChild;
			pChild->HeapIndex = iIndex;
			iIndex = iChild;
		}
		pShard->Heap[iIndex] = pLast;
		pLast->HeapIndex = iIndex;
	}
	pShard->Heap[pShard->HeapCount] = NULL;
	return pRemoved;
}



/* 后移探测簇填补删除空洞，并同步修正堆中的稳定指针。 */
static bool __xrtHttpDigestReplayRemove(
	xrt_http_digest_replay_shard* pShard,
	xrt_http_digest_replay_entry* pEntry
)
{
	uintptr_t iSlotsAddress = (uintptr_t)pShard->Slots;
	uintptr_t iEntryAddress = (uintptr_t)pEntry;
	size_t iSlotBytes;
	size_t iHole;
	size_t iScan;

	if ( (pShard->SlotCapacity == 0) ||
		(pShard->SlotCapacity >
		 (SIZE_MAX / sizeof(xrt_http_digest_replay_entry))) ) {
		__xrtErrorSetInternal();
		return false;
	}
	iSlotBytes = pShard->SlotCapacity *
		sizeof(xrt_http_digest_replay_entry);
	if ( (iSlotsAddress > (UINTPTR_MAX - iSlotBytes)) ||
		(iEntryAddress < iSlotsAddress) ||
		(iEntryAddress >= (iSlotsAddress + iSlotBytes)) ||
		(((iEntryAddress - iSlotsAddress) %
		  sizeof(xrt_http_digest_replay_entry)) != 0) ) {
		__xrtErrorSetInternal();
		return false;
	}
	iHole = (size_t)(
		(iEntryAddress - iSlotsAddress) /
		sizeof(xrt_http_digest_replay_entry)
	);
	pEntry = &pShard->Slots[iHole];
	if ( !pEntry->Occupied || (pShard->EntryCount == 0) ) {
		__xrtErrorSetInternal();
		return false;
	}
	pEntry->Occupied = false;
	iScan = __xrtHttpDigestReplayNext(iHole, pShard->SlotCapacity);
	while ( pShard->Slots[iScan].Occupied ) {
		xrt_http_digest_replay_entry* pMove = &pShard->Slots[iScan];
		size_t iHome = (size_t)(
			__xrtHttpDigestReplayHash(&pMove->Key) %
			(uint64)pShard->SlotCapacity
		);

		if ( __xrtHttpDigestReplayDistance(
			iHome, iHole, pShard->SlotCapacity
		) < __xrtHttpDigestReplayDistance(
			iHome, iScan, pShard->SlotCapacity
		) ) {
			pShard->Slots[iHole] = *pMove;
			pShard->Heap[pShard->Slots[iHole].HeapIndex] =
				&pShard->Slots[iHole];
			pMove->Occupied = false;
			iHole = iScan;
		}
		iScan = __xrtHttpDigestReplayNext(
			iScan, pShard->SlotCapacity
		);
	}
	memset(&pShard->Slots[iHole], 0, sizeof(pShard->Slots[iHole]));
	pShard->EntryCount--;
	return true;
}



/* 在线性堆顶路径上删除全部已经失效的记录。 */
static bool __xrtHttpDigestReplayPurgeLocked(
	xrt_http_digest_replay_shard* pShard,
	int64 iNowSeconds,
	size_t* pRemoved
)
{
	size_t iRemoved = 0;

	while ( (pShard->HeapCount != 0) &&
		(pShard->Heap[0]->ExpiresSeconds < iNowSeconds) ) {
		xrt_http_digest_replay_entry* pEntry =
			__xrtHttpDigestReplayHeapPop(pShard);

		if ( !__xrtHttpDigestReplayRemove(pShard, pEntry) ) {
			return false;
		}
		iRemoved++;
	}
	__xrtHttpDigestReplayCounterAdd(&pShard->Purged, iRemoved);
	*pRemoved = iRemoved;
	return true;
}



/* 从固定键选择分片。 */
static size_t __xrtHttpDigestReplayShard(
	const xhttpdigestreplay* pReplay,
	const xhttpdigestreplaykey* pKey
)
{
	return (size_t)(
		__xrtHttpDigestReplayHash(pKey) %
		(uint64)pReplay->ShardCount
	);
}



/* 解析默认或调用方配置，并验证所有容量乘法。 */
static bool __xrtHttpDigestReplayConfigResolve(
	const xhttpdigestreplayconfig* pInput,
	xhttpdigestreplayconfig* pConfig,
	size_t* pTotalCapacity,
	size_t* pSlotsPerShard,
	size_t* pTotalSlots
)
{
	size_t iExtraSlots;

	xrtHttpDigestReplayConfigInit(pConfig);
	if ( pInput != NULL ) {
		if ( !__xrtRangeValid(pInput, sizeof(*pConfig)) ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
		memcpy(pConfig, pInput, sizeof(*pConfig));
	}
	if ( (pConfig->Shards == 0) ||
		(pConfig->Shards > XRT_HTTP_DIGEST_REPLAY_SHARDS_MAX) ||
		(pConfig->EntriesPerShard == 0) ||
		(pConfig->LifetimeSeconds <= 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pConfig->Shards >
		(SIZE_MAX / pConfig->EntriesPerShard) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pTotalCapacity = pConfig->Shards * pConfig->EntriesPerShard;
	iExtraSlots = (pConfig->EntriesPerShard / 2u) +
		(pConfig->EntriesPerShard % 2u);
	if ( pConfig->EntriesPerShard > (SIZE_MAX - iExtraSlots) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pSlotsPerShard = pConfig->EntriesPerShard + iExtraSlots;
	if ( pConfig->Shards > (SIZE_MAX / *pSlotsPerShard) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pTotalSlots = pConfig->Shards * *pSlotsPerShard;
	if ( (*pTotalCapacity >
		 (SIZE_MAX / sizeof(xrt_http_digest_replay_entry))) ||
		(*pTotalCapacity >
		 (SIZE_MAX / sizeof(xrt_http_digest_replay_entry*))) ||
		(*pTotalSlots >
		 (SIZE_MAX / sizeof(xrt_http_digest_replay_entry))) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	return true;
}



/* 释放已初始化的 Mutex 前缀。 */
static void __xrtHttpDigestReplayLocksUnit(
	xhttpdigestreplay* pReplay,
	size_t iShards
)
{
	while ( iShards != 0 ) {
		xrt_http_digest_replay_shard* pShard =
			&pReplay->Shards[--iShards];

		if ( pShard->LockReady ) {
			(void)xrtMutexUnit(&pShard->Lock);
		}
		pShard->LockReady = false;
	}
}



/* 初始化默认容量和五分钟有效期。 */
XRT_API void xrtHttpDigestReplayConfigInit(
	xhttpdigestreplayconfig* pConfig
)
{
	xhttpdigestreplayconfig Config = {
		XRT_HTTP_DIGEST_REPLAY_DEFAULT_SHARDS,
		XRT_HTTP_DIGEST_REPLAY_DEFAULT_ENTRIES,
		XRT_HTTP_DIGEST_REPLAY_DEFAULT_LIFETIME
	};

	if ( (pConfig == NULL) ||
		!__xrtRangeValid(pConfig, sizeof(Config)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 构建可供内置或外部原子存储使用的固定重放键。 */
XRT_API bool xrtHttpDigestReplayKey(
	xstrview Username,
	xstrview Nonce,
	xstrview Cnonce,
	xhttpdigestreplaykey* pKey
)
{
	xsha256 State;
	xhttpdigestreplaykey Key;
	uint8 Length[8];
	uint64 iTotal = sizeof(__xrtHttpDigestReplayDomain) - 1u + 24u;
	bool bResult = false;

	memset(&State, 0, sizeof(State));
	memset(&Key, 0, sizeof(Key));
	memset(Length, 0, sizeof(Length));
	if ( !__xrtHttpViewValid(Username) ||
		!__xrtHttpViewValid(Nonce) ||
		!__xrtHttpViewValid(Cnonce) ||
		(Nonce.Size == 0) || (Cnonce.Size == 0) ||
		(pKey == NULL) || !__xrtRangeValid(pKey, sizeof(Key)) ||
		__xrtRangesOverlap(
			pKey, sizeof(Key), Username.Data, Username.Size
		) || __xrtRangesOverlap(
			pKey, sizeof(Key), Nonce.Data, Nonce.Size
		) || __xrtRangesOverlap(
			pKey, sizeof(Key), Cnonce.Data, Cnonce.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		goto cleanup;
	}
	if ( ((uint64)Username.Size >
		 (XRT_HTTP_DIGEST_REPLAY_MAX_MESSAGE - iTotal)) ) {
		__xrtErrorSetSizeOverflow();
		goto cleanup;
	}
	iTotal += (uint64)Username.Size;
	if ( ((uint64)Nonce.Size >
		 (XRT_HTTP_DIGEST_REPLAY_MAX_MESSAGE - iTotal)) ) {
		__xrtErrorSetSizeOverflow();
		goto cleanup;
	}
	iTotal += (uint64)Nonce.Size;
	if ( ((uint64)Cnonce.Size >
		 (XRT_HTTP_DIGEST_REPLAY_MAX_MESSAGE - iTotal)) ) {
		__xrtErrorSetSizeOverflow();
		goto cleanup;
	}
	xrtSha256Init(&State);
	if ( !xrtSha256Update(
		&State,
		__xrtHttpDigestReplayDomain,
		sizeof(__xrtHttpDigestReplayDomain) - 1u
	) ) {
		goto cleanup;
	}
	__xrtHttpDigestReplayStore64(Length, (uint64)Username.Size);
	if ( !xrtSha256Update(&State, Length, sizeof(Length)) ||
		!xrtSha256Update(&State, Username.Data, Username.Size) ) {
		goto cleanup;
	}
	__xrtHttpDigestReplayStore64(Length, (uint64)Nonce.Size);
	if ( !xrtSha256Update(&State, Length, sizeof(Length)) ||
		!xrtSha256Update(&State, Nonce.Data, Nonce.Size) ) {
		goto cleanup;
	}
	__xrtHttpDigestReplayStore64(Length, (uint64)Cnonce.Size);
	if ( !xrtSha256Update(&State, Length, sizeof(Length)) ||
		!xrtSha256Update(&State, Cnonce.Data, Cnonce.Size) ||
		!xrtSha256Final(&State, Key.Bytes) ) {
		goto cleanup;
	}
	memcpy(pKey, &Key, sizeof(Key));
	bResult = true;

cleanup:
	xrtSecureZero(&State, sizeof(State));
	xrtSecureZero(&Key, sizeof(Key));
	xrtSecureZero(Length, sizeof(Length));
	return bResult;
}



/* 创建全部分片及固定容量槽和过期堆。 */
XRT_API xhttpdigestreplay* xrtHttpDigestReplayCreate(
	const xhttpdigestreplayconfig* pConfig
)
{
	xhttpdigestreplayconfig Config;
	xhttpdigestreplay* pReplay;
	size_t iTotalCapacity;
	size_t iSlotsPerShard;
	size_t iTotalSlots;
	size_t iObjectSize;
	size_t iInitialized = 0;
	xerror* pError;

	if ( !__xrtHttpDigestReplayConfigResolve(
		pConfig, &Config, &iTotalCapacity,
		&iSlotsPerShard, &iTotalSlots
	) ) {
		return NULL;
	}
	if ( Config.Shards >
		((SIZE_MAX - sizeof(*pReplay)) /
		 sizeof(xrt_http_digest_replay_shard)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iObjectSize = sizeof(*pReplay) +
		(Config.Shards * sizeof(xrt_http_digest_replay_shard));
	pReplay = (xhttpdigestreplay*)xrtMalloc(iObjectSize);
	if ( pReplay == NULL ) {
		return NULL;
	}
	memset(pReplay, 0, iObjectSize);
	pReplay->SlotStorage =
		(xrt_http_digest_replay_entry*)xrtMalloc(
			iTotalSlots *
			sizeof(xrt_http_digest_replay_entry)
		);
	if ( pReplay->SlotStorage == NULL ) {
		goto failure;
	}
	memset(
		pReplay->SlotStorage,
		0,
		iTotalSlots * sizeof(xrt_http_digest_replay_entry)
	);
	pReplay->HeapStorage =
		(xrt_http_digest_replay_entry**)xrtMalloc(
			iTotalCapacity *
			sizeof(xrt_http_digest_replay_entry*)
		);
	if ( pReplay->HeapStorage == NULL ) {
		goto failure;
	}
	memset(
		pReplay->HeapStorage,
		0,
		iTotalCapacity * sizeof(xrt_http_digest_replay_entry*)
	);
	pReplay->LifetimeSeconds = Config.LifetimeSeconds;
	pReplay->ShardCount = Config.Shards;
	pReplay->TotalCapacity = iTotalCapacity;
	pReplay->TotalSlots = iTotalSlots;
	for ( size_t i = 0; i < Config.Shards; i++ ) {
		xrt_http_digest_replay_shard* pShard = &pReplay->Shards[i];

		pShard->Slots = pReplay->SlotStorage +
			(i * iSlotsPerShard);
		pShard->Heap = pReplay->HeapStorage +
			(i * Config.EntriesPerShard);
		pShard->Capacity = Config.EntriesPerShard;
		pShard->SlotCapacity = iSlotsPerShard;
		if ( !xrtMutexInit(&pShard->Lock) ) {
			goto failure;
		}
		pShard->LockReady = true;
		iInitialized = i + 1u;
	}
	pReplay->Guard = XRT_HTTP_DIGEST_REPLAY_GUARD;
	return pReplay;

failure:
	pError = xrtTakeError();
	__xrtHttpDigestReplayLocksUnit(pReplay, iInitialized);
	xrtFree(pReplay->HeapStorage);
	xrtFree(pReplay->SlotStorage);
	xrtSecureZero(pReplay, iObjectSize);
	xrtFree(pReplay);
	xrtClearError();
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	} else {
		__xrtErrorSetInternal();
	}
	return NULL;
}



/* 销毁全部分片，同时保持调用线程原有错误不变。 */
XRT_API void xrtHttpDigestReplayDestroy(
	xhttpdigestreplay* pReplay
)
{
	xerror* pSaved;
	size_t iObjectSize;
	size_t iShards;

	if ( pReplay == NULL ) {
		return;
	}
	if ( !__xrtHttpDigestReplayValid(pReplay) ) {
		return;
	}
	pSaved = xrtTakeError();
	iShards = pReplay->ShardCount;
	iObjectSize = sizeof(*pReplay) +
		(iShards * sizeof(xrt_http_digest_replay_shard));
	pReplay->Guard = 0;
	__xrtHttpDigestReplayLocksUnit(pReplay, iShards);
	xrtSecureZero(
		pReplay->SlotStorage,
		pReplay->TotalSlots *
		sizeof(xrt_http_digest_replay_entry)
	);
	xrtFree(pReplay->HeapStorage);
	xrtFree(pReplay->SlotStorage);
	xrtSecureZero(pReplay, iObjectSize);
	xrtFree(pReplay);
	xrtClearError();
	if ( pSaved != NULL ) {
		__xrtErrorSetOwned(pSaved);
	}
}



/* 在一个分片锁内完成过期清理、重放判断和单调更新。 */
XRT_API xhttpdigestreplaycheck xrtHttpDigestReplayCheckKey(
	xhttpdigestreplay* pReplay,
	const xhttpdigestreplaykey* pKey,
	uint32 iNonceCount,
	int64 iIssuedSeconds,
	int64 iNowSeconds
)
{
	xhttpdigestreplaykey Key;
	xrt_http_digest_replay_shard* pShard;
	xrt_http_digest_replay_entry* pEntry;
	xrt_http_digest_replay_entry* pEmpty;
	xhttpdigestreplaycheck Check = XHTTP_DIGEST_REPLAY_ERROR;
	int64 iExpiresSeconds;
	size_t iRemoved;

	if ( !__xrtHttpDigestReplayValid(pReplay) ||
		(pKey == NULL) || !__xrtRangeValid(pKey, sizeof(Key)) ) {
		if ( (pKey == NULL) ||
			((pKey != NULL) && !__xrtRangeValid(pKey, sizeof(Key))) ) {
			__xrtErrorSetInvalidArgument();
		}
		return XHTTP_DIGEST_REPLAY_ERROR;
	}
	if ( (iNonceCount == 0) || (iIssuedSeconds < 0) ||
		(iNowSeconds < 0) ||
		(iIssuedSeconds >
		 (INT64_MAX - pReplay->LifetimeSeconds)) ) {
		__xrtErrorSetValue();
		return XHTTP_DIGEST_REPLAY_ERROR;
	}
	memcpy(&Key, pKey, sizeof(Key));
	iExpiresSeconds = iIssuedSeconds + pReplay->LifetimeSeconds;
	pShard = &pReplay->Shards[
		__xrtHttpDigestReplayShard(pReplay, &Key)
	];
	if ( !xrtMutexLock(&pShard->Lock) ) {
		goto cleanup;
	}
	if ( !__xrtHttpDigestReplayPurgeLocked(
		pShard, iNowSeconds, &iRemoved
	) ) {
		(void)xrtMutexUnlock(&pShard->Lock);
		goto cleanup;
	}
	if ( iNowSeconds > iExpiresSeconds ) {
		__xrtHttpDigestReplayCounterAdd(&pShard->Expired, 1u);
		Check = XHTTP_DIGEST_REPLAY_EXPIRED;
		goto unlock;
	}
	pEntry = __xrtHttpDigestReplayFind(pShard, &Key, &pEmpty);
	if ( pEntry != NULL ) {
		if ( iNonceCount <= pEntry->NonceCount ) {
			__xrtHttpDigestReplayCounterAdd(&pShard->Replayed, 1u);
			Check = XHTTP_DIGEST_REPLAY_REPLAY;
		} else {
			pEntry->NonceCount = iNonceCount;
			__xrtHttpDigestReplayCounterAdd(&pShard->Accepted, 1u);
			Check = XHTTP_DIGEST_REPLAY_ACCEPTED;
		}
		goto unlock;
	}
	if ( (pShard->EntryCount >= pShard->Capacity) ||
		(pEmpty == NULL) ) {
		__xrtHttpDigestReplayCounterAdd(&pShard->Full, 1u);
		Check = XHTTP_DIGEST_REPLAY_FULL;
		goto unlock;
	}
	memcpy(&pEmpty->Key, &Key, sizeof(Key));
	pEmpty->NonceCount = iNonceCount;
	pEmpty->ExpiresSeconds = iExpiresSeconds;
	pEmpty->Occupied = true;
	pShard->EntryCount++;
	__xrtHttpDigestReplayHeapPush(pShard, pEmpty);
	__xrtHttpDigestReplayCounterAdd(&pShard->Accepted, 1u);
	Check = XHTTP_DIGEST_REPLAY_ACCEPTED;

unlock:
	if ( !xrtMutexUnlock(&pShard->Lock) ) {
		Check = XHTTP_DIGEST_REPLAY_ERROR;
	}

cleanup:
	xrtSecureZero(&Key, sizeof(Key));
	return Check;
}



/* 组合公开重放键派生与低层原子检查。 */
XRT_API xhttpdigestreplaycheck xrtHttpDigestReplayCheck(
	xhttpdigestreplay* pReplay,
	xstrview Username,
	xstrview Nonce,
	xstrview Cnonce,
	uint32 iNonceCount,
	int64 iIssuedSeconds,
	int64 iNowSeconds
)
{
	xhttpdigestreplaykey Key;
	xhttpdigestreplaycheck Check;

	memset(&Key, 0, sizeof(Key));
	if ( !xrtHttpDigestReplayKey(
		Username, Nonce, Cnonce, &Key
	) ) {
		return XHTTP_DIGEST_REPLAY_ERROR;
	}
	Check = xrtHttpDigestReplayCheckKey(
		pReplay,
		&Key,
		iNonceCount,
		iIssuedSeconds,
		iNowSeconds
	);
	xrtSecureZero(&Key, sizeof(Key));
	return Check;
}



/* 逐分片删除全部过期记录。 */
XRT_API size_t xrtHttpDigestReplayPurge(
	xhttpdigestreplay* pReplay,
	int64 iNowSeconds
)
{
	size_t iTotal = 0;

	if ( !__xrtHttpDigestReplayValid(pReplay) ||
		(iNowSeconds < 0) ) {
		if ( iNowSeconds < 0 ) {
			__xrtErrorSetValue();
		}
		return 0;
	}
	for ( size_t i = 0; i < pReplay->ShardCount; i++ ) {
		xrt_http_digest_replay_shard* pShard = &pReplay->Shards[i];
		size_t iRemoved;

		if ( !xrtMutexLock(&pShard->Lock) ) {
			return iTotal;
		}
		if ( !__xrtHttpDigestReplayPurgeLocked(
			pShard, iNowSeconds, &iRemoved
		) ) {
			(void)xrtMutexUnlock(&pShard->Lock);
			return iTotal;
		}
		iTotal += iRemoved;
		if ( !xrtMutexUnlock(&pShard->Lock) ) {
			return iTotal;
		}
	}
	return iTotal;
}



/* 清空所有分片并保留已分配容量。 */
XRT_API void xrtHttpDigestReplayClear(
	xhttpdigestreplay* pReplay
)
{
	if ( !__xrtHttpDigestReplayValid(pReplay) ) {
		return;
	}
	for ( size_t i = 0; i < pReplay->ShardCount; i++ ) {
		xrt_http_digest_replay_shard* pShard = &pReplay->Shards[i];

		if ( !xrtMutexLock(&pShard->Lock) ) {
			return;
		}
		memset(
			pShard->Slots,
			0,
			pShard->SlotCapacity * sizeof(*pShard->Slots)
		);
		memset(
			pShard->Heap,
			0,
			pShard->Capacity * sizeof(*pShard->Heap)
		);
		pShard->EntryCount = 0;
		pShard->HeapCount = 0;
		if ( !xrtMutexUnlock(&pShard->Lock) ) {
			return;
		}
	}
}



/* 汇总各分片当前状态和饱和计数。 */
XRT_API bool xrtHttpDigestReplayStats(
	xhttpdigestreplay* pReplay,
	xhttpdigestreplaystats* pStats
)
{
	xhttpdigestreplaystats Stats;

	memset(&Stats, 0, sizeof(Stats));
	if ( !__xrtHttpDigestReplayValid(pReplay) ||
		(pStats == NULL) || !__xrtRangeValid(pStats, sizeof(Stats)) ) {
		if ( (pStats == NULL) ||
			((pStats != NULL) &&
			 !__xrtRangeValid(pStats, sizeof(Stats))) ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	for ( size_t i = 0; i < pReplay->ShardCount; i++ ) {
		xrt_http_digest_replay_shard* pShard = &pReplay->Shards[i];

		if ( !xrtMutexLock(&pShard->Lock) ) {
			return false;
		}
		Stats.Entries += pShard->EntryCount;
		Stats.Capacity += pShard->Capacity;
		__xrtHttpDigestReplayCounterAdd(
			&Stats.Accepted, pShard->Accepted
		);
		__xrtHttpDigestReplayCounterAdd(
			&Stats.Replayed, pShard->Replayed
		);
		__xrtHttpDigestReplayCounterAdd(
			&Stats.Expired, pShard->Expired
		);
		__xrtHttpDigestReplayCounterAdd(
			&Stats.Full, pShard->Full
		);
		__xrtHttpDigestReplayCounterAdd(
			&Stats.Purged, pShard->Purged
		);
		if ( !xrtMutexUnlock(&pShard->Lock) ) {
			return false;
		}
	}
	memcpy(pStats, &Stats, sizeof(Stats));
	return true;
}



#undef XRT_HTTP_DIGEST_REPLAY_GUARD
#undef XRT_HTTP_DIGEST_REPLAY_DEFAULT_SHARDS
#undef XRT_HTTP_DIGEST_REPLAY_DEFAULT_ENTRIES
#undef XRT_HTTP_DIGEST_REPLAY_DEFAULT_LIFETIME
#undef XRT_HTTP_DIGEST_REPLAY_SHARDS_MAX
#undef XRT_HTTP_DIGEST_REPLAY_MAX_MESSAGE

#endif
