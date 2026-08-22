#include "../internal/xrt_net_buffer.h"



#if defined(XRT_FEATURE_NET_BUFFER)

/* 分配连续的拥有型网络字节结果。 */
xnetbytes* __xrtNetBytesAlloc(size_t iSize, xnetwspan* pSpan)
{
	xnetbytes* pBytes;
	size_t iAllocation;

	if ( pSpan == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pSpan->Data = NULL;
	pSpan->Size = 0;
	if ( iSize > (SIZE_MAX - offsetof(xnetbytes, Data)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iAllocation = offsetof(xnetbytes, Data) + iSize;
	pBytes = (xnetbytes*)xrtMalloc(iAllocation);
	if ( pBytes == NULL ) {
		return NULL;
	}
	pBytes->View.Data = pBytes->Data;
	pBytes->View.Size = iSize;
	pBytes->References = 1;
	pSpan->Data = pBytes->Data;
	pSpan->Size = iSize;
	return pBytes;
}



/* 增加拥有型网络字节结果的引用。 */
XRT_API xnetbytes* xrtNetBytesRef(xnetbytes* pBytes)
{
	if ( (pBytes == NULL) ||
		 (xrtRefRetain(&pBytes->References) < 0) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return pBytes;
}



/* 释放拥有型网络字节结果。 */
XRT_API void xrtNetBytesDestroy(xnetbytes* pBytes)
{
	if ( (pBytes != NULL) &&
		 (xrtRefRelease(&pBytes->References) == 0) ) {
		xrtFree(pBytes);
	}
}



/* 返回拥有型网络字节结果的借用视图。 */
XRT_API xbytesview xrtNetBytesView(const xnetbytes* pBytes)
{
	xbytesview Empty = { NULL, 0 };

	return pBytes != NULL ? pBytes->View : Empty;
}



/* 返回拥有块尾部可直接写入的字节数。 */
static size_t __xrtNetBlockWritable(const xnetblock* pBlock)
{
	if ( (pBlock == NULL) || (pBlock->Class == XRT_NET_BLOCK_REF) ||
		(pBlock->Class == XRT_NET_BLOCK_FILE) ||
		(pBlock->End > pBlock->Capacity) ) {
		return 0;
	}
	return pBlock->Capacity - pBlock->End;
}



/* 返回统计使用的块字节数。 */
static size_t __xrtNetBlockBytes(const xnetblock* pBlock)
{
	if ( pBlock->Class == XRT_NET_BLOCK_FILE ) {
		return 0;
	}
	return pBlock->Class == XRT_NET_BLOCK_REF ?
		pBlock->ExternalSize : pBlock->Capacity;
}



/* 选择第一个能够容纳请求的缓存尺寸类。 */
static uint32 __xrtNetBufClass(const xnetbufpool* pPool, size_t iMinimum)
{
	uint32 i;

	if ( pPool == NULL ) {
		return XRT_NET_BLOCK_DYNAMIC;
	}
	for ( i = 0; i < XNET_BUFFER_CLASS_COUNT; i++ ) {
		if ( iMinimum <= pPool->Config.BlockSize[i] ) {
			return i;
		}
	}
	return XRT_NET_BLOCK_DYNAMIC;
}



/* 将一个活动块计入池实时统计。 */
static bool __xrtNetBufPoolAddLive(xnetbufpool* pPool, size_t iBytes)
{
	if ( pPool == NULL ) {
		return true;
	}
	if ( (pPool->Info.LiveBlocks == SIZE_MAX) ||
		(pPool->Info.LiveBytes > (SIZE_MAX - iBytes)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	pPool->Info.LiveBlocks++;
	pPool->Info.LiveBytes += iBytes;
	if ( pPool->Info.LiveBlocks > pPool->Info.PeakBlocks ) {
		pPool->Info.PeakBlocks = pPool->Info.LiveBlocks;
	}
	if ( pPool->Info.LiveBytes > pPool->Info.PeakBytes ) {
		pPool->Info.PeakBytes = pPool->Info.LiveBytes;
	}
	return true;
}



/* 从池实时统计移除一个活动块。 */
static void __xrtNetBufPoolRemoveLive(xnetbufpool* pPool, size_t iBytes)
{
	if ( pPool == NULL ) {
		return;
	}
	if ( pPool->Info.LiveBlocks != 0 ) {
		pPool->Info.LiveBlocks--;
	}
	if ( pPool->Info.LiveBytes >= iBytes ) {
		pPool->Info.LiveBytes -= iBytes;
	} else {
		pPool->Info.LiveBytes = 0;
	}
}



/* 分配一个拥有数据区的块，优先复用最小匹配尺寸类。 */
static xnetblock* __xrtNetBlockAlloc(xnetbufpool* pPool, size_t iMinimum)
{
	xnetblock* pBlock = NULL;
	uint32 iClass;
	size_t iCapacity;
	size_t iAllocation;

	if ( iMinimum == 0 ) {
		iMinimum = 1;
	}
	iClass = __xrtNetBufClass(pPool, iMinimum);
	iCapacity = iClass < XNET_BUFFER_CLASS_COUNT ?
		pPool->Config.BlockSize[iClass] : iMinimum;
	if ( (pPool != NULL) && (iClass < XNET_BUFFER_CLASS_COUNT) &&
		(pPool->Free[iClass] != NULL) ) {
		pBlock = pPool->Free[iClass];
		pPool->Free[iClass] = pBlock->Next;
		pPool->Cached[iClass]--;
		pPool->Info.CachedBlocks--;
		pPool->Info.CachedBytes -= pBlock->Capacity;
		pPool->Info.ReuseCount++;
	}
	if ( pBlock == NULL ) {
		if ( iCapacity > (SIZE_MAX - offsetof(xnetblock, Data)) ) {
			__xrtErrorSetSizeOverflow();
			return NULL;
		}
		iAllocation = offsetof(xnetblock, Data) + iCapacity;
		pBlock = (xnetblock*)xrtMalloc(iAllocation);
		if ( pBlock == NULL ) {
			return NULL;
		}
		if ( pPool != NULL ) {
			pPool->Info.AllocCount++;
			if ( iClass == XRT_NET_BLOCK_DYNAMIC ) {
				pPool->Info.DynamicCount++;
			}
		}
	}
	memset(pBlock, 0, offsetof(xnetblock, Data));
	pBlock->Pool = pPool;
	pBlock->Capacity = iCapacity;
	pBlock->Class = iClass;
	if ( !__xrtNetBufPoolAddLive(pPool, iCapacity) ) {
		xrtFree(pBlock);
		return NULL;
	}
	return pBlock;
}



/* 分配一个只引用外部数据的轻量块头。 */
static xnetblock* __xrtNetBlockAllocRef(xnetbufpool* pPool,
	const void* pData, size_t iSize, xnetreleaseproc pRelease, ptr pContext)
{
	xnetblock* pBlock;

	pBlock = (xnetblock*)xrtMalloc(offsetof(xnetblock, Data));
	if ( pBlock == NULL ) {
		return NULL;
	}
	memset(pBlock, 0, offsetof(xnetblock, Data));
	pBlock->Pool = pPool;
	pBlock->External = (cbytes)pData;
	pBlock->Release = pRelease;
	pBlock->ReleaseContext = pContext;
	pBlock->End = iSize;
	pBlock->ExternalSize = iSize;
	pBlock->Class = XRT_NET_BLOCK_REF;
	if ( !__xrtNetBufPoolAddLive(pPool, iSize) ) {
		xrtFree(pBlock);
		return NULL;
	}
	if ( pPool != NULL ) {
		pPool->Info.AllocCount++;
		pPool->Info.RefCount++;
	}
	return pBlock;
}



/* 释放块，匹配尺寸类在池预算内回到空闲链。 */
static void __xrtNetBlockFree(xnetblock* pBlock)
{
	xnetbufpool* pPool;
	size_t iBytes;

	if ( pBlock == NULL ) {
		return;
	}
	pPool = pBlock->Pool;
	iBytes = __xrtNetBlockBytes(pBlock);
	__xrtNetBufPoolRemoveLive(pPool, iBytes);
	if ( (pBlock->Class == XRT_NET_BLOCK_REF) ||
		(pBlock->Class == XRT_NET_BLOCK_FILE) ) {
		xnetreleaseproc pRelease = pBlock->Release;
		ptr pContext = pBlock->ReleaseContext;
		cbytes pData = pBlock->External;
		size_t iSize = pBlock->ExternalSize;

		xrtFree(pBlock);
		if ( pRelease != NULL ) {
			pRelease(pContext, pData, iSize);
		}
		return;
	}
	if ( (pPool != NULL) && (pBlock->Class < XNET_BUFFER_CLASS_COUNT) &&
		(pBlock->Capacity <= pPool->Config.MaxCacheBytes) &&
		(pPool->Cached[pBlock->Class] <
		 pPool->Config.CacheLimit[pBlock->Class]) &&
		(pPool->Info.CachedBytes <=
		 (pPool->Config.MaxCacheBytes - pBlock->Capacity)) ) {
		uint32 iClass = pBlock->Class;

		pBlock->Next = pPool->Free[iClass];
		pBlock->Begin = 0;
		pBlock->End = 0;
		pPool->Free[iClass] = pBlock;
		pPool->Cached[iClass]++;
		pPool->Info.CachedBlocks++;
		pPool->Info.CachedBytes += pBlock->Capacity;
		return;
	}
	xrtFree(pBlock);
}



/* 将一个非空块挂到缓冲链尾部。 */
static void __xrtNetBufLink(xnetbuf* pBuffer, xnetblock* pBlock)
{
	pBlock->Next = NULL;
	if ( pBuffer->Tail != NULL ) {
		pBuffer->Tail->Next = pBlock;
	} else {
		pBuffer->Head = pBlock;
	}
	pBuffer->Tail = pBlock;
	pBuffer->Blocks++;
}



/* 检查缓冲当前没有未完成的写入预留。 */
static bool __xrtNetBufMutable(xnetbuf* pBuffer, cstr sOperation)
{
	if ( pBuffer == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pBuffer->Reserved != NULL ) {
		__xrtNetSetError(XERR_STATE, XNET_ERROR_BUFFER_STATE,
			sOperation, "network buffer has an active write reservation", 0);
		return false;
	}
	return true;
}



/* 释放由缓冲接管的 XRT 分配。 */
static void __xrtNetBufFreeTaken(ptr pContext, cbytes pData, size_t iSize)
{
	(void)pContext;
	(void)iSize;
	xrtFree((ptr)pData);
}



/* 写入默认缓冲池配置。 */
XRT_API void xrtNetBufPoolConfigInit(xnetbufpoolconfig* pConfig)
{
	if ( pConfig == NULL ) {
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->BlockSize[0] = 512;
	pConfig->BlockSize[1] = 2048;
	pConfig->BlockSize[2] = 8192;
	pConfig->BlockSize[3] = 32768;
	pConfig->CacheLimit[0] = 256;
	pConfig->CacheLimit[1] = 128;
	pConfig->CacheLimit[2] = 64;
	pConfig->CacheLimit[3] = 16;
	pConfig->MaxCacheBytes = 2u * 1024u * 1024u;
}



/* 验证尺寸类严格递增并且缓存预算可安全计算。 */
bool __xrtNetBufPoolConfigValid(const xnetbufpoolconfig* pConfig)
{
	uint32 i;

	if ( pConfig->MaxCacheBytes == 0 ) {
		__xrtNetSetError(XERR_VALUE, XNET_ERROR_BUFFER_STATE,
			"create-buffer-pool", "buffer pool cache budget must be non-zero", 0);
		return false;
	}
	for ( i = 0; i < XNET_BUFFER_CLASS_COUNT; i++ ) {
		if ( (pConfig->BlockSize[i] == 0) ||
			(pConfig->BlockSize[i] >
			 (SIZE_MAX - offsetof(xnetblock, Data))) ||
			((i != 0) &&
			 (pConfig->BlockSize[i] <= pConfig->BlockSize[i - 1])) ) {
			__xrtNetSetError(XERR_VALUE, XNET_ERROR_BUFFER_STATE,
				"create-buffer-pool", "buffer pool classes must be non-zero and strictly increasing", 0);
			return false;
		}
	}
	return true;
}



/* 创建一个线程归属缓冲池。 */
XRT_API xnetbufpool* xrtNetBufPoolCreate(const xnetbufpoolconfig* pConfig)
{
	xnetbufpoolconfig Config;
	xnetbufpool* pPool;

	xrtNetBufPoolConfigInit(&Config);
	if ( pConfig != NULL ) {
		Config = *pConfig;
	}
	if ( !__xrtNetBufPoolConfigValid(&Config) ) {
		return NULL;
	}
	pPool = (xnetbufpool*)xrtCalloc(1, sizeof(*pPool));
	if ( pPool == NULL ) {
		return NULL;
	}
	pPool->Config = Config;
	return pPool;
}



/* 将缓存从大块开始裁剪到指定总字节数。 */
XRT_API size_t xrtNetBufPoolTrim(xnetbufpool* pPool, size_t iRetainBytes)
{
	size_t iFreed = 0;
	uint32 iClass = XNET_BUFFER_CLASS_COUNT;

	if ( pPool == NULL ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	while ( (iClass != 0) && (pPool->Info.CachedBytes > iRetainBytes) ) {
		xnetblock* pBlock;

		iClass--;
		while ( (pPool->Free[iClass] != NULL) &&
			(pPool->Info.CachedBytes > iRetainBytes) ) {
			pBlock = pPool->Free[iClass];
			pPool->Free[iClass] = pBlock->Next;
			pPool->Cached[iClass]--;
			pPool->Info.CachedBlocks--;
			pPool->Info.CachedBytes -= pBlock->Capacity;
			xrtFree(pBlock);
			iFreed++;
		}
	}
	return iFreed;
}



/* 销毁一个已经没有实时块的缓冲池。 */
XRT_API bool xrtNetBufPoolDestroy(xnetbufpool* pPool)
{
	if ( pPool == NULL ) {
		return true;
	}
	if ( pPool->Info.LiveBlocks != 0 ) {
		__xrtNetSetError(XERR_STATE, XNET_ERROR_POOL_BUSY,
			"destroy-buffer-pool", "buffer pool still owns live blocks", 0);
		return false;
	}
	(void)xrtNetBufPoolTrim(pPool, 0);
	xrtFree(pPool);
	return true;
}



/* 复制缓冲池统计。 */
XRT_API void xrtNetBufPoolGet(const xnetbufpool* pPool, xnetbufpoolinfo* pInfo)
{
	if ( pInfo == NULL ) {
		return;
	}
	memset(pInfo, 0, sizeof(*pInfo));
	if ( pPool != NULL ) {
		*pInfo = pPool->Info;
	}
}



/* 初始化一个空缓冲链。 */
XRT_API bool xrtNetBufInit(xnetbuf* pBuffer, xnetbufpool* pPool)
{
	if ( pBuffer == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pBuffer, 0, sizeof(*pBuffer));
	pBuffer->Pool = pPool;
	return true;
}



/* 释放缓冲链全部块和未提交预留。 */
XRT_API void xrtNetBufClear(xnetbuf* pBuffer)
{
	xnetblock* pBlock;

	if ( pBuffer == NULL ) {
		return;
	}
	if ( (pBuffer->Reserved != NULL) && pBuffer->ReservedNew ) {
		__xrtNetBlockFree(pBuffer->Reserved);
	}
	pBuffer->Reserved = NULL;
	pBuffer->ReservedNew = false;
	pBlock = pBuffer->Head;
	while ( pBlock != NULL ) {
		xnetblock* pNext = pBlock->Next;

		__xrtNetBlockFree(pBlock);
		pBlock = pNext;
	}
	pBuffer->Head = NULL;
	pBuffer->Tail = NULL;
	pBuffer->Size = 0;
	pBuffer->Blocks = 0;
}



/* 返回缓冲总字节数。 */
XRT_API size_t xrtNetBufSize(const xnetbuf* pBuffer)
{
	return pBuffer != NULL ? pBuffer->Size : 0;
}



/* 返回缓冲是否为空。 */
XRT_API bool xrtNetBufEmpty(const xnetbuf* pBuffer)
{
	return (pBuffer == NULL) || (pBuffer->Size == 0);
}



/* 返回缓冲当前非空 Span 数。 */
XRT_API size_t xrtNetBufSpanCount(const xnetbuf* pBuffer)
{
	return ((pBuffer != NULL) && (pBuffer->Size != 0)) ?
		pBuffer->Blocks : 0;
}



/* 获取缓冲链前若干只读 Span。 */
XRT_API size_t xrtNetBufSpans(const xnetbuf* pBuffer,
	xnetspan* pSpans, size_t iCapacity)
{
	xnetblock* pBlock;
	size_t iCount = 0;

	if ( (pBuffer == NULL) || ((pSpans == NULL) && (iCapacity != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	pBlock = pBuffer->Head;
	while ( (pBlock != NULL) && (iCount < iCapacity) ) {
		size_t iReadable = __xrtNetBlockReadable(pBlock);

		if ( pBlock->Class == XRT_NET_BLOCK_FILE ) {
			break;
		}
		if ( iReadable != 0 ) {
			pSpans[iCount].Data = __xrtNetBlockData(pBlock) + pBlock->Begin;
			pSpans[iCount].Size = iReadable;
			iCount++;
		}
		pBlock = pBlock->Next;
	}
	return iCount;
}



/* 获取首个连续可读 Span。 */
XRT_API bool xrtNetBufFront(const xnetbuf* pBuffer, xnetspan* pSpan)
{
	if ( (pBuffer == NULL) || (pSpan == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pBuffer->Size == 0 ) {
		pSpan->Data = NULL;
		pSpan->Size = 0;
		return false;
	}
	pSpan->Data = __xrtNetBlockData(pBuffer->Head) + pBuffer->Head->Begin;
	pSpan->Size = __xrtNetBlockReadable(pBuffer->Head);
	return true;
}



/* 原子追加一份数据副本。 */
XRT_API bool xrtNetBufAppend(xnetbuf* pBuffer, const void* pData, size_t iSize)
{
	xnetblock* pNewHead = NULL;
	xnetblock* pNewTail = NULL;
	xnetblock* pBlock;
	cbytes pRead = (cbytes)pData;
	size_t iTailWrite;
	size_t iRemaining;
	size_t iNewBlocks = 0;

	if ( !__xrtNetBufMutable(pBuffer, "append-buffer") ) {
		return false;
	}
	if ( iSize == 0 ) {
		return true;
	}
	if ( pData == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pBuffer->Size > (SIZE_MAX - iSize) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iTailWrite = __xrtNetBlockWritable(pBuffer->Tail);
	if ( iTailWrite > iSize ) {
		iTailWrite = iSize;
	}
	iRemaining = iSize - iTailWrite;
	while ( iRemaining != 0 ) {
		size_t iChunk;

		pBlock = __xrtNetBlockAlloc(pBuffer->Pool, iRemaining);
		if ( pBlock == NULL ) {
			while ( pNewHead != NULL ) {
				xnetblock* pNext = pNewHead->Next;

				__xrtNetBlockFree(pNewHead);
				pNewHead = pNext;
			}
			return false;
		}
		iChunk = iRemaining < pBlock->Capacity ? iRemaining : pBlock->Capacity;
		pBlock->End = iChunk;
		if ( pNewTail != NULL ) {
			pNewTail->Next = pBlock;
		} else {
			pNewHead = pBlock;
		}
		pNewTail = pBlock;
		iNewBlocks++;
		iRemaining -= iChunk;
	}

	if ( iTailWrite != 0 ) {
		memmove(pBuffer->Tail->Data + pBuffer->Tail->End, pRead, iTailWrite);
		pBuffer->Tail->End += iTailWrite;
		pRead += iTailWrite;
	}
	pBlock = pNewHead;
	while ( pBlock != NULL ) {
		size_t iChunk = pBlock->End;

		memmove(pBlock->Data, pRead, iChunk);
		pRead += iChunk;
		pBlock = pBlock->Next;
	}
	if ( pNewHead != NULL ) {
		if ( pBuffer->Tail != NULL ) {
			pBuffer->Tail->Next = pNewHead;
		} else {
			pBuffer->Head = pNewHead;
		}
		pBuffer->Tail = pNewTail;
		pBuffer->Blocks += iNewBlocks;
	}
	pBuffer->Size += iSize;
	return true;
}



/* 为协议封装分配一个头部块并挂到现有链首，不搬移后续负载。 */
XRT_API bool xrtNetBufPrepend(
	xnetbuf* pBuffer,
	const void* pData,
	size_t iSize
)
{
	xnetblock* pBlock;

	if ( !__xrtNetBufMutable(pBuffer, "prepend-buffer") ) {
		return false;
	}
	if ( iSize == 0u ) {
		return true;
	}
	if ( pData == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pBuffer->Size > (SIZE_MAX - iSize) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	pBlock = __xrtNetBlockAlloc(pBuffer->Pool, iSize);
	if ( pBlock == NULL ) {
		return false;
	}
	memcpy(pBlock->Data, pData, iSize);
	pBlock->End = iSize;
	pBlock->Next = pBuffer->Head;
	pBuffer->Head = pBlock;
	if ( pBuffer->Tail == NULL ) {
		pBuffer->Tail = pBlock;
	}
	pBuffer->Size += iSize;
	pBuffer->Blocks++;
	return true;
}



/* 追加一个外部引用块并按成功结果转移释放责任。 */
static bool __xrtNetBufAppendRef(xnetbuf* pBuffer, const void* pData,
	size_t iSize, xnetreleaseproc pRelease, ptr pContext)
{
	xnetblock* pBlock;

	if ( !__xrtNetBufMutable(pBuffer, "append-buffer-reference") ) {
		return false;
	}
	if ( (pData == NULL) || (iSize == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pBuffer->Size > (SIZE_MAX - iSize) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	pBlock = __xrtNetBlockAllocRef(pBuffer->Pool,
		pData, iSize, pRelease, pContext);
	if ( pBlock == NULL ) {
		return false;
	}
	__xrtNetBufLink(pBuffer, pBlock);
	pBuffer->Size += iSize;
	return true;
}



/* 追加一段借用数据。 */
XRT_API bool xrtNetBufAppendBorrow(xnetbuf* pBuffer,
	const void* pData, size_t iSize)
{
	return __xrtNetBufAppendRef(pBuffer, pData, iSize, NULL, NULL);
}



/* 接管一段由 XRT 分配的数据。 */
XRT_API bool xrtNetBufAppendTake(xnetbuf* pBuffer, ptr pData, size_t iSize)
{
	return __xrtNetBufAppendRef(pBuffer, pData, iSize,
		__xrtNetBufFreeTaken, NULL);
}



/* 接管一段带自定义释放过程的数据。 */
XRT_API bool xrtNetBufAppendRef(xnetbuf* pBuffer, const void* pData,
	size_t iSize, xnetreleaseproc pRelease, ptr pContext)
{
	if ( pRelease == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtNetBufAppendRef(pBuffer, pData, iSize, pRelease, pContext);
}



/* 追加文件描述符节点；节点参与缓冲顺序和消费，但不会暴露为内存 Span。 */
bool __xrtNetBufAppendFile(
	xnetbuf* pBuffer,
	ptr pDescriptor,
	size_t iSize,
	xnetreleaseproc pRelease,
	ptr pContext
)
{
	xnetblock* pBlock;

	if ( !__xrtNetBufMutable(pBuffer, "append-buffer-file") ) {
		return false;
	}
	if ( (pDescriptor == NULL) || (iSize == 0) || (pRelease == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pBuffer->Size > (SIZE_MAX - iSize) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	pBlock = __xrtNetBlockAllocRef(
		pBuffer->Pool,
		pDescriptor,
		iSize,
		pRelease,
		pContext
	);
	if ( pBlock == NULL ) {
		return false;
	}
	__xrtNetBufPoolRemoveLive(pBlock->Pool, iSize);
	if ( !__xrtNetBufPoolAddLive(pBlock->Pool, 0) ) {
		xrtFree(pBlock);
		return false;
	}
	pBlock->Class = XRT_NET_BLOCK_FILE;
	__xrtNetBufLink(pBuffer, pBlock);
	pBuffer->Size += iSize;
	return true;
}



/* 查询文件队首而不泄露内部块结构。 */
bool __xrtNetBufFileFront(
	const xnetbuf* pBuffer,
	ptr* pDescriptor,
	size_t* pOffset,
	size_t* pSize
)
{
	xnetblock* pBlock;

	if ( (pBuffer == NULL) || (pDescriptor == NULL) ||
		(pOffset == NULL) || (pSize == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pBlock = pBuffer->Head;
	if ( (pBlock == NULL) || (pBlock->Class != XRT_NET_BLOCK_FILE) ) {
		return false;
	}
	*pDescriptor = (ptr)pBlock->External;
	*pOffset = pBlock->Begin;
	*pSize = __xrtNetBlockReadable(pBlock);
	return true;
}



/* 预留至少给定大小的连续尾部空间。 */
XRT_API bool xrtNetBufReserve(xnetbuf* pBuffer,
	size_t iMinimum, xnetwspan* pSpan)
{
	xnetblock* pBlock;

	if ( (pBuffer == NULL) || (pSpan == NULL) || (iMinimum == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pBuffer->Reserved != NULL ) {
		__xrtNetSetError(XERR_STATE, XNET_ERROR_BUFFER_STATE,
			"reserve-buffer", "network buffer already has a write reservation", 0);
		return false;
	}
	pBlock = pBuffer->Tail;
	if ( __xrtNetBlockWritable(pBlock) >= iMinimum ) {
		pBuffer->Reserved = pBlock;
		pBuffer->ReservedNew = false;
	} else {
		pBlock = __xrtNetBlockAlloc(pBuffer->Pool, iMinimum);
		if ( pBlock == NULL ) {
			return false;
		}
		pBuffer->Reserved = pBlock;
		pBuffer->ReservedNew = true;
	}
	pSpan->Data = pBlock->Data + pBlock->End;
	pSpan->Size = pBlock->Capacity - pBlock->End;
	return true;
}



/* 提交预留空间中的已写字节。 */
XRT_API bool xrtNetBufCommit(xnetbuf* pBuffer, size_t iSize)
{
	xnetblock* pBlock;
	size_t iWritable;

	if ( (pBuffer == NULL) || (pBuffer->Reserved == NULL) ) {
		__xrtNetSetError(XERR_STATE, XNET_ERROR_BUFFER_STATE,
			"commit-buffer", "network buffer has no write reservation", 0);
		return false;
	}
	pBlock = pBuffer->Reserved;
	iWritable = __xrtNetBlockWritable(pBlock);
	if ( iSize > iWritable ) {
		__xrtNetSetError(XERR_RANGE, XNET_ERROR_BUFFER,
			"commit-buffer", "committed byte count exceeds the reservation", 0);
		return false;
	}
	if ( pBuffer->Size > (SIZE_MAX - iSize) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	if ( iSize == 0 ) {
		return xrtNetBufCancel(pBuffer);
	}
	pBlock->End += iSize;
	if ( pBuffer->ReservedNew ) {
		__xrtNetBufLink(pBuffer, pBlock);
	}
	pBuffer->Size += iSize;
	pBuffer->Reserved = NULL;
	pBuffer->ReservedNew = false;
	return true;
}



/* 放弃当前写入预留。 */
XRT_API bool xrtNetBufCancel(xnetbuf* pBuffer)
{
	if ( (pBuffer == NULL) || (pBuffer->Reserved == NULL) ) {
		__xrtNetSetError(XERR_STATE, XNET_ERROR_BUFFER_STATE,
			"cancel-buffer", "network buffer has no write reservation", 0);
		return false;
	}
	if ( pBuffer->ReservedNew ) {
		__xrtNetBlockFree(pBuffer->Reserved);
	}
	pBuffer->Reserved = NULL;
	pBuffer->ReservedNew = false;
	return true;
}



/* 在上层已经验证全部前置条件后移动块链，不重复维护链接细节。 */
void __xrtNetBufMoveTrusted(xnetbuf* pTarget, xnetbuf* pSource)
{
	if ( pSource->Head == NULL ) {
		return;
	}
	if ( pTarget->Tail != NULL ) {
		pTarget->Tail->Next = pSource->Head;
	} else {
		pTarget->Head = pSource->Head;
	}
	pTarget->Tail = pSource->Tail;
	pTarget->Size += pSource->Size;
	pTarget->Blocks += pSource->Blocks;
	pSource->Head = NULL;
	pSource->Tail = NULL;
	pSource->Size = 0;
	pSource->Blocks = 0;
	return;
}



/* 把源缓冲的全部块移动到目标尾部。 */
XRT_API bool xrtNetBufMove(xnetbuf* pTarget, xnetbuf* pSource)
{
	if ( (pTarget == NULL) || (pSource == NULL) || (pTarget == pSource) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtNetBufMutable(pTarget, "move-buffer") ||
		!__xrtNetBufMutable(pSource, "move-buffer") ) {
		return false;
	}
	if ( pTarget->Size > (SIZE_MAX - pSource->Size) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	__xrtNetBufMoveTrusted(pTarget, pSource);
	return true;
}



/* 从指定偏移复制数据而不消费。 */
XRT_API size_t xrtNetBufPeek(const xnetbuf* pBuffer,
	size_t iOffset, void* pOutput, size_t iSize)
{
	xnetblock* pBlock;
	bytes pWrite = (bytes)pOutput;
	size_t iPosition = 0;
	size_t iCopied = 0;

	if ( (pBuffer == NULL) || ((pOutput == NULL) && (iSize != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	if ( (iSize == 0) || (iOffset >= pBuffer->Size) ) {
		return 0;
	}
	pBlock = pBuffer->Head;
	while ( (pBlock != NULL) && (iCopied < iSize) ) {
		size_t iReadable = __xrtNetBlockReadable(pBlock);
		size_t iBegin;
		size_t iChunk;

		if ( (iPosition + iReadable) <= iOffset ) {
			iPosition += iReadable;
			pBlock = pBlock->Next;
			continue;
		}
		iBegin = iOffset > iPosition ? iOffset - iPosition : 0;
		iChunk = iReadable - iBegin;
		if ( iChunk > (iSize - iCopied) ) {
			iChunk = iSize - iCopied;
		}
		memcpy(pWrite + iCopied,
			__xrtNetBlockData(pBlock) + pBlock->Begin + iBegin, iChunk);
		iCopied += iChunk;
		iPosition += iReadable;
		pBlock = pBlock->Next;
	}
	return iCopied;
}



/* 消费缓冲链前若干字节。 */
XRT_API size_t xrtNetBufConsume(xnetbuf* pBuffer, size_t iSize)
{
	size_t iConsumed;
	size_t iRemaining;

	if ( pBuffer == NULL ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	iConsumed = iSize < pBuffer->Size ? iSize : pBuffer->Size;
	iRemaining = iConsumed;
	while ( (pBuffer->Head != NULL) && (iRemaining != 0) ) {
		xnetblock* pBlock = pBuffer->Head;
		size_t iReadable = __xrtNetBlockReadable(pBlock);

		if ( iReadable == 0 ) {
			break;
		}
		if ( iRemaining < iReadable ) {
			pBlock->Begin += iRemaining;
			pBuffer->Size -= iRemaining;
			iRemaining = 0;
			break;
		}
		iRemaining -= iReadable;
		pBuffer->Size -= iReadable;
		if ( pBlock == pBuffer->Reserved ) {
			/* 内核仍借用尾部写区，只清空已提交前缀。 */
			pBlock->Begin = pBlock->End;
			break;
		}
		pBuffer->Head = pBlock->Next;
		pBuffer->Blocks--;
		if ( pBuffer->Head == NULL ) {
			pBuffer->Tail = NULL;
		}
		__xrtNetBlockFree(pBlock);
	}
	if ( pBuffer->Head == NULL ) {
		pBuffer->Tail = NULL;
	}
	return iConsumed;
}



/* 清零拥有块中即将消费的可读前缀，避免认证材料残留在缓存块中。 */
size_t __xrtNetBufConsumeSecure(xnetbuf* pBuffer, size_t iSize)
{
	xnetblock* pBlock;
	size_t iRemaining;
	size_t iConsumed;

	if ( pBuffer == NULL ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	iConsumed = iSize < pBuffer->Size ? iSize : pBuffer->Size;
	iRemaining = iConsumed;
	pBlock = pBuffer->Head;
	while ( (pBlock != NULL) && (iRemaining != 0) ) {
		size_t iReadable = __xrtNetBlockReadable(pBlock);
		size_t iWipe = iRemaining < iReadable ? iRemaining : iReadable;

		if ( (pBlock->Class != XRT_NET_BLOCK_REF) &&
			(pBlock->Class != XRT_NET_BLOCK_FILE) && (iWipe != 0) ) {
			xrtSecureZero(pBlock->Data + pBlock->Begin, iWipe);
		}
		iRemaining -= iWipe;
		pBlock = pBlock->Next;
	}
	return xrtNetBufConsume(pBuffer, iConsumed);
}



/* 清零拥有块的完整存储区后按普通清理语义释放全部块。 */
void __xrtNetBufClearSecure(xnetbuf* pBuffer)
{
	xnetblock* pBlock;

	if ( pBuffer == NULL ) {
		return;
	}
	pBlock = pBuffer->Head;
	while ( pBlock != NULL ) {
		if ( (pBlock->Class != XRT_NET_BLOCK_REF) &&
			(pBlock->Class != XRT_NET_BLOCK_FILE) ) {
			xrtSecureZero(pBlock->Data, pBlock->Capacity);
		}
		pBlock = pBlock->Next;
	}
	if ( (pBuffer->Reserved != NULL) && pBuffer->ReservedNew &&
		(pBuffer->Reserved->Class != XRT_NET_BLOCK_REF) ) {
		xrtSecureZero(
			pBuffer->Reserved->Data,
			pBuffer->Reserved->Capacity
		);
	}
	xrtNetBufClear(pBuffer);
}



/* 复制并消费缓冲链前若干字节。 */
XRT_API size_t xrtNetBufRead(xnetbuf* pBuffer, void* pOutput, size_t iSize)
{
	size_t iRead;

	if ( pBuffer == NULL ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	iRead = xrtNetBufPeek(pBuffer, 0, pOutput, iSize);
	if ( iRead != 0 ) {
		(void)xrtNetBufConsume(pBuffer, iRead);
	}
	return iRead;
}



/* 查找一个字节并返回链内绝对偏移。 */
XRT_API size_t xrtNetBufFind(const xnetbuf* pBuffer,
	uint8 iByte, size_t iOffset)
{
	xnetblock* pBlock;
	size_t iPosition = 0;

	if ( pBuffer == NULL ) {
		return XRT_NPOS;
	}
	pBlock = pBuffer->Head;
	while ( pBlock != NULL ) {
		size_t iReadable = __xrtNetBlockReadable(pBlock);
		size_t iBegin;
		cbytes pFound;

		if ( (iPosition + iReadable) <= iOffset ) {
			iPosition += iReadable;
			pBlock = pBlock->Next;
			continue;
		}
		iBegin = iOffset > iPosition ? iOffset - iPosition : 0;
		pFound = (cbytes)memchr(__xrtNetBlockData(pBlock) +
			pBlock->Begin + iBegin, iByte, iReadable - iBegin);
		if ( pFound != NULL ) {
			return iPosition + (size_t)(pFound -
				(__xrtNetBlockData(pBlock) + pBlock->Begin));
		}
		iPosition += iReadable;
		pBlock = pBlock->Next;
	}
	return XRT_NPOS;
}



/* 确保给定长度的缓冲前缀连续。 */
XRT_API bool xrtNetBufPullup(xnetbuf* pBuffer,
	size_t iSize, xnetspan* pSpan)
{
	xnetblock* pBlock;

	if ( (pBuffer == NULL) || (pSpan == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtNetBufMutable(pBuffer, "pullup-buffer") ) {
		return false;
	}
	if ( iSize > pBuffer->Size ) {
		__xrtNetSetError(XERR_RANGE, XNET_ERROR_BUFFER,
			"pullup-buffer", "requested prefix exceeds buffered data", 0);
		return false;
	}
	if ( iSize == 0 ) {
		pSpan->Data = NULL;
		pSpan->Size = 0;
		return true;
	}
	if ( __xrtNetBlockReadable(pBuffer->Head) >= iSize ) {
		pSpan->Data = __xrtNetBlockData(pBuffer->Head) + pBuffer->Head->Begin;
		pSpan->Size = iSize;
		return true;
	}
	pBlock = __xrtNetBlockAlloc(pBuffer->Pool, iSize);
	if ( pBlock == NULL ) {
		return false;
	}
	if ( xrtNetBufPeek(pBuffer, 0, pBlock->Data, iSize) != iSize ) {
		__xrtNetBlockFree(pBlock);
		__xrtNetSetError(XERR_INTERNAL, XNET_ERROR_BUFFER_STATE,
			"pullup-buffer", "buffer prefix copy violated chain size", 0);
		return false;
	}
	pBlock->End = iSize;
	(void)xrtNetBufConsume(pBuffer, iSize);
	pBlock->Next = pBuffer->Head;
	pBuffer->Head = pBlock;
	if ( pBuffer->Tail == NULL ) {
		pBuffer->Tail = pBlock;
	}
	pBuffer->Size += iSize;
	pBuffer->Blocks++;
	pSpan->Data = pBlock->Data;
	pSpan->Size = iSize;
	return true;
}

#endif
