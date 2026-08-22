#ifndef XRT_XCODEC_HTTP1_H
#define XRT_XCODEC_HTTP1_H

/*
	Strict HTTP/1.0 and HTTP/1.1 message codec.

	Parsed strings are views into message-owned storage. They remain valid until
	xrtCodecHttp1MessageUnit() is called. The legacy Parse functions use bounded
	defaults; ParseEx variants accept explicit limits and return structured errors.
*/

#if !defined(XRT_BUILD_CORE)

/* Inline descriptor capacity. String length is not limited by these values. */
#define XCODEC_HTTP1_MAX_HEADERS 32u
#define XCODEC_HTTP1_MAX_TRAILERS 16u

/* Legacy source-compatibility constants. Parsers no longer truncate to them. */
#define XCODEC_HTTP1_TOKEN_CAP   32u
#define XCODEC_HTTP1_TARGET_CAP  256u
#define XCODEC_HTTP1_VALUE_CAP   256u
#define XCODEC_HTTP1_REASON_CAP  128u

#define XCODEC_HTTP1_F_NONE       0x00000000u
#define XCODEC_HTTP1_F_REQUEST    0x00000001u
#define XCODEC_HTTP1_F_RESPONSE   0x00000002u
#define XCODEC_HTTP1_F_CHUNKED    0x00000004u
#define XCODEC_HTTP1_F_KEEPALIVE  0x00000008u
#define XCODEC_HTTP1_F_UPGRADE    0x00000010u
#define XCODEC_HTTP1_F_HAS_TRAILERS 0x00000020u

#define XCODEC_HTTP1_LIMITS_VERSION 1u
#define XCODEC_HTTP1_DEFAULT_START_LINE_BYTES 8192u
#define XCODEC_HTTP1_DEFAULT_HEADER_BYTES 65536u
#define XCODEC_HTTP1_DEFAULT_HEADER_LINE_BYTES 8192u
#define XCODEC_HTTP1_DEFAULT_HEADER_COUNT 100u
#define XCODEC_HTTP1_DEFAULT_CHUNK_LINE_BYTES 8192u
#define XCODEC_HTTP1_DEFAULT_TRAILER_BYTES 16384u
#define XCODEC_HTTP1_DEFAULT_TRAILER_COUNT 32u
#define XCODEC_HTTP1_DEFAULT_BODY_BYTES UINT64_C(67108864)

typedef enum xrt_codec_http1_error {
	XCODEC_HTTP1_ERROR_NONE = 0,
	XCODEC_HTTP1_ERROR_INVALID_ARGUMENT,
	XCODEC_HTTP1_ERROR_OUT_OF_MEMORY,
	XCODEC_HTTP1_ERROR_HEADER_TOO_LARGE,
	XCODEC_HTTP1_ERROR_START_LINE_TOO_LARGE,
	XCODEC_HTTP1_ERROR_HEADER_LINE_TOO_LARGE,
	XCODEC_HTTP1_ERROR_TOO_MANY_HEADERS,
	XCODEC_HTTP1_ERROR_INVALID_START_LINE,
	XCODEC_HTTP1_ERROR_INVALID_METHOD,
	XCODEC_HTTP1_ERROR_INVALID_TARGET,
	XCODEC_HTTP1_ERROR_INVALID_VERSION,
	XCODEC_HTTP1_ERROR_INVALID_STATUS,
	XCODEC_HTTP1_ERROR_INVALID_REASON,
	XCODEC_HTTP1_ERROR_INVALID_HEADER_NAME,
	XCODEC_HTTP1_ERROR_INVALID_HEADER_VALUE,
	XCODEC_HTTP1_ERROR_INVALID_CONTENT_LENGTH,
	XCODEC_HTTP1_ERROR_CONFLICTING_CONTENT_LENGTH,
	XCODEC_HTTP1_ERROR_TRANSFER_ENCODING_CONTENT_LENGTH,
	XCODEC_HTTP1_ERROR_INVALID_TRANSFER_ENCODING,
	XCODEC_HTTP1_ERROR_UNSUPPORTED_TRANSFER_ENCODING,
	XCODEC_HTTP1_ERROR_INVALID_CHUNK_SIZE,
	XCODEC_HTTP1_ERROR_CHUNK_LINE_TOO_LARGE,
	XCODEC_HTTP1_ERROR_INVALID_CHUNK_TERMINATOR,
	XCODEC_HTTP1_ERROR_BODY_TOO_LARGE,
	XCODEC_HTTP1_ERROR_TRAILER_TOO_LARGE,
	XCODEC_HTTP1_ERROR_TOO_MANY_TRAILERS,
	XCODEC_HTTP1_ERROR_INVALID_TRAILER,
	XCODEC_HTTP1_ERROR_FORBIDDEN_TRAILER
} xcodechttp1error;

typedef struct xrt_codec_http1_error_info {
	xcodechttp1error eCode;
	size_t iOffset;
	uint32 iLine;
} xcodechttp1errorinfo;

typedef struct xrt_codec_http1_limits {
	uint32 iSize;
	uint32 iVersion;
	size_t iMaxStartLineBytes;
	size_t iMaxHeaderBytes;
	size_t iMaxHeaderLineBytes;
	uint32 iMaxHeaderCount;
	size_t iMaxChunkLineBytes;
	size_t iMaxTrailerBytes;
	uint32 iMaxTrailerCount;
	uint64 iMaxBodyBytes;
} xcodechttp1limits;

#define XCODEC_HTTP1_LIMITS_V1_SIZE ((uint32)(offsetof(xcodechttp1limits, iMaxBodyBytes) + sizeof(uint64)))

typedef struct xrt_codec_http1_header {
	const char* sName;
	const char* sValue;
	size_t iNameLen;
	size_t iValueLen;
} xcodechttp1header;

typedef struct xrt_codec_http1_message {
	uint32 iFlags;
	uint32 iHeaderCount;
	uint32 iHeaderCap;
	uint32 iTrailerCount;
	uint32 iTrailerCap;
	uint32 iStatusCode;
	int64_t iContentLength;
	size_t iHeadBytes;
	const char* sMethod;
	const char* sTarget;
	const char* sVersion;
	const char* sReason;
	size_t iMethodLen;
	size_t iTargetLen;
	size_t iVersionLen;
	size_t iReasonLen;
	xcodechttp1header arrHeaders[XCODEC_HTTP1_MAX_HEADERS];
	xcodechttp1header* pHeaders;
	xcodechttp1header arrTrailers[XCODEC_HTTP1_MAX_TRAILERS];
	xcodechttp1header* pTrailers;
	char* pHeaderStorage;
	char* pTrailerStorage;
	xcodechttp1errorinfo tError;
} xcodechttp1msg;

#endif /* !XRT_BUILD_CORE */


static char __xcodecHttpToLower(char ch)
{
	return (ch >= 'A' && ch <= 'Z') ? (char)(ch + ('a' - 'A')) : ch;
}


static bool __xcodecHttpStrEqNoCase(const char* sA, const char* sB)
{
	size_t i = 0u;
	if ( !sA || !sB ) { return false; }
	while ( sA[i] && sB[i] ) {
		if ( __xcodecHttpToLower(sA[i]) != __xcodecHttpToLower(sB[i]) ) { return false; }
		i++;
	}
	return sA[i] == '\0' && sB[i] == '\0';
}


static bool __xcodecHttpIsTchar(unsigned char ch)
{
	if ( (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ) { return true; }
	return ch == '!' || ch == '#' || ch == '$' || ch == '%' || ch == '&' || ch == '\'' ||
		ch == '*' || ch == '+' || ch == '-' || ch == '.' || ch == '^' || ch == '_' ||
		ch == '`' || ch == '|' || ch == '~';
}


static bool __xcodecHttpValidToken(const char* sText, size_t iLen)
{
	if ( !sText || iLen == 0u ) { return false; }
	for ( size_t i = 0u; i < iLen; ++i ) {
		if ( !__xcodecHttpIsTchar((unsigned char)sText[i]) ) { return false; }
	}
	return true;
}


static bool __xcodecHttpValidFieldValue(const char* sText, size_t iLen)
{
	if ( !sText && iLen != 0u ) { return false; }
	for ( size_t i = 0u; i < iLen; ++i ) {
		unsigned char ch = (unsigned char)sText[i];
		if ( ch == '\t' || ch >= 0x20u ) {
			if ( ch != 0x7fu ) { continue; }
		}
		return false;
	}
	return true;
}


static bool __xcodecHttpValidReason(const char* sText, size_t iLen)
{
	return __xcodecHttpValidFieldValue(sText, iLen);
}


static bool __xcodecHttpValidTarget(const char* sText, size_t iLen)
{
	if ( !sText || iLen == 0u ) { return false; }
	for ( size_t i = 0u; i < iLen; ++i ) {
		unsigned char ch = (unsigned char)sText[i];
		if ( ch <= 0x20u || ch == 0x7fu || ch == '#') { return false; }
	}
	return true;
}


static bool __xcodecHttpVersionSupported(const char* sText, size_t iLen)
{
	return iLen == 8u && memcmp(sText, "HTTP/1.", 7u) == 0 && (sText[7] == '0' || sText[7] == '1');
}


static void __xcodecHttpTrimView(char** ppText, size_t* pLen)
{
	char* sText;
	size_t iLen;
	if ( !ppText || !pLen || !*ppText ) { return; }
	sText = *ppText;
	iLen = *pLen;
	while ( iLen > 0u && (*sText == ' ' || *sText == '\t') ) { sText++; iLen--; }
	while ( iLen > 0u && (sText[iLen - 1u] == ' ' || sText[iLen - 1u] == '\t') ) { iLen--; }
	*ppText = sText;
	*pLen = iLen;
}


static bool __xcodecHttpTokenEqNoCase(const char* sText, size_t iLen, const char* sToken)
{
	size_t iTokenLen;
	if ( !sText || !sToken ) { return false; }
	iTokenLen = strlen(sToken);
	if ( iLen != iTokenLen ) { return false; }
	for ( size_t i = 0u; i < iLen; ++i ) {
		if ( __xcodecHttpToLower(sText[i]) != __xcodecHttpToLower(sToken[i]) ) { return false; }
	}
	return true;
}


static bool __xcodecHttpContainsTokenNoCase(const char* sText, const char* sToken)
{
	const char* p;
	if ( !sText || !sToken || !sToken[0] ) { return false; }
	p = sText;
	while ( *p ) {
		const char* sBegin;
		const char* sEnd;
		while ( *p == ' ' || *p == '\t' || *p == ',' ) { p++; }
		sBegin = p;
		while ( *p && *p != ',' ) { p++; }
		sEnd = p;
		while ( sEnd > sBegin && (sEnd[-1] == ' ' || sEnd[-1] == '\t') ) { sEnd--; }
		if ( __xcodecHttpTokenEqNoCase(sBegin, (size_t)(sEnd - sBegin), sToken) ) { return true; }
		if ( *p == ',' ) { p++; }
	}
	return false;
}


XXAPI void xrtCodecHttp1LimitsInit(xcodechttp1limits* pLimits)
{
	if ( !pLimits ) { return; }
	memset(pLimits, 0, sizeof(*pLimits));
	pLimits->iSize = XCODEC_HTTP1_LIMITS_V1_SIZE;
	pLimits->iVersion = XCODEC_HTTP1_LIMITS_VERSION;
	pLimits->iMaxStartLineBytes = XCODEC_HTTP1_DEFAULT_START_LINE_BYTES;
	pLimits->iMaxHeaderBytes = XCODEC_HTTP1_DEFAULT_HEADER_BYTES;
	pLimits->iMaxHeaderLineBytes = XCODEC_HTTP1_DEFAULT_HEADER_LINE_BYTES;
	pLimits->iMaxHeaderCount = XCODEC_HTTP1_DEFAULT_HEADER_COUNT;
	pLimits->iMaxChunkLineBytes = XCODEC_HTTP1_DEFAULT_CHUNK_LINE_BYTES;
	pLimits->iMaxTrailerBytes = XCODEC_HTTP1_DEFAULT_TRAILER_BYTES;
	pLimits->iMaxTrailerCount = XCODEC_HTTP1_DEFAULT_TRAILER_COUNT;
	pLimits->iMaxBodyBytes = XCODEC_HTTP1_DEFAULT_BODY_BYTES;
}


XXAPI const char* xrtCodecHttp1ErrorName(xcodechttp1error eError)
{
	switch ( eError ) {
		case XCODEC_HTTP1_ERROR_NONE: return "none";
		case XCODEC_HTTP1_ERROR_INVALID_ARGUMENT: return "invalid_argument";
		case XCODEC_HTTP1_ERROR_OUT_OF_MEMORY: return "out_of_memory";
		case XCODEC_HTTP1_ERROR_HEADER_TOO_LARGE: return "header_too_large";
		case XCODEC_HTTP1_ERROR_START_LINE_TOO_LARGE: return "start_line_too_large";
		case XCODEC_HTTP1_ERROR_HEADER_LINE_TOO_LARGE: return "header_line_too_large";
		case XCODEC_HTTP1_ERROR_TOO_MANY_HEADERS: return "too_many_headers";
		case XCODEC_HTTP1_ERROR_INVALID_START_LINE: return "invalid_start_line";
		case XCODEC_HTTP1_ERROR_INVALID_METHOD: return "invalid_method";
		case XCODEC_HTTP1_ERROR_INVALID_TARGET: return "invalid_target";
		case XCODEC_HTTP1_ERROR_INVALID_VERSION: return "invalid_version";
		case XCODEC_HTTP1_ERROR_INVALID_STATUS: return "invalid_status";
		case XCODEC_HTTP1_ERROR_INVALID_REASON: return "invalid_reason";
		case XCODEC_HTTP1_ERROR_INVALID_HEADER_NAME: return "invalid_header_name";
		case XCODEC_HTTP1_ERROR_INVALID_HEADER_VALUE: return "invalid_header_value";
		case XCODEC_HTTP1_ERROR_INVALID_CONTENT_LENGTH: return "invalid_content_length";
		case XCODEC_HTTP1_ERROR_CONFLICTING_CONTENT_LENGTH: return "conflicting_content_length";
		case XCODEC_HTTP1_ERROR_TRANSFER_ENCODING_CONTENT_LENGTH: return "transfer_encoding_content_length";
		case XCODEC_HTTP1_ERROR_INVALID_TRANSFER_ENCODING: return "invalid_transfer_encoding";
		case XCODEC_HTTP1_ERROR_UNSUPPORTED_TRANSFER_ENCODING: return "unsupported_transfer_encoding";
		case XCODEC_HTTP1_ERROR_INVALID_CHUNK_SIZE: return "invalid_chunk_size";
		case XCODEC_HTTP1_ERROR_CHUNK_LINE_TOO_LARGE: return "chunk_line_too_large";
		case XCODEC_HTTP1_ERROR_INVALID_CHUNK_TERMINATOR: return "invalid_chunk_terminator";
		case XCODEC_HTTP1_ERROR_BODY_TOO_LARGE: return "body_too_large";
		case XCODEC_HTTP1_ERROR_TRAILER_TOO_LARGE: return "trailer_too_large";
		case XCODEC_HTTP1_ERROR_TOO_MANY_TRAILERS: return "too_many_trailers";
		case XCODEC_HTTP1_ERROR_INVALID_TRAILER: return "invalid_trailer";
		case XCODEC_HTTP1_ERROR_FORBIDDEN_TRAILER: return "forbidden_trailer";
		default: return "unknown";
	}
}


XXAPI void xrtCodecHttp1MessageInit(xcodechttp1msg* pMsg)
{
	if ( !pMsg ) { return; }
	memset(pMsg, 0, sizeof(*pMsg));
	pMsg->pHeaders = pMsg->arrHeaders;
	pMsg->iHeaderCap = XCODEC_HTTP1_MAX_HEADERS;
	pMsg->pTrailers = pMsg->arrTrailers;
	pMsg->iTrailerCap = XCODEC_HTTP1_MAX_TRAILERS;
	pMsg->iContentLength = -1;
}


XXAPI void xrtCodecHttp1MessageUnit(xcodechttp1msg* pMsg)
{
	if ( !pMsg ) { return; }
	if ( pMsg->pHeaderStorage ) { XNET_FREE(pMsg->pHeaderStorage); }
	if ( pMsg->pTrailerStorage ) { XNET_FREE(pMsg->pTrailerStorage); }
	if ( pMsg->pHeaders && pMsg->pHeaders != pMsg->arrHeaders ) { XNET_FREE(pMsg->pHeaders); }
	if ( pMsg->pTrailers && pMsg->pTrailers != pMsg->arrTrailers ) { XNET_FREE(pMsg->pTrailers); }
	memset(pMsg, 0, sizeof(*pMsg));
	pMsg->iContentLength = -1;
}


static const xcodechttp1header* __xcodecHttpGetHeaderAt(const xcodechttp1header* pHeaders, uint32 iCount, uint32 iIndex)
{
	if ( !pHeaders || iIndex >= iCount ) { return NULL; }
	return &pHeaders[iIndex];
}


XXAPI const xcodechttp1header* xrtCodecHttp1HeaderAt(const xcodechttp1msg* pMsg, uint32 iIndex)
{
	return pMsg ? __xcodecHttpGetHeaderAt(pMsg->pHeaders, pMsg->iHeaderCount, iIndex) : NULL;
}


XXAPI const xcodechttp1header* xrtCodecHttp1TrailerAt(const xcodechttp1msg* pMsg, uint32 iIndex)
{
	return pMsg ? __xcodecHttpGetHeaderAt(pMsg->pTrailers, pMsg->iTrailerCount, iIndex) : NULL;
}


static const char* __xcodecHttpFindHeader(const xcodechttp1header* pHeaders, uint32 iCount, const char* sName)
{
	if ( !pHeaders || !sName ) { return NULL; }
	for ( uint32 i = 0u; i < iCount; ++i ) {
		if ( __xcodecHttpStrEqNoCase(pHeaders[i].sName, sName) ) { return pHeaders[i].sValue; }
	}
	return NULL;
}


XXAPI const char* xrtCodecHttp1GetHeader(const xcodechttp1msg* pMsg, const char* sName)
{
	return pMsg ? __xcodecHttpFindHeader(pMsg->pHeaders, pMsg->iHeaderCount, sName) : NULL;
}


XXAPI const char* xrtCodecHttp1GetTrailer(const xcodechttp1msg* pMsg, const char* sName)
{
	return pMsg ? __xcodecHttpFindHeader(pMsg->pTrailers, pMsg->iTrailerCount, sName) : NULL;
}


static bool __xcodecHttpEnsureDescriptorCap(xcodechttp1header** ppItems, xcodechttp1header* pInline,
	uint32* pCap, uint32 iCount, uint32 iNeed)
{
	xcodechttp1header* pNew;
	uint32 iNewCap;
	if ( !ppItems || !pInline || !pCap ) { return false; }
	if ( iNeed <= *pCap ) { return true; }
	iNewCap = *pCap ? *pCap : 1u;
	while ( iNewCap < iNeed ) {
		if ( iNewCap > UINT32_MAX / 2u ) { return false; }
		iNewCap *= 2u;
	}
#if SIZE_MAX == UINT32_MAX
	if ( iNewCap > (uint32)(SIZE_MAX / sizeof(*pNew)) ) { return false; }
#endif
	pNew = (xcodechttp1header*)XNET_ALLOC(sizeof(*pNew) * iNewCap);
	if ( !pNew ) { return false; }
	memset(pNew, 0, sizeof(*pNew) * iNewCap);
	if ( iCount > 0u ) { memcpy(pNew, *ppItems, sizeof(*pNew) * iCount); }
	if ( *ppItems && *ppItems != pInline ) { XNET_FREE(*ppItems); }
	*ppItems = pNew;
	*pCap = iNewCap;
	return true;
}


static bool __xcodecHttpResolveLimits(const xcodechttp1limits* pInput, xcodechttp1limits* pOut)
{
	if ( !pOut ) { return false; }
	xrtCodecHttp1LimitsInit(pOut);
	if ( !pInput ) { return true; }
	if ( pInput->iSize < XCODEC_HTTP1_LIMITS_V1_SIZE ||
		(pInput->iVersion != 0u && pInput->iVersion != XCODEC_HTTP1_LIMITS_VERSION) ) { return false; }
	*pOut = *pInput;
	if ( pOut->iMaxStartLineBytes == 0u || pOut->iMaxHeaderBytes == 0u ||
		pOut->iMaxHeaderLineBytes == 0u || pOut->iMaxHeaderCount == 0u ||
		pOut->iMaxChunkLineBytes == 0u || pOut->iMaxTrailerBytes == 0u ||
		pOut->iMaxTrailerCount == 0u || pOut->iMaxBodyBytes == 0u ) { return false; }
	return true;
}


static xcodecstatus __xcodecHttpSetError(xcodechttp1msg* pMsg, xcodechttp1errorinfo* pError,
	xcodechttp1error eCode, size_t iOffset, uint32 iLine)
{
	xcodechttp1errorinfo tError;
	tError.eCode = eCode;
	tError.iOffset = iOffset;
	tError.iLine = iLine;
	if ( pMsg ) {
		xrtCodecHttp1MessageUnit(pMsg);
		pMsg->tError = tError;
	}
	if ( pError ) { *pError = tError; }
	return XCODEC_STATUS_ERROR;
}


static void __xcodecHttpClearError(xcodechttp1errorinfo* pError)
{
	if ( pError ) { memset(pError, 0, sizeof(*pError)); }
}


static size_t __xcodecHttpFindCrlf(const char* sText, size_t iStart, size_t iEnd)
{
	if ( !sText || iStart >= iEnd ) { return (size_t)-1; }
	for ( size_t i = iStart; i + 1u < iEnd; ++i ) {
		if ( sText[i] == '\r' && sText[i + 1u] == '\n' ) { return i; }
	}
	return (size_t)-1;
}


static bool __xcodecHttpParseContentLength(const char* sText, int64_t* pValue, bool* pHasValue)
{
	const char* p = sText;
	if ( !p || !pValue || !pHasValue ) { return false; }
	for ( ;; ) {
		uint64 iValue = 0u;
		bool bDigit = false;
		while ( *p == ' ' || *p == '\t' ) { p++; }
		while ( *p >= '0' && *p <= '9' ) {
			uint32 iDigit = (uint32)(*p - '0');
			if ( iValue > (uint64)(INT64_MAX - iDigit) / 10u ) { return false; }
			iValue = iValue * 10u + iDigit;
			bDigit = true;
			p++;
		}
		if ( !bDigit ) { return false; }
		while ( *p == ' ' || *p == '\t' ) { p++; }
		if ( *p != '\0' && *p != ',' ) { return false; }
		if ( *pHasValue && *pValue != (int64_t)iValue ) { return false; }
		*pValue = (int64_t)iValue;
		*pHasValue = true;
		if ( *p == '\0' ) { return true; }
		p++;
		if ( *p == '\0' ) { return false; }
	}
}


static bool __xcodecHttpParseTransferEncoding(const char* sText, bool* pSeen, bool* pFinalChunked,
	uint32* pChunkedCount, bool* pUnsupported)
{
	const char* p = sText;
	if ( !p || !pSeen || !pFinalChunked || !pChunkedCount || !pUnsupported ) { return false; }
	for ( ;; ) {
		const char* sBegin;
		const char* sEnd;
		const char* sSemi;
		while ( *p == ' ' || *p == '\t' ) { p++; }
		sBegin = p;
		while ( *p && *p != ',' ) { p++; }
		sEnd = p;
		while ( sEnd > sBegin && (sEnd[-1] == ' ' || sEnd[-1] == '\t') ) { sEnd--; }
		if ( sEnd == sBegin ) { return false; }
		sSemi = sBegin;
		while ( sSemi < sEnd && *sSemi != ';' ) { sSemi++; }
		{
			const char* sCodingEnd = sSemi;
			while ( sCodingEnd > sBegin && (sCodingEnd[-1] == ' ' || sCodingEnd[-1] == '\t') ) { sCodingEnd--; }
			if ( !__xcodecHttpValidToken(sBegin, (size_t)(sCodingEnd - sBegin)) ) { return false; }
			*pSeen = true;
			*pFinalChunked = __xcodecHttpTokenEqNoCase(sBegin, (size_t)(sCodingEnd - sBegin), "chunked");
			if ( *pFinalChunked ) {
				(*pChunkedCount)++;
				if ( sSemi != sEnd ) { return false; }
			} else { *pUnsupported = true; }
		}
		if ( sSemi < sEnd ) {
			/* Validate parameters conservatively; quoted strings may contain separators. */
			bool bQuoted = false;
			bool bEscape = false;
			for ( const char* q = sSemi + 1; q < sEnd; ++q ) {
				unsigned char ch = (unsigned char)*q;
				if ( bEscape ) { bEscape = false; continue; }
				if ( bQuoted && ch == '\\' ) { bEscape = true; continue; }
				if ( ch == '"' ) { bQuoted = !bQuoted; continue; }
				if ( ch < 0x20u && ch != '\t' ) { return false; }
			}
			if ( bQuoted || bEscape ) { return false; }
		}
		if ( *p == '\0' ) { return true; }
		p++;
		if ( *p == '\0' ) { return false; }
	}
}


static xcodecstatus __xcodecHttpParseHeadStorage(char* sStorage, size_t iHeadBytes,
	xcodecframe* pFrame, xcodechttp1msg* pMsg, const xcodechttp1limits* pLimits,
	xcodechttp1errorinfo* pError)
{
	size_t iLineEnd;
	size_t iCursor;
	uint32 iLine = 1u;
	bool bHasContentLength = false;
	bool bHasTransferEncoding = false;
	bool bFinalChunked = false;
	bool bUnsupportedTransferEncoding = false;
	uint32 iChunkedCount = 0u;
	bool bConnectionClose = false;
	bool bConnectionKeepAlive = false;
	if ( !sStorage || !pFrame || !pMsg || !pLimits || iHeadBytes < 4u ) {
		return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_ARGUMENT, 0u, 0u);
	}
	iLineEnd = __xcodecHttpFindCrlf(sStorage, 0u, iHeadBytes);
	if ( iLineEnd == (size_t)-1 ) {
		return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_START_LINE, 0u, 1u);
	}
	if ( iLineEnd > pLimits->iMaxStartLineBytes ) {
		return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_START_LINE_TOO_LARGE, iLineEnd, 1u);
	}
	if ( iLineEnd == 0u ) {
		return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_START_LINE, 0u, 1u);
	}
	if ( iLineEnd >= 5u && memcmp(sStorage, "HTTP/", 5u) == 0 ) {
		char* sFirstSpace = (char*)memchr(sStorage, ' ', iLineEnd);
		char* sSecondSpace;
		char* sStatus;
		char* sReason;
		if ( !sFirstSpace || sFirstSpace == sStorage ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_START_LINE, 0u, 1u);
		}
		sStatus = sFirstSpace + 1;
		sSecondSpace = (char*)memchr(sStatus, ' ', iLineEnd - (size_t)(sStatus - sStorage));
		if ( sSecondSpace ) {
			sReason = sSecondSpace + 1;
		} else {
			sReason = sStorage + iLineEnd;
		}
		if ( (sSecondSpace ? (size_t)(sSecondSpace - sStatus) : iLineEnd - (size_t)(sStatus - sStorage)) != 3u ||
			sStatus[0] < '1' || sStatus[0] > '9' || sStatus[1] < '0' || sStatus[1] > '9' ||
			sStatus[2] < '0' || sStatus[2] > '9' ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_STATUS, (size_t)(sStatus - sStorage), 1u);
		}
		pMsg->iVersionLen = (size_t)(sFirstSpace - sStorage);
		if ( !__xcodecHttpVersionSupported(sStorage, pMsg->iVersionLen) ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_VERSION, 0u, 1u);
		}
		pMsg->iStatusCode = (uint32)((sStatus[0] - '0') * 100 + (sStatus[1] - '0') * 10 + (sStatus[2] - '0'));
		pMsg->iReasonLen = sSecondSpace ? iLineEnd - (size_t)(sReason - sStorage) : 0u;
		if ( !__xcodecHttpValidReason(sReason, pMsg->iReasonLen) ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_REASON, (size_t)(sReason - sStorage), 1u);
		}
		*sFirstSpace = '\0';
		if ( sSecondSpace ) { *sSecondSpace = '\0'; }
		sStorage[iLineEnd] = '\0';
		pMsg->sVersion = sStorage;
		pMsg->sReason = sReason;
		pMsg->iFlags |= XCODEC_HTTP1_F_RESPONSE;
		pFrame->iFlags |= XCODEC_FRAME_F_RESPONSE;
	} else {
		char* sFirstSpace = (char*)memchr(sStorage, ' ', iLineEnd);
		char* sTarget;
		char* sSecondSpace;
		char* sVersion;
		if ( !sFirstSpace || sFirstSpace == sStorage ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_START_LINE, 0u, 1u);
		}
		sTarget = sFirstSpace + 1;
		sSecondSpace = (char*)memchr(sTarget, ' ', iLineEnd - (size_t)(sTarget - sStorage));
		if ( !sSecondSpace || sSecondSpace == sTarget ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_START_LINE, (size_t)(sTarget - sStorage), 1u);
		}
		sVersion = sSecondSpace + 1;
		if ( memchr(sVersion, ' ', iLineEnd - (size_t)(sVersion - sStorage)) != NULL || sVersion == sStorage + iLineEnd ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_START_LINE, (size_t)(sVersion - sStorage), 1u);
		}
		pMsg->iMethodLen = (size_t)(sFirstSpace - sStorage);
		pMsg->iTargetLen = (size_t)(sSecondSpace - sTarget);
		pMsg->iVersionLen = iLineEnd - (size_t)(sVersion - sStorage);
		if ( !__xcodecHttpValidToken(sStorage, pMsg->iMethodLen) ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_METHOD, 0u, 1u);
		}
		if ( !__xcodecHttpValidTarget(sTarget, pMsg->iTargetLen) ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_TARGET, (size_t)(sTarget - sStorage), 1u);
		}
		if ( !__xcodecHttpVersionSupported(sVersion, pMsg->iVersionLen) ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_VERSION, (size_t)(sVersion - sStorage), 1u);
		}
		*sFirstSpace = '\0';
		*sSecondSpace = '\0';
		sStorage[iLineEnd] = '\0';
		pMsg->sMethod = sStorage;
		pMsg->sTarget = sTarget;
		pMsg->sVersion = sVersion;
		pMsg->sReason = "";
		pMsg->iFlags |= XCODEC_HTTP1_F_REQUEST;
		pFrame->iFlags |= XCODEC_FRAME_F_REQUEST;
	}

	iCursor = iLineEnd + 2u;
	while ( iCursor + 1u < iHeadBytes ) {
		char* sLine;
		char* sColon;
		char* sValue;
		size_t iNext;
		size_t iNameLen;
		size_t iValueLen;
		xcodechttp1header* pHeader;
		iLine++;
		if ( sStorage[iCursor] == '\r' && sStorage[iCursor + 1u] == '\n' ) { break; }
		iNext = __xcodecHttpFindCrlf(sStorage, iCursor, iHeadBytes);
		if ( iNext == (size_t)-1 ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_HEADER_VALUE, iCursor, iLine);
		}
		if ( iNext - iCursor > pLimits->iMaxHeaderLineBytes ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_HEADER_LINE_TOO_LARGE, iCursor, iLine);
		}
		sLine = sStorage + iCursor;
		if ( sLine[0] == ' ' || sLine[0] == '\t' ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_HEADER_NAME, iCursor, iLine);
		}
		sColon = (char*)memchr(sLine, ':', iNext - iCursor);
		if ( !sColon ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_HEADER_NAME, iCursor, iLine);
		}
		iNameLen = (size_t)(sColon - sLine);
		if ( !__xcodecHttpValidToken(sLine, iNameLen) ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_HEADER_NAME, iCursor, iLine);
		}
		sValue = sColon + 1;
		iValueLen = iNext - (size_t)(sValue - sStorage);
		__xcodecHttpTrimView(&sValue, &iValueLen);
		if ( !__xcodecHttpValidFieldValue(sValue, iValueLen) ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_HEADER_VALUE, (size_t)(sValue - sStorage), iLine);
		}
		if ( pMsg->iHeaderCount >= pLimits->iMaxHeaderCount ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_TOO_MANY_HEADERS, iCursor, iLine);
		}
		if ( !__xcodecHttpEnsureDescriptorCap(&pMsg->pHeaders, pMsg->arrHeaders, &pMsg->iHeaderCap,
			pMsg->iHeaderCount, pMsg->iHeaderCount + 1u) ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_OUT_OF_MEMORY, iCursor, iLine);
		}
		*sColon = '\0';
		sValue[iValueLen] = '\0';
		sStorage[iNext] = '\0';
		pHeader = &pMsg->pHeaders[pMsg->iHeaderCount++];
		pHeader->sName = sLine;
		pHeader->sValue = sValue;
		pHeader->iNameLen = iNameLen;
		pHeader->iValueLen = iValueLen;
		if ( __xcodecHttpStrEqNoCase(sLine, "Content-Length") ) {
			int64_t iParsed = -1;
			bool bParsed = false;
			if ( !__xcodecHttpParseContentLength(sValue, &iParsed, &bParsed) ) {
				return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_CONTENT_LENGTH,
					(size_t)(sValue - sStorage), iLine);
			}
			if ( bHasContentLength && pMsg->iContentLength != iParsed ) {
				return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_CONFLICTING_CONTENT_LENGTH,
					(size_t)(sValue - sStorage), iLine);
			}
			pMsg->iContentLength = iParsed;
			bHasContentLength = true;
		} else if ( __xcodecHttpStrEqNoCase(sLine, "Transfer-Encoding") ) {
			if ( !__xcodecHttpParseTransferEncoding(sValue, &bHasTransferEncoding, &bFinalChunked,
				&iChunkedCount, &bUnsupportedTransferEncoding) ) {
				return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_TRANSFER_ENCODING,
					(size_t)(sValue - sStorage), iLine);
			}
		} else if ( __xcodecHttpStrEqNoCase(sLine, "Connection") ) {
			if ( __xcodecHttpContainsTokenNoCase(sValue, "close") ) { bConnectionClose = true; }
			if ( __xcodecHttpContainsTokenNoCase(sValue, "keep-alive") ) { bConnectionKeepAlive = true; }
			if ( __xcodecHttpContainsTokenNoCase(sValue, "upgrade") ) {
				pMsg->iFlags |= XCODEC_HTTP1_F_UPGRADE;
				pFrame->iFlags |= XCODEC_FRAME_F_UPGRADE;
			}
		} else if ( __xcodecHttpStrEqNoCase(sLine, "Upgrade") && iValueLen > 0u ) {
			pMsg->iFlags |= XCODEC_HTTP1_F_UPGRADE;
			pFrame->iFlags |= XCODEC_FRAME_F_UPGRADE;
		}
		iCursor = iNext + 2u;
	}
	if ( bHasTransferEncoding ) {
		if ( bHasContentLength ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_TRANSFER_ENCODING_CONTENT_LENGTH, 0u, 0u);
		}
		if ( bUnsupportedTransferEncoding || !bFinalChunked || iChunkedCount != 1u ||
			!__xcodecHttpStrEqNoCase(pMsg->sVersion, "HTTP/1.1") ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_UNSUPPORTED_TRANSFER_ENCODING, 0u, 0u);
		}
		pMsg->iFlags |= XCODEC_HTTP1_F_CHUNKED;
		pFrame->iFlags |= XCODEC_FRAME_F_CHUNKED;
	}
	if ( (!bConnectionClose && __xcodecHttpStrEqNoCase(pMsg->sVersion, "HTTP/1.1")) ||
		(!bConnectionClose && bConnectionKeepAlive) ) {
		pMsg->iFlags |= XCODEC_HTTP1_F_KEEPALIVE;
		pFrame->iFlags |= XCODEC_FRAME_F_KEEPALIVE;
	}
	pMsg->iHeadBytes = iHeadBytes;
	pFrame->iKind = XCODEC_KIND_HTTP1;
	pFrame->iHeaderBytes = iHeadBytes;
	pFrame->iPayloadOffset = iHeadBytes;
	return XCODEC_STATUS_FRAME;
}


static bool __xcodecHttpParseHexU64(const char* sText, size_t iLen, uint64* pValue)
{
	uint64 iValue = 0u;
	size_t i = 0u;
	bool bDigit = false;
	if ( !sText || !pValue || iLen == 0u ) { return false; }
	while ( i < iLen ) {
		uint32 iDigit;
		unsigned char ch = (unsigned char)sText[i];
		if ( ch == ';' ) { break; }
		if ( ch >= '0' && ch <= '9' ) { iDigit = (uint32)(ch - '0'); }
		else if ( ch >= 'a' && ch <= 'f' ) { iDigit = 10u + (uint32)(ch - 'a'); }
		else if ( ch >= 'A' && ch <= 'F' ) { iDigit = 10u + (uint32)(ch - 'A'); }
		else { return false; }
		if ( iValue > (UINT64_MAX - iDigit) / 16u ) { return false; }
		iValue = iValue * 16u + iDigit;
		bDigit = true;
		i++;
	}
	if ( !bDigit ) { return false; }
	while ( i < iLen ) {
		const char* sName;
		size_t iNameLen;
		if ( sText[i++] != ';' ) { return false; }
		sName = sText + i;
		while ( i < iLen && __xcodecHttpIsTchar((unsigned char)sText[i]) ) { i++; }
		iNameLen = (size_t)((sText + i) - sName);
		if ( iNameLen == 0u ) { return false; }
		if ( i < iLen && sText[i] == '=' ) {
			i++;
			if ( i >= iLen ) { return false; }
			if ( sText[i] == '"' ) {
				bool bClosed = false;
				i++;
				while ( i < iLen ) {
					unsigned char ch = (unsigned char)sText[i++];
					if ( ch == '"' ) { bClosed = true; break; }
					if ( ch == '\\' ) {
						if ( i >= iLen || (unsigned char)sText[i] < 0x20u ) { return false; }
						i++;
					} else if ( ch < 0x20u || ch == 0x7fu ) { return false; }
				}
				if ( !bClosed ) { return false; }
			} else {
				size_t iBegin = i;
				while ( i < iLen && __xcodecHttpIsTchar((unsigned char)sText[i]) ) { i++; }
				if ( i == iBegin ) { return false; }
			}
		}
		if ( i < iLen && sText[i] != ';' ) { return false; }
	}
	*pValue = iValue;
	return true;
}


static xcodecstatus __xcodecHttpReadChunkHeaderEx(const xnetchain* pInput, size_t iOffset,
	const xcodechttp1limits* pLimits, uint64* pChunkSize, size_t* pDataOffset,
	xcodechttp1msg* pMsg, xcodechttp1errorinfo* pError)
{
	static const uint8 aCrlf[] = { '\r', '\n' };
	size_t iLineEnd;
	size_t iLineLen;
	size_t iInputBytes;
	char* sLine;
	if ( !pInput || !pLimits || !pChunkSize ) {
		return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_ARGUMENT, iOffset, 0u);
	}
	iInputBytes = xrtNetChainBytes(pInput);
	iLineEnd = __xcodecChainFindPattern(pInput, aCrlf, sizeof(aCrlf), iOffset);
	if ( iLineEnd == (size_t)-1 ) {
		if ( iInputBytes > iOffset && iInputBytes - iOffset > pLimits->iMaxChunkLineBytes ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_CHUNK_LINE_TOO_LARGE, iOffset, 0u);
		}
		return XCODEC_STATUS_NEED_MORE;
	}
	iLineLen = iLineEnd - iOffset;
	if ( iLineLen > pLimits->iMaxChunkLineBytes ) {
		return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_CHUNK_LINE_TOO_LARGE, iOffset, 0u);
	}
	sLine = (char*)XNET_ALLOC(iLineLen + 1u);
	if ( !sLine ) { return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_OUT_OF_MEMORY, iOffset, 0u); }
	if ( __xcodecChainPeekAt(pInput, iOffset, sLine, iLineLen) != iLineLen ) {
		XNET_FREE(sLine);
		return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_CHUNK_SIZE, iOffset, 0u);
	}
	sLine[iLineLen] = '\0';
	if ( !__xcodecHttpParseHexU64(sLine, iLineLen, pChunkSize) ) {
		XNET_FREE(sLine);
		return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_CHUNK_SIZE, iOffset, 0u);
	}
	XNET_FREE(sLine);
	if ( pDataOffset ) { *pDataOffset = iLineEnd + 2u; }
	return XCODEC_STATUS_FRAME;
}


static bool __xcodecHttpForbiddenTrailer(const char* sName)
{
	return __xcodecHttpStrEqNoCase(sName, "Content-Length") ||
		__xcodecHttpStrEqNoCase(sName, "Transfer-Encoding") ||
		__xcodecHttpStrEqNoCase(sName, "Host") ||
		__xcodecHttpStrEqNoCase(sName, "Connection") ||
		__xcodecHttpStrEqNoCase(sName, "Upgrade") ||
		__xcodecHttpStrEqNoCase(sName, "Trailer");
}


static xcodecstatus __xcodecHttpParseTrailerStorage(char* sStorage, size_t iStorageBytes,
	xcodechttp1msg* pMsg, const xcodechttp1limits* pLimits, size_t iBaseOffset,
	xcodechttp1errorinfo* pError)
{
	size_t iCursor = 0u;
	uint32 iLine = 1u;
	if ( !sStorage || !pMsg || !pLimits ) {
		return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_ARGUMENT, iBaseOffset, 0u);
	}
	while ( iCursor + 1u < iStorageBytes ) {
		size_t iNext;
		char* sLine;
		char* sColon;
		char* sValue;
		size_t iNameLen;
		size_t iValueLen;
		xcodechttp1header* pHeader;
		if ( sStorage[iCursor] == '\r' && sStorage[iCursor + 1u] == '\n' ) { return XCODEC_STATUS_FRAME; }
		iNext = __xcodecHttpFindCrlf(sStorage, iCursor, iStorageBytes);
		if ( iNext == (size_t)-1 || iNext - iCursor > pLimits->iMaxHeaderLineBytes ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_TRAILER, iBaseOffset + iCursor, iLine);
		}
		sLine = sStorage + iCursor;
		if ( sLine[0] == ' ' || sLine[0] == '\t' ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_TRAILER, iBaseOffset + iCursor, iLine);
		}
		sColon = (char*)memchr(sLine, ':', iNext - iCursor);
		if ( !sColon ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_TRAILER, iBaseOffset + iCursor, iLine);
		}
		iNameLen = (size_t)(sColon - sLine);
		sValue = sColon + 1;
		iValueLen = iNext - (size_t)(sValue - sStorage);
		__xcodecHttpTrimView(&sValue, &iValueLen);
		if ( !__xcodecHttpValidToken(sLine, iNameLen) || !__xcodecHttpValidFieldValue(sValue, iValueLen) ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_TRAILER, iBaseOffset + iCursor, iLine);
		}
		*sColon = '\0';
		sValue[iValueLen] = '\0';
		sStorage[iNext] = '\0';
		if ( __xcodecHttpForbiddenTrailer(sLine) ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_FORBIDDEN_TRAILER, iBaseOffset + iCursor, iLine);
		}
		if ( pMsg->iTrailerCount >= pLimits->iMaxTrailerCount ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_TOO_MANY_TRAILERS, iBaseOffset + iCursor, iLine);
		}
		if ( !__xcodecHttpEnsureDescriptorCap(&pMsg->pTrailers, pMsg->arrTrailers, &pMsg->iTrailerCap,
			pMsg->iTrailerCount, pMsg->iTrailerCount + 1u) ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_OUT_OF_MEMORY, iBaseOffset + iCursor, iLine);
		}
		pHeader = &pMsg->pTrailers[pMsg->iTrailerCount++];
		pHeader->sName = sLine;
		pHeader->sValue = sValue;
		pHeader->iNameLen = iNameLen;
		pHeader->iValueLen = iValueLen;
		pMsg->iFlags |= XCODEC_HTTP1_F_HAS_TRAILERS;
		iCursor = iNext + 2u;
		iLine++;
	}
	return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_TRAILER, iBaseOffset + iCursor, iLine);
}


XXAPI xcodecstatus xrtCodecHttp1ParseTrailersEx(const xnetchain* pInput, size_t* pConsumedBytes,
	xcodechttp1msg* pMsg, const xcodechttp1limits* pInputLimits, xcodechttp1errorinfo* pError)
{
	static const uint8 aCrlf[] = { '\r', '\n' };
	static const uint8 aTrailerEnd[] = { '\r', '\n', '\r', '\n' };
	xcodechttp1limits tLimits;
	size_t iEnd;
	size_t iBytes;
	char* sStorage;
	if ( pConsumedBytes ) { *pConsumedBytes = 0u; }
	__xcodecHttpClearError(pError);
	if ( !pInput || !pMsg || !__xcodecHttpResolveLimits(pInputLimits, &tLimits) ) {
		return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_ARGUMENT, 0u, 0u);
	}
	if ( xrtNetChainBytes(pInput) < 2u ) { return XCODEC_STATUS_NEED_MORE; }
	if ( __xcodecChainMatchAt(pInput, 0u, aCrlf, sizeof(aCrlf)) ) {
		if ( pConsumedBytes ) { *pConsumedBytes = 2u; }
		return XCODEC_STATUS_FRAME;
	}
	iEnd = __xcodecChainFindPattern(pInput, aTrailerEnd, sizeof(aTrailerEnd), 0u);
	if ( iEnd == (size_t)-1 ) {
		if ( xrtNetChainBytes(pInput) > tLimits.iMaxTrailerBytes ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_TRAILER_TOO_LARGE, 0u, 0u);
		}
		return XCODEC_STATUS_NEED_MORE;
	}
	iBytes = iEnd + sizeof(aTrailerEnd);
	if ( iBytes > tLimits.iMaxTrailerBytes ) {
		return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_TRAILER_TOO_LARGE, 0u, 0u);
	}
	sStorage = (char*)XNET_ALLOC(iBytes + 1u);
	if ( !sStorage ) { return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_OUT_OF_MEMORY, 0u, 0u); }
	if ( __xcodecChainPeekAt(pInput, 0u, sStorage, iBytes) != iBytes ) {
		XNET_FREE(sStorage);
		return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_TRAILER, 0u, 0u);
	}
	sStorage[iBytes] = '\0';
	if ( pMsg->pTrailerStorage ) { XNET_FREE(pMsg->pTrailerStorage); pMsg->pTrailerStorage = NULL; }
	pMsg->pTrailerStorage = sStorage;
	if ( __xcodecHttpParseTrailerStorage(sStorage, iBytes, pMsg, &tLimits, 0u, pError) != XCODEC_STATUS_FRAME ) {
		return XCODEC_STATUS_ERROR;
	}
	if ( pConsumedBytes ) { *pConsumedBytes = iBytes; }
	return XCODEC_STATUS_FRAME;
}


static xcodecstatus __xcodecHttpMeasureChunkedBody(const xnetchain* pInput, size_t iBodyOffset,
	const xcodechttp1limits* pLimits, size_t* pChunkBodyBytes, size_t* pDecodedBytes,
	xcodechttp1msg* pMsg, xcodechttp1errorinfo* pError)
{
	static const uint8 aCrlf[] = { '\r', '\n' };
	size_t iPos = iBodyOffset;
	uint64 iDecoded = 0u;
	if ( !pInput || !pLimits || !pChunkBodyBytes || !pDecodedBytes ) {
		return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_ARGUMENT, iBodyOffset, 0u);
	}
	for ( ;; ) {
		uint64 iChunkSize = 0u;
		size_t iDataOffset = 0u;
		size_t iChunkBytes;
		xcodecstatus eStatus = __xcodecHttpReadChunkHeaderEx(pInput, iPos, pLimits, &iChunkSize,
			&iDataOffset, pMsg, pError);
		if ( eStatus != XCODEC_STATUS_FRAME ) { return eStatus; }
		if ( iChunkSize == 0u ) {
			size_t iTrailerBytes = 0u;
			if ( __xcodecChainMatchAt(pInput, iDataOffset, aCrlf, sizeof(aCrlf)) ) {
				*pChunkBodyBytes = iDataOffset + 2u - iBodyOffset;
				*pDecodedBytes = (size_t)iDecoded;
				return XCODEC_STATUS_FRAME;
			}
			{
				static const uint8 aTrailerEnd[] = { '\r', '\n', '\r', '\n' };
				size_t iEnd = __xcodecChainFindPattern(pInput, aTrailerEnd, sizeof(aTrailerEnd), iDataOffset);
				char* sStorage;
				if ( iEnd == (size_t)-1 ) {
					if ( xrtNetChainBytes(pInput) > iDataOffset + pLimits->iMaxTrailerBytes ) {
						return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_TRAILER_TOO_LARGE, iDataOffset, 0u);
					}
					return XCODEC_STATUS_NEED_MORE;
				}
				iTrailerBytes = iEnd + sizeof(aTrailerEnd) - iDataOffset;
				if ( iTrailerBytes > pLimits->iMaxTrailerBytes ) {
					return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_TRAILER_TOO_LARGE, iDataOffset, 0u);
				}
				sStorage = (char*)XNET_ALLOC(iTrailerBytes + 1u);
				if ( !sStorage ) { return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_OUT_OF_MEMORY, iDataOffset, 0u); }
				if ( __xcodecChainPeekAt(pInput, iDataOffset, sStorage, iTrailerBytes) != iTrailerBytes ) {
					XNET_FREE(sStorage);
					return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_TRAILER, iDataOffset, 0u);
				}
				sStorage[iTrailerBytes] = '\0';
				pMsg->pTrailerStorage = sStorage;
				if ( __xcodecHttpParseTrailerStorage(sStorage, iTrailerBytes, pMsg, pLimits, iDataOffset, pError) != XCODEC_STATUS_FRAME ) {
					return XCODEC_STATUS_ERROR;
				}
				*pChunkBodyBytes = iDataOffset + iTrailerBytes - iBodyOffset;
				*pDecodedBytes = (size_t)iDecoded;
				return XCODEC_STATUS_FRAME;
			}
		}
		if ( iDecoded > pLimits->iMaxBodyBytes || iChunkSize > pLimits->iMaxBodyBytes - iDecoded ||
			iDecoded > (uint64)SIZE_MAX || iChunkSize > (uint64)SIZE_MAX - iDecoded ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_BODY_TOO_LARGE, iPos, 0u);
		}
		if ( iChunkSize > (uint64)(SIZE_MAX - iDataOffset - 2u) ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_BODY_TOO_LARGE, iPos, 0u);
		}
		iChunkBytes = (size_t)iChunkSize;
		if ( xrtNetChainBytes(pInput) < iDataOffset + iChunkBytes + 2u ) { return XCODEC_STATUS_NEED_MORE; }
		if ( !__xcodecChainMatchAt(pInput, iDataOffset + iChunkBytes, aCrlf, sizeof(aCrlf)) ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_CHUNK_TERMINATOR,
				iDataOffset + iChunkBytes, 0u);
		}
		iDecoded += iChunkSize;
		iPos = iDataOffset + iChunkBytes + 2u;
	}
}


static xcodecstatus __xcodecHttp1ParseInternal(const xnetchain* pInput, xcodecframe* pFrame,
	xcodechttp1msg* pMsg, const xcodechttp1limits* pInputLimits,
	xcodechttp1errorinfo* pError, bool bHeaderOnly)
{
	static const uint8 aHeadEnd[] = { '\r', '\n', '\r', '\n' };
	xcodechttp1limits tLimits;
	size_t iHeadEnd;
	size_t iHeadBytes;
	size_t iBodyBytes = 0u;
	char* sStorage;
	if ( !pInput || !pFrame || !pMsg ) { return XCODEC_STATUS_ERROR; }
	xrtCodecFrameInit(pFrame);
	xrtCodecHttp1MessageInit(pMsg);
	__xcodecHttpClearError(pError);
	if ( !__xcodecHttpResolveLimits(pInputLimits, &tLimits) ) {
		return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_ARGUMENT, 0u, 0u);
	}
	iHeadEnd = __xcodecChainFindPattern(pInput, aHeadEnd, sizeof(aHeadEnd), 0u);
	if ( iHeadEnd == (size_t)-1 ) {
		if ( xrtNetChainBytes(pInput) > tLimits.iMaxHeaderBytes ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_HEADER_TOO_LARGE, tLimits.iMaxHeaderBytes, 0u);
		}
		return XCODEC_STATUS_NEED_MORE;
	}
	iHeadBytes = iHeadEnd + sizeof(aHeadEnd);
	if ( iHeadBytes > tLimits.iMaxHeaderBytes ) {
		return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_HEADER_TOO_LARGE, iHeadEnd, 0u);
	}
	sStorage = (char*)XNET_ALLOC(iHeadBytes + 1u);
	if ( !sStorage ) { return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_OUT_OF_MEMORY, 0u, 0u); }
	if ( __xcodecChainPeekAt(pInput, 0u, sStorage, iHeadBytes) != iHeadBytes ) {
		XNET_FREE(sStorage);
		return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_INVALID_START_LINE, 0u, 0u);
	}
	sStorage[iHeadBytes] = '\0';
	pMsg->pHeaderStorage = sStorage;
	if ( __xcodecHttpParseHeadStorage(sStorage, iHeadBytes, pFrame, pMsg, &tLimits, pError) != XCODEC_STATUS_FRAME ) {
		return XCODEC_STATUS_ERROR;
	}
	if ( bHeaderOnly ) {
		pFrame->iFrameBytes = iHeadBytes;
		return XCODEC_STATUS_FRAME;
	}
	if ( (pMsg->iFlags & XCODEC_HTTP1_F_CHUNKED) != 0u ) {
		size_t iEncodedBytes = 0u;
		size_t iDecodedBytes = 0u;
		xcodecstatus eStatus = __xcodecHttpMeasureChunkedBody(pInput, iHeadBytes, &tLimits,
			&iEncodedBytes, &iDecodedBytes, pMsg, pError);
		if ( eStatus != XCODEC_STATUS_FRAME ) {
			if ( eStatus == XCODEC_STATUS_NEED_MORE ) { xrtCodecHttp1MessageUnit(pMsg); }
			return eStatus;
		}
		pFrame->iPayloadBytes = 0u;
		pFrame->iFrameBytes = iHeadBytes + iEncodedBytes;
		pFrame->iMeta0 = (uint64)iDecodedBytes;
		pFrame->iMeta1 = (uint64)iEncodedBytes;
		return XCODEC_STATUS_FRAME;
	}
	if ( pMsg->iContentLength > 0 ) {
		if ( (uint64)pMsg->iContentLength > tLimits.iMaxBodyBytes || (uint64)pMsg->iContentLength > (uint64)SIZE_MAX ) {
			return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_BODY_TOO_LARGE, iHeadBytes, 0u);
		}
		iBodyBytes = (size_t)pMsg->iContentLength;
	}
	if ( iBodyBytes > SIZE_MAX - iHeadBytes ) {
		return __xcodecHttpSetError(pMsg, pError, XCODEC_HTTP1_ERROR_BODY_TOO_LARGE, iHeadBytes, 0u);
	}
	pFrame->iPayloadBytes = iBodyBytes;
	pFrame->iFrameBytes = iHeadBytes + iBodyBytes;
	if ( xrtNetChainBytes(pInput) < pFrame->iFrameBytes ) {
		xrtCodecHttp1MessageUnit(pMsg);
		return XCODEC_STATUS_NEED_MORE;
	}
	return XCODEC_STATUS_FRAME;
}


XXAPI xcodecstatus xrtCodecHttp1ParseHeadEx(const xnetchain* pInput, xcodecframe* pFrame,
	xcodechttp1msg* pMsg, const xcodechttp1limits* pLimits, xcodechttp1errorinfo* pError)
{
	return __xcodecHttp1ParseInternal(pInput, pFrame, pMsg, pLimits, pError, true);
}


XXAPI xcodecstatus xrtCodecHttp1ParseEx(const xnetchain* pInput, xcodecframe* pFrame,
	xcodechttp1msg* pMsg, const xcodechttp1limits* pLimits, xcodechttp1errorinfo* pError)
{
	return __xcodecHttp1ParseInternal(pInput, pFrame, pMsg, pLimits, pError, false);
}


XXAPI xcodecstatus xrtCodecHttp1ParseHead(const xnetchain* pInput, xcodecframe* pFrame, xcodechttp1msg* pMsg)
{
	return xrtCodecHttp1ParseHeadEx(pInput, pFrame, pMsg, NULL, NULL);
}


XXAPI xcodecstatus xrtCodecHttp1Parse(const xnetchain* pInput, xcodecframe* pFrame, xcodechttp1msg* pMsg)
{
	return xrtCodecHttp1ParseEx(pInput, pFrame, pMsg, NULL, NULL);
}


XXAPI size_t xrtCodecHttp1BodyBytes(const xcodecframe* pFrame)
{
	if ( !pFrame ) { return 0u; }
	return (pFrame->iFlags & XCODEC_FRAME_F_CHUNKED) != 0u ? (size_t)pFrame->iMeta0 : pFrame->iPayloadBytes;
}


XXAPI size_t xrtCodecHttp1CopyBody(const xnetchain* pInput, const xcodecframe* pFrame, ptr pOut, size_t iLen)
{
	xcodechttp1limits tLimits;
	uint8* pDst = (uint8*)pOut;
	size_t iWant;
	size_t iCopied = 0u;
	size_t iPos;
	if ( !pInput || !pFrame || !pOut || iLen == 0u ) { return 0u; }
	if ( (pFrame->iFlags & XCODEC_FRAME_F_CHUNKED) == 0u ) {
		return xrtCodecFramePeek(pInput, pFrame, pOut, iLen);
	}
	xrtCodecHttp1LimitsInit(&tLimits);
	iWant = xrtCodecHttp1BodyBytes(pFrame);
	if ( iWant > iLen ) { iWant = iLen; }
	iPos = pFrame->iPayloadOffset;
	while ( iCopied < iWant ) {
		uint64 iChunkSize = 0u;
		size_t iDataOffset = 0u;
		size_t iChunkBytes;
		if ( __xcodecHttpReadChunkHeaderEx(pInput, iPos, &tLimits, &iChunkSize, &iDataOffset, NULL, NULL) != XCODEC_STATUS_FRAME ) { break; }
		if ( iChunkSize == 0u || iChunkSize > (uint64)SIZE_MAX ) { break; }
		iChunkBytes = (size_t)iChunkSize;
		if ( iChunkBytes > iWant - iCopied ) { iChunkBytes = iWant - iCopied; }
		if ( __xcodecChainPeekAt(pInput, iDataOffset, pDst + iCopied, iChunkBytes) != iChunkBytes ) { break; }
		iCopied += iChunkBytes;
		if ( iChunkSize > (uint64)(SIZE_MAX - iDataOffset - 2u) ) { break; }
		iPos = iDataOffset + (size_t)iChunkSize + 2u;
	}
	return iCopied;
}

#endif /* XRT_XCODEC_HTTP1_H */
