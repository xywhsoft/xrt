#ifndef XRT_XURL_H
#define XRT_XURL_H

/*
	XRT URL/query implementation layer.

	Public declarations live in xrt.h.
	This file is intended to be expanded only from xrt.c / single-head generation.
*/

// 构造字符串视图
XXAPI xrtstrview xrtStrView(const char* sPtr, size_t iLen)
{
	xrtstrview tView;
	tView.sPtr = sPtr;
	tView.iLen = iLen;
	return tView;
}


// 判断字符串视图是否为空
XXAPI bool xrtStrViewIsEmpty(xrtstrview tView)
{
	return tView.sPtr == NULL || tView.iLen == 0u;
}


// 复制字符串视图
XXAPI bool xrtStrViewCopyTo(xrtstrview tView, char* sOut, size_t iOutCap)
{
	if ( sOut == NULL || iOutCap == 0u ) { return false; }
	if ( xrtStrViewIsEmpty(tView) ) {
		sOut[0] = '\0';
		return true;
	}
	if ( tView.iLen >= iOutCap ) { return false; }
	memcpy(sOut, tView.sPtr, tView.iLen);
	sOut[tView.iLen] = '\0';
	return true;
}


// 内部函数：判断两个 ASCII 字符是否忽略大小写相等
static bool __xrtUrlAsciiEqNoCase(char chA, char chB)
{
	if ( chA >= 'A' && chA <= 'Z' ) { chA = (char)(chA + 32); }
	if ( chB >= 'A' && chB <= 'Z' ) { chB = (char)(chB + 32); }
	return chA == chB;
}


// 内部函数：判断 URL 视图与文本是否忽略大小写相等
static bool __xrtUrlViewEqNoCase(xrtstrview tView, const char* sText)
{
	size_t i = 0u;
	if ( sText == NULL ) { return false; }
	while ( sText[i] != '\0' ) {
		if ( i >= tView.iLen ) { return false; }
		if ( !__xrtUrlAsciiEqNoCase(tView.sPtr[i], sText[i]) ) { return false; }
		i++;
	}
	return i == tView.iLen;
}


// 内部函数：判断字符是否为控制字符或空白
static bool __xrtUrlIsCtlOrSpace(char ch)
{
	unsigned char chU = (unsigned char)ch;
	return chU <= 0x20u || chU == 0x7Fu;
}


// 内部函数：校验文本中是否包含控制字符或空白
static bool __xrtUrlValidateNoCtlOrSpace(const char* sText, size_t iLen)
{
	size_t i;
	if ( sText == NULL ) { return false; }
	for ( i = 0u; i < iLen; ++i ) {
		if ( __xrtUrlIsCtlOrSpace(sText[i]) ) { return false; }
	}
	return true;
}


// 内部函数：判断是否为 URL 协议字符
static bool __xrtUrlIsSchemeChar(char ch, bool bFirst)
{
	if ( (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ) { return true; }
	if ( bFirst ) { return false; }
	if ( ch >= '0' && ch <= '9' ) { return true; }
	return ch == '+' || ch == '-' || ch == '.';
}


// 内部函数：解析 URL 端口
static bool __xrtUrlParsePort(const char* sText, size_t iLen, uint16* pPort)
{
	uint32 iValue = 0u;
	size_t i;
	if ( sText == NULL || pPort == NULL || iLen == 0u || iLen > 5u ) { return false; }
	for ( i = 0u; i < iLen; ++i ) {
		char ch = sText[i];
		if ( ch < '0' || ch > '9' ) { return false; }
		iValue = (iValue * 10u) + (uint32)(ch - '0');
		if ( iValue > 65535u ) { return false; }
	}
	if ( iValue == 0u ) { return false; }
	*pPort = (uint16)iValue;
	return true;
}


// 获取 URL 默认端口
XXAPI uint16 xrtUrlDefaultPort(xrtstrview tScheme)
{
	if ( __xrtUrlViewEqNoCase(tScheme, "http") ) { return 80u; }
	if ( __xrtUrlViewEqNoCase(tScheme, "ws") ) { return 80u; }
	if ( __xrtUrlViewEqNoCase(tScheme, "https") ) { return 443u; }
	if ( __xrtUrlViewEqNoCase(tScheme, "wss") ) { return 443u; }
	return 0u;
}


// 判断是否为 URL 安全协议
XXAPI bool xrtUrlIsSecureScheme(xrtstrview tScheme)
{
	return __xrtUrlViewEqNoCase(tScheme, "https") || __xrtUrlViewEqNoCase(tScheme, "wss");
}


// 内部函数：__xrtUrlHostNeedsBrackets
static bool __xrtUrlHostNeedsBrackets(xrtstrview tHost)
{
	size_t i;
	for ( i = 0u; i < tHost.iLen; ++i ) {
		if ( tHost.sPtr[i] == ':' ) { return true; }
	}
	return false;
}


// 内部函数：__xrtUrlAppendBytes
static bool __xrtUrlAppendBytes(char* sOut, size_t iOutCap, size_t* pOffset, const char* sText, size_t iLen)
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


// 内部函数：追加 URL 端口
static bool __xrtUrlAppendPort(char* sOut, size_t iOutCap, size_t* pOffset, uint16 iPort)
{
	char sBuf[8];
	int iLen = snprintf(sBuf, sizeof(sBuf), "%u", (unsigned)iPort);
	if ( iLen <= 0 || (size_t)iLen >= sizeof(sBuf) ) { return false; }
	return __xrtUrlAppendBytes(sOut, iOutCap, pOffset, sBuf, (size_t)iLen);
}


// 判断是否为 URL 默认端口
XXAPI bool xrtUrlIsDefaultPort(const xrturlview* pURL)
{
	uint16 iDefaultPort;
	if ( pURL == NULL || xrtStrViewIsEmpty(pURL->tScheme) || pURL->iPort == 0u ) { return false; }
	iDefaultPort = xrtUrlDefaultPort(pURL->tScheme);
	return iDefaultPort != 0u && iDefaultPort == pURL->iPort;
}


// 判断是否为 URL 视图协议
XXAPI bool xrtUrlViewIsScheme(const xrturlview* pURL, const char* sScheme)
{
	return pURL != NULL && sScheme != NULL && !xrtStrViewIsEmpty(pURL->tScheme) && __xrtUrlViewEqNoCase(pURL->tScheme, sScheme);
}


// 判断 URL 视图是否匹配两个协议之一
XXAPI bool xrtUrlViewMatchesScheme2(const xrturlview* pURL, const char* sSchemeA, const char* sSchemeB)
{
	return xrtUrlViewIsScheme(pURL, sSchemeA) || xrtUrlViewIsScheme(pURL, sSchemeB);
}


// 复制 URL 视图协议
XXAPI bool xrtUrlViewCopySchemeTo(const xrturlview* pURL, char* sOut, size_t iOutCap)
{
	if ( pURL == NULL ) { return false; }
	return xrtStrViewCopyTo(pURL->tScheme, sOut, iOutCap);
}


// 复制 URL 视图授权段
XXAPI bool xrtUrlViewCopyAuthorityTo(const xrturlview* pURL, char* sOut, size_t iOutCap)
{
	if ( pURL == NULL ) { return false; }
	return xrtStrViewCopyTo(pURL->tAuthority, sOut, iOutCap);
}


// 复制 URL 视图路径
XXAPI bool xrtUrlViewCopyPathTo(const xrturlview* pURL, char* sOut, size_t iOutCap)
{
	if ( pURL == NULL ) { return false; }
	return xrtStrViewCopyTo(pURL->tPath, sOut, iOutCap);
}


// 复制 URL 视图查询
XXAPI bool xrtUrlViewCopyQueryTo(const xrturlview* pURL, char* sOut, size_t iOutCap)
{
	if ( pURL == NULL ) { return false; }
	return xrtStrViewCopyTo(pURL->tQuery, sOut, iOutCap);
}


// 复制 URL 视图片段
XXAPI bool xrtUrlViewCopyFragmentTo(const xrturlview* pURL, char* sOut, size_t iOutCap)
{
	if ( pURL == NULL ) { return false; }
	return xrtStrViewCopyTo(pURL->tFragment, sOut, iOutCap);
}


// 解析 URL 授权段
XXAPI bool xrtUrlParseAuthorityN(const char* sText, size_t iLen, xrturlview* pOut)
{
	size_t iAt = (size_t)-1;
	size_t iHostStart = 0u;
	size_t iColon = (size_t)-1;
	size_t i;
	if ( pOut == NULL || sText == NULL || iLen == 0u ) { return false; }
	// 清零输出并校验无控制字符
	memset(pOut, 0, sizeof(xrturlview));
	if ( !__xrtUrlValidateNoCtlOrSpace(sText, iLen) ) { return false; }
	// 查找最后一个 '@' 以分离 userinfo 和 host 部分
	for ( i = 0u; i < iLen; ++i ) {
		if ( sText[i] == '@' ) { iAt = i; }
	}
	// 若存在 userinfo 则提取并记录起始位置
	if ( iAt != (size_t)-1 ) {
		if ( iAt == 0u || iAt + 1u >= iLen ) { return false; }
		pOut->tUserInfo = xrtStrView(sText, iAt);
		pOut->iFlags |= XRT_URL_F_HAS_USERINFO;
		iHostStart = iAt + 1u;
	}
	// IPv6 地址以 '[' 开头，需特殊处理括号内的地址
	if ( sText[iHostStart] == '[' ) {
		size_t iClose = (size_t)-1;
		for ( i = iHostStart + 1u; i < iLen; ++i ) {
			if ( sText[i] == ']' ) {
				iClose = i;
				break;
			}
		}
		// 括号必须闭合且内部不能为空
		if ( iClose == (size_t)-1 || iClose == iHostStart + 1u ) { return false; }
		pOut->tHost = xrtStrView(sText + iHostStart + 1u, iClose - iHostStart - 1u);
		pOut->iFlags |= XRT_URL_F_HAS_HOST;
		// 括号后可选端口
		if ( iClose + 1u < iLen ) {
			if ( sText[iClose + 1u] != ':' ) { return false; }
			if ( !__xrtUrlParsePort(sText + iClose + 2u, iLen - iClose - 2u, &pOut->iPort) ) { return false; }
			pOut->iFlags |= XRT_URL_F_HAS_PORT;
		}
	} else {
		// 非 IPv6 主机：查找冒号分隔主机和端口
		size_t iFirstColon = (size_t)-1;
		size_t iLastColon = (size_t)-1;
		for ( i = iHostStart; i < iLen; ++i ) {
			if ( sText[i] == ':' ) {
				if ( iFirstColon == (size_t)-1 ) { iFirstColon = i; }
				iLastColon = i;
			}
		}
		// 多个冒号说明不是合法的 host:port 格式
		if ( iFirstColon != (size_t)-1 && iFirstColon != iLastColon ) { return false; }
		if ( iLastColon != (size_t)-1 ) { iColon = iLastColon; }
		// 有冒号则分别提取主机和端口
		if ( iColon != (size_t)-1 ) {
			if ( iColon == iHostStart || iColon + 1u >= iLen ) { return false; }
			pOut->tHost = xrtStrView(sText + iHostStart, iColon - iHostStart);
			if ( !__xrtUrlParsePort(sText + iColon + 1u, iLen - iColon - 1u, &pOut->iPort) ) { return false; }
			pOut->iFlags |= XRT_URL_F_HAS_PORT;
		} else {
			pOut->tHost = xrtStrView(sText + iHostStart, iLen - iHostStart);
		}
		if ( xrtStrViewIsEmpty(pOut->tHost) ) { return false; }
		pOut->iFlags |= XRT_URL_F_HAS_HOST;
	}
	// 记录完整授权段视图
	pOut->tAuthority = xrtStrView(sText, iLen);
	pOut->iFlags |= XRT_URL_F_HAS_AUTHORITY;
	return true;
}


// 解析 URL 授权段
XXAPI bool xrtUrlParseAuthority(const char* sText, xrturlview* pOut)
{
	if ( sText == NULL ) { return false; }
	return xrtUrlParseAuthorityN(sText, strlen(__xrt_cstr(sText)), pOut);
}


// 解析 URL Target 部分
XXAPI bool xrtUrlParseTargetN(const char* sText, size_t iLen, xrturlview* pOut)
{
	size_t iQuery = (size_t)-1;
	size_t iFrag = (size_t)-1;
	size_t i;
	if ( pOut == NULL || sText == NULL ) { return false; }
	memset(pOut, 0, sizeof(xrturlview));
	if ( iLen == 0u ) { return false; }
	if ( !__xrtUrlValidateNoCtlOrSpace(sText, iLen) ) { return false; }
	// 查找 '?' 和 '#' 的边界位置
	for ( i = 0u; i < iLen; ++i ) {
		if ( sText[i] == '#' ) {
			iFrag = i;
			break;
		}
		if ( sText[i] == '?' && iQuery == (size_t)-1 ) { iQuery = i; }
	}
	// 设置标志位
	pOut->iFlags = XRT_URL_F_TARGET_ONLY;
	if ( iQuery != (size_t)-1 ) { pOut->iFlags |= XRT_URL_F_HAS_QUERY; }
	if ( iFrag != (size_t)-1 ) { pOut->iFlags |= XRT_URL_F_HAS_FRAGMENT; }
	// 根据边界提取路径和查询
	if ( iQuery != (size_t)-1 ) {
		pOut->tPath = xrtStrView(sText, iQuery);
		pOut->tQuery = xrtStrView(sText + iQuery + 1u, ((iFrag != (size_t)-1) ? iFrag : iLen) - iQuery - 1u);
	} else {
		pOut->tPath = xrtStrView(sText, ((iFrag != (size_t)-1) ? iFrag : iLen));
	}
	if ( pOut->tPath.iLen > 0u ) { pOut->iFlags |= XRT_URL_F_HAS_PATH; }
	// 提取片段
	if ( iFrag != (size_t)-1 && iFrag + 1u <= iLen ) {
		pOut->tFragment = xrtStrView(sText + iFrag + 1u, iLen - iFrag - 1u);
	}
	// 构建 target 视图（路径+查询，不含片段）
	pOut->tTarget = xrtStrView(sText, ((iFrag != (size_t)-1) ? iFrag : iLen));
	return true;
}


// 解析 URL Target 部分
XXAPI bool xrtUrlParseTarget(const char* sText, xrturlview* pOut)
{
	if ( sText == NULL ) { return false; }
	return xrtUrlParseTargetN(sText, strlen(__xrt_cstr(sText)), pOut);
}


// 解析 URL 视图
XXAPI bool xrtUrlParseViewN(const char* sText, size_t iLen, xrturlview* pOut)
{
	size_t iSchemeEnd = (size_t)-1;
	size_t iPos;
	size_t iPathStart;
	size_t iPathEnd;
	size_t iQuery = (size_t)-1;
	size_t iFrag = (size_t)-1;
	xrturlview tAuthority;
	size_t i;
	if ( pOut == NULL || sText == NULL ) { return false; }
	memset(pOut, 0, sizeof(xrturlview));
	if ( iLen == 0u ) { return false; }
	// R13: 扫描协议段，遇到 ':' 判断是否为 scheme:end，否则按相对 URL 处理
	for ( i = 0u; i < iLen; ++i ) {
		char ch = sText[i];
		if ( ch == ':' ) {
			iSchemeEnd = i;
			break;
		}
		if ( ch == '/' || ch == '?' || ch == '#' ) { break; }
		if ( !__xrtUrlIsSchemeChar(ch, i == 0u) ) {
			iSchemeEnd = (size_t)-1;
			break;
		}
	}
	// 无合法 scheme 或缺少 "://" 则按 target-only（相对URL）解析
	if ( iSchemeEnd == (size_t)-1 || iSchemeEnd + 2u >= iLen || sText[iSchemeEnd + 1u] != '/' || sText[iSchemeEnd + 2u] != '/' ) {
		return xrtUrlParseTargetN(sText, iLen, pOut);
	}
	if ( !__xrtUrlValidateNoCtlOrSpace(sText, iLen) ) { return false; }
	// 提取协议并标记为绝对 URL
	pOut->tScheme = xrtStrView(sText, iSchemeEnd);
	pOut->iFlags |= XRT_URL_F_ABSOLUTE;
	if ( xrtUrlIsSecureScheme(pOut->tScheme) ) { pOut->iFlags |= XRT_URL_F_SECURE; }
	// 跳过 "://" 定位授权段起始位置
	iPos = iSchemeEnd + 3u;
	iPathStart = iPos;
	// 授权段在第一个 '/'、'?' 或 '#' 之前结束
	while ( iPathStart < iLen && sText[iPathStart] != '/' && sText[iPathStart] != '?' && sText[iPathStart] != '#' ) { iPathStart++; }
	if ( iPathStart == iPos ) { return false; }
	// 解析授权段（含 userinfo、host、port）
	if ( !xrtUrlParseAuthorityN(sText + iPos, iPathStart - iPos, &tAuthority) ) { return false; }
	// 将授权段结果复制到输出结构
	pOut->tAuthority = tAuthority.tAuthority;
	pOut->tUserInfo = tAuthority.tUserInfo;
	pOut->tHost = tAuthority.tHost;
	pOut->iPort = tAuthority.iPort;
	pOut->iFlags |= tAuthority.iFlags & (XRT_URL_F_HAS_AUTHORITY | XRT_URL_F_HAS_USERINFO | XRT_URL_F_HAS_HOST | XRT_URL_F_HAS_PORT);
	// 未显式指定端口时使用协议默认端口
	if ( (pOut->iFlags & XRT_URL_F_HAS_PORT) == 0u ) {
		pOut->iPort = xrtUrlDefaultPort(pOut->tScheme);
	}
	// 从路径起始位置起查找 query 和 fragment 的边界
	for ( i = iPathStart; i < iLen; ++i ) {
		if ( sText[i] == '#' ) {
			iFrag = i;
			break;
		}
		if ( sText[i] == '?' && iQuery == (size_t)-1 ) { iQuery = i; }
	}
	// 提取路径、查询和片段
	iPathEnd = (iQuery != (size_t)-1) ? iQuery : ((iFrag != (size_t)-1) ? iFrag : iLen);
	pOut->tPath = xrtStrView(sText + iPathStart, iPathEnd - iPathStart);
	if ( pOut->tPath.iLen > 0u ) { pOut->iFlags |= XRT_URL_F_HAS_PATH; }
	if ( iQuery != (size_t)-1 ) {
		size_t iQueryEnd = (iFrag != (size_t)-1) ? iFrag : iLen;
		pOut->tQuery = xrtStrView(sText + iQuery + 1u, iQueryEnd - iQuery - 1u);
		pOut->iFlags |= XRT_URL_F_HAS_QUERY;
	}
	if ( iFrag != (size_t)-1 ) {
		pOut->tFragment = xrtStrView(sText + iFrag + 1u, iLen - iFrag - 1u);
		pOut->iFlags |= XRT_URL_F_HAS_FRAGMENT;
	}
	// 构建 target 视图（路径+查询，不含片段）
	pOut->tTarget = xrtStrView(sText + iPathStart, ((iFrag != (size_t)-1) ? iFrag : iLen) - iPathStart);
	return true;
}


// 解析 URL 视图
XXAPI bool xrtUrlParseView(const char* sText, xrturlview* pOut)
{
	if ( sText == NULL ) { return false; }
	return xrtUrlParseViewN(sText, strlen(__xrt_cstr(sText)), pOut);
}


// 复制 URL 视图主机
XXAPI bool xrtUrlViewCopyHostTo(const xrturlview* pURL, char* sOut, size_t iOutCap)
{
	if ( pURL == NULL || xrtStrViewIsEmpty(pURL->tHost) ) { return false; }
	return xrtStrViewCopyTo(pURL->tHost, sOut, iOutCap);
}


// 复制 URL 视图 Target 部分
XXAPI bool xrtUrlViewCopyTargetTo(const xrturlview* pURL, char* sOut, size_t iOutCap)
{
	if ( pURL == NULL || sOut == NULL || iOutCap == 0u ) { return false; }
	if ( xrtStrViewIsEmpty(pURL->tTarget) ) {
		if ( iOutCap < 2u ) { return false; }
		sOut[0] = '/';
		sOut[1] = '\0';
		return true;
	}
	if ( pURL->tTarget.sPtr[0] == '?' ) {
		if ( pURL->tTarget.iLen + 2u > iOutCap ) { return false; }
		sOut[0] = '/';
		memcpy(sOut + 1u, pURL->tTarget.sPtr, pURL->tTarget.iLen);
		sOut[pURL->tTarget.iLen + 1u] = '\0';
		return true;
	}
	return xrtStrViewCopyTo(pURL->tTarget, sOut, iOutCap);
}


// 构建 URL 主机头部
XXAPI bool xrtUrlMakeHostHeader(const xrturlview* pURL, char* sOut, size_t iOutCap)
{
	bool bDefaultPort;
	if ( sOut == NULL || iOutCap == 0u || pURL == NULL || xrtStrViewIsEmpty(pURL->tHost) ) { return false; }
	sOut[0] = '\0';
	bDefaultPort = pURL->iPort != 0u && pURL->iPort == xrtUrlDefaultPort(pURL->tScheme);
	if ( __xrtUrlHostNeedsBrackets(pURL->tHost) ) {
		if ( bDefaultPort || (pURL->iFlags & XRT_URL_F_HAS_PORT) == 0u ) {
			return snprintf(sOut, iOutCap, "[%.*s]", (int)pURL->tHost.iLen, pURL->tHost.sPtr) > 0 &&
				strlen(sOut) < iOutCap;
		}
		return snprintf(sOut, iOutCap, "[%.*s]:%u", (int)pURL->tHost.iLen, pURL->tHost.sPtr, (unsigned)pURL->iPort) > 0 &&
			strlen(sOut) < iOutCap;
	}
	if ( bDefaultPort || (pURL->iFlags & XRT_URL_F_HAS_PORT) == 0u ) {
		return snprintf(sOut, iOutCap, "%.*s", (int)pURL->tHost.iLen, pURL->tHost.sPtr) > 0 &&
			strlen(sOut) < iOutCap;
	}
	return snprintf(sOut, iOutCap, "%.*s:%u", (int)pURL->tHost.iLen, pURL->tHost.sPtr, (unsigned)pURL->iPort) > 0 &&
		strlen(sOut) < iOutCap;
}


// 构建 URL 主机头部固定长度
XXAPI bool xrtUrlMakeHostHeaderFixed(const char* sScheme, const char* sHost, uint16 iPort, char* sOut, size_t iOutCap)
{
	xrturlview tURL;
	if ( sScheme == NULL || sHost == NULL || sOut == NULL || iOutCap == 0u ) { return false; }
	memset(&tURL, 0, sizeof(tURL));
	tURL.tScheme = xrtStrView(sScheme, strlen(sScheme));
	tURL.tHost = xrtStrView(sHost, strlen(sHost));
	tURL.iPort = iPort;
	tURL.iFlags = XRT_URL_F_ABSOLUTE | XRT_URL_F_HAS_HOST;
	if ( iPort != 0u ) { tURL.iFlags |= XRT_URL_F_HAS_PORT; }
	if ( xrtUrlIsSecureScheme(tURL.tScheme) ) { tURL.iFlags |= XRT_URL_F_SECURE; }
	return xrtUrlMakeHostHeader(&tURL, sOut, iOutCap);
}


// 内部函数：获取 URL 最后一次 segment
static bool __xrtUrlGetLastSegment(const char* sPath, size_t iLen, size_t* pStart, size_t* pLen)
{
	size_t i;
	if ( pStart ) { *pStart = 0u; }
	if ( pLen ) { *pLen = 0u; }
	if ( sPath == NULL || iLen == 0u ) { return false; }
	i = iLen;
	while ( i > 0u && sPath[i - 1u] == '/' ) { i--; }
	if ( i == 0u ) { return false; }
	{
		size_t iEnd = i;
		while ( i > 0u && sPath[i - 1u] != '/' ) { i--; }
		if ( pStart ) { *pStart = i; }
		if ( pLen ) { *pLen = iEnd - i; }
		return iEnd > i;
	}
}


// 内部函数：__xrtUrlAppendSegment
static bool __xrtUrlAppendSegment(char* sOut, size_t iOutCap, size_t* pOffset, const char* sSeg, size_t iSegLen)
{
	if ( sOut == NULL || pOffset == NULL || (sSeg == NULL && iSegLen != 0u) ) { return false; }
	if ( *pOffset > 0u && sOut[*pOffset - 1u] != '/' ) {
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, pOffset, "/", 1u) ) { return false; }
	}
	return __xrtUrlAppendBytes(sOut, iOutCap, pOffset, sSeg, iSegLen);
}


// 内部函数：__xrtUrlAppendDotDotSegment
static bool __xrtUrlAppendDotDotSegment(char* sOut, size_t iOutCap, size_t* pOffset)
{
	return __xrtUrlAppendSegment(sOut, iOutCap, pOffset, "..", 2u);
}


// 内部函数：弹出 URL 最后一次 segment
static bool __xrtUrlPopLastSegment(char* sOut, size_t iOutCap, size_t* pOffset, bool bAbsolute)
{
	size_t iStart = 0u;
	size_t iLen = 0u;
	(void)iOutCap;
	if ( sOut == NULL || pOffset == NULL ) { return false; }
	if ( *pOffset == 0u ) { return false; }
	if ( bAbsolute && *pOffset == 1u && sOut[0] == '/' ) { return false; }
	if ( !__xrtUrlGetLastSegment(sOut, *pOffset, &iStart, &iLen) ) { return false; }
	if ( !bAbsolute && iLen == 2u && memcmp(sOut + iStart, "..", 2u) == 0 ) { return false; }
	*pOffset = iStart;
	if ( *pOffset > 0u && sOut[*pOffset - 1u] == '/' && !(bAbsolute && *pOffset == 1u) ) { (*pOffset)--; }
	sOut[*pOffset] = '\0';
	if ( bAbsolute && *pOffset == 0u ) {
		sOut[0] = '/';
		sOut[1] = '\0';
		*pOffset = 1u;
	}
	return true;
}


// 规范化 URL 路径
XXAPI bool xrtUrlNormalizePathTo(const char* sPath, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	bool bAbsolute;
	bool bTrailingSlash;
	size_t i = 0u;
	size_t iOff = 0u;
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( sPath == NULL || sOut == NULL || iOutCap == 0u ) { return false; }
	if ( !__xrtUrlValidateNoCtlOrSpace(sPath, iLen) ) { return false; }
	sOut[0] = '\0';
	// R13: 判断路径是绝对路径（以 '/' 开头）还是相对路径
	bAbsolute = iLen > 0u && sPath[0] == '/';
	bTrailingSlash = iLen > 0u && sPath[iLen - 1u] == '/';
	// 绝对路径以 '/' 开头，跳过多余的连续斜杠
	if ( bAbsolute ) {
		sOut[0] = '/';
		sOut[1] = '\0';
		iOff = 1u;
		while ( i < iLen && sPath[i] == '/' ) { i++; }
	}
	// 逐段处理路径
	while ( i < iLen ) {
		size_t iSegStart = i;
		size_t iSegLen;
		while ( i < iLen && sPath[i] != '/' ) { i++; }
		iSegLen = i - iSegStart;
		// 空段或单点段 "." 直接跳过
		if ( iSegLen == 0u || (iSegLen == 1u && sPath[iSegStart] == '.') ) {
		} else if ( iSegLen == 2u && sPath[iSegStart] == '.' && sPath[iSegStart + 1u] == '.' ) {
			// 双点段 ".." 回退到上一级目录
			if ( !__xrtUrlPopLastSegment(sOut, iOutCap, &iOff, bAbsolute) ) {
				if ( !bAbsolute ) {
					if ( !__xrtUrlAppendDotDotSegment(sOut, iOutCap, &iOff) ) { return false; }
				}
			}
		} else {
			// 普通段直接追加
			if ( !__xrtUrlAppendSegment(sOut, iOutCap, &iOff, sPath + iSegStart, iSegLen) ) { return false; }
		}
		// 跳过连续斜杠
		while ( i < iLen && sPath[i] == '/' ) { i++; }
	}
	// 处理尾部斜杠
	if ( bTrailingSlash ) {
		if ( iOff == 0u ) {
			if ( bAbsolute ) {
				if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "/", 1u) ) { return false; }
			} else {
				if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "./", 2u) ) { return false; }
			}
		} else if ( sOut[iOff - 1u] != '/' ) {
			if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "/", 1u) ) { return false; }
		}
	}
	// 绝对路径至少输出 "/"
	if ( bAbsolute && iOff == 0u ) {
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "/", 1u) ) { return false; }
	}
	if ( pOutLen ) { *pOutLen = iOff; }
	return true;
}


// 构建 URL Target 部分
XXAPI bool xrtUrlBuildTarget(const xrturlview* pURL, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( sOut == NULL || iOutCap == 0u || pURL == NULL ) { return false; }
	sOut[0] = '\0';
	// 追加路径部分，无路径时使用 "/"
	if ( !xrtStrViewIsEmpty(pURL->tPath) ) {
		if ( pURL->tPath.sPtr[0] != '/' ) { return false; }
		if ( !__xrtUrlValidateNoCtlOrSpace(pURL->tPath.sPtr, pURL->tPath.iLen) ) { return false; }
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, pURL->tPath.sPtr, pURL->tPath.iLen) ) { return false; }
	} else {
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "/", 1u) ) { return false; }
	}
	// 追加查询字符串（以 '?' 开头）
	if ( !xrtStrViewIsEmpty(pURL->tQuery) ) {
		if ( !__xrtUrlValidateNoCtlOrSpace(pURL->tQuery.sPtr, pURL->tQuery.iLen) ) { return false; }
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "?", 1u) ) { return false; }
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, pURL->tQuery.sPtr, pURL->tQuery.iLen) ) { return false; }
	}
	// 追加片段（以 '#' 开头）
	if ( !xrtStrViewIsEmpty(pURL->tFragment) ) {
		if ( !__xrtUrlValidateNoCtlOrSpace(pURL->tFragment.sPtr, pURL->tFragment.iLen) ) { return false; }
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "#", 1u) ) { return false; }
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, pURL->tFragment.sPtr, pURL->tFragment.iLen) ) { return false; }
	}
	if ( pOutLen ) { *pOutLen = iOff; }
	return true;
}


// 构建 URL 授权段
XXAPI bool xrtUrlBuildAuthority(const xrturlview* pURL, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	bool bNeedPort;
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( sOut == NULL || iOutCap == 0u || pURL == NULL || xrtStrViewIsEmpty(pURL->tHost) ) { return false; }
	sOut[0] = '\0';
	// 若有 userinfo 则先追加，后接 '@'
	if ( !xrtStrViewIsEmpty(pURL->tUserInfo) ) {
		if ( !__xrtUrlValidateNoCtlOrSpace(pURL->tUserInfo.sPtr, pURL->tUserInfo.iLen) ) { return false; }
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, pURL->tUserInfo.sPtr, pURL->tUserInfo.iLen) ) { return false; }
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "@", 1u) ) { return false; }
	}
	// IPv6 地址需用方括号包裹
	if ( __xrtUrlHostNeedsBrackets(pURL->tHost) ) {
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "[", 1u) ) { return false; }
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, pURL->tHost.sPtr, pURL->tHost.iLen) ) { return false; }
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "]", 1u) ) { return false; }
	} else {
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, pURL->tHost.sPtr, pURL->tHost.iLen) ) { return false; }
	}
	// 非默认端口时追加端口号
	bNeedPort = (pURL->iFlags & XRT_URL_F_HAS_PORT) != 0u || (pURL->iPort != 0u && !xrtUrlIsDefaultPort(pURL));
	if ( bNeedPort && pURL->iPort != 0u ) {
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, ":", 1u) ) { return false; }
		if ( !__xrtUrlAppendPort(sOut, iOutCap, &iOff, pURL->iPort) ) { return false; }
	}
	if ( pOutLen ) { *pOutLen = iOff; }
	return true;
}


// 构建 URL
XXAPI bool xrtUrlBuild(const xrturlview* pURL, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	char sTarget[4096];
	size_t iTargetLen = 0u;
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( sOut == NULL || iOutCap == 0u || pURL == NULL ) { return false; }
	sOut[0] = '\0';
	if ( (pURL->iFlags & XRT_URL_F_ABSOLUTE) == 0u ) {
		return xrtUrlBuildTarget(pURL, sOut, iOutCap, pOutLen);
	}
	if ( xrtStrViewIsEmpty(pURL->tScheme) || xrtStrViewIsEmpty(pURL->tHost) ) { return false; }
	if ( !__xrtUrlValidateNoCtlOrSpace(pURL->tScheme.sPtr, pURL->tScheme.iLen) ) { return false; }
	if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, pURL->tScheme.sPtr, pURL->tScheme.iLen) ) { return false; }
	if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "://", 3u) ) { return false; }
	if ( !xrtUrlBuildAuthority(pURL, sOut + iOff, iOutCap - iOff, &iTargetLen) ) { return false; }
	iOff += iTargetLen;
	if ( !xrtUrlBuildTarget(pURL, sTarget, sizeof(sTarget), &iTargetLen) ) { return false; }
	if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, sTarget, iTargetLen) ) { return false; }
	if ( pOutLen ) { *pOutLen = iOff; }
	return true;
}


// 解析 URL
XXAPI bool xrtUrlResolveTo(const xrturlview* pBase, const char* sRef, size_t iRefLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	xrturlview tRef;
	xrturlview tOut;
	char sMergedPath[4096];
	char sNormalizedPath[4096];
	size_t iNormLen = 0u;
	size_t iMergeLen = 0u;
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( pBase == NULL || sOut == NULL || iOutCap == 0u || sRef == NULL ) { return false; }
	// 基准 URL 必须是绝对 URL
	if ( (pBase->iFlags & XRT_URL_F_ABSOLUTE) == 0u || xrtStrViewIsEmpty(pBase->tScheme) || xrtStrViewIsEmpty(pBase->tHost) ) { return false; }
	// 空引用直接返回基准 URL
	if ( iRefLen == 0u ) { return xrtUrlBuild(pBase, sOut, iOutCap, pOutLen); }
	// R13: 协议相对 URL（以 "//" 开头），替换基准 URL 的授权段
	if ( iRefLen >= 2u && sRef[0] == '/' && sRef[1] == '/' ) {
		size_t iAuthEnd = 2u;
		xrturlview tAuthority;
		memset(&tOut, 0, sizeof(tOut));
		// 定位授权段结束位置
		while ( iAuthEnd < iRefLen && sRef[iAuthEnd] != '/' && sRef[iAuthEnd] != '?' && sRef[iAuthEnd] != '#' ) { iAuthEnd++; }
		if ( !xrtUrlParseAuthorityN(sRef + 2u, iAuthEnd - 2u, &tAuthority) ) { return false; }
		// 继承基准 URL 的协议，替换授权段
		tOut = *pBase;
		tOut.tAuthority = tAuthority.tAuthority;
		tOut.tUserInfo = tAuthority.tUserInfo;
		tOut.tHost = tAuthority.tHost;
		tOut.iPort = (tAuthority.iFlags & XRT_URL_F_HAS_PORT) ? tAuthority.iPort : xrtUrlDefaultPort(pBase->tScheme);
		tOut.iFlags &= ~(XRT_URL_F_HAS_USERINFO | XRT_URL_F_HAS_PORT | XRT_URL_F_HAS_PATH | XRT_URL_F_HAS_QUERY | XRT_URL_F_HAS_FRAGMENT);
		tOut.iFlags |= XRT_URL_F_ABSOLUTE | XRT_URL_F_HAS_AUTHORITY | XRT_URL_F_HAS_HOST | (pBase->iFlags & XRT_URL_F_SECURE);
		tOut.iFlags |= tAuthority.iFlags & (XRT_URL_F_HAS_USERINFO | XRT_URL_F_HAS_PORT);
		// 授权段后可能还有 target 部分
		if ( iAuthEnd < iRefLen ) {
			if ( !xrtUrlParseTargetN(sRef + iAuthEnd, iRefLen - iAuthEnd, &tRef) ) { return false; }
			tOut.tPath = tRef.tPath;
			tOut.tQuery = tRef.tQuery;
			tOut.tFragment = tRef.tFragment;
			tOut.tTarget = tRef.tTarget;
			tOut.iFlags |= tRef.iFlags & (XRT_URL_F_HAS_PATH | XRT_URL_F_HAS_QUERY | XRT_URL_F_HAS_FRAGMENT);
		}
		return xrtUrlBuild(&tOut, sOut, iOutCap, pOutLen);
	}
	// 解析引用 URL
	if ( !xrtUrlParseViewN(sRef, iRefLen, &tRef) ) { return false; }
	// 绝对引用直接返回
	if ( (tRef.iFlags & XRT_URL_F_ABSOLUTE) != 0u ) { return xrtUrlBuild(&tRef, sOut, iOutCap, pOutLen); }
	// 相对引用：以基准 URL 为基础，清除路径/查询/片段
	tOut = *pBase;
	tOut.iFlags &= ~(XRT_URL_F_HAS_PATH | XRT_URL_F_HAS_QUERY | XRT_URL_F_HAS_FRAGMENT);
	tOut.tQuery = xrtStrView(NULL, 0u);
	tOut.tFragment = xrtStrView(NULL, 0u);
	// 处理相对路径引用
	if ( (tRef.iFlags & XRT_URL_F_HAS_PATH) != 0u && !xrtStrViewIsEmpty(tRef.tPath) ) {
		if ( tRef.tPath.sPtr[0] == '/' ) {
			// 绝对路径直接规范化
			if ( !xrtUrlNormalizePathTo(tRef.tPath.sPtr, tRef.tPath.iLen, sNormalizedPath, sizeof(sNormalizedPath), &iNormLen) ) { return false; }
		} else {
			// 相对路径：合并基准路径的目录部分与引用路径
			size_t iBaseDirLen = 0u;
			if ( !xrtStrViewIsEmpty(pBase->tPath) ) {
				size_t i;
				for ( i = pBase->tPath.iLen; i > 0u; --i ) {
					if ( pBase->tPath.sPtr[i - 1u] == '/' ) {
						iBaseDirLen = i;
						break;
					}
				}
				if ( iBaseDirLen == 0u ) { iBaseDirLen = 1u; }
			} else {
				iBaseDirLen = 1u;
			}
			// 拼接基准目录 + 引用路径
			if ( iBaseDirLen == 1u && (xrtStrViewIsEmpty(pBase->tPath) || pBase->tPath.sPtr[0] != '/') ) {
				if ( !__xrtUrlAppendBytes(sMergedPath, sizeof(sMergedPath), &iMergeLen, "/", 1u) ) { return false; }
			} else {
				if ( !__xrtUrlAppendBytes(sMergedPath, sizeof(sMergedPath), &iMergeLen, pBase->tPath.sPtr, iBaseDirLen) ) { return false; }
			}
			if ( !__xrtUrlAppendBytes(sMergedPath, sizeof(sMergedPath), &iMergeLen, tRef.tPath.sPtr, tRef.tPath.iLen) ) { return false; }
			// 规范化合并后的路径
			if ( !xrtUrlNormalizePathTo(sMergedPath, iMergeLen, sNormalizedPath, sizeof(sNormalizedPath), &iNormLen) ) { return false; }
		}
		tOut.tPath = xrtStrView(sNormalizedPath, iNormLen);
		tOut.iFlags |= XRT_URL_F_HAS_PATH;
		// 引用中的查询和片段覆盖基准
		if ( (tRef.iFlags & XRT_URL_F_HAS_QUERY) != 0u ) {
			tOut.tQuery = tRef.tQuery;
			tOut.iFlags |= XRT_URL_F_HAS_QUERY;
		}
		if ( (tRef.iFlags & XRT_URL_F_HAS_FRAGMENT) != 0u ) {
			tOut.tFragment = tRef.tFragment;
			tOut.iFlags |= XRT_URL_F_HAS_FRAGMENT;
		}
		return xrtUrlBuild(&tOut, sOut, iOutCap, pOutLen);
	}
	// 引用无路径：保留基准路径，查询和片段由引用或基准决定
	tOut.tPath = pBase->tPath;
	if ( !xrtStrViewIsEmpty(pBase->tPath) ) { tOut.iFlags |= XRT_URL_F_HAS_PATH; }
	// 引用有查询则使用引用的，否则保留基准的
	if ( (tRef.iFlags & XRT_URL_F_HAS_QUERY) != 0u ) {
		tOut.tQuery = tRef.tQuery;
		tOut.iFlags |= XRT_URL_F_HAS_QUERY;
	} else if ( (pBase->iFlags & XRT_URL_F_HAS_QUERY) != 0u ) {
		tOut.tQuery = pBase->tQuery;
		tOut.iFlags |= XRT_URL_F_HAS_QUERY;
	}
	// 引用有片段则使用引用的
	if ( (tRef.iFlags & XRT_URL_F_HAS_FRAGMENT) != 0u ) {
		tOut.tFragment = tRef.tFragment;
		tOut.iFlags |= XRT_URL_F_HAS_FRAGMENT;
	}
	return xrtUrlBuild(&tOut, sOut, iOutCap, pOutLen);
}


// 解析 URL
XXAPI bool xrtUrlResolve(const xrturlview* pBase, const char* sRef, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( sRef == NULL ) { return false; }
	return xrtUrlResolveTo(pBase, sRef, strlen(sRef), sOut, iOutCap, pOutLen);
}


// 获取下一个查询
XXAPI bool xrtQueryNextN(const char* sQuery, size_t iLen, size_t* pOffset, xrtquerypair* pOut)
{
	size_t iCur;
	size_t iAmp;
	size_t iEq = (size_t)-1;
	size_t i;
	if ( sQuery == NULL || pOffset == NULL || pOut == NULL ) { return false; }
	iCur = *pOffset;
	// 跳过开头的 '?' 字符
	if ( iCur == 0u && iLen > 0u && sQuery[0] == '?' ) { iCur = 1u; }
	// 跳过多余的 '&' 分隔符
	while ( iCur < iLen && sQuery[iCur] == '&' ) { iCur++; }
	if ( iCur >= iLen ) {
		*pOffset = iCur;
		return false;
	}
	// 查找当前键值对的边界（到下一个 '&' 或字符串末尾）
	iAmp = iCur;
	while ( iAmp < iLen && sQuery[iAmp] != '&' ) {
		if ( sQuery[iAmp] == '=' && iEq == (size_t)-1 ) { iEq = iAmp; }
		iAmp++;
	}
	// 解析 key 和 value
	memset(pOut, 0, sizeof(xrtquerypair));
	if ( iEq != (size_t)-1 ) {
		pOut->tKey = xrtStrView(sQuery + iCur, iEq - iCur);
		pOut->tValue = xrtStrView(sQuery + iEq + 1u, iAmp - iEq - 1u);
		pOut->iFlags |= XRT_QUERY_F_HAS_VALUE;
	} else {
		pOut->tKey = xrtStrView(sQuery + iCur, iAmp - iCur);
	}
	// 校验 key 中不含控制字符或空白
	for ( i = 0u; i < pOut->tKey.iLen; ++i ) {
		if ( __xrtUrlIsCtlOrSpace(pOut->tKey.sPtr[i]) ) { return false; }
	}
	// 更新偏移到下一个键值对
	*pOffset = (iAmp < iLen) ? (iAmp + 1u) : iAmp;
	return true;
}


// 获取下一个查询
XXAPI bool xrtQueryNext(const char* sQuery, size_t* pOffset, xrtquerypair* pOut)
{
	if ( sQuery == NULL ) { return false; }
	return xrtQueryNextN(sQuery, strlen(sQuery), pOffset, pOut);
}


// 统计查询
XXAPI size_t xrtQueryCountN(const char* sQuery, size_t iLen)
{
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtquerypair tPair;
	if ( sQuery == NULL ) { return 0u; }
	while ( xrtQueryNextN(sQuery, iLen, &iOffset, &tPair) ) { iCount++; }
	return iCount;
}


// 统计查询
XXAPI size_t xrtQueryCount(const char* sQuery)
{
	if ( sQuery == NULL ) { return 0u; }
	return xrtQueryCountN(sQuery, strlen(sQuery));
}


// 查找查询
XXAPI bool xrtQueryFindN(const char* sQuery, size_t iLen, const char* sKey, size_t iKeyLen, xrtquerypair* pOut)
{
	size_t iOffset = 0u;
	xrtquerypair tPair;
	if ( sQuery == NULL || sKey == NULL ) { return false; }
	while ( xrtQueryNextN(sQuery, iLen, &iOffset, &tPair) ) {
		if ( tPair.tKey.iLen == iKeyLen && memcmp(tPair.tKey.sPtr, sKey, iKeyLen) == 0 ) {
			if ( pOut ) { *pOut = tPair; }
			return true;
		}
	}
	return false;
}


// 查找查询
XXAPI bool xrtQueryFind(const char* sQuery, const char* sKey, xrtquerypair* pOut)
{
	if ( sQuery == NULL || sKey == NULL ) { return false; }
	return xrtQueryFindN(sQuery, strlen(sQuery), sKey, strlen(sKey), pOut);
}


// 解析查询
XXAPI bool xrtQueryParseToN(const char* sQuery, size_t iLen, xrtquerypair* pOut, size_t iCap, size_t* pCount)
{
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtquerypair tPair;
	if ( pCount ) { *pCount = 0u; }
	if ( sQuery == NULL || (pOut == NULL && iCap != 0u) ) { return false; }
	while ( xrtQueryNextN(sQuery, iLen, &iOffset, &tPair) ) {
		if ( iCount >= iCap ) { return false; }
		if ( pOut ) { pOut[iCount] = tPair; }
		++iCount;
	}
	if ( pCount ) { *pCount = iCount; }
	return true;
}


// 解析查询
XXAPI bool xrtQueryParseTo(const char* sQuery, xrtquerypair* pOut, size_t iCap, size_t* pCount)
{
	if ( sQuery == NULL ) { return false; }
	return xrtQueryParseToN(sQuery, strlen(sQuery), pOut, iCap, pCount);
}


// 内部函数：获取 URL hex 值
static int __xrtUrlHexValue(char ch)
{
	if ( ch >= '0' && ch <= '9' ) { return (int)(ch - '0'); }
	if ( ch >= 'a' && ch <= 'f' ) { return 10 + (int)(ch - 'a'); }
	if ( ch >= 'A' && ch <= 'F' ) { return 10 + (int)(ch - 'A'); }
	return -1;
}


// xrtPercentDecodeTo 相关处理
XXAPI bool xrtPercentDecodeTo(const char* sText, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen, bool bPlusAsSpace)
{
	size_t iOut = 0u;
	size_t i;
	if ( sText == NULL || sOut == NULL || iOutCap == 0u ) { return false; }
	for ( i = 0u; i < iLen; ++i ) {
		char ch = sText[i];
		// 百分号编码：%XX 转为对应字节
		if ( ch == '%' ) {
			int iHi;
			int iLo;
			if ( i + 2u >= iLen ) { return false; }
			iHi = __xrtUrlHexValue(sText[i + 1u]);
			iLo = __xrtUrlHexValue(sText[i + 2u]);
			if ( iHi < 0 || iLo < 0 ) { return false; }
			if ( iOut + 1u >= iOutCap ) { return false; }
			sOut[iOut++] = (char)((iHi << 4) | iLo);
			i += 2u;
			continue;
		}
		// 可选将 '+' 转为空格
		if ( ch == '+' && bPlusAsSpace ) { ch = ' '; }
		if ( iOut + 1u >= iOutCap ) { return false; }
		sOut[iOut++] = ch;
	}
	sOut[iOut] = '\0';
	if ( pOutLen ) { *pOutLen = iOut; }
	return true;
}


// 解析 URL 固定长度
XXAPI bool xrtUrlParseFixedTo(const char* sURL, const char* sSchemeA, const char* sSchemeB, bool* pSchemeB, char* sHost, size_t iHostCap, uint16* pPort, char* sTarget, size_t iTargetCap)
{
	xrturlview tURL;
	if ( sURL == NULL || sSchemeA == NULL || sHost == NULL || iHostCap == 0u || sTarget == NULL || iTargetCap == 0u ) { return false; }
	if ( !xrtUrlParseView(sURL, &tURL) ) { return false; }
	if ( (tURL.iFlags & XRT_URL_F_ABSOLUTE) == 0u ) { return false; }
	if ( sSchemeB != NULL ) {
		if ( !xrtUrlViewMatchesScheme2(&tURL, sSchemeA, sSchemeB) ) { return false; }
	} else if ( !xrtUrlViewIsScheme(&tURL, sSchemeA) ) {
		return false;
	}
	if ( !xrtUrlViewCopyHostTo(&tURL, sHost, iHostCap) ) { return false; }
	if ( !xrtUrlViewCopyTargetTo(&tURL, sTarget, iTargetCap) ) { return false; }
	if ( pPort ) { *pPort = tURL.iPort; }
	if ( pSchemeB ) { *pSchemeB = (sSchemeB != NULL) ? xrtUrlViewIsScheme(&tURL, sSchemeB) : false; }
	return true;
}


// 解析 URL
XXAPI bool xrtUrlParse(const char* sURL, xurl pOut)
{
	if ( pOut == NULL || sURL == NULL ) { return false; }
	memset(pOut, 0, sizeof(xurl_struct));
	if ( !xrtUrlParseFixedTo(sURL, "http", "https", &pOut->bHttps, pOut->sHost, sizeof(pOut->sHost), &pOut->iPort, pOut->sPath, sizeof(pOut->sPath)) ) { return false; }
	return true;
}

#endif
