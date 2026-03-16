#ifndef XRT_XHTTP_UTIL_H
#define XRT_XHTTP_UTIL_H

#ifndef XXAPI
	#define XXAPI
#endif

#ifndef XHTTP_UTIL_REALLOC
	#define XHTTP_UTIL_REALLOC realloc
#endif

#ifndef XHTTP_UTIL_FREE
	#define XHTTP_UTIL_FREE free
#endif

/*
    XRT public HTTP utility layer.

    This header intentionally exposes only hot-path-safe helpers:
      - length-bounded header line splitting
      - case-insensitive header token matching
      - semicolon-parameter parsing for HTTP header values
      - cookie / Set-Cookie parsing without heap allocation
      - bounded percent-encode and query/form/cookie append helpers
*/

typedef struct {
	xrtstrview tName;
	xrtstrview tValue;
} xrtheaderpair;

typedef struct {
	xrtstrview tName;
	xrtstrview tValue;
} xrtcookiepair;

#define XRT_HTTP_PARAM_F_NONE       0x00000000u
#define XRT_HTTP_PARAM_F_HAS_VALUE  0x00000001u

typedef struct {
	uint32 iFlags;
	xrtstrview tName;
	xrtstrview tValue;
} xrthttpparam;

#define XRT_HTTP_MEDIA_TYPE_F_NONE        0x00000000u
#define XRT_HTTP_MEDIA_TYPE_F_HAS_SUFFIX  0x00000001u
#define XRT_HTTP_MEDIA_TYPE_F_HAS_PARAMS  0x00000002u

typedef struct {
	uint32 iFlags;
	xrtstrview tType;
	xrtstrview tSubType;
	xrtstrview tSuffix;
	xrtstrview tParams;
} xrtmediatypeview;

#define XRT_HTTP_CONTENT_DISPOSITION_F_NONE              0x00000000u
#define XRT_HTTP_CONTENT_DISPOSITION_F_HAS_PARAMS        0x00000001u
#define XRT_HTTP_CONTENT_DISPOSITION_F_HAS_NAME          0x00000002u
#define XRT_HTTP_CONTENT_DISPOSITION_F_HAS_FILENAME      0x00000004u
#define XRT_HTTP_CONTENT_DISPOSITION_F_HAS_FILENAME_EXT  0x00000008u

typedef struct {
	uint32 iFlags;
	xrtstrview tType;
	xrtstrview tParams;
	xrtstrview tName;
	xrtstrview tFileName;
	xrtstrview tFileNameExt;
	xrtstrview tFileNameCharset;
	xrtstrview tFileNameLanguage;
} xrtcontentdispositionview;

#define XRT_SET_COOKIE_F_NONE            0x00000000u
#define XRT_SET_COOKIE_F_HAS_VALUE       0x00000001u
#define XRT_SET_COOKIE_F_HAS_DOMAIN      0x00000002u
#define XRT_SET_COOKIE_F_HAS_PATH        0x00000004u
#define XRT_SET_COOKIE_F_HAS_EXPIRES     0x00000008u
#define XRT_SET_COOKIE_F_HAS_MAX_AGE     0x00000010u
#define XRT_SET_COOKIE_F_HAS_SAME_SITE   0x00000020u
#define XRT_SET_COOKIE_F_SECURE          0x00000040u
#define XRT_SET_COOKIE_F_HTTP_ONLY       0x00000080u
#define XRT_SET_COOKIE_F_PARTITIONED     0x00000100u
#define XRT_SET_COOKIE_F_SAME_PARTY      0x00000200u
#define XRT_SET_COOKIE_F_HAS_PRIORITY    0x00000400u

#define XRT_SAME_SITE_UNSPECIFIED        0u
#define XRT_SAME_SITE_LAX                1u
#define XRT_SAME_SITE_STRICT             2u
#define XRT_SAME_SITE_NONE               3u

#define XRT_COOKIE_PRIORITY_UNSPECIFIED  0u
#define XRT_COOKIE_PRIORITY_LOW          1u
#define XRT_COOKIE_PRIORITY_MEDIUM       2u
#define XRT_COOKIE_PRIORITY_HIGH         3u

typedef struct {
	uint32 iFlags;
	int32_t iMaxAge;
	uint8 iSameSite;
	uint8 iPriority;
	xrtstrview tName;
	xrtstrview tValue;
	xrtstrview tDomain;
	xrtstrview tPath;
	xrtstrview tExpires;
} xrtsetcookieview;

#define XRT_MULTIPART_F_NONE                   0x00000000u
#define XRT_MULTIPART_F_HAS_CONTENT_DISP       0x00000001u
#define XRT_MULTIPART_F_HAS_NAME               0x00000002u
#define XRT_MULTIPART_F_HAS_FILENAME           0x00000004u
#define XRT_MULTIPART_F_HAS_CONTENT_TYPE       0x00000008u
#define XRT_MULTIPART_F_HAS_TRANSFER_ENCODING  0x00000010u
#define XRT_MULTIPART_F_HAS_FILENAME_EXT       0x00000020u

typedef struct {
	uint32 iFlags;
	xrtstrview tHeaders;
	xrtstrview tBody;
	xrtstrview tContentDisposition;
	xrtstrview tName;
	xrtstrview tFileName;
	xrtstrview tFileNameExt;
	xrtstrview tFileNameCharset;
	xrtstrview tFileNameLanguage;
	xrtstrview tContentType;
	xrtstrview tTransferEncoding;
} xrtmultipartpartview;

typedef struct {
	size_t iMaxBufferedBytes;
	size_t iMaxHeaderBytes;
	size_t iMaxPartHeaders;
	size_t iTailReserve;
} xrtmultipartstreamconfig;

typedef struct {
	size_t iMaxNameBytes;
	size_t iMaxValueBytes;
	size_t iMaxPairs;
	size_t iMaxHeaderLineBytes;
	size_t iMaxHeaderBytes;
	size_t iMaxHeaderCount;
	size_t iMaxTokenBytes;
	size_t iMaxBoundaryBytes;
	size_t iMaxMultipartHeaders;
	size_t iMaxMultipartParts;
	size_t iMaxMultipartBytes;
} xrthttputillimits;

typedef enum {
	XRT_MULTIPART_STREAM_RESULT_ERROR     = -1,
	XRT_MULTIPART_STREAM_RESULT_NEED_MORE = 0,
	XRT_MULTIPART_STREAM_RESULT_PART_BEGIN = 1,
	XRT_MULTIPART_STREAM_RESULT_DATA      = 2,
	XRT_MULTIPART_STREAM_RESULT_PART_END  = 3,
	XRT_MULTIPART_STREAM_RESULT_END       = 4
} xrtmultipartstreamresult;

#define XRT_MULTIPART_STREAM_ERR_NONE             0u
#define XRT_MULTIPART_STREAM_ERR_INVALID_BOUNDARY 1u
#define XRT_MULTIPART_STREAM_ERR_BUFFER_LIMIT     2u
#define XRT_MULTIPART_STREAM_ERR_HEADER_LIMIT     3u
#define XRT_MULTIPART_STREAM_ERR_INVALID_HEADER   4u
#define XRT_MULTIPART_STREAM_ERR_TRUNCATED        5u

typedef struct {
	xrtmultipartstreamresult iResult;
	xrtmultipartpartview tPart;
	xrtstrview tData;
} xrtmultipartstreamevent;

typedef struct {
	char* pBuffer;
	size_t iBufferLen;
	size_t iBufferCap;
	size_t iCursor;
	size_t iBoundaryPos;
	size_t iAfterBoundary;
	size_t iBoundaryLen;
	size_t iMaxBufferedBytes;
	size_t iMaxHeaderBytes;
	size_t iMaxPartHeaders;
	size_t iTailReserve;
	uint32 iError;
	uint32 iState;
	bool bFinalBoundary;
	bool bFinishedInput;
	xrtmultipartpartview tCurrentPart;
	char aBoundary[71];
} xrtmultipartstream;

#define XRT_MULTIPART_STREAM_STATE_SEEK_BOUNDARY  0u
#define XRT_MULTIPART_STREAM_STATE_HEADERS        1u
#define XRT_MULTIPART_STREAM_STATE_BODY           2u
#define XRT_MULTIPART_STREAM_STATE_PART_END       3u
#define XRT_MULTIPART_STREAM_STATE_DONE           4u
#define XRT_MULTIPART_STREAM_STATE_ERROR          5u

XXAPI bool xrtQueryNextN(const char* sText, size_t iLen, size_t* pOffset, xrtquerypair* pOut);
XXAPI bool xrtHttpTokenNextN(const char* sText, size_t iLen, size_t* pOffset, xrtstrview* pOut);
XXAPI bool xrtHttpHeaderNextLineN(const char* sBlock, size_t iLen, size_t* pOffset, xrtheaderpair* pOut);
XXAPI bool xrtCookieNextN(const char* sText, size_t iLen, size_t* pOffset, xrtcookiepair* pOut);
XXAPI bool xrtSetCookieParseN(const char* sText, size_t iLen, xrtsetcookieview* pOut);
XXAPI bool xrtHttpParamNextN(const char* sText, size_t iLen, size_t* pOffset, xrthttpparam* pOut);
XXAPI bool xrtMultipartNextN(const char* sBody, size_t iLen, const char* sBoundary, size_t iBoundaryLen, size_t* pOffset, xrtmultipartpartview* pOut);

static char __xrtHttpUtilToLower(char ch)
{
	if ( ch >= 'A' && ch <= 'Z' ) return (char)(ch + 32);
	return ch;
}

static bool __xrtHttpUtilEqNoCaseN(const char* sA, size_t iLenA, const char* sB, size_t iLenB)
{
	size_t i;
	if ( sA == NULL || sB == NULL || iLenA != iLenB ) return false;
	for ( i = 0u; i < iLenA; ++i ) {
		if ( __xrtHttpUtilToLower(sA[i]) != __xrtHttpUtilToLower(sB[i]) ) return false;
	}
	return true;
}

static bool __xrtHttpUtilIsTokenChar(char ch)
{
	if ( ch >= '0' && ch <= '9' ) return true;
	if ( ch >= 'A' && ch <= 'Z' ) return true;
	if ( ch >= 'a' && ch <= 'z' ) return true;
	switch ( ch ) {
		case '!': case '#': case '$': case '%': case '&': case '\'':
		case '*': case '+': case '-': case '.': case '^': case '_':
		case '`': case '|': case '~':
			return true;
		default:
			return false;
	}
}

static bool __xrtHttpUtilIsCookieOctet(char ch)
{
	unsigned char chU = (unsigned char)ch;
	return chU >= 0x21u && chU <= 0x7Eu && ch != '"' && ch != ',' && ch != ';' && ch != '\\';
}

static bool __xrtHttpUtilContainsCtl(const char* sText, size_t iLen)
{
	size_t i;
	if ( sText == NULL ) return true;
	for ( i = 0u; i < iLen; ++i ) {
		unsigned char ch = (unsigned char)sText[i];
		if ( ch < 0x20u || ch == 0x7Fu ) return true;
	}
	return false;
}

static void __xrtHttpUtilTrimView(xrtstrview* pView)
{
	if ( pView == NULL || pView->sPtr == NULL ) return;
	while ( pView->iLen > 0u && (pView->sPtr[0] == ' ' || pView->sPtr[0] == '\t') ) {
		pView->sPtr++;
		pView->iLen--;
	}
	while ( pView->iLen > 0u && (pView->sPtr[pView->iLen - 1u] == ' ' || pView->sPtr[pView->iLen - 1u] == '\t') ) {
		pView->iLen--;
	}
}

static bool __xrtHttpUtilParseInt32(const char* sText, size_t iLen, int32_t* pOut)
{
	bool bNeg = false;
	size_t i = 0u;
	int64_t iValue = 0;
	if ( sText == NULL || pOut == NULL || iLen == 0u ) return false;
	if ( sText[0] == '-' || sText[0] == '+' ) {
		bNeg = sText[0] == '-';
		i = 1u;
		if ( i >= iLen ) return false;
	}
	for ( ; i < iLen; ++i ) {
		char ch = sText[i];
		if ( ch < '0' || ch > '9' ) return false;
		iValue = (iValue * 10) + (int64_t)(ch - '0');
		if ( !bNeg && iValue > 2147483647LL ) return false;
		if ( bNeg && iValue > 2147483648LL ) return false;
	}
	*pOut = bNeg ? (int32_t)(-iValue) : (int32_t)iValue;
	return true;
}

static bool __xrtHttpUtilAppendBytes(char* sOut, size_t iOutCap, size_t* pOffset, const char* sText, size_t iLen)
{
	size_t iCur;
	if ( sOut == NULL || pOffset == NULL || (sText == NULL && iLen != 0u) ) return false;
	iCur = *pOffset;
	if ( iCur + iLen >= iOutCap ) return false;
	if ( iLen > 0u ) memcpy(sOut + iCur, sText, iLen);
	iCur += iLen;
	sOut[iCur] = '\0';
	*pOffset = iCur;
	return true;
}

static bool __xrtHttpUtilValidateBoundaryN(const char* sBoundary, size_t iBoundaryLen)
{
	size_t i;
	if ( sBoundary == NULL || iBoundaryLen == 0u || iBoundaryLen > 70u ) return false;
	for ( i = 0u; i < iBoundaryLen; ++i ) {
		unsigned char ch = (unsigned char)sBoundary[i];
		if ( ch < 0x20u || ch == 0x7Fu ) return false;
	}
	return true;
}

XXAPI void xrtHttpUtilLimitsInit(xrthttputillimits* pLimits)
{
	if ( !pLimits ) return;
	pLimits->iMaxNameBytes = 256u;
	pLimits->iMaxValueBytes = 4096u;
	pLimits->iMaxPairs = 128u;
	pLimits->iMaxHeaderLineBytes = 8192u;
	pLimits->iMaxHeaderBytes = 64u * 1024u;
	pLimits->iMaxHeaderCount = 64u;
	pLimits->iMaxTokenBytes = 256u;
	pLimits->iMaxBoundaryBytes = 70u;
	pLimits->iMaxMultipartHeaders = 64u;
	pLimits->iMaxMultipartParts = 64u;
	pLimits->iMaxMultipartBytes = 256u * 1024u;
}

static const xrthttputillimits* __xrtHttpUtilResolveLimits(const xrthttputillimits* pIn, xrthttputillimits* pLocal)
{
	if ( pIn ) return pIn;
	xrtHttpUtilLimitsInit(pLocal);
	return pLocal;
}

XXAPI void xrtMultipartStreamConfigApplyLimits(xrtmultipartstreamconfig* pConfig, const xrthttputillimits* pLimits)
{
	xrthttputillimits tLocal;
	const xrthttputillimits* pResolved = __xrtHttpUtilResolveLimits(pLimits, &tLocal);
	if ( !pConfig ) return;
	pConfig->iMaxBufferedBytes = pResolved->iMaxMultipartBytes;
	pConfig->iMaxHeaderBytes = pResolved->iMaxHeaderBytes;
	pConfig->iMaxPartHeaders = pResolved->iMaxMultipartHeaders;
	if ( pResolved->iMaxBoundaryBytes > 0u ) {
		pConfig->iTailReserve = pResolved->iMaxBoundaryBytes + 8u;
	}
}

XXAPI bool xrtHttpTokenValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits)
{
	xrthttputillimits tLocal;
	const xrthttputillimits* pResolved = __xrtHttpUtilResolveLimits(pLimits, &tLocal);
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtstrview tToken;
	if ( sText == NULL ) return false;
	while ( xrtHttpTokenNextN(sText, iLen, &iOffset, &tToken) ) {
		if ( tToken.iLen == 0u || tToken.iLen > pResolved->iMaxTokenBytes ) return false;
		if ( ++iCount > pResolved->iMaxPairs ) return false;
	}
	return true;
}

XXAPI bool xrtHttpTokenValidate(const char* sText, const xrthttputillimits* pLimits)
{
	if ( sText == NULL ) return false;
	return xrtHttpTokenValidateN(sText, strlen(sText), pLimits);
}

XXAPI bool xrtHttpParamValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits)
{
	xrthttputillimits tLocal;
	const xrthttputillimits* pResolved = __xrtHttpUtilResolveLimits(pLimits, &tLocal);
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrthttpparam tParam;
	if ( sText == NULL ) return false;
	while ( xrtHttpParamNextN(sText, iLen, &iOffset, &tParam) ) {
		if ( tParam.tName.iLen == 0u || tParam.tName.iLen > pResolved->iMaxNameBytes ) return false;
		if ( (tParam.iFlags & XRT_HTTP_PARAM_F_HAS_VALUE) != 0u && tParam.tValue.iLen > pResolved->iMaxValueBytes ) return false;
		if ( ++iCount > pResolved->iMaxPairs ) return false;
	}
	return true;
}

XXAPI bool xrtHttpParamValidate(const char* sText, const xrthttputillimits* pLimits)
{
	if ( sText == NULL ) return false;
	return xrtHttpParamValidateN(sText, strlen(sText), pLimits);
}

XXAPI bool xrtQueryValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits)
{
	xrthttputillimits tLocal;
	const xrthttputillimits* pResolved = __xrtHttpUtilResolveLimits(pLimits, &tLocal);
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtquerypair tPair;
	if ( sText == NULL ) return false;
	while ( xrtQueryNextN(sText, iLen, &iOffset, &tPair) ) {
		if ( tPair.tKey.iLen == 0u || tPair.tKey.iLen > pResolved->iMaxNameBytes ) return false;
		if ( (tPair.iFlags & XRT_QUERY_F_HAS_VALUE) != 0u && tPair.tValue.iLen > pResolved->iMaxValueBytes ) return false;
		if ( ++iCount > pResolved->iMaxPairs ) return false;
	}
	return true;
}

XXAPI bool xrtQueryValidate(const char* sText, const xrthttputillimits* pLimits)
{
	if ( sText == NULL ) return false;
	return xrtQueryValidateN(sText, strlen(sText), pLimits);
}

XXAPI bool xrtCookieValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits)
{
	xrthttputillimits tLocal;
	const xrthttputillimits* pResolved = __xrtHttpUtilResolveLimits(pLimits, &tLocal);
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtcookiepair tCookie;
	if ( sText == NULL ) return false;
	while ( xrtCookieNextN(sText, iLen, &iOffset, &tCookie) ) {
		if ( tCookie.tName.iLen == 0u || tCookie.tName.iLen > pResolved->iMaxNameBytes ) return false;
		if ( tCookie.tValue.iLen > pResolved->iMaxValueBytes ) return false;
		if ( ++iCount > pResolved->iMaxPairs ) return false;
	}
	return true;
}

XXAPI bool xrtCookieValidate(const char* sText, const xrthttputillimits* pLimits)
{
	if ( sText == NULL ) return false;
	return xrtCookieValidateN(sText, strlen(sText), pLimits);
}

XXAPI bool xrtFormUrlEncodedValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits)
{
	return xrtQueryValidateN(sText, iLen, pLimits);
}

XXAPI bool xrtFormUrlEncodedValidate(const char* sText, const xrthttputillimits* pLimits)
{
	if ( sText == NULL ) return false;
	return xrtFormUrlEncodedValidateN(sText, strlen(sText), pLimits);
}

XXAPI bool xrtHttpHeaderBlockValidateN(const char* sBlock, size_t iLen, const xrthttputillimits* pLimits)
{
	xrthttputillimits tLocal;
	const xrthttputillimits* pResolved = __xrtHttpUtilResolveLimits(pLimits, &tLocal);
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtheaderpair tHeader;
	if ( sBlock == NULL ) return false;
	if ( iLen > pResolved->iMaxHeaderBytes ) return false;
	while ( xrtHttpHeaderNextLineN(sBlock, iLen, &iOffset, &tHeader) ) {
		size_t iLineBytes = tHeader.tName.iLen + 2u + tHeader.tValue.iLen;
		if ( tHeader.tName.iLen == 0u || tHeader.tName.iLen > pResolved->iMaxNameBytes ) return false;
		if ( tHeader.tValue.iLen > pResolved->iMaxValueBytes ) return false;
		if ( iLineBytes > pResolved->iMaxHeaderLineBytes ) return false;
		if ( ++iCount > pResolved->iMaxHeaderCount ) return false;
	}
	return iOffset == iLen;
}

XXAPI bool xrtHttpHeaderBlockValidate(const char* sBlock, const xrthttputillimits* pLimits)
{
	if ( sBlock == NULL ) return false;
	return xrtHttpHeaderBlockValidateN(sBlock, strlen(sBlock), pLimits);
}

XXAPI bool xrtSetCookieValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits)
{
	xrthttputillimits tLocal;
	const xrthttputillimits* pResolved = __xrtHttpUtilResolveLimits(pLimits, &tLocal);
	xrtsetcookieview tCookie;
	if ( !xrtSetCookieParseN(sText, iLen, &tCookie) ) return false;
	if ( tCookie.tName.iLen == 0u || tCookie.tName.iLen > pResolved->iMaxNameBytes ) return false;
	if ( tCookie.tValue.iLen > pResolved->iMaxValueBytes ) return false;
	if ( (tCookie.iFlags & XRT_SET_COOKIE_F_HAS_DOMAIN) != 0u && tCookie.tDomain.iLen > pResolved->iMaxValueBytes ) return false;
	if ( (tCookie.iFlags & XRT_SET_COOKIE_F_HAS_PATH) != 0u && tCookie.tPath.iLen > pResolved->iMaxValueBytes ) return false;
	if ( (tCookie.iFlags & XRT_SET_COOKIE_F_HAS_EXPIRES) != 0u && tCookie.tExpires.iLen > pResolved->iMaxValueBytes ) return false;
	return true;
}

XXAPI bool xrtSetCookieValidate(const char* sText, const xrthttputillimits* pLimits)
{
	if ( sText == NULL ) return false;
	return xrtSetCookieValidateN(sText, strlen(sText), pLimits);
}

XXAPI bool xrtMultipartValidateN(const char* sBody, size_t iLen, const char* sBoundary, size_t iBoundaryLen, const xrthttputillimits* pLimits)
{
	xrthttputillimits tLocal;
	const xrthttputillimits* pResolved = __xrtHttpUtilResolveLimits(pLimits, &tLocal);
	size_t iOffset = 0u;
	size_t iPartCount = 0u;
	xrtmultipartpartview tPart;
	size_t iHeaderOff;
	size_t iHeaderCount;
	xrtheaderpair tHeader;
	if ( sBody == NULL || sBoundary == NULL ) return false;
	if ( iBoundaryLen == 0u || iBoundaryLen > pResolved->iMaxBoundaryBytes ) return false;
	if ( iLen > pResolved->iMaxMultipartBytes ) return false;
	if ( !__xrtHttpUtilValidateBoundaryN(sBoundary, iBoundaryLen) ) return false;
	while ( xrtMultipartNextN(sBody, iLen, sBoundary, iBoundaryLen, &iOffset, &tPart) ) {
		if ( ++iPartCount > pResolved->iMaxMultipartParts ) return false;
		if ( (tPart.iFlags & XRT_MULTIPART_F_HAS_NAME) != 0u && (tPart.tName.iLen == 0u || tPart.tName.iLen > pResolved->iMaxNameBytes) ) return false;
		if ( (tPart.iFlags & XRT_MULTIPART_F_HAS_FILENAME) != 0u && tPart.tFileName.iLen > pResolved->iMaxValueBytes ) return false;
		if ( (tPart.iFlags & XRT_MULTIPART_F_HAS_FILENAME_EXT) != 0u && tPart.tFileNameExt.iLen > pResolved->iMaxValueBytes ) return false;
		if ( (tPart.iFlags & XRT_MULTIPART_F_HAS_CONTENT_TYPE) != 0u && tPart.tContentType.iLen > pResolved->iMaxValueBytes ) return false;
		if ( (tPart.iFlags & XRT_MULTIPART_F_HAS_TRANSFER_ENCODING) != 0u && tPart.tTransferEncoding.iLen > pResolved->iMaxValueBytes ) return false;
		iHeaderOff = 0u;
		iHeaderCount = 0u;
		while ( xrtHttpHeaderNextLineN(tPart.tHeaders.sPtr, tPart.tHeaders.iLen, &iHeaderOff, &tHeader) ) {
			size_t iLineBytes = tHeader.tName.iLen + 2u + tHeader.tValue.iLen;
			if ( tHeader.tName.iLen == 0u || tHeader.tName.iLen > pResolved->iMaxNameBytes ) return false;
			if ( tHeader.tValue.iLen > pResolved->iMaxValueBytes ) return false;
			if ( iLineBytes > pResolved->iMaxHeaderLineBytes ) return false;
			if ( ++iHeaderCount > pResolved->iMaxMultipartHeaders ) return false;
		}
		if ( iHeaderOff != tPart.tHeaders.iLen ) return false;
	}
	return true;
}

XXAPI bool xrtMultipartValidate(const char* sBody, const char* sBoundary, const xrthttputillimits* pLimits)
{
	if ( sBody == NULL || sBoundary == NULL ) return false;
	return xrtMultipartValidateN(sBody, strlen(sBody), sBoundary, strlen(sBoundary), pLimits);
}

static bool __xrtHttpUtilIsAttrChar(char ch)
{
	if ( ch >= '0' && ch <= '9' ) return true;
	if ( ch >= 'A' && ch <= 'Z' ) return true;
	if ( ch >= 'a' && ch <= 'z' ) return true;
	switch ( ch ) {
		case '!': case '#': case '$': case '&': case '+': case '-':
		case '.': case '^': case '_': case '`': case '|': case '~':
			return true;
		default:
			return false;
	}
}

static char __xrtHttpUtilHexDigit(uint8 iValue)
{
	return (char)((iValue < 10u) ? ('0' + iValue) : ('A' + (iValue - 10u)));
}

static bool __xrtHttpUtilAppendQuotedString(char* sOut, size_t iOutCap, size_t* pOffset, const char* sText, size_t iLen)
{
	size_t i;
	if ( sOut == NULL || pOffset == NULL || (sText == NULL && iLen != 0u) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\"", 1u) ) return false;
	for ( i = 0u; i < iLen; ++i ) {
		char ch = sText[i];
		if ( ch == '\r' || ch == '\n' || ch == '\0' ) return false;
		if ( ch == '"' || ch == '\\' ) {
			if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\\", 1u) ) return false;
		}
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, &ch, 1u) ) return false;
	}
	return __xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\"", 1u);
}

XXAPI bool xrtHttpIsTokenN(const char* sText, size_t iLen)
{
	size_t i;
	if ( sText == NULL || iLen == 0u ) return false;
	for ( i = 0u; i < iLen; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(sText[i]) ) return false;
	}
	return true;
}

XXAPI bool xrtHttpIsToken(const char* sText)
{
	if ( sText == NULL ) return false;
	return xrtHttpIsTokenN(sText, strlen(sText));
}

XXAPI bool xrtHttpQuotedStringDecodeToN(const char* sText, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iIn;
	size_t iOut = 0u;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sText == NULL || sOut == NULL || iOutCap == 0u ) return false;
	if ( iLen < 2u || sText[0] != '"' || sText[iLen - 1u] != '"' ) return false;
	sOut[0] = '\0';
	for ( iIn = 1u; iIn + 1u < iLen; ++iIn ) {
		unsigned char ch = (unsigned char)sText[iIn];
		if ( ch == '\r' || ch == '\n' || ch == '\0' ) return false;
		if ( ch == '\\' ) {
			if ( iIn + 2u > iLen - 1u ) return false;
			ch = (unsigned char)sText[++iIn];
			if ( ch == '\r' || ch == '\n' || ch == '\0' ) return false;
		} else if ( ch == '"' ) {
			return false;
		}
		if ( iOut + 1u >= iOutCap ) return false;
		sOut[iOut++] = (char)ch;
	}
	sOut[iOut] = '\0';
	if ( pOutLen ) *pOutLen = iOut;
	return true;
}

XXAPI bool xrtHttpQuotedStringDecodeTo(const char* sText, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( sText == NULL ) return false;
	return xrtHttpQuotedStringDecodeToN(sText, strlen(sText), sOut, iOutCap, pOutLen);
}

XXAPI bool xrtHttpQuotedStringBuildToN(const char* sText, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sOut == NULL || iOutCap == 0u || (sText == NULL && iLen != 0u) ) return false;
	sOut[0] = '\0';
	if ( !__xrtHttpUtilAppendQuotedString(sOut, iOutCap, &iOff, sText, iLen) ) return false;
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}

XXAPI bool xrtHttpQuotedStringBuildTo(const char* sText, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( sText == NULL ) return false;
	return xrtHttpQuotedStringBuildToN(sText, strlen(sText), sOut, iOutCap, pOutLen);
}

XXAPI bool xrtPercentEncodeTo(const char* sText, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen, bool bSpaceAsPlus)
{
	size_t iOut = 0u;
	size_t i;
	if ( sText == NULL || sOut == NULL || iOutCap == 0u ) return false;
	for ( i = 0u; i < iLen; ++i ) {
		unsigned char ch = (unsigned char)sText[i];
		bool bKeep = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
			ch == '-' || ch == '_' || ch == '.' || ch == '~';
		if ( ch == ' ' && bSpaceAsPlus ) {
			if ( iOut + 1u >= iOutCap ) return false;
			sOut[iOut++] = '+';
			continue;
		}
		if ( bKeep ) {
			if ( iOut + 1u >= iOutCap ) return false;
			sOut[iOut++] = (char)ch;
			continue;
		}
		if ( iOut + 3u >= iOutCap ) return false;
		sOut[iOut++] = '%';
		sOut[iOut++] = __xrtHttpUtilHexDigit((uint8)((ch >> 4) & 0x0Fu));
		sOut[iOut++] = __xrtHttpUtilHexDigit((uint8)(ch & 0x0Fu));
	}
	sOut[iOut] = '\0';
	if ( pOutLen ) *pOutLen = iOut;
	return true;
}

static bool __xrtHttpUtilParseExtValueView(xrtstrview tRaw, xrtstrview* pCharset, xrtstrview* pLanguage, xrtstrview* pEncoded)
{
	size_t iFirstTick = (size_t)-1;
	size_t iSecondTick = (size_t)-1;
	size_t i;
	if ( pCharset ) *pCharset = xrtStrView(NULL, 0u);
	if ( pLanguage ) *pLanguage = xrtStrView(NULL, 0u);
	if ( pEncoded ) *pEncoded = xrtStrView(NULL, 0u);
	if ( xrtStrViewIsEmpty(tRaw) ) return false;
	for ( i = 0u; i < tRaw.iLen; ++i ) {
		if ( tRaw.sPtr[i] == '\'' ) {
			if ( iFirstTick == (size_t)-1 ) iFirstTick = i;
			else {
				iSecondTick = i;
				break;
			}
		}
	}
	if ( iFirstTick == (size_t)-1 || iSecondTick == (size_t)-1 || iFirstTick == 0u ) return false;
	for ( i = 0u; i < iFirstTick; ++i ) {
		unsigned char ch = (unsigned char)tRaw.sPtr[i];
		if ( ch <= 0x20u || ch >= 0x7Fu || !__xrtHttpUtilIsTokenChar(tRaw.sPtr[i]) ) return false;
	}
	if ( pCharset ) *pCharset = xrtStrView(tRaw.sPtr, iFirstTick);
	if ( pLanguage ) *pLanguage = xrtStrView(tRaw.sPtr + iFirstTick + 1u, iSecondTick - iFirstTick - 1u);
	if ( pEncoded ) *pEncoded = xrtStrView(tRaw.sPtr + iSecondTick + 1u, tRaw.iLen - iSecondTick - 1u);
	return true;
}

XXAPI bool xrtHttpDecodeExtValueTo(const char* sText, size_t iLen, xrtstrview* pCharset, xrtstrview* pLanguage, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	xrtstrview tCharset;
	xrtstrview tLanguage;
	xrtstrview tEncoded;
	if ( sText == NULL || sOut == NULL || iOutCap == 0u ) return false;
	if ( !__xrtHttpUtilParseExtValueView(xrtStrView(sText, iLen), &tCharset, &tLanguage, &tEncoded) ) return false;
	if ( !xrtPercentDecodeTo(tEncoded.sPtr, tEncoded.iLen, sOut, iOutCap, pOutLen, false) ) return false;
	if ( pCharset ) *pCharset = tCharset;
	if ( pLanguage ) *pLanguage = tLanguage;
	return true;
}

XXAPI bool xrtHttpDecodeExtValue(const char* sText, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( sText == NULL ) return false;
	return xrtHttpDecodeExtValueTo(sText, strlen(sText), NULL, NULL, sOut, iOutCap, pOutLen);
}

XXAPI bool xrtHttpBuildExtValueTo(const char* sCharset, const char* sLanguage, const char* sText, size_t iTextLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	size_t iEncLen = 0u;
	size_t iCharsetLen;
	size_t iLanguageLen;
	size_t i;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sText == NULL || sOut == NULL || iOutCap == 0u ) return false;
	if ( sCharset == NULL || sCharset[0] == '\0' ) sCharset = "UTF-8";
	if ( sLanguage == NULL ) sLanguage = "";
	iCharsetLen = strlen(sCharset);
	iLanguageLen = strlen(sLanguage);
	for ( i = 0u; i < iCharsetLen; ++i ) {
		unsigned char ch = (unsigned char)sCharset[i];
		if ( ch <= 0x20u || ch >= 0x7Fu || !__xrtHttpUtilIsTokenChar(sCharset[i]) ) return false;
	}
	for ( i = 0u; i < iLanguageLen; ++i ) {
		unsigned char ch = (unsigned char)sLanguage[i];
		if ( ch <= 0x20u || ch >= 0x7Fu || sLanguage[i] == '\'' ) return false;
	}
	sOut[0] = '\0';
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, sCharset, iCharsetLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "'", 1u) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, sLanguage, iLanguageLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "'", 1u) ) return false;
	if ( !xrtPercentEncodeTo(sText, iTextLen, sOut + iOff, iOutCap - iOff, &iEncLen, false) ) return false;
	iOff += iEncLen;
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}

XXAPI bool xrtHttpBuildExtValue(const char* sCharset, const char* sLanguage, const char* sText, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( sText == NULL ) return false;
	return xrtHttpBuildExtValueTo(sCharset, sLanguage, sText, strlen(sText), sOut, iOutCap, pOutLen);
}

XXAPI bool xrtHttpHeaderSplitLineN(const char* sLine, size_t iLen, xrtheaderpair* pOut)
{
	size_t iColon = (size_t)-1;
	size_t i;
	if ( sLine == NULL || pOut == NULL || iLen == 0u ) return false;
	for ( i = 0u; i < iLen; ++i ) {
		if ( sLine[i] == ':' ) {
			iColon = i;
			break;
		}
		if ( sLine[i] == '\r' || sLine[i] == '\n' ) return false;
	}
	if ( iColon == (size_t)-1 || iColon == 0u ) return false;
	for ( i = 0u; i < iColon; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(sLine[i]) ) return false;
	}
	pOut->tName = xrtStrView(sLine, iColon);
	pOut->tValue = xrtStrView(sLine + iColon + 1u, iLen - iColon - 1u);
	__xrtHttpUtilTrimView(&pOut->tValue);
	return true;
}

XXAPI bool xrtHttpHeaderSplitLine(const char* sLine, xrtheaderpair* pOut)
{
	if ( sLine == NULL ) return false;
	return xrtHttpHeaderSplitLineN(sLine, strlen(sLine), pOut);
}

XXAPI bool xrtHttpHeaderBuildLineTo(const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	size_t i;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sName == NULL || (sValue == NULL && iValueLen != 0u) || sOut == NULL || iOutCap == 0u ) return false;
	if ( iNameLen == 0u ) return false;
	for ( i = 0u; i < iNameLen; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(sName[i]) ) return false;
	}
	if ( __xrtHttpUtilContainsCtl(sValue, iValueLen) ) return false;
	sOut[0] = '\0';
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, sName, iNameLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, ": ", 2u) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, sValue, iValueLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "\r\n", 2u) ) return false;
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}

XXAPI bool xrtHttpHeaderBuildLine(const char* sName, const char* sValue, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( sName == NULL || sValue == NULL ) return false;
	return xrtHttpHeaderBuildLineTo(sName, strlen(sName), sValue, strlen(sValue), sOut, iOutCap, pOutLen);
}

XXAPI bool xrtHttpHeaderBuildCanonicalLineToN(const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	size_t i;
	bool bUpper = true;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sName == NULL || (sValue == NULL && iValueLen != 0u) || sOut == NULL || iOutCap == 0u ) return false;
	if ( iNameLen == 0u ) return false;
	if ( __xrtHttpUtilContainsCtl(sValue, iValueLen) ) return false;
	sOut[0] = '\0';
	for ( i = 0u; i < iNameLen; ++i ) {
		char ch = sName[i];
		if ( !__xrtHttpUtilIsTokenChar(ch) ) return false;
		if ( ch >= 'A' && ch <= 'Z' ) ch = (char)(ch + 32);
		if ( bUpper && ch >= 'a' && ch <= 'z' ) ch = (char)(ch - 32);
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, &ch, 1u) ) return false;
		bUpper = (ch == '-');
	}
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, ": ", 2u) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, sValue, iValueLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "\r\n", 2u) ) return false;
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}

XXAPI bool xrtHttpHeaderBuildCanonicalLineTo(const char* sName, const char* sValue, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( sName == NULL || sValue == NULL ) return false;
	return xrtHttpHeaderBuildCanonicalLineToN(sName, strlen(sName), sValue, strlen(sValue), sOut, iOutCap, pOutLen);
}

XXAPI bool xrtHttpHeaderBuildBlockTo(const xrtheaderpair* pHeaders, size_t iCount, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	size_t iLineLen = 0u;
	size_t i;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sOut == NULL || iOutCap == 0u || (pHeaders == NULL && iCount != 0u) ) return false;
	sOut[0] = '\0';
	for ( i = 0u; i < iCount; ++i ) {
		if ( !xrtHttpHeaderBuildLineTo(
			pHeaders[i].tName.sPtr,
			pHeaders[i].tName.iLen,
			pHeaders[i].tValue.sPtr,
			pHeaders[i].tValue.iLen,
			sOut + iOff,
			iOutCap - iOff,
			&iLineLen) ) return false;
		iOff += iLineLen;
	}
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "\r\n", 2u) ) return false;
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}

XXAPI bool xrtHttpHeaderBuildCanonicalBlockTo(const xrtheaderpair* pHeaders, size_t iCount, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	size_t iLineLen = 0u;
	size_t i;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sOut == NULL || iOutCap == 0u || (pHeaders == NULL && iCount != 0u) ) return false;
	sOut[0] = '\0';
	for ( i = 0u; i < iCount; ++i ) {
		if ( !xrtHttpHeaderBuildCanonicalLineToN(
			pHeaders[i].tName.sPtr,
			pHeaders[i].tName.iLen,
			pHeaders[i].tValue.sPtr,
			pHeaders[i].tValue.iLen,
			sOut + iOff,
			iOutCap - iOff,
			&iLineLen) ) return false;
		iOff += iLineLen;
	}
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "\r\n", 2u) ) return false;
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}

XXAPI bool xrtHttpTokenNextN(const char* sText, size_t iLen, size_t* pOffset, xrtstrview* pOut)
{
	size_t iCur;
	size_t iEnd;
	size_t i;
	if ( pOut ) memset(pOut, 0, sizeof(xrtstrview));
	if ( sText == NULL || pOffset == NULL || pOut == NULL ) return false;
	iCur = *pOffset;
	while ( iCur < iLen && (sText[iCur] == ',' || sText[iCur] == ' ' || sText[iCur] == '\t') ) iCur++;
	if ( iCur >= iLen ) {
		*pOffset = iCur;
		return false;
	}
	iEnd = iCur;
	while ( iEnd < iLen && sText[iEnd] != ',' ) iEnd++;
	while ( iEnd > iCur && (sText[iEnd - 1u] == ' ' || sText[iEnd - 1u] == '\t') ) iEnd--;
	if ( iEnd <= iCur ) return false;
	for ( i = iCur; i < iEnd; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(sText[i]) ) return false;
	}
	pOut->sPtr = sText + iCur;
	pOut->iLen = iEnd - iCur;
	*pOffset = (iEnd < iLen) ? (iEnd + 1u) : iEnd;
	return true;
}

XXAPI bool xrtHttpTokenNext(const char* sText, size_t* pOffset, xrtstrview* pOut)
{
	if ( sText == NULL ) return false;
	return xrtHttpTokenNextN(sText, strlen(sText), pOffset, pOut);
}

XXAPI size_t xrtHttpTokenCountN(const char* sText, size_t iLen)
{
	size_t iCount = 0u;
	size_t iOffset = 0u;
	xrtstrview tToken;
	while ( xrtHttpTokenNextN(sText, iLen, &iOffset, &tToken) ) ++iCount;
	return iCount;
}

XXAPI size_t xrtHttpTokenCount(const char* sText)
{
	if ( sText == NULL ) return 0u;
	return xrtHttpTokenCountN(sText, strlen(sText));
}

XXAPI bool xrtHttpTokenFindN(const char* sText, size_t iLen, const char* sToken, size_t iTokenLen, xrtstrview* pOut)
{
	size_t iOffset = 0u;
	xrtstrview tToken;
	if ( pOut ) memset(pOut, 0, sizeof(xrtstrview));
	if ( sText == NULL || sToken == NULL || iTokenLen == 0u ) return false;
	while ( xrtHttpTokenNextN(sText, iLen, &iOffset, &tToken) ) {
		if ( tToken.iLen == iTokenLen && __xrtHttpUtilEqNoCaseN(tToken.sPtr, tToken.iLen, sToken, iTokenLen) ) {
			if ( pOut ) *pOut = tToken;
			return true;
		}
	}
	return false;
}

XXAPI bool xrtHttpTokenFind(const char* sText, const char* sToken, xrtstrview* pOut)
{
	if ( sText == NULL || sToken == NULL ) return false;
	return xrtHttpTokenFindN(sText, strlen(sText), sToken, strlen(sToken), pOut);
}

XXAPI bool xrtHttpTokenParseToN(const char* sText, size_t iLen, xrtstrview* pOut, size_t iCap, size_t* pCount)
{
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtstrview tToken;
	if ( pCount ) *pCount = 0u;
	if ( sText == NULL || (pOut == NULL && iCap != 0u) ) return false;
	while ( xrtHttpTokenNextN(sText, iLen, &iOffset, &tToken) ) {
		if ( iCount >= iCap ) return false;
		if ( pOut ) pOut[iCount] = tToken;
		++iCount;
	}
	if ( pCount ) *pCount = iCount;
	return true;
}

XXAPI bool xrtHttpTokenParseTo(const char* sText, xrtstrview* pOut, size_t iCap, size_t* pCount)
{
	if ( sText == NULL ) return false;
	return xrtHttpTokenParseToN(sText, strlen(sText), pOut, iCap, pCount);
}

XXAPI bool xrtHttpTokenAppendTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sToken, size_t iTokenLen)
{
	size_t i;
	if ( sOut == NULL || pOffset == NULL || sToken == NULL || iTokenLen == 0u ) return false;
	for ( i = 0u; i < iTokenLen; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(sToken[i]) ) return false;
	}
	if ( *pOffset > 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, ", ", 2u) ) return false;
	}
	return __xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sToken, iTokenLen);
}

XXAPI bool xrtHttpTokenAppend(char* sOut, size_t iOutCap, size_t* pOffset, const char* sToken)
{
	if ( sToken == NULL ) return false;
	return xrtHttpTokenAppendTo(sOut, iOutCap, pOffset, sToken, strlen(sToken));
}

XXAPI bool xrtHttpHeaderContainsTokenN(const char* sValue, size_t iValueLen, const char* sToken)
{
	if ( sValue == NULL || sToken == NULL || sToken[0] == '\0' ) return false;
	return xrtHttpTokenFindN(sValue, iValueLen, sToken, strlen(sToken), NULL);
}

XXAPI bool xrtHttpHeaderContainsToken(const char* sValue, const char* sToken)
{
	if ( sValue == NULL ) return false;
	return xrtHttpHeaderContainsTokenN(sValue, strlen(sValue), sToken);
}

XXAPI bool xrtHttpHeaderFindN(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, xrtstrview* pOut)
{
	size_t iNameLen;
	size_t i;
	if ( pHeaders == NULL || sName == NULL ) return false;
	iNameLen = strlen(sName);
	for ( i = 0u; i < iCount; ++i ) {
		if ( __xrtHttpUtilEqNoCaseN(pHeaders[i].tName.sPtr, pHeaders[i].tName.iLen, sName, iNameLen) ) {
			if ( pOut ) *pOut = pHeaders[i].tValue;
			return true;
		}
	}
	return false;
}

XXAPI bool xrtHttpHeaderFind(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, xrtstrview* pOut)
{
	return xrtHttpHeaderFindN(pHeaders, iCount, sName, pOut);
}

XXAPI size_t xrtHttpHeaderCountN(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, size_t iNameLen)
{
	size_t i;
	size_t iHits = 0u;
	if ( pHeaders == NULL || sName == NULL || iNameLen == 0u ) return 0u;
	for ( i = 0u; i < iCount; ++i ) {
		if ( __xrtHttpUtilEqNoCaseN(pHeaders[i].tName.sPtr, pHeaders[i].tName.iLen, sName, iNameLen) ) ++iHits;
	}
	return iHits;
}

XXAPI size_t xrtHttpHeaderCount(const xrtheaderpair* pHeaders, size_t iCount, const char* sName)
{
	if ( sName == NULL ) return 0u;
	return xrtHttpHeaderCountN(pHeaders, iCount, sName, strlen(sName));
}

XXAPI bool xrtHttpHeaderFindNthN(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, size_t iNameLen, size_t iNth, xrtstrview* pOut)
{
	size_t i;
	size_t iSeen = 0u;
	if ( pOut ) *pOut = xrtStrView(NULL, 0u);
	if ( pHeaders == NULL || sName == NULL || pOut == NULL || iNameLen == 0u ) return false;
	for ( i = 0u; i < iCount; ++i ) {
		if ( !__xrtHttpUtilEqNoCaseN(pHeaders[i].tName.sPtr, pHeaders[i].tName.iLen, sName, iNameLen) ) continue;
		if ( iSeen == iNth ) {
			*pOut = pHeaders[i].tValue;
			return true;
		}
		++iSeen;
	}
	return false;
}

XXAPI bool xrtHttpHeaderFindNth(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, size_t iNth, xrtstrview* pOut)
{
	if ( sName == NULL ) return false;
	return xrtHttpHeaderFindNthN(pHeaders, iCount, sName, strlen(sName), iNth, pOut);
}

XXAPI size_t xrtHttpHeaderFindAllToN(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, size_t iNameLen, xrtstrview* pOut, size_t iOutCap)
{
	size_t i;
	size_t iHits = 0u;
	if ( pHeaders == NULL || sName == NULL || iNameLen == 0u ) return 0u;
	for ( i = 0u; i < iCount; ++i ) {
		if ( !__xrtHttpUtilEqNoCaseN(pHeaders[i].tName.sPtr, pHeaders[i].tName.iLen, sName, iNameLen) ) continue;
		if ( pOut != NULL && iHits < iOutCap ) pOut[iHits] = pHeaders[i].tValue;
		++iHits;
	}
	return iHits;
}

XXAPI size_t xrtHttpHeaderFindAllTo(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, xrtstrview* pOut, size_t iOutCap)
{
	if ( sName == NULL ) return 0u;
	return xrtHttpHeaderFindAllToN(pHeaders, iCount, sName, strlen(sName), pOut, iOutCap);
}

XXAPI bool xrtHttpHeaderCanonicalizeNameToN(const char* sName, size_t iNameLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t i;
	bool bUpper = true;
	if ( sName == NULL || sOut == NULL || iOutCap == 0u ) return false;
	if ( iNameLen + 1u > iOutCap ) return false;
	for ( i = 0u; i < iNameLen; ++i ) {
		char ch = sName[i];
		if ( !__xrtHttpUtilIsTokenChar(ch) ) return false;
		if ( ch >= 'A' && ch <= 'Z' ) ch = (char)(ch + 32);
		if ( bUpper && ch >= 'a' && ch <= 'z' ) ch = (char)(ch - 32);
		sOut[i] = ch;
		bUpper = (ch == '-');
	}
	sOut[iNameLen] = '\0';
	if ( pOutLen ) *pOutLen = iNameLen;
	return true;
}

XXAPI bool xrtHttpHeaderCanonicalizeNameTo(const char* sName, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( sName == NULL ) return false;
	return xrtHttpHeaderCanonicalizeNameToN(sName, strlen(sName), sOut, iOutCap, pOutLen);
}

XXAPI bool xrtHttpHeaderJoinValuesTo(const xrtstrview* pValues, size_t iCount, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t i;
	size_t iOff = 0u;
	if ( sOut == NULL || iOutCap == 0u || (pValues == NULL && iCount != 0u) ) return false;
	sOut[0] = '\0';
	for ( i = 0u; i < iCount; ++i ) {
		if ( i != 0u ) {
			if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, ", ", 2u) ) return false;
		}
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pValues[i].sPtr, pValues[i].iLen) ) return false;
	}
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}

XXAPI bool xrtHttpHeaderCollectAndJoinToN(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, size_t iNameLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t i;
	size_t iOff = 0u;
	size_t iHits = 0u;
	if ( pOutLen ) *pOutLen = 0u;
	if ( pHeaders == NULL || sName == NULL || sOut == NULL || iOutCap == 0u || iNameLen == 0u ) return false;
	sOut[0] = '\0';
	for ( i = 0u; i < iCount; ++i ) {
		if ( !__xrtHttpUtilEqNoCaseN(pHeaders[i].tName.sPtr, pHeaders[i].tName.iLen, sName, iNameLen) ) continue;
		if ( iHits != 0u ) {
			if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, ", ", 2u) ) return false;
		}
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pHeaders[i].tValue.sPtr, pHeaders[i].tValue.iLen) ) return false;
		++iHits;
	}
	if ( iHits == 0u ) return false;
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}

XXAPI bool xrtHttpHeaderCollectAndJoinTo(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( sName == NULL ) return false;
	return xrtHttpHeaderCollectAndJoinToN(pHeaders, iCount, sName, strlen(sName), sOut, iOutCap, pOutLen);
}

XXAPI bool xrtHttpHeaderNextLineN(const char* sBlock, size_t iLen, size_t* pOffset, xrtheaderpair* pOut)
{
	size_t iCur;
	size_t iEnd;
	if ( sBlock == NULL || pOffset == NULL || pOut == NULL ) return false;
	iCur = *pOffset;
	if ( iCur >= iLen ) return false;
	if ( iCur + 1u < iLen && sBlock[iCur] == '\r' && sBlock[iCur + 1u] == '\n' ) {
		*pOffset = iCur + 2u;
		return false;
	}
	iEnd = iCur;
	while ( iEnd < iLen ) {
		if ( sBlock[iEnd] == '\n' ) {
			if ( iEnd == iCur || sBlock[iEnd - 1u] != '\r' ) return false;
			break;
		}
		iEnd++;
	}
	if ( iEnd < iLen ) {
		if ( !xrtHttpHeaderSplitLineN(sBlock + iCur, iEnd - iCur - 1u, pOut) ) return false;
		*pOffset = iEnd + 1u;
		return true;
	}
	if ( !xrtHttpHeaderSplitLineN(sBlock + iCur, iLen - iCur, pOut) ) return false;
	*pOffset = iLen;
	return true;
}

XXAPI bool xrtHttpHeaderNextLine(const char* sBlock, size_t* pOffset, xrtheaderpair* pOut)
{
	if ( sBlock == NULL ) return false;
	return xrtHttpHeaderNextLineN(sBlock, strlen(sBlock), pOffset, pOut);
}

XXAPI bool xrtHttpHeaderFindLineN(const char* sBlock, size_t iLen, const char* sName, xrtheaderpair* pOut)
{
	size_t iOffset = 0u;
	xrtheaderpair tHeader;
	size_t iNameLen;
	if ( sBlock == NULL || sName == NULL ) return false;
	iNameLen = strlen(sName);
	while ( xrtHttpHeaderNextLineN(sBlock, iLen, &iOffset, &tHeader) ) {
		if ( __xrtHttpUtilEqNoCaseN(tHeader.tName.sPtr, tHeader.tName.iLen, sName, iNameLen) ) {
			if ( pOut ) *pOut = tHeader;
			return true;
		}
	}
	return false;
}

XXAPI bool xrtHttpHeaderFindLine(const char* sBlock, const char* sName, xrtheaderpair* pOut)
{
	if ( sBlock == NULL ) return false;
	return xrtHttpHeaderFindLineN(sBlock, strlen(sBlock), sName, pOut);
}

XXAPI bool xrtHttpHeaderParseBlockToN(const char* sBlock, size_t iLen, xrtheaderpair* pHeaders, size_t iCap, size_t* pCount)
{
	size_t iOffset = 0u;
	size_t iCountOut = 0u;
	xrtheaderpair tHeader;
	if ( pCount ) *pCount = 0u;
	if ( sBlock == NULL || (pHeaders == NULL && iCap != 0u) ) return false;
	while ( xrtHttpHeaderNextLineN(sBlock, iLen, &iOffset, &tHeader) ) {
		if ( iCountOut >= iCap ) return false;
		pHeaders[iCountOut++] = tHeader;
	}
	if ( iOffset < iLen ) {
		if ( !(iOffset + 1u == iLen && sBlock[iOffset] == '\n') &&
		     !(iOffset + 2u == iLen && sBlock[iOffset] == '\r' && sBlock[iOffset + 1u] == '\n') ) {
			if ( iOffset + 1u < iLen || !(sBlock[iOffset] == '\r' || sBlock[iOffset] == '\n') ) return false;
		}
	}
	if ( pCount ) *pCount = iCountOut;
	return true;
}

XXAPI bool xrtHttpHeaderParseBlockTo(const char* sBlock, xrtheaderpair* pHeaders, size_t iCap, size_t* pCount)
{
	if ( sBlock == NULL ) return false;
	return xrtHttpHeaderParseBlockToN(sBlock, strlen(sBlock), pHeaders, iCap, pCount);
}

XXAPI bool xrtHttpHeaderAppendPairN(xrtheaderpair* pHeaders, size_t iCap, size_t* pCount, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen)
{
	size_t i;
	if ( pHeaders == NULL || pCount == NULL || sName == NULL || (sValue == NULL && iValueLen != 0u) ) return false;
	if ( *pCount >= iCap || iNameLen == 0u ) return false;
	for ( i = 0u; i < iNameLen; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(sName[i]) ) return false;
	}
	if ( __xrtHttpUtilContainsCtl(sValue, iValueLen) ) return false;
	pHeaders[*pCount].tName = xrtStrView(sName, iNameLen);
	pHeaders[*pCount].tValue = xrtStrView(sValue, iValueLen);
	(*pCount)++;
	return true;
}

XXAPI bool xrtHttpHeaderAppendPair(xrtheaderpair* pHeaders, size_t iCap, size_t* pCount, const char* sName, const char* sValue)
{
	if ( sName == NULL || sValue == NULL ) return false;
	return xrtHttpHeaderAppendPairN(pHeaders, iCap, pCount, sName, strlen(sName), sValue, strlen(sValue));
}

XXAPI bool xrtHttpHeaderSetPairN(xrtheaderpair* pHeaders, size_t iCap, size_t* pCount, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen)
{
	size_t i;
	if ( pHeaders == NULL || pCount == NULL || sName == NULL || (sValue == NULL && iValueLen != 0u) ) return false;
	for ( i = 0u; i < *pCount; ++i ) {
		if ( pHeaders[i].tName.iLen == iNameLen && __xrtHttpUtilEqNoCaseN(pHeaders[i].tName.sPtr, pHeaders[i].tName.iLen, sName, iNameLen) ) {
			if ( __xrtHttpUtilContainsCtl(sValue, iValueLen) ) return false;
			pHeaders[i].tValue = xrtStrView(sValue, iValueLen);
			return true;
		}
	}
	return xrtHttpHeaderAppendPairN(pHeaders, iCap, pCount, sName, iNameLen, sValue, iValueLen);
}

XXAPI bool xrtHttpHeaderSetPair(xrtheaderpair* pHeaders, size_t iCap, size_t* pCount, const char* sName, const char* sValue)
{
	if ( sName == NULL || sValue == NULL ) return false;
	return xrtHttpHeaderSetPairN(pHeaders, iCap, pCount, sName, strlen(sName), sValue, strlen(sValue));
}

XXAPI size_t xrtHttpHeaderRemoveN(xrtheaderpair* pHeaders, size_t* pCount, const char* sName, size_t iNameLen)
{
	size_t iRead;
	size_t iWrite = 0u;
	size_t iRemoved = 0u;
	if ( pHeaders == NULL || pCount == NULL || sName == NULL || iNameLen == 0u ) return 0u;
	for ( iRead = 0u; iRead < *pCount; ++iRead ) {
		if ( pHeaders[iRead].tName.iLen == iNameLen &&
		     __xrtHttpUtilEqNoCaseN(pHeaders[iRead].tName.sPtr, pHeaders[iRead].tName.iLen, sName, iNameLen) ) {
			iRemoved++;
			continue;
		}
		if ( iWrite != iRead ) pHeaders[iWrite] = pHeaders[iRead];
		iWrite++;
	}
	*pCount = iWrite;
	return iRemoved;
}

XXAPI size_t xrtHttpHeaderRemove(xrtheaderpair* pHeaders, size_t* pCount, const char* sName)
{
	if ( sName == NULL ) return 0u;
	return xrtHttpHeaderRemoveN(pHeaders, pCount, sName, strlen(sName));
}

XXAPI bool xrtCookieNextN(const char* sText, size_t iLen, size_t* pOffset, xrtcookiepair* pOut)
{
	size_t iCur;
	size_t iEnd;
	size_t iEq = (size_t)-1;
	size_t i;
	if ( sText == NULL || pOffset == NULL || pOut == NULL ) return false;
	iCur = *pOffset;
	while ( iCur < iLen && (sText[iCur] == ';' || sText[iCur] == ' ' || sText[iCur] == '\t') ) iCur++;
	if ( iCur >= iLen ) {
		*pOffset = iCur;
		return false;
	}
	iEnd = iCur;
	while ( iEnd < iLen && sText[iEnd] != ';' ) {
		if ( sText[iEnd] == '=' && iEq == (size_t)-1 ) iEq = iEnd;
		iEnd++;
	}
	if ( iEq == (size_t)-1 || iEq == iCur ) return false;
	pOut->tName = xrtStrView(sText + iCur, iEq - iCur);
	pOut->tValue = xrtStrView(sText + iEq + 1u, iEnd - iEq - 1u);
	__xrtHttpUtilTrimView(&pOut->tName);
	__xrtHttpUtilTrimView(&pOut->tValue);
	if ( pOut->tName.iLen == 0u ) return false;
	for ( i = 0u; i < pOut->tName.iLen; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(pOut->tName.sPtr[i]) ) return false;
	}
	*pOffset = (iEnd < iLen) ? (iEnd + 1u) : iEnd;
	return true;
}

XXAPI bool xrtCookieNext(const char* sText, size_t* pOffset, xrtcookiepair* pOut)
{
	if ( sText == NULL ) return false;
	return xrtCookieNextN(sText, strlen(sText), pOffset, pOut);
}

XXAPI bool xrtCookieFindN(const char* sText, size_t iLen, const char* sName, size_t iNameLen, xrtcookiepair* pOut)
{
	size_t iOffset = 0u;
	xrtcookiepair tCookie;
	if ( sText == NULL || sName == NULL || iNameLen == 0u ) return false;
	while ( xrtCookieNextN(sText, iLen, &iOffset, &tCookie) ) {
		if ( tCookie.tName.iLen == iNameLen && memcmp(tCookie.tName.sPtr, sName, iNameLen) == 0 ) {
			if ( pOut ) *pOut = tCookie;
			return true;
		}
	}
	return false;
}

XXAPI bool xrtCookieFind(const char* sText, const char* sName, xrtcookiepair* pOut)
{
	if ( sText == NULL || sName == NULL ) return false;
	return xrtCookieFindN(sText, strlen(sText), sName, strlen(sName), pOut);
}

XXAPI bool xrtCookieParseToN(const char* sText, size_t iLen, xrtcookiepair* pOut, size_t iCap, size_t* pCount)
{
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtcookiepair tCookie;
	if ( pCount ) *pCount = 0u;
	if ( sText == NULL || (pOut == NULL && iCap != 0u) ) return false;
	while ( xrtCookieNextN(sText, iLen, &iOffset, &tCookie) ) {
		if ( iCount >= iCap ) return false;
		if ( pOut ) pOut[iCount] = tCookie;
		++iCount;
	}
	if ( pCount ) *pCount = iCount;
	return true;
}

XXAPI bool xrtCookieParseTo(const char* sText, xrtcookiepair* pOut, size_t iCap, size_t* pCount)
{
	if ( sText == NULL ) return false;
	return xrtCookieParseToN(sText, strlen(sText), pOut, iCap, pCount);
}

XXAPI bool xrtSetCookieParseN(const char* sText, size_t iLen, xrtsetcookieview* pOut)
{
	size_t iCur;
	size_t iEnd;
	size_t iEq = (size_t)-1;
	if ( sText == NULL || pOut == NULL || iLen == 0u ) return false;
	if ( __xrtHttpUtilContainsCtl(sText, iLen) ) return false;
	memset(pOut, 0, sizeof(xrtsetcookieview));
	iCur = 0u;
	while ( iCur < iLen && (sText[iCur] == ' ' || sText[iCur] == '\t' || sText[iCur] == ';') ) iCur++;
	if ( iCur >= iLen ) return false;
	iEnd = iCur;
	while ( iEnd < iLen && sText[iEnd] != ';' ) {
		if ( sText[iEnd] == '=' && iEq == (size_t)-1 ) iEq = iEnd;
		iEnd++;
	}
	if ( iEq == (size_t)-1 || iEq == iCur ) return false;
	pOut->tName = xrtStrView(sText + iCur, iEq - iCur);
	pOut->tValue = xrtStrView(sText + iEq + 1u, iEnd - iEq - 1u);
	__xrtHttpUtilTrimView(&pOut->tName);
	__xrtHttpUtilTrimView(&pOut->tValue);
	if ( pOut->tName.iLen == 0u ) return false;
	for ( iCur = 0u; iCur < pOut->tName.iLen; ++iCur ) {
		if ( !__xrtHttpUtilIsTokenChar(pOut->tName.sPtr[iCur]) ) return false;
	}
	for ( iCur = 0u; iCur < pOut->tValue.iLen; ++iCur ) {
		if ( !__xrtHttpUtilIsCookieOctet(pOut->tValue.sPtr[iCur]) ) return false;
	}
	pOut->iFlags |= XRT_SET_COOKIE_F_HAS_VALUE;
	iCur = (iEnd < iLen) ? (iEnd + 1u) : iEnd;
	while ( iCur < iLen ) {
		xrtstrview tAttrName;
		xrtstrview tAttrValue;
		size_t iAttrEnd = iCur;
		size_t iAttrEq = (size_t)-1;
		while ( iCur < iLen && (sText[iCur] == ' ' || sText[iCur] == '\t' || sText[iCur] == ';') ) iCur++;
		if ( iCur >= iLen ) break;
		iAttrEnd = iCur;
		while ( iAttrEnd < iLen && sText[iAttrEnd] != ';' ) {
			if ( sText[iAttrEnd] == '=' && iAttrEq == (size_t)-1 ) iAttrEq = iAttrEnd;
			iAttrEnd++;
		}
		if ( iAttrEq == (size_t)-1 ) {
			tAttrName = xrtStrView(sText + iCur, iAttrEnd - iCur);
			__xrtHttpUtilTrimView(&tAttrName);
			if ( __xrtHttpUtilEqNoCaseN(tAttrName.sPtr, tAttrName.iLen, "Secure", 6u) ) {
				pOut->iFlags |= XRT_SET_COOKIE_F_SECURE;
			} else if ( __xrtHttpUtilEqNoCaseN(tAttrName.sPtr, tAttrName.iLen, "HttpOnly", 8u) ) {
				pOut->iFlags |= XRT_SET_COOKIE_F_HTTP_ONLY;
			} else if ( __xrtHttpUtilEqNoCaseN(tAttrName.sPtr, tAttrName.iLen, "Partitioned", 11u) ) {
				pOut->iFlags |= XRT_SET_COOKIE_F_PARTITIONED;
			} else if ( __xrtHttpUtilEqNoCaseN(tAttrName.sPtr, tAttrName.iLen, "SameParty", 9u) ) {
				pOut->iFlags |= XRT_SET_COOKIE_F_SAME_PARTY;
			}
		} else {
			int32_t iMaxAge = 0;
			tAttrName = xrtStrView(sText + iCur, iAttrEq - iCur);
			tAttrValue = xrtStrView(sText + iAttrEq + 1u, iAttrEnd - iAttrEq - 1u);
			__xrtHttpUtilTrimView(&tAttrName);
			__xrtHttpUtilTrimView(&tAttrValue);
			if ( __xrtHttpUtilEqNoCaseN(tAttrName.sPtr, tAttrName.iLen, "Domain", 6u) ) {
				pOut->tDomain = tAttrValue;
				pOut->iFlags |= XRT_SET_COOKIE_F_HAS_DOMAIN;
			} else if ( __xrtHttpUtilEqNoCaseN(tAttrName.sPtr, tAttrName.iLen, "Path", 4u) ) {
				pOut->tPath = tAttrValue;
				pOut->iFlags |= XRT_SET_COOKIE_F_HAS_PATH;
			} else if ( __xrtHttpUtilEqNoCaseN(tAttrName.sPtr, tAttrName.iLen, "Expires", 7u) ) {
				pOut->tExpires = tAttrValue;
				pOut->iFlags |= XRT_SET_COOKIE_F_HAS_EXPIRES;
			} else if ( __xrtHttpUtilEqNoCaseN(tAttrName.sPtr, tAttrName.iLen, "Max-Age", 7u) ) {
				if ( !__xrtHttpUtilParseInt32(tAttrValue.sPtr, tAttrValue.iLen, &iMaxAge) ) return false;
				pOut->iMaxAge = iMaxAge;
				pOut->iFlags |= XRT_SET_COOKIE_F_HAS_MAX_AGE;
			} else if ( __xrtHttpUtilEqNoCaseN(tAttrName.sPtr, tAttrName.iLen, "SameSite", 8u) ) {
				if ( __xrtHttpUtilEqNoCaseN(tAttrValue.sPtr, tAttrValue.iLen, "Lax", 3u) ) {
					pOut->iSameSite = XRT_SAME_SITE_LAX;
				} else if ( __xrtHttpUtilEqNoCaseN(tAttrValue.sPtr, tAttrValue.iLen, "Strict", 6u) ) {
					pOut->iSameSite = XRT_SAME_SITE_STRICT;
				} else if ( __xrtHttpUtilEqNoCaseN(tAttrValue.sPtr, tAttrValue.iLen, "None", 4u) ) {
					pOut->iSameSite = XRT_SAME_SITE_NONE;
				} else {
					return false;
				}
				pOut->iFlags |= XRT_SET_COOKIE_F_HAS_SAME_SITE;
			} else if ( __xrtHttpUtilEqNoCaseN(tAttrName.sPtr, tAttrName.iLen, "Priority", 8u) ) {
				if ( __xrtHttpUtilEqNoCaseN(tAttrValue.sPtr, tAttrValue.iLen, "Low", 3u) ) {
					pOut->iPriority = XRT_COOKIE_PRIORITY_LOW;
				} else if ( __xrtHttpUtilEqNoCaseN(tAttrValue.sPtr, tAttrValue.iLen, "Medium", 6u) ) {
					pOut->iPriority = XRT_COOKIE_PRIORITY_MEDIUM;
				} else if ( __xrtHttpUtilEqNoCaseN(tAttrValue.sPtr, tAttrValue.iLen, "High", 4u) ) {
					pOut->iPriority = XRT_COOKIE_PRIORITY_HIGH;
				} else {
					return false;
				}
				pOut->iFlags |= XRT_SET_COOKIE_F_HAS_PRIORITY;
			}
		}
		iCur = (iAttrEnd < iLen) ? (iAttrEnd + 1u) : iAttrEnd;
	}
	return true;
}

XXAPI bool xrtSetCookieParse(const char* sText, xrtsetcookieview* pOut)
{
	if ( sText == NULL ) return false;
	return xrtSetCookieParseN(sText, strlen(sText), pOut);
}

XXAPI bool xrtSetCookieParseLineN(const char* sLine, size_t iLen, xrtsetcookieview* pOut)
{
	xrtheaderpair tHeader;
	if ( !xrtHttpHeaderSplitLineN(sLine, iLen, &tHeader) ) return false;
	if ( !__xrtHttpUtilEqNoCaseN(tHeader.tName.sPtr, tHeader.tName.iLen, "Set-Cookie", 10u) ) return false;
	return xrtSetCookieParseN(tHeader.tValue.sPtr, tHeader.tValue.iLen, pOut);
}

XXAPI bool xrtSetCookieParseLine(const char* sLine, xrtsetcookieview* pOut)
{
	if ( sLine == NULL ) return false;
	return xrtSetCookieParseLineN(sLine, strlen(sLine), pOut);
}

static bool __xrtHttpUtilParseParamValue(xrtstrview tRaw, xrtstrview* pOut);

XXAPI bool xrtHttpParamNextN(const char* sText, size_t iLen, size_t* pOffset, xrthttpparam* pOut)
{
	size_t iCur;
	size_t iEnd;
	size_t iEq = (size_t)-1;
	xrtstrview tName;
	xrtstrview tValue;
	size_t i;
	if ( sText == NULL || pOffset == NULL || pOut == NULL ) return false;
	iCur = *pOffset;
	while ( iCur < iLen && (sText[iCur] == ';' || sText[iCur] == ' ' || sText[iCur] == '\t') ) iCur++;
	if ( iCur >= iLen ) {
		*pOffset = iLen;
		return false;
	}
	iEnd = iCur;
	while ( iEnd < iLen && sText[iEnd] != ';' ) {
		if ( sText[iEnd] == '=' && iEq == (size_t)-1 ) iEq = iEnd;
		iEnd++;
	}
	if ( iEq == (size_t)-1 ) {
		tName = xrtStrView(sText + iCur, iEnd - iCur);
		tValue = xrtStrView(NULL, 0u);
	} else {
		tName = xrtStrView(sText + iCur, iEq - iCur);
		tValue = xrtStrView(sText + iEq + 1u, iEnd - iEq - 1u);
	}
	__xrtHttpUtilTrimView(&tName);
	if ( xrtStrViewIsEmpty(tName) ) return false;
	for ( i = 0u; i < tName.iLen; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(tName.sPtr[i]) ) return false;
	}
	if ( iEq != (size_t)-1 ) {
		if ( !__xrtHttpUtilParseParamValue(tValue, &tValue) ) return false;
	}
	pOut->iFlags = (iEq != (size_t)-1) ? XRT_HTTP_PARAM_F_HAS_VALUE : XRT_HTTP_PARAM_F_NONE;
	pOut->tName = tName;
	pOut->tValue = tValue;
	*pOffset = (iEnd < iLen) ? (iEnd + 1u) : iEnd;
	return true;
}

XXAPI bool xrtHttpParamNext(const char* sText, size_t* pOffset, xrthttpparam* pOut)
{
	if ( sText == NULL ) return false;
	return xrtHttpParamNextN(sText, strlen(sText), pOffset, pOut);
}

XXAPI size_t xrtHttpParamCountN(const char* sText, size_t iLen)
{
	size_t iCount = 0u;
	size_t iOffset = 0u;
	xrthttpparam tParam;
	while ( xrtHttpParamNextN(sText, iLen, &iOffset, &tParam) ) ++iCount;
	return iCount;
}

XXAPI size_t xrtHttpParamCount(const char* sText)
{
	if ( sText == NULL ) return 0u;
	return xrtHttpParamCountN(sText, strlen(sText));
}

XXAPI bool xrtHttpParamFindN(const char* sText, size_t iLen, const char* sName, size_t iNameLen, xrthttpparam* pOut)
{
	size_t iOffset = 0u;
	xrthttpparam tParam;
	if ( pOut ) memset(pOut, 0, sizeof(xrthttpparam));
	if ( sText == NULL || sName == NULL || iNameLen == 0u ) return false;
	while ( xrtHttpParamNextN(sText, iLen, &iOffset, &tParam) ) {
		if ( __xrtHttpUtilEqNoCaseN(tParam.tName.sPtr, tParam.tName.iLen, sName, iNameLen) ) {
			if ( pOut ) *pOut = tParam;
			return true;
		}
	}
	return false;
}

XXAPI bool xrtHttpParamFind(const char* sText, const char* sName, xrthttpparam* pOut)
{
	if ( sText == NULL || sName == NULL ) return false;
	return xrtHttpParamFindN(sText, strlen(sText), sName, strlen(sName), pOut);
}

XXAPI bool xrtHttpParamAppendPairTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen, bool bHasValue, bool bQuoteValue)
{
	size_t i;
	if ( sOut == NULL || pOffset == NULL || sName == NULL || (sValue == NULL && iValueLen != 0u) ) return false;
	if ( iNameLen == 0u ) return false;
	for ( i = 0u; i < iNameLen; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(sName[i]) ) return false;
	}
	if ( *pOffset > 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "; ", 2u) ) return false;
	}
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sName, iNameLen) ) return false;
	if ( !bHasValue ) return true;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "=", 1u) ) return false;
	if ( bQuoteValue ) return __xrtHttpUtilAppendQuotedString(sOut, iOutCap, pOffset, sValue, iValueLen);
	if ( __xrtHttpUtilContainsCtl(sValue, iValueLen) ) return false;
	return __xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sValue, iValueLen);
}

XXAPI bool xrtHttpParamAppendPair(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, const char* sValue, bool bHasValue, bool bQuoteValue)
{
	if ( sName == NULL ) return false;
	return xrtHttpParamAppendPairTo(sOut, iOutCap, pOffset, sName, strlen(sName), sValue, sValue ? strlen(sValue) : 0u, bHasValue, bQuoteValue);
}

XXAPI bool xrtHttpMediaTypeParseN(const char* sText, size_t iLen, xrtmediatypeview* pOut)
{
	size_t iSemi = 0u;
	size_t iSlash = (size_t)-1;
	size_t iPlus = (size_t)-1;
	size_t i;
	xrtstrview tToken;
	xrtstrview tParams;
	if ( pOut == NULL ) return false;
	memset(pOut, 0, sizeof(xrtmediatypeview));
	if ( sText == NULL || iLen == 0u ) return false;
	while ( iSemi < iLen && sText[iSemi] != ';' ) iSemi++;
	tToken = xrtStrView(sText, iSemi);
	__xrtHttpUtilTrimView(&tToken);
	if ( xrtStrViewIsEmpty(tToken) ) return false;
	for ( i = 0u; i < tToken.iLen; ++i ) {
		char ch = tToken.sPtr[i];
		if ( ch == '/' && iSlash == (size_t)-1 ) {
			iSlash = i;
			continue;
		}
		if ( ch == '+' && iPlus == (size_t)-1 ) {
			iPlus = i;
			continue;
		}
		if ( !__xrtHttpUtilIsTokenChar(ch) ) return false;
	}
	if ( iSlash == (size_t)-1 || iSlash == 0u || iSlash + 1u >= tToken.iLen ) return false;
	if ( iPlus != (size_t)-1 && iPlus <= iSlash + 1u ) return false;
	pOut->tType = xrtStrView(tToken.sPtr, iSlash);
	if ( iPlus == (size_t)-1 ) {
		pOut->tSubType = xrtStrView(tToken.sPtr + iSlash + 1u, tToken.iLen - iSlash - 1u);
	} else {
		if ( iPlus + 1u >= tToken.iLen ) return false;
		pOut->tSubType = xrtStrView(tToken.sPtr + iSlash + 1u, iPlus - iSlash - 1u);
		pOut->tSuffix = xrtStrView(tToken.sPtr + iPlus + 1u, tToken.iLen - iPlus - 1u);
		pOut->iFlags |= XRT_HTTP_MEDIA_TYPE_F_HAS_SUFFIX;
	}
	if ( xrtStrViewIsEmpty(pOut->tSubType) ) return false;
	if ( iSemi < iLen ) {
		tParams = xrtStrView(sText + iSemi + 1u, iLen - iSemi - 1u);
		__xrtHttpUtilTrimView(&tParams);
		if ( !xrtStrViewIsEmpty(tParams) ) {
			pOut->tParams = tParams;
			pOut->iFlags |= XRT_HTTP_MEDIA_TYPE_F_HAS_PARAMS;
		}
	}
	return true;
}

XXAPI bool xrtHttpMediaTypeParse(const char* sText, xrtmediatypeview* pOut)
{
	if ( sText == NULL ) return false;
	return xrtHttpMediaTypeParseN(sText, strlen(sText), pOut);
}

XXAPI bool xrtHttpMediaTypeBuildTo(const xrtmediatypeview* pType, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	if ( pOutLen ) *pOutLen = 0u;
	if ( pType == NULL || sOut == NULL || iOutCap == 0u ) return false;
	sOut[0] = '\0';
	if ( xrtStrViewIsEmpty(pType->tType) || xrtStrViewIsEmpty(pType->tSubType) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pType->tType.sPtr, pType->tType.iLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "/", 1u) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pType->tSubType.sPtr, pType->tSubType.iLen) ) return false;
	if ( (pType->iFlags & XRT_HTTP_MEDIA_TYPE_F_HAS_SUFFIX) != 0u ) {
		if ( xrtStrViewIsEmpty(pType->tSuffix) ) return false;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "+", 1u) ) return false;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pType->tSuffix.sPtr, pType->tSuffix.iLen) ) return false;
	}
	if ( (pType->iFlags & XRT_HTTP_MEDIA_TYPE_F_HAS_PARAMS) != 0u ) {
		if ( __xrtHttpUtilContainsCtl(pType->tParams.sPtr, pType->tParams.iLen) ) return false;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; ", 2u) ) return false;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pType->tParams.sPtr, pType->tParams.iLen) ) return false;
	}
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}

XXAPI bool xrtHttpMediaTypeBuild(const xrtmediatypeview* pType, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	return xrtHttpMediaTypeBuildTo(pType, sOut, iOutCap, pOutLen);
}

XXAPI bool xrtHttpMediaTypeFindParamN(const xrtmediatypeview* pType, const char* sName, size_t iNameLen, xrthttpparam* pOut)
{
	if ( pOut ) memset(pOut, 0, sizeof(xrthttpparam));
	if ( pType == NULL || sName == NULL || iNameLen == 0u ) return false;
	if ( (pType->iFlags & XRT_HTTP_MEDIA_TYPE_F_HAS_PARAMS) == 0u || xrtStrViewIsEmpty(pType->tParams) ) return false;
	return xrtHttpParamFindN(pType->tParams.sPtr, pType->tParams.iLen, sName, iNameLen, pOut);
}

XXAPI bool xrtHttpMediaTypeFindParam(const xrtmediatypeview* pType, const char* sName, xrthttpparam* pOut)
{
	if ( sName == NULL ) return false;
	return xrtHttpMediaTypeFindParamN(pType, sName, strlen(sName), pOut);
}

XXAPI bool xrtHttpContentDispositionParseN(const char* sText, size_t iLen, xrtcontentdispositionview* pOut)
{
	size_t iSemi = 0u;
	size_t i;
	xrthttpparam tParam;
	xrtstrview tToken;
	xrtstrview tParams;
	xrtstrview tExtValue;
	xrtstrview tCharset;
	xrtstrview tLanguage;
	xrtstrview tEncoded;
	size_t iParamOff = 0u;
	if ( pOut == NULL ) return false;
	memset(pOut, 0, sizeof(xrtcontentdispositionview));
	if ( sText == NULL || iLen == 0u ) return false;
	while ( iSemi < iLen && sText[iSemi] != ';' ) iSemi++;
	tToken = xrtStrView(sText, iSemi);
	__xrtHttpUtilTrimView(&tToken);
	if ( xrtStrViewIsEmpty(tToken) ) return false;
	for ( i = 0u; i < tToken.iLen; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(tToken.sPtr[i]) ) return false;
	}
	pOut->tType = tToken;
	if ( iSemi < iLen ) {
		tParams = xrtStrView(sText + iSemi + 1u, iLen - iSemi - 1u);
		__xrtHttpUtilTrimView(&tParams);
		if ( !xrtStrViewIsEmpty(tParams) ) {
			pOut->tParams = tParams;
			pOut->iFlags |= XRT_HTTP_CONTENT_DISPOSITION_F_HAS_PARAMS;
			while ( xrtHttpParamNextN(tParams.sPtr, tParams.iLen, &iParamOff, &tParam) ) {
				if ( __xrtHttpUtilEqNoCaseN(tParam.tName.sPtr, tParam.tName.iLen, "name", 4u) &&
				     (tParam.iFlags & XRT_HTTP_PARAM_F_HAS_VALUE) != 0u ) {
					pOut->tName = tParam.tValue;
					pOut->iFlags |= XRT_HTTP_CONTENT_DISPOSITION_F_HAS_NAME;
				} else if ( __xrtHttpUtilEqNoCaseN(tParam.tName.sPtr, tParam.tName.iLen, "filename", 8u) &&
				            (tParam.iFlags & XRT_HTTP_PARAM_F_HAS_VALUE) != 0u ) {
					pOut->tFileName = tParam.tValue;
					pOut->iFlags |= XRT_HTTP_CONTENT_DISPOSITION_F_HAS_FILENAME;
				} else if ( __xrtHttpUtilEqNoCaseN(tParam.tName.sPtr, tParam.tName.iLen, "filename*", 9u) &&
				            (tParam.iFlags & XRT_HTTP_PARAM_F_HAS_VALUE) != 0u ) {
					tExtValue = tParam.tValue;
					if ( !__xrtHttpUtilParseExtValueView(tExtValue, &tCharset, &tLanguage, &tEncoded) ) return false;
					pOut->tFileNameExt = tExtValue;
					pOut->tFileNameCharset = tCharset;
					pOut->tFileNameLanguage = tLanguage;
					pOut->iFlags |= XRT_HTTP_CONTENT_DISPOSITION_F_HAS_FILENAME_EXT;
				}
			}
		}
	}
	return true;
}

XXAPI bool xrtHttpContentDispositionParse(const char* sText, xrtcontentdispositionview* pOut)
{
	if ( sText == NULL ) return false;
	return xrtHttpContentDispositionParseN(sText, strlen(sText), pOut);
}

XXAPI bool xrtHttpContentDispositionDecodeFileNameTo(const xrtcontentdispositionview* pDisp, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iLen;
	if ( pOutLen ) *pOutLen = 0u;
	if ( pDisp == NULL || sOut == NULL || iOutCap == 0u ) return false;
	if ( (pDisp->iFlags & XRT_HTTP_CONTENT_DISPOSITION_F_HAS_FILENAME_EXT) != 0u ) {
		return xrtHttpDecodeExtValueTo(
			pDisp->tFileNameExt.sPtr,
			pDisp->tFileNameExt.iLen,
			NULL,
			NULL,
			sOut,
			iOutCap,
			pOutLen);
	}
	if ( (pDisp->iFlags & XRT_HTTP_CONTENT_DISPOSITION_F_HAS_FILENAME) == 0u ) return false;
	iLen = pDisp->tFileName.iLen;
	if ( iLen + 1u > iOutCap ) return false;
	if ( iLen > 0u ) memcpy(sOut, pDisp->tFileName.sPtr, iLen);
	sOut[iLen] = '\0';
	if ( pOutLen ) *pOutLen = iLen;
	return true;
}

XXAPI bool xrtHttpContentDispositionDecodeFileName(const xrtcontentdispositionview* pDisp, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	return xrtHttpContentDispositionDecodeFileNameTo(pDisp, sOut, iOutCap, pOutLen);
}

XXAPI bool xrtHttpContentDispositionBuildTo(const xrtcontentdispositionview* pDisp, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	if ( pOutLen ) *pOutLen = 0u;
	if ( pDisp == NULL || sOut == NULL || iOutCap == 0u ) return false;
	sOut[0] = '\0';
	if ( xrtStrViewIsEmpty(pDisp->tType) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pDisp->tType.sPtr, pDisp->tType.iLen) ) return false;
	if ( (pDisp->iFlags & XRT_HTTP_CONTENT_DISPOSITION_F_HAS_NAME) != 0u ) {
		if ( !xrtHttpParamAppendPairTo(sOut, iOutCap, &iOff, "name", 4u, pDisp->tName.sPtr, pDisp->tName.iLen, true, true) ) return false;
	}
	if ( (pDisp->iFlags & XRT_HTTP_CONTENT_DISPOSITION_F_HAS_FILENAME) != 0u ) {
		if ( !xrtHttpParamAppendPairTo(sOut, iOutCap, &iOff, "filename", 8u, pDisp->tFileName.sPtr, pDisp->tFileName.iLen, true, true) ) return false;
	}
	if ( (pDisp->iFlags & XRT_HTTP_CONTENT_DISPOSITION_F_HAS_FILENAME_EXT) != 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; filename*=", 12u) ) return false;
		/* tFileNameExt stores the raw RFC 5987 ext-value; preserve it verbatim when rebuilding. */
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pDisp->tFileNameExt.sPtr, pDisp->tFileNameExt.iLen) ) return false;
	}
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}

XXAPI bool xrtHttpContentDispositionBuild(const xrtcontentdispositionview* pDisp, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	return xrtHttpContentDispositionBuildTo(pDisp, sOut, iOutCap, pOutLen);
}

XXAPI bool xrtCookieAppendPairTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen)
{
	size_t i;
	if ( sOut == NULL || pOffset == NULL || sName == NULL ) return false;
	if ( iNameLen == 0u ) return false;
	for ( i = 0u; i < iNameLen; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(sName[i]) ) return false;
	}
	for ( i = 0u; i < iValueLen; ++i ) {
		if ( !__xrtHttpUtilIsCookieOctet(sValue[i]) ) return false;
	}
	if ( *pOffset > 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "; ", 2u) ) return false;
	}
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sName, iNameLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "=", 1u) ) return false;
	return __xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sValue, iValueLen);
}

XXAPI bool xrtCookieAppendPair(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, const char* sValue)
{
	if ( sName == NULL || sValue == NULL ) return false;
	return xrtCookieAppendPairTo(sOut, iOutCap, pOffset, sName, strlen(sName), sValue, strlen(sValue));
}

XXAPI bool xrtCookieBuildTo(const xrtcookiepair* pPairs, size_t iCount, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	size_t i;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sOut == NULL || iOutCap == 0u || (pPairs == NULL && iCount != 0u) ) return false;
	sOut[0] = '\0';
	for ( i = 0u; i < iCount; ++i ) {
		if ( !xrtCookieAppendPairTo(sOut, iOutCap, &iOff, pPairs[i].tName.sPtr, pPairs[i].tName.iLen, pPairs[i].tValue.sPtr, pPairs[i].tValue.iLen) ) return false;
	}
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}

XXAPI bool xrtQueryAppendPairTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sKey, size_t iKeyLen, const char* sValue, size_t iValueLen, bool bHasValue, bool bPlusAsSpace)
{
	size_t iWritten = 0u;
	if ( sOut == NULL || pOffset == NULL || sKey == NULL ) return false;
	if ( *pOffset > 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "&", 1u) ) return false;
	}
	if ( !xrtPercentEncodeTo(sKey, iKeyLen, sOut + *pOffset, iOutCap - *pOffset, &iWritten, bPlusAsSpace) ) return false;
	*pOffset += iWritten;
	if ( bHasValue ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "=", 1u) ) return false;
		if ( !xrtPercentEncodeTo(sValue, iValueLen, sOut + *pOffset, iOutCap - *pOffset, &iWritten, bPlusAsSpace) ) return false;
		*pOffset += iWritten;
	}
	return true;
}

XXAPI bool xrtQueryAppendPair(char* sOut, size_t iOutCap, size_t* pOffset, const char* sKey, const char* sValue)
{
	if ( sKey == NULL ) return false;
	return xrtQueryAppendPairTo(sOut, iOutCap, pOffset, sKey, strlen(sKey), sValue, sValue ? strlen(sValue) : 0u, sValue != NULL, false);
}

XXAPI bool xrtQueryBuildTo(const xrtquerypair* pPairs, size_t iCount, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	size_t i;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sOut == NULL || iOutCap == 0u || (pPairs == NULL && iCount != 0u) ) return false;
	sOut[0] = '\0';
	for ( i = 0u; i < iCount; ++i ) {
		if ( !xrtQueryAppendPairTo(sOut, iOutCap, &iOff,
			pPairs[i].tKey.sPtr,
			pPairs[i].tKey.iLen,
			pPairs[i].tValue.sPtr,
			pPairs[i].tValue.iLen,
			(pPairs[i].iFlags & XRT_QUERY_F_HAS_VALUE) != 0u,
			false) ) return false;
	}
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}

XXAPI bool xrtFormUrlEncodedNextN(const char* sText, size_t iLen, size_t* pOffset, xrtquerypair* pOut)
{
	return xrtQueryNextN(sText, iLen, pOffset, pOut);
}

XXAPI bool xrtFormUrlEncodedNext(const char* sText, size_t* pOffset, xrtquerypair* pOut)
{
	return xrtQueryNext(sText, pOffset, pOut);
}

XXAPI bool xrtFormUrlEncodedParseToN(const char* sText, size_t iLen, xrtquerypair* pOut, size_t iCap, size_t* pCount)
{
	return xrtQueryParseToN(sText, iLen, pOut, iCap, pCount);
}

XXAPI bool xrtFormUrlEncodedParseTo(const char* sText, xrtquerypair* pOut, size_t iCap, size_t* pCount)
{
	if ( sText == NULL ) return false;
	return xrtFormUrlEncodedParseToN(sText, strlen(sText), pOut, iCap, pCount);
}

XXAPI bool xrtFormUrlEncodedDecodeTo(const char* sText, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	return xrtPercentDecodeTo(sText, iLen, sOut, iOutCap, pOutLen, true);
}

XXAPI bool xrtFormUrlEncodedAppendFieldTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen, bool bHasValue)
{
	return xrtQueryAppendPairTo(sOut, iOutCap, pOffset, sName, iNameLen, sValue, iValueLen, bHasValue, true);
}

XXAPI bool xrtFormUrlEncodedAppendField(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, const char* sValue)
{
	if ( sName == NULL ) return false;
	return xrtFormUrlEncodedAppendFieldTo(sOut, iOutCap, pOffset, sName, strlen(sName), sValue, sValue ? strlen(sValue) : 0u, sValue != NULL);
}

XXAPI bool xrtFormUrlEncodedBuildTo(const xrtquerypair* pPairs, size_t iCount, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	size_t i;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sOut == NULL || iOutCap == 0u || (pPairs == NULL && iCount != 0u) ) return false;
	sOut[0] = '\0';
	for ( i = 0u; i < iCount; ++i ) {
		if ( !xrtFormUrlEncodedAppendFieldTo(sOut, iOutCap, &iOff,
			pPairs[i].tKey.sPtr,
			pPairs[i].tKey.iLen,
			pPairs[i].tValue.sPtr,
			pPairs[i].tValue.iLen,
			(pPairs[i].iFlags & XRT_QUERY_F_HAS_VALUE) != 0u) ) return false;
	}
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}

XXAPI bool xrtSetCookieBuildTo(const xrtsetcookieview* pCookie, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sOut == NULL || iOutCap == 0u || pCookie == NULL ) return false;
	sOut[0] = '\0';
	if ( xrtStrViewIsEmpty(pCookie->tName) ) return false;
	if ( !xrtCookieAppendPairTo(sOut, iOutCap, &iOff, pCookie->tName.sPtr, pCookie->tName.iLen, pCookie->tValue.sPtr, pCookie->tValue.iLen) ) return false;
	if ( (pCookie->iFlags & XRT_SET_COOKIE_F_HAS_DOMAIN) != 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; Domain=", 9u) ) return false;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pCookie->tDomain.sPtr, pCookie->tDomain.iLen) ) return false;
	}
	if ( (pCookie->iFlags & XRT_SET_COOKIE_F_HAS_PATH) != 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; Path=", 7u) ) return false;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pCookie->tPath.sPtr, pCookie->tPath.iLen) ) return false;
	}
	if ( (pCookie->iFlags & XRT_SET_COOKIE_F_HAS_EXPIRES) != 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; Expires=", 10u) ) return false;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pCookie->tExpires.sPtr, pCookie->tExpires.iLen) ) return false;
	}
	if ( (pCookie->iFlags & XRT_SET_COOKIE_F_HAS_MAX_AGE) != 0u ) {
		char sNum[32];
		int iLen = snprintf(sNum, sizeof(sNum), "%d", (int)pCookie->iMaxAge);
		if ( iLen <= 0 || (size_t)iLen >= sizeof(sNum) ) return false;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; Max-Age=", 11u) ) return false;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, sNum, (size_t)iLen) ) return false;
	}
	if ( (pCookie->iFlags & XRT_SET_COOKIE_F_HAS_SAME_SITE) != 0u ) {
		const char* sSameSite = NULL;
		if ( pCookie->iSameSite == XRT_SAME_SITE_LAX ) sSameSite = "Lax";
		else if ( pCookie->iSameSite == XRT_SAME_SITE_STRICT ) sSameSite = "Strict";
		else if ( pCookie->iSameSite == XRT_SAME_SITE_NONE ) sSameSite = "None";
		else return false;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; SameSite=", 12u) ) return false;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, sSameSite, strlen(sSameSite)) ) return false;
	}
	if ( (pCookie->iFlags & XRT_SET_COOKIE_F_SECURE) != 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; Secure", 8u) ) return false;
	}
	if ( (pCookie->iFlags & XRT_SET_COOKIE_F_HTTP_ONLY) != 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; HttpOnly", 10u) ) return false;
	}
	if ( (pCookie->iFlags & XRT_SET_COOKIE_F_PARTITIONED) != 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; Partitioned", 13u) ) return false;
	}
	if ( (pCookie->iFlags & XRT_SET_COOKIE_F_SAME_PARTY) != 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; SameParty", 11u) ) return false;
	}
	if ( (pCookie->iFlags & XRT_SET_COOKIE_F_HAS_PRIORITY) != 0u ) {
		const char* sPriority = NULL;
		if ( pCookie->iPriority == XRT_COOKIE_PRIORITY_LOW ) sPriority = "Low";
		else if ( pCookie->iPriority == XRT_COOKIE_PRIORITY_MEDIUM ) sPriority = "Medium";
		else if ( pCookie->iPriority == XRT_COOKIE_PRIORITY_HIGH ) sPriority = "High";
		else return false;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; Priority=", 11u) ) return false;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, sPriority, strlen(sPriority)) ) return false;
	}
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}

XXAPI bool xrtSetCookieBuildLineTo(const xrtsetcookieview* pCookie, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	size_t iLineLen = 0u;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sOut == NULL || iOutCap == 0u || pCookie == NULL ) return false;
	sOut[0] = '\0';
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "Set-Cookie: ", 12u) ) return false;
	if ( !xrtSetCookieBuildTo(pCookie, sOut + iOff, iOutCap - iOff, &iLineLen) ) return false;
	iOff += iLineLen;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "\r\n", 2u) ) return false;
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}

XXAPI bool xrtSetCookieBuildLine(const xrtsetcookieview* pCookie, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	return xrtSetCookieBuildLineTo(pCookie, sOut, iOutCap, pOutLen);
}

static bool __xrtHttpUtilParseParamValue(xrtstrview tRaw, xrtstrview* pOut)
{
	if ( pOut == NULL ) return false;
	*pOut = tRaw;
	__xrtHttpUtilTrimView(pOut);
	if ( pOut->iLen >= 2u && pOut->sPtr[0] == '"' && pOut->sPtr[pOut->iLen - 1u] == '"' ) {
		pOut->sPtr++;
		pOut->iLen -= 2u;
	}
	return !xrtStrViewIsEmpty(*pOut);
}

static bool __xrtHttpUtilFindParamN(xrtstrview tValue, const char* sName, xrtstrview* pOut)
{
	xrthttpparam tParam;
	if ( pOut ) *pOut = xrtStrView(NULL, 0u);
	if ( sName == NULL ) return false;
	if ( !xrtHttpParamFindN(tValue.sPtr, tValue.iLen, sName, strlen(sName), &tParam) ) return false;
	if ( (tParam.iFlags & XRT_HTTP_PARAM_F_HAS_VALUE) == 0u ) return false;
	if ( pOut ) *pOut = tParam.tValue;
	return true;
}

XXAPI bool xrtMultipartBoundaryFromContentTypeN(const char* sValue, size_t iLen, xrtstrview* pOut)
{
	xrtmediatypeview tMediaType;
	xrtstrview tBoundary;
	size_t i;
	if ( sValue == NULL || pOut == NULL || iLen == 0u ) return false;
	if ( !xrtHttpMediaTypeParseN(sValue, iLen, &tMediaType) ) return false;
	if ( !__xrtHttpUtilEqNoCaseN(tMediaType.tType.sPtr, tMediaType.tType.iLen, "multipart", 9u) ) return false;
	if ( !__xrtHttpUtilEqNoCaseN(tMediaType.tSubType.sPtr, tMediaType.tSubType.iLen, "form-data", 9u) &&
	     !__xrtHttpUtilEqNoCaseN(tMediaType.tSubType.sPtr, tMediaType.tSubType.iLen, "mixed", 5u) ) return false;
	if ( (tMediaType.iFlags & XRT_HTTP_MEDIA_TYPE_F_HAS_PARAMS) == 0u ) return false;
	if ( !__xrtHttpUtilFindParamN(tMediaType.tParams, "boundary", &tBoundary) ) return false;
	if ( xrtStrViewIsEmpty(tBoundary) || tBoundary.iLen > 70u ) return false;
	for ( i = 0u; i < tBoundary.iLen; ++i ) {
		unsigned char ch = (unsigned char)tBoundary.sPtr[i];
		if ( ch < 0x20u || ch == 0x7Fu ) return false;
	}
	*pOut = tBoundary;
	return true;
}

XXAPI bool xrtMultipartBoundaryFromContentType(const char* sValue, xrtstrview* pOut)
{
	if ( sValue == NULL ) return false;
	return xrtMultipartBoundaryFromContentTypeN(sValue, strlen(sValue), pOut);
}

XXAPI bool xrtMultipartBuildContentTypeTo(const char* sBoundary, size_t iBoundaryLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sOut == NULL || iOutCap == 0u ) return false;
	if ( !__xrtHttpUtilValidateBoundaryN(sBoundary, iBoundaryLen) ) return false;
	sOut[0] = '\0';
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "multipart/form-data; boundary=", 30u) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, sBoundary, iBoundaryLen) ) return false;
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}

XXAPI bool xrtMultipartBuildContentType(const char* sBoundary, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( sBoundary == NULL ) return false;
	return xrtMultipartBuildContentTypeTo(sBoundary, strlen(sBoundary), sOut, iOutCap, pOutLen);
}

static bool __xrtHttpUtilMatchBoundaryAt(const char* sBody, size_t iLen, size_t iPos, const char* sBoundary, size_t iBoundaryLen, bool* pFinal, size_t* pAfter)
{
	if ( pFinal ) *pFinal = false;
	if ( pAfter ) *pAfter = 0u;
	if ( sBody == NULL || sBoundary == NULL ) return false;
	if ( iPos + 2u + iBoundaryLen > iLen ) return false;
	if ( sBody[iPos] != '-' || sBody[iPos + 1u] != '-' ) return false;
	if ( memcmp(sBody + iPos + 2u, sBoundary, iBoundaryLen) != 0 ) return false;
	iPos += 2u + iBoundaryLen;
	if ( iPos + 1u < iLen && sBody[iPos] == '-' && sBody[iPos + 1u] == '-' ) {
		if ( pFinal ) *pFinal = true;
		if ( pAfter ) *pAfter = iPos + 2u;
		return true;
	}
	if ( iPos + 1u < iLen && sBody[iPos] == '\r' && sBody[iPos + 1u] == '\n' ) {
		if ( pAfter ) *pAfter = iPos + 2u;
		return true;
	}
	return false;
}

static bool __xrtHttpUtilFindBoundaryLine(const char* sBody, size_t iLen, size_t iStart, const char* sBoundary, size_t iBoundaryLen, size_t* pPos, bool* pFinal, size_t* pAfter)
{
	size_t i;
	if ( pPos ) *pPos = 0u;
	if ( sBody == NULL || sBoundary == NULL ) return false;
	for ( i = iStart; i < iLen; ++i ) {
		bool bFinal = false;
		size_t iAfter = 0u;
		bool bLineStart = (i == 0u) || (i >= 2u && sBody[i - 2u] == '\r' && sBody[i - 1u] == '\n');
		if ( !bLineStart ) continue;
		if ( !__xrtHttpUtilMatchBoundaryAt(sBody, iLen, i, sBoundary, iBoundaryLen, &bFinal, &iAfter) ) continue;
		if ( pPos ) *pPos = i;
		if ( pFinal ) *pFinal = bFinal;
		if ( pAfter ) *pAfter = iAfter;
		return true;
	}
	return false;
}

static bool __xrtHttpUtilMultipartParseContentDisposition(xrtstrview tValue, xrtmultipartpartview* pOut)
{
	xrtcontentdispositionview tDisp;
	if ( pOut == NULL ) return false;
	if ( !xrtHttpContentDispositionParseN(tValue.sPtr, tValue.iLen, &tDisp) ) return false;
	if ( !__xrtHttpUtilEqNoCaseN(tDisp.tType.sPtr, tDisp.tType.iLen, "form-data", 9u) ) return false;
	pOut->tContentDisposition = tValue;
	pOut->iFlags |= XRT_MULTIPART_F_HAS_CONTENT_DISP;
	if ( (tDisp.iFlags & XRT_HTTP_CONTENT_DISPOSITION_F_HAS_NAME) != 0u ) {
		pOut->tName = tDisp.tName;
		pOut->iFlags |= XRT_MULTIPART_F_HAS_NAME;
	}
	if ( (tDisp.iFlags & XRT_HTTP_CONTENT_DISPOSITION_F_HAS_FILENAME) != 0u ) {
		pOut->tFileName = tDisp.tFileName;
		pOut->iFlags |= XRT_MULTIPART_F_HAS_FILENAME;
	}
	if ( (tDisp.iFlags & XRT_HTTP_CONTENT_DISPOSITION_F_HAS_FILENAME_EXT) != 0u ) {
		pOut->tFileNameExt = tDisp.tFileNameExt;
		pOut->tFileNameCharset = tDisp.tFileNameCharset;
		pOut->tFileNameLanguage = tDisp.tFileNameLanguage;
		pOut->iFlags |= XRT_MULTIPART_F_HAS_FILENAME_EXT;
	}
	return true;
}

XXAPI bool xrtMultipartNextN(const char* sBody, size_t iLen, const char* sBoundary, size_t iBoundaryLen, size_t* pOffset, xrtmultipartpartview* pOut)
{
	size_t iPos;
	size_t iAfterBoundary;
	bool bFinal = false;
	size_t iHeaderEnd;
	size_t iBodyEnd;
	size_t iNextPos;
	bool bNextFinal = false;
	size_t iUnusedAfter = 0u;
	xrtheaderpair tHeader;
	size_t iHeaderOff = 0u;
	if ( sBody == NULL || sBoundary == NULL || pOffset == NULL || pOut == NULL || iBoundaryLen == 0u ) return false;
	if ( !__xrtHttpUtilFindBoundaryLine(sBody, iLen, *pOffset, sBoundary, iBoundaryLen, &iPos, &bFinal, &iAfterBoundary) ) {
		*pOffset = iLen;
		return false;
	}
	if ( bFinal ) {
		*pOffset = iLen;
		return false;
	}
	if ( iAfterBoundary > iLen ) return false;
	memset(pOut, 0, sizeof(xrtmultipartpartview));
	iHeaderEnd = iAfterBoundary;
	while ( iHeaderEnd + 3u < iLen ) {
		if ( sBody[iHeaderEnd] == '\r' && sBody[iHeaderEnd + 1u] == '\n' && sBody[iHeaderEnd + 2u] == '\r' && sBody[iHeaderEnd + 3u] == '\n' ) break;
		iHeaderEnd++;
	}
	if ( iHeaderEnd + 3u >= iLen ) return false;
	pOut->tHeaders = xrtStrView(sBody + iAfterBoundary, iHeaderEnd - iAfterBoundary);
	iBodyEnd = iHeaderEnd + 4u;
	if ( !__xrtHttpUtilFindBoundaryLine(sBody, iLen, iBodyEnd, sBoundary, iBoundaryLen, &iNextPos, &bNextFinal, &iUnusedAfter) ) return false;
	if ( iNextPos < 2u || sBody[iNextPos - 2u] != '\r' || sBody[iNextPos - 1u] != '\n' ) return false;
	pOut->tBody = xrtStrView(sBody + iBodyEnd, iNextPos - iBodyEnd - 2u);
	while ( xrtHttpHeaderNextLineN(pOut->tHeaders.sPtr, pOut->tHeaders.iLen, &iHeaderOff, &tHeader) ) {
		if ( __xrtHttpUtilEqNoCaseN(tHeader.tName.sPtr, tHeader.tName.iLen, "Content-Disposition", 19u) ) {
			if ( !__xrtHttpUtilMultipartParseContentDisposition(tHeader.tValue, pOut) ) return false;
		} else if ( __xrtHttpUtilEqNoCaseN(tHeader.tName.sPtr, tHeader.tName.iLen, "Content-Type", 12u) ) {
			pOut->tContentType = tHeader.tValue;
			pOut->iFlags |= XRT_MULTIPART_F_HAS_CONTENT_TYPE;
		} else if ( __xrtHttpUtilEqNoCaseN(tHeader.tName.sPtr, tHeader.tName.iLen, "Content-Transfer-Encoding", 25u) ) {
			pOut->tTransferEncoding = tHeader.tValue;
			pOut->iFlags |= XRT_MULTIPART_F_HAS_TRANSFER_ENCODING;
		}
	}
	*pOffset = iNextPos;
	(void)bNextFinal;
	return true;
}

XXAPI bool xrtMultipartNext(const char* sBody, const char* sBoundary, size_t* pOffset, xrtmultipartpartview* pOut)
{
	if ( sBody == NULL || sBoundary == NULL ) return false;
	return xrtMultipartNextN(sBody, strlen(sBody), sBoundary, strlen(sBoundary), pOffset, pOut);
}

XXAPI bool xrtMultipartParseToN(const char* sBody, size_t iLen, const char* sBoundary, size_t iBoundaryLen, xrtmultipartpartview* pOut, size_t iCap, size_t* pCount)
{
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtmultipartpartview tPart;
	if ( pCount ) *pCount = 0u;
	if ( sBody == NULL || sBoundary == NULL || (pOut == NULL && iCap != 0u) ) return false;
	while ( xrtMultipartNextN(sBody, iLen, sBoundary, iBoundaryLen, &iOffset, &tPart) ) {
		if ( iCount >= iCap ) return false;
		if ( pOut ) pOut[iCount] = tPart;
		++iCount;
	}
	if ( pCount ) *pCount = iCount;
	return true;
}

XXAPI bool xrtMultipartParseTo(const char* sBody, const char* sBoundary, xrtmultipartpartview* pOut, size_t iCap, size_t* pCount)
{
	if ( sBody == NULL || sBoundary == NULL ) return false;
	return xrtMultipartParseToN(sBody, strlen(sBody), sBoundary, strlen(sBoundary), pOut, iCap, pCount);
}

XXAPI bool xrtMultipartDecodeFileNameTo(const xrtmultipartpartview* pPart, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iLen;
	if ( pOutLen ) *pOutLen = 0u;
	if ( pPart == NULL || sOut == NULL || iOutCap == 0u ) return false;
	if ( (pPart->iFlags & XRT_MULTIPART_F_HAS_FILENAME_EXT) != 0u ) {
		return xrtHttpDecodeExtValueTo(
			pPart->tFileNameExt.sPtr,
			pPart->tFileNameExt.iLen,
			NULL,
			NULL,
			sOut,
			iOutCap,
			pOutLen);
	}
	if ( (pPart->iFlags & XRT_MULTIPART_F_HAS_FILENAME) == 0u ) return false;
	iLen = pPart->tFileName.iLen;
	if ( iLen + 1u > iOutCap ) return false;
	if ( iLen > 0u ) memcpy(sOut, pPart->tFileName.sPtr, iLen);
	sOut[iLen] = '\0';
	if ( pOutLen ) *pOutLen = iLen;
	return true;
}

XXAPI bool xrtMultipartDecodeFileName(const xrtmultipartpartview* pPart, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	return xrtMultipartDecodeFileNameTo(pPart, sOut, iOutCap, pOutLen);
}

XXAPI void xrtMultipartStreamConfigInit(xrtmultipartstreamconfig* pConfig)
{
	if ( !pConfig ) return;
	pConfig->iMaxBufferedBytes = 256u * 1024u;
	pConfig->iMaxHeaderBytes = 64u * 1024u;
	pConfig->iMaxPartHeaders = 64u;
	pConfig->iTailReserve = 0u;
}

static void __xrtHttpUtilMultipartStreamZeroEvent(xrtmultipartstreamevent* pEvent)
{
	if ( !pEvent ) return;
	memset(pEvent, 0, sizeof(xrtmultipartstreamevent));
	pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_NEED_MORE;
}

static void __xrtHttpUtilMultipartStreamCompact(xrtmultipartstream* pStream)
{
	if ( !pStream || pStream->iCursor == 0u ) return;
	if ( pStream->iCursor >= pStream->iBufferLen ) {
		pStream->iCursor = 0u;
		pStream->iBufferLen = 0u;
		return;
	}
	memmove(pStream->pBuffer, pStream->pBuffer + pStream->iCursor, pStream->iBufferLen - pStream->iCursor);
	pStream->iBufferLen -= pStream->iCursor;
	pStream->iCursor = 0u;
}

static bool __xrtHttpUtilMultipartStreamEnsureCap(xrtmultipartstream* pStream, size_t iNeed)
{
	size_t iNewCap;
	char* pNewBuf;
	if ( !pStream ) return false;
	if ( iNeed <= pStream->iBufferCap ) return true;
	if ( iNeed > pStream->iMaxBufferedBytes ) return false;
	iNewCap = pStream->iBufferCap ? pStream->iBufferCap : 1024u;
	while ( iNewCap < iNeed ) {
		size_t iGrow = iNewCap < 65536u ? iNewCap : 65536u;
		if ( iNewCap > ((size_t)-1) - iGrow ) return false;
		iNewCap += iGrow;
	}
	if ( iNewCap > pStream->iMaxBufferedBytes ) iNewCap = pStream->iMaxBufferedBytes;
	if ( iNewCap < iNeed ) return false;
	pNewBuf = (char*)XHTTP_UTIL_REALLOC(pStream->pBuffer, iNewCap);
	if ( !pNewBuf ) return false;
	pStream->pBuffer = pNewBuf;
	pStream->iBufferCap = iNewCap;
	return true;
}

XXAPI bool xrtMultipartStreamInit(xrtmultipartstream* pStream, const char* sBoundary, size_t iBoundaryLen, const xrtmultipartstreamconfig* pConfig)
{
	xrtmultipartstreamconfig tConfig;
	if ( !pStream ) return false;
	memset(pStream, 0, sizeof(xrtmultipartstream));
	xrtMultipartStreamConfigInit(&tConfig);
	if ( pConfig ) tConfig = *pConfig;
	if ( !__xrtHttpUtilValidateBoundaryN(sBoundary, iBoundaryLen) ) {
		pStream->iError = XRT_MULTIPART_STREAM_ERR_INVALID_BOUNDARY;
		pStream->iState = XRT_MULTIPART_STREAM_STATE_ERROR;
		return false;
	}
	if ( tConfig.iMaxBufferedBytes == 0u ) tConfig.iMaxBufferedBytes = 256u * 1024u;
	if ( tConfig.iMaxHeaderBytes == 0u ) tConfig.iMaxHeaderBytes = 64u * 1024u;
	if ( tConfig.iMaxPartHeaders == 0u ) tConfig.iMaxPartHeaders = 64u;
	if ( tConfig.iTailReserve == 0u ) tConfig.iTailReserve = iBoundaryLen + 8u;
	if ( tConfig.iTailReserve < iBoundaryLen + 8u ) tConfig.iTailReserve = iBoundaryLen + 8u;
	if ( tConfig.iMaxBufferedBytes < tConfig.iMaxHeaderBytes + tConfig.iTailReserve + 4u ) {
		tConfig.iMaxBufferedBytes = tConfig.iMaxHeaderBytes + tConfig.iTailReserve + 4u;
	}
	memcpy(pStream->aBoundary, sBoundary, iBoundaryLen);
	pStream->aBoundary[iBoundaryLen] = '\0';
	pStream->iBoundaryLen = iBoundaryLen;
	pStream->iMaxBufferedBytes = tConfig.iMaxBufferedBytes;
	pStream->iMaxHeaderBytes = tConfig.iMaxHeaderBytes;
	pStream->iMaxPartHeaders = tConfig.iMaxPartHeaders;
	pStream->iTailReserve = tConfig.iTailReserve;
	pStream->iState = XRT_MULTIPART_STREAM_STATE_SEEK_BOUNDARY;
	return true;
}

XXAPI void xrtMultipartStreamUnit(xrtmultipartstream* pStream)
{
	if ( !pStream ) return;
	if ( pStream->pBuffer ) {
		XHTTP_UTIL_FREE(pStream->pBuffer);
		pStream->pBuffer = NULL;
	}
	memset(pStream, 0, sizeof(xrtmultipartstream));
}

XXAPI void xrtMultipartStreamReset(xrtmultipartstream* pStream)
{
	if ( !pStream ) return;
	pStream->iBufferLen = 0u;
	pStream->iCursor = 0u;
	pStream->iBoundaryPos = 0u;
	pStream->iAfterBoundary = 0u;
	pStream->iError = XRT_MULTIPART_STREAM_ERR_NONE;
	pStream->iState = XRT_MULTIPART_STREAM_STATE_SEEK_BOUNDARY;
	pStream->bFinalBoundary = false;
	pStream->bFinishedInput = false;
	memset(&pStream->tCurrentPart, 0, sizeof(xrtmultipartpartview));
}

XXAPI bool xrtMultipartStreamFeed(xrtmultipartstream* pStream, const void* pData, size_t iLen)
{
	if ( !pStream || pStream->iState == XRT_MULTIPART_STREAM_STATE_ERROR || pStream->iState == XRT_MULTIPART_STREAM_STATE_DONE ) return false;
	if ( pStream->bFinishedInput ) return false;
	if ( iLen == 0u ) return true;
	if ( pData == NULL ) return false;
	__xrtHttpUtilMultipartStreamCompact(pStream);
	if ( pStream->iBufferLen > pStream->iMaxBufferedBytes || iLen > pStream->iMaxBufferedBytes - pStream->iBufferLen ) {
		pStream->iError = XRT_MULTIPART_STREAM_ERR_BUFFER_LIMIT;
		pStream->iState = XRT_MULTIPART_STREAM_STATE_ERROR;
		return false;
	}
	if ( !__xrtHttpUtilMultipartStreamEnsureCap(pStream, pStream->iBufferLen + iLen + 1u) ) {
		pStream->iError = XRT_MULTIPART_STREAM_ERR_BUFFER_LIMIT;
		pStream->iState = XRT_MULTIPART_STREAM_STATE_ERROR;
		return false;
	}
	memcpy(pStream->pBuffer + pStream->iBufferLen, pData, iLen);
	pStream->iBufferLen += iLen;
	pStream->pBuffer[pStream->iBufferLen] = '\0';
	return true;
}

XXAPI void xrtMultipartStreamFinish(xrtmultipartstream* pStream)
{
	if ( !pStream ) return;
	pStream->bFinishedInput = true;
}

XXAPI uint32 xrtMultipartStreamError(const xrtmultipartstream* pStream)
{
	return pStream ? pStream->iError : XRT_MULTIPART_STREAM_ERR_NONE;
}

static bool __xrtHttpUtilMultipartStreamParseHeaders(xrtmultipartstream* pStream, xrtmultipartpartview* pOut)
{
	size_t iHeaderEnd;
	size_t iHeaderOff = 0u;
	size_t iHeaderCount = 0u;
	xrtheaderpair tHeader;
	if ( !pStream || !pOut ) return false;
	iHeaderEnd = pStream->iCursor;
	while ( iHeaderEnd + 3u < pStream->iBufferLen ) {
		if ( pStream->pBuffer[iHeaderEnd] == '\r' &&
		     pStream->pBuffer[iHeaderEnd + 1u] == '\n' &&
		     pStream->pBuffer[iHeaderEnd + 2u] == '\r' &&
		     pStream->pBuffer[iHeaderEnd + 3u] == '\n' ) {
			break;
		}
		iHeaderEnd++;
		if ( iHeaderEnd - pStream->iCursor > pStream->iMaxHeaderBytes ) {
			pStream->iError = XRT_MULTIPART_STREAM_ERR_HEADER_LIMIT;
			pStream->iState = XRT_MULTIPART_STREAM_STATE_ERROR;
			return false;
		}
	}
	if ( iHeaderEnd + 3u >= pStream->iBufferLen ) {
		if ( pStream->bFinishedInput ) {
			pStream->iError = XRT_MULTIPART_STREAM_ERR_TRUNCATED;
			pStream->iState = XRT_MULTIPART_STREAM_STATE_ERROR;
			return false;
		}
		return false;
	}
	memset(pOut, 0, sizeof(xrtmultipartpartview));
	pOut->tHeaders = xrtStrView(pStream->pBuffer + pStream->iCursor, iHeaderEnd - pStream->iCursor);
	while ( xrtHttpHeaderNextLineN(pOut->tHeaders.sPtr, pOut->tHeaders.iLen, &iHeaderOff, &tHeader) ) {
		++iHeaderCount;
		if ( iHeaderCount > pStream->iMaxPartHeaders ) {
			pStream->iError = XRT_MULTIPART_STREAM_ERR_HEADER_LIMIT;
			pStream->iState = XRT_MULTIPART_STREAM_STATE_ERROR;
			return false;
		}
		if ( __xrtHttpUtilEqNoCaseN(tHeader.tName.sPtr, tHeader.tName.iLen, "Content-Disposition", 19u) ) {
			if ( !__xrtHttpUtilMultipartParseContentDisposition(tHeader.tValue, pOut) ) {
				pStream->iError = XRT_MULTIPART_STREAM_ERR_INVALID_HEADER;
				pStream->iState = XRT_MULTIPART_STREAM_STATE_ERROR;
				return false;
			}
		} else if ( __xrtHttpUtilEqNoCaseN(tHeader.tName.sPtr, tHeader.tName.iLen, "Content-Type", 12u) ) {
			pOut->tContentType = tHeader.tValue;
			pOut->iFlags |= XRT_MULTIPART_F_HAS_CONTENT_TYPE;
		} else if ( __xrtHttpUtilEqNoCaseN(tHeader.tName.sPtr, tHeader.tName.iLen, "Content-Transfer-Encoding", 25u) ) {
			pOut->tTransferEncoding = tHeader.tValue;
			pOut->iFlags |= XRT_MULTIPART_F_HAS_TRANSFER_ENCODING;
		}
	}
	pStream->iCursor = iHeaderEnd + 4u;
	return true;
}

XXAPI xrtmultipartstreamresult xrtMultipartStreamNext(xrtmultipartstream* pStream, xrtmultipartstreamevent* pEvent)
{
	if ( pEvent ) __xrtHttpUtilMultipartStreamZeroEvent(pEvent);
	if ( pStream == NULL || pEvent == NULL ) return XRT_MULTIPART_STREAM_RESULT_ERROR;
	if ( pStream->iState == XRT_MULTIPART_STREAM_STATE_ERROR ) {
		pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_ERROR;
		return pEvent->iResult;
	}
	if ( pStream->iState == XRT_MULTIPART_STREAM_STATE_DONE ) {
		pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_END;
		return pEvent->iResult;
	}
	__xrtHttpUtilMultipartStreamCompact(pStream);
	for (;;) {
		if ( pStream->iState == XRT_MULTIPART_STREAM_STATE_SEEK_BOUNDARY ) {
			size_t iPos = 0u;
			bool bFinal = false;
			size_t iAfter = 0u;
			if ( !__xrtHttpUtilFindBoundaryLine(pStream->pBuffer, pStream->iBufferLen, pStream->iCursor, pStream->aBoundary, pStream->iBoundaryLen, &iPos, &bFinal, &iAfter) ) {
				if ( pStream->bFinishedInput ) {
					pStream->iError = XRT_MULTIPART_STREAM_ERR_TRUNCATED;
					pStream->iState = XRT_MULTIPART_STREAM_STATE_ERROR;
					pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_ERROR;
					return pEvent->iResult;
				}
				return XRT_MULTIPART_STREAM_RESULT_NEED_MORE;
			}
			pStream->iCursor = iAfter;
			if ( bFinal ) {
				pStream->iState = XRT_MULTIPART_STREAM_STATE_DONE;
				pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_END;
				return pEvent->iResult;
			}
			pStream->iState = XRT_MULTIPART_STREAM_STATE_HEADERS;
			continue;
		}
		if ( pStream->iState == XRT_MULTIPART_STREAM_STATE_HEADERS ) {
			xrtmultipartpartview tPart;
			if ( !__xrtHttpUtilMultipartStreamParseHeaders(pStream, &tPart) ) {
				if ( pStream->iState == XRT_MULTIPART_STREAM_STATE_ERROR ) {
					pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_ERROR;
					return pEvent->iResult;
				}
				return XRT_MULTIPART_STREAM_RESULT_NEED_MORE;
			}
			pStream->tCurrentPart = tPart;
			pStream->iState = XRT_MULTIPART_STREAM_STATE_BODY;
			pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_PART_BEGIN;
			pEvent->tPart = tPart;
			return pEvent->iResult;
		}
		if ( pStream->iState == XRT_MULTIPART_STREAM_STATE_BODY ) {
			size_t iPos = 0u;
			bool bFinal = false;
			size_t iAfter = 0u;
			if ( __xrtHttpUtilFindBoundaryLine(pStream->pBuffer, pStream->iBufferLen, pStream->iCursor, pStream->aBoundary, pStream->iBoundaryLen, &iPos, &bFinal, &iAfter) ) {
				size_t iDataLen = (iPos >= pStream->iCursor + 2u) ? (iPos - pStream->iCursor - 2u) : 0u;
				if ( iDataLen > 0u ) {
					pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_DATA;
					pEvent->tPart = pStream->tCurrentPart;
					pEvent->tData = xrtStrView(pStream->pBuffer + pStream->iCursor, iDataLen);
					pStream->iCursor += iDataLen;
					return pEvent->iResult;
				}
				pStream->iBoundaryPos = iPos;
				pStream->iAfterBoundary = iAfter;
				pStream->bFinalBoundary = bFinal;
				pStream->iState = XRT_MULTIPART_STREAM_STATE_PART_END;
				continue;
			}
			if ( pStream->bFinishedInput ) {
				pStream->iError = XRT_MULTIPART_STREAM_ERR_TRUNCATED;
				pStream->iState = XRT_MULTIPART_STREAM_STATE_ERROR;
				pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_ERROR;
				return pEvent->iResult;
			}
			if ( pStream->iBufferLen > pStream->iCursor + pStream->iTailReserve ) {
				size_t iDataLen = pStream->iBufferLen - pStream->iCursor - pStream->iTailReserve;
				if ( iDataLen > 0u ) {
					pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_DATA;
					pEvent->tPart = pStream->tCurrentPart;
					pEvent->tData = xrtStrView(pStream->pBuffer + pStream->iCursor, iDataLen);
					pStream->iCursor += iDataLen;
					return pEvent->iResult;
				}
			}
			return XRT_MULTIPART_STREAM_RESULT_NEED_MORE;
		}
		if ( pStream->iState == XRT_MULTIPART_STREAM_STATE_PART_END ) {
			pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_PART_END;
			pEvent->tPart = pStream->tCurrentPart;
			pStream->iCursor = pStream->iAfterBoundary;
			memset(&pStream->tCurrentPart, 0, sizeof(xrtmultipartpartview));
			pStream->iState = pStream->bFinalBoundary ? XRT_MULTIPART_STREAM_STATE_DONE : XRT_MULTIPART_STREAM_STATE_HEADERS;
			return pEvent->iResult;
		}
		if ( pStream->iState == XRT_MULTIPART_STREAM_STATE_DONE ) {
			pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_END;
			return pEvent->iResult;
		}
		pStream->iError = XRT_MULTIPART_STREAM_ERR_INVALID_HEADER;
		pStream->iState = XRT_MULTIPART_STREAM_STATE_ERROR;
		pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_ERROR;
		return pEvent->iResult;
	}
}

XXAPI bool xrtMultipartAppendFieldPartTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, size_t iBoundaryLen, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen)
{
	if ( sOut == NULL || pOffset == NULL || sName == NULL || (sValue == NULL && iValueLen != 0u) ) return false;
	if ( !__xrtHttpUtilValidateBoundaryN(sBoundary, iBoundaryLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "--", 2u) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sBoundary, iBoundaryLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\r\nContent-Disposition: form-data; name=", 39u) ) return false;
	if ( !__xrtHttpUtilAppendQuotedString(sOut, iOutCap, pOffset, sName, iNameLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\r\n\r\n", 4u) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sValue, iValueLen) ) return false;
	return __xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\r\n", 2u);
}

XXAPI bool xrtMultipartAppendFieldPart(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, const char* sName, const char* sValue)
{
	if ( sBoundary == NULL || sName == NULL || sValue == NULL ) return false;
	return xrtMultipartAppendFieldPartTo(sOut, iOutCap, pOffset, sBoundary, strlen(sBoundary), sName, strlen(sName), sValue, strlen(sValue));
}

XXAPI bool xrtMultipartAppendRawPartTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, size_t iBoundaryLen, const xrtheaderpair* pHeaders, size_t iHeaderCount, const char* pBody, size_t iBodyLen)
{
	size_t i;
	if ( sOut == NULL || pOffset == NULL || (pHeaders == NULL && iHeaderCount != 0u) || (pBody == NULL && iBodyLen != 0u) ) return false;
	if ( !__xrtHttpUtilValidateBoundaryN(sBoundary, iBoundaryLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "--", 2u) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sBoundary, iBoundaryLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\r\n", 2u) ) return false;
	for ( i = 0u; i < iHeaderCount; ++i ) {
		size_t iLineLen = 0u;
		if ( !xrtHttpHeaderBuildLineTo(
			pHeaders[i].tName.sPtr,
			pHeaders[i].tName.iLen,
			pHeaders[i].tValue.sPtr,
			pHeaders[i].tValue.iLen,
			sOut + *pOffset,
			iOutCap - *pOffset,
			&iLineLen) ) return false;
		*pOffset += iLineLen;
	}
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\r\n", 2u) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, pBody, iBodyLen) ) return false;
	return __xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\r\n", 2u);
}

XXAPI bool xrtMultipartAppendRawPart(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, const xrtheaderpair* pHeaders, size_t iHeaderCount, const char* pBody, size_t iBodyLen)
{
	if ( sBoundary == NULL ) return false;
	return xrtMultipartAppendRawPartTo(sOut, iOutCap, pOffset, sBoundary, strlen(sBoundary), pHeaders, iHeaderCount, pBody, iBodyLen);
}

XXAPI bool xrtMultipartAppendFilePartExtTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, size_t iBoundaryLen, const char* sName, size_t iNameLen, const char* sFileName, size_t iFileNameLen, const char* sFileNameExt, size_t iFileNameExtLen, const char* sContentType, size_t iContentTypeLen, const char* pBody, size_t iBodyLen)
{
	if ( sOut == NULL || pOffset == NULL || sName == NULL || sFileName == NULL || (sContentType == NULL && iContentTypeLen != 0u) || (pBody == NULL && iBodyLen != 0u) ) return false;
	if ( !__xrtHttpUtilValidateBoundaryN(sBoundary, iBoundaryLen) ) return false;
	if ( iContentTypeLen != 0u && __xrtHttpUtilContainsCtl(sContentType, iContentTypeLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "--", 2u) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sBoundary, iBoundaryLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\r\nContent-Disposition: form-data; name=", 39u) ) return false;
	if ( !__xrtHttpUtilAppendQuotedString(sOut, iOutCap, pOffset, sName, iNameLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "; filename=", 11u) ) return false;
	if ( !__xrtHttpUtilAppendQuotedString(sOut, iOutCap, pOffset, sFileName, iFileNameLen) ) return false;
	if ( sFileNameExt != NULL && iFileNameExtLen != 0u ) {
		size_t iExtLen = 0u;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "; filename*=", 12u) ) return false;
		if ( !xrtHttpBuildExtValueTo("UTF-8", "", sFileNameExt, iFileNameExtLen, sOut + *pOffset, iOutCap - *pOffset, &iExtLen) ) return false;
		*pOffset += iExtLen;
	}
	if ( iContentTypeLen != 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\r\nContent-Type: ", 16u) ) return false;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sContentType, iContentTypeLen) ) return false;
	}
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\r\n\r\n", 4u) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, pBody, iBodyLen) ) return false;
	return __xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\r\n", 2u);
}

XXAPI bool xrtMultipartAppendFilePartExt(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, const char* sName, const char* sFileName, const char* sFileNameExt, const char* sContentType, const char* pBody, size_t iBodyLen)
{
	if ( sBoundary == NULL || sName == NULL || sFileName == NULL || pBody == NULL ) return false;
	return xrtMultipartAppendFilePartExtTo(
		sOut,
		iOutCap,
		pOffset,
		sBoundary,
		strlen(sBoundary),
		sName,
		strlen(sName),
		sFileName,
		strlen(sFileName),
		sFileNameExt,
		sFileNameExt ? strlen(sFileNameExt) : 0u,
		sContentType,
		sContentType ? strlen(sContentType) : 0u,
		pBody,
		iBodyLen);
}

XXAPI bool xrtMultipartAppendFilePartTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, size_t iBoundaryLen, const char* sName, size_t iNameLen, const char* sFileName, size_t iFileNameLen, const char* sContentType, size_t iContentTypeLen, const char* pBody, size_t iBodyLen)
{
	return xrtMultipartAppendFilePartExtTo(
		sOut,
		iOutCap,
		pOffset,
		sBoundary,
		iBoundaryLen,
		sName,
		iNameLen,
		sFileName,
		iFileNameLen,
		NULL,
		0u,
		sContentType,
		iContentTypeLen,
		pBody,
		iBodyLen);
}

XXAPI bool xrtMultipartAppendFilePart(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, const char* sName, const char* sFileName, const char* sContentType, const char* pBody, size_t iBodyLen)
{
	return xrtMultipartAppendFilePartExt(sOut, iOutCap, pOffset, sBoundary, sName, sFileName, NULL, sContentType, pBody, iBodyLen);
}

XXAPI bool xrtMultipartAppendFinishTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, size_t iBoundaryLen)
{
	if ( sOut == NULL || pOffset == NULL ) return false;
	if ( !__xrtHttpUtilValidateBoundaryN(sBoundary, iBoundaryLen) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "--", 2u) ) return false;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sBoundary, iBoundaryLen) ) return false;
	return __xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "--\r\n", 4u);
}

XXAPI bool xrtMultipartAppendFinish(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary)
{
	if ( sBoundary == NULL ) return false;
	return xrtMultipartAppendFinishTo(sOut, iOutCap, pOffset, sBoundary, strlen(sBoundary));
}

#endif
