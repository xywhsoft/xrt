#ifndef XRT_XHTTP_SEMANTICS_H
#define XRT_XHTTP_SEMANTICS_H

/*
 * Protocol-independent HTTP value objects.
 *
 * Public declarations live in xrt.h. This implementation is expanded after
 * xhttp_util.h so the strict token and header-line validators can be reused.
 */

typedef struct __xhttp_headers_entry {
	size_t iNameOffset;
	size_t iNameLen;
	size_t iValueOffset;
	size_t iValueLen;
} __xhttp_headers_entry;

struct xrt_http_headers {
	xhttpheadersconfig tConfig;
	__xhttp_headers_entry* pEntries;
	char* pData;
	size_t iCount;
	size_t iEntryCap;
	size_t iDataLen;
	size_t iDataCap;
	size_t iBytes;
	size_t iStoredBytes;
};


static bool __xhttpSemAddSize(size_t iA, size_t iB, size_t* pOut)
{
	if ( pOut == NULL || iA > (size_t)-1 - iB ) { return false; }
	*pOut = iA + iB;
	return true;
}


static char __xhttpSemLower(char ch)
{
	return (ch >= 'A' && ch <= 'Z') ? (char)(ch + ('a' - 'A')) : ch;
}


static bool __xhttpSemNameEq(const xhttpheaders* pHeaders, const __xhttp_headers_entry* pEntry,
	const char* sName, size_t iNameLen)
{
	size_t i;
	const char* sStored;
	if ( pHeaders == NULL || pEntry == NULL || sName == NULL || pEntry->iNameLen != iNameLen ) { return false; }
	sStored = pHeaders->pData + pEntry->iNameOffset;
	for ( i = 0u; i < iNameLen; ++i ) {
		if ( __xhttpSemLower(sStored[i]) != __xhttpSemLower(sName[i]) ) { return false; }
	}
	return true;
}


static xhttp_semantic_result __xhttpSemValidateField(const xhttpheaders* pHeaders,
	const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen)
{
	size_t i;
	if ( pHeaders == NULL || sName == NULL || (sValue == NULL && iValueLen != 0u) ) {
		return XHTTP_SEMANTIC_INVALID_ARGUMENT;
	}
	if ( iNameLen == 0u || !xrtHttpIsTokenN(sName, iNameLen) ) { return XHTTP_SEMANTIC_INVALID_NAME; }
	if ( iNameLen > pHeaders->tConfig.iMaxNameBytes ) { return XHTTP_SEMANTIC_LIMIT; }
	if ( iValueLen > pHeaders->tConfig.iMaxValueBytes ) { return XHTTP_SEMANTIC_LIMIT; }
	for ( i = 0u; i < iValueLen; ++i ) {
		unsigned char ch = (unsigned char)sValue[i];
		if ( (ch < 0x20u && ch != '\t') || ch == 0x7Fu ) { return XHTTP_SEMANTIC_INVALID_VALUE; }
	}
	return XHTTP_SEMANTIC_OK;
}


static bool __xhttpSemRangeAliasesData(const xhttpheaders* pHeaders, const char* pText, size_t iLen)
{
	uintptr_t iText;
	uintptr_t iTextEnd;
	uintptr_t iData;
	uintptr_t iDataEnd;
	if ( pHeaders == NULL || pHeaders->pData == NULL || pText == NULL || iLen == 0u ) { return false; }
	iText = (uintptr_t)(const void*)pText;
	iData = (uintptr_t)(const void*)pHeaders->pData;
	if ( iText > UINTPTR_MAX - iLen || iData > UINTPTR_MAX - pHeaders->iDataLen ) { return true; }
	iTextEnd = iText + iLen;
	iDataEnd = iData + pHeaders->iDataLen;
	return iText < iDataEnd && iData < iTextEnd;
}


static xhttp_semantic_result __xhttpSemStabilizeInputs(const xhttpheaders* pHeaders,
	const char** ppName, size_t iNameLen, const char** ppValue, size_t iValueLen, char** ppStorage)
{
	size_t iBytes;
	char* pStorage;
	if ( ppName == NULL || ppValue == NULL || ppStorage == NULL ) { return XHTTP_SEMANTIC_INVALID_ARGUMENT; }
	*ppStorage = NULL;
	if ( !__xhttpSemRangeAliasesData(pHeaders, *ppName, iNameLen) &&
		!__xhttpSemRangeAliasesData(pHeaders, *ppValue, iValueLen) ) {
		return XHTTP_SEMANTIC_OK;
	}
	if ( !__xhttpSemAddSize(iNameLen, iValueLen, &iBytes) ) { return XHTTP_SEMANTIC_LIMIT; }
	pStorage = (char*)xrtMalloc(iBytes ? iBytes : 1u);
	if ( pStorage == NULL ) { return XHTTP_SEMANTIC_OUT_OF_MEMORY; }
	memmove(pStorage, *ppName, iNameLen);
	if ( iValueLen != 0u ) { memmove(pStorage + iNameLen, *ppValue, iValueLen); }
	*ppName = pStorage;
	*ppValue = pStorage + iNameLen;
	*ppStorage = pStorage;
	return XHTTP_SEMANTIC_OK;
}


static xhttp_semantic_result __xhttpSemReserveEntries(xhttpheaders* pHeaders, size_t iCount)
{
	size_t iCap;
	__xhttp_headers_entry* pNew;
	if ( iCount <= pHeaders->iEntryCap ) { return XHTTP_SEMANTIC_OK; }
	if ( iCount > pHeaders->tConfig.iMaxCount || iCount > (size_t)-1 / sizeof(*pNew) ) {
		return XHTTP_SEMANTIC_LIMIT;
	}
	iCap = pHeaders->iEntryCap ? pHeaders->iEntryCap : (size_t)pHeaders->tConfig.iInitialCount;
	if ( iCap == 0u ) { iCap = 8u; }
	while ( iCap < iCount ) {
		size_t iNext = iCap > (size_t)-1 / 2u ? iCount : iCap * 2u;
		if ( iNext > pHeaders->tConfig.iMaxCount ) { iNext = pHeaders->tConfig.iMaxCount; }
		if ( iNext <= iCap ) { iCap = iCount; break; }
		iCap = iNext;
	}
	pNew = (__xhttp_headers_entry*)xrtRealloc(pHeaders->pEntries, iCap * sizeof(*pNew));
	if ( pNew == NULL ) { return XHTTP_SEMANTIC_OUT_OF_MEMORY; }
	pHeaders->pEntries = pNew;
	pHeaders->iEntryCap = iCap;
	return XHTTP_SEMANTIC_OK;
}


static xhttp_semantic_result __xhttpSemReserveData(xhttpheaders* pHeaders, size_t iBytes)
{
	size_t iCap;
	char* pNew;
	if ( iBytes <= pHeaders->iDataCap ) { return XHTTP_SEMANTIC_OK; }
	iCap = pHeaders->iDataCap ? pHeaders->iDataCap : pHeaders->tConfig.iInitialBytes;
	if ( iCap == 0u ) { iCap = 512u; }
	while ( iCap < iBytes ) {
		size_t iNext = iCap > (size_t)-1 / 2u ? iBytes : iCap * 2u;
		if ( iNext <= iCap ) { iCap = iBytes; break; }
		iCap = iNext;
	}
	pNew = (char*)xrtRealloc(pHeaders->pData, iCap);
	if ( pNew == NULL ) { return XHTTP_SEMANTIC_OUT_OF_MEMORY; }
	pHeaders->pData = pNew;
	pHeaders->iDataCap = iCap;
	return XHTTP_SEMANTIC_OK;
}


static xhttp_semantic_result __xhttpSemStoreEntry(xhttpheaders* pHeaders, __xhttp_headers_entry* pEntry,
	const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen)
{
	size_t iNeed;
	size_t iEnd;
	size_t iStorageLimit;
	xhttp_semantic_result eResult;
	if ( !__xhttpSemAddSize(iNameLen, iValueLen, &iNeed) ||
		!__xhttpSemAddSize(iNeed, 2u, &iNeed) ||
		!__xhttpSemAddSize(pHeaders->tConfig.iMaxBytes,
			(size_t)pHeaders->tConfig.iMaxCount * 2u, &iStorageLimit) ) {
		return XHTTP_SEMANTIC_LIMIT;
	}
	if ( !__xhttpSemAddSize(pHeaders->iDataLen, iNeed, &iEnd) || iEnd > iStorageLimit ||
		(iEnd > pHeaders->iDataCap && pHeaders->iDataLen > pHeaders->iStoredBytes + 1024u) ) {
		eResult = xrtHttpHeadersCompact(pHeaders);
		if ( eResult != XHTTP_SEMANTIC_OK ) { return eResult; }
		if ( !__xhttpSemAddSize(pHeaders->iDataLen, iNeed, &iEnd) || iEnd > iStorageLimit ) {
			return XHTTP_SEMANTIC_LIMIT;
		}
	}
	eResult = __xhttpSemReserveData(pHeaders, iEnd);
	if ( eResult != XHTTP_SEMANTIC_OK ) { return eResult; }
	pEntry->iNameOffset = pHeaders->iDataLen;
	pEntry->iNameLen = iNameLen;
	if ( iNameLen != 0u ) { memcpy(pHeaders->pData + pHeaders->iDataLen, sName, iNameLen); }
	pHeaders->iDataLen += iNameLen;
	pHeaders->pData[pHeaders->iDataLen++] = '\0';
	pEntry->iValueOffset = pHeaders->iDataLen;
	pEntry->iValueLen = iValueLen;
	if ( iValueLen != 0u ) { memcpy(pHeaders->pData + pHeaders->iDataLen, sValue, iValueLen); }
	pHeaders->iDataLen += iValueLen;
	pHeaders->pData[pHeaders->iDataLen++] = '\0';
	return XHTTP_SEMANTIC_OK;
}


XXAPI const char* xrtHttpSemanticResultName(xhttp_semantic_result eResult)
{
	switch ( eResult ) {
		case XHTTP_SEMANTIC_OK: return "ok";
		case XHTTP_SEMANTIC_NOT_FOUND: return "not_found";
		case XHTTP_SEMANTIC_INVALID_ARGUMENT: return "invalid_argument";
		case XHTTP_SEMANTIC_OUT_OF_MEMORY: return "out_of_memory";
		case XHTTP_SEMANTIC_LIMIT: return "limit";
		case XHTTP_SEMANTIC_INVALID_NAME: return "invalid_name";
		case XHTTP_SEMANTIC_INVALID_VALUE: return "invalid_value";
		case XHTTP_SEMANTIC_INVALID_SYNTAX: return "invalid_syntax";
		case XHTTP_SEMANTIC_BUFFER_TOO_SMALL: return "buffer_too_small";
		case XHTTP_SEMANTIC_NOT_REPLAYABLE: return "not_replayable";
		case XHTTP_SEMANTIC_IO: return "io";
		case XHTTP_SEMANTIC_CANCELLED: return "cancelled";
		default: return "unknown";
	}
}


XXAPI void xrtHttpHeadersConfigInit(xhttpheadersconfig* pConfig)
{
	if ( pConfig == NULL ) { return; }
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->iSize = XHTTP_HEADERS_CONFIG_V1_SIZE;
	pConfig->iVersion = XHTTP_HEADERS_CONFIG_VERSION;
	pConfig->iInitialCount = 8u;
	pConfig->iMaxCount = 1024u;
	pConfig->iInitialBytes = 512u;
	pConfig->iMaxNameBytes = 8192u;
	pConfig->iMaxValueBytes = 65536u;
	pConfig->iMaxBytes = 1024u * 1024u;
}


XXAPI xhttpheaders* xrtHttpHeadersCreateEx(const xhttpheadersconfig* pConfig)
{
	xhttpheadersconfig tConfig;
	xhttpheaders* pHeaders;
	xhttp_semantic_result eResult;
	xrtHttpHeadersConfigInit(&tConfig);
	if ( pConfig != NULL ) {
		if ( pConfig->iSize < XHTTP_HEADERS_CONFIG_V1_SIZE ||
			(pConfig->iVersion != 0u && pConfig->iVersion != XHTTP_HEADERS_CONFIG_VERSION) ||
			pConfig->iMaxCount == 0u || pConfig->iMaxNameBytes == 0u ||
			pConfig->iMaxValueBytes == 0u || pConfig->iMaxBytes == 0u ||
			pConfig->iInitialCount > pConfig->iMaxCount || pConfig->iInitialBytes > pConfig->iMaxBytes ) {
			return NULL;
		}
		tConfig = *pConfig;
	}
	pHeaders = (xhttpheaders*)xrtCalloc(1u, sizeof(*pHeaders));
	if ( pHeaders == NULL ) { return NULL; }
	pHeaders->tConfig = tConfig;
	eResult = xrtHttpHeadersReserve(pHeaders, tConfig.iInitialCount, tConfig.iInitialBytes);
	if ( eResult != XHTTP_SEMANTIC_OK ) {
		xrtHttpHeadersDestroy(pHeaders);
		return NULL;
	}
	return pHeaders;
}


XXAPI xhttpheaders* xrtHttpHeadersCreate(void)
{
	return xrtHttpHeadersCreateEx(NULL);
}


XXAPI void xrtHttpHeadersDestroy(xhttpheaders* pHeaders)
{
	if ( pHeaders == NULL ) { return; }
	xrtFree(pHeaders->pEntries);
	xrtFree(pHeaders->pData);
	memset(pHeaders, 0, sizeof(*pHeaders));
	xrtFree(pHeaders);
}


XXAPI void xrtHttpHeadersClear(xhttpheaders* pHeaders)
{
	if ( pHeaders == NULL ) { return; }
	pHeaders->iCount = 0u;
	pHeaders->iDataLen = 0u;
	pHeaders->iBytes = 0u;
	pHeaders->iStoredBytes = 0u;
}


XXAPI xhttp_semantic_result xrtHttpHeadersReserve(xhttpheaders* pHeaders, size_t iCount, size_t iBytes)
{
	xhttp_semantic_result eResult;
	size_t iStorageBytes;
	if ( pHeaders == NULL ) { return XHTTP_SEMANTIC_INVALID_ARGUMENT; }
	if ( iCount > pHeaders->tConfig.iMaxCount || iBytes > pHeaders->tConfig.iMaxBytes ) {
		return XHTTP_SEMANTIC_LIMIT;
	}
	if ( iCount > ((size_t)-1 / 2u) || !__xhttpSemAddSize(iBytes, iCount * 2u, &iStorageBytes) ) {
		return XHTTP_SEMANTIC_LIMIT;
	}
	eResult = __xhttpSemReserveEntries(pHeaders, iCount);
	if ( eResult != XHTTP_SEMANTIC_OK ) { return eResult; }
	return __xhttpSemReserveData(pHeaders, iStorageBytes);
}


XXAPI size_t xrtHttpHeadersCount(const xhttpheaders* pHeaders)
{
	return pHeaders ? pHeaders->iCount : 0u;
}


XXAPI size_t xrtHttpHeadersBytes(const xhttpheaders* pHeaders)
{
	return pHeaders ? pHeaders->iBytes : 0u;
}


XXAPI xhttp_semantic_result xrtHttpHeadersCompact(xhttpheaders* pHeaders)
{
	char* pNew;
	size_t iCap;
	size_t iOff = 0u;
	size_t i;
	if ( pHeaders == NULL ) { return XHTTP_SEMANTIC_INVALID_ARGUMENT; }
	if ( pHeaders->iDataLen == pHeaders->iStoredBytes ) { return XHTTP_SEMANTIC_OK; }
	if ( pHeaders->iStoredBytes == 0u ) {
		pHeaders->iDataLen = 0u;
		return XHTTP_SEMANTIC_OK;
	}
	iCap = pHeaders->iStoredBytes;
	if ( iCap < pHeaders->tConfig.iInitialBytes ) { iCap = pHeaders->tConfig.iInitialBytes; }
	pNew = (char*)xrtMalloc(iCap);
	if ( pNew == NULL ) { return XHTTP_SEMANTIC_OUT_OF_MEMORY; }
	for ( i = 0u; i < pHeaders->iCount; ++i ) {
		__xhttp_headers_entry* pEntry = &pHeaders->pEntries[i];
		const char* sName = pHeaders->pData + pEntry->iNameOffset;
		const char* sValue = pHeaders->pData + pEntry->iValueOffset;
		pEntry->iNameOffset = iOff;
		memcpy(pNew + iOff, sName, pEntry->iNameLen + 1u);
		iOff += pEntry->iNameLen + 1u;
		pEntry->iValueOffset = iOff;
		memcpy(pNew + iOff, sValue, pEntry->iValueLen + 1u);
		iOff += pEntry->iValueLen + 1u;
	}
	xrtFree(pHeaders->pData);
	pHeaders->pData = pNew;
	pHeaders->iDataLen = iOff;
	pHeaders->iDataCap = iCap;
	return XHTTP_SEMANTIC_OK;
}


XXAPI xhttp_semantic_result xrtHttpHeadersAppendN(xhttpheaders* pHeaders, const char* sName,
	size_t iNameLen, const char* sValue, size_t iValueLen)
{
	xhttp_semantic_result eResult;
	size_t iNewBytes;
	size_t iNeed;
	__xhttp_headers_entry tEntry;
	char* pStable = NULL;
	eResult = __xhttpSemValidateField(pHeaders, sName, iNameLen, sValue, iValueLen);
	if ( eResult != XHTTP_SEMANTIC_OK ) { return eResult; }
	if ( pHeaders->iCount >= pHeaders->tConfig.iMaxCount ||
		!__xhttpSemAddSize(iNameLen, iValueLen, &iNeed) ||
		!__xhttpSemAddSize(pHeaders->iBytes, iNeed, &iNewBytes) ||
		iNewBytes > pHeaders->tConfig.iMaxBytes ) {
		return XHTTP_SEMANTIC_LIMIT;
	}
	eResult = __xhttpSemStabilizeInputs(pHeaders, &sName, iNameLen, &sValue, iValueLen, &pStable);
	if ( eResult != XHTTP_SEMANTIC_OK ) { return eResult; }
	eResult = __xhttpSemReserveEntries(pHeaders, pHeaders->iCount + 1u);
	if ( eResult != XHTTP_SEMANTIC_OK ) { xrtFree(pStable); return eResult; }
	eResult = __xhttpSemStoreEntry(pHeaders, &tEntry, sName, iNameLen, sValue, iValueLen);
	if ( eResult != XHTTP_SEMANTIC_OK ) { xrtFree(pStable); return eResult; }
	pHeaders->pEntries[pHeaders->iCount++] = tEntry;
	pHeaders->iBytes = iNewBytes;
	pHeaders->iStoredBytes += iNeed + 2u;
	xrtFree(pStable);
	return XHTTP_SEMANTIC_OK;
}


XXAPI xhttp_semantic_result xrtHttpHeadersAppend(xhttpheaders* pHeaders, const char* sName, const char* sValue)
{
	if ( sName == NULL || sValue == NULL ) { return XHTTP_SEMANTIC_INVALID_ARGUMENT; }
	return xrtHttpHeadersAppendN(pHeaders, sName, strlen(sName), sValue, strlen(sValue));
}


XXAPI xhttp_semantic_result xrtHttpHeadersSetN(xhttpheaders* pHeaders, const char* sName,
	size_t iNameLen, const char* sValue, size_t iValueLen)
{
	xhttp_semantic_result eResult;
	size_t i;
	size_t iFirst = (size_t)-1;
	size_t iMatches = 0u;
	size_t iRemovedBytes = 0u;
	size_t iRemovedStorage = 0u;
	size_t iAddedBytes;
	size_t iNewBytes;
	size_t iNewCount;
	__xhttp_headers_entry tEntry;
	char* pStable = NULL;
	eResult = __xhttpSemValidateField(pHeaders, sName, iNameLen, sValue, iValueLen);
	if ( eResult != XHTTP_SEMANTIC_OK ) { return eResult; }
	eResult = __xhttpSemStabilizeInputs(pHeaders, &sName, iNameLen, &sValue, iValueLen, &pStable);
	if ( eResult != XHTTP_SEMANTIC_OK ) { return eResult; }
	for ( i = 0u; i < pHeaders->iCount; ++i ) {
		if ( __xhttpSemNameEq(pHeaders, &pHeaders->pEntries[i], sName, iNameLen) ) {
			const __xhttp_headers_entry* pEntry = &pHeaders->pEntries[i];
			if ( iFirst == (size_t)-1 ) { iFirst = i; }
			iMatches++;
			iRemovedBytes += pEntry->iNameLen + pEntry->iValueLen;
			iRemovedStorage += pEntry->iNameLen + pEntry->iValueLen + 2u;
		}
	}
	if ( !__xhttpSemAddSize(iNameLen, iValueLen, &iAddedBytes) ) {
		xrtFree(pStable);
		return XHTTP_SEMANTIC_LIMIT;
	}
	iNewBytes = pHeaders->iBytes - iRemovedBytes;
	if ( !__xhttpSemAddSize(iNewBytes, iAddedBytes, &iNewBytes) || iNewBytes > pHeaders->tConfig.iMaxBytes ) {
		xrtFree(pStable);
		return XHTTP_SEMANTIC_LIMIT;
	}
	iNewCount = pHeaders->iCount - iMatches + 1u;
	if ( iNewCount > pHeaders->tConfig.iMaxCount ) { xrtFree(pStable); return XHTTP_SEMANTIC_LIMIT; }
	eResult = __xhttpSemReserveEntries(pHeaders, iNewCount);
	if ( eResult != XHTTP_SEMANTIC_OK ) { xrtFree(pStable); return eResult; }
	if ( pHeaders->iDataLen > pHeaders->iStoredBytes + 1024u &&
		pHeaders->iDataLen - pHeaders->iStoredBytes > pHeaders->iStoredBytes ) {
		eResult = xrtHttpHeadersCompact(pHeaders);
		if ( eResult != XHTTP_SEMANTIC_OK ) { xrtFree(pStable); return eResult; }
	}
	eResult = __xhttpSemStoreEntry(pHeaders, &tEntry, sName, iNameLen, sValue, iValueLen);
	if ( eResult != XHTTP_SEMANTIC_OK ) { xrtFree(pStable); return eResult; }
	if ( iMatches == 0u ) {
		pHeaders->pEntries[pHeaders->iCount++] = tEntry;
	} else {
		size_t iRead;
		size_t iWrite = iFirst + 1u;
		pHeaders->pEntries[iFirst] = tEntry;
		for ( iRead = iFirst + 1u; iRead < pHeaders->iCount; ++iRead ) {
			if ( __xhttpSemNameEq(pHeaders, &pHeaders->pEntries[iRead], sName, iNameLen) ) { continue; }
			if ( iWrite != iRead ) { pHeaders->pEntries[iWrite] = pHeaders->pEntries[iRead]; }
			iWrite++;
		}
		pHeaders->iCount = iWrite;
	}
	pHeaders->iBytes = iNewBytes;
	pHeaders->iStoredBytes = pHeaders->iStoredBytes - iRemovedStorage + iAddedBytes + 2u;
	xrtFree(pStable);
	return XHTTP_SEMANTIC_OK;
}


XXAPI xhttp_semantic_result xrtHttpHeadersSet(xhttpheaders* pHeaders, const char* sName, const char* sValue)
{
	if ( sName == NULL || sValue == NULL ) { return XHTTP_SEMANTIC_INVALID_ARGUMENT; }
	return xrtHttpHeadersSetN(pHeaders, sName, strlen(sName), sValue, strlen(sValue));
}


XXAPI size_t xrtHttpHeadersRemoveN(xhttpheaders* pHeaders, const char* sName, size_t iNameLen)
{
	size_t iRead;
	size_t iWrite = 0u;
	size_t iRemoved = 0u;
	if ( pHeaders == NULL || sName == NULL || iNameLen == 0u ) { return 0u; }
	for ( iRead = 0u; iRead < pHeaders->iCount; ++iRead ) {
		const __xhttp_headers_entry* pEntry = &pHeaders->pEntries[iRead];
		if ( __xhttpSemNameEq(pHeaders, pEntry, sName, iNameLen) ) {
			pHeaders->iBytes -= pEntry->iNameLen + pEntry->iValueLen;
			pHeaders->iStoredBytes -= pEntry->iNameLen + pEntry->iValueLen + 2u;
			iRemoved++;
			continue;
		}
		if ( iWrite != iRead ) { pHeaders->pEntries[iWrite] = pHeaders->pEntries[iRead]; }
		iWrite++;
	}
	pHeaders->iCount = iWrite;
	if ( pHeaders->iStoredBytes == 0u ) { pHeaders->iDataLen = 0u; }
	return iRemoved;
}


XXAPI size_t xrtHttpHeadersRemove(xhttpheaders* pHeaders, const char* sName)
{
	return sName ? xrtHttpHeadersRemoveN(pHeaders, sName, strlen(sName)) : 0u;
}


XXAPI size_t xrtHttpHeadersCountNameN(const xhttpheaders* pHeaders, const char* sName, size_t iNameLen)
{
	size_t i;
	size_t iCount = 0u;
	if ( pHeaders == NULL || sName == NULL || iNameLen == 0u ) { return 0u; }
	for ( i = 0u; i < pHeaders->iCount; ++i ) {
		if ( __xhttpSemNameEq(pHeaders, &pHeaders->pEntries[i], sName, iNameLen) ) { iCount++; }
	}
	return iCount;
}


XXAPI size_t xrtHttpHeadersCountName(const xhttpheaders* pHeaders, const char* sName)
{
	return sName ? xrtHttpHeadersCountNameN(pHeaders, sName, strlen(sName)) : 0u;
}


XXAPI bool xrtHttpHeadersContainsN(const xhttpheaders* pHeaders, const char* sName, size_t iNameLen)
{
	return xrtHttpHeadersCountNameN(pHeaders, sName, iNameLen) != 0u;
}


XXAPI bool xrtHttpHeadersContains(const xhttpheaders* pHeaders, const char* sName)
{
	return sName ? xrtHttpHeadersContainsN(pHeaders, sName, strlen(sName)) : false;
}


XXAPI bool xrtHttpHeadersAt(const xhttpheaders* pHeaders, size_t iIndex, xrtheaderpair* pOut)
{
	const __xhttp_headers_entry* pEntry;
	if ( pOut != NULL ) { memset(pOut, 0, sizeof(*pOut)); }
	if ( pHeaders == NULL || pOut == NULL || iIndex >= pHeaders->iCount ) { return false; }
	pEntry = &pHeaders->pEntries[iIndex];
	pOut->tName = xrtStrView(pHeaders->pData + pEntry->iNameOffset, pEntry->iNameLen);
	pOut->tValue = xrtStrView(pHeaders->pData + pEntry->iValueOffset, pEntry->iValueLen);
	return true;
}


XXAPI bool xrtHttpHeadersGetNthN(const xhttpheaders* pHeaders, const char* sName, size_t iNameLen,
	size_t iNth, xrtstrview* pOut)
{
	size_t i;
	if ( pOut != NULL ) { memset(pOut, 0, sizeof(*pOut)); }
	if ( pHeaders == NULL || sName == NULL || iNameLen == 0u || pOut == NULL ) { return false; }
	for ( i = 0u; i < pHeaders->iCount; ++i ) {
		const __xhttp_headers_entry* pEntry = &pHeaders->pEntries[i];
		if ( !__xhttpSemNameEq(pHeaders, pEntry, sName, iNameLen) ) { continue; }
		if ( iNth-- != 0u ) { continue; }
		*pOut = xrtStrView(pHeaders->pData + pEntry->iValueOffset, pEntry->iValueLen);
		return true;
	}
	return false;
}


XXAPI bool xrtHttpHeadersGetNth(const xhttpheaders* pHeaders, const char* sName, size_t iNth, xrtstrview* pOut)
{
	return sName ? xrtHttpHeadersGetNthN(pHeaders, sName, strlen(sName), iNth, pOut) : false;
}


XXAPI bool xrtHttpHeadersGetN(const xhttpheaders* pHeaders, const char* sName, size_t iNameLen, xrtstrview* pOut)
{
	return xrtHttpHeadersGetNthN(pHeaders, sName, iNameLen, 0u, pOut);
}


XXAPI bool xrtHttpHeadersGet(const xhttpheaders* pHeaders, const char* sName, xrtstrview* pOut)
{
	return sName ? xrtHttpHeadersGetN(pHeaders, sName, strlen(sName), pOut) : false;
}


XXAPI size_t xrtHttpHeadersGetAllToN(const xhttpheaders* pHeaders, const char* sName, size_t iNameLen,
	xrtstrview* pOut, size_t iOutCap)
{
	size_t i;
	size_t iCount = 0u;
	if ( pHeaders == NULL || sName == NULL || iNameLen == 0u || (pOut == NULL && iOutCap != 0u) ) { return 0u; }
	for ( i = 0u; i < pHeaders->iCount; ++i ) {
		const __xhttp_headers_entry* pEntry = &pHeaders->pEntries[i];
		if ( !__xhttpSemNameEq(pHeaders, pEntry, sName, iNameLen) ) { continue; }
		if ( iCount < iOutCap ) {
			pOut[iCount] = xrtStrView(pHeaders->pData + pEntry->iValueOffset, pEntry->iValueLen);
		}
		iCount++;
	}
	return iCount;
}


XXAPI size_t xrtHttpHeadersGetAllTo(const xhttpheaders* pHeaders, const char* sName,
	xrtstrview* pOut, size_t iOutCap)
{
	return sName ? xrtHttpHeadersGetAllToN(pHeaders, sName, strlen(sName), pOut, iOutCap) : 0u;
}


XXAPI xhttpheaders* xrtHttpHeadersClone(const xhttpheaders* pHeaders)
{
	xhttpheaders* pClone;
	size_t i;
	if ( pHeaders == NULL ) { return NULL; }
	pClone = xrtHttpHeadersCreateEx(&pHeaders->tConfig);
	if ( pClone == NULL ) { return NULL; }
	if ( xrtHttpHeadersReserve(pClone, pHeaders->iCount, pHeaders->iBytes) != XHTTP_SEMANTIC_OK ) {
		xrtHttpHeadersDestroy(pClone);
		return NULL;
	}
	for ( i = 0u; i < pHeaders->iCount; ++i ) {
		const __xhttp_headers_entry* pEntry = &pHeaders->pEntries[i];
		if ( xrtHttpHeadersAppendN(pClone,
			pHeaders->pData + pEntry->iNameOffset, pEntry->iNameLen,
			pHeaders->pData + pEntry->iValueOffset, pEntry->iValueLen) != XHTTP_SEMANTIC_OK ) {
			xrtHttpHeadersDestroy(pClone);
			return NULL;
		}
	}
	return pClone;
}


XXAPI xhttp_semantic_result xrtHttpHeadersParseAppendN(xhttpheaders* pHeaders, const char* sBlock,
	size_t iLen, size_t* pErrorOffset)
{
	size_t iOffset = 0u;
	xhttpheaders* pWork;
	xhttp_semantic_result eFinal = XHTTP_SEMANTIC_OK;
	if ( pErrorOffset != NULL ) { *pErrorOffset = 0u; }
	if ( pHeaders == NULL || (sBlock == NULL && iLen != 0u) ) { return XHTTP_SEMANTIC_INVALID_ARGUMENT; }
	pWork = xrtHttpHeadersClone(pHeaders);
	if ( pWork == NULL ) { return XHTTP_SEMANTIC_OUT_OF_MEMORY; }
	while ( iOffset < iLen ) {
		size_t iLineStart = iOffset;
		size_t iLineEnd = iOffset;
		xrtheaderpair tPair;
		xhttp_semantic_result eResult;
		while ( iLineEnd < iLen && sBlock[iLineEnd] != '\r' && sBlock[iLineEnd] != '\n' ) { iLineEnd++; }
		if ( iLineEnd < iLen ) {
			if ( sBlock[iLineEnd] != '\r' || iLineEnd + 1u >= iLen || sBlock[iLineEnd + 1u] != '\n' ) {
				if ( pErrorOffset != NULL ) { *pErrorOffset = iLineEnd; }
				eFinal = XHTTP_SEMANTIC_INVALID_SYNTAX;
				goto done;
			}
			iOffset = iLineEnd + 2u;
		} else {
			iOffset = iLineEnd;
		}
		if ( iLineEnd == iLineStart ) {
			if ( iOffset != iLen ) {
				if ( pErrorOffset != NULL ) { *pErrorOffset = iOffset; }
				eFinal = XHTTP_SEMANTIC_INVALID_SYNTAX;
				goto done;
			}
			break;
		}
		if ( !xrtHttpHeaderSplitLineN(sBlock + iLineStart, iLineEnd - iLineStart, &tPair) ) {
			if ( pErrorOffset != NULL ) { *pErrorOffset = iLineStart; }
			eFinal = XHTTP_SEMANTIC_INVALID_SYNTAX;
			goto done;
		}
		eResult = xrtHttpHeadersAppendN(pWork, tPair.tName.sPtr, tPair.tName.iLen,
			tPair.tValue.sPtr, tPair.tValue.iLen);
		if ( eResult != XHTTP_SEMANTIC_OK ) {
			if ( pErrorOffset != NULL ) { *pErrorOffset = iLineStart; }
			eFinal = eResult;
			goto done;
		}
	}
	xrtFree(pHeaders->pEntries);
	xrtFree(pHeaders->pData);
	pHeaders->pEntries = pWork->pEntries;
	pHeaders->pData = pWork->pData;
	pHeaders->iCount = pWork->iCount;
	pHeaders->iEntryCap = pWork->iEntryCap;
	pHeaders->iDataLen = pWork->iDataLen;
	pHeaders->iDataCap = pWork->iDataCap;
	pHeaders->iBytes = pWork->iBytes;
	pHeaders->iStoredBytes = pWork->iStoredBytes;
	pWork->pEntries = NULL;
	pWork->pData = NULL;

done:
	xrtHttpHeadersDestroy(pWork);
	return eFinal;
}


XXAPI xhttp_semantic_result xrtHttpHeadersParseAppend(xhttpheaders* pHeaders, const char* sBlock,
	size_t* pErrorOffset)
{
	if ( sBlock == NULL ) { return XHTTP_SEMANTIC_INVALID_ARGUMENT; }
	return xrtHttpHeadersParseAppendN(pHeaders, sBlock, strlen(sBlock), pErrorOffset);
}


XXAPI xhttp_semantic_result xrtHttpHeadersBuildBlockTo(const xhttpheaders* pHeaders, char* sOut,
	size_t iOutCap, size_t* pOutLen)
{
	size_t iRequired = 2u;
	size_t iOff = 0u;
	size_t i;
	if ( pOutLen != NULL ) { *pOutLen = 0u; }
	if ( pHeaders == NULL ) { return XHTTP_SEMANTIC_INVALID_ARGUMENT; }
	for ( i = 0u; i < pHeaders->iCount; ++i ) {
		const __xhttp_headers_entry* pEntry = &pHeaders->pEntries[i];
		size_t iLine;
		if ( !__xhttpSemAddSize(pEntry->iNameLen, pEntry->iValueLen, &iLine) ||
			!__xhttpSemAddSize(iLine, 4u, &iLine) || !__xhttpSemAddSize(iRequired, iLine, &iRequired) ) {
			return XHTTP_SEMANTIC_LIMIT;
		}
	}
	if ( pOutLen != NULL ) { *pOutLen = iRequired; }
	if ( sOut == NULL || iOutCap <= iRequired ) { return XHTTP_SEMANTIC_BUFFER_TOO_SMALL; }
	for ( i = 0u; i < pHeaders->iCount; ++i ) {
		const __xhttp_headers_entry* pEntry = &pHeaders->pEntries[i];
		memcpy(sOut + iOff, pHeaders->pData + pEntry->iNameOffset, pEntry->iNameLen);
		iOff += pEntry->iNameLen;
		memcpy(sOut + iOff, ": ", 2u);
		iOff += 2u;
		memcpy(sOut + iOff, pHeaders->pData + pEntry->iValueOffset, pEntry->iValueLen);
		iOff += pEntry->iValueLen;
		memcpy(sOut + iOff, "\r\n", 2u);
		iOff += 2u;
	}
	memcpy(sOut + iOff, "\r\n", 2u);
	iOff += 2u;
	sOut[iOff] = '\0';
	return XHTTP_SEMANTIC_OK;
}


XXAPI str xrtHttpHeadersBuildBlock(const xhttpheaders* pHeaders, size_t* pOutLen)
{
	xhttp_semantic_result eResult;
	size_t iLen = 0u;
	char* sOut;
	if ( pOutLen != NULL ) { *pOutLen = 0u; }
	eResult = xrtHttpHeadersBuildBlockTo(pHeaders, NULL, 0u, &iLen);
	if ( eResult != XHTTP_SEMANTIC_BUFFER_TOO_SMALL ) { return NULL; }
	sOut = (char*)xrtMalloc(iLen + 1u);
	if ( sOut == NULL ) { return NULL; }
	eResult = xrtHttpHeadersBuildBlockTo(pHeaders, sOut, iLen + 1u, &iLen);
	if ( eResult != XHTTP_SEMANTIC_OK ) {
		xrtFree(sOut);
		return NULL;
	}
	if ( pOutLen != NULL ) { *pOutLen = iLen; }
	return (str)sOut;
}

#endif
