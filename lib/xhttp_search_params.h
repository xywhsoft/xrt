#ifndef XRT_XHTTP_SEARCH_PARAMS_H
#define XRT_XHTTP_SEARCH_PARAMS_H

/* Owned URLSearchParams and application/x-www-form-urlencoded values. */

typedef struct __xhttp_params_entry {
	size_t iNameOffset;
	size_t iNameLen;
	size_t iValueOffset;
	size_t iValueLen;
	uint32 iFlags;
} __xhttp_params_entry;

struct xrt_http_search_params {
	xhttpsearchparamsconfig tConfig;
	__xhttp_params_entry* pEntries;
	char* pData;
	size_t iCount;
	size_t iEntryCap;
	size_t iDataLen;
	size_t iDataCap;
	size_t iBytes;
	size_t iStoredBytes;
};


static bool __xhttpParamsAddSize(size_t iA, size_t iB, size_t* pOut)
{
	if ( pOut == NULL || iA > (size_t)-1 - iB ) { return false; }
	*pOut = iA + iB;
	return true;
}


static bool __xhttpParamsNameEq(const xhttpsearchparams* pParams, const __xhttp_params_entry* pEntry,
	const char* sName, size_t iNameLen)
{
	return pParams != NULL && pEntry != NULL && sName != NULL && pEntry->iNameLen == iNameLen &&
		(iNameLen == 0u || memcmp(pParams->pData + pEntry->iNameOffset, sName, iNameLen) == 0);
}


static xhttp_semantic_result __xhttpParamsValidatePair(const xhttpsearchparams* pParams,
	const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen, bool bHasValue)
{
	if ( pParams == NULL || sName == NULL || (sValue == NULL && iValueLen != 0u) || (!bHasValue && iValueLen != 0u) ) {
		return XHTTP_SEMANTIC_INVALID_ARGUMENT;
	}
	if ( iNameLen > pParams->tConfig.iMaxNameBytes || iValueLen > pParams->tConfig.iMaxValueBytes ) {
		return XHTTP_SEMANTIC_LIMIT;
	}
	return XHTTP_SEMANTIC_OK;
}


static bool __xhttpParamsAliasesData(const xhttpsearchparams* pParams, const char* pText, size_t iLen)
{
	uintptr_t iText;
	uintptr_t iTextEnd;
	uintptr_t iData;
	uintptr_t iDataEnd;
	if ( pParams == NULL || pParams->pData == NULL || pText == NULL || iLen == 0u ) { return false; }
	iText = (uintptr_t)(const void*)pText;
	iData = (uintptr_t)(const void*)pParams->pData;
	if ( iText > UINTPTR_MAX - iLen || iData > UINTPTR_MAX - pParams->iDataLen ) { return true; }
	iTextEnd = iText + iLen;
	iDataEnd = iData + pParams->iDataLen;
	return iText < iDataEnd && iData < iTextEnd;
}


static xhttp_semantic_result __xhttpParamsStabilizeInputs(const xhttpsearchparams* pParams,
	const char** ppName, size_t iNameLen, const char** ppValue, size_t iValueLen, char** ppStorage)
{
	size_t iBytes;
	char* pStorage;
	if ( ppName == NULL || ppValue == NULL || ppStorage == NULL ) { return XHTTP_SEMANTIC_INVALID_ARGUMENT; }
	*ppStorage = NULL;
	if ( !__xhttpParamsAliasesData(pParams, *ppName, iNameLen) &&
		!__xhttpParamsAliasesData(pParams, *ppValue, iValueLen) ) {
		return XHTTP_SEMANTIC_OK;
	}
	if ( !__xhttpParamsAddSize(iNameLen, iValueLen, &iBytes) ) { return XHTTP_SEMANTIC_LIMIT; }
	pStorage = (char*)xrtMalloc(iBytes ? iBytes : 1u);
	if ( pStorage == NULL ) { return XHTTP_SEMANTIC_OUT_OF_MEMORY; }
	if ( iNameLen != 0u ) { memmove(pStorage, *ppName, iNameLen); }
	if ( iValueLen != 0u ) { memmove(pStorage + iNameLen, *ppValue, iValueLen); }
	*ppName = pStorage;
	*ppValue = pStorage + iNameLen;
	*ppStorage = pStorage;
	return XHTTP_SEMANTIC_OK;
}


static xhttp_semantic_result __xhttpParamsReserveEntries(xhttpsearchparams* pParams, size_t iCount)
{
	size_t iCap;
	__xhttp_params_entry* pNew;
	if ( iCount <= pParams->iEntryCap ) { return XHTTP_SEMANTIC_OK; }
	if ( iCount > pParams->tConfig.iMaxCount || iCount > (size_t)-1 / sizeof(*pNew) ) {
		return XHTTP_SEMANTIC_LIMIT;
	}
	iCap = pParams->iEntryCap ? pParams->iEntryCap : (size_t)pParams->tConfig.iInitialCount;
	if ( iCap == 0u ) { iCap = 8u; }
	while ( iCap < iCount ) {
		size_t iNext = iCap > (size_t)-1 / 2u ? iCount : iCap * 2u;
		if ( iNext > pParams->tConfig.iMaxCount ) { iNext = pParams->tConfig.iMaxCount; }
		if ( iNext <= iCap ) { iCap = iCount; break; }
		iCap = iNext;
	}
	pNew = (__xhttp_params_entry*)xrtRealloc(pParams->pEntries, iCap * sizeof(*pNew));
	if ( pNew == NULL ) { return XHTTP_SEMANTIC_OUT_OF_MEMORY; }
	pParams->pEntries = pNew;
	pParams->iEntryCap = iCap;
	return XHTTP_SEMANTIC_OK;
}


static xhttp_semantic_result __xhttpParamsReserveData(xhttpsearchparams* pParams, size_t iBytes)
{
	size_t iCap;
	char* pNew;
	if ( iBytes <= pParams->iDataCap ) { return XHTTP_SEMANTIC_OK; }
	iCap = pParams->iDataCap ? pParams->iDataCap : pParams->tConfig.iInitialBytes;
	if ( iCap == 0u ) { iCap = 256u; }
	while ( iCap < iBytes ) {
		size_t iNext = iCap > (size_t)-1 / 2u ? iBytes : iCap * 2u;
		if ( iNext <= iCap ) { iCap = iBytes; break; }
		iCap = iNext;
	}
	pNew = (char*)xrtRealloc(pParams->pData, iCap);
	if ( pNew == NULL ) { return XHTTP_SEMANTIC_OUT_OF_MEMORY; }
	pParams->pData = pNew;
	pParams->iDataCap = iCap;
	return XHTTP_SEMANTIC_OK;
}


static xhttp_semantic_result __xhttpParamsStoreEntry(xhttpsearchparams* pParams,
	__xhttp_params_entry* pEntry, const char* sName, size_t iNameLen,
	const char* sValue, size_t iValueLen, bool bHasValue)
{
	size_t iNeed;
	size_t iEnd;
	size_t iStorageLimit;
	xhttp_semantic_result eResult;
	if ( !__xhttpParamsAddSize(iNameLen, iValueLen, &iNeed) ||
		!__xhttpParamsAddSize(iNeed, 2u, &iNeed) ||
		!__xhttpParamsAddSize(pParams->tConfig.iMaxBytes,
			(size_t)pParams->tConfig.iMaxCount * 2u, &iStorageLimit) ) {
		return XHTTP_SEMANTIC_LIMIT;
	}
	if ( !__xhttpParamsAddSize(pParams->iDataLen, iNeed, &iEnd) || iEnd > iStorageLimit ||
		(iEnd > pParams->iDataCap && pParams->iDataLen > pParams->iStoredBytes + 1024u) ) {
		eResult = xrtHttpSearchParamsCompact(pParams);
		if ( eResult != XHTTP_SEMANTIC_OK ) { return eResult; }
		if ( !__xhttpParamsAddSize(pParams->iDataLen, iNeed, &iEnd) || iEnd > iStorageLimit ) {
			return XHTTP_SEMANTIC_LIMIT;
		}
	}
	eResult = __xhttpParamsReserveData(pParams, iEnd);
	if ( eResult != XHTTP_SEMANTIC_OK ) { return eResult; }
	pEntry->iNameOffset = pParams->iDataLen;
	pEntry->iNameLen = iNameLen;
	if ( iNameLen != 0u ) { memcpy(pParams->pData + pParams->iDataLen, sName, iNameLen); }
	pParams->iDataLen += iNameLen;
	pParams->pData[pParams->iDataLen++] = '\0';
	pEntry->iValueOffset = pParams->iDataLen;
	pEntry->iValueLen = iValueLen;
	if ( iValueLen != 0u ) { memcpy(pParams->pData + pParams->iDataLen, sValue, iValueLen); }
	pParams->iDataLen += iValueLen;
	pParams->pData[pParams->iDataLen++] = '\0';
	pEntry->iFlags = bHasValue ? XRT_QUERY_F_HAS_VALUE : XRT_QUERY_F_NONE;
	return XHTTP_SEMANTIC_OK;
}


XXAPI void xrtHttpSearchParamsConfigInit(xhttpsearchparamsconfig* pConfig)
{
	if ( pConfig == NULL ) { return; }
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->iSize = XHTTP_SEARCH_PARAMS_CONFIG_V1_SIZE;
	pConfig->iVersion = XHTTP_SEARCH_PARAMS_CONFIG_VERSION;
	pConfig->iFlags = XHTTP_SEARCH_PARAMS_F_STRICT_PERCENT;
	pConfig->iInitialCount = 8u;
	pConfig->iMaxCount = 4096u;
	pConfig->iInitialBytes = 256u;
	pConfig->iMaxNameBytes = 65536u;
	pConfig->iMaxValueBytes = 1024u * 1024u;
	pConfig->iMaxBytes = 4u * 1024u * 1024u;
}


XXAPI xhttpsearchparams* xrtHttpSearchParamsCreateEx(const xhttpsearchparamsconfig* pConfig)
{
	xhttpsearchparamsconfig tConfig;
	xhttpsearchparams* pParams;
	xrtHttpSearchParamsConfigInit(&tConfig);
	if ( pConfig != NULL ) {
		if ( pConfig->iSize < XHTTP_SEARCH_PARAMS_CONFIG_V1_SIZE ||
			(pConfig->iVersion != 0u && pConfig->iVersion != XHTTP_SEARCH_PARAMS_CONFIG_VERSION) ||
			(pConfig->iFlags & ~XHTTP_SEARCH_PARAMS_F_STRICT_PERCENT) != 0u ||
			pConfig->iMaxCount == 0u || pConfig->iMaxNameBytes == 0u ||
			pConfig->iMaxValueBytes == 0u || pConfig->iMaxBytes == 0u ||
			pConfig->iInitialCount > pConfig->iMaxCount || pConfig->iInitialBytes > pConfig->iMaxBytes ) {
			return NULL;
		}
		tConfig = *pConfig;
	}
	pParams = (xhttpsearchparams*)xrtCalloc(1u, sizeof(*pParams));
	if ( pParams == NULL ) { return NULL; }
	pParams->tConfig = tConfig;
	if ( xrtHttpSearchParamsReserve(pParams, tConfig.iInitialCount, tConfig.iInitialBytes) != XHTTP_SEMANTIC_OK ) {
		xrtHttpSearchParamsDestroy(pParams);
		return NULL;
	}
	return pParams;
}


XXAPI xhttpsearchparams* xrtHttpSearchParamsCreate(void)
{
	return xrtHttpSearchParamsCreateEx(NULL);
}


XXAPI void xrtHttpSearchParamsDestroy(xhttpsearchparams* pParams)
{
	if ( pParams == NULL ) { return; }
	xrtFree(pParams->pEntries);
	xrtFree(pParams->pData);
	memset(pParams, 0, sizeof(*pParams));
	xrtFree(pParams);
}


XXAPI void xrtHttpSearchParamsClear(xhttpsearchparams* pParams)
{
	if ( pParams == NULL ) { return; }
	pParams->iCount = 0u;
	pParams->iDataLen = 0u;
	pParams->iBytes = 0u;
	pParams->iStoredBytes = 0u;
}


XXAPI xhttp_semantic_result xrtHttpSearchParamsReserve(xhttpsearchparams* pParams,
	size_t iCount, size_t iBytes)
{
	size_t iStorageBytes;
	xhttp_semantic_result eResult;
	if ( pParams == NULL ) { return XHTTP_SEMANTIC_INVALID_ARGUMENT; }
	if ( iCount > pParams->tConfig.iMaxCount || iBytes > pParams->tConfig.iMaxBytes ||
		iCount > (size_t)-1 / 2u || !__xhttpParamsAddSize(iBytes, iCount * 2u, &iStorageBytes) ) {
		return XHTTP_SEMANTIC_LIMIT;
	}
	eResult = __xhttpParamsReserveEntries(pParams, iCount);
	if ( eResult != XHTTP_SEMANTIC_OK ) { return eResult; }
	return __xhttpParamsReserveData(pParams, iStorageBytes);
}


XXAPI size_t xrtHttpSearchParamsCount(const xhttpsearchparams* pParams)
{
	return pParams ? pParams->iCount : 0u;
}


XXAPI size_t xrtHttpSearchParamsBytes(const xhttpsearchparams* pParams)
{
	return pParams ? pParams->iBytes : 0u;
}


XXAPI xhttp_semantic_result xrtHttpSearchParamsCompact(xhttpsearchparams* pParams)
{
	char* pNew;
	size_t iCap;
	size_t iOff = 0u;
	size_t i;
	if ( pParams == NULL ) { return XHTTP_SEMANTIC_INVALID_ARGUMENT; }
	if ( pParams->iDataLen == pParams->iStoredBytes ) { return XHTTP_SEMANTIC_OK; }
	if ( pParams->iStoredBytes == 0u ) {
		pParams->iDataLen = 0u;
		return XHTTP_SEMANTIC_OK;
	}
	iCap = pParams->iStoredBytes;
	if ( iCap < pParams->tConfig.iInitialBytes ) { iCap = pParams->tConfig.iInitialBytes; }
	pNew = (char*)xrtMalloc(iCap);
	if ( pNew == NULL ) { return XHTTP_SEMANTIC_OUT_OF_MEMORY; }
	for ( i = 0u; i < pParams->iCount; ++i ) {
		__xhttp_params_entry* pEntry = &pParams->pEntries[i];
		const char* sName = pParams->pData + pEntry->iNameOffset;
		const char* sValue = pParams->pData + pEntry->iValueOffset;
		pEntry->iNameOffset = iOff;
		memcpy(pNew + iOff, sName, pEntry->iNameLen + 1u);
		iOff += pEntry->iNameLen + 1u;
		pEntry->iValueOffset = iOff;
		memcpy(pNew + iOff, sValue, pEntry->iValueLen + 1u);
		iOff += pEntry->iValueLen + 1u;
	}
	xrtFree(pParams->pData);
	pParams->pData = pNew;
	pParams->iDataLen = iOff;
	pParams->iDataCap = iCap;
	return XHTTP_SEMANTIC_OK;
}


XXAPI xhttp_semantic_result xrtHttpSearchParamsAppendN(xhttpsearchparams* pParams,
	const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen, bool bHasValue)
{
	xhttp_semantic_result eResult;
	__xhttp_params_entry tEntry;
	size_t iPairBytes;
	size_t iNewBytes;
	char* pStable = NULL;
	eResult = __xhttpParamsValidatePair(pParams, sName, iNameLen, sValue, iValueLen, bHasValue);
	if ( eResult != XHTTP_SEMANTIC_OK ) { return eResult; }
	if ( pParams->iCount >= pParams->tConfig.iMaxCount ||
		!__xhttpParamsAddSize(iNameLen, iValueLen, &iPairBytes) ||
		!__xhttpParamsAddSize(pParams->iBytes, iPairBytes, &iNewBytes) ||
		iNewBytes > pParams->tConfig.iMaxBytes ) {
		return XHTTP_SEMANTIC_LIMIT;
	}
	eResult = __xhttpParamsStabilizeInputs(pParams, &sName, iNameLen, &sValue, iValueLen, &pStable);
	if ( eResult != XHTTP_SEMANTIC_OK ) { return eResult; }
	eResult = __xhttpParamsReserveEntries(pParams, pParams->iCount + 1u);
	if ( eResult == XHTTP_SEMANTIC_OK ) {
		eResult = __xhttpParamsStoreEntry(pParams, &tEntry, sName, iNameLen, sValue, iValueLen, bHasValue);
	}
	if ( eResult == XHTTP_SEMANTIC_OK ) {
		pParams->pEntries[pParams->iCount++] = tEntry;
		pParams->iBytes = iNewBytes;
		pParams->iStoredBytes += iPairBytes + 2u;
	}
	xrtFree(pStable);
	return eResult;
}


XXAPI xhttp_semantic_result xrtHttpSearchParamsAppend(xhttpsearchparams* pParams,
	const char* sName, const char* sValue)
{
	if ( sName == NULL ) { return XHTTP_SEMANTIC_INVALID_ARGUMENT; }
	return xrtHttpSearchParamsAppendN(pParams, sName, strlen(sName), sValue,
		sValue ? strlen(sValue) : 0u, sValue != NULL);
}


XXAPI xhttp_semantic_result xrtHttpSearchParamsSetN(xhttpsearchparams* pParams,
	const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen, bool bHasValue)
{
	xhttp_semantic_result eResult;
	__xhttp_params_entry tEntry;
	char* pStable = NULL;
	size_t i;
	size_t iFirst = (size_t)-1;
	size_t iMatches = 0u;
	size_t iRemovedBytes = 0u;
	size_t iRemovedStorage = 0u;
	size_t iAddedBytes;
	size_t iNewBytes;
	size_t iNewCount;
	eResult = __xhttpParamsValidatePair(pParams, sName, iNameLen, sValue, iValueLen, bHasValue);
	if ( eResult != XHTTP_SEMANTIC_OK ) { return eResult; }
	eResult = __xhttpParamsStabilizeInputs(pParams, &sName, iNameLen, &sValue, iValueLen, &pStable);
	if ( eResult != XHTTP_SEMANTIC_OK ) { return eResult; }
	for ( i = 0u; i < pParams->iCount; ++i ) {
		const __xhttp_params_entry* pEntry = &pParams->pEntries[i];
		if ( !__xhttpParamsNameEq(pParams, pEntry, sName, iNameLen) ) { continue; }
		if ( iFirst == (size_t)-1 ) { iFirst = i; }
		iMatches++;
		iRemovedBytes += pEntry->iNameLen + pEntry->iValueLen;
		iRemovedStorage += pEntry->iNameLen + pEntry->iValueLen + 2u;
	}
	if ( !__xhttpParamsAddSize(iNameLen, iValueLen, &iAddedBytes) ) {
		xrtFree(pStable);
		return XHTTP_SEMANTIC_LIMIT;
	}
	iNewBytes = pParams->iBytes - iRemovedBytes;
	if ( !__xhttpParamsAddSize(iNewBytes, iAddedBytes, &iNewBytes) || iNewBytes > pParams->tConfig.iMaxBytes ) {
		xrtFree(pStable);
		return XHTTP_SEMANTIC_LIMIT;
	}
	iNewCount = pParams->iCount - iMatches + 1u;
	if ( iNewCount > pParams->tConfig.iMaxCount ) { xrtFree(pStable); return XHTTP_SEMANTIC_LIMIT; }
	eResult = __xhttpParamsReserveEntries(pParams, iNewCount);
	if ( eResult == XHTTP_SEMANTIC_OK && pParams->iDataLen > pParams->iStoredBytes + 1024u &&
		pParams->iDataLen - pParams->iStoredBytes > pParams->iStoredBytes ) {
		eResult = xrtHttpSearchParamsCompact(pParams);
	}
	if ( eResult == XHTTP_SEMANTIC_OK ) {
		eResult = __xhttpParamsStoreEntry(pParams, &tEntry, sName, iNameLen, sValue, iValueLen, bHasValue);
	}
	if ( eResult != XHTTP_SEMANTIC_OK ) { xrtFree(pStable); return eResult; }
	if ( iMatches == 0u ) {
		pParams->pEntries[pParams->iCount++] = tEntry;
	} else {
		size_t iRead;
		size_t iWrite = iFirst + 1u;
		pParams->pEntries[iFirst] = tEntry;
		for ( iRead = iFirst + 1u; iRead < pParams->iCount; ++iRead ) {
			if ( __xhttpParamsNameEq(pParams, &pParams->pEntries[iRead], sName, iNameLen) ) { continue; }
			if ( iWrite != iRead ) { pParams->pEntries[iWrite] = pParams->pEntries[iRead]; }
			iWrite++;
		}
		pParams->iCount = iWrite;
	}
	pParams->iBytes = iNewBytes;
	pParams->iStoredBytes = pParams->iStoredBytes - iRemovedStorage + iAddedBytes + 2u;
	xrtFree(pStable);
	return XHTTP_SEMANTIC_OK;
}


XXAPI xhttp_semantic_result xrtHttpSearchParamsSet(xhttpsearchparams* pParams,
	const char* sName, const char* sValue)
{
	if ( sName == NULL ) { return XHTTP_SEMANTIC_INVALID_ARGUMENT; }
	return xrtHttpSearchParamsSetN(pParams, sName, strlen(sName), sValue,
		sValue ? strlen(sValue) : 0u, sValue != NULL);
}


XXAPI size_t xrtHttpSearchParamsDeleteN(xhttpsearchparams* pParams, const char* sName, size_t iNameLen)
{
	size_t iRead;
	size_t iWrite = 0u;
	size_t iRemoved = 0u;
	if ( pParams == NULL || sName == NULL ) { return 0u; }
	for ( iRead = 0u; iRead < pParams->iCount; ++iRead ) {
		const __xhttp_params_entry* pEntry = &pParams->pEntries[iRead];
		if ( __xhttpParamsNameEq(pParams, pEntry, sName, iNameLen) ) {
			pParams->iBytes -= pEntry->iNameLen + pEntry->iValueLen;
			pParams->iStoredBytes -= pEntry->iNameLen + pEntry->iValueLen + 2u;
			iRemoved++;
			continue;
		}
		if ( iWrite != iRead ) { pParams->pEntries[iWrite] = pParams->pEntries[iRead]; }
		iWrite++;
	}
	pParams->iCount = iWrite;
	if ( pParams->iStoredBytes == 0u ) { pParams->iDataLen = 0u; }
	return iRemoved;
}


XXAPI size_t xrtHttpSearchParamsDelete(xhttpsearchparams* pParams, const char* sName)
{
	return sName ? xrtHttpSearchParamsDeleteN(pParams, sName, strlen(sName)) : 0u;
}


XXAPI size_t xrtHttpSearchParamsCountNameN(const xhttpsearchparams* pParams,
	const char* sName, size_t iNameLen)
{
	size_t i;
	size_t iCount = 0u;
	if ( pParams == NULL || sName == NULL ) { return 0u; }
	for ( i = 0u; i < pParams->iCount; ++i ) {
		if ( __xhttpParamsNameEq(pParams, &pParams->pEntries[i], sName, iNameLen) ) { iCount++; }
	}
	return iCount;
}


XXAPI size_t xrtHttpSearchParamsCountName(const xhttpsearchparams* pParams, const char* sName)
{
	return sName ? xrtHttpSearchParamsCountNameN(pParams, sName, strlen(sName)) : 0u;
}


XXAPI bool xrtHttpSearchParamsHasN(const xhttpsearchparams* pParams,
	const char* sName, size_t iNameLen)
{
	return xrtHttpSearchParamsCountNameN(pParams, sName, iNameLen) != 0u;
}


XXAPI bool xrtHttpSearchParamsHas(const xhttpsearchparams* pParams, const char* sName)
{
	return sName ? xrtHttpSearchParamsHasN(pParams, sName, strlen(sName)) : false;
}


XXAPI bool xrtHttpSearchParamsAt(const xhttpsearchparams* pParams, size_t iIndex, xrtquerypair* pOut)
{
	const __xhttp_params_entry* pEntry;
	if ( pOut != NULL ) { memset(pOut, 0, sizeof(*pOut)); }
	if ( pParams == NULL || pOut == NULL || iIndex >= pParams->iCount ) { return false; }
	pEntry = &pParams->pEntries[iIndex];
	pOut->iFlags = pEntry->iFlags;
	pOut->tKey = xrtStrView(pParams->pData + pEntry->iNameOffset, pEntry->iNameLen);
	pOut->tValue = xrtStrView(pParams->pData + pEntry->iValueOffset, pEntry->iValueLen);
	return true;
}


XXAPI bool xrtHttpSearchParamsGetNthN(const xhttpsearchparams* pParams,
	const char* sName, size_t iNameLen, size_t iNth, xrtstrview* pValue, bool* pHasValue)
{
	size_t i;
	if ( pValue != NULL ) { memset(pValue, 0, sizeof(*pValue)); }
	if ( pHasValue != NULL ) { *pHasValue = false; }
	if ( pParams == NULL || sName == NULL || pValue == NULL ) { return false; }
	for ( i = 0u; i < pParams->iCount; ++i ) {
		const __xhttp_params_entry* pEntry = &pParams->pEntries[i];
		if ( !__xhttpParamsNameEq(pParams, pEntry, sName, iNameLen) ) { continue; }
		if ( iNth-- != 0u ) { continue; }
		*pValue = xrtStrView(pParams->pData + pEntry->iValueOffset, pEntry->iValueLen);
		if ( pHasValue != NULL ) { *pHasValue = (pEntry->iFlags & XRT_QUERY_F_HAS_VALUE) != 0u; }
		return true;
	}
	return false;
}


XXAPI bool xrtHttpSearchParamsGetNth(const xhttpsearchparams* pParams,
	const char* sName, size_t iNth, xrtstrview* pValue, bool* pHasValue)
{
	return sName ? xrtHttpSearchParamsGetNthN(pParams, sName, strlen(sName), iNth, pValue, pHasValue) : false;
}


XXAPI bool xrtHttpSearchParamsGetN(const xhttpsearchparams* pParams,
	const char* sName, size_t iNameLen, xrtstrview* pValue, bool* pHasValue)
{
	return xrtHttpSearchParamsGetNthN(pParams, sName, iNameLen, 0u, pValue, pHasValue);
}


XXAPI bool xrtHttpSearchParamsGet(const xhttpsearchparams* pParams,
	const char* sName, xrtstrview* pValue, bool* pHasValue)
{
	return sName ? xrtHttpSearchParamsGetN(pParams, sName, strlen(sName), pValue, pHasValue) : false;
}


XXAPI xhttpsearchparams* xrtHttpSearchParamsClone(const xhttpsearchparams* pParams)
{
	xhttpsearchparams* pClone;
	size_t i;
	if ( pParams == NULL ) { return NULL; }
	pClone = xrtHttpSearchParamsCreateEx(&pParams->tConfig);
	if ( pClone == NULL ) { return NULL; }
	if ( xrtHttpSearchParamsReserve(pClone, pParams->iCount, pParams->iBytes) != XHTTP_SEMANTIC_OK ) {
		xrtHttpSearchParamsDestroy(pClone);
		return NULL;
	}
	for ( i = 0u; i < pParams->iCount; ++i ) {
		const __xhttp_params_entry* pEntry = &pParams->pEntries[i];
		if ( xrtHttpSearchParamsAppendN(pClone,
			pParams->pData + pEntry->iNameOffset, pEntry->iNameLen,
			pParams->pData + pEntry->iValueOffset, pEntry->iValueLen,
			(pEntry->iFlags & XRT_QUERY_F_HAS_VALUE) != 0u) != XHTTP_SEMANTIC_OK ) {
			xrtHttpSearchParamsDestroy(pClone);
			return NULL;
		}
	}
	return pClone;
}


static int __xhttpParamsHexValue(unsigned char ch)
{
	if ( ch >= '0' && ch <= '9' ) { return (int)(ch - '0'); }
	if ( ch >= 'a' && ch <= 'f' ) { return (int)(ch - 'a') + 10; }
	if ( ch >= 'A' && ch <= 'F' ) { return (int)(ch - 'A') + 10; }
	return -1;
}


static xhttp_semantic_result __xhttpParamsDecode(const char* sText, size_t iLen,
	char* sOut, size_t* pOutLen, bool bStrict, size_t* pErrorOffset)
{
	size_t i = 0u;
	size_t iOut = 0u;
	while ( i < iLen ) {
		unsigned char ch = (unsigned char)sText[i];
		if ( ch == '+' ) {
			sOut[iOut++] = ' ';
			i++;
			continue;
		}
		if ( ch == '%' ) {
			int iHi;
			int iLo;
			if ( i + 2u < iLen && (iHi = __xhttpParamsHexValue((unsigned char)sText[i + 1u])) >= 0 &&
				(iLo = __xhttpParamsHexValue((unsigned char)sText[i + 2u])) >= 0 ) {
				sOut[iOut++] = (char)((iHi << 4) | iLo);
				i += 3u;
				continue;
			}
			if ( bStrict ) {
				if ( pErrorOffset != NULL ) { *pErrorOffset = i; }
				return XHTTP_SEMANTIC_INVALID_SYNTAX;
			}
		}
		sOut[iOut++] = (char)ch;
		i++;
	}
	*pOutLen = iOut;
	return XHTTP_SEMANTIC_OK;
}


XXAPI xhttp_semantic_result xrtHttpSearchParamsParseAppendN(xhttpsearchparams* pParams,
	const char* sText, size_t iLen, size_t* pErrorOffset)
{
	xhttpsearchparams* pWork;
	xhttp_semantic_result eFinal = XHTTP_SEMANTIC_OK;
	size_t iOffset = 0u;
	if ( pErrorOffset != NULL ) { *pErrorOffset = 0u; }
	if ( pParams == NULL || (sText == NULL && iLen != 0u) ) { return XHTTP_SEMANTIC_INVALID_ARGUMENT; }
	pWork = xrtHttpSearchParamsClone(pParams);
	if ( pWork == NULL ) { return XHTTP_SEMANTIC_OUT_OF_MEMORY; }
	if ( iLen != 0u && sText[0] == '?' ) { iOffset = 1u; }
	while ( iOffset < iLen ) {
		size_t iSegmentStart = iOffset;
		size_t iSegmentEnd;
		size_t iEquals;
		size_t iNameRawLen;
		size_t iValueRawLen;
		size_t iNameLen = 0u;
		size_t iValueLen = 0u;
		size_t iDecodeError = 0u;
		size_t iDecodedCap;
		bool bHasValue;
		char* pDecoded;
		while ( iOffset < iLen && sText[iOffset] != '&' ) { iOffset++; }
		iSegmentEnd = iOffset;
		if ( iOffset < iLen ) { iOffset++; }
		if ( iSegmentEnd == iSegmentStart ) { continue; }
		iEquals = iSegmentStart;
		while ( iEquals < iSegmentEnd && sText[iEquals] != '=' ) { iEquals++; }
		bHasValue = iEquals < iSegmentEnd;
		iNameRawLen = iEquals - iSegmentStart;
		iValueRawLen = bHasValue ? iSegmentEnd - iEquals - 1u : 0u;
		if ( !__xhttpParamsAddSize(iNameRawLen, iValueRawLen, &iDecodedCap) ||
			!__xhttpParamsAddSize(iDecodedCap, 2u, &iDecodedCap) ) {
			eFinal = XHTTP_SEMANTIC_LIMIT;
			goto done;
		}
		pDecoded = (char*)xrtMalloc(iDecodedCap);
		if ( pDecoded == NULL ) { eFinal = XHTTP_SEMANTIC_OUT_OF_MEMORY; goto done; }
		eFinal = __xhttpParamsDecode(sText + iSegmentStart, iNameRawLen,
			pDecoded, &iNameLen,
			(pWork->tConfig.iFlags & XHTTP_SEARCH_PARAMS_F_STRICT_PERCENT) != 0u, &iDecodeError);
		if ( eFinal != XHTTP_SEMANTIC_OK ) {
			if ( pErrorOffset != NULL ) { *pErrorOffset = iSegmentStart + iDecodeError; }
			xrtFree(pDecoded);
			goto done;
		}
		if ( bHasValue ) {
			eFinal = __xhttpParamsDecode(sText + iEquals + 1u, iValueRawLen,
				pDecoded + iNameLen, &iValueLen,
				(pWork->tConfig.iFlags & XHTTP_SEARCH_PARAMS_F_STRICT_PERCENT) != 0u, &iDecodeError);
			if ( eFinal != XHTTP_SEMANTIC_OK ) {
				if ( pErrorOffset != NULL ) { *pErrorOffset = iEquals + 1u + iDecodeError; }
				xrtFree(pDecoded);
				goto done;
			}
		}
		eFinal = xrtHttpSearchParamsAppendN(pWork, pDecoded, iNameLen,
			pDecoded + iNameLen, iValueLen, bHasValue);
		xrtFree(pDecoded);
		if ( eFinal != XHTTP_SEMANTIC_OK ) {
			if ( pErrorOffset != NULL ) { *pErrorOffset = iSegmentStart; }
			goto done;
		}
	}
	xrtFree(pParams->pEntries);
	xrtFree(pParams->pData);
	pParams->pEntries = pWork->pEntries;
	pParams->pData = pWork->pData;
	pParams->iCount = pWork->iCount;
	pParams->iEntryCap = pWork->iEntryCap;
	pParams->iDataLen = pWork->iDataLen;
	pParams->iDataCap = pWork->iDataCap;
	pParams->iBytes = pWork->iBytes;
	pParams->iStoredBytes = pWork->iStoredBytes;
	pWork->pEntries = NULL;
	pWork->pData = NULL;

done:
	xrtHttpSearchParamsDestroy(pWork);
	return eFinal;
}


XXAPI xhttp_semantic_result xrtHttpSearchParamsParseAppend(xhttpsearchparams* pParams,
	const char* sText, size_t* pErrorOffset)
{
	if ( sText == NULL ) { return XHTTP_SEMANTIC_INVALID_ARGUMENT; }
	return xrtHttpSearchParamsParseAppendN(pParams, sText, strlen(sText), pErrorOffset);
}


XXAPI xhttpsearchparams* xrtHttpSearchParamsParseN(const char* sText, size_t iLen,
	const xhttpsearchparamsconfig* pConfig, xhttp_semantic_result* pResult, size_t* pErrorOffset)
{
	xhttpsearchparams* pParams;
	xhttp_semantic_result eResult;
	if ( pResult != NULL ) { *pResult = XHTTP_SEMANTIC_INVALID_ARGUMENT; }
	pParams = xrtHttpSearchParamsCreateEx(pConfig);
	if ( pParams == NULL ) {
		if ( pResult != NULL ) { *pResult = XHTTP_SEMANTIC_OUT_OF_MEMORY; }
		return NULL;
	}
	eResult = xrtHttpSearchParamsParseAppendN(pParams, sText, iLen, pErrorOffset);
	if ( pResult != NULL ) { *pResult = eResult; }
	if ( eResult != XHTTP_SEMANTIC_OK ) {
		xrtHttpSearchParamsDestroy(pParams);
		return NULL;
	}
	return pParams;
}


XXAPI xhttpsearchparams* xrtHttpSearchParamsParse(const char* sText,
	const xhttpsearchparamsconfig* pConfig, xhttp_semantic_result* pResult, size_t* pErrorOffset)
{
	if ( sText == NULL ) {
		if ( pResult != NULL ) { *pResult = XHTTP_SEMANTIC_INVALID_ARGUMENT; }
		return NULL;
	}
	return xrtHttpSearchParamsParseN(sText, strlen(sText), pConfig, pResult, pErrorOffset);
}


static int __xhttpParamsCompareEntries(const xhttpsearchparams* pParams,
	const __xhttp_params_entry* pA, const __xhttp_params_entry* pB)
{
	size_t iMin = pA->iNameLen < pB->iNameLen ? pA->iNameLen : pB->iNameLen;
	size_t i;
	const unsigned char* sA = (const unsigned char*)pParams->pData + pA->iNameOffset;
	const unsigned char* sB = (const unsigned char*)pParams->pData + pB->iNameOffset;
	for ( i = 0u; i < iMin; ++i ) {
		if ( sA[i] < sB[i] ) { return -1; }
		if ( sA[i] > sB[i] ) { return 1; }
	}
	if ( pA->iNameLen < pB->iNameLen ) { return -1; }
	if ( pA->iNameLen > pB->iNameLen ) { return 1; }
	return 0;
}


XXAPI xhttp_semantic_result xrtHttpSearchParamsSort(xhttpsearchparams* pParams)
{
	__xhttp_params_entry* pTemp;
	__xhttp_params_entry* pSrc;
	__xhttp_params_entry* pDst;
	size_t iWidth;
	if ( pParams == NULL ) { return XHTTP_SEMANTIC_INVALID_ARGUMENT; }
	if ( pParams->iCount < 2u ) { return XHTTP_SEMANTIC_OK; }
	pTemp = (__xhttp_params_entry*)xrtMalloc(pParams->iCount * sizeof(*pTemp));
	if ( pTemp == NULL ) { return XHTTP_SEMANTIC_OUT_OF_MEMORY; }
	pSrc = pParams->pEntries;
	pDst = pTemp;
	for ( iWidth = 1u; iWidth < pParams->iCount; ) {
		size_t iBase;
		for ( iBase = 0u; iBase < pParams->iCount; iBase += iWidth * 2u ) {
			size_t iLeft = iBase;
			size_t iLeftEnd = iBase + iWidth;
			size_t iRight;
			size_t iRightEnd;
			size_t iOut = iBase;
			if ( iLeftEnd > pParams->iCount ) { iLeftEnd = pParams->iCount; }
			iRight = iLeftEnd;
			iRightEnd = iBase + iWidth * 2u;
			if ( iRightEnd > pParams->iCount ) { iRightEnd = pParams->iCount; }
			while ( iLeft < iLeftEnd && iRight < iRightEnd ) {
				if ( __xhttpParamsCompareEntries(pParams, &pSrc[iLeft], &pSrc[iRight]) <= 0 ) {
					pDst[iOut++] = pSrc[iLeft++];
				} else {
					pDst[iOut++] = pSrc[iRight++];
				}
			}
			while ( iLeft < iLeftEnd ) { pDst[iOut++] = pSrc[iLeft++]; }
			while ( iRight < iRightEnd ) { pDst[iOut++] = pSrc[iRight++]; }
		}
		{
			__xhttp_params_entry* pSwap = pSrc;
			pSrc = pDst;
			pDst = pSwap;
		}
		if ( iWidth > pParams->iCount / 2u ) { break; }
		iWidth *= 2u;
	}
	if ( pSrc != pParams->pEntries ) {
		memcpy(pParams->pEntries, pSrc, pParams->iCount * sizeof(*pSrc));
	}
	xrtFree(pTemp);
	return XHTTP_SEMANTIC_OK;
}


static bool __xhttpParamsKeepFormByte(unsigned char ch)
{
	return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
		(ch >= '0' && ch <= '9') || ch == '*' || ch == '-' || ch == '.' || ch == '_';
}


static bool __xhttpParamsEncodedSize(const char* sText, size_t iLen, size_t* pSize)
{
	size_t i;
	size_t iSize = 0u;
	for ( i = 0u; i < iLen; ++i ) {
		size_t iAdd = (__xhttpParamsKeepFormByte((unsigned char)sText[i]) || sText[i] == ' ') ? 1u : 3u;
		if ( !__xhttpParamsAddSize(iSize, iAdd, &iSize) ) { return false; }
	}
	*pSize = iSize;
	return true;
}


static void __xhttpParamsEncodeTo(const char* sText, size_t iLen, char* sOut, size_t* pOffset)
{
	static const char sHex[] = "0123456789ABCDEF";
	size_t i;
	for ( i = 0u; i < iLen; ++i ) {
		unsigned char ch = (unsigned char)sText[i];
		if ( ch == ' ' ) {
			sOut[(*pOffset)++] = '+';
		} else if ( __xhttpParamsKeepFormByte(ch) ) {
			sOut[(*pOffset)++] = (char)ch;
		} else {
			sOut[(*pOffset)++] = '%';
			sOut[(*pOffset)++] = sHex[(ch >> 4) & 0x0Fu];
			sOut[(*pOffset)++] = sHex[ch & 0x0Fu];
		}
	}
}


XXAPI xhttp_semantic_result xrtHttpSearchParamsBuildTo(const xhttpsearchparams* pParams,
	char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iRequired = 0u;
	size_t iOff = 0u;
	size_t i;
	if ( pOutLen != NULL ) { *pOutLen = 0u; }
	if ( pParams == NULL ) { return XHTTP_SEMANTIC_INVALID_ARGUMENT; }
	for ( i = 0u; i < pParams->iCount; ++i ) {
		const __xhttp_params_entry* pEntry = &pParams->pEntries[i];
		size_t iNameEncoded;
		size_t iValueEncoded;
		if ( !__xhttpParamsEncodedSize(pParams->pData + pEntry->iNameOffset, pEntry->iNameLen, &iNameEncoded) ||
			!__xhttpParamsEncodedSize(pParams->pData + pEntry->iValueOffset, pEntry->iValueLen, &iValueEncoded) ||
			!__xhttpParamsAddSize(iRequired, iNameEncoded, &iRequired) ) {
			return XHTTP_SEMANTIC_LIMIT;
		}
		if ( (pEntry->iFlags & XRT_QUERY_F_HAS_VALUE) != 0u ) {
			if ( !__xhttpParamsAddSize(iRequired, 1u, &iRequired) ||
				!__xhttpParamsAddSize(iRequired, iValueEncoded, &iRequired) ) {
				return XHTTP_SEMANTIC_LIMIT;
			}
		}
		if ( i != 0u && !__xhttpParamsAddSize(iRequired, 1u, &iRequired) ) {
			return XHTTP_SEMANTIC_LIMIT;
		}
	}
	if ( pOutLen != NULL ) { *pOutLen = iRequired; }
	if ( sOut == NULL || iOutCap <= iRequired ) { return XHTTP_SEMANTIC_BUFFER_TOO_SMALL; }
	for ( i = 0u; i < pParams->iCount; ++i ) {
		const __xhttp_params_entry* pEntry = &pParams->pEntries[i];
		if ( i != 0u ) { sOut[iOff++] = '&'; }
		__xhttpParamsEncodeTo(pParams->pData + pEntry->iNameOffset, pEntry->iNameLen, sOut, &iOff);
		if ( (pEntry->iFlags & XRT_QUERY_F_HAS_VALUE) != 0u ) {
			sOut[iOff++] = '=';
			__xhttpParamsEncodeTo(pParams->pData + pEntry->iValueOffset, pEntry->iValueLen, sOut, &iOff);
		}
	}
	sOut[iOff] = '\0';
	return XHTTP_SEMANTIC_OK;
}


XXAPI str xrtHttpSearchParamsBuild(const xhttpsearchparams* pParams, size_t* pOutLen)
{
	xhttp_semantic_result eResult;
	size_t iLen = 0u;
	char* sOut;
	if ( pOutLen != NULL ) { *pOutLen = 0u; }
	eResult = xrtHttpSearchParamsBuildTo(pParams, NULL, 0u, &iLen);
	if ( eResult != XHTTP_SEMANTIC_BUFFER_TOO_SMALL ) { return NULL; }
	sOut = (char*)xrtMalloc(iLen + 1u);
	if ( sOut == NULL ) { return NULL; }
	eResult = xrtHttpSearchParamsBuildTo(pParams, sOut, iLen + 1u, &iLen);
	if ( eResult != XHTTP_SEMANTIC_OK ) {
		xrtFree(sOut);
		return NULL;
	}
	if ( pOutLen != NULL ) { *pOutLen = iLen; }
	return (str)sOut;
}

#endif
