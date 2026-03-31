


#ifndef XRT_XNET_MEM_H
#define XRT_XNET_MEM_H

/*
    XRT mainline network memory blocks and chains.

    This header provides:
      - cached small/medium/large block classes for transport and protocol I/O
      - dynamic blocks for oversized payloads
      - ref blocks for zero-copy send and external buffer ownership
      - xnetchain helpers for append, peek, span extraction, and consume
*/


#ifndef XNET_ALLOC
	#define XNET_ALLOC malloc
#endif

#ifndef XNET_FREE
	#define XNET_FREE free
#endif



/* ============================== Block classes ============================== */

#define XNET_MEM_CLASS_SMALL    1u
#define XNET_MEM_CLASS_MEDIUM   2u
#define XNET_MEM_CLASS_LARGE    3u
#define XNET_MEM_CLASS_DYNAMIC  4u
#define XNET_MEM_CLASS_REF      5u

#define XNET_BLK_F_REF          0x0001u

#define __XNET_BLK_MIN_CAPACITY 256u

#if !defined(XRT_BUILD_CORE)

typedef struct __xnet_blk __xnet_blk;



/* ============================== Allocator config ============================== */

typedef struct {
	uint32 iSmallBlockSize;
	uint32 iMediumBlockSize;
	uint32 iLargeBlockSize;
	uint32 iSmallCacheLimit;
	uint32 iMediumCacheLimit;
	uint32 iLargeCacheLimit;
} xnetmemconfig;

typedef struct {
	uint32 iSmallCached;
	uint32 iMediumCached;
	uint32 iLargeCached;
	uint64 iSmallAllocCount;
	uint64 iMediumAllocCount;
	uint64 iLargeAllocCount;
	uint64 iDynamicAllocCount;
	uint64 iRefAllocCount;
	uint64 iSmallReuseCount;
	uint64 iMediumReuseCount;
	uint64 iLargeReuseCount;
	uint64 iDynamicFreeCount;
	uint64 iRefFreeCount;
} xnetmemstats;

struct xrt_net_mem_ctx {
	xnetmemconfig tConfig;
	__xnet_blk* pSmallFree;
	__xnet_blk* pMediumFree;
	__xnet_blk* pLargeFree;
	xnetmemstats tStats;
};



/* ============================== Block model ============================== */

struct xrt_net_chain {
	__xnet_blk* pHead;
	__xnet_blk* pTail;
	ptr pMemCtx;
	uint32 iBytes;
	uint32 iBlockCount;
};
#endif /* !XRT_BUILD_CORE */

struct __xnet_blk {
	__xnet_blk* pNext;
	xnetmemctx* pMemCtx;
	uint16 iClassId;
	uint16 iRefCount;
	uint16 iFlags;
	uint16 iReserved;
	uint32 iBegin;
	uint32 iEnd;
	uint32 iCapacity;
	uint32 iRefLen;
	const uint8* pRefData;
	void (*pfnRelease)(ptr pCtx, const void* pData, size_t iLen);
	ptr pReleaseCtx;
	uint8 aData[1];
};


// 内部函数：__xnetChainSplice
static void __xnetChainSplice(xnetchain* pDst, xnetchain* pSrc)
{
	if ( !pDst || !pSrc || !pSrc->pHead ) return;
	if ( pDst->pTail ) {
		pDst->pTail->pNext = pSrc->pHead;
	} else {
		pDst->pHead = pSrc->pHead;
	}
	pDst->pTail = pSrc->pTail;
	pDst->iBytes += pSrc->iBytes;
	pDst->iBlockCount += pSrc->iBlockCount;
	pSrc->pHead = NULL;
	pSrc->pTail = NULL;
	pSrc->iBytes = 0;
	pSrc->iBlockCount = 0;
}



/* ============================== Config helpers ============================== */

XXAPI void xrtNetMemConfigInit(xnetmemconfig* pCfg)
{
	if ( !pCfg ) return;
	memset(pCfg, 0, sizeof(xnetmemconfig));
	pCfg->iSmallBlockSize = 256;
	pCfg->iMediumBlockSize = 2048;
	pCfg->iLargeBlockSize = 16384;
	pCfg->iSmallCacheLimit = 256;
	pCfg->iMediumCacheLimit = 128;
	pCfg->iLargeCacheLimit = 64;
}


// 内部函数：规范化内存配置
static void __xnetMemNormalizeConfig(xnetmemconfig* pCfg)
{
	if ( !pCfg ) return;
	if ( pCfg->iSmallBlockSize < __XNET_BLK_MIN_CAPACITY ) {
		pCfg->iSmallBlockSize = __XNET_BLK_MIN_CAPACITY;
	}
	if ( pCfg->iMediumBlockSize < pCfg->iSmallBlockSize ) {
		pCfg->iMediumBlockSize = pCfg->iSmallBlockSize;
	}
	if ( pCfg->iLargeBlockSize < pCfg->iMediumBlockSize ) {
		pCfg->iLargeBlockSize = pCfg->iMediumBlockSize;
	}
}


// 初始化网络内存 ctx
XXAPI void xrtNetMemCtxInit(xnetmemctx* pCtx, const xnetmemconfig* pCfg)
{
	if ( !pCtx ) return;
	memset(pCtx, 0, sizeof(xnetmemctx));
	if ( pCfg ) {
		pCtx->tConfig = *pCfg;
	} else {
		xrtNetMemConfigInit(&pCtx->tConfig);
	}
	__xnetMemNormalizeConfig(&pCtx->tConfig);
}



/* ============================== Internal allocator helpers ============================== */

static __xnet_blk* __xnetBlkAllocRaw(size_t iCapacity)
{
	size_t iCap = iCapacity < __XNET_BLK_MIN_CAPACITY ? __XNET_BLK_MIN_CAPACITY : iCapacity;
	__xnet_blk* pBlk = (__xnet_blk*)XNET_ALLOC(sizeof(__xnet_blk) + iCap - 1);
	if ( !pBlk ) return NULL;
	memset(pBlk, 0, sizeof(__xnet_blk));
	pBlk->iRefCount = 1;
	pBlk->iCapacity = (uint32)iCap;
	return pBlk;
}


// 内部函数：内存分类容量相关处理
static uint32 __xnetMemClassCapacity(const xnetmemctx* pCtx, uint16 iClassId)
{
	if ( !pCtx ) return 0;
	switch ( iClassId ) {
		case XNET_MEM_CLASS_SMALL:  return pCtx->tConfig.iSmallBlockSize;
		case XNET_MEM_CLASS_MEDIUM: return pCtx->tConfig.iMediumBlockSize;
		case XNET_MEM_CLASS_LARGE:  return pCtx->tConfig.iLargeBlockSize;
		default: return 0;
	}
}


// 内部函数：统计内存分类 cache
static uint32* __xnetMemClassCacheCount(xnetmemctx* pCtx, uint16 iClassId)
{
	if ( !pCtx ) return NULL;
	switch ( iClassId ) {
		case XNET_MEM_CLASS_SMALL:  return &pCtx->tStats.iSmallCached;
		case XNET_MEM_CLASS_MEDIUM: return &pCtx->tStats.iMediumCached;
		case XNET_MEM_CLASS_LARGE:  return &pCtx->tStats.iLargeCached;
		default: return NULL;
	}
}


// 内部函数：__xnetMemClassCacheLimit
static uint32 __xnetMemClassCacheLimit(const xnetmemctx* pCtx, uint16 iClassId)
{
	if ( !pCtx ) return 0;
	switch ( iClassId ) {
		case XNET_MEM_CLASS_SMALL:  return pCtx->tConfig.iSmallCacheLimit;
		case XNET_MEM_CLASS_MEDIUM: return pCtx->tConfig.iMediumCacheLimit;
		case XNET_MEM_CLASS_LARGE:  return pCtx->tConfig.iLargeCacheLimit;
		default: return 0;
	}
}


// 内部函数：释放内存分类列表
static __xnet_blk** __xnetMemClassFreeList(xnetmemctx* pCtx, uint16 iClassId)
{
	if ( !pCtx ) return NULL;
	switch ( iClassId ) {
		case XNET_MEM_CLASS_SMALL:  return &pCtx->pSmallFree;
		case XNET_MEM_CLASS_MEDIUM: return &pCtx->pMediumFree;
		case XNET_MEM_CLASS_LARGE:  return &pCtx->pLargeFree;
		default: return NULL;
	}
}


// 内部函数：__xnetMemCountAlloc
static void __xnetMemCountAlloc(xnetmemctx* pCtx, uint16 iClassId)
{
	if ( !pCtx ) return;
	switch ( iClassId ) {
		case XNET_MEM_CLASS_SMALL:  pCtx->tStats.iSmallAllocCount++; break;
		case XNET_MEM_CLASS_MEDIUM: pCtx->tStats.iMediumAllocCount++; break;
		case XNET_MEM_CLASS_LARGE:  pCtx->tStats.iLargeAllocCount++; break;
		case XNET_MEM_CLASS_DYNAMIC: pCtx->tStats.iDynamicAllocCount++; break;
		case XNET_MEM_CLASS_REF: pCtx->tStats.iRefAllocCount++; break;
	}
}


// 内部函数：__xnetMemCountReuse
static void __xnetMemCountReuse(xnetmemctx* pCtx, uint16 iClassId)
{
	if ( !pCtx ) return;
	switch ( iClassId ) {
		case XNET_MEM_CLASS_SMALL:  pCtx->tStats.iSmallReuseCount++; break;
		case XNET_MEM_CLASS_MEDIUM: pCtx->tStats.iMediumReuseCount++; break;
		case XNET_MEM_CLASS_LARGE:  pCtx->tStats.iLargeReuseCount++; break;
	}
}


// 内部函数：__xnetMemPickClass
static uint16 __xnetMemPickClass(const xnetmemctx* pCtx, size_t iCapacity)
{
	if ( !pCtx ) return XNET_MEM_CLASS_DYNAMIC;
	if ( iCapacity <= pCtx->tConfig.iSmallBlockSize ) return XNET_MEM_CLASS_SMALL;
	if ( iCapacity <= pCtx->tConfig.iMediumBlockSize ) return XNET_MEM_CLASS_MEDIUM;
	if ( iCapacity <= pCtx->tConfig.iLargeBlockSize ) return XNET_MEM_CLASS_LARGE;
	return XNET_MEM_CLASS_DYNAMIC;
}


// 内部函数：__xnetMemPopCached
static __xnet_blk* __xnetMemPopCached(xnetmemctx* pCtx, uint16 iClassId)
{
	__xnet_blk** ppHead = __xnetMemClassFreeList(pCtx, iClassId);
	uint32* pCached = __xnetMemClassCacheCount(pCtx, iClassId);
	if ( !ppHead || !pCached || !*ppHead ) return NULL;
	
	__xnet_blk* pBlk = *ppHead;
	*ppHead = pBlk->pNext;
	pBlk->pNext = NULL;
	if ( *pCached > 0 ) (*pCached)--;
	__xnetMemCountReuse(pCtx, iClassId);
	return pBlk;
}


// 内部函数：__xnetBlkAllocEx
static __xnet_blk* __xnetBlkAllocEx(xnetmemctx* pCtx, size_t iCapacity)
{
	uint16 iClassId = __xnetMemPickClass(pCtx, iCapacity);
	__xnet_blk* pBlk = NULL;
	
	if ( iClassId != XNET_MEM_CLASS_DYNAMIC ) {
		pBlk = __xnetMemPopCached(pCtx, iClassId);
		if ( !pBlk ) {
			uint32 iCap = __xnetMemClassCapacity(pCtx, iClassId);
			pBlk = __xnetBlkAllocRaw(iCap);
			if ( !pBlk ) return NULL;
			__xnetMemCountAlloc(pCtx, iClassId);
		}
	} else {
		pBlk = __xnetBlkAllocRaw(iCapacity);
		if ( !pBlk ) return NULL;
		__xnetMemCountAlloc(pCtx, XNET_MEM_CLASS_DYNAMIC);
	}
	
	pBlk->pNext = NULL;
	pBlk->pMemCtx = pCtx;
	pBlk->iClassId = iClassId;
	pBlk->iRefCount = 1;
	pBlk->iBegin = 0;
	pBlk->iEnd = 0;
	return pBlk;
}


// 内部函数：__xnetBlkAllocRef
static __xnet_blk* __xnetBlkAllocRef(xnetmemctx* pCtx, const xnetbufref* pRef)
{
	if ( !pRef || !pRef->pData || pRef->iLen == 0 ) return NULL;

	__xnet_blk* pBlk = (__xnet_blk*)XNET_ALLOC(sizeof(__xnet_blk));
	if ( !pBlk ) return NULL;
	memset(pBlk, 0, sizeof(__xnet_blk));
	pBlk->pMemCtx = pCtx;
	pBlk->iClassId = XNET_MEM_CLASS_REF;
	pBlk->iRefCount = 1;
	pBlk->iFlags = XNET_BLK_F_REF;
	pBlk->iBegin = 0;
	pBlk->iEnd = pRef->iLen;
	pBlk->iCapacity = 0;
	pBlk->iRefLen = pRef->iLen;
	pBlk->pRefData = (const uint8*)pRef->pData;
	pBlk->pfnRelease = pRef->pfnRelease;
	pBlk->pReleaseCtx = pRef->pReleaseCtx;
	__xnetMemCountAlloc(pCtx, XNET_MEM_CLASS_REF);
	return pBlk;
}


// 内部函数：__xnetBlkDataPtr
static const uint8* __xnetBlkDataPtr(const __xnet_blk* pBlk)
{
	if ( !pBlk ) return NULL;
	if ( pBlk->iFlags & XNET_BLK_F_REF ) {
		return pBlk->pRefData;
	}
	return pBlk->aData;
}


// 内部函数：__xnetBlkReleaseOne
static void __xnetBlkReleaseOne(__xnet_blk* pBlk)
{
	if ( !pBlk ) return;
	
	xnetmemctx* pCtx = pBlk->pMemCtx;
	uint16 iClassId = pBlk->iClassId;
	if ( pBlk->iFlags & XNET_BLK_F_REF ) {
		if ( pBlk->pfnRelease ) {
			pBlk->pfnRelease(pBlk->pReleaseCtx, pBlk->pRefData, pBlk->iRefLen);
		}
		if ( pCtx ) {
			pCtx->tStats.iRefFreeCount++;
		}
		XNET_FREE(pBlk);
		return;
	}
	if ( pCtx && iClassId != XNET_MEM_CLASS_DYNAMIC ) {
		uint32 iLimit = __xnetMemClassCacheLimit(pCtx, iClassId);
		uint32* pCached = __xnetMemClassCacheCount(pCtx, iClassId);
		__xnet_blk** ppHead = __xnetMemClassFreeList(pCtx, iClassId);
		if ( iLimit > 0 && pCached && ppHead && *pCached < iLimit ) {
			pBlk->pNext = *ppHead;
			pBlk->iBegin = 0;
			pBlk->iEnd = 0;
			pBlk->iRefCount = 1;
			*ppHead = pBlk;
			(*pCached)++;
			return;
		}
	}
	
	if ( pCtx && iClassId == XNET_MEM_CLASS_DYNAMIC ) {
		pCtx->tStats.iDynamicFreeCount++;
	}
	XNET_FREE(pBlk);
}


// 内部函数：__xnetBlkFree
static void __xnetBlkFree(__xnet_blk* pBlk)
{
	if ( !pBlk ) return;
	if ( pBlk->iRefCount > 1 ) {
		pBlk->iRefCount--;
		return;
	}
	__xnetBlkReleaseOne(pBlk);
}


// 内部函数：__xnetBlkReadable
static uint32 __xnetBlkReadable(const __xnet_blk* pBlk)
{
	if ( !pBlk || pBlk->iEnd < pBlk->iBegin ) return 0;
	return pBlk->iEnd - pBlk->iBegin;
}


// 内部函数：__xnetBlkWritable
static uint32 __xnetBlkWritable(const __xnet_blk* pBlk)
{
	if ( !pBlk || (pBlk->iFlags & XNET_BLK_F_REF) || pBlk->iEnd > pBlk->iCapacity ) return 0;
	return pBlk->iCapacity - pBlk->iEnd;
}


// 内部函数：__xnetChainLinkBlock
static void __xnetChainLinkBlock(xnetchain* pChain, __xnet_blk* pBlk)
{
	if ( !pChain || !pBlk ) return;
	if ( pChain->pTail ) {
		pChain->pTail->pNext = pBlk;
	} else {
		pChain->pHead = pBlk;
	}
	pChain->pTail = pBlk;
	pChain->iBlockCount++;
	pChain->iBytes += __xnetBlkReadable(pBlk);
}


// 内部函数：裁剪内存列表
static void __xnetMemTrimList(__xnet_blk** ppHead, uint32* pCached, uint32 iTarget)
{
	if ( !ppHead || !pCached ) return;
	while ( *ppHead && *pCached > iTarget ) {
		__xnet_blk* pBlk = *ppHead;
		*ppHead = pBlk->pNext;
		pBlk->pNext = NULL;
		XNET_FREE(pBlk);
		(*pCached)--;
	}
}



/* ============================== Allocator public helpers ============================== */

XXAPI void xrtNetMemCtxTrim(xnetmemctx* pCtx)
{
	if ( !pCtx ) return;
	__xnetMemTrimList(&pCtx->pSmallFree, &pCtx->tStats.iSmallCached, 0);
	__xnetMemTrimList(&pCtx->pMediumFree, &pCtx->tStats.iMediumCached, 0);
	__xnetMemTrimList(&pCtx->pLargeFree, &pCtx->tStats.iLargeCached, 0);
}


// 释放网络内存 ctx
XXAPI void xrtNetMemCtxUnit(xnetmemctx* pCtx)
{
	if ( !pCtx ) return;
	xrtNetMemCtxTrim(pCtx);
	memset(pCtx, 0, sizeof(xnetmemctx));
}


// xrtNetMemCtxGetStats 相关处理
XXAPI void xrtNetMemCtxGetStats(const xnetmemctx* pCtx, xnetmemstats* pStats)
{
	if ( !pStats ) return;
	memset(pStats, 0, sizeof(xnetmemstats));
	if ( !pCtx ) return;
	*pStats = pCtx->tStats;
}



/* ============================== Chain lifecycle ============================== */

XXAPI void xrtNetChainInitEx(xnetchain* pChain, xnetmemctx* pMemCtx)
{
	if ( !pChain ) return;
	memset(pChain, 0, sizeof(xnetchain));
	pChain->pMemCtx = pMemCtx;
}


// xrtNetChainInit 相关处理
XXAPI void xrtNetChainInit(xnetchain* pChain)
{
	xrtNetChainInitEx(pChain, NULL);
}


// xrtNetChainClear 相关处理
XXAPI void xrtNetChainClear(xnetchain* pChain)
{
	if ( !pChain ) return;
	while ( pChain->pHead ) {
		__xnet_blk* pNext = pChain->pHead->pNext;
		__xnetBlkFree(pChain->pHead);
		pChain->pHead = pNext;
	}
	pChain->pTail = NULL;
	pChain->iBytes = 0;
	pChain->iBlockCount = 0;
}



/* ============================== Chain append ============================== */

XXAPI bool xrtNetChainAppendCopy(xnetchain* pChain, const void* pData, size_t iLen)
{
	if ( !pChain || !pData || iLen == 0 ) return false;
	
	xnetmemctx* pMemCtx = (xnetmemctx*)pChain->pMemCtx;
	const uint8* pSrc = (const uint8*)pData;
	size_t iLeft = iLen;
	
	while ( iLeft > 0 ) {
		__xnet_blk* pTail = pChain->pTail;
		if ( pTail && __xnetBlkWritable(pTail) > 0 ) {
			uint32 iWritable = __xnetBlkWritable(pTail);
			uint32 iChunk = (uint32)(iLeft < iWritable ? iLeft : iWritable);
			memcpy(pTail->aData + pTail->iEnd, pSrc, iChunk);
			pTail->iEnd += iChunk;
			pChain->iBytes += iChunk;
			pSrc += iChunk;
			iLeft -= iChunk;
			continue;
		}
		
		__xnet_blk* pBlk = __xnetBlkAllocEx(pMemCtx, iLeft);
		if ( !pBlk ) return false;
		
		uint32 iChunk = (uint32)(iLeft < pBlk->iCapacity ? iLeft : pBlk->iCapacity);
		memcpy(pBlk->aData, pSrc, iChunk);
		pBlk->iBegin = 0;
		pBlk->iEnd = iChunk;
		__xnetChainLinkBlock(pChain, pBlk);
		
		pSrc += iChunk;
		iLeft -= iChunk;
	}
	
	return true;
}


// xrtNetChainAppendRef 相关处理
XXAPI bool xrtNetChainAppendRef(xnetchain* pChain, const xnetbufref* pRef)
{
	if ( !pChain || !pRef || !pRef->pData || pRef->iLen == 0 ) return false;

	xnetmemctx* pMemCtx = (xnetmemctx*)pChain->pMemCtx;
	__xnet_blk* pBlk = __xnetBlkAllocRef(pMemCtx, pRef);
	if ( !pBlk ) return false;
	__xnetChainLinkBlock(pChain, pBlk);
	return true;
}



/* ============================== Public chain queries ============================== */

XXAPI size_t xrtNetChainBytes(const xnetchain* pChain)
{
	return pChain ? pChain->iBytes : 0;
}


// xrtNetChainSpanCount 相关处理
XXAPI uint32 xrtNetChainSpanCount(const xnetchain* pChain)
{
	if ( !pChain ) return 0;
	uint32 iCount = 0;
	for ( __xnet_blk* pBlk = pChain->pHead; pBlk; pBlk = pBlk->pNext ) {
		if ( __xnetBlkReadable(pBlk) > 0 ) iCount++;
	}
	return iCount;
}


// xrtNetChainGetSpans 相关处理
XXAPI uint32 xrtNetChainGetSpans(const xnetchain* pChain, xnetspan* pOut, uint32 iMaxCount)
{
	if ( !pChain || !pOut || iMaxCount == 0 ) return 0;
	
	uint32 iCount = 0;
	for ( __xnet_blk* pBlk = pChain->pHead; pBlk && iCount < iMaxCount; pBlk = pBlk->pNext ) {
		uint32 iReadable = __xnetBlkReadable(pBlk);
		if ( iReadable == 0 ) continue;
		pOut[iCount].pData = __xnetBlkDataPtr(pBlk) + pBlk->iBegin;
		pOut[iCount].iLen = iReadable;
		iCount++;
	}
	return iCount;
}


// xrtNetChainPeek 相关处理
XXAPI size_t xrtNetChainPeek(const xnetchain* pChain, ptr pOut, size_t iLen)
{
	if ( !pChain || !pOut || iLen == 0 ) return 0;
	
	uint8* pDst = (uint8*)pOut;
	size_t iCopied = 0;
	
	for ( __xnet_blk* pBlk = pChain->pHead; pBlk && iCopied < iLen; pBlk = pBlk->pNext ) {
		uint32 iReadable = __xnetBlkReadable(pBlk);
		if ( iReadable == 0 ) continue;
		
		size_t iChunk = (iLen - iCopied) < iReadable ? (iLen - iCopied) : iReadable;
		memcpy(pDst + iCopied, __xnetBlkDataPtr(pBlk) + pBlk->iBegin, iChunk);
		iCopied += iChunk;
	}
	
	return iCopied;
}


// xrtNetChainFindByte 相关处理
XXAPI size_t xrtNetChainFindByte(const xnetchain* pChain, uint8 ch, size_t iStartOff)
{
	if ( !pChain || iStartOff >= pChain->iBytes ) return (size_t)-1;
	
	size_t iOffset = 0;
	
	for ( __xnet_blk* pBlk = pChain->pHead; pBlk; pBlk = pBlk->pNext ) {
		uint32 iReadable = __xnetBlkReadable(pBlk);
		if ( iReadable == 0 ) continue;
		
		if ( iOffset + iReadable <= iStartOff ) {
			iOffset += iReadable;
			continue;
		}
		
		size_t iBegin = iStartOff > iOffset ? (iStartOff - iOffset) : 0;
		const uint8* pFound = (const uint8*)memchr(__xnetBlkDataPtr(pBlk) + pBlk->iBegin + iBegin, ch, iReadable - iBegin);
		if ( pFound ) {
			return iOffset + (size_t)(pFound - (__xnetBlkDataPtr(pBlk) + pBlk->iBegin));
		}
		
		iOffset += iReadable;
	}
	
	return (size_t)-1;
}



/* ============================== Public chain consume ============================== */

XXAPI void xrtNetChainConsume(xnetchain* pChain, size_t iLen)
{
	if ( !pChain || iLen == 0 ) return;
	if ( iLen >= pChain->iBytes ) {
		xrtNetChainClear(pChain);
		return;
	}
	
	size_t iLeft = iLen;
	while ( pChain->pHead && iLeft > 0 ) {
		__xnet_blk* pBlk = pChain->pHead;
		uint32 iReadable = __xnetBlkReadable(pBlk);
		
		if ( iReadable == 0 ) {
			pChain->pHead = pBlk->pNext;
			if ( pChain->pTail == pBlk ) pChain->pTail = NULL;
			__xnetBlkFree(pBlk);
			if ( pChain->iBlockCount > 0 ) pChain->iBlockCount--;
			continue;
		}
		
		if ( iLeft < iReadable ) {
			pBlk->iBegin += (uint32)iLeft;
			pChain->iBytes -= (uint32)iLeft;
			iLeft = 0;
			break;
		}
		
		iLeft -= iReadable;
		pChain->iBytes -= iReadable;
		pChain->pHead = pBlk->pNext;
		if ( pChain->pTail == pBlk ) pChain->pTail = NULL;
		__xnetBlkFree(pBlk);
		if ( pChain->iBlockCount > 0 ) pChain->iBlockCount--;
	}
}


#endif
