#ifndef XRT_XHTTP_FORM_DATA_H
#define XRT_XHTTP_FORM_DATA_H

/* Owned multipart/form-data values and a replay-aware streaming encoder. */

typedef struct __xhttp_form_entry {
	char* sName;
	size_t iNameLen;
	char* sFileName;
	size_t iFileNameLen;
	char* sContentType;
	size_t iContentTypeLen;
	uint32 iFlags;
	xhttpbody* pBody;
} __xhttp_form_entry;

struct xrt_http_form_data {
	xhttpformdataconfig tConfig;
	__xhttp_form_entry* pEntries;
	size_t iCount;
	size_t iCap;
	size_t iMetadataBytes;
};

typedef struct __xhttp_form_body_part {
	char* pHeader;
	size_t iHeaderLen;
	xhttpbody* pBody;
} __xhttp_form_body_part;

typedef struct __xhttp_form_body_factory {
	__xhttp_form_body_part* pParts;
	size_t iCount;
	char sBoundary[71];
	size_t iBoundaryLen;
} __xhttp_form_body_factory;

typedef struct __xhttp_form_body_reader {
	const __xhttp_form_body_factory* pFactory;
	size_t iPart;
	size_t iOffset;
	uint32 iPhase;
	xhttpbodyreader* pPartReader;
	xhttp_body_read_result ePending;
	bool bPending;
} __xhttp_form_body_reader;

#define __XHTTP_FORM_PHASE_HEADER 0u
#define __XHTTP_FORM_PHASE_BODY   1u
#define __XHTTP_FORM_PHASE_CRLF   2u
#define __XHTTP_FORM_PHASE_FINAL  3u
#define __XHTTP_FORM_PHASE_DONE   4u


static bool __xhttpFormAddSize(size_t iA, size_t iB, size_t* pOut)
{
	if ( !pOut || iA > SIZE_MAX - iB ) { return false; }
	*pOut = iA + iB;
	return true;
}


static bool __xhttpFormValidQuotedValue(const char* sValue, size_t iLen)
{
	if ( !sValue ) { return false; }
	for ( size_t i = 0u; i < iLen; ++i ) {
		unsigned char ch = (unsigned char)sValue[i];
		if ( ch < 0x20u || ch == 0x7fu ) { return false; }
	}
	return true;
}


static char* __xhttpFormCopy(const char* sValue, size_t iLen)
{
	char* sCopy;
	if ( !sValue || iLen == SIZE_MAX ) { return NULL; }
	sCopy = (char*)xrtMalloc(iLen + 1u);
	if ( !sCopy ) { return NULL; }
	if ( iLen > 0u ) { memcpy(sCopy, sValue, iLen); }
	sCopy[iLen] = '\0';
	return sCopy;
}


static void __xhttpFormEntryUnit(__xhttp_form_entry* pEntry)
{
	if ( !pEntry ) { return; }
	xrtFree(pEntry->sName);
	xrtFree(pEntry->sFileName);
	xrtFree(pEntry->sContentType);
	xrtHttpBodyRelease(pEntry->pBody);
	memset(pEntry, 0, sizeof(*pEntry));
}


static size_t __xhttpFormEntryMetadata(const __xhttp_form_entry* pEntry)
{
	return pEntry ? pEntry->iNameLen + pEntry->iFileNameLen + pEntry->iContentTypeLen : 0u;
}


static bool __xhttpFormNameEq(const __xhttp_form_entry* pEntry, const char* sName, size_t iNameLen)
{
	return pEntry && sName && pEntry->iNameLen == iNameLen &&
		(iNameLen == 0u || memcmp(pEntry->sName, sName, iNameLen) == 0);
}


static xhttp_semantic_result __xhttpFormPrepareEntry(const xhttpformdata* pForm,
	const char* sName, size_t iNameLen, xhttpbody* pBody,
	const char* sFileName, size_t iFileNameLen,
	const char* sContentType, size_t iContentTypeLen, __xhttp_form_entry* pOut)
{
	size_t iMetadata;
	if ( !pForm || !sName || !pBody || !pOut || (!sFileName && iFileNameLen != 0u) ||
		(!sContentType && iContentTypeLen != 0u) ) {
		return XHTTP_SEMANTIC_INVALID_ARGUMENT;
	}
	if ( iNameLen > pForm->tConfig.iMaxNameBytes ||
		iFileNameLen > pForm->tConfig.iMaxFileNameBytes ||
		iContentTypeLen > pForm->tConfig.iMaxContentTypeBytes ) {
		return XHTTP_SEMANTIC_LIMIT;
	}
	if ( !__xhttpFormAddSize(iNameLen, iFileNameLen, &iMetadata) ||
		!__xhttpFormAddSize(iMetadata, iContentTypeLen, &iMetadata) ) {
		return XHTTP_SEMANTIC_LIMIT;
	}
	if ( !__xhttpFormValidQuotedValue(sName, iNameLen) ||
		(sFileName && !__xhttpFormValidQuotedValue(sFileName, iFileNameLen)) ) {
		return XHTTP_SEMANTIC_INVALID_VALUE;
	}
	if ( sContentType ) {
		xrtmediatypeview tMediaType;
		if ( iContentTypeLen == 0u || !__xhttpFormValidQuotedValue(sContentType, iContentTypeLen) ||
			!xrtHttpMediaTypeParseN(sContentType, iContentTypeLen, &tMediaType) ) {
			return XHTTP_SEMANTIC_INVALID_VALUE;
		}
	}
	memset(pOut, 0, sizeof(*pOut));
	pOut->sName = __xhttpFormCopy(sName, iNameLen);
	if ( !pOut->sName ) { goto oom; }
	if ( sFileName ) {
		pOut->sFileName = __xhttpFormCopy(sFileName, iFileNameLen);
		if ( !pOut->sFileName ) { goto oom; }
		pOut->iFlags |= XHTTP_FORM_DATA_PART_F_FILENAME;
	}
	if ( sContentType ) {
		pOut->sContentType = __xhttpFormCopy(sContentType, iContentTypeLen);
		if ( !pOut->sContentType ) { goto oom; }
		pOut->iFlags |= XHTTP_FORM_DATA_PART_F_CONTENT_TYPE;
	}
	pOut->pBody = xrtHttpBodyRetain(pBody);
	if ( !pOut->pBody ) { goto oom; }
	pOut->iNameLen = iNameLen;
	pOut->iFileNameLen = iFileNameLen;
	pOut->iContentTypeLen = iContentTypeLen;
	(void)iMetadata;
	return XHTTP_SEMANTIC_OK;

oom:
	__xhttpFormEntryUnit(pOut);
	return XHTTP_SEMANTIC_OUT_OF_MEMORY;
}


static xhttp_semantic_result __xhttpFormReserve(xhttpformdata* pForm, size_t iNeed)
{
	__xhttp_form_entry* pNew;
	size_t iCap;
	if ( iNeed <= pForm->iCap ) { return XHTTP_SEMANTIC_OK; }
	if ( iNeed > pForm->tConfig.iMaxCount || iNeed > SIZE_MAX / sizeof(*pNew) ) {
		return XHTTP_SEMANTIC_LIMIT;
	}
	iCap = pForm->iCap ? pForm->iCap : pForm->tConfig.iInitialCount;
	if ( iCap == 0u ) { iCap = 8u; }
	while ( iCap < iNeed ) {
		size_t iNext = iCap > SIZE_MAX / 2u ? iNeed : iCap * 2u;
		if ( iNext > pForm->tConfig.iMaxCount ) { iNext = pForm->tConfig.iMaxCount; }
		if ( iNext <= iCap ) { iCap = iNeed; break; }
		iCap = iNext;
	}
	pNew = (__xhttp_form_entry*)xrtRealloc(pForm->pEntries, iCap * sizeof(*pNew));
	if ( !pNew ) { return XHTTP_SEMANTIC_OUT_OF_MEMORY; }
	if ( iCap > pForm->iCap ) {
		memset(pNew + pForm->iCap, 0, (iCap - pForm->iCap) * sizeof(*pNew));
	}
	pForm->pEntries = pNew;
	pForm->iCap = iCap;
	return XHTTP_SEMANTIC_OK;
}


XXAPI void xrtHttpFormDataConfigInit(xhttpformdataconfig* pConfig)
{
	if ( !pConfig ) { return; }
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->iSize = XHTTP_FORM_DATA_CONFIG_V1_SIZE;
	pConfig->iVersion = XHTTP_FORM_DATA_CONFIG_VERSION;
	pConfig->iInitialCount = 8u;
	pConfig->iMaxCount = 1024u;
	pConfig->iMaxNameBytes = 4096u;
	pConfig->iMaxFileNameBytes = 16384u;
	pConfig->iMaxContentTypeBytes = 4096u;
	pConfig->iMaxPartBytes = SIZE_MAX;
	pConfig->iMaxBodyBytes = SIZE_MAX;
	pConfig->iMaxMetadataBytes = 1024u * 1024u;
}


XXAPI xhttpformdata* xrtHttpFormDataCreateEx(const xhttpformdataconfig* pConfig)
{
	xhttpformdataconfig tConfig;
	xhttpformdata* pForm;
	xrtHttpFormDataConfigInit(&tConfig);
	if ( pConfig ) {
		if ( pConfig->iSize < XHTTP_FORM_DATA_CONFIG_V1_SIZE ||
			(pConfig->iVersion != 0u && pConfig->iVersion != XHTTP_FORM_DATA_CONFIG_VERSION) ||
			pConfig->iMaxCount == 0u || pConfig->iInitialCount > pConfig->iMaxCount ||
			pConfig->iMaxNameBytes == 0u || pConfig->iMaxFileNameBytes == 0u ||
			pConfig->iMaxContentTypeBytes == 0u || pConfig->iMaxMetadataBytes == 0u ) {
			return NULL;
		}
		tConfig = *pConfig;
	}
	pForm = (xhttpformdata*)xrtCalloc(1u, sizeof(*pForm));
	if ( !pForm ) { return NULL; }
	pForm->tConfig = tConfig;
	return pForm;
}


XXAPI xhttpformdata* xrtHttpFormDataCreate(void)
{
	return xrtHttpFormDataCreateEx(NULL);
}


XXAPI void xrtHttpFormDataClear(xhttpformdata* pForm)
{
	if ( !pForm ) { return; }
	for ( size_t i = 0u; i < pForm->iCount; ++i ) { __xhttpFormEntryUnit(&pForm->pEntries[i]); }
	pForm->iCount = 0u;
	pForm->iMetadataBytes = 0u;
}


XXAPI void xrtHttpFormDataDestroy(xhttpformdata* pForm)
{
	if ( !pForm ) { return; }
	xrtHttpFormDataClear(pForm);
	xrtFree(pForm->pEntries);
	memset(pForm, 0, sizeof(*pForm));
	xrtFree(pForm);
}


XXAPI size_t xrtHttpFormDataCount(const xhttpformdata* pForm)
{
	return pForm ? pForm->iCount : 0u;
}


XXAPI size_t xrtHttpFormDataCountNameN(const xhttpformdata* pForm, const char* sName, size_t iNameLen)
{
	size_t iCount = 0u;
	if ( !pForm || !sName ) { return 0u; }
	for ( size_t i = 0u; i < pForm->iCount; ++i ) {
		if ( __xhttpFormNameEq(&pForm->pEntries[i], sName, iNameLen) ) { iCount++; }
	}
	return iCount;
}


XXAPI size_t xrtHttpFormDataCountName(const xhttpformdata* pForm, const char* sName)
{
	return sName ? xrtHttpFormDataCountNameN(pForm, sName, strlen(sName)) : 0u;
}


XXAPI bool xrtHttpFormDataHasN(const xhttpformdata* pForm, const char* sName, size_t iNameLen)
{
	return xrtHttpFormDataCountNameN(pForm, sName, iNameLen) != 0u;
}


XXAPI bool xrtHttpFormDataHas(const xhttpformdata* pForm, const char* sName)
{
	return sName && xrtHttpFormDataHasN(pForm, sName, strlen(sName));
}


XXAPI xhttp_semantic_result xrtHttpFormDataAppendBodyN(xhttpformdata* pForm,
	const char* sName, size_t iNameLen, xhttpbody* pBody,
	const char* sFileName, size_t iFileNameLen, const char* sContentType, size_t iContentTypeLen)
{
	__xhttp_form_entry tEntry;
	xhttp_semantic_result eResult;
	size_t iMetadata;
	if ( !pForm ) { return XHTTP_SEMANTIC_INVALID_ARGUMENT; }
	if ( pForm->iCount >= pForm->tConfig.iMaxCount ) { return XHTTP_SEMANTIC_LIMIT; }
	eResult = __xhttpFormPrepareEntry(pForm, sName, iNameLen, pBody,
		sFileName, iFileNameLen, sContentType, iContentTypeLen, &tEntry);
	if ( eResult != XHTTP_SEMANTIC_OK ) { return eResult; }
	iMetadata = __xhttpFormEntryMetadata(&tEntry);
	if ( iMetadata > pForm->tConfig.iMaxMetadataBytes ||
		pForm->iMetadataBytes > pForm->tConfig.iMaxMetadataBytes - iMetadata ) {
		__xhttpFormEntryUnit(&tEntry);
		return XHTTP_SEMANTIC_LIMIT;
	}
	eResult = __xhttpFormReserve(pForm, pForm->iCount + 1u);
	if ( eResult != XHTTP_SEMANTIC_OK ) { __xhttpFormEntryUnit(&tEntry); return eResult; }
	pForm->pEntries[pForm->iCount++] = tEntry;
	pForm->iMetadataBytes += iMetadata;
	return XHTTP_SEMANTIC_OK;
}


XXAPI xhttp_semantic_result xrtHttpFormDataAppendBody(xhttpformdata* pForm,
	const char* sName, xhttpbody* pBody, const char* sFileName, const char* sContentType)
{
	return sName ? xrtHttpFormDataAppendBodyN(pForm, sName, strlen(sName), pBody,
		sFileName, sFileName ? strlen(sFileName) : 0u,
		sContentType, sContentType ? strlen(sContentType) : 0u) : XHTTP_SEMANTIC_INVALID_ARGUMENT;
}


XXAPI xhttp_semantic_result xrtHttpFormDataAppendTextN(xhttpformdata* pForm,
	const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen)
{
	xhttpbody* pBody;
	xhttp_semantic_result eResult;
	if ( !sValue && iValueLen != 0u ) { return XHTTP_SEMANTIC_INVALID_ARGUMENT; }
	pBody = xrtHttpBodyCreateBytesCopy(sValue, iValueLen);
	if ( !pBody ) { return XHTTP_SEMANTIC_OUT_OF_MEMORY; }
	eResult = xrtHttpFormDataAppendBodyN(pForm, sName, iNameLen, pBody, NULL, 0u, NULL, 0u);
	xrtHttpBodyRelease(pBody);
	return eResult;
}


XXAPI xhttp_semantic_result xrtHttpFormDataAppendText(xhttpformdata* pForm,
	const char* sName, const char* sValue)
{
	return sName && sValue ? xrtHttpFormDataAppendTextN(pForm,
		sName, strlen(sName), sValue, strlen(sValue)) : XHTTP_SEMANTIC_INVALID_ARGUMENT;
}


XXAPI xhttp_semantic_result xrtHttpFormDataAppendBytesCopyN(xhttpformdata* pForm,
	const char* sName, size_t iNameLen, const void* pData, size_t iDataLen,
	const char* sFileName, size_t iFileNameLen, const char* sContentType, size_t iContentTypeLen)
{
	xhttpbody* pBody;
	xhttp_semantic_result eResult;
	if ( !pData && iDataLen != 0u ) { return XHTTP_SEMANTIC_INVALID_ARGUMENT; }
	pBody = xrtHttpBodyCreateBytesCopy(pData, iDataLen);
	if ( !pBody ) { return XHTTP_SEMANTIC_OUT_OF_MEMORY; }
	eResult = xrtHttpFormDataAppendBodyN(pForm, sName, iNameLen, pBody,
		sFileName, iFileNameLen, sContentType, iContentTypeLen);
	xrtHttpBodyRelease(pBody);
	return eResult;
}


XXAPI size_t xrtHttpFormDataDeleteN(xhttpformdata* pForm, const char* sName, size_t iNameLen)
{
	size_t iRemoved = 0u;
	if ( !pForm || !sName ) { return 0u; }
	for ( size_t i = 0u; i < pForm->iCount; ) {
		if ( __xhttpFormNameEq(&pForm->pEntries[i], sName, iNameLen) ) {
			pForm->iMetadataBytes -= __xhttpFormEntryMetadata(&pForm->pEntries[i]);
			__xhttpFormEntryUnit(&pForm->pEntries[i]);
			if ( i + 1u < pForm->iCount ) {
				memmove(&pForm->pEntries[i], &pForm->pEntries[i + 1u],
					(pForm->iCount - i - 1u) * sizeof(*pForm->pEntries));
			}
			pForm->iCount--;
			memset(&pForm->pEntries[pForm->iCount], 0, sizeof(*pForm->pEntries));
			iRemoved++;
		} else { i++; }
	}
	return iRemoved;
}


XXAPI size_t xrtHttpFormDataDelete(xhttpformdata* pForm, const char* sName)
{
	return sName ? xrtHttpFormDataDeleteN(pForm, sName, strlen(sName)) : 0u;
}


XXAPI xhttp_semantic_result xrtHttpFormDataSetBodyN(xhttpformdata* pForm,
	const char* sName, size_t iNameLen, xhttpbody* pBody,
	const char* sFileName, size_t iFileNameLen, const char* sContentType, size_t iContentTypeLen)
{
	__xhttp_form_entry tEntry;
	xhttp_semantic_result eResult;
	size_t iRemovedCount = 0u;
	size_t iRemovedMetadata = 0u;
	size_t iTargetCount;
	size_t iNewMetadata;
	if ( !pForm ) { return XHTTP_SEMANTIC_INVALID_ARGUMENT; }
	eResult = __xhttpFormPrepareEntry(pForm, sName, iNameLen, pBody,
		sFileName, iFileNameLen, sContentType, iContentTypeLen, &tEntry);
	if ( eResult != XHTTP_SEMANTIC_OK ) { return eResult; }
	for ( size_t i = 0u; i < pForm->iCount; ++i ) {
		if ( __xhttpFormNameEq(&pForm->pEntries[i], sName, iNameLen) ) {
			iRemovedCount++;
			iRemovedMetadata += __xhttpFormEntryMetadata(&pForm->pEntries[i]);
		}
	}
	iTargetCount = pForm->iCount - iRemovedCount + 1u;
	iNewMetadata = __xhttpFormEntryMetadata(&tEntry);
	if ( iTargetCount > pForm->tConfig.iMaxCount ||
		iNewMetadata > pForm->tConfig.iMaxMetadataBytes ||
		pForm->iMetadataBytes - iRemovedMetadata > pForm->tConfig.iMaxMetadataBytes - iNewMetadata ) {
		__xhttpFormEntryUnit(&tEntry);
		return XHTTP_SEMANTIC_LIMIT;
	}
	eResult = __xhttpFormReserve(pForm, iTargetCount);
	if ( eResult != XHTTP_SEMANTIC_OK ) { __xhttpFormEntryUnit(&tEntry); return eResult; }
	(void)xrtHttpFormDataDeleteN(pForm, sName, iNameLen);
	pForm->pEntries[pForm->iCount++] = tEntry;
	pForm->iMetadataBytes += __xhttpFormEntryMetadata(&tEntry);
	return XHTTP_SEMANTIC_OK;
}


XXAPI bool xrtHttpFormDataAt(const xhttpformdata* pForm, size_t iIndex, xhttpformdatapartinfo* pOut)
{
	const __xhttp_form_entry* pEntry;
	if ( !pForm || !pOut || iIndex >= pForm->iCount ) { return false; }
	pEntry = &pForm->pEntries[iIndex];
	memset(pOut, 0, sizeof(*pOut));
	pOut->iFlags = pEntry->iFlags;
	pOut->tName = xrtStrView(pEntry->sName, pEntry->iNameLen);
	pOut->tFileName = xrtStrView(pEntry->sFileName, pEntry->iFileNameLen);
	pOut->tContentType = xrtStrView(pEntry->sContentType, pEntry->iContentTypeLen);
	pOut->iContentLength = xrtHttpBodyContentLength(pEntry->pBody);
	pOut->pBody = pEntry->pBody;
	return true;
}


XXAPI bool xrtHttpFormDataGetN(const xhttpformdata* pForm,
	const char* sName, size_t iNameLen, xhttpformdatapartinfo* pOut)
{
	if ( !pForm || !sName || !pOut ) { return false; }
	for ( size_t i = 0u; i < pForm->iCount; ++i ) {
		if ( __xhttpFormNameEq(&pForm->pEntries[i], sName, iNameLen) ) {
			return xrtHttpFormDataAt(pForm, i, pOut);
		}
	}
	return false;
}


XXAPI bool xrtHttpFormDataGet(const xhttpformdata* pForm,
	const char* sName, xhttpformdatapartinfo* pOut)
{
	return sName && xrtHttpFormDataGetN(pForm, sName, strlen(sName), pOut);
}


XXAPI xhttpformdata* xrtHttpFormDataClone(const xhttpformdata* pForm)
{
	xhttpformdata* pClone;
	if ( !pForm ) { return NULL; }
	pClone = xrtHttpFormDataCreateEx(&pForm->tConfig);
	if ( !pClone ) { return NULL; }
	for ( size_t i = 0u; i < pForm->iCount; ++i ) {
		const __xhttp_form_entry* pEntry = &pForm->pEntries[i];
		if ( xrtHttpFormDataAppendBodyN(pClone, pEntry->sName, pEntry->iNameLen, pEntry->pBody,
			pEntry->sFileName, pEntry->iFileNameLen,
			pEntry->sContentType, pEntry->iContentTypeLen) != XHTTP_SEMANTIC_OK ) {
			xrtHttpFormDataDestroy(pClone);
			return NULL;
		}
	}
	return pClone;
}


static bool __xhttpFormAttrChar(unsigned char ch)
{
	return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
		(ch >= '0' && ch <= '9') || ch == '!' || ch == '#' || ch == '$' ||
		ch == '&' || ch == '+' || ch == '-' || ch == '.' || ch == '^' ||
		ch == '_' || ch == '`' || ch == '|' || ch == '~';
}


static bool __xhttpFormHasNonAscii(const char* sText, size_t iLen)
{
	for ( size_t i = 0u; i < iLen; ++i ) {
		if ( (unsigned char)sText[i] >= 0x80u ) { return true; }
	}
	return false;
}


static bool __xhttpFormQuotedLen(const char* sText, size_t iLen, size_t* pOut)
{
	size_t iNeed = iLen;
	for ( size_t i = 0u; i < iLen; ++i ) {
		if ( sText[i] == '"' || sText[i] == '\\' ) {
			if ( iNeed == SIZE_MAX ) { return false; }
			iNeed++;
		}
	}
	*pOut = iNeed;
	return true;
}


static bool __xhttpFormExtLen(const char* sText, size_t iLen, size_t* pOut)
{
	size_t iNeed = 0u;
	for ( size_t i = 0u; i < iLen; ++i ) {
		size_t iStep = __xhttpFormAttrChar((unsigned char)sText[i]) ? 1u : 3u;
		if ( !__xhttpFormAddSize(iNeed, iStep, &iNeed) ) { return false; }
	}
	*pOut = iNeed;
	return true;
}


static void __xhttpFormAppendRaw(char* pOut, size_t* pOffset, const char* pData, size_t iLen)
{
	if ( iLen > 0u ) { memcpy(pOut + *pOffset, pData, iLen); }
	*pOffset += iLen;
}


static void __xhttpFormAppendQuoted(char* pOut, size_t* pOffset, const char* sText, size_t iLen)
{
	for ( size_t i = 0u; i < iLen; ++i ) {
		if ( sText[i] == '"' || sText[i] == '\\' ) { pOut[(*pOffset)++] = '\\'; }
		pOut[(*pOffset)++] = sText[i];
	}
}


static void __xhttpFormAppendExt(char* pOut, size_t* pOffset, const char* sText, size_t iLen)
{
	static const char aHex[] = "0123456789ABCDEF";
	for ( size_t i = 0u; i < iLen; ++i ) {
		unsigned char ch = (unsigned char)sText[i];
		if ( __xhttpFormAttrChar(ch) ) { pOut[(*pOffset)++] = (char)ch; }
		else {
			pOut[(*pOffset)++] = '%';
			pOut[(*pOffset)++] = aHex[ch >> 4u];
			pOut[(*pOffset)++] = aHex[ch & 0x0fu];
		}
	}
}


static xhttp_semantic_result __xhttpFormBuildPartHeader(const __xhttp_form_entry* pEntry,
	const char* sBoundary, size_t iBoundaryLen, char** ppHeader, size_t* pHeaderLen)
{
	static const char sDisp[] = "Content-Disposition: form-data; name=\"";
	static const char sFile[] = "\"; filename=\"";
	static const char sFileExt[] = "\"; filename*=UTF-8''";
	static const char sType[] = "\r\nContent-Type: ";
	static const char sEnd[] = "\r\n\r\n";
	size_t iNameEncoded;
	size_t iFileEncoded = 0u;
	size_t iFileExtEncoded = 0u;
	size_t iNeed = 0u;
	size_t iOff = 0u;
	bool bFileExt;
	char* pHeader;
	if ( !pEntry || !ppHeader || !pHeaderLen ||
		!__xhttpFormQuotedLen(pEntry->sName, pEntry->iNameLen, &iNameEncoded) ) {
		return XHTTP_SEMANTIC_LIMIT;
	}
	bFileExt = (pEntry->iFlags & XHTTP_FORM_DATA_PART_F_FILENAME) != 0u &&
		__xhttpFormHasNonAscii(pEntry->sFileName, pEntry->iFileNameLen);
	if ( (pEntry->iFlags & XHTTP_FORM_DATA_PART_F_FILENAME) != 0u &&
		(!__xhttpFormQuotedLen(pEntry->sFileName, pEntry->iFileNameLen, &iFileEncoded) ||
		(bFileExt && !__xhttpFormExtLen(pEntry->sFileName, pEntry->iFileNameLen, &iFileExtEncoded))) ) {
		return XHTTP_SEMANTIC_LIMIT;
	}
#define __XHTTP_FORM_ADD_HEADER(n) do { if ( !__xhttpFormAddSize(iNeed, (n), &iNeed) ) return XHTTP_SEMANTIC_LIMIT; } while (0)
	__XHTTP_FORM_ADD_HEADER(2u + iBoundaryLen + 2u);
	__XHTTP_FORM_ADD_HEADER(sizeof(sDisp) - 1u + iNameEncoded);
	if ( (pEntry->iFlags & XHTTP_FORM_DATA_PART_F_FILENAME) != 0u ) {
		__XHTTP_FORM_ADD_HEADER(sizeof(sFile) - 1u + iFileEncoded);
		if ( bFileExt ) { __XHTTP_FORM_ADD_HEADER(sizeof(sFileExt) - 1u + iFileExtEncoded); }
	}
	if ( (pEntry->iFlags & XHTTP_FORM_DATA_PART_F_FILENAME) == 0u || !bFileExt ) {
		__XHTTP_FORM_ADD_HEADER(1u);
	}
	if ( (pEntry->iFlags & XHTTP_FORM_DATA_PART_F_CONTENT_TYPE) != 0u ) {
		__XHTTP_FORM_ADD_HEADER(sizeof(sType) - 1u + pEntry->iContentTypeLen);
	}
	__XHTTP_FORM_ADD_HEADER(sizeof(sEnd) - 1u);
#undef __XHTTP_FORM_ADD_HEADER
	pHeader = (char*)xrtMalloc(iNeed ? iNeed : 1u);
	if ( !pHeader ) { return XHTTP_SEMANTIC_OUT_OF_MEMORY; }
	__xhttpFormAppendRaw(pHeader, &iOff, "--", 2u);
	__xhttpFormAppendRaw(pHeader, &iOff, sBoundary, iBoundaryLen);
	__xhttpFormAppendRaw(pHeader, &iOff, "\r\n", 2u);
	__xhttpFormAppendRaw(pHeader, &iOff, sDisp, sizeof(sDisp) - 1u);
	__xhttpFormAppendQuoted(pHeader, &iOff, pEntry->sName, pEntry->iNameLen);
	if ( (pEntry->iFlags & XHTTP_FORM_DATA_PART_F_FILENAME) != 0u ) {
		__xhttpFormAppendRaw(pHeader, &iOff, sFile, sizeof(sFile) - 1u);
		__xhttpFormAppendQuoted(pHeader, &iOff, pEntry->sFileName, pEntry->iFileNameLen);
		if ( bFileExt ) {
			__xhttpFormAppendRaw(pHeader, &iOff, sFileExt, sizeof(sFileExt) - 1u);
			__xhttpFormAppendExt(pHeader, &iOff, pEntry->sFileName, pEntry->iFileNameLen);
		}
	}
	if ( (pEntry->iFlags & XHTTP_FORM_DATA_PART_F_FILENAME) == 0u || !bFileExt ) {
		pHeader[iOff++] = '"';
	}
	if ( (pEntry->iFlags & XHTTP_FORM_DATA_PART_F_CONTENT_TYPE) != 0u ) {
		__xhttpFormAppendRaw(pHeader, &iOff, sType, sizeof(sType) - 1u);
		__xhttpFormAppendRaw(pHeader, &iOff, pEntry->sContentType, pEntry->iContentTypeLen);
	}
	__xhttpFormAppendRaw(pHeader, &iOff, sEnd, sizeof(sEnd) - 1u);
	if ( iOff != iNeed ) { xrtFree(pHeader); return XHTTP_SEMANTIC_INVALID_SYNTAX; }
	*ppHeader = pHeader;
	*pHeaderLen = iNeed;
	return XHTTP_SEMANTIC_OK;
}


static void __xhttpFormBodyFactoryFree(ptr pContext)
{
	__xhttp_form_body_factory* pFactory = (__xhttp_form_body_factory*)pContext;
	if ( !pFactory ) { return; }
	for ( size_t i = 0u; i < pFactory->iCount; ++i ) {
		xrtFree(pFactory->pParts[i].pHeader);
		xrtHttpBodyRelease(pFactory->pParts[i].pBody);
	}
	xrtFree(pFactory->pParts);
	memset(pFactory, 0, sizeof(*pFactory));
	xrtFree(pFactory);
}


static xhttp_body_read_result __xhttpFormBodyRead(ptr pContext, void* pBuffer,
	size_t iCapacity, size_t* pRead, const xhttpcontext* pRequestContext)
{
	__xhttp_form_body_reader* pReader = (__xhttp_form_body_reader*)pContext;
	uint8* pOut = (uint8*)pBuffer;
	size_t iDone = 0u;
	if ( !pReader || !pOut || !pRead || iCapacity == 0u ) { return XHTTP_BODY_READ_ERROR; }
	if ( xrtHttpContextIsDone(pRequestContext) ) { return XHTTP_BODY_READ_CANCELLED; }
	if ( pReader->bPending ) {
		xhttp_body_read_result ePending = pReader->ePending;
		pReader->bPending = false;
		return ePending;
	}
	while ( iDone < iCapacity ) {
		if ( pReader->iPhase == __XHTTP_FORM_PHASE_HEADER ) {
			const __xhttp_form_body_part* pPart = &pReader->pFactory->pParts[pReader->iPart];
			size_t iRemain = pPart->iHeaderLen - pReader->iOffset;
			size_t iStep = iRemain < iCapacity - iDone ? iRemain : iCapacity - iDone;
			memcpy(pOut + iDone, pPart->pHeader + pReader->iOffset, iStep);
			pReader->iOffset += iStep;
			iDone += iStep;
			if ( pReader->iOffset == pPart->iHeaderLen ) {
				pReader->iOffset = 0u;
				pReader->iPhase = __XHTTP_FORM_PHASE_BODY;
			}
			continue;
		}
		if ( pReader->iPhase == __XHTTP_FORM_PHASE_BODY ) {
			xhttp_body_read_result ePart;
			size_t iPartRead = 0u;
			if ( !pReader->pPartReader ) {
				xhttp_semantic_result eOpen = xrtHttpBodyOpen(
					pReader->pFactory->pParts[pReader->iPart].pBody,
					pRequestContext, &pReader->pPartReader);
				if ( eOpen != XHTTP_SEMANTIC_OK ) {
					xhttp_body_read_result eOpenRead = eOpen == XHTTP_SEMANTIC_CANCELLED
						? XHTTP_BODY_READ_CANCELLED : XHTTP_BODY_READ_ERROR;
					if ( iDone > 0u ) { pReader->ePending = eOpenRead; pReader->bPending = true; break; }
					return eOpenRead;
				}
			}
			ePart = xrtHttpBodyReaderRead(pReader->pPartReader, pOut + iDone,
				iCapacity - iDone, &iPartRead, pRequestContext);
			if ( ePart == XHTTP_BODY_READ_DATA ) { iDone += iPartRead; continue; }
			if ( ePart == XHTTP_BODY_READ_EOF ) {
				xrtHttpBodyReaderClose(pReader->pPartReader);
				pReader->pPartReader = NULL;
				pReader->iPhase = __XHTTP_FORM_PHASE_CRLF;
				pReader->iOffset = 0u;
				continue;
			}
			if ( iDone > 0u ) { pReader->ePending = ePart; pReader->bPending = true; break; }
			return ePart;
		}
		if ( pReader->iPhase == __XHTTP_FORM_PHASE_CRLF ) {
			while ( iDone < iCapacity && pReader->iOffset < 2u ) {
				pOut[iDone++] = "\r\n"[pReader->iOffset++];
			}
			if ( pReader->iOffset == 2u ) {
				pReader->iPart++;
				pReader->iOffset = 0u;
				pReader->iPhase = pReader->iPart < pReader->pFactory->iCount
					? __XHTTP_FORM_PHASE_HEADER : __XHTTP_FORM_PHASE_FINAL;
			}
			continue;
		}
		if ( pReader->iPhase == __XHTTP_FORM_PHASE_FINAL ) {
			size_t iFinalLen = pReader->pFactory->iBoundaryLen + 6u;
			while ( iDone < iCapacity && pReader->iOffset < iFinalLen ) {
				size_t i = pReader->iOffset++;
				if ( i < 2u ) { pOut[iDone++] = '-'; }
				else if ( i < 2u + pReader->pFactory->iBoundaryLen ) {
					pOut[iDone++] = (uint8)pReader->pFactory->sBoundary[i - 2u];
				} else { pOut[iDone++] = (uint8)"--\r\n"[i - 2u - pReader->pFactory->iBoundaryLen]; }
			}
			if ( pReader->iOffset == iFinalLen ) { pReader->iPhase = __XHTTP_FORM_PHASE_DONE; }
			continue;
		}
		break;
	}
	*pRead = iDone;
	return iDone > 0u ? XHTTP_BODY_READ_DATA : XHTTP_BODY_READ_EOF;
}


static xnetfuture* __xhttpFormBodyWait(ptr pContext, const xhttpcontext* pRequestContext)
{
	__xhttp_form_body_reader* pReader = (__xhttp_form_body_reader*)pContext;
	return pReader && pReader->iPhase == __XHTTP_FORM_PHASE_BODY && pReader->pPartReader
		? xrtHttpBodyReaderWaitReadable(pReader->pPartReader, pRequestContext) : NULL;
}


static void __xhttpFormBodyReaderClose(ptr pContext)
{
	__xhttp_form_body_reader* pReader = (__xhttp_form_body_reader*)pContext;
	if ( !pReader ) { return; }
	xrtHttpBodyReaderClose(pReader->pPartReader);
	pReader->pPartReader = NULL;
	xrtFree(pReader);
}


static xhttp_semantic_result __xhttpFormBodyOpen(ptr pContext,
	const xhttpcontext* pRequestContext, xhttpbodyreaderops* pOutOps, ptr* ppReaderContext)
{
	__xhttp_form_body_factory* pFactory = (__xhttp_form_body_factory*)pContext;
	__xhttp_form_body_reader* pReader;
	if ( !pFactory || !pOutOps || !ppReaderContext ) { return XHTTP_SEMANTIC_INVALID_ARGUMENT; }
	if ( xrtHttpContextIsDone(pRequestContext) ) { return XHTTP_SEMANTIC_CANCELLED; }
	pReader = (__xhttp_form_body_reader*)xrtCalloc(1u, sizeof(*pReader));
	if ( !pReader ) { return XHTTP_SEMANTIC_OUT_OF_MEMORY; }
	pReader->pFactory = pFactory;
	pReader->iPhase = pFactory->iCount > 0u ? __XHTTP_FORM_PHASE_HEADER : __XHTTP_FORM_PHASE_FINAL;
	xrtHttpBodyReaderOpsInit(pOutOps);
	pOutOps->Read = __xhttpFormBodyRead;
	pOutOps->WaitReadable = __xhttpFormBodyWait;
	pOutOps->Close = __xhttpFormBodyReaderClose;
	*ppReaderContext = pReader;
	return XHTTP_SEMANTIC_OK;
}


static void __xhttpFormGenerateBoundary(char sBoundary[71], size_t* pLen)
{
	uint8 aRandom[16];
	static const char aHex[] = "0123456789abcdef";
	memset(aRandom, 0, sizeof(aRandom));
#ifndef XRT_NO_CRYPTO
	xrtRandomBytes(aRandom, sizeof(aRandom));
#else
	{
		static volatile int64 iSequence;
		uint64 iSeed = (uint64)__xrtAtomicAddFetch64(&iSequence, 1) ^ (uint64)(uintptr_t)sBoundary;
		for ( size_t i = 0u; i < sizeof(aRandom); ++i ) {
			iSeed ^= iSeed << 13u; iSeed ^= iSeed >> 7u; iSeed ^= iSeed << 17u;
			aRandom[i] = (uint8)iSeed;
		}
	}
#endif
	memcpy(sBoundary, "----xrt-form-", 13u);
	for ( size_t i = 0u; i < sizeof(aRandom); ++i ) {
		sBoundary[13u + i * 2u] = aHex[aRandom[i] >> 4u];
		sBoundary[14u + i * 2u] = aHex[aRandom[i] & 0x0fu];
	}
	sBoundary[45] = '\0';
	*pLen = 45u;
}


XXAPI xhttp_semantic_result xrtHttpFormDataBuildBody(const xhttpformdata* pForm,
	const char* sBoundary, xhttpbody** ppBody,
	char* sContentType, size_t iContentTypeCap, size_t* pContentTypeLen)
{
	__xhttp_form_body_factory* pFactory = NULL;
	xhttpbody* pBody = NULL;
	char aBoundary[71];
	size_t iBoundaryLen;
	size_t iContentTypeNeed;
	int64_t iContentLength = 0;
	uint32 iBodyFlags = XHTTP_BODY_F_REPLAYABLE;
	xhttp_semantic_result eResult = XHTTP_SEMANTIC_OK;
	if ( ppBody ) { *ppBody = NULL; }
	if ( pContentTypeLen ) { *pContentTypeLen = 0u; }
	if ( !pForm || !ppBody || !pContentTypeLen ) { return XHTTP_SEMANTIC_INVALID_ARGUMENT; }
	if ( sBoundary ) {
		iBoundaryLen = strlen(sBoundary);
		if ( !__xrtHttpUtilValidateBoundaryN(sBoundary, iBoundaryLen) ) {
			return XHTTP_SEMANTIC_INVALID_VALUE;
		}
		memcpy(aBoundary, sBoundary, iBoundaryLen + 1u);
	} else { __xhttpFormGenerateBoundary(aBoundary, &iBoundaryLen); }
	if ( !__xhttpFormAddSize(30u, iBoundaryLen, &iContentTypeNeed) ) { return XHTTP_SEMANTIC_LIMIT; }
	*pContentTypeLen = iContentTypeNeed;
	if ( !sContentType || iContentTypeCap <= iContentTypeNeed ) { return XHTTP_SEMANTIC_BUFFER_TOO_SMALL; }
	pFactory = (__xhttp_form_body_factory*)xrtCalloc(1u, sizeof(*pFactory));
	if ( !pFactory ) { return XHTTP_SEMANTIC_OUT_OF_MEMORY; }
	memcpy(pFactory->sBoundary, aBoundary, iBoundaryLen + 1u);
	pFactory->iBoundaryLen = iBoundaryLen;
	pFactory->iCount = pForm->iCount;
	if ( pForm->iCount > 0u ) {
		if ( pForm->iCount > SIZE_MAX / sizeof(*pFactory->pParts) ) { eResult = XHTTP_SEMANTIC_LIMIT; goto fail; }
		pFactory->pParts = (__xhttp_form_body_part*)xrtCalloc(pForm->iCount, sizeof(*pFactory->pParts));
		if ( !pFactory->pParts ) { eResult = XHTTP_SEMANTIC_OUT_OF_MEMORY; goto fail; }
	}
	iContentLength = (int64_t)(iBoundaryLen + 6u);
	for ( size_t i = 0u; i < pForm->iCount; ++i ) {
		const __xhttp_form_entry* pEntry = &pForm->pEntries[i];
		int64_t iPartLength = xrtHttpBodyContentLength(pEntry->pBody);
		eResult = __xhttpFormBuildPartHeader(pEntry, aBoundary, iBoundaryLen,
			&pFactory->pParts[i].pHeader, &pFactory->pParts[i].iHeaderLen);
		if ( eResult != XHTTP_SEMANTIC_OK ) { goto fail; }
		pFactory->pParts[i].pBody = xrtHttpBodyRetain(pEntry->pBody);
		if ( !pFactory->pParts[i].pBody ) { eResult = XHTTP_SEMANTIC_OUT_OF_MEMORY; goto fail; }
		if ( !xrtHttpBodyIsReplayable(pEntry->pBody) ) { iBodyFlags &= ~XHTTP_BODY_F_REPLAYABLE; }
		if ( iContentLength >= 0 ) {
			if ( iPartLength < 0 || pFactory->pParts[i].iHeaderLen > (size_t)INT64_MAX ||
				iContentLength > INT64_MAX - (int64_t)pFactory->pParts[i].iHeaderLen - 2 ||
				iPartLength > INT64_MAX - iContentLength - (int64_t)pFactory->pParts[i].iHeaderLen - 2 ) {
				iContentLength = XHTTP_BODY_LENGTH_UNKNOWN;
			} else {
				iContentLength += (int64_t)pFactory->pParts[i].iHeaderLen + iPartLength + 2;
			}
		}
	}
	pBody = xrtHttpBodyCreateFactory(__xhttpFormBodyOpen, pFactory,
		__xhttpFormBodyFactoryFree, iContentLength, iBodyFlags);
	if ( !pBody ) { eResult = XHTTP_SEMANTIC_OUT_OF_MEMORY; goto fail; }
	if ( !xrtMultipartBuildContentTypeTo(aBoundary, iBoundaryLen,
		sContentType, iContentTypeCap, pContentTypeLen) ) {
		xrtHttpBodyRelease(pBody);
		return XHTTP_SEMANTIC_BUFFER_TOO_SMALL;
	}
	*ppBody = pBody;
	return XHTTP_SEMANTIC_OK;

fail:
	__xhttpFormBodyFactoryFree(pFactory);
	return eResult;
}


XXAPI xhttpformdata* xrtHttpFormDataParseN(const char* sContentType, size_t iContentTypeLen,
	const void* pData, size_t iDataLen, const xhttpformdataconfig* pConfig,
	xhttp_semantic_result* pResult, size_t* pErrorOffset)
{
	xrtstrview tBoundary;
	xhttpformdata* pForm = NULL;
	const char* sBody = (const char*)pData;
	size_t iOffset = 0u;
	xhttp_semantic_result eResult = XHTTP_SEMANTIC_INVALID_ARGUMENT;
	if ( pResult ) { *pResult = eResult; }
	if ( pErrorOffset ) { *pErrorOffset = 0u; }
	if ( !sContentType || (!pData && iDataLen != 0u) ||
		!xrtMultipartBoundaryFromContentTypeN(sContentType, iContentTypeLen, &tBoundary) ) {
		return NULL;
	}
	pForm = xrtHttpFormDataCreateEx(pConfig);
	if ( !pForm ) { if ( pResult ) { *pResult = XHTTP_SEMANTIC_OUT_OF_MEMORY; } return NULL; }
	if ( iDataLen > pForm->tConfig.iMaxBodyBytes ) { eResult = XHTTP_SEMANTIC_LIMIT; goto fail; }
	for ( ;; ) {
		size_t iBoundaryPos = 0u;
		size_t iAfter = 0u;
		bool bFinal = false;
		xrtmultipartpartview tPart;
		xhttpbody* pPartBody = NULL;
		char* sFileName = NULL;
		size_t iFileNameLen = 0u;
		if ( !__xrtHttpUtilFindBoundaryLine(sBody, iDataLen, iOffset,
			tBoundary.sPtr, tBoundary.iLen, &iBoundaryPos, &bFinal, &iAfter) ) {
			eResult = XHTTP_SEMANTIC_INVALID_SYNTAX;
			if ( pErrorOffset ) { *pErrorOffset = iOffset; }
			goto fail;
		}
		if ( bFinal ) { break; }
		if ( !xrtMultipartNextN(sBody, iDataLen, tBoundary.sPtr, tBoundary.iLen, &iOffset, &tPart) ) {
			eResult = XHTTP_SEMANTIC_INVALID_SYNTAX;
			if ( pErrorOffset ) { *pErrorOffset = iBoundaryPos; }
			goto fail;
		}
		if ( (tPart.iFlags & XRT_MULTIPART_F_HAS_NAME) == 0u ||
			tPart.tBody.iLen > pForm->tConfig.iMaxPartBytes ) {
			eResult = (tPart.iFlags & XRT_MULTIPART_F_HAS_NAME) == 0u
				? XHTTP_SEMANTIC_INVALID_VALUE : XHTTP_SEMANTIC_LIMIT;
			if ( pErrorOffset ) { *pErrorOffset = iBoundaryPos; }
			goto fail;
		}
		if ( (tPart.iFlags & (XRT_MULTIPART_F_HAS_FILENAME | XRT_MULTIPART_F_HAS_FILENAME_EXT)) != 0u ) {
			size_t iRawLen = tPart.tFileName.iLen > tPart.tFileNameExt.iLen
				? tPart.tFileName.iLen : tPart.tFileNameExt.iLen;
			sFileName = (char*)xrtMalloc(iRawLen + 1u);
			if ( !sFileName ) { eResult = XHTTP_SEMANTIC_OUT_OF_MEMORY; goto fail; }
			if ( !xrtMultipartDecodeFileNameTo(&tPart, sFileName, iRawLen + 1u, &iFileNameLen) ) {
				xrtFree(sFileName);
				eResult = XHTTP_SEMANTIC_INVALID_VALUE;
				goto fail;
			}
		}
		pPartBody = xrtHttpBodyCreateBytesCopy(tPart.tBody.sPtr, tPart.tBody.iLen);
		if ( !pPartBody ) { xrtFree(sFileName); eResult = XHTTP_SEMANTIC_OUT_OF_MEMORY; goto fail; }
		eResult = xrtHttpFormDataAppendBodyN(pForm, tPart.tName.sPtr, tPart.tName.iLen,
			pPartBody, sFileName, iFileNameLen,
			(tPart.iFlags & XRT_MULTIPART_F_HAS_CONTENT_TYPE) != 0u ? tPart.tContentType.sPtr : NULL,
			(tPart.iFlags & XRT_MULTIPART_F_HAS_CONTENT_TYPE) != 0u ? tPart.tContentType.iLen : 0u);
		xrtHttpBodyRelease(pPartBody);
		xrtFree(sFileName);
		if ( eResult != XHTTP_SEMANTIC_OK ) { goto fail; }
		(void)iAfter;
	}
	if ( pResult ) { *pResult = XHTTP_SEMANTIC_OK; }
	return pForm;

fail:
	if ( pResult ) { *pResult = eResult; }
	xrtHttpFormDataDestroy(pForm);
	return NULL;
}


XXAPI xhttpformdata* xrtHttpFormDataParse(const char* sContentType, const void* pData,
	size_t iDataLen, const xhttpformdataconfig* pConfig, xhttp_semantic_result* pResult)
{
	return sContentType ? xrtHttpFormDataParseN(sContentType, strlen(sContentType),
		pData, iDataLen, pConfig, pResult, NULL) : NULL;
}

#endif
