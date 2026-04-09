#ifndef XRT_XHTTP_UTIL_H
#define XRT_XHTTP_UTIL_H

#ifndef XHTTP_UTIL_REALLOC
	#define XHTTP_UTIL_REALLOC realloc
#endif

#ifndef XHTTP_UTIL_FREE
	#define XHTTP_UTIL_FREE free
#endif

#define XRT_MULTIPART_STREAM_STATE_SEEK_BOUNDARY 1u
#define XRT_MULTIPART_STREAM_STATE_HEADERS       2u
#define XRT_MULTIPART_STREAM_STATE_BODY          3u
#define XRT_MULTIPART_STREAM_STATE_PART_END      4u
#define XRT_MULTIPART_STREAM_STATE_DONE          5u
#define XRT_MULTIPART_STREAM_STATE_ERROR         6u

/*
	XRT HTTP utility implementation layer.

	Public declarations live in xrt.h.
	This file is intended to be expanded only from xrt.c / single-head generation.
*/

static char __xrtHttpUtilToLower(char ch)
{
	if ( ch >= 'A' && ch <= 'Z' ) { return (char)(ch + 32); }
	return ch;
}


// 内部函数：判断两段文本是否忽略大小写相等
static bool __xrtHttpUtilEqNoCaseN(const char* sA, size_t iLenA, const char* sB, size_t iLenB)
{
	size_t i;
	if ( sA == NULL || sB == NULL || iLenA != iLenB ) { return false; }
	for ( i = 0u; i < iLenA; ++i ) {
		if ( __xrtHttpUtilToLower(sA[i]) != __xrtHttpUtilToLower(sB[i]) ) { return false; }
	}
	return true;
}


// 内部函数：判断是否为 HTTP Token 字符
static bool __xrtHttpUtilIsTokenChar(char ch)
{
	if ( ch >= '0' && ch <= '9' ) { return true; }
	if ( ch >= 'A' && ch <= 'Z' ) { return true; }
	if ( ch >= 'a' && ch <= 'z' ) { return true; }
	switch ( ch ) {
		case '!': case '#': case '$': case '%': case '&': case '\'':
		case '*': case '+': case '-': case '.': case '^': case '_':
		case '`': case '|': case '~':
			return true;
		default:
			return false;
	}
}


// 内部函数：判断是否为 Cookie Octet 字符
static bool __xrtHttpUtilIsCookieOctet(char ch)
{
	unsigned char chU = (unsigned char)ch;
	return chU >= 0x21u && chU <= 0x7Eu && ch != '"' && ch != ',' && ch != ';' && ch != '\\';
}


// 内部函数：判断文本中是否包含控制字符
static bool __xrtHttpUtilContainsCtl(const char* sText, size_t iLen)
{
	size_t i;
	if ( sText == NULL ) { return true; }
	for ( i = 0u; i < iLen; ++i ) {
		unsigned char ch = (unsigned char)sText[i];
		if ( ch < 0x20u || ch == 0x7Fu ) { return true; }
	}
	return false;
}


// 内部函数：裁剪 HTTP 视图两端空白
static void __xrtHttpUtilTrimView(xrtstrview* pView)
{
	if ( pView == NULL || pView->sPtr == NULL ) { return; }
	while ( pView->iLen > 0u && (pView->sPtr[0] == ' ' || pView->sPtr[0] == '\t') ) {
		pView->sPtr++;
		pView->iLen--;
	}
	while ( pView->iLen > 0u && (pView->sPtr[pView->iLen - 1u] == ' ' || pView->sPtr[pView->iLen - 1u] == '\t') ) {
		pView->iLen--;
	}
}


// 内部函数：解析 32 位整数
static bool __xrtHttpUtilParseInt32(const char* sText, size_t iLen, int32_t* pOut)
{
	bool bNeg = false;
	size_t i = 0u;
	int64_t iValue = 0;
	if ( sText == NULL || pOut == NULL || iLen == 0u ) { return false; }
	if ( sText[0] == '-' || sText[0] == '+' ) {
		bNeg = sText[0] == '-';
		i = 1u;
		if ( i >= iLen ) { return false; }
	}
	for ( ; i < iLen; ++i ) {
		char ch = sText[i];
		if ( ch < '0' || ch > '9' ) { return false; }
		iValue = (iValue * 10) + (int64_t)(ch - '0');
		if ( !bNeg && iValue > 2147483647LL ) { return false; }
		if ( bNeg && iValue > 2147483648LL ) { return false; }
	}
	*pOut = bNeg ? (int32_t)(-iValue) : (int32_t)iValue;
	return true;
}


// 内部函数：向输出缓冲区追加字节
static bool __xrtHttpUtilAppendBytes(char* sOut, size_t iOutCap, size_t* pOffset, const char* sText, size_t iLen)
{
	size_t iCur;
	if ( sOut == NULL || pOffset == NULL || (sText == NULL && iLen != 0u) ) { return false; }
	iCur = *pOffset;
	if ( iCur + iLen >= iOutCap ) { return false; }
	if ( iLen > 0u ) { memcpy(sOut + iCur, sText, iLen); }
	iCur += iLen;
	sOut[iCur] = '\0';
	*pOffset = iCur;
	return true;
}


// 内部函数：校验 Multipart 边界
static bool __xrtHttpUtilValidateBoundaryN(const char* sBoundary, size_t iBoundaryLen)
{
	size_t i;
	if ( sBoundary == NULL || iBoundaryLen == 0u || iBoundaryLen > 70u ) { return false; }
	for ( i = 0u; i < iBoundaryLen; ++i ) {
		unsigned char ch = (unsigned char)sBoundary[i];
		if ( ch < 0x20u || ch == 0x7Fu ) { return false; }
	}
	return true;
}


// 初始化 HTTP 工具限制配置
XXAPI void xrtHttpUtilLimitsInit(xrthttputillimits* pLimits)
{
	if ( !pLimits ) { return; }
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


// 内部函数：解析 HTTP 工具限制配置
static const xrthttputillimits* __xrtHttpUtilResolveLimits(const xrthttputillimits* pIn, xrthttputillimits* pLocal)
{
	if ( pIn ) { return pIn; }
	xrtHttpUtilLimitsInit(pLocal);
	return pLocal;
}


// 应用 Multipart 流限制配置
XXAPI void xrtMultipartStreamConfigApplyLimits(xrtmultipartstreamconfig* pConfig, const xrthttputillimits* pLimits)
{
	xrthttputillimits tLocal;
	const xrthttputillimits* pResolved = __xrtHttpUtilResolveLimits(pLimits, &tLocal);
	if ( !pConfig ) { return; }
	pConfig->iMaxBufferedBytes = pResolved->iMaxMultipartBytes;
	pConfig->iMaxHeaderBytes = pResolved->iMaxHeaderBytes;
	pConfig->iMaxPartHeaders = pResolved->iMaxMultipartHeaders;
	if ( pResolved->iMaxBoundaryBytes > 0u ) {
		pConfig->iTailReserve = pResolved->iMaxBoundaryBytes + 8u;
	}
}


// 校验 HTTP Token
XXAPI bool xrtHttpTokenValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits)
{
	xrthttputillimits tLocal;
	const xrthttputillimits* pResolved = __xrtHttpUtilResolveLimits(pLimits, &tLocal);
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtstrview tToken;
	if ( sText == NULL ) { return false; }
	while ( xrtHttpTokenNextN(sText, iLen, &iOffset, &tToken) ) {
		if ( tToken.iLen == 0u || tToken.iLen > pResolved->iMaxTokenBytes ) { return false; }
		if ( ++iCount > pResolved->iMaxPairs ) { return false; }
	}
	return true;
}


// 校验 HTTP Token
XXAPI bool xrtHttpTokenValidate(const char* sText, const xrthttputillimits* pLimits)
{
	if ( sText == NULL ) { return false; }
	return xrtHttpTokenValidateN(sText, strlen(__xrt_cstr(sText)), pLimits);
}


// 校验 HTTP 参数
XXAPI bool xrtHttpParamValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits)
{
	xrthttputillimits tLocal;
	const xrthttputillimits* pResolved = __xrtHttpUtilResolveLimits(pLimits, &tLocal);
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrthttpparam tParam;
	if ( sText == NULL ) { return false; }
	while ( xrtHttpParamNextN(sText, iLen, &iOffset, &tParam) ) {
		if ( tParam.tName.iLen == 0u || tParam.tName.iLen > pResolved->iMaxNameBytes ) { return false; }
		if ( (tParam.iFlags & XRT_HTTP_PARAM_F_HAS_VALUE) != 0u && tParam.tValue.iLen > pResolved->iMaxValueBytes ) { return false; }
		if ( ++iCount > pResolved->iMaxPairs ) { return false; }
	}
	return true;
}


// 校验 HTTP 参数
XXAPI bool xrtHttpParamValidate(const char* sText, const xrthttputillimits* pLimits)
{
	if ( sText == NULL ) { return false; }
	return xrtHttpParamValidateN(sText, strlen(__xrt_cstr(sText)), pLimits);
}


// 校验查询
XXAPI bool xrtQueryValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits)
{
	xrthttputillimits tLocal;
	const xrthttputillimits* pResolved = __xrtHttpUtilResolveLimits(pLimits, &tLocal);
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtquerypair tPair;
	if ( sText == NULL ) { return false; }
	while ( xrtQueryNextN(sText, iLen, &iOffset, &tPair) ) {
		if ( tPair.tKey.iLen == 0u || tPair.tKey.iLen > pResolved->iMaxNameBytes ) { return false; }
		if ( (tPair.iFlags & XRT_QUERY_F_HAS_VALUE) != 0u && tPair.tValue.iLen > pResolved->iMaxValueBytes ) { return false; }
		if ( ++iCount > pResolved->iMaxPairs ) { return false; }
	}
	return true;
}


// 校验查询
XXAPI bool xrtQueryValidate(const char* sText, const xrthttputillimits* pLimits)
{
	if ( sText == NULL ) { return false; }
	return xrtQueryValidateN(sText, strlen(__xrt_cstr(sText)), pLimits);
}


// 校验 Cookie
XXAPI bool xrtCookieValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits)
{
	xrthttputillimits tLocal;
	const xrthttputillimits* pResolved = __xrtHttpUtilResolveLimits(pLimits, &tLocal);
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtcookiepair tCookie;
	if ( sText == NULL ) { return false; }
	while ( xrtCookieNextN(sText, iLen, &iOffset, &tCookie) ) {
		if ( tCookie.tName.iLen == 0u || tCookie.tName.iLen > pResolved->iMaxNameBytes ) { return false; }
		if ( tCookie.tValue.iLen > pResolved->iMaxValueBytes ) { return false; }
		if ( ++iCount > pResolved->iMaxPairs ) { return false; }
	}
	return true;
}


// 校验 Cookie
XXAPI bool xrtCookieValidate(const char* sText, const xrthttputillimits* pLimits)
{
	if ( sText == NULL ) { return false; }
	return xrtCookieValidateN(sText, strlen(__xrt_cstr(sText)), pLimits);
}


// 校验表单 URL 编码
XXAPI bool xrtFormUrlEncodedValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits)
{
	return xrtQueryValidateN(sText, iLen, pLimits);
}


// 校验表单 URL 编码
XXAPI bool xrtFormUrlEncodedValidate(const char* sText, const xrthttputillimits* pLimits)
{
	if ( sText == NULL ) { return false; }
	return xrtFormUrlEncodedValidateN(sText, strlen(__xrt_cstr(sText)), pLimits);
}


// 校验 HTTP 头部块
XXAPI bool xrtHttpHeaderBlockValidateN(const char* sBlock, size_t iLen, const xrthttputillimits* pLimits)
{
	xrthttputillimits tLocal;
	const xrthttputillimits* pResolved = __xrtHttpUtilResolveLimits(pLimits, &tLocal);
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtheaderpair tHeader;
	if ( sBlock == NULL ) { return false; }
	if ( iLen > pResolved->iMaxHeaderBytes ) { return false; }
	while ( xrtHttpHeaderNextLineN(sBlock, iLen, &iOffset, &tHeader) ) {
		size_t iLineBytes = tHeader.tName.iLen + 2u + tHeader.tValue.iLen;
		if ( tHeader.tName.iLen == 0u || tHeader.tName.iLen > pResolved->iMaxNameBytes ) { return false; }
		if ( tHeader.tValue.iLen > pResolved->iMaxValueBytes ) { return false; }
		if ( iLineBytes > pResolved->iMaxHeaderLineBytes ) { return false; }
		if ( ++iCount > pResolved->iMaxHeaderCount ) { return false; }
	}
	return iOffset == iLen;
}


// 校验 HTTP 头部块
XXAPI bool xrtHttpHeaderBlockValidate(const char* sBlock, const xrthttputillimits* pLimits)
{
	if ( sBlock == NULL ) { return false; }
	return xrtHttpHeaderBlockValidateN(sBlock, strlen(sBlock), pLimits);
}


// xrtSetCookieValidateN 相关处理
XXAPI bool xrtSetCookieValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits)
{
	xrthttputillimits tLocal;
	const xrthttputillimits* pResolved = __xrtHttpUtilResolveLimits(pLimits, &tLocal);
	xrtsetcookieview tCookie;
	if ( !xrtSetCookieParseN(sText, iLen, &tCookie) ) { return false; }
	if ( tCookie.tName.iLen == 0u || tCookie.tName.iLen > pResolved->iMaxNameBytes ) { return false; }
	if ( tCookie.tValue.iLen > pResolved->iMaxValueBytes ) { return false; }
	if ( (tCookie.iFlags & XRT_SET_COOKIE_F_HAS_DOMAIN) != 0u && tCookie.tDomain.iLen > pResolved->iMaxValueBytes ) { return false; }
	if ( (tCookie.iFlags & XRT_SET_COOKIE_F_HAS_PATH) != 0u && tCookie.tPath.iLen > pResolved->iMaxValueBytes ) { return false; }
	if ( (tCookie.iFlags & XRT_SET_COOKIE_F_HAS_EXPIRES) != 0u && tCookie.tExpires.iLen > pResolved->iMaxValueBytes ) { return false; }
	return true;
}


// xrtSetCookieValidate 相关处理
XXAPI bool xrtSetCookieValidate(const char* sText, const xrthttputillimits* pLimits)
{
	if ( sText == NULL ) { return false; }
	return xrtSetCookieValidateN(sText, strlen(__xrt_cstr(sText)), pLimits);
}


// 校验 Multipart 数据 (RFC 2046)
// 算法说明 - 校验 Multipart body 的完整性和各项限制
//   逐个遍历 Part, 校验边界、头部、各字段的长度限制以及头部行数限制
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
	// 步骤: 基本参数校验
	if ( sBody == NULL || sBoundary == NULL ) { return false; }
	if ( iBoundaryLen == 0u || iBoundaryLen > pResolved->iMaxBoundaryBytes ) { return false; }
	if ( iLen > pResolved->iMaxMultipartBytes ) { return false; }
	// 步骤: 校验边界字符串合法性
	if ( !__xrtHttpUtilValidateBoundaryN(sBoundary, iBoundaryLen) ) { return false; }
	// 算法步骤: 逐个遍历 Part, 校验每个 Part 的字段长度和头部
	while ( xrtMultipartNextN(sBody, iLen, sBoundary, iBoundaryLen, &iOffset, &tPart) ) {
		// 步骤: 校验 Part 数量限制
		if ( ++iPartCount > pResolved->iMaxMultipartParts ) { return false; }
		// 步骤: 校验 name、filename 等字段长度
		if ( (tPart.iFlags & XRT_MULTIPART_F_HAS_NAME) != 0u && (tPart.tName.iLen == 0u || tPart.tName.iLen > pResolved->iMaxNameBytes) ) { return false; }
		if ( (tPart.iFlags & XRT_MULTIPART_F_HAS_FILENAME) != 0u && tPart.tFileName.iLen > pResolved->iMaxValueBytes ) { return false; }
		if ( (tPart.iFlags & XRT_MULTIPART_F_HAS_FILENAME_EXT) != 0u && tPart.tFileNameExt.iLen > pResolved->iMaxValueBytes ) { return false; }
		if ( (tPart.iFlags & XRT_MULTIPART_F_HAS_CONTENT_TYPE) != 0u && tPart.tContentType.iLen > pResolved->iMaxValueBytes ) { return false; }
		if ( (tPart.iFlags & XRT_MULTIPART_F_HAS_TRANSFER_ENCODING) != 0u && tPart.tTransferEncoding.iLen > pResolved->iMaxValueBytes ) { return false; }
		// 算法步骤: 校验 Part 头部行数和各行长度限制
		iHeaderOff = 0u;
		iHeaderCount = 0u;
		while ( xrtHttpHeaderNextLineN(tPart.tHeaders.sPtr, tPart.tHeaders.iLen, &iHeaderOff, &tHeader) ) {
			size_t iLineBytes = tHeader.tName.iLen + 2u + tHeader.tValue.iLen;
			if ( tHeader.tName.iLen == 0u || tHeader.tName.iLen > pResolved->iMaxNameBytes ) { return false; }
			if ( tHeader.tValue.iLen > pResolved->iMaxValueBytes ) { return false; }
			if ( iLineBytes > pResolved->iMaxHeaderLineBytes ) { return false; }
			if ( ++iHeaderCount > pResolved->iMaxMultipartHeaders ) { return false; }
		}
		// 步骤: 校验头部数据是否完整消费
		if ( iHeaderOff != tPart.tHeaders.iLen ) { return false; }
	}
	return true;
}


// 校验 Multipart
XXAPI bool xrtMultipartValidate(const char* sBody, const char* sBoundary, const xrthttputillimits* pLimits)
{
	if ( sBody == NULL || sBoundary == NULL ) { return false; }
	return xrtMultipartValidateN(sBody, strlen(sBody), sBoundary, strlen(sBoundary), pLimits);
}


// 内部函数：__xrtHttpUtilIsAttrChar
static bool UNUSED_ATTR __xrtHttpUtilIsAttrChar(char ch)
{
	if ( ch >= '0' && ch <= '9' ) { return true; }
	if ( ch >= 'A' && ch <= 'Z' ) { return true; }
	if ( ch >= 'a' && ch <= 'z' ) { return true; }
	switch ( ch ) {
		case '!': case '#': case '$': case '&': case '+': case '-':
		case '.': case '^': case '_': case '`': case '|': case '~':
			return true;
		default:
			return false;
	}
}


// 内部函数：__xrtHttpUtilHexDigit
static char __xrtHttpUtilHexDigit(uint8 iValue)
{
	if ( iValue < 10u ) {
		return (char)('0' + (char)iValue);
	}
	return (char)('A' + (char)(iValue - 10u));
}


// 内部函数：追加 HTTP util 引用字符串
static bool __xrtHttpUtilAppendQuotedString(char* sOut, size_t iOutCap, size_t* pOffset, const char* sText, size_t iLen)
{
	size_t i;
	if ( sOut == NULL || pOffset == NULL || (sText == NULL && iLen != 0u) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\"", 1u) ) { return false; }
	for ( i = 0u; i < iLen; ++i ) {
		char ch = sText[i];
		if ( ch == '\r' || ch == '\n' || ch == '\0' ) { return false; }
		if ( ch == '"' || ch == '\\' ) {
			if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\\", 1u) ) { return false; }
		}
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, &ch, 1u) ) { return false; }
	}
	return __xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\"", 1u);
}


// 判断是否为 HTTP Token
XXAPI bool xrtHttpIsTokenN(const char* sText, size_t iLen)
{
	size_t i;
	if ( sText == NULL || iLen == 0u ) { return false; }
	for ( i = 0u; i < iLen; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(sText[i]) ) { return false; }
	}
	return true;
}


// 判断是否为 HTTP Token
XXAPI bool xrtHttpIsToken(const char* sText)
{
	if ( sText == NULL ) { return false; }
	return xrtHttpIsTokenN(sText, strlen(__xrt_cstr(sText)));
}


// 解码 HTTP 引用字符串
XXAPI bool xrtHttpQuotedStringDecodeToN(const char* sText, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iIn;
	size_t iOut = 0u;
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( sText == NULL || sOut == NULL || iOutCap == 0u ) { return false; }
	if ( iLen < 2u || sText[0] != '"' || sText[iLen - 1u] != '"' ) { return false; }
	sOut[0] = '\0';
	for ( iIn = 1u; iIn + 1u < iLen; ++iIn ) {
		unsigned char ch = (unsigned char)sText[iIn];
		if ( ch == '\r' || ch == '\n' || ch == '\0' ) { return false; }
		if ( ch == '\\' ) {
			if ( iIn + 2u > iLen - 1u ) { return false; }
			ch = (unsigned char)sText[++iIn];
			if ( ch == '\r' || ch == '\n' || ch == '\0' ) { return false; }
		} else if ( ch == '"' ) {
			return false;
		}
		if ( iOut + 1u >= iOutCap ) { return false; }
		sOut[iOut++] = (char)ch;
	}
	sOut[iOut] = '\0';
	if ( pOutLen ) { *pOutLen = iOut; }
	return true;
}


// 解码 HTTP 引用字符串
XXAPI bool xrtHttpQuotedStringDecodeTo(const char* sText, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( sText == NULL ) { return false; }
	return xrtHttpQuotedStringDecodeToN(sText, strlen(__xrt_cstr(sText)), sOut, iOutCap, pOutLen);
}


// 构建 HTTP 引用字符串
XXAPI bool xrtHttpQuotedStringBuildToN(const char* sText, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( sOut == NULL || iOutCap == 0u || (sText == NULL && iLen != 0u) ) { return false; }
	sOut[0] = '\0';
	if ( !__xrtHttpUtilAppendQuotedString(sOut, iOutCap, &iOff, sText, iLen) ) { return false; }
	if ( pOutLen ) { *pOutLen = iOff; }
	return true;
}


// 构建 HTTP 引用字符串
XXAPI bool xrtHttpQuotedStringBuildTo(const char* sText, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( sText == NULL ) { return false; }
	return xrtHttpQuotedStringBuildToN(sText, strlen(__xrt_cstr(sText)), sOut, iOutCap, pOutLen);
}


// URL 百分号编码 (RFC 3986 §2.1)
// 算法说明 - 将非保留字符以外的字节编码为 %HH 形式
//   保留字符 (A-Z a-z 0-9 - _ . ~) 直接输出
//   空格可选编码为 '+' (application/x-www-form-urlencoded) 或 '%20' (URI)
//   其他字节编码为 %XX (大写十六进制)
XXAPI bool xrtPercentEncodeTo(const char* sText, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen, bool bSpaceAsPlus)
{
	size_t iOut = 0u;
	size_t i;
	if ( sText == NULL || sOut == NULL || iOutCap == 0u ) { return false; }
	for ( i = 0u; i < iLen; ++i ) {
		unsigned char ch = (unsigned char)sText[i];
		// 算法步骤: 判断是否为无需编码的保留字符
		bool bKeep = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
			ch == '-' || ch == '_' || ch == '.' || ch == '~';
		// 步骤: 空格编码为 '+' (form-urlencoded 模式)
		if ( ch == ' ' && bSpaceAsPlus ) {
			if ( iOut + 1u >= iOutCap ) { return false; }
			sOut[iOut++] = '+';
			continue;
		}
		// 步骤: 保留字符直接输出
		if ( bKeep ) {
			if ( iOut + 1u >= iOutCap ) { return false; }
			sOut[iOut++] = (char)ch;
			continue;
		}
		// 算法步骤: 编码为 %XX 形式 (高4位 + 低4位)
		if ( iOut + 3u >= iOutCap ) { return false; }
		sOut[iOut++] = '%';
		sOut[iOut++] = __xrtHttpUtilHexDigit((uint8)((ch >> 4) & 0x0Fu));
		sOut[iOut++] = __xrtHttpUtilHexDigit((uint8)(ch & 0x0Fu));
	}
	sOut[iOut] = '\0';
	if ( pOutLen ) { *pOutLen = iOut; }
	return true;
}


// 内部函数：解析 RFC 5987 扩展值视图
// 算法说明 - 解析 RFC 5987 扩展值格式 "charset'language'encoded-value"
//   通过两个单引号 ''' 将字符串分为 charset、language、percent-encoded-value 三部分
//   charset 必须为合法 Token 字符, encoded-value 由百分号编码组成
static bool __xrtHttpUtilParseExtValueView(xrtstrview tRaw, xrtstrview* pCharset, xrtstrview* pLanguage, xrtstrview* pEncoded)
{
	size_t iFirstTick = (size_t)-1;
	size_t iSecondTick = (size_t)-1;
	size_t i;
	// 步骤: 初始化输出
	if ( pCharset ) { *pCharset = xrtStrView(NULL, 0u); }
	if ( pLanguage ) { *pLanguage = xrtStrView(NULL, 0u); }
	if ( pEncoded ) { *pEncoded = xrtStrView(NULL, 0u); }
	if ( xrtStrViewIsEmpty(tRaw) ) { return false; }
	// 算法步骤: 查找两个单引号分隔符
	for ( i = 0u; i < tRaw.iLen; ++i ) {
		if ( tRaw.sPtr[i] == '\'' ) {
			if ( iFirstTick == (size_t)-1 ) { iFirstTick = i; }
			else {
				iSecondTick = i;
				break;
			}
		}
	}
	// 步骤: 校验必须有两个单引号且 charset 非空
	if ( iFirstTick == (size_t)-1 || iSecondTick == (size_t)-1 || iFirstTick == 0u ) { return false; }
	// 步骤: 校验 charset 为合法的可打印 Token 字符
	for ( i = 0u; i < iFirstTick; ++i ) {
		unsigned char ch = (unsigned char)tRaw.sPtr[i];
		if ( ch <= 0x20u || ch >= 0x7Fu || !__xrtHttpUtilIsTokenChar(tRaw.sPtr[i]) ) { return false; }
	}
	// 步骤: 提取三个部分: charset、language、encoded-value
	if ( pCharset ) { *pCharset = xrtStrView(tRaw.sPtr, iFirstTick); }
	if ( pLanguage ) { *pLanguage = xrtStrView(tRaw.sPtr + iFirstTick + 1u, iSecondTick - iFirstTick - 1u); }
	if ( pEncoded ) { *pEncoded = xrtStrView(tRaw.sPtr + iSecondTick + 1u, tRaw.iLen - iSecondTick - 1u); }
	return true;
}


// 解码 HTTP 扩展值
XXAPI bool xrtHttpDecodeExtValueTo(const char* sText, size_t iLen, xrtstrview* pCharset, xrtstrview* pLanguage, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	xrtstrview tCharset;
	xrtstrview tLanguage;
	xrtstrview tEncoded;
	if ( sText == NULL || sOut == NULL || iOutCap == 0u ) { return false; }
	if ( !__xrtHttpUtilParseExtValueView(xrtStrView(sText, iLen), &tCharset, &tLanguage, &tEncoded) ) { return false; }
	if ( !xrtPercentDecodeTo(tEncoded.sPtr, tEncoded.iLen, sOut, iOutCap, pOutLen, false) ) { return false; }
	if ( pCharset ) { *pCharset = tCharset; }
	if ( pLanguage ) { *pLanguage = tLanguage; }
	return true;
}


// 解码 HTTP 扩展值
XXAPI bool xrtHttpDecodeExtValue(const char* sText, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( sText == NULL ) { return false; }
	return xrtHttpDecodeExtValueTo(sText, strlen(__xrt_cstr(sText)), NULL, NULL, sOut, iOutCap, pOutLen);
}


// 构建 HTTP 扩展值 (RFC 5987)
// 算法说明 - 构建 RFC 5987 扩展值格式 "charset'language'percent-encoded-value"
//   charset 默认 UTF-8, language 可为空, value 部分使用百分号编码
XXAPI bool xrtHttpBuildExtValueTo(const char* sCharset, const char* sLanguage, const char* sText, size_t iTextLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	size_t iEncLen = 0u;
	size_t iCharsetLen;
	size_t iLanguageLen;
	size_t i;
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( sText == NULL || sOut == NULL || iOutCap == 0u ) { return false; }
	// 步骤: 设置默认值
	if ( sCharset == NULL || sCharset[0] == '\0' ) { sCharset = "UTF-8"; }
	if ( sLanguage == NULL ) { sLanguage = ""; }
	iCharsetLen = strlen(sCharset);
	iLanguageLen = strlen(sLanguage);
	// 步骤: 校验 charset 为合法可打印 Token 字符
	for ( i = 0u; i < iCharsetLen; ++i ) {
		unsigned char ch = (unsigned char)sCharset[i];
		if ( ch <= 0x20u || ch >= 0x7Fu || !__xrtHttpUtilIsTokenChar(sCharset[i]) ) { return false; }
	}
	// 步骤: 校验 language 为合法可打印字符且不含单引号
	for ( i = 0u; i < iLanguageLen; ++i ) {
		unsigned char ch = (unsigned char)sLanguage[i];
		if ( ch <= 0x20u || ch >= 0x7Fu || sLanguage[i] == '\'' ) { return false; }
	}
	// 算法步骤: 拼接 charset'language'
	sOut[0] = '\0';
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, sCharset, iCharsetLen) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "'", 1u) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, sLanguage, iLanguageLen) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "'", 1u) ) { return false; }
	// 算法步骤: 对 value 部分进行百分号编码
	if ( !xrtPercentEncodeTo(sText, iTextLen, sOut + iOff, iOutCap - iOff, &iEncLen, false) ) { return false; }
	iOff += iEncLen;
	if ( pOutLen ) { *pOutLen = iOff; }
	return true;
}


// 构建 HTTP 扩展值
XXAPI bool xrtHttpBuildExtValue(const char* sCharset, const char* sLanguage, const char* sText, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( sText == NULL ) { return false; }
	return xrtHttpBuildExtValueTo(sCharset, sLanguage, sText, strlen(__xrt_cstr(sText)), sOut, iOutCap, pOutLen);
}


// HTTP 头部 split 行相关处理
XXAPI bool xrtHttpHeaderSplitLineN(const char* sLine, size_t iLen, xrtheaderpair* pOut)
{
	size_t iColon = (size_t)-1;
	size_t i;
	if ( sLine == NULL || pOut == NULL || iLen == 0u ) { return false; }
	for ( i = 0u; i < iLen; ++i ) {
		if ( sLine[i] == ':' ) {
			iColon = i;
			break;
		}
		if ( sLine[i] == '\r' || sLine[i] == '\n' ) { return false; }
	}
	if ( iColon == (size_t)-1 || iColon == 0u ) { return false; }
	for ( i = 0u; i < iColon; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(sLine[i]) ) { return false; }
	}
	pOut->tName = xrtStrView(sLine, iColon);
	pOut->tValue = xrtStrView(sLine + iColon + 1u, iLen - iColon - 1u);
	__xrtHttpUtilTrimView(&pOut->tValue);
	return true;
}


// HTTP 头部 split 行相关处理
XXAPI bool xrtHttpHeaderSplitLine(const char* sLine, xrtheaderpair* pOut)
{
	if ( sLine == NULL ) { return false; }
	return xrtHttpHeaderSplitLineN(sLine, strlen(sLine), pOut);
}


// 构建 HTTP 头部行
XXAPI bool xrtHttpHeaderBuildLineTo(const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	size_t i;
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( sName == NULL || (sValue == NULL && iValueLen != 0u) || sOut == NULL || iOutCap == 0u ) { return false; }
	if ( iNameLen == 0u ) { return false; }
	for ( i = 0u; i < iNameLen; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(sName[i]) ) { return false; }
	}
	if ( __xrtHttpUtilContainsCtl(sValue, iValueLen) ) { return false; }
	sOut[0] = '\0';
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, sName, iNameLen) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, ": ", 2u) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, sValue, iValueLen) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "\r\n", 2u) ) { return false; }
	if ( pOutLen ) { *pOutLen = iOff; }
	return true;
}


// 构建 HTTP 头部行
XXAPI bool xrtHttpHeaderBuildLine(const char* sName, const char* sValue, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( sName == NULL || sValue == NULL ) { return false; }
	return xrtHttpHeaderBuildLineTo(sName, strlen(__xrt_cstr(sName)), sValue, strlen(sValue), sOut, iOutCap, pOutLen);
}


// 构建 HTTP 头部规范化行
XXAPI bool xrtHttpHeaderBuildCanonicalLineToN(const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	size_t i;
	bool bUpper = true;
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( sName == NULL || (sValue == NULL && iValueLen != 0u) || sOut == NULL || iOutCap == 0u ) { return false; }
	if ( iNameLen == 0u ) { return false; }
	if ( __xrtHttpUtilContainsCtl(sValue, iValueLen) ) { return false; }
	sOut[0] = '\0';
	for ( i = 0u; i < iNameLen; ++i ) {
		char ch = sName[i];
		if ( !__xrtHttpUtilIsTokenChar(ch) ) { return false; }
		if ( ch >= 'A' && ch <= 'Z' ) { ch = (char)(ch + 32); }
		if ( bUpper && ch >= 'a' && ch <= 'z' ) { ch = (char)(ch - 32); }
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, &ch, 1u) ) { return false; }
		bUpper = (ch == '-');
	}
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, ": ", 2u) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, sValue, iValueLen) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "\r\n", 2u) ) { return false; }
	if ( pOutLen ) { *pOutLen = iOff; }
	return true;
}


// 构建 HTTP 头部规范化行
XXAPI bool xrtHttpHeaderBuildCanonicalLineTo(const char* sName, const char* sValue, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( sName == NULL || sValue == NULL ) { return false; }
	return xrtHttpHeaderBuildCanonicalLineToN(sName, strlen(__xrt_cstr(sName)), sValue, strlen(sValue), sOut, iOutCap, pOutLen);
}


// 构建 HTTP 头部块
XXAPI bool xrtHttpHeaderBuildBlockTo(const xrtheaderpair* pHeaders, size_t iCount, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	size_t iLineLen = 0u;
	size_t i;
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( sOut == NULL || iOutCap == 0u || (pHeaders == NULL && iCount != 0u) ) { return false; }
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
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "\r\n", 2u) ) { return false; }
	if ( pOutLen ) { *pOutLen = iOff; }
	return true;
}


// 构建 HTTP 头部规范化块
XXAPI bool xrtHttpHeaderBuildCanonicalBlockTo(const xrtheaderpair* pHeaders, size_t iCount, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	size_t iLineLen = 0u;
	size_t i;
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( sOut == NULL || iOutCap == 0u || (pHeaders == NULL && iCount != 0u) ) { return false; }
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
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "\r\n", 2u) ) { return false; }
	if ( pOutLen ) { *pOutLen = iOff; }
	return true;
}


// 获取下一个 HTTP Token
XXAPI bool xrtHttpTokenNextN(const char* sText, size_t iLen, size_t* pOffset, xrtstrview* pOut)
{
	size_t iCur;
	size_t iEnd;
	size_t i;
	if ( pOut ) { memset(pOut, 0, sizeof(xrtstrview)); }
	if ( sText == NULL || pOffset == NULL || pOut == NULL ) { return false; }
	iCur = *pOffset;
	while ( iCur < iLen && (sText[iCur] == ',' || sText[iCur] == ' ' || sText[iCur] == '\t') ) { iCur++; }
	if ( iCur >= iLen ) {
		*pOffset = iCur;
		return false;
	}
	iEnd = iCur;
	while ( iEnd < iLen && sText[iEnd] != ',' ) { iEnd++; }
	while ( iEnd > iCur && (sText[iEnd - 1u] == ' ' || sText[iEnd - 1u] == '\t') ) { iEnd--; }
	if ( iEnd <= iCur ) { return false; }
	for ( i = iCur; i < iEnd; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(sText[i]) ) { return false; }
	}
	pOut->sPtr = sText + iCur;
	pOut->iLen = iEnd - iCur;
	*pOffset = (iEnd < iLen) ? (iEnd + 1u) : iEnd;
	return true;
}


// 获取下一个 HTTP Token
XXAPI bool xrtHttpTokenNext(const char* sText, size_t* pOffset, xrtstrview* pOut)
{
	if ( sText == NULL ) { return false; }
	return xrtHttpTokenNextN(sText, strlen(__xrt_cstr(sText)), pOffset, pOut);
}


// 统计 HTTP Token
XXAPI size_t xrtHttpTokenCountN(const char* sText, size_t iLen)
{
	size_t iCount = 0u;
	size_t iOffset = 0u;
	xrtstrview tToken;
	while ( xrtHttpTokenNextN(sText, iLen, &iOffset, &tToken) ) { ++iCount; }
	return iCount;
}


// 统计 HTTP Token
XXAPI size_t xrtHttpTokenCount(const char* sText)
{
	if ( sText == NULL ) { return 0u; }
	return xrtHttpTokenCountN(sText, strlen(__xrt_cstr(sText)));
}


// 查找 HTTP Token
XXAPI bool xrtHttpTokenFindN(const char* sText, size_t iLen, const char* sToken, size_t iTokenLen, xrtstrview* pOut)
{
	size_t iOffset = 0u;
	xrtstrview tToken;
	if ( pOut ) { memset(pOut, 0, sizeof(xrtstrview)); }
	if ( sText == NULL || sToken == NULL || iTokenLen == 0u ) { return false; }
	while ( xrtHttpTokenNextN(sText, iLen, &iOffset, &tToken) ) {
		if ( tToken.iLen == iTokenLen && __xrtHttpUtilEqNoCaseN(tToken.sPtr, tToken.iLen, sToken, iTokenLen) ) {
			if ( pOut ) { *pOut = tToken; }
			return true;
		}
	}
	return false;
}


// 查找 HTTP Token
XXAPI bool xrtHttpTokenFind(const char* sText, const char* sToken, xrtstrview* pOut)
{
	if ( sText == NULL || sToken == NULL ) { return false; }
	return xrtHttpTokenFindN(sText, strlen(__xrt_cstr(sText)), sToken, strlen(__xrt_cstr(sToken)), pOut);
}


// 解析 HTTP Token
XXAPI bool xrtHttpTokenParseToN(const char* sText, size_t iLen, xrtstrview* pOut, size_t iCap, size_t* pCount)
{
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtstrview tToken;
	if ( pCount ) { *pCount = 0u; }
	if ( sText == NULL || (pOut == NULL && iCap != 0u) ) { return false; }
	while ( xrtHttpTokenNextN(sText, iLen, &iOffset, &tToken) ) {
		if ( iCount >= iCap ) { return false; }
		if ( pOut ) { pOut[iCount] = tToken; }
		++iCount;
	}
	if ( pCount ) { *pCount = iCount; }
	return true;
}


// 解析 HTTP Token
XXAPI bool xrtHttpTokenParseTo(const char* sText, xrtstrview* pOut, size_t iCap, size_t* pCount)
{
	if ( sText == NULL ) { return false; }
	return xrtHttpTokenParseToN(sText, strlen(__xrt_cstr(sText)), pOut, iCap, pCount);
}


// 追加 HTTP Token
XXAPI bool xrtHttpTokenAppendTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sToken, size_t iTokenLen)
{
	size_t i;
	if ( sOut == NULL || pOffset == NULL || sToken == NULL || iTokenLen == 0u ) { return false; }
	for ( i = 0u; i < iTokenLen; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(sToken[i]) ) { return false; }
	}
	if ( *pOffset > 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, ", ", 2u) ) { return false; }
	}
	return __xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sToken, iTokenLen);
}


// 追加 HTTP Token
XXAPI bool xrtHttpTokenAppend(char* sOut, size_t iOutCap, size_t* pOffset, const char* sToken)
{
	if ( sToken == NULL ) { return false; }
	return xrtHttpTokenAppendTo(sOut, iOutCap, pOffset, sToken, strlen(__xrt_cstr(sToken)));
}


// 判断是否包含 HTTP 头部 Token
XXAPI bool xrtHttpHeaderContainsTokenN(const char* sValue, size_t iValueLen, const char* sToken)
{
	if ( sValue == NULL || sToken == NULL || sToken[0] == '\0' ) { return false; }
	return xrtHttpTokenFindN(sValue, iValueLen, sToken, strlen(__xrt_cstr(sToken)), NULL);
}


// 判断是否包含 HTTP 头部 Token
XXAPI bool xrtHttpHeaderContainsToken(const char* sValue, const char* sToken)
{
	if ( sValue == NULL ) { return false; }
	return xrtHttpHeaderContainsTokenN(sValue, strlen(sValue), sToken);
}


// 查找 HTTP 头部
XXAPI bool xrtHttpHeaderFindN(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, xrtstrview* pOut)
{
	size_t iNameLen;
	size_t i;
	if ( pHeaders == NULL || sName == NULL ) { return false; }
	iNameLen = strlen(__xrt_cstr(sName));
	for ( i = 0u; i < iCount; ++i ) {
		if ( __xrtHttpUtilEqNoCaseN(pHeaders[i].tName.sPtr, pHeaders[i].tName.iLen, sName, iNameLen) ) {
			if ( pOut ) { *pOut = pHeaders[i].tValue; }
			return true;
		}
	}
	return false;
}


// 查找 HTTP 头部
XXAPI bool xrtHttpHeaderFind(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, xrtstrview* pOut)
{
	return xrtHttpHeaderFindN(pHeaders, iCount, sName, pOut);
}


// 统计 HTTP 头部
XXAPI size_t xrtHttpHeaderCountN(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, size_t iNameLen)
{
	size_t i;
	size_t iHits = 0u;
	if ( pHeaders == NULL || sName == NULL || iNameLen == 0u ) { return 0u; }
	for ( i = 0u; i < iCount; ++i ) {
		if ( __xrtHttpUtilEqNoCaseN(pHeaders[i].tName.sPtr, pHeaders[i].tName.iLen, sName, iNameLen) ) { ++iHits; }
	}
	return iHits;
}


// 统计 HTTP 头部
XXAPI size_t xrtHttpHeaderCount(const xrtheaderpair* pHeaders, size_t iCount, const char* sName)
{
	if ( sName == NULL ) { return 0u; }
	return xrtHttpHeaderCountN(pHeaders, iCount, sName, strlen(__xrt_cstr(sName)));
}


// 查找 HTTP 头部 nth
XXAPI bool xrtHttpHeaderFindNthN(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, size_t iNameLen, size_t iNth, xrtstrview* pOut)
{
	size_t i;
	size_t iSeen = 0u;
	if ( pOut ) { *pOut = xrtStrView(NULL, 0u); }
	if ( pHeaders == NULL || sName == NULL || pOut == NULL || iNameLen == 0u ) { return false; }
	for ( i = 0u; i < iCount; ++i ) {
		if ( !__xrtHttpUtilEqNoCaseN(pHeaders[i].tName.sPtr, pHeaders[i].tName.iLen, sName, iNameLen) ) { continue; }
		if ( iSeen == iNth ) {
			*pOut = pHeaders[i].tValue;
			return true;
		}
		++iSeen;
	}
	return false;
}


// 查找 HTTP 头部 nth
XXAPI bool xrtHttpHeaderFindNth(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, size_t iNth, xrtstrview* pOut)
{
	if ( sName == NULL ) { return false; }
	return xrtHttpHeaderFindNthN(pHeaders, iCount, sName, strlen(__xrt_cstr(sName)), iNth, pOut);
}


// 查找 HTTP 头部全部
XXAPI size_t xrtHttpHeaderFindAllToN(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, size_t iNameLen, xrtstrview* pOut, size_t iOutCap)
{
	size_t i;
	size_t iHits = 0u;
	if ( pHeaders == NULL || sName == NULL || iNameLen == 0u ) { return 0u; }
	for ( i = 0u; i < iCount; ++i ) {
		if ( !__xrtHttpUtilEqNoCaseN(pHeaders[i].tName.sPtr, pHeaders[i].tName.iLen, sName, iNameLen) ) { continue; }
		if ( pOut != NULL && iHits < iOutCap ) { pOut[iHits] = pHeaders[i].tValue; }
		++iHits;
	}
	return iHits;
}


// 查找 HTTP 头部全部
XXAPI size_t xrtHttpHeaderFindAllTo(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, xrtstrview* pOut, size_t iOutCap)
{
	if ( sName == NULL ) { return 0u; }
	return xrtHttpHeaderFindAllToN(pHeaders, iCount, sName, strlen(__xrt_cstr(sName)), pOut, iOutCap);
}


// HTTP 头部 canonicalize 名称相关处理
XXAPI bool xrtHttpHeaderCanonicalizeNameToN(const char* sName, size_t iNameLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t i;
	bool bUpper = true;
	if ( sName == NULL || sOut == NULL || iOutCap == 0u ) { return false; }
	if ( iNameLen + 1u > iOutCap ) { return false; }
	for ( i = 0u; i < iNameLen; ++i ) {
		char ch = sName[i];
		if ( !__xrtHttpUtilIsTokenChar(ch) ) { return false; }
		if ( ch >= 'A' && ch <= 'Z' ) { ch = (char)(ch + 32); }
		if ( bUpper && ch >= 'a' && ch <= 'z' ) { ch = (char)(ch - 32); }
		sOut[i] = ch;
		bUpper = (ch == '-');
	}
	sOut[iNameLen] = '\0';
	if ( pOutLen ) { *pOutLen = iNameLen; }
	return true;
}


// HTTP 头部 canonicalize 名称相关处理
XXAPI bool xrtHttpHeaderCanonicalizeNameTo(const char* sName, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( sName == NULL ) { return false; }
	return xrtHttpHeaderCanonicalizeNameToN(sName, strlen(__xrt_cstr(sName)), sOut, iOutCap, pOutLen);
}


// 加入 HTTP 头部 values
XXAPI bool xrtHttpHeaderJoinValuesTo(const xrtstrview* pValues, size_t iCount, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t i;
	size_t iOff = 0u;
	if ( sOut == NULL || iOutCap == 0u || (pValues == NULL && iCount != 0u) ) { return false; }
	sOut[0] = '\0';
	for ( i = 0u; i < iCount; ++i ) {
		if ( i != 0u ) {
			if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, ", ", 2u) ) { return false; }
		}
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pValues[i].sPtr, pValues[i].iLen) ) { return false; }
	}
	if ( pOutLen ) { *pOutLen = iOff; }
	return true;
}


// xrtHttpHeaderCollectAndJoinToN 相关处理
XXAPI bool xrtHttpHeaderCollectAndJoinToN(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, size_t iNameLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t i;
	size_t iOff = 0u;
	size_t iHits = 0u;
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( pHeaders == NULL || sName == NULL || sOut == NULL || iOutCap == 0u || iNameLen == 0u ) { return false; }
	sOut[0] = '\0';
	for ( i = 0u; i < iCount; ++i ) {
		if ( !__xrtHttpUtilEqNoCaseN(pHeaders[i].tName.sPtr, pHeaders[i].tName.iLen, sName, iNameLen) ) { continue; }
		if ( iHits != 0u ) {
			if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, ", ", 2u) ) { return false; }
		}
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pHeaders[i].tValue.sPtr, pHeaders[i].tValue.iLen) ) { return false; }
		++iHits;
	}
	if ( iHits == 0u ) { return false; }
	if ( pOutLen ) { *pOutLen = iOff; }
	return true;
}


// xrtHttpHeaderCollectAndJoinTo 相关处理
XXAPI bool xrtHttpHeaderCollectAndJoinTo(const xrtheaderpair* pHeaders, size_t iCount, const char* sName, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( sName == NULL ) { return false; }
	return xrtHttpHeaderCollectAndJoinToN(pHeaders, iCount, sName, strlen(__xrt_cstr(sName)), sOut, iOutCap, pOutLen);
}


// 获取下一个 HTTP 头部行
// 算法说明 - 迭代解析 HTTP 头部块, 每行格式为 "Name: Value\r\n"
//   连续的 \r\n 表示头部块结束, 最后一行可无 \r\n 终止
XXAPI bool xrtHttpHeaderNextLineN(const char* sBlock, size_t iLen, size_t* pOffset, xrtheaderpair* pOut)
{
	size_t iCur;
	size_t iEnd;
	if ( sBlock == NULL || pOffset == NULL || pOut == NULL ) { return false; }
	iCur = *pOffset;
	if ( iCur >= iLen ) { return false; }
	// 步骤: 检查是否到达头部块结束标记(\r\n 空行)
	if ( iCur + 1u < iLen && sBlock[iCur] == '\r' && sBlock[iCur + 1u] == '\n' ) {
		*pOffset = iCur + 2u;
		return false;
	}
	// 算法步骤: 定位当前行的结尾(查找 \r\n)
	iEnd = iCur;
	while ( iEnd < iLen ) {
		if ( sBlock[iEnd] == '\n' ) {
			// 步骤: 校验 \n 前必须有 \r
			if ( iEnd == iCur || sBlock[iEnd - 1u] != '\r' ) { return false; }
			break;
		}
		iEnd++;
	}
	// 步骤: 找到 \r\n, 解析该行头部并跳过换行符
	if ( iEnd < iLen ) {
		if ( !xrtHttpHeaderSplitLineN(sBlock + iCur, iEnd - iCur - 1u, pOut) ) { return false; }
		*pOffset = iEnd + 1u;
		return true;
	}
	// 步骤: 未找到 \r\n(最后一行), 解析剩余内容
	if ( !xrtHttpHeaderSplitLineN(sBlock + iCur, iLen - iCur, pOut) ) { return false; }
	*pOffset = iLen;
	return true;
}


// 获取下一个 HTTP 头部行
XXAPI bool xrtHttpHeaderNextLine(const char* sBlock, size_t* pOffset, xrtheaderpair* pOut)
{
	if ( sBlock == NULL ) { return false; }
	return xrtHttpHeaderNextLineN(sBlock, strlen(sBlock), pOffset, pOut);
}


// 查找 HTTP 头部行
XXAPI bool xrtHttpHeaderFindLineN(const char* sBlock, size_t iLen, const char* sName, xrtheaderpair* pOut)
{
	size_t iOffset = 0u;
	xrtheaderpair tHeader;
	size_t iNameLen;
	if ( sBlock == NULL || sName == NULL ) { return false; }
	iNameLen = strlen(__xrt_cstr(sName));
	while ( xrtHttpHeaderNextLineN(sBlock, iLen, &iOffset, &tHeader) ) {
		if ( __xrtHttpUtilEqNoCaseN(tHeader.tName.sPtr, tHeader.tName.iLen, sName, iNameLen) ) {
			if ( pOut ) { *pOut = tHeader; }
			return true;
		}
	}
	return false;
}


// 查找 HTTP 头部行
XXAPI bool xrtHttpHeaderFindLine(const char* sBlock, const char* sName, xrtheaderpair* pOut)
{
	if ( sBlock == NULL ) { return false; }
	return xrtHttpHeaderFindLineN(sBlock, strlen(sBlock), sName, pOut);
}


// 解析 HTTP 头部块
XXAPI bool xrtHttpHeaderParseBlockToN(const char* sBlock, size_t iLen, xrtheaderpair* pHeaders, size_t iCap, size_t* pCount)
{
	size_t iOffset = 0u;
	size_t iCountOut = 0u;
	xrtheaderpair tHeader;
	if ( pCount ) { *pCount = 0u; }
	if ( sBlock == NULL || (pHeaders == NULL && iCap != 0u) ) { return false; }
	while ( xrtHttpHeaderNextLineN(sBlock, iLen, &iOffset, &tHeader) ) {
		if ( iCountOut >= iCap ) { return false; }
		pHeaders[iCountOut++] = tHeader;
	}
	if ( iOffset < iLen ) {
		if ( !(iOffset + 1u == iLen && sBlock[iOffset] == '\n') &&
		     !(iOffset + 2u == iLen && sBlock[iOffset] == '\r' && sBlock[iOffset + 1u] == '\n') ) {
			if ( iOffset + 1u < iLen || !(sBlock[iOffset] == '\r' || sBlock[iOffset] == '\n') ) { return false; }
		}
	}
	if ( pCount ) { *pCount = iCountOut; }
	return true;
}


// 解析 HTTP 头部块
XXAPI bool xrtHttpHeaderParseBlockTo(const char* sBlock, xrtheaderpair* pHeaders, size_t iCap, size_t* pCount)
{
	if ( sBlock == NULL ) { return false; }
	return xrtHttpHeaderParseBlockToN(sBlock, strlen(sBlock), pHeaders, iCap, pCount);
}


// 追加 HTTP 头部键值对
XXAPI bool xrtHttpHeaderAppendPairN(xrtheaderpair* pHeaders, size_t iCap, size_t* pCount, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen)
{
	size_t i;
	if ( pHeaders == NULL || pCount == NULL || sName == NULL || (sValue == NULL && iValueLen != 0u) ) { return false; }
	if ( *pCount >= iCap || iNameLen == 0u ) { return false; }
	for ( i = 0u; i < iNameLen; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(sName[i]) ) { return false; }
	}
	if ( __xrtHttpUtilContainsCtl(sValue, iValueLen) ) { return false; }
	pHeaders[*pCount].tName = xrtStrView(sName, iNameLen);
	pHeaders[*pCount].tValue = xrtStrView(sValue, iValueLen);
	(*pCount)++;
	return true;
}


// 追加 HTTP 头部键值对
XXAPI bool xrtHttpHeaderAppendPair(xrtheaderpair* pHeaders, size_t iCap, size_t* pCount, const char* sName, const char* sValue)
{
	if ( sName == NULL || sValue == NULL ) { return false; }
	return xrtHttpHeaderAppendPairN(pHeaders, iCap, pCount, sName, strlen(__xrt_cstr(sName)), sValue, strlen(sValue));
}


// 设置 HTTP 头部键值对
XXAPI bool xrtHttpHeaderSetPairN(xrtheaderpair* pHeaders, size_t iCap, size_t* pCount, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen)
{
	size_t i;
	if ( pHeaders == NULL || pCount == NULL || sName == NULL || (sValue == NULL && iValueLen != 0u) ) { return false; }
	for ( i = 0u; i < *pCount; ++i ) {
		if ( pHeaders[i].tName.iLen == iNameLen && __xrtHttpUtilEqNoCaseN(pHeaders[i].tName.sPtr, pHeaders[i].tName.iLen, sName, iNameLen) ) {
			if ( __xrtHttpUtilContainsCtl(sValue, iValueLen) ) { return false; }
			pHeaders[i].tValue = xrtStrView(sValue, iValueLen);
			return true;
		}
	}
	return xrtHttpHeaderAppendPairN(pHeaders, iCap, pCount, sName, iNameLen, sValue, iValueLen);
}


// 设置 HTTP 头部键值对
XXAPI bool xrtHttpHeaderSetPair(xrtheaderpair* pHeaders, size_t iCap, size_t* pCount, const char* sName, const char* sValue)
{
	if ( sName == NULL || sValue == NULL ) { return false; }
	return xrtHttpHeaderSetPairN(pHeaders, iCap, pCount, sName, strlen(__xrt_cstr(sName)), sValue, strlen(sValue));
}


// 删除 HTTP 头部
XXAPI size_t xrtHttpHeaderRemoveN(xrtheaderpair* pHeaders, size_t* pCount, const char* sName, size_t iNameLen)
{
	size_t iRead;
	size_t iWrite = 0u;
	size_t iRemoved = 0u;
	if ( pHeaders == NULL || pCount == NULL || sName == NULL || iNameLen == 0u ) { return 0u; }
	for ( iRead = 0u; iRead < *pCount; ++iRead ) {
		if ( pHeaders[iRead].tName.iLen == iNameLen &&
		     __xrtHttpUtilEqNoCaseN(pHeaders[iRead].tName.sPtr, pHeaders[iRead].tName.iLen, sName, iNameLen) ) {
			iRemoved++;
			continue;
		}
		if ( iWrite != iRead ) { pHeaders[iWrite] = pHeaders[iRead]; }
		iWrite++;
	}
	*pCount = iWrite;
	return iRemoved;
}


// 删除 HTTP 头部
XXAPI size_t xrtHttpHeaderRemove(xrtheaderpair* pHeaders, size_t* pCount, const char* sName)
{
	if ( sName == NULL ) { return 0u; }
	return xrtHttpHeaderRemoveN(pHeaders, pCount, sName, strlen(__xrt_cstr(sName)));
}


// 获取下一个 Cookie (RFC 6265 §4.2.1)
// 算法说明 - 迭代解析 Cookie 头部, 格式为 "name1=value1; name2=value2; ..."
//   Cookie 对以 ';' 分隔, name 和 value 以 '=' 分隔
//   name 必须为合法 HTTP Token, value 两端空白会被裁剪
XXAPI bool xrtCookieNextN(const char* sText, size_t iLen, size_t* pOffset, xrtcookiepair* pOut)
{
	size_t iCur;
	size_t iEnd;
	size_t iEq = (size_t)-1;
	size_t i;
	if ( sText == NULL || pOffset == NULL || pOut == NULL ) { return false; }
	// 步骤: 跳过分隔符 ';' 和空白字符
	iCur = *pOffset;
	while ( iCur < iLen && (sText[iCur] == ';' || sText[iCur] == ' ' || sText[iCur] == '\t') ) { iCur++; }
	if ( iCur >= iLen ) {
		*pOffset = iCur;
		return false;
	}
	// 算法步骤: 定位当前 Cookie 对的边界, 查找分号和等号
	iEnd = iCur;
	while ( iEnd < iLen && sText[iEnd] != ';' ) {
		if ( sText[iEnd] == '=' && iEq == (size_t)-1 ) { iEq = iEnd; }
		iEnd++;
	}
	// 步骤: 校验必须有等号且 name 非空
	if ( iEq == (size_t)-1 || iEq == iCur ) { return false; }
	// 步骤: 提取 name 和 value 并裁剪两端空白
	pOut->tName = xrtStrView(sText + iCur, iEq - iCur);
	pOut->tValue = xrtStrView(sText + iEq + 1u, iEnd - iEq - 1u);
	__xrtHttpUtilTrimView(&pOut->tName);
	__xrtHttpUtilTrimView(&pOut->tValue);
	if ( pOut->tName.iLen == 0u ) { return false; }
	// 步骤: 校验 name 为合法 Token 字符
	for ( i = 0u; i < pOut->tName.iLen; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(pOut->tName.sPtr[i]) ) { return false; }
	}
	// 步骤: 更新偏移位置
	*pOffset = (iEnd < iLen) ? (iEnd + 1u) : iEnd;
	return true;
}


// 获取下一个 Cookie
XXAPI bool xrtCookieNext(const char* sText, size_t* pOffset, xrtcookiepair* pOut)
{
	if ( sText == NULL ) { return false; }
	return xrtCookieNextN(sText, strlen(__xrt_cstr(sText)), pOffset, pOut);
}


// 查找 Cookie
XXAPI bool xrtCookieFindN(const char* sText, size_t iLen, const char* sName, size_t iNameLen, xrtcookiepair* pOut)
{
	size_t iOffset = 0u;
	xrtcookiepair tCookie;
	if ( sText == NULL || sName == NULL || iNameLen == 0u ) { return false; }
	while ( xrtCookieNextN(sText, iLen, &iOffset, &tCookie) ) {
		if ( tCookie.tName.iLen == iNameLen && memcmp(tCookie.tName.sPtr, sName, iNameLen) == 0 ) {
			if ( pOut ) { *pOut = tCookie; }
			return true;
		}
	}
	return false;
}


// 查找 Cookie
XXAPI bool xrtCookieFind(const char* sText, const char* sName, xrtcookiepair* pOut)
{
	if ( sText == NULL || sName == NULL ) { return false; }
	return xrtCookieFindN(sText, strlen(__xrt_cstr(sText)), sName, strlen(__xrt_cstr(sName)), pOut);
}


// 解析 Cookie
XXAPI bool xrtCookieParseToN(const char* sText, size_t iLen, xrtcookiepair* pOut, size_t iCap, size_t* pCount)
{
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtcookiepair tCookie;
	if ( pCount ) { *pCount = 0u; }
	if ( sText == NULL || (pOut == NULL && iCap != 0u) ) { return false; }
	while ( xrtCookieNextN(sText, iLen, &iOffset, &tCookie) ) {
		if ( iCount >= iCap ) { return false; }
		if ( pOut ) { pOut[iCount] = tCookie; }
		++iCount;
	}
	if ( pCount ) { *pCount = iCount; }
	return true;
}


// 解析 Cookie
XXAPI bool xrtCookieParseTo(const char* sText, xrtcookiepair* pOut, size_t iCap, size_t* pCount)
{
	if ( sText == NULL ) { return false; }
	return xrtCookieParseToN(sText, strlen(__xrt_cstr(sText)), pOut, iCap, pCount);
}


// 解析 Set-Cookie 头部值 (RFC 6265 §4.1.1)
// 算法说明 - Set-Cookie 头部格式为 "name=value; 属性1; 属性2=值2 ..."
//   第一对 name=value 为必须项, 之后为可选属性(布尔型或键值型)
//   布尔属性: Secure, HttpOnly, Partitioned, SameParty
//   键值属性: Domain, Path, Expires, Max-Age, SameSite, Priority
XXAPI bool xrtSetCookieParseN(const char* sText, size_t iLen, xrtsetcookieview* pOut)
{
	size_t iCur;
	size_t iEnd;
	size_t iEq = (size_t)-1;
	if ( sText == NULL || pOut == NULL || iLen == 0u ) { return false; }
	// 算法步骤: 检查是否包含控制字符(禁止出现)
	if ( __xrtHttpUtilContainsCtl(sText, iLen) ) { return false; }
	// 算法步骤: 初始化输出结构
	memset(pOut, 0, sizeof(xrtsetcookieview));
	// 算法步骤: 跳过前导空白和分号
	iCur = 0u;
	while ( iCur < iLen && (sText[iCur] == ' ' || sText[iCur] == '\t' || sText[iCur] == ';') ) { iCur++; }
	if ( iCur >= iLen ) { return false; }
	// 算法步骤: 定位第一个 name=value 对的边界(到第一个分号)
	iEnd = iCur;
	while ( iEnd < iLen && sText[iEnd] != ';' ) {
		if ( sText[iEnd] == '=' && iEq == (size_t)-1 ) { iEq = iEnd; }
		iEnd++;
	}
	// 算法步骤: 校验 name=value 对必须有等号且 name 非空
	if ( iEq == (size_t)-1 || iEq == iCur ) { return false; }
	// 算法步骤: 提取并裁剪 name 和 value
	pOut->tName = xrtStrView(sText + iCur, iEq - iCur);
	pOut->tValue = xrtStrView(sText + iEq + 1u, iEnd - iEq - 1u);
	__xrtHttpUtilTrimView(&pOut->tName);
	__xrtHttpUtilTrimView(&pOut->tValue);
	if ( pOut->tName.iLen == 0u ) { return false; }
	// 算法步骤: 校验 name 必须为合法 HTTP Token 字符
	for ( iCur = 0u; iCur < pOut->tName.iLen; ++iCur ) {
		if ( !__xrtHttpUtilIsTokenChar(pOut->tName.sPtr[iCur]) ) { return false; }
	}
	// 算法步骤: 校验 value 必须为合法 Cookie Octet 字符
	for ( iCur = 0u; iCur < pOut->tValue.iLen; ++iCur ) {
		if ( !__xrtHttpUtilIsCookieOctet(pOut->tValue.sPtr[iCur]) ) { return false; }
	}
	pOut->iFlags |= XRT_SET_COOKIE_F_HAS_VALUE;
	// 算法步骤: 逐个解析后续属性(RFC 6265 §5.2)
	iCur = (iEnd < iLen) ? (iEnd + 1u) : iEnd;
	while ( iCur < iLen ) {
		xrtstrview tAttrName;
		xrtstrview tAttrValue;
		size_t iAttrEnd = iCur;
		size_t iAttrEq = (size_t)-1;
		// 步骤: 跳过属性间的空白和分号分隔符
		while ( iCur < iLen && (sText[iCur] == ' ' || sText[iCur] == '\t' || sText[iCur] == ';') ) { iCur++; }
		if ( iCur >= iLen ) { break; }
		// 步骤: 定位当前属性的边界
		iAttrEnd = iCur;
		while ( iAttrEnd < iLen && sText[iAttrEnd] != ';' ) {
			if ( sText[iAttrEnd] == '=' && iAttrEq == (size_t)-1 ) { iAttrEq = iAttrEnd; }
			iAttrEnd++;
		}
		// 步骤: 分支处理 - 无等号为布尔属性, 有等号为键值属性
		if ( iAttrEq == (size_t)-1 ) {
			// 步骤: 解析布尔属性(Secure, HttpOnly, Partitioned, SameParty)
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
			// 步骤: 提取并裁剪属性名和属性值
			tAttrName = xrtStrView(sText + iCur, iAttrEq - iCur);
			tAttrValue = xrtStrView(sText + iAttrEq + 1u, iAttrEnd - iAttrEq - 1u);
			__xrtHttpUtilTrimView(&tAttrName);
			__xrtHttpUtilTrimView(&tAttrValue);
			// 步骤: 按属性名分发处理
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
				// 步骤: 解析 Max-Age 为有符号 32 位整数
				if ( !__xrtHttpUtilParseInt32(tAttrValue.sPtr, tAttrValue.iLen, &iMaxAge) ) { return false; }
				pOut->iMaxAge = iMaxAge;
				pOut->iFlags |= XRT_SET_COOKIE_F_HAS_MAX_AGE;
			} else if ( __xrtHttpUtilEqNoCaseN(tAttrName.sPtr, tAttrName.iLen, "SameSite", 8u) ) {
				// 步骤: 解析 SameSite 枚举值 (Lax/Strict/None)
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
				// 步骤: 解析 Priority 枚举值 (Low/Medium/High)
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
		// 步骤: 移动到下一个属性
		iCur = (iAttrEnd < iLen) ? (iAttrEnd + 1u) : iAttrEnd;
	}
	return true;
}


// xrtSetCookieParse 相关处理
XXAPI bool xrtSetCookieParse(const char* sText, xrtsetcookieview* pOut)
{
	if ( sText == NULL ) { return false; }
	return xrtSetCookieParseN(sText, strlen(__xrt_cstr(sText)), pOut);
}


// 解析 set Cookie 行
XXAPI bool xrtSetCookieParseLineN(const char* sLine, size_t iLen, xrtsetcookieview* pOut)
{
	xrtheaderpair tHeader;
	if ( !xrtHttpHeaderSplitLineN(sLine, iLen, &tHeader) ) { return false; }
	if ( !__xrtHttpUtilEqNoCaseN(tHeader.tName.sPtr, tHeader.tName.iLen, "Set-Cookie", 10u) ) { return false; }
	return xrtSetCookieParseN(tHeader.tValue.sPtr, tHeader.tValue.iLen, pOut);
}


// 解析 set Cookie 行
XXAPI bool xrtSetCookieParseLine(const char* sLine, xrtsetcookieview* pOut)
{
	if ( sLine == NULL ) { return false; }
	return xrtSetCookieParseLineN(sLine, strlen(sLine), pOut);
}

static bool __xrtHttpUtilParseParamValue(xrtstrview tRaw, xrtstrview* pOut);


// 获取下一个 HTTP 参数
// 算法说明 - 迭代解析 HTTP 参数列表 (RFC 2616 attribute=value 对)
//   参数以 ';' 分隔, 格式为 "name[=value]; name[=value]; ..."
//   value 可以是裸值或双引号引用字符串
XXAPI bool xrtHttpParamNextN(const char* sText, size_t iLen, size_t* pOffset, xrthttpparam* pOut)
{
	size_t iCur;
	size_t iEnd;
	size_t iEq = (size_t)-1;
	xrtstrview tName;
	xrtstrview tValue;
	size_t i;
	if ( sText == NULL || pOffset == NULL || pOut == NULL ) { return false; }
	// 步骤: 跳过分隔符和空白
	iCur = *pOffset;
	while ( iCur < iLen && (sText[iCur] == ';' || sText[iCur] == ' ' || sText[iCur] == '\t') ) { iCur++; }
	if ( iCur >= iLen ) {
		*pOffset = iLen;
		return false;
	}
	// 算法步骤: 定位当前参数的边界(到下一个分号), 同时查找等号
	iEnd = iCur;
	while ( iEnd < iLen && sText[iEnd] != ';' ) {
		if ( sText[iEnd] == '=' && iEq == (size_t)-1 ) { iEq = iEnd; }
		iEnd++;
	}
	// 步骤: 根据是否有等号, 分别提取 name 或 name+value
	if ( iEq == (size_t)-1 ) {
		tName = xrtStrView(sText + iCur, iEnd - iCur);
		tValue = xrtStrView(NULL, 0u);
	} else {
		tName = xrtStrView(sText + iCur, iEq - iCur);
		tValue = xrtStrView(sText + iEq + 1u, iEnd - iEq - 1u);
	}
	// 步骤: 裁剪 name 两端空白
	__xrtHttpUtilTrimView(&tName);
	if ( xrtStrViewIsEmpty(tName) ) { return false; }
	// 步骤: 校验 name 为合法 Token 字符
	for ( i = 0u; i < tName.iLen; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(tName.sPtr[i]) ) { return false; }
	}
	// 算法步骤: 如果有 value, 解析 value (处理引号字符串)
	if ( iEq != (size_t)-1 ) {
		if ( !__xrtHttpUtilParseParamValue(tValue, &tValue) ) { return false; }
	}
	// 步骤: 填充输出结果
	pOut->iFlags = (iEq != (size_t)-1) ? XRT_HTTP_PARAM_F_HAS_VALUE : XRT_HTTP_PARAM_F_NONE;
	pOut->tName = tName;
	pOut->tValue = tValue;
	*pOffset = (iEnd < iLen) ? (iEnd + 1u) : iEnd;
	return true;
}


// 获取下一个 HTTP 参数
XXAPI bool xrtHttpParamNext(const char* sText, size_t* pOffset, xrthttpparam* pOut)
{
	if ( sText == NULL ) { return false; }
	return xrtHttpParamNextN(sText, strlen(__xrt_cstr(sText)), pOffset, pOut);
}


// 统计 HTTP 参数
XXAPI size_t xrtHttpParamCountN(const char* sText, size_t iLen)
{
	size_t iCount = 0u;
	size_t iOffset = 0u;
	xrthttpparam tParam;
	while ( xrtHttpParamNextN(sText, iLen, &iOffset, &tParam) ) { ++iCount; }
	return iCount;
}


// 统计 HTTP 参数
XXAPI size_t xrtHttpParamCount(const char* sText)
{
	if ( sText == NULL ) { return 0u; }
	return xrtHttpParamCountN(sText, strlen(__xrt_cstr(sText)));
}


// 查找 HTTP 参数
XXAPI bool xrtHttpParamFindN(const char* sText, size_t iLen, const char* sName, size_t iNameLen, xrthttpparam* pOut)
{
	size_t iOffset = 0u;
	xrthttpparam tParam;
	if ( pOut ) { memset(pOut, 0, sizeof(xrthttpparam)); }
	if ( sText == NULL || sName == NULL || iNameLen == 0u ) { return false; }
	while ( xrtHttpParamNextN(sText, iLen, &iOffset, &tParam) ) {
		if ( __xrtHttpUtilEqNoCaseN(tParam.tName.sPtr, tParam.tName.iLen, sName, iNameLen) ) {
			if ( pOut ) { *pOut = tParam; }
			return true;
		}
	}
	return false;
}


// 查找 HTTP 参数
XXAPI bool xrtHttpParamFind(const char* sText, const char* sName, xrthttpparam* pOut)
{
	if ( sText == NULL || sName == NULL ) { return false; }
	return xrtHttpParamFindN(sText, strlen(__xrt_cstr(sText)), sName, strlen(__xrt_cstr(sName)), pOut);
}


// 追加 HTTP 参数键值对
XXAPI bool xrtHttpParamAppendPairTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen, bool bHasValue, bool bQuoteValue)
{
	size_t i;
	if ( sOut == NULL || pOffset == NULL || sName == NULL || (sValue == NULL && iValueLen != 0u) ) { return false; }
	if ( iNameLen == 0u ) { return false; }
	for ( i = 0u; i < iNameLen; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(sName[i]) ) { return false; }
	}
	if ( *pOffset > 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "; ", 2u) ) { return false; }
	}
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sName, iNameLen) ) { return false; }
	if ( !bHasValue ) { return true; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "=", 1u) ) { return false; }
	if ( bQuoteValue ) { return __xrtHttpUtilAppendQuotedString(sOut, iOutCap, pOffset, sValue, iValueLen); }
	if ( __xrtHttpUtilContainsCtl(sValue, iValueLen) ) { return false; }
	return __xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sValue, iValueLen);
}


// 追加 HTTP 参数键值对
XXAPI bool xrtHttpParamAppendPair(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, const char* sValue, bool bHasValue, bool bQuoteValue)
{
	if ( sName == NULL ) { return false; }
	return xrtHttpParamAppendPairTo(sOut, iOutCap, pOffset, sName, strlen(__xrt_cstr(sName)), sValue, sValue ? strlen(sValue) : 0u, bHasValue, bQuoteValue);
}


// 解析 HTTP 媒体类型 (RFC 6838 / MIME Content-Type)
// 算法说明 - 解析 MIME 媒体类型字符串, 格式为 "type/subtype[+suffix][;params]"
//   例如: "text/html;charset=UTF-8", "application/vnd.api+json"
//   通过 '/' 分离 type 和 subtype, '+' 分离可选 suffix, ';' 分离参数
XXAPI bool xrtHttpMediaTypeParseN(const char* sText, size_t iLen, xrtmediatypeview* pOut)
{
	size_t iSemi = 0u;
	size_t iSlash = (size_t)-1;
	size_t iPlus = (size_t)-1;
	size_t i;
	xrtstrview tToken;
	xrtstrview tParams;
	if ( pOut == NULL ) { return false; }
	// 步骤: 初始化输出结构
	memset(pOut, 0, sizeof(xrtmediatypeview));
	if ( sText == NULL || iLen == 0u ) { return false; }
	// 算法步骤: 定位第一个分号, 分离媒体类型令牌和参数部分
	while ( iSemi < iLen && sText[iSemi] != ';' ) { iSemi++; }
	tToken = xrtStrView(sText, iSemi);
	__xrtHttpUtilTrimView(&tToken);
	if ( xrtStrViewIsEmpty(tToken) ) { return false; }
	// 算法步骤: 扫描令牌, 定位 '/' (type/subtype) 和 '+' (可选 suffix)
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
		if ( !__xrtHttpUtilIsTokenChar(ch) ) { return false; }
	}
	// 步骤: 校验 '/' 必须存在且前后都有内容
	if ( iSlash == (size_t)-1 || iSlash == 0u || iSlash + 1u >= tToken.iLen ) { return false; }
	// 步骤: 校验 '+' 如果存在不能紧跟在 '/' 之后
	if ( iPlus != (size_t)-1 && iPlus <= iSlash + 1u ) { return false; }
	// 步骤: 提取 type 部分
	pOut->tType = xrtStrView(tToken.sPtr, iSlash);
	// 步骤: 提取 subtype 和可选 suffix
	if ( iPlus == (size_t)-1 ) {
		pOut->tSubType = xrtStrView(tToken.sPtr + iSlash + 1u, tToken.iLen - iSlash - 1u);
	} else {
		if ( iPlus + 1u >= tToken.iLen ) { return false; }
		pOut->tSubType = xrtStrView(tToken.sPtr + iSlash + 1u, iPlus - iSlash - 1u);
		pOut->tSuffix = xrtStrView(tToken.sPtr + iPlus + 1u, tToken.iLen - iPlus - 1u);
		pOut->iFlags |= XRT_HTTP_MEDIA_TYPE_F_HAS_SUFFIX;
	}
	if ( xrtStrViewIsEmpty(pOut->tSubType) ) { return false; }
	// 步骤: 如果有分号, 提取参数部分视图
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


// xrtHttpMediaTypeParse 相关处理
XXAPI bool xrtHttpMediaTypeParse(const char* sText, xrtmediatypeview* pOut)
{
	if ( sText == NULL ) { return false; }
	return xrtHttpMediaTypeParseN(sText, strlen(__xrt_cstr(sText)), pOut);
}


// xrtHttpMediaTypeBuildTo 相关处理
XXAPI bool xrtHttpMediaTypeBuildTo(const xrtmediatypeview* pType, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( pType == NULL || sOut == NULL || iOutCap == 0u ) { return false; }
	sOut[0] = '\0';
	if ( xrtStrViewIsEmpty(pType->tType) || xrtStrViewIsEmpty(pType->tSubType) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pType->tType.sPtr, pType->tType.iLen) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "/", 1u) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pType->tSubType.sPtr, pType->tSubType.iLen) ) { return false; }
	if ( (pType->iFlags & XRT_HTTP_MEDIA_TYPE_F_HAS_SUFFIX) != 0u ) {
		if ( xrtStrViewIsEmpty(pType->tSuffix) ) { return false; }
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "+", 1u) ) { return false; }
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pType->tSuffix.sPtr, pType->tSuffix.iLen) ) { return false; }
	}
	if ( (pType->iFlags & XRT_HTTP_MEDIA_TYPE_F_HAS_PARAMS) != 0u ) {
		if ( __xrtHttpUtilContainsCtl(pType->tParams.sPtr, pType->tParams.iLen) ) { return false; }
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; ", 2u) ) { return false; }
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pType->tParams.sPtr, pType->tParams.iLen) ) { return false; }
	}
	if ( pOutLen ) { *pOutLen = iOff; }
	return true;
}


// xrtHttpMediaTypeBuild 相关处理
XXAPI bool xrtHttpMediaTypeBuild(const xrtmediatypeview* pType, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	return xrtHttpMediaTypeBuildTo(pType, sOut, iOutCap, pOutLen);
}


// xrtHttpMediaTypeFindParamN 相关处理
XXAPI bool xrtHttpMediaTypeFindParamN(const xrtmediatypeview* pType, const char* sName, size_t iNameLen, xrthttpparam* pOut)
{
	if ( pOut ) { memset(pOut, 0, sizeof(xrthttpparam)); }
	if ( pType == NULL || sName == NULL || iNameLen == 0u ) { return false; }
	if ( (pType->iFlags & XRT_HTTP_MEDIA_TYPE_F_HAS_PARAMS) == 0u || xrtStrViewIsEmpty(pType->tParams) ) { return false; }
	return xrtHttpParamFindN(pType->tParams.sPtr, pType->tParams.iLen, sName, iNameLen, pOut);
}


// xrtHttpMediaTypeFindParam 相关处理
XXAPI bool xrtHttpMediaTypeFindParam(const xrtmediatypeview* pType, const char* sName, xrthttpparam* pOut)
{
	if ( sName == NULL ) { return false; }
	return xrtHttpMediaTypeFindParamN(pType, sName, strlen(__xrt_cstr(sName)), pOut);
}


// 解析 HTTP Content-Disposition 头部 (RFC 6266)
// 算法说明 - 解析 Content-Disposition 头部, 格式为 "disposition-type; name=...; filename=...; filename*=..."
//   disposition-type 通常为 "form-data" 或 "attachment"
//   filename* 使用 RFC 5987 扩展值编码, 优先级高于 filename
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
	if ( pOut == NULL ) { return false; }
	// 步骤: 初始化输出结构
	memset(pOut, 0, sizeof(xrtcontentdispositionview));
	if ( sText == NULL || iLen == 0u ) { return false; }
	// 算法步骤: 定位分号, 分离 disposition-type 和参数部分
	while ( iSemi < iLen && sText[iSemi] != ';' ) { iSemi++; }
	tToken = xrtStrView(sText, iSemi);
	__xrtHttpUtilTrimView(&tToken);
	if ( xrtStrViewIsEmpty(tToken) ) { return false; }
	// 步骤: 校验 disposition-type 为合法 Token
	for ( i = 0u; i < tToken.iLen; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(tToken.sPtr[i]) ) { return false; }
	}
	pOut->tType = tToken;
	// 算法步骤: 如果有参数部分, 逐个解析并提取 name/filename/filename*
	if ( iSemi < iLen ) {
		tParams = xrtStrView(sText + iSemi + 1u, iLen - iSemi - 1u);
		__xrtHttpUtilTrimView(&tParams);
		if ( !xrtStrViewIsEmpty(tParams) ) {
			pOut->tParams = tParams;
			pOut->iFlags |= XRT_HTTP_CONTENT_DISPOSITION_F_HAS_PARAMS;
			while ( xrtHttpParamNextN(tParams.sPtr, tParams.iLen, &iParamOff, &tParam) ) {
				// 步骤: 提取 name 参数
				if ( __xrtHttpUtilEqNoCaseN(tParam.tName.sPtr, tParam.tName.iLen, "name", 4u) &&
				     (tParam.iFlags & XRT_HTTP_PARAM_F_HAS_VALUE) != 0u ) {
					pOut->tName = tParam.tValue;
					pOut->iFlags |= XRT_HTTP_CONTENT_DISPOSITION_F_HAS_NAME;
				// 步骤: 提取 filename 参数
				} else if ( __xrtHttpUtilEqNoCaseN(tParam.tName.sPtr, tParam.tName.iLen, "filename", 8u) &&
				            (tParam.iFlags & XRT_HTTP_PARAM_F_HAS_VALUE) != 0u ) {
					pOut->tFileName = tParam.tValue;
					pOut->iFlags |= XRT_HTTP_CONTENT_DISPOSITION_F_HAS_FILENAME;
				// 步骤: 提取 filename* 参数 (RFC 5987 扩展值, 含编码信息)
				} else if ( __xrtHttpUtilEqNoCaseN(tParam.tName.sPtr, tParam.tName.iLen, "filename*", 9u) &&
				            (tParam.iFlags & XRT_HTTP_PARAM_F_HAS_VALUE) != 0u ) {
					tExtValue = tParam.tValue;
					if ( !__xrtHttpUtilParseExtValueView(tExtValue, &tCharset, &tLanguage, &tEncoded) ) { return false; }
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


// xrtHttpContentDispositionParse 相关处理
XXAPI bool xrtHttpContentDispositionParse(const char* sText, xrtcontentdispositionview* pOut)
{
	if ( sText == NULL ) { return false; }
	return xrtHttpContentDispositionParseN(sText, strlen(__xrt_cstr(sText)), pOut);
}


// 解码 HTTP content disposition 文件名称
XXAPI bool xrtHttpContentDispositionDecodeFileNameTo(const xrtcontentdispositionview* pDisp, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iLen;
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( pDisp == NULL || sOut == NULL || iOutCap == 0u ) { return false; }
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
	if ( (pDisp->iFlags & XRT_HTTP_CONTENT_DISPOSITION_F_HAS_FILENAME) == 0u ) { return false; }
	iLen = pDisp->tFileName.iLen;
	if ( iLen + 1u > iOutCap ) { return false; }
	if ( iLen > 0u ) { memcpy(sOut, pDisp->tFileName.sPtr, iLen); }
	sOut[iLen] = '\0';
	if ( pOutLen ) { *pOutLen = iLen; }
	return true;
}


// 解码 HTTP content disposition 文件名称
XXAPI bool xrtHttpContentDispositionDecodeFileName(const xrtcontentdispositionview* pDisp, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	return xrtHttpContentDispositionDecodeFileNameTo(pDisp, sOut, iOutCap, pOutLen);
}


// xrtHttpContentDispositionBuildTo 相关处理
XXAPI bool xrtHttpContentDispositionBuildTo(const xrtcontentdispositionview* pDisp, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( pDisp == NULL || sOut == NULL || iOutCap == 0u ) { return false; }
	sOut[0] = '\0';
	if ( xrtStrViewIsEmpty(pDisp->tType) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pDisp->tType.sPtr, pDisp->tType.iLen) ) { return false; }
	if ( (pDisp->iFlags & XRT_HTTP_CONTENT_DISPOSITION_F_HAS_NAME) != 0u ) {
		if ( !xrtHttpParamAppendPairTo(sOut, iOutCap, &iOff, "name", 4u, pDisp->tName.sPtr, pDisp->tName.iLen, true, true) ) { return false; }
	}
	if ( (pDisp->iFlags & XRT_HTTP_CONTENT_DISPOSITION_F_HAS_FILENAME) != 0u ) {
		if ( !xrtHttpParamAppendPairTo(sOut, iOutCap, &iOff, "filename", 8u, pDisp->tFileName.sPtr, pDisp->tFileName.iLen, true, true) ) { return false; }
	}
	if ( (pDisp->iFlags & XRT_HTTP_CONTENT_DISPOSITION_F_HAS_FILENAME_EXT) != 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; filename*=", 12u) ) { return false; }
		/* tFileNameExt stores the raw RFC 5987 ext-value; preserve it verbatim when rebuilding. */
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pDisp->tFileNameExt.sPtr, pDisp->tFileNameExt.iLen) ) { return false; }
	}
	if ( pOutLen ) { *pOutLen = iOff; }
	return true;
}


// xrtHttpContentDispositionBuild 相关处理
XXAPI bool xrtHttpContentDispositionBuild(const xrtcontentdispositionview* pDisp, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	return xrtHttpContentDispositionBuildTo(pDisp, sOut, iOutCap, pOutLen);
}


// 追加 Cookie 键值对
XXAPI bool xrtCookieAppendPairTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen)
{
	size_t i;
	if ( sOut == NULL || pOffset == NULL || sName == NULL ) { return false; }
	if ( iNameLen == 0u ) { return false; }
	for ( i = 0u; i < iNameLen; ++i ) {
		if ( !__xrtHttpUtilIsTokenChar(sName[i]) ) { return false; }
	}
	for ( i = 0u; i < iValueLen; ++i ) {
		if ( !__xrtHttpUtilIsCookieOctet(sValue[i]) ) { return false; }
	}
	if ( *pOffset > 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "; ", 2u) ) { return false; }
	}
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sName, iNameLen) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "=", 1u) ) { return false; }
	return __xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sValue, iValueLen);
}


// 追加 Cookie 键值对
XXAPI bool xrtCookieAppendPair(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, const char* sValue)
{
	if ( sName == NULL || sValue == NULL ) { return false; }
	return xrtCookieAppendPairTo(sOut, iOutCap, pOffset, sName, strlen(__xrt_cstr(sName)), sValue, strlen(sValue));
}


// 构建 Cookie
XXAPI bool xrtCookieBuildTo(const xrtcookiepair* pPairs, size_t iCount, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	size_t i;
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( sOut == NULL || iOutCap == 0u || (pPairs == NULL && iCount != 0u) ) { return false; }
	sOut[0] = '\0';
	for ( i = 0u; i < iCount; ++i ) {
		if ( !xrtCookieAppendPairTo(sOut, iOutCap, &iOff, pPairs[i].tName.sPtr, pPairs[i].tName.iLen, pPairs[i].tValue.sPtr, pPairs[i].tValue.iLen) ) { return false; }
	}
	if ( pOutLen ) { *pOutLen = iOff; }
	return true;
}


// 追加查询键值对
XXAPI bool xrtQueryAppendPairTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sKey, size_t iKeyLen, const char* sValue, size_t iValueLen, bool bHasValue, bool bPlusAsSpace)
{
	size_t iWritten = 0u;
	if ( sOut == NULL || pOffset == NULL || sKey == NULL ) { return false; }
	if ( *pOffset > 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "&", 1u) ) { return false; }
	}
	if ( !xrtPercentEncodeTo(sKey, iKeyLen, sOut + *pOffset, iOutCap - *pOffset, &iWritten, bPlusAsSpace) ) { return false; }
	*pOffset += iWritten;
	if ( bHasValue ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "=", 1u) ) { return false; }
		if ( !xrtPercentEncodeTo(sValue, iValueLen, sOut + *pOffset, iOutCap - *pOffset, &iWritten, bPlusAsSpace) ) { return false; }
		*pOffset += iWritten;
	}
	return true;
}


// 追加查询键值对
XXAPI bool xrtQueryAppendPair(char* sOut, size_t iOutCap, size_t* pOffset, const char* sKey, const char* sValue)
{
	if ( sKey == NULL ) { return false; }
	return xrtQueryAppendPairTo(sOut, iOutCap, pOffset, sKey, strlen(sKey), sValue, sValue ? strlen(sValue) : 0u, sValue != NULL, false);
}


// 构建查询
XXAPI bool xrtQueryBuildTo(const xrtquerypair* pPairs, size_t iCount, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	size_t i;
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( sOut == NULL || iOutCap == 0u || (pPairs == NULL && iCount != 0u) ) { return false; }
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
	if ( pOutLen ) { *pOutLen = iOff; }
	return true;
}


// 获取下一个表单 URL 编码
XXAPI bool xrtFormUrlEncodedNextN(const char* sText, size_t iLen, size_t* pOffset, xrtquerypair* pOut)
{
	return xrtQueryNextN(sText, iLen, pOffset, pOut);
}


// 获取下一个表单 URL 编码
XXAPI bool xrtFormUrlEncodedNext(const char* sText, size_t* pOffset, xrtquerypair* pOut)
{
	return xrtQueryNext(sText, pOffset, pOut);
}


// 解析表单 URL 编码
XXAPI bool xrtFormUrlEncodedParseToN(const char* sText, size_t iLen, xrtquerypair* pOut, size_t iCap, size_t* pCount)
{
	return xrtQueryParseToN(sText, iLen, pOut, iCap, pCount);
}


// 解析表单 URL 编码
XXAPI bool xrtFormUrlEncodedParseTo(const char* sText, xrtquerypair* pOut, size_t iCap, size_t* pCount)
{
	if ( sText == NULL ) { return false; }
	return xrtFormUrlEncodedParseToN(sText, strlen(__xrt_cstr(sText)), pOut, iCap, pCount);
}


// 解码表单 URL 编码
XXAPI bool xrtFormUrlEncodedDecodeTo(const char* sText, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	return xrtPercentDecodeTo(sText, iLen, sOut, iOutCap, pOutLen, true);
}


// 追加表单 URL 编码 field
XXAPI bool xrtFormUrlEncodedAppendFieldTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen, bool bHasValue)
{
	return xrtQueryAppendPairTo(sOut, iOutCap, pOffset, sName, iNameLen, sValue, iValueLen, bHasValue, true);
}


// 追加表单 URL 编码 field
XXAPI bool xrtFormUrlEncodedAppendField(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, const char* sValue)
{
	if ( sName == NULL ) { return false; }
	return xrtFormUrlEncodedAppendFieldTo(sOut, iOutCap, pOffset, sName, strlen(__xrt_cstr(sName)), sValue, sValue ? strlen(sValue) : 0u, sValue != NULL);
}


// 构建表单 URL 编码
XXAPI bool xrtFormUrlEncodedBuildTo(const xrtquerypair* pPairs, size_t iCount, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	size_t i;
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( sOut == NULL || iOutCap == 0u || (pPairs == NULL && iCount != 0u) ) { return false; }
	sOut[0] = '\0';
	for ( i = 0u; i < iCount; ++i ) {
		if ( !xrtFormUrlEncodedAppendFieldTo(sOut, iOutCap, &iOff,
			pPairs[i].tKey.sPtr,
			pPairs[i].tKey.iLen,
			pPairs[i].tValue.sPtr,
			pPairs[i].tValue.iLen,
			(pPairs[i].iFlags & XRT_QUERY_F_HAS_VALUE) != 0u) ) return false;
	}
	if ( pOutLen ) { *pOutLen = iOff; }
	return true;
}


// 构建 Set-Cookie 值 (RFC 6265 §4.1.1)
// 算法说明 - 按规范顺序拼接 Set-Cookie 值: name=value; 属性列表
//   属性按 Domain/Path/Expires/Max-Age/SameSite/Secure/HttpOnly/Partitioned/SameParty/Priority 顺序输出
XXAPI bool xrtSetCookieBuildTo(const xrtsetcookieview* pCookie, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( sOut == NULL || iOutCap == 0u || pCookie == NULL ) { return false; }
	sOut[0] = '\0';
	// 步骤: 写入必须的 name=value
	if ( xrtStrViewIsEmpty(pCookie->tName) ) { return false; }
	if ( !xrtCookieAppendPairTo(sOut, iOutCap, &iOff, pCookie->tName.sPtr, pCookie->tName.iLen, pCookie->tValue.sPtr, pCookie->tValue.iLen) ) { return false; }
	// 步骤: 写入 Domain 属性
	if ( (pCookie->iFlags & XRT_SET_COOKIE_F_HAS_DOMAIN) != 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; Domain=", 9u) ) { return false; }
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pCookie->tDomain.sPtr, pCookie->tDomain.iLen) ) { return false; }
	}
	// 步骤: 写入 Path 属性
	if ( (pCookie->iFlags & XRT_SET_COOKIE_F_HAS_PATH) != 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; Path=", 7u) ) { return false; }
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pCookie->tPath.sPtr, pCookie->tPath.iLen) ) { return false; }
	}
	// 步骤: 写入 Expires 属性
	if ( (pCookie->iFlags & XRT_SET_COOKIE_F_HAS_EXPIRES) != 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; Expires=", 10u) ) { return false; }
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, pCookie->tExpires.sPtr, pCookie->tExpires.iLen) ) { return false; }
	}
	// 步骤: 写入 Max-Age 属性(整数转字符串)
	if ( (pCookie->iFlags & XRT_SET_COOKIE_F_HAS_MAX_AGE) != 0u ) {
		char sNum[32];
		int iLen = snprintf(sNum, sizeof(sNum), "%d", (int)pCookie->iMaxAge);
		if ( iLen <= 0 || (size_t)iLen >= sizeof(sNum) ) { return false; }
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; Max-Age=", 11u) ) { return false; }
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, sNum, (size_t)iLen) ) { return false; }
	}
	// 步骤: 写入 SameSite 属性(Lax/Strict/None)
	if ( (pCookie->iFlags & XRT_SET_COOKIE_F_HAS_SAME_SITE) != 0u ) {
		const char* sSameSite = NULL;
		if ( pCookie->iSameSite == XRT_SAME_SITE_LAX ) { sSameSite = "Lax"; }
		else if ( pCookie->iSameSite == XRT_SAME_SITE_STRICT ) { sSameSite = "Strict"; }
		else if ( pCookie->iSameSite == XRT_SAME_SITE_NONE ) { sSameSite = "None"; }
		else { return false; }
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; SameSite=", 12u) ) { return false; }
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, sSameSite, strlen(sSameSite)) ) { return false; }
	}
	// 步骤: 写入布尔属性 (Secure, HttpOnly, Partitioned, SameParty)
	if ( (pCookie->iFlags & XRT_SET_COOKIE_F_SECURE) != 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; Secure", 8u) ) { return false; }
	}
	if ( (pCookie->iFlags & XRT_SET_COOKIE_F_HTTP_ONLY) != 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; HttpOnly", 10u) ) { return false; }
	}
	if ( (pCookie->iFlags & XRT_SET_COOKIE_F_PARTITIONED) != 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; Partitioned", 13u) ) { return false; }
	}
	if ( (pCookie->iFlags & XRT_SET_COOKIE_F_SAME_PARTY) != 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; SameParty", 11u) ) { return false; }
	}
	// 步骤: 写入 Priority 属性(Low/Medium/High)
	if ( (pCookie->iFlags & XRT_SET_COOKIE_F_HAS_PRIORITY) != 0u ) {
		const char* sPriority = NULL;
		if ( pCookie->iPriority == XRT_COOKIE_PRIORITY_LOW ) { sPriority = "Low"; }
		else if ( pCookie->iPriority == XRT_COOKIE_PRIORITY_MEDIUM ) { sPriority = "Medium"; }
		else if ( pCookie->iPriority == XRT_COOKIE_PRIORITY_HIGH ) { sPriority = "High"; }
		else { return false; }
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "; Priority=", 11u) ) { return false; }
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, sPriority, strlen(sPriority)) ) { return false; }
	}
	if ( pOutLen ) { *pOutLen = iOff; }
	return true;
}


// 构建 set Cookie 行
XXAPI bool xrtSetCookieBuildLineTo(const xrtsetcookieview* pCookie, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	size_t iLineLen = 0u;
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( sOut == NULL || iOutCap == 0u || pCookie == NULL ) { return false; }
	sOut[0] = '\0';
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "Set-Cookie: ", 12u) ) { return false; }
	if ( !xrtSetCookieBuildTo(pCookie, sOut + iOff, iOutCap - iOff, &iLineLen) ) { return false; }
	iOff += iLineLen;
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "\r\n", 2u) ) { return false; }
	if ( pOutLen ) { *pOutLen = iOff; }
	return true;
}


// 构建 set Cookie 行
XXAPI bool xrtSetCookieBuildLine(const xrtsetcookieview* pCookie, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	return xrtSetCookieBuildLineTo(pCookie, sOut, iOutCap, pOutLen);
}


// 内部函数：解析 HTTP util 参数值
static bool __xrtHttpUtilParseParamValue(xrtstrview tRaw, xrtstrview* pOut)
{
	if ( pOut == NULL ) { return false; }
	*pOut = tRaw;
	__xrtHttpUtilTrimView(pOut);
	if ( pOut->iLen >= 2u && pOut->sPtr[0] == '"' && pOut->sPtr[pOut->iLen - 1u] == '"' ) {
		pOut->sPtr++;
		pOut->iLen -= 2u;
	}
	return !xrtStrViewIsEmpty(*pOut);
}


// 内部函数：查找 HTTP util 参数
static bool __xrtHttpUtilFindParamN(xrtstrview tValue, const char* sName, xrtstrview* pOut)
{
	xrthttpparam tParam;
	if ( pOut ) { *pOut = xrtStrView(NULL, 0u); }
	if ( sName == NULL ) { return false; }
	if ( !xrtHttpParamFindN(tValue.sPtr, tValue.iLen, sName, strlen(__xrt_cstr(sName)), &tParam) ) { return false; }
	if ( (tParam.iFlags & XRT_HTTP_PARAM_F_HAS_VALUE) == 0u ) { return false; }
	if ( pOut ) { *pOut = tParam.tValue; }
	return true;
}


// xrtMultipartBoundaryFromContentTypeN 相关处理
XXAPI bool xrtMultipartBoundaryFromContentTypeN(const char* sValue, size_t iLen, xrtstrview* pOut)
{
	xrtmediatypeview tMediaType;
	xrtstrview tBoundary;
	size_t i;
	if ( sValue == NULL || pOut == NULL || iLen == 0u ) { return false; }
	if ( !xrtHttpMediaTypeParseN(sValue, iLen, &tMediaType) ) { return false; }
	if ( !__xrtHttpUtilEqNoCaseN(tMediaType.tType.sPtr, tMediaType.tType.iLen, "multipart", 9u) ) { return false; }
	if ( !__xrtHttpUtilEqNoCaseN(tMediaType.tSubType.sPtr, tMediaType.tSubType.iLen, "form-data", 9u) &&
	     !__xrtHttpUtilEqNoCaseN(tMediaType.tSubType.sPtr, tMediaType.tSubType.iLen, "mixed", 5u) ) return false;
	if ( (tMediaType.iFlags & XRT_HTTP_MEDIA_TYPE_F_HAS_PARAMS) == 0u ) { return false; }
	if ( !__xrtHttpUtilFindParamN(tMediaType.tParams, "boundary", &tBoundary) ) { return false; }
	if ( xrtStrViewIsEmpty(tBoundary) || tBoundary.iLen > 70u ) { return false; }
	for ( i = 0u; i < tBoundary.iLen; ++i ) {
		unsigned char ch = (unsigned char)tBoundary.sPtr[i];
		if ( ch < 0x20u || ch == 0x7Fu ) { return false; }
	}
	*pOut = tBoundary;
	return true;
}


// xrtMultipartBoundaryFromContentType 相关处理
XXAPI bool xrtMultipartBoundaryFromContentType(const char* sValue, xrtstrview* pOut)
{
	if ( sValue == NULL ) { return false; }
	return xrtMultipartBoundaryFromContentTypeN(sValue, strlen(sValue), pOut);
}


// xrtMultipartBuildContentTypeTo 相关处理
XXAPI bool xrtMultipartBuildContentTypeTo(const char* sBoundary, size_t iBoundaryLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( sOut == NULL || iOutCap == 0u ) { return false; }
	if ( !__xrtHttpUtilValidateBoundaryN(sBoundary, iBoundaryLen) ) { return false; }
	sOut[0] = '\0';
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, "multipart/form-data; boundary=", 30u) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, &iOff, sBoundary, iBoundaryLen) ) { return false; }
	if ( pOutLen ) { *pOutLen = iOff; }
	return true;
}


// xrtMultipartBuildContentType 相关处理
XXAPI bool xrtMultipartBuildContentType(const char* sBoundary, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( sBoundary == NULL ) { return false; }
	return xrtMultipartBuildContentTypeTo(sBoundary, strlen(sBoundary), sOut, iOutCap, pOutLen);
}


// 内部函数：匹配 HTTP util 边界
static bool __xrtHttpUtilMatchBoundaryAt(const char* sBody, size_t iLen, size_t iPos, const char* sBoundary, size_t iBoundaryLen, bool* pFinal, size_t* pAfter)
{
	if ( pFinal ) { *pFinal = false; }
	if ( pAfter ) { *pAfter = 0u; }
	if ( sBody == NULL || sBoundary == NULL ) { return false; }
	if ( iPos + 2u + iBoundaryLen > iLen ) { return false; }
	if ( sBody[iPos] != '-' || sBody[iPos + 1u] != '-' ) { return false; }
	if ( memcmp(sBody + iPos + 2u, sBoundary, iBoundaryLen) != 0 ) { return false; }
	iPos += 2u + iBoundaryLen;
	if ( iPos + 1u < iLen && sBody[iPos] == '-' && sBody[iPos + 1u] == '-' ) {
		if ( pFinal ) { *pFinal = true; }
		if ( pAfter ) { *pAfter = iPos + 2u; }
		return true;
	}
	if ( iPos + 1u < iLen && sBody[iPos] == '\r' && sBody[iPos + 1u] == '\n' ) {
		if ( pAfter ) { *pAfter = iPos + 2u; }
		return true;
	}
	return false;
}


// 内部函数：查找 HTTP util 边界行
static bool __xrtHttpUtilFindBoundaryLine(const char* sBody, size_t iLen, size_t iStart, const char* sBoundary, size_t iBoundaryLen, size_t* pPos, bool* pFinal, size_t* pAfter)
{
	size_t i;
	if ( pPos ) { *pPos = 0u; }
	if ( sBody == NULL || sBoundary == NULL ) { return false; }
	for ( i = iStart; i < iLen; ++i ) {
		bool bFinal = false;
		size_t iAfter = 0u;
		bool bLineStart = (i == 0u) || (i >= 2u && sBody[i - 2u] == '\r' && sBody[i - 1u] == '\n');
		if ( !bLineStart ) { continue; }
		if ( !__xrtHttpUtilMatchBoundaryAt(sBody, iLen, i, sBoundary, iBoundaryLen, &bFinal, &iAfter) ) { continue; }
		if ( pPos ) { *pPos = i; }
		if ( pFinal ) { *pFinal = bFinal; }
		if ( pAfter ) { *pAfter = iAfter; }
		return true;
	}
	return false;
}


// 内部函数：__xrtHttpUtilMultipartParseContentDisposition
static bool __xrtHttpUtilMultipartParseContentDisposition(xrtstrview tValue, xrtmultipartpartview* pOut)
{
	xrtcontentdispositionview tDisp;
	if ( pOut == NULL ) { return false; }
	if ( !xrtHttpContentDispositionParseN(tValue.sPtr, tValue.iLen, &tDisp) ) { return false; }
	if ( !__xrtHttpUtilEqNoCaseN(tDisp.tType.sPtr, tDisp.tType.iLen, "form-data", 9u) ) { return false; }
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


// 获取下一个 Multipart Part
// 算法说明 - Multipart 整体解析, 按 RFC 2046 格式定位每个 Part
//   格式: --boundary\r\n<headers>\r\n\r\n<body>\r\n--boundary(--) ...
//   先定位边界行, 再查找头部与 body 的分隔符(\r\n\r\n), 最后定位下一个边界
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
	if ( sBody == NULL || sBoundary == NULL || pOffset == NULL || pOut == NULL || iBoundaryLen == 0u ) { return false; }
	// 算法步骤: 从当前偏移位置查找下一个边界行
	if ( !__xrtHttpUtilFindBoundaryLine(sBody, iLen, *pOffset, sBoundary, iBoundaryLen, &iPos, &bFinal, &iAfterBoundary) ) {
		*pOffset = iLen;
		return false;
	}
	// 步骤: 结束边界表示 Multipart 数据结束
	if ( bFinal ) {
		*pOffset = iLen;
		return false;
	}
	if ( iAfterBoundary > iLen ) { return false; }
	// 步骤: 初始化输出结构
	memset(pOut, 0, sizeof(xrtmultipartpartview));
	// 算法步骤: 查找头部结束标记 \r\n\r\n (头部与 body 的分隔)
	iHeaderEnd = iAfterBoundary;
	while ( iHeaderEnd + 3u < iLen ) {
		if ( sBody[iHeaderEnd] == '\r' && sBody[iHeaderEnd + 1u] == '\n' && sBody[iHeaderEnd + 2u] == '\r' && sBody[iHeaderEnd + 3u] == '\n' ) { break; }
		iHeaderEnd++;
	}
	if ( iHeaderEnd + 3u >= iLen ) { return false; }
	// 步骤: 提取头部区域视图
	pOut->tHeaders = xrtStrView(sBody + iAfterBoundary, iHeaderEnd - iAfterBoundary);
	// 算法步骤: 查找 body 之后的下一个边界行, 确定 body 长度
	iBodyEnd = iHeaderEnd + 4u;
	if ( !__xrtHttpUtilFindBoundaryLine(sBody, iLen, iBodyEnd, sBoundary, iBoundaryLen, &iNextPos, &bNextFinal, &iUnusedAfter) ) { return false; }
	// 步骤: 边界前应有 \r\n 前缀, 去掉后得到纯 body 数据
	if ( iNextPos < 2u || sBody[iNextPos - 2u] != '\r' || sBody[iNextPos - 1u] != '\n' ) { return false; }
	pOut->tBody = xrtStrView(sBody + iBodyEnd, iNextPos - iBodyEnd - 2u);
	// 算法步骤: 逐行解析头部, 提取 Content-Disposition / Content-Type / Content-Transfer-Encoding
	while ( xrtHttpHeaderNextLineN(pOut->tHeaders.sPtr, pOut->tHeaders.iLen, &iHeaderOff, &tHeader) ) {
		if ( __xrtHttpUtilEqNoCaseN(tHeader.tName.sPtr, tHeader.tName.iLen, "Content-Disposition", 19u) ) {
			if ( !__xrtHttpUtilMultipartParseContentDisposition(tHeader.tValue, pOut) ) { return false; }
		} else if ( __xrtHttpUtilEqNoCaseN(tHeader.tName.sPtr, tHeader.tName.iLen, "Content-Type", 12u) ) {
			pOut->tContentType = tHeader.tValue;
			pOut->iFlags |= XRT_MULTIPART_F_HAS_CONTENT_TYPE;
		} else if ( __xrtHttpUtilEqNoCaseN(tHeader.tName.sPtr, tHeader.tName.iLen, "Content-Transfer-Encoding", 25u) ) {
			pOut->tTransferEncoding = tHeader.tValue;
			pOut->iFlags |= XRT_MULTIPART_F_HAS_TRANSFER_ENCODING;
		}
	}
	// 步骤: 更新偏移到下一个边界位置
	*pOffset = iNextPos;
	(void)bNextFinal;
	return true;
}


// 获取下一个 Multipart
XXAPI bool xrtMultipartNext(const char* sBody, const char* sBoundary, size_t* pOffset, xrtmultipartpartview* pOut)
{
	if ( sBody == NULL || sBoundary == NULL ) { return false; }
	return xrtMultipartNextN(sBody, strlen(sBody), sBoundary, strlen(sBoundary), pOffset, pOut);
}


// 解析 Multipart
XXAPI bool xrtMultipartParseToN(const char* sBody, size_t iLen, const char* sBoundary, size_t iBoundaryLen, xrtmultipartpartview* pOut, size_t iCap, size_t* pCount)
{
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtmultipartpartview tPart;
	if ( pCount ) { *pCount = 0u; }
	if ( sBody == NULL || sBoundary == NULL || (pOut == NULL && iCap != 0u) ) { return false; }
	while ( xrtMultipartNextN(sBody, iLen, sBoundary, iBoundaryLen, &iOffset, &tPart) ) {
		if ( iCount >= iCap ) { return false; }
		if ( pOut ) { pOut[iCount] = tPart; }
		++iCount;
	}
	if ( pCount ) { *pCount = iCount; }
	return true;
}


// 解析 Multipart
XXAPI bool xrtMultipartParseTo(const char* sBody, const char* sBoundary, xrtmultipartpartview* pOut, size_t iCap, size_t* pCount)
{
	if ( sBody == NULL || sBoundary == NULL ) { return false; }
	return xrtMultipartParseToN(sBody, strlen(sBody), sBoundary, strlen(sBoundary), pOut, iCap, pCount);
}


// 解码 Multipart 文件名称
XXAPI bool xrtMultipartDecodeFileNameTo(const xrtmultipartpartview* pPart, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iLen;
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( pPart == NULL || sOut == NULL || iOutCap == 0u ) { return false; }
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
	if ( (pPart->iFlags & XRT_MULTIPART_F_HAS_FILENAME) == 0u ) { return false; }
	iLen = pPart->tFileName.iLen;
	if ( iLen + 1u > iOutCap ) { return false; }
	if ( iLen > 0u ) { memcpy(sOut, pPart->tFileName.sPtr, iLen); }
	sOut[iLen] = '\0';
	if ( pOutLen ) { *pOutLen = iLen; }
	return true;
}


// 解码 Multipart 文件名称
XXAPI bool xrtMultipartDecodeFileName(const xrtmultipartpartview* pPart, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	return xrtMultipartDecodeFileNameTo(pPart, sOut, iOutCap, pOutLen);
}


// 初始化 Multipart 流配置
XXAPI void xrtMultipartStreamConfigInit(xrtmultipartstreamconfig* pConfig)
{
	if ( !pConfig ) { return; }
	pConfig->iMaxBufferedBytes = 256u * 1024u;
	pConfig->iMaxHeaderBytes = 64u * 1024u;
	pConfig->iMaxPartHeaders = 64u;
	pConfig->iTailReserve = 0u;
}


// 内部函数：__xrtHttpUtilMultipartStreamZeroEvent
static void __xrtHttpUtilMultipartStreamZeroEvent(xrtmultipartstreamevent* pEvent)
{
	if ( !pEvent ) { return; }
	memset(pEvent, 0, sizeof(xrtmultipartstreamevent));
	pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_NEED_MORE;
}


// 内部函数：__xrtHttpUtilMultipartStreamCompact
static void __xrtHttpUtilMultipartStreamCompact(xrtmultipartstream* pStream)
{
	if ( !pStream || pStream->iCursor == 0u ) { return; }
	if ( pStream->iCursor >= pStream->iBufferLen ) {
		pStream->iCursor = 0u;
		pStream->iBufferLen = 0u;
		return;
	}
	memmove(pStream->pBuffer, pStream->pBuffer + pStream->iCursor, pStream->iBufferLen - pStream->iCursor);
	pStream->iBufferLen -= pStream->iCursor;
	pStream->iCursor = 0u;
}


// 内部函数：确保 HTTP util Multipart 流 cap
static bool __xrtHttpUtilMultipartStreamEnsureCap(xrtmultipartstream* pStream, size_t iNeed)
{
	size_t iNewCap;
	char* pNewBuf;
	if ( !pStream ) { return false; }
	if ( iNeed <= pStream->iBufferCap ) { return true; }
	if ( iNeed > pStream->iMaxBufferedBytes ) { return false; }
	iNewCap = pStream->iBufferCap ? pStream->iBufferCap : 1024u;
	while ( iNewCap < iNeed ) {
		size_t iGrow = iNewCap < 65536u ? iNewCap : 65536u;
		if ( iNewCap > ((size_t)-1) - iGrow ) { return false; }
		iNewCap += iGrow;
	}
	if ( iNewCap > pStream->iMaxBufferedBytes ) { iNewCap = pStream->iMaxBufferedBytes; }
	if ( iNewCap < iNeed ) { return false; }
	pNewBuf = (char*)XHTTP_UTIL_REALLOC(pStream->pBuffer, iNewCap);
	if ( !pNewBuf ) { return false; }
	pStream->pBuffer = pNewBuf;
	pStream->iBufferCap = iNewCap;
	return true;
}


// 初始化 Multipart 流
// 算法说明 - 初始化流式 Multipart 解析器, 配置缓冲区大小限制和边界字符串
//   自动计算尾部保留空间(边界长度 + 8), 确保缓冲区能容纳至少一个头部块加尾部保留
XXAPI bool xrtMultipartStreamInit(xrtmultipartstream* pStream, const char* sBoundary, size_t iBoundaryLen, const xrtmultipartstreamconfig* pConfig)
{
	xrtmultipartstreamconfig tConfig;
	if ( !pStream ) { return false; }
	// 步骤: 初始化流结构并加载默认配置
	memset(pStream, 0, sizeof(xrtmultipartstream));
	xrtMultipartStreamConfigInit(&tConfig);
	if ( pConfig ) { tConfig = *pConfig; }
	// 步骤: 校验边界字符串合法性
	if ( !__xrtHttpUtilValidateBoundaryN(sBoundary, iBoundaryLen) ) {
		pStream->iError = XRT_MULTIPART_STREAM_ERR_INVALID_BOUNDARY;
		pStream->iState = XRT_MULTIPART_STREAM_STATE_ERROR;
		return false;
	}
	// 步骤: 填充配置项默认值
	if ( tConfig.iMaxBufferedBytes == 0u ) { tConfig.iMaxBufferedBytes = 256u * 1024u; }
	if ( tConfig.iMaxHeaderBytes == 0u ) { tConfig.iMaxHeaderBytes = 64u * 1024u; }
	if ( tConfig.iMaxPartHeaders == 0u ) { tConfig.iMaxPartHeaders = 64u; }
	// 步骤: 计算尾部保留空间(用于防止在边界分割处丢失数据)
	if ( tConfig.iTailReserve == 0u ) { tConfig.iTailReserve = iBoundaryLen + 8u; }
	if ( tConfig.iTailReserve < iBoundaryLen + 8u ) { tConfig.iTailReserve = iBoundaryLen + 8u; }
	// 步骤: 确保缓冲区最小容量
	if ( tConfig.iMaxBufferedBytes < tConfig.iMaxHeaderBytes + tConfig.iTailReserve + 4u ) {
		tConfig.iMaxBufferedBytes = tConfig.iMaxHeaderBytes + tConfig.iTailReserve + 4u;
	}
	// 步骤: 保存边界字符串和配置到流结构
	memcpy(pStream->aBoundary, sBoundary, iBoundaryLen);
	pStream->aBoundary[iBoundaryLen] = '\0';
	pStream->iBoundaryLen = iBoundaryLen;
	pStream->iMaxBufferedBytes = tConfig.iMaxBufferedBytes;
	pStream->iMaxHeaderBytes = tConfig.iMaxHeaderBytes;
	pStream->iMaxPartHeaders = tConfig.iMaxPartHeaders;
	pStream->iTailReserve = tConfig.iTailReserve;
	// 步骤: 设置初始状态为查找边界
	pStream->iState = XRT_MULTIPART_STREAM_STATE_SEEK_BOUNDARY;
	return true;
}


// 释放 Multipart 流
XXAPI void xrtMultipartStreamUnit(xrtmultipartstream* pStream)
{
	if ( !pStream ) { return; }
	if ( pStream->pBuffer ) {
		XHTTP_UTIL_FREE(pStream->pBuffer);
		pStream->pBuffer = NULL;
	}
	memset(pStream, 0, sizeof(xrtmultipartstream));
}


// 重置 Multipart 流
XXAPI void xrtMultipartStreamReset(xrtmultipartstream* pStream)
{
	if ( !pStream ) { return; }
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


// xrtMultipartStreamFeed 相关处理
XXAPI bool xrtMultipartStreamFeed(xrtmultipartstream* pStream, const void* pData, size_t iLen)
{
	if ( !pStream || pStream->iState == XRT_MULTIPART_STREAM_STATE_ERROR || pStream->iState == XRT_MULTIPART_STREAM_STATE_DONE ) { return false; }
	if ( pStream->bFinishedInput ) { return false; }
	if ( iLen == 0u ) { return true; }
	if ( pData == NULL ) { return false; }
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


// 完成 Multipart 流
XXAPI void xrtMultipartStreamFinish(xrtmultipartstream* pStream)
{
	if ( !pStream ) { return; }
	pStream->bFinishedInput = true;
}


// 获取 Multipart 流错误
XXAPI uint32 xrtMultipartStreamError(const xrtmultipartstream* pStream)
{
	return pStream ? pStream->iError : XRT_MULTIPART_STREAM_ERR_NONE;
}


// 内部函数：从流缓冲区中解析当前 Part 的头部块
// 算法说明 - 在流式缓冲区中查找 \r\n\r\n 分隔符定位头部区域,
//   然后逐行解析头部字段, 提取 Content-Disposition/Content-Type/Content-Transfer-Encoding
static bool __xrtHttpUtilMultipartStreamParseHeaders(xrtmultipartstream* pStream, xrtmultipartpartview* pOut)
{
	size_t iHeaderEnd;
	size_t iHeaderOff = 0u;
	size_t iHeaderCount = 0u;
	xrtheaderpair tHeader;
	if ( !pStream || !pOut ) { return false; }
	// 算法步骤: 从游标位置开始查找 \r\n\r\n (头部结束标记)
	iHeaderEnd = pStream->iCursor;
	while ( iHeaderEnd + 3u < pStream->iBufferLen ) {
		if ( pStream->pBuffer[iHeaderEnd] == '\r' &&
		     pStream->pBuffer[iHeaderEnd + 1u] == '\n' &&
		     pStream->pBuffer[iHeaderEnd + 2u] == '\r' &&
		     pStream->pBuffer[iHeaderEnd + 3u] == '\n' ) {
			break;
		}
		iHeaderEnd++;
		// 步骤: 检查头部大小是否超出限制
		if ( iHeaderEnd - pStream->iCursor > pStream->iMaxHeaderBytes ) {
			pStream->iError = XRT_MULTIPART_STREAM_ERR_HEADER_LIMIT;
			pStream->iState = XRT_MULTIPART_STREAM_STATE_ERROR;
			return false;
		}
	}
	// 步骤: 未找到分隔符, 检查是否需要更多数据或报错
	if ( iHeaderEnd + 3u >= pStream->iBufferLen ) {
		if ( pStream->bFinishedInput ) {
			pStream->iError = XRT_MULTIPART_STREAM_ERR_TRUNCATED;
			pStream->iState = XRT_MULTIPART_STREAM_STATE_ERROR;
			return false;
		}
		return false;
	}
	// 步骤: 提取头部区域视图
	memset(pOut, 0, sizeof(xrtmultipartpartview));
	pOut->tHeaders = xrtStrView(pStream->pBuffer + pStream->iCursor, iHeaderEnd - pStream->iCursor);
	// 算法步骤: 逐行解析头部字段
	while ( xrtHttpHeaderNextLineN(pOut->tHeaders.sPtr, pOut->tHeaders.iLen, &iHeaderOff, &tHeader) ) {
		++iHeaderCount;
		// 步骤: 检查头部行数是否超出限制
		if ( iHeaderCount > pStream->iMaxPartHeaders ) {
			pStream->iError = XRT_MULTIPART_STREAM_ERR_HEADER_LIMIT;
			pStream->iState = XRT_MULTIPART_STREAM_STATE_ERROR;
			return false;
		}
		// 步骤: 按头部名称分发处理
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
	// 步骤: 移动游标跳过头部和分隔符, 指向 body 起始位置
	pStream->iCursor = iHeaderEnd + 4u;
	return true;
}


// 获取下一个 Multipart 流事件
// 算法说明 - Multipart 流式解析状态机, 按 RFC 2046 multipart 格式逐步解析
//   状态流转: SEEK_BOUNDARY -> HEADERS -> BODY -> PART_END -> (循环或 DONE)
//   每次调用返回一个事件(PART_BEGIN/DATA/PART_END/END/NEED_MORE/ERROR)
//   BODY 状态下采用尾部保留策略, 避免在边界分割处遗漏数据
XXAPI xrtmultipartstreamresult xrtMultipartStreamNext(xrtmultipartstream* pStream, xrtmultipartstreamevent* pEvent)
{
	// 步骤: 初始化事件结构
	if ( pEvent ) { __xrtHttpUtilMultipartStreamZeroEvent(pEvent); }
	if ( pStream == NULL || pEvent == NULL ) { return XRT_MULTIPART_STREAM_RESULT_ERROR; }
	// 步骤: 检查是否已处于终态
	if ( pStream->iState == XRT_MULTIPART_STREAM_STATE_ERROR ) {
		pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_ERROR;
		return pEvent->iResult;
	}
	if ( pStream->iState == XRT_MULTIPART_STREAM_STATE_DONE ) {
		pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_END;
		return pEvent->iResult;
	}
	// 步骤: 压缩缓冲区, 移除已消费的数据
	__xrtHttpUtilMultipartStreamCompact(pStream);
	// 步骤: 状态机主循环
	for (;;) {
		// 状态: 查找下一个边界行
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
			// 步骤: 找到边界后移动游标到边界之后
			pStream->iCursor = iAfter;
			if ( bFinal ) {
				// 步骤: 结束边界(--boundary--), 流结束
				pStream->iState = XRT_MULTIPART_STREAM_STATE_DONE;
				pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_END;
				return pEvent->iResult;
			}
			// 步骤: 普通边界, 切换到头部解析状态
			pStream->iState = XRT_MULTIPART_STREAM_STATE_HEADERS;
			continue;
		}
		// 状态: 解析 Part 的头部块
		if ( pStream->iState == XRT_MULTIPART_STREAM_STATE_HEADERS ) {
			xrtmultipartpartview tPart;
			if ( !__xrtHttpUtilMultipartStreamParseHeaders(pStream, &tPart) ) {
				if ( pStream->iState == XRT_MULTIPART_STREAM_STATE_ERROR ) {
					pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_ERROR;
					return pEvent->iResult;
				}
				return XRT_MULTIPART_STREAM_RESULT_NEED_MORE;
			}
			// 步骤: 头部解析完成, 发出 PART_BEGIN 事件
			pStream->tCurrentPart = tPart;
			pStream->iState = XRT_MULTIPART_STREAM_STATE_BODY;
			pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_PART_BEGIN;
			pEvent->tPart = tPart;
			return pEvent->iResult;
		}
		// 状态: 流式输出 Part 的 Body 数据
		if ( pStream->iState == XRT_MULTIPART_STREAM_STATE_BODY ) {
			size_t iPos = 0u;
			bool bFinal = false;
			size_t iAfter = 0u;
			// 步骤: 尝试在缓冲区中查找下一个边界
			if ( __xrtHttpUtilFindBoundaryLine(pStream->pBuffer, pStream->iBufferLen, pStream->iCursor, pStream->aBoundary, pStream->iBoundaryLen, &iPos, &bFinal, &iAfter) ) {
				// 步骤: 找到边界, 输出边界之前的全部 body 数据(去掉 \r\n 前缀)
				size_t iDataLen = (iPos >= pStream->iCursor + 2u) ? (iPos - pStream->iCursor - 2u) : 0u;
				if ( iDataLen > 0u ) {
					pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_DATA;
					pEvent->tPart = pStream->tCurrentPart;
					pEvent->tData = xrtStrView(pStream->pBuffer + pStream->iCursor, iDataLen);
					pStream->iCursor += iDataLen;
					return pEvent->iResult;
				}
				// 步骤: body 数据已全部输出, 保存边界信息并转入 PART_END
				pStream->iBoundaryPos = iPos;
				pStream->iAfterBoundary = iAfter;
				pStream->bFinalBoundary = bFinal;
				pStream->iState = XRT_MULTIPART_STREAM_STATE_PART_END;
				continue;
			}
			// 步骤: 未找到边界且输入已结束, 报错
			if ( pStream->bFinishedInput ) {
				pStream->iError = XRT_MULTIPART_STREAM_ERR_TRUNCATED;
				pStream->iState = XRT_MULTIPART_STREAM_STATE_ERROR;
				pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_ERROR;
				return pEvent->iResult;
			}
			// 步骤: 尾部保留策略 - 只输出超出保留范围的数据, 保留部分可能包含不完整边界
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
		// 状态: Part 结束, 发出 PART_END 事件
		if ( pStream->iState == XRT_MULTIPART_STREAM_STATE_PART_END ) {
			pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_PART_END;
			pEvent->tPart = pStream->tCurrentPart;
			// 步骤: 移动游标到边界之后, 为下一个 Part 做准备
			pStream->iCursor = pStream->iAfterBoundary;
			memset(&pStream->tCurrentPart, 0, sizeof(xrtmultipartpartview));
			// 步骤: 根据是否为结束边界决定进入 DONE 还是继续解析头部
			pStream->iState = pStream->bFinalBoundary ? XRT_MULTIPART_STREAM_STATE_DONE : XRT_MULTIPART_STREAM_STATE_HEADERS;
			return pEvent->iResult;
		}
		if ( pStream->iState == XRT_MULTIPART_STREAM_STATE_DONE ) {
			pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_END;
			return pEvent->iResult;
		}
		// 步骤: 未知状态, 报错
		pStream->iError = XRT_MULTIPART_STREAM_ERR_INVALID_HEADER;
		pStream->iState = XRT_MULTIPART_STREAM_STATE_ERROR;
		pEvent->iResult = XRT_MULTIPART_STREAM_RESULT_ERROR;
		return pEvent->iResult;
	}
}


// xrtMultipartAppendFieldPartTo 相关处理
XXAPI bool xrtMultipartAppendFieldPartTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, size_t iBoundaryLen, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen)
{
	if ( sOut == NULL || pOffset == NULL || sName == NULL || (sValue == NULL && iValueLen != 0u) ) { return false; }
	if ( !__xrtHttpUtilValidateBoundaryN(sBoundary, iBoundaryLen) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "--", 2u) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sBoundary, iBoundaryLen) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\r\nContent-Disposition: form-data; name=", 39u) ) { return false; }
	if ( !__xrtHttpUtilAppendQuotedString(sOut, iOutCap, pOffset, sName, iNameLen) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\r\n\r\n", 4u) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sValue, iValueLen) ) { return false; }
	return __xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\r\n", 2u);
}


// xrtMultipartAppendFieldPart 相关处理
XXAPI bool xrtMultipartAppendFieldPart(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, const char* sName, const char* sValue)
{
	if ( sBoundary == NULL || sName == NULL || sValue == NULL ) { return false; }
	return xrtMultipartAppendFieldPartTo(sOut, iOutCap, pOffset, sBoundary, strlen(sBoundary), sName, strlen(__xrt_cstr(sName)), sValue, strlen(sValue));
}


// 追加 Multipart 原始数据 part
XXAPI bool xrtMultipartAppendRawPartTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, size_t iBoundaryLen, const xrtheaderpair* pHeaders, size_t iHeaderCount, const char* pBody, size_t iBodyLen)
{
	size_t i;
	if ( sOut == NULL || pOffset == NULL || (pHeaders == NULL && iHeaderCount != 0u) || (pBody == NULL && iBodyLen != 0u) ) { return false; }
	if ( !__xrtHttpUtilValidateBoundaryN(sBoundary, iBoundaryLen) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "--", 2u) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sBoundary, iBoundaryLen) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\r\n", 2u) ) { return false; }
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
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\r\n", 2u) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, pBody, iBodyLen) ) { return false; }
	return __xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\r\n", 2u);
}


// 追加 Multipart 原始数据 part
XXAPI bool xrtMultipartAppendRawPart(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, const xrtheaderpair* pHeaders, size_t iHeaderCount, const char* pBody, size_t iBodyLen)
{
	if ( sBoundary == NULL ) { return false; }
	return xrtMultipartAppendRawPartTo(sOut, iOutCap, pOffset, sBoundary, strlen(sBoundary), pHeaders, iHeaderCount, pBody, iBodyLen);
}


// 追加 Multipart 文件 part 扩展
XXAPI bool xrtMultipartAppendFilePartExtTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, size_t iBoundaryLen, const char* sName, size_t iNameLen, const char* sFileName, size_t iFileNameLen, const char* sFileNameExt, size_t iFileNameExtLen, const char* sContentType, size_t iContentTypeLen, const char* pBody, size_t iBodyLen)
{
	if ( sOut == NULL || pOffset == NULL || sName == NULL || sFileName == NULL || (sContentType == NULL && iContentTypeLen != 0u) || (pBody == NULL && iBodyLen != 0u) ) { return false; }
	if ( !__xrtHttpUtilValidateBoundaryN(sBoundary, iBoundaryLen) ) { return false; }
	if ( iContentTypeLen != 0u && __xrtHttpUtilContainsCtl(sContentType, iContentTypeLen) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "--", 2u) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sBoundary, iBoundaryLen) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\r\nContent-Disposition: form-data; name=", 39u) ) { return false; }
	if ( !__xrtHttpUtilAppendQuotedString(sOut, iOutCap, pOffset, sName, iNameLen) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "; filename=", 11u) ) { return false; }
	if ( !__xrtHttpUtilAppendQuotedString(sOut, iOutCap, pOffset, sFileName, iFileNameLen) ) { return false; }
	if ( sFileNameExt != NULL && iFileNameExtLen != 0u ) {
		size_t iExtLen = 0u;
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "; filename*=", 12u) ) { return false; }
		if ( !xrtHttpBuildExtValueTo("UTF-8", "", sFileNameExt, iFileNameExtLen, sOut + *pOffset, iOutCap - *pOffset, &iExtLen) ) { return false; }
		*pOffset += iExtLen;
	}
	if ( iContentTypeLen != 0u ) {
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\r\nContent-Type: ", 16u) ) { return false; }
		if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sContentType, iContentTypeLen) ) { return false; }
	}
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\r\n\r\n", 4u) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, pBody, iBodyLen) ) { return false; }
	return __xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "\r\n", 2u);
}


// 追加 Multipart 文件 part 扩展
XXAPI bool xrtMultipartAppendFilePartExt(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, const char* sName, const char* sFileName, const char* sFileNameExt, const char* sContentType, const char* pBody, size_t iBodyLen)
{
	if ( sBoundary == NULL || sName == NULL || sFileName == NULL || pBody == NULL ) { return false; }
	return xrtMultipartAppendFilePartExtTo(
		sOut,
		iOutCap,
		pOffset,
		sBoundary,
		strlen(sBoundary),
		sName,
		strlen(__xrt_cstr(sName)),
		sFileName,
		strlen(sFileName),
		sFileNameExt,
		sFileNameExt ? strlen(sFileNameExt) : 0u,
		sContentType,
		sContentType ? strlen(sContentType) : 0u,
		pBody,
		iBodyLen);
}


// 追加 Multipart 文件 part
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


// 追加 Multipart 文件 part
XXAPI bool xrtMultipartAppendFilePart(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, const char* sName, const char* sFileName, const char* sContentType, const char* pBody, size_t iBodyLen)
{
	return xrtMultipartAppendFilePartExt(sOut, iOutCap, pOffset, sBoundary, sName, sFileName, NULL, sContentType, pBody, iBodyLen);
}


// xrtMultipartAppendFinishTo 相关处理
XXAPI bool xrtMultipartAppendFinishTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary, size_t iBoundaryLen)
{
	if ( sOut == NULL || pOffset == NULL ) { return false; }
	if ( !__xrtHttpUtilValidateBoundaryN(sBoundary, iBoundaryLen) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "--", 2u) ) { return false; }
	if ( !__xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, sBoundary, iBoundaryLen) ) { return false; }
	return __xrtHttpUtilAppendBytes(sOut, iOutCap, pOffset, "--\r\n", 4u);
}


// xrtMultipartAppendFinish 相关处理
XXAPI bool xrtMultipartAppendFinish(char* sOut, size_t iOutCap, size_t* pOffset, const char* sBoundary)
{
	if ( sBoundary == NULL ) { return false; }
	return xrtMultipartAppendFinishTo(sOut, iOutCap, pOffset, sBoundary, strlen(sBoundary));
}

#endif
