#ifndef XRT_XHTTP_CONTEXT_BODY_H
#define XRT_XHTTP_CONTEXT_BODY_H

struct xrt_http_context {
	volatile int32 iRefCount;
	struct xrt_http_context* pParent;
	xnetcancel* pCancel;
	int64_t iDeadlineMs;
	char* sKey;
	size_t iKeyLen;
	ptr pValue;
	xhttpvaluefree pFreeValue;
};


struct xrt_http_body {
	volatile int32 iRefCount;
	xhttpbodyopenfn Open;
	ptr pFactoryContext;
	xhttpbodyfactoryfreefn FreeFactory;
	int64_t iContentLength;
	uint32 iFlags;
};


struct xrt_http_body_reader {
	xhttpbodyreaderops tOps;
	ptr pContext;
	xhttpbody* pBody;
	bool bClosed;
};


typedef struct __xhttp_bytes_factory {
	const uint8* pData;
	size_t iLen;
	xhttpbodydatafreefn FreeData;
	ptr pFreeContext;
} __xhttp_bytes_factory;


typedef struct __xhttp_bytes_reader {
	const __xhttp_bytes_factory* pFactory;
	size_t iOffset;
} __xhttp_bytes_reader;


static int64_t __xhttpContextNowMs(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		uint64 iNow = (uint64)GetTickCount64();
	#else
		struct timespec tNow;
		uint64 iNow;
		if ( clock_gettime(CLOCK_MONOTONIC, &tNow) != 0 ) { return 0; }
		iNow = (uint64)tNow.tv_sec * 1000u + (uint64)tNow.tv_nsec / 1000000u;
	#endif
	return iNow > (uint64)INT64_MAX ? INT64_MAX : (int64_t)iNow;
}


static xhttpcontext* __xhttpContextCreateNode(xhttpcontext* pParent, int64_t iDeadlineMs)
{
	xhttpcontext* pContext = (xhttpcontext*)xrtCalloc(1u, sizeof(*pContext));
	if ( pContext == NULL ) { return NULL; }
	pContext->iRefCount = 1;
	pContext->pParent = xrtHttpContextRetain(pParent);
	pContext->pCancel = xrtNetCancelCreateChild(pParent ? pParent->pCancel : NULL);
	if ( pContext->pCancel == NULL ) {
		xrtHttpContextRelease(pContext->pParent);
		xrtFree(pContext);
		return NULL;
	}
	if ( iDeadlineMs < 0 ) { iDeadlineMs = 0; }
	if ( pParent != NULL && pParent->iDeadlineMs > 0 &&
		(iDeadlineMs == 0 || pParent->iDeadlineMs < iDeadlineMs) ) {
		iDeadlineMs = pParent->iDeadlineMs;
	}
	pContext->iDeadlineMs = iDeadlineMs;
	return pContext;
}


XXAPI xhttpcontext* xrtHttpContextCreate(void)
{
	return __xhttpContextCreateNode(NULL, 0);
}


XXAPI xhttpcontext* xrtHttpContextFromNet(const xnetcontext* pNetContext)
{
	xhttpcontext* pContext;
	if ( pNetContext != NULL && (pNetContext->iSize < XNET_CONTEXT_V1_SIZE ||
		(pNetContext->iVersion != 0u && pNetContext->iVersion != XNET_CONTEXT_VERSION)) ) {
		return NULL;
	}
	pContext = __xhttpContextCreateNode(NULL, pNetContext ? pNetContext->iDeadlineMs : 0);
	if ( pContext == NULL ) { return NULL; }
	if ( pNetContext != NULL && pNetContext->pCancel != NULL ) {
		xnetcancel* pChild = xrtNetCancelCreateChild(pNetContext->pCancel);
		if ( pChild == NULL ) {
			xrtHttpContextRelease(pContext);
			return NULL;
		}
		xrtNetCancelRelease(pContext->pCancel);
		pContext->pCancel = pChild;
	}
	return pContext;
}


XXAPI xhttpcontext* xrtHttpContextWithCancel(xhttpcontext* pParent)
{
	return __xhttpContextCreateNode(pParent, 0);
}


XXAPI xhttpcontext* xrtHttpContextWithDeadline(xhttpcontext* pParent, int64_t iDeadlineMs)
{
	return __xhttpContextCreateNode(pParent, iDeadlineMs);
}


XXAPI xhttpcontext* xrtHttpContextWithTimeout(xhttpcontext* pParent, uint32 iTimeoutMs)
{
	int64_t iNow = __xhttpContextNowMs();
	int64_t iDeadline;
	if ( iNow > INT64_MAX - (int64_t)iTimeoutMs ) { iDeadline = INT64_MAX; }
	else { iDeadline = iNow + (int64_t)iTimeoutMs; }
	return __xhttpContextCreateNode(pParent, iDeadline);
}


XXAPI xhttpcontext* xrtHttpContextWithValueN(xhttpcontext* pParent, const char* sKey,
	size_t iKeyLen, ptr pValue, xhttpvaluefree pFreeValue)
{
	xhttpcontext* pContext;
	char* sCopy;
	if ( sKey == NULL || iKeyLen == 0u || iKeyLen == (size_t)-1 ) { return NULL; }
	sCopy = (char*)xrtMalloc(iKeyLen + 1u);
	if ( sCopy == NULL ) { return NULL; }
	memcpy(sCopy, sKey, iKeyLen);
	sCopy[iKeyLen] = '\0';
	pContext = __xhttpContextCreateNode(pParent, 0);
	if ( pContext == NULL ) {
		xrtFree(sCopy);
		return NULL;
	}
	pContext->sKey = sCopy;
	pContext->iKeyLen = iKeyLen;
	pContext->pValue = pValue;
	pContext->pFreeValue = pFreeValue;
	return pContext;
}


XXAPI xhttpcontext* xrtHttpContextWithValue(xhttpcontext* pParent, const char* sKey,
	ptr pValue, xhttpvaluefree pFreeValue)
{
	return sKey ? xrtHttpContextWithValueN(pParent, sKey, strlen(sKey), pValue, pFreeValue) : NULL;
}


XXAPI xhttpcontext* xrtHttpContextRetain(xhttpcontext* pContext)
{
	if ( pContext == NULL ) { return NULL; }
	return xrtAtomicRefRetain(&pContext->iRefCount) > 0 ? pContext : NULL;
}


XXAPI void xrtHttpContextRelease(xhttpcontext* pContext)
{
	while ( pContext != NULL && xrtAtomicRefRelease(&pContext->iRefCount) == 0 ) {
		xhttpcontext* pParent = pContext->pParent;
		if ( pContext->pFreeValue != NULL ) { pContext->pFreeValue(pContext->pValue); }
		xrtFree(pContext->sKey);
		xrtNetCancelRelease(pContext->pCancel);
		memset(pContext, 0, sizeof(*pContext));
		xrtFree(pContext);
		pContext = pParent;
	}
}


XXAPI bool xrtHttpContextCancel(xhttpcontext* pContext)
{
	return pContext != NULL && xrtNetCancelRequest(pContext->pCancel);
}


XXAPI bool xrtHttpContextIsCancelled(const xhttpcontext* pContext)
{
	return pContext != NULL && xrtNetCancelIsRequested(pContext->pCancel);
}


XXAPI bool xrtHttpContextIsDone(const xhttpcontext* pContext)
{
	return pContext != NULL && (xrtHttpContextIsCancelled(pContext) ||
		(pContext->iDeadlineMs > 0 && __xhttpContextNowMs() >= pContext->iDeadlineMs));
}


XXAPI int64_t xrtHttpContextDeadline(const xhttpcontext* pContext)
{
	return pContext ? pContext->iDeadlineMs : 0;
}


XXAPI xnetcancel* xrtHttpContextCancelToken(const xhttpcontext* pContext)
{
	return pContext ? pContext->pCancel : NULL;
}


XXAPI ptr xrtHttpContextValueN(const xhttpcontext* pContext, const char* sKey, size_t iKeyLen)
{
	if ( sKey == NULL || iKeyLen == 0u ) { return NULL; }
	while ( pContext != NULL ) {
		if ( pContext->sKey != NULL && pContext->iKeyLen == iKeyLen &&
			memcmp(pContext->sKey, sKey, iKeyLen) == 0 ) {
			return pContext->pValue;
		}
		pContext = pContext->pParent;
	}
	return NULL;
}


XXAPI ptr xrtHttpContextValue(const xhttpcontext* pContext, const char* sKey)
{
	return sKey ? xrtHttpContextValueN(pContext, sKey, strlen(sKey)) : NULL;
}


XXAPI bool xrtHttpContextToNet(const xhttpcontext* pContext, xnetcontext* pOut)
{
	if ( pContext == NULL || pOut == NULL ) { return false; }
	xrtNetContextInit(pOut);
	pOut->iDeadlineMs = pContext->iDeadlineMs;
	pOut->pCancel = pContext->pCancel;
	pOut->pUserData = (ptr)pContext;
	return true;
}


XXAPI void xrtHttpBodyReaderOpsInit(xhttpbodyreaderops* pOps)
{
	if ( pOps == NULL ) { return; }
	memset(pOps, 0, sizeof(*pOps));
	pOps->iSize = XHTTP_BODY_READER_OPS_V1_SIZE;
	pOps->iVersion = XHTTP_BODY_READER_OPS_VERSION;
}


static xhttpbody* __xhttpBodyAllocate(xhttpbodyopenfn pOpen, ptr pFactoryContext,
	xhttpbodyfactoryfreefn pFreeFactory, int64_t iContentLength, uint32 iFlags)
{
	xhttpbody* pBody;
	if ( pOpen == NULL || iContentLength < XHTTP_BODY_LENGTH_UNKNOWN ||
		(iFlags & ~XHTTP_BODY_F_REPLAYABLE) != 0u ) {
		return NULL;
	}
	pBody = (xhttpbody*)xrtCalloc(1u, sizeof(*pBody));
	if ( pBody == NULL ) { return NULL; }
	pBody->iRefCount = 1;
	pBody->Open = pOpen;
	pBody->pFactoryContext = pFactoryContext;
	pBody->FreeFactory = pFreeFactory;
	pBody->iContentLength = iContentLength;
	pBody->iFlags = iFlags;
	return pBody;
}


XXAPI xhttpbody* xrtHttpBodyCreateFactory(xhttpbodyopenfn pOpen, ptr pFactoryContext,
	xhttpbodyfactoryfreefn pFreeFactory, int64_t iContentLength, uint32 iFlags)
{
	return __xhttpBodyAllocate(pOpen, pFactoryContext, pFreeFactory, iContentLength, iFlags);
}


static xhttp_body_read_result __xhttpBytesRead(ptr pContext, void* pBuffer,
	size_t iCapacity, size_t* pRead, const xhttpcontext* pRequestContext)
{
	__xhttp_bytes_reader* pReader = (__xhttp_bytes_reader*)pContext;
	size_t iRemain;
	size_t iDone;
	if ( xrtHttpContextIsDone(pRequestContext) ) { return XHTTP_BODY_READ_CANCELLED; }
	if ( pReader == NULL || pBuffer == NULL || iCapacity == 0u ) { return XHTTP_BODY_READ_ERROR; }
	if ( pReader->iOffset >= pReader->pFactory->iLen ) { return XHTTP_BODY_READ_EOF; }
	iRemain = pReader->pFactory->iLen - pReader->iOffset;
	iDone = iCapacity < iRemain ? iCapacity : iRemain;
	memcpy(pBuffer, pReader->pFactory->pData + pReader->iOffset, iDone);
	pReader->iOffset += iDone;
	*pRead = iDone;
	return XHTTP_BODY_READ_DATA;
}


static void __xhttpBytesReaderClose(ptr pContext)
{
	xrtFree(pContext);
}


static xhttp_semantic_result __xhttpBytesOpen(ptr pFactoryContext,
	const xhttpcontext* pRequestContext, xhttpbodyreaderops* pOutOps, ptr* ppReaderContext)
{
	__xhttp_bytes_reader* pReader;
	if ( pFactoryContext == NULL || pOutOps == NULL || ppReaderContext == NULL ) {
		return XHTTP_SEMANTIC_INVALID_ARGUMENT;
	}
	if ( xrtHttpContextIsDone(pRequestContext) ) { return XHTTP_SEMANTIC_CANCELLED; }
	pReader = (__xhttp_bytes_reader*)xrtCalloc(1u, sizeof(*pReader));
	if ( pReader == NULL ) { return XHTTP_SEMANTIC_OUT_OF_MEMORY; }
	pReader->pFactory = (const __xhttp_bytes_factory*)pFactoryContext;
	xrtHttpBodyReaderOpsInit(pOutOps);
	pOutOps->Read = __xhttpBytesRead;
	pOutOps->Close = __xhttpBytesReaderClose;
	*ppReaderContext = pReader;
	return XHTTP_SEMANTIC_OK;
}


static void __xhttpBytesFactoryFree(ptr pFactoryContext)
{
	__xhttp_bytes_factory* pFactory = (__xhttp_bytes_factory*)pFactoryContext;
	if ( pFactory == NULL ) { return; }
	if ( pFactory->FreeData != NULL ) {
		pFactory->FreeData(pFactory->pFreeContext, pFactory->pData, pFactory->iLen);
	}
	xrtFree(pFactory);
}


static void __xhttpBytesOwnedFree(ptr pContext, const void* pData, size_t iLen)
{
	(void)pContext;
	(void)iLen;
	xrtFree((ptr)pData);
}


XXAPI xhttpbody* xrtHttpBodyCreateBytesRef(const void* pData, size_t iLen,
	xhttpbodydatafreefn pFreeData, ptr pFreeContext)
{
	__xhttp_bytes_factory* pFactory;
	xhttpbody* pBody;
	if ( pData == NULL && iLen != 0u ) { return NULL; }
	if ( iLen > (size_t)INT64_MAX ) { return NULL; }
	pFactory = (__xhttp_bytes_factory*)xrtCalloc(1u, sizeof(*pFactory));
	if ( pFactory == NULL ) { return NULL; }
	pFactory->pData = (const uint8*)pData;
	pFactory->iLen = iLen;
	pFactory->FreeData = pFreeData;
	pFactory->pFreeContext = pFreeContext;
	pBody = __xhttpBodyAllocate(__xhttpBytesOpen, pFactory, __xhttpBytesFactoryFree,
		(int64_t)iLen, XHTTP_BODY_F_REPLAYABLE);
	if ( pBody == NULL ) {
		xrtFree(pFactory);
		return NULL;
	}
	return pBody;
}


XXAPI xhttpbody* xrtHttpBodyCreateBytesCopy(const void* pData, size_t iLen)
{
	void* pCopy = NULL;
	xhttpbody* pBody;
	if ( pData == NULL && iLen != 0u ) { return NULL; }
	if ( iLen != 0u ) {
		pCopy = xrtMalloc(iLen);
		if ( pCopy == NULL ) { return NULL; }
		memcpy(pCopy, pData, iLen);
	}
	pBody = xrtHttpBodyCreateBytesRef(pCopy, iLen, __xhttpBytesOwnedFree, NULL);
	if ( pBody == NULL ) { xrtFree(pCopy); }
	return pBody;
}


XXAPI xhttpbody* xrtHttpBodyCreateBytesOwned(void* pData, size_t iLen)
{
	if ( pData == NULL && iLen != 0u ) { return NULL; }
	return xrtHttpBodyCreateBytesRef(pData, iLen, __xhttpBytesOwnedFree, NULL);
}


XXAPI xhttpbody* xrtHttpBodyCreateEmpty(void)
{
	return xrtHttpBodyCreateBytesRef(NULL, 0u, NULL, NULL);
}


XXAPI xhttpbody* xrtHttpBodyRetain(xhttpbody* pBody)
{
	if ( pBody == NULL ) { return NULL; }
	return xrtAtomicRefRetain(&pBody->iRefCount) > 0 ? pBody : NULL;
}


XXAPI void xrtHttpBodyRelease(xhttpbody* pBody)
{
	if ( pBody == NULL || xrtAtomicRefRelease(&pBody->iRefCount) != 0 ) { return; }
	if ( pBody->FreeFactory != NULL ) { pBody->FreeFactory(pBody->pFactoryContext); }
	memset(pBody, 0, sizeof(*pBody));
	xrtFree(pBody);
}


XXAPI int64_t xrtHttpBodyContentLength(const xhttpbody* pBody)
{
	return pBody ? pBody->iContentLength : XHTTP_BODY_LENGTH_UNKNOWN;
}


XXAPI uint32 xrtHttpBodyFlags(const xhttpbody* pBody)
{
	return pBody ? pBody->iFlags : XHTTP_BODY_F_NONE;
}


XXAPI bool xrtHttpBodyIsReplayable(const xhttpbody* pBody)
{
	return pBody != NULL && (pBody->iFlags & XHTTP_BODY_F_REPLAYABLE) != 0u;
}


XXAPI xhttp_semantic_result xrtHttpBodyOpen(xhttpbody* pBody,
	const xhttpcontext* pRequestContext, xhttpbodyreader** ppReader)
{
	xhttpbodyreaderops tOps;
	ptr pReaderContext = NULL;
	xhttpbodyreader* pReader;
	xhttp_semantic_result eResult;
	if ( ppReader != NULL ) { *ppReader = NULL; }
	if ( pBody == NULL || ppReader == NULL || pBody->Open == NULL ) { return XHTTP_SEMANTIC_INVALID_ARGUMENT; }
	if ( xrtHttpContextIsDone(pRequestContext) ) { return XHTTP_SEMANTIC_CANCELLED; }
	xrtHttpBodyReaderOpsInit(&tOps);
	eResult = pBody->Open(pBody->pFactoryContext, pRequestContext, &tOps, &pReaderContext);
	if ( eResult != XHTTP_SEMANTIC_OK ) { return eResult; }
	if ( tOps.iSize < XHTTP_BODY_READER_OPS_V1_SIZE ||
		(tOps.iVersion != 0u && tOps.iVersion != XHTTP_BODY_READER_OPS_VERSION) || tOps.Read == NULL ) {
		if ( tOps.Close != NULL ) { tOps.Close(pReaderContext); }
		return XHTTP_SEMANTIC_INVALID_ARGUMENT;
	}
	pReader = (xhttpbodyreader*)xrtCalloc(1u, sizeof(*pReader));
	if ( pReader == NULL ) {
		if ( tOps.Close != NULL ) { tOps.Close(pReaderContext); }
		return XHTTP_SEMANTIC_OUT_OF_MEMORY;
	}
	pReader->tOps = tOps;
	pReader->pContext = pReaderContext;
	pReader->pBody = xrtHttpBodyRetain(pBody);
	if ( pReader->pBody == NULL ) {
		if ( tOps.Close != NULL ) { tOps.Close(pReaderContext); }
		xrtFree(pReader);
		return XHTTP_SEMANTIC_INVALID_ARGUMENT;
	}
	*ppReader = pReader;
	return XHTTP_SEMANTIC_OK;
}


XXAPI xhttp_body_read_result xrtHttpBodyReaderRead(xhttpbodyreader* pReader,
	void* pBuffer, size_t iCapacity, size_t* pRead, const xhttpcontext* pRequestContext)
{
	xhttp_body_read_result eResult;
	size_t iRead = 0u;
	if ( pRead != NULL ) { *pRead = 0u; }
	if ( pReader == NULL || pReader->bClosed || pBuffer == NULL || iCapacity == 0u || pRead == NULL ) {
		return XHTTP_BODY_READ_ERROR;
	}
	if ( xrtHttpContextIsDone(pRequestContext) ) { return XHTTP_BODY_READ_CANCELLED; }
	eResult = pReader->tOps.Read(pReader->pContext, pBuffer, iCapacity, &iRead, pRequestContext);
	if ( iRead > iCapacity || (eResult == XHTTP_BODY_READ_DATA && iRead == 0u) ||
		(eResult != XHTTP_BODY_READ_DATA && iRead != 0u) ||
		eResult < XHTTP_BODY_READ_ERROR || eResult > XHTTP_BODY_READ_CANCELLED ) {
		return XHTTP_BODY_READ_ERROR;
	}
	*pRead = iRead;
	return eResult;
}


XXAPI xnetfuture* xrtHttpBodyReaderWaitReadable(xhttpbodyreader* pReader,
	const xhttpcontext* pRequestContext)
{
	if ( pReader == NULL || pReader->bClosed || pReader->tOps.WaitReadable == NULL ||
		xrtHttpContextIsDone(pRequestContext) ) {
		return NULL;
	}
	return pReader->tOps.WaitReadable(pReader->pContext, pRequestContext);
}


XXAPI void xrtHttpBodyReaderClose(xhttpbodyreader* pReader)
{
	if ( pReader == NULL ) { return; }
	if ( !pReader->bClosed ) {
		pReader->bClosed = true;
		if ( pReader->tOps.Close != NULL ) { pReader->tOps.Close(pReader->pContext); }
		pReader->pContext = NULL;
	}
	xrtHttpBodyRelease(pReader->pBody);
	pReader->pBody = NULL;
	xrtFree(pReader);
}

#endif
