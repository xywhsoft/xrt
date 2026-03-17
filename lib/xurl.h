#ifndef XRT_XURL_H
#define XRT_XURL_H

/*
    XRT URL/query implementation layer.

    Public declarations live in xrt.h.
    This file is intended to be expanded only from xrt.c / single-head generation.
*/

XXAPI xrtstrview xrtStrView(const char* sPtr, size_t iLen)
{
	xrtstrview tView;
	tView.sPtr = sPtr;
	tView.iLen = iLen;
	return tView;
}

XXAPI bool xrtStrViewIsEmpty(xrtstrview tView)
{
	return tView.sPtr == NULL || tView.iLen == 0u;
}

XXAPI bool xrtStrViewCopyTo(xrtstrview tView, char* sOut, size_t iOutCap)
{
	if ( sOut == NULL || iOutCap == 0u ) return false;
	if ( xrtStrViewIsEmpty(tView) ) {
		sOut[0] = '\0';
		return true;
	}
	if ( tView.iLen >= iOutCap ) return false;
	memcpy(sOut, tView.sPtr, tView.iLen);
	sOut[tView.iLen] = '\0';
	return true;
}

static bool __xrtUrlAsciiEqNoCase(char chA, char chB)
{
	if ( chA >= 'A' && chA <= 'Z' ) chA = (char)(chA + 32);
	if ( chB >= 'A' && chB <= 'Z' ) chB = (char)(chB + 32);
	return chA == chB;
}

static bool __xrtUrlViewEqNoCase(xrtstrview tView, const char* sText)
{
	size_t i = 0u;
	if ( sText == NULL ) return false;
	while ( sText[i] != '\0' ) {
		if ( i >= tView.iLen ) return false;
		if ( !__xrtUrlAsciiEqNoCase(tView.sPtr[i], sText[i]) ) return false;
		i++;
	}
	return i == tView.iLen;
}

static bool __xrtUrlIsCtlOrSpace(char ch)
{
	unsigned char chU = (unsigned char)ch;
	return chU <= 0x20u || chU == 0x7Fu;
}

static bool __xrtUrlValidateNoCtlOrSpace(const char* sText, size_t iLen)
{
	size_t i;
	if ( sText == NULL ) return false;
	for ( i = 0u; i < iLen; ++i ) {
		if ( __xrtUrlIsCtlOrSpace(sText[i]) ) return false;
	}
	return true;
}

static bool __xrtUrlIsSchemeChar(char ch, bool bFirst)
{
	if ( (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ) return true;
	if ( bFirst ) return false;
	if ( ch >= '0' && ch <= '9' ) return true;
	return ch == '+' || ch == '-' || ch == '.';
}

static bool __xrtUrlParsePort(const char* sText, size_t iLen, uint16* pPort)
{
	uint32 iValue = 0u;
	size_t i;
	if ( sText == NULL || pPort == NULL || iLen == 0u || iLen > 5u ) return false;
	for ( i = 0u; i < iLen; ++i ) {
		char ch = sText[i];
		if ( ch < '0' || ch > '9' ) return false;
		iValue = (iValue * 10u) + (uint32)(ch - '0');
		if ( iValue > 65535u ) return false;
	}
	if ( iValue == 0u ) return false;
	*pPort = (uint16)iValue;
	return true;
}

XXAPI uint16 xrtUrlDefaultPort(xrtstrview tScheme)
{
	if ( __xrtUrlViewEqNoCase(tScheme, "http") ) return 80u;
	if ( __xrtUrlViewEqNoCase(tScheme, "ws") ) return 80u;
	if ( __xrtUrlViewEqNoCase(tScheme, "https") ) return 443u;
	if ( __xrtUrlViewEqNoCase(tScheme, "wss") ) return 443u;
	return 0u;
}

XXAPI bool xrtUrlIsSecureScheme(xrtstrview tScheme)
{
	return __xrtUrlViewEqNoCase(tScheme, "https") || __xrtUrlViewEqNoCase(tScheme, "wss");
}

static bool __xrtUrlHostNeedsBrackets(xrtstrview tHost)
{
	size_t i;
	for ( i = 0u; i < tHost.iLen; ++i ) {
		if ( tHost.sPtr[i] == ':' ) return true;
	}
	return false;
}

static bool __xrtUrlAppendBytes(char* sOut, size_t iOutCap, size_t* pOffset, const char* sText, size_t iLen)
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

static bool __xrtUrlAppendPort(char* sOut, size_t iOutCap, size_t* pOffset, uint16 iPort)
{
	char sBuf[8];
	int iLen = snprintf(sBuf, sizeof(sBuf), "%u", (unsigned)iPort);
	if ( iLen <= 0 || (size_t)iLen >= sizeof(sBuf) ) return false;
	return __xrtUrlAppendBytes(sOut, iOutCap, pOffset, sBuf, (size_t)iLen);
}

XXAPI bool xrtUrlIsDefaultPort(const xrturlview* pURL)
{
	uint16 iDefaultPort;
	if ( pURL == NULL || xrtStrViewIsEmpty(pURL->tScheme) || pURL->iPort == 0u ) return false;
	iDefaultPort = xrtUrlDefaultPort(pURL->tScheme);
	return iDefaultPort != 0u && iDefaultPort == pURL->iPort;
}

XXAPI bool xrtUrlViewIsScheme(const xrturlview* pURL, const char* sScheme)
{
	return pURL != NULL && sScheme != NULL && !xrtStrViewIsEmpty(pURL->tScheme) && __xrtUrlViewEqNoCase(pURL->tScheme, sScheme);
}

XXAPI bool xrtUrlViewMatchesScheme2(const xrturlview* pURL, const char* sSchemeA, const char* sSchemeB)
{
	return xrtUrlViewIsScheme(pURL, sSchemeA) || xrtUrlViewIsScheme(pURL, sSchemeB);
}

XXAPI bool xrtUrlViewCopySchemeTo(const xrturlview* pURL, char* sOut, size_t iOutCap)
{
	if ( pURL == NULL ) return false;
	return xrtStrViewCopyTo(pURL->tScheme, sOut, iOutCap);
}

XXAPI bool xrtUrlViewCopyAuthorityTo(const xrturlview* pURL, char* sOut, size_t iOutCap)
{
	if ( pURL == NULL ) return false;
	return xrtStrViewCopyTo(pURL->tAuthority, sOut, iOutCap);
}

XXAPI bool xrtUrlViewCopyPathTo(const xrturlview* pURL, char* sOut, size_t iOutCap)
{
	if ( pURL == NULL ) return false;
	return xrtStrViewCopyTo(pURL->tPath, sOut, iOutCap);
}

XXAPI bool xrtUrlViewCopyQueryTo(const xrturlview* pURL, char* sOut, size_t iOutCap)
{
	if ( pURL == NULL ) return false;
	return xrtStrViewCopyTo(pURL->tQuery, sOut, iOutCap);
}

XXAPI bool xrtUrlViewCopyFragmentTo(const xrturlview* pURL, char* sOut, size_t iOutCap)
{
	if ( pURL == NULL ) return false;
	return xrtStrViewCopyTo(pURL->tFragment, sOut, iOutCap);
}

XXAPI bool xrtUrlParseAuthorityN(const char* sText, size_t iLen, xrturlview* pOut)
{
	size_t iAt = (size_t)-1;
	size_t iHostStart = 0u;
	size_t iColon = (size_t)-1;
	size_t i;
	if ( pOut == NULL || sText == NULL || iLen == 0u ) return false;
	memset(pOut, 0, sizeof(xrturlview));
	if ( !__xrtUrlValidateNoCtlOrSpace(sText, iLen) ) return false;
	for ( i = 0u; i < iLen; ++i ) {
		if ( sText[i] == '@' ) iAt = i;
	}
	if ( iAt != (size_t)-1 ) {
		if ( iAt == 0u || iAt + 1u >= iLen ) return false;
		pOut->tUserInfo = xrtStrView(sText, iAt);
		pOut->iFlags |= XRT_URL_F_HAS_USERINFO;
		iHostStart = iAt + 1u;
	}
	if ( sText[iHostStart] == '[' ) {
		size_t iClose = (size_t)-1;
		for ( i = iHostStart + 1u; i < iLen; ++i ) {
			if ( sText[i] == ']' ) {
				iClose = i;
				break;
			}
		}
		if ( iClose == (size_t)-1 || iClose == iHostStart + 1u ) return false;
		pOut->tHost = xrtStrView(sText + iHostStart + 1u, iClose - iHostStart - 1u);
		pOut->iFlags |= XRT_URL_F_HAS_HOST;
		if ( iClose + 1u < iLen ) {
			if ( sText[iClose + 1u] != ':' ) return false;
			if ( !__xrtUrlParsePort(sText + iClose + 2u, iLen - iClose - 2u, &pOut->iPort) ) return false;
			pOut->iFlags |= XRT_URL_F_HAS_PORT;
		}
	} else {
		size_t iFirstColon = (size_t)-1;
		size_t iLastColon = (size_t)-1;
		for ( i = iHostStart; i < iLen; ++i ) {
			if ( sText[i] == ':' ) {
				if ( iFirstColon == (size_t)-1 ) iFirstColon = i;
				iLastColon = i;
			}
		}
		if ( iFirstColon != (size_t)-1 && iFirstColon != iLastColon ) return false;
		if ( iLastColon != (size_t)-1 ) iColon = iLastColon;
		if ( iColon != (size_t)-1 ) {
			if ( iColon == iHostStart || iColon + 1u >= iLen ) return false;
			pOut->tHost = xrtStrView(sText + iHostStart, iColon - iHostStart);
			if ( !__xrtUrlParsePort(sText + iColon + 1u, iLen - iColon - 1u, &pOut->iPort) ) return false;
			pOut->iFlags |= XRT_URL_F_HAS_PORT;
		} else {
			pOut->tHost = xrtStrView(sText + iHostStart, iLen - iHostStart);
		}
		if ( xrtStrViewIsEmpty(pOut->tHost) ) return false;
		pOut->iFlags |= XRT_URL_F_HAS_HOST;
	}
	pOut->tAuthority = xrtStrView(sText, iLen);
	pOut->iFlags |= XRT_URL_F_HAS_AUTHORITY;
	return true;
}

XXAPI bool xrtUrlParseAuthority(const char* sText, xrturlview* pOut)
{
	if ( sText == NULL ) return false;
	return xrtUrlParseAuthorityN(sText, strlen(sText), pOut);
}

XXAPI bool xrtUrlParseTargetN(const char* sText, size_t iLen, xrturlview* pOut)
{
	size_t iQuery = (size_t)-1;
	size_t iFrag = (size_t)-1;
	size_t i;
	if ( pOut == NULL || sText == NULL ) return false;
	memset(pOut, 0, sizeof(xrturlview));
	if ( iLen == 0u ) return false;
	if ( !__xrtUrlValidateNoCtlOrSpace(sText, iLen) ) return false;
	for ( i = 0u; i < iLen; ++i ) {
		if ( sText[i] == '#' ) {
			iFrag = i;
			break;
		}
		if ( sText[i] == '?' && iQuery == (size_t)-1 ) iQuery = i;
	}
	pOut->iFlags = XRT_URL_F_TARGET_ONLY;
	if ( iQuery != (size_t)-1 ) pOut->iFlags |= XRT_URL_F_HAS_QUERY;
	if ( iFrag != (size_t)-1 ) pOut->iFlags |= XRT_URL_F_HAS_FRAGMENT;
	if ( iQuery != (size_t)-1 ) {
		pOut->tPath = xrtStrView(sText, iQuery);
		pOut->tQuery = xrtStrView(sText + iQuery + 1u, ((iFrag != (size_t)-1) ? iFrag : iLen) - iQuery - 1u);
	} else {
		pOut->tPath = xrtStrView(sText, ((iFrag != (size_t)-1) ? iFrag : iLen));
	}
	if ( pOut->tPath.iLen > 0u ) pOut->iFlags |= XRT_URL_F_HAS_PATH;
	if ( iFrag != (size_t)-1 && iFrag + 1u <= iLen ) {
		pOut->tFragment = xrtStrView(sText + iFrag + 1u, iLen - iFrag - 1u);
	}
	pOut->tTarget = xrtStrView(sText, ((iFrag != (size_t)-1) ? iFrag : iLen));
	return true;
}

XXAPI bool xrtUrlParseTarget(const char* sText, xrturlview* pOut)
{
	if ( sText == NULL ) return false;
	return xrtUrlParseTargetN(sText, strlen(sText), pOut);
}

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
	if ( pOut == NULL || sText == NULL ) return false;
	memset(pOut, 0, sizeof(xrturlview));
	if ( iLen == 0u ) return false;
	for ( i = 0u; i < iLen; ++i ) {
		char ch = sText[i];
		if ( ch == ':' ) {
			iSchemeEnd = i;
			break;
		}
		if ( ch == '/' || ch == '?' || ch == '#' ) break;
		if ( !__xrtUrlIsSchemeChar(ch, i == 0u) ) {
			iSchemeEnd = (size_t)-1;
			break;
		}
	}
	if ( iSchemeEnd == (size_t)-1 || iSchemeEnd + 2u >= iLen || sText[iSchemeEnd + 1u] != '/' || sText[iSchemeEnd + 2u] != '/' ) {
		return xrtUrlParseTargetN(sText, iLen, pOut);
	}
	if ( !__xrtUrlValidateNoCtlOrSpace(sText, iLen) ) return false;
	pOut->tScheme = xrtStrView(sText, iSchemeEnd);
	pOut->iFlags |= XRT_URL_F_ABSOLUTE;
	if ( xrtUrlIsSecureScheme(pOut->tScheme) ) pOut->iFlags |= XRT_URL_F_SECURE;
	iPos = iSchemeEnd + 3u;
	iPathStart = iPos;
	while ( iPathStart < iLen && sText[iPathStart] != '/' && sText[iPathStart] != '?' && sText[iPathStart] != '#' ) iPathStart++;
	if ( iPathStart == iPos ) return false;
	if ( !xrtUrlParseAuthorityN(sText + iPos, iPathStart - iPos, &tAuthority) ) return false;
	pOut->tAuthority = tAuthority.tAuthority;
	pOut->tUserInfo = tAuthority.tUserInfo;
	pOut->tHost = tAuthority.tHost;
	pOut->iPort = tAuthority.iPort;
	pOut->iFlags |= tAuthority.iFlags & (XRT_URL_F_HAS_AUTHORITY | XRT_URL_F_HAS_USERINFO | XRT_URL_F_HAS_HOST | XRT_URL_F_HAS_PORT);
	if ( (pOut->iFlags & XRT_URL_F_HAS_PORT) == 0u ) {
		pOut->iPort = xrtUrlDefaultPort(pOut->tScheme);
	}
	for ( i = iPathStart; i < iLen; ++i ) {
		if ( sText[i] == '#' ) {
			iFrag = i;
			break;
		}
		if ( sText[i] == '?' && iQuery == (size_t)-1 ) iQuery = i;
	}
	iPathEnd = (iQuery != (size_t)-1) ? iQuery : ((iFrag != (size_t)-1) ? iFrag : iLen);
	pOut->tPath = xrtStrView(sText + iPathStart, iPathEnd - iPathStart);
	if ( pOut->tPath.iLen > 0u ) pOut->iFlags |= XRT_URL_F_HAS_PATH;
	if ( iQuery != (size_t)-1 ) {
		size_t iQueryEnd = (iFrag != (size_t)-1) ? iFrag : iLen;
		pOut->tQuery = xrtStrView(sText + iQuery + 1u, iQueryEnd - iQuery - 1u);
		pOut->iFlags |= XRT_URL_F_HAS_QUERY;
	}
	if ( iFrag != (size_t)-1 ) {
		pOut->tFragment = xrtStrView(sText + iFrag + 1u, iLen - iFrag - 1u);
		pOut->iFlags |= XRT_URL_F_HAS_FRAGMENT;
	}
	pOut->tTarget = xrtStrView(sText + iPathStart, ((iFrag != (size_t)-1) ? iFrag : iLen) - iPathStart);
	return true;
}

XXAPI bool xrtUrlParseView(const char* sText, xrturlview* pOut)
{
	if ( sText == NULL ) return false;
	return xrtUrlParseViewN(sText, strlen(sText), pOut);
}

XXAPI bool xrtUrlViewCopyHostTo(const xrturlview* pURL, char* sOut, size_t iOutCap)
{
	if ( pURL == NULL || xrtStrViewIsEmpty(pURL->tHost) ) return false;
	return xrtStrViewCopyTo(pURL->tHost, sOut, iOutCap);
}

XXAPI bool xrtUrlViewCopyTargetTo(const xrturlview* pURL, char* sOut, size_t iOutCap)
{
	if ( pURL == NULL || sOut == NULL || iOutCap == 0u ) return false;
	if ( xrtStrViewIsEmpty(pURL->tTarget) ) {
		if ( iOutCap < 2u ) return false;
		sOut[0] = '/';
		sOut[1] = '\0';
		return true;
	}
	if ( pURL->tTarget.sPtr[0] == '?' ) {
		if ( pURL->tTarget.iLen + 2u > iOutCap ) return false;
		sOut[0] = '/';
		memcpy(sOut + 1u, pURL->tTarget.sPtr, pURL->tTarget.iLen);
		sOut[pURL->tTarget.iLen + 1u] = '\0';
		return true;
	}
	return xrtStrViewCopyTo(pURL->tTarget, sOut, iOutCap);
}

XXAPI bool xrtUrlMakeHostHeader(const xrturlview* pURL, char* sOut, size_t iOutCap)
{
	bool bDefaultPort;
	if ( sOut == NULL || iOutCap == 0u || pURL == NULL || xrtStrViewIsEmpty(pURL->tHost) ) return false;
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

XXAPI bool xrtUrlMakeHostHeaderFixed(const char* sScheme, const char* sHost, uint16 iPort, char* sOut, size_t iOutCap)
{
	xrturlview tURL;
	if ( sScheme == NULL || sHost == NULL || sOut == NULL || iOutCap == 0u ) return false;
	memset(&tURL, 0, sizeof(tURL));
	tURL.tScheme = xrtStrView(sScheme, strlen(sScheme));
	tURL.tHost = xrtStrView(sHost, strlen(sHost));
	tURL.iPort = iPort;
	tURL.iFlags = XRT_URL_F_ABSOLUTE | XRT_URL_F_HAS_HOST;
	if ( iPort != 0u ) tURL.iFlags |= XRT_URL_F_HAS_PORT;
	if ( xrtUrlIsSecureScheme(tURL.tScheme) ) tURL.iFlags |= XRT_URL_F_SECURE;
	return xrtUrlMakeHostHeader(&tURL, sOut, iOutCap);
}

static bool __xrtUrlGetLastSegment(const char* sPath, size_t iLen, size_t* pStart, size_t* pLen)
{
	size_t i;
	if ( pStart ) *pStart = 0u;
	if ( pLen ) *pLen = 0u;
	if ( sPath == NULL || iLen == 0u ) return false;
	i = iLen;
	while ( i > 0u && sPath[i - 1u] == '/' ) i--;
	if ( i == 0u ) return false;
	{
		size_t iEnd = i;
		while ( i > 0u && sPath[i - 1u] != '/' ) i--;
		if ( pStart ) *pStart = i;
		if ( pLen ) *pLen = iEnd - i;
		return iEnd > i;
	}
}

static bool __xrtUrlAppendSegment(char* sOut, size_t iOutCap, size_t* pOffset, const char* sSeg, size_t iSegLen)
{
	if ( sOut == NULL || pOffset == NULL || (sSeg == NULL && iSegLen != 0u) ) return false;
	if ( *pOffset > 0u && sOut[*pOffset - 1u] != '/' ) {
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, pOffset, "/", 1u) ) return false;
	}
	return __xrtUrlAppendBytes(sOut, iOutCap, pOffset, sSeg, iSegLen);
}

static bool __xrtUrlAppendDotDotSegment(char* sOut, size_t iOutCap, size_t* pOffset)
{
	return __xrtUrlAppendSegment(sOut, iOutCap, pOffset, "..", 2u);
}

static bool __xrtUrlPopLastSegment(char* sOut, size_t iOutCap, size_t* pOffset, bool bAbsolute)
{
	size_t iStart = 0u;
	size_t iLen = 0u;
	(void)iOutCap;
	if ( sOut == NULL || pOffset == NULL ) return false;
	if ( *pOffset == 0u ) return false;
	if ( bAbsolute && *pOffset == 1u && sOut[0] == '/' ) return false;
	if ( !__xrtUrlGetLastSegment(sOut, *pOffset, &iStart, &iLen) ) return false;
	if ( !bAbsolute && iLen == 2u && memcmp(sOut + iStart, "..", 2u) == 0 ) return false;
	*pOffset = iStart;
	if ( *pOffset > 0u && sOut[*pOffset - 1u] == '/' && !(bAbsolute && *pOffset == 1u) ) (*pOffset)--;
	sOut[*pOffset] = '\0';
	if ( bAbsolute && *pOffset == 0u ) {
		sOut[0] = '/';
		sOut[1] = '\0';
		*pOffset = 1u;
	}
	return true;
}

XXAPI bool xrtUrlNormalizePathTo(const char* sPath, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	bool bAbsolute;
	bool bTrailingSlash;
	size_t i = 0u;
	size_t iOff = 0u;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sPath == NULL || sOut == NULL || iOutCap == 0u ) return false;
	if ( !__xrtUrlValidateNoCtlOrSpace(sPath, iLen) ) return false;
	sOut[0] = '\0';
	bAbsolute = iLen > 0u && sPath[0] == '/';
	bTrailingSlash = iLen > 0u && sPath[iLen - 1u] == '/';
	if ( bAbsolute ) {
		sOut[0] = '/';
		sOut[1] = '\0';
		iOff = 1u;
		while ( i < iLen && sPath[i] == '/' ) i++;
	}
	while ( i < iLen ) {
		size_t iSegStart = i;
		size_t iSegLen;
		while ( i < iLen && sPath[i] != '/' ) i++;
		iSegLen = i - iSegStart;
		if ( iSegLen == 0u || (iSegLen == 1u && sPath[iSegStart] == '.') ) {
		} else if ( iSegLen == 2u && sPath[iSegStart] == '.' && sPath[iSegStart + 1u] == '.' ) {
			if ( !__xrtUrlPopLastSegment(sOut, iOutCap, &iOff, bAbsolute) ) {
				if ( !bAbsolute ) {
					if ( !__xrtUrlAppendDotDotSegment(sOut, iOutCap, &iOff) ) return false;
				}
			}
		} else {
			if ( !__xrtUrlAppendSegment(sOut, iOutCap, &iOff, sPath + iSegStart, iSegLen) ) return false;
		}
		while ( i < iLen && sPath[i] == '/' ) i++;
	}
	if ( bTrailingSlash ) {
		if ( iOff == 0u ) {
			if ( bAbsolute ) {
				if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "/", 1u) ) return false;
			} else {
				if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "./", 2u) ) return false;
			}
		} else if ( sOut[iOff - 1u] != '/' ) {
			if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "/", 1u) ) return false;
		}
	}
	if ( bAbsolute && iOff == 0u ) {
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "/", 1u) ) return false;
	}
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}

XXAPI bool xrtUrlBuildTarget(const xrturlview* pURL, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sOut == NULL || iOutCap == 0u || pURL == NULL ) return false;
	sOut[0] = '\0';
	if ( !xrtStrViewIsEmpty(pURL->tPath) ) {
		if ( pURL->tPath.sPtr[0] != '/' ) return false;
		if ( !__xrtUrlValidateNoCtlOrSpace(pURL->tPath.sPtr, pURL->tPath.iLen) ) return false;
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, pURL->tPath.sPtr, pURL->tPath.iLen) ) return false;
	} else {
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "/", 1u) ) return false;
	}
	if ( !xrtStrViewIsEmpty(pURL->tQuery) ) {
		if ( !__xrtUrlValidateNoCtlOrSpace(pURL->tQuery.sPtr, pURL->tQuery.iLen) ) return false;
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "?", 1u) ) return false;
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, pURL->tQuery.sPtr, pURL->tQuery.iLen) ) return false;
	}
	if ( !xrtStrViewIsEmpty(pURL->tFragment) ) {
		if ( !__xrtUrlValidateNoCtlOrSpace(pURL->tFragment.sPtr, pURL->tFragment.iLen) ) return false;
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "#", 1u) ) return false;
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, pURL->tFragment.sPtr, pURL->tFragment.iLen) ) return false;
	}
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}

XXAPI bool xrtUrlBuildAuthority(const xrturlview* pURL, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	bool bNeedPort;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sOut == NULL || iOutCap == 0u || pURL == NULL || xrtStrViewIsEmpty(pURL->tHost) ) return false;
	sOut[0] = '\0';
	if ( !xrtStrViewIsEmpty(pURL->tUserInfo) ) {
		if ( !__xrtUrlValidateNoCtlOrSpace(pURL->tUserInfo.sPtr, pURL->tUserInfo.iLen) ) return false;
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, pURL->tUserInfo.sPtr, pURL->tUserInfo.iLen) ) return false;
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "@", 1u) ) return false;
	}
	if ( __xrtUrlHostNeedsBrackets(pURL->tHost) ) {
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "[", 1u) ) return false;
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, pURL->tHost.sPtr, pURL->tHost.iLen) ) return false;
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "]", 1u) ) return false;
	} else {
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, pURL->tHost.sPtr, pURL->tHost.iLen) ) return false;
	}
	bNeedPort = (pURL->iFlags & XRT_URL_F_HAS_PORT) != 0u || (pURL->iPort != 0u && !xrtUrlIsDefaultPort(pURL));
	if ( bNeedPort && pURL->iPort != 0u ) {
		if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, ":", 1u) ) return false;
		if ( !__xrtUrlAppendPort(sOut, iOutCap, &iOff, pURL->iPort) ) return false;
	}
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}

XXAPI bool xrtUrlBuild(const xrturlview* pURL, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	size_t iOff = 0u;
	char sTarget[4096];
	size_t iTargetLen = 0u;
	if ( pOutLen ) *pOutLen = 0u;
	if ( sOut == NULL || iOutCap == 0u || pURL == NULL ) return false;
	sOut[0] = '\0';
	if ( (pURL->iFlags & XRT_URL_F_ABSOLUTE) == 0u ) {
		return xrtUrlBuildTarget(pURL, sOut, iOutCap, pOutLen);
	}
	if ( xrtStrViewIsEmpty(pURL->tScheme) || xrtStrViewIsEmpty(pURL->tHost) ) return false;
	if ( !__xrtUrlValidateNoCtlOrSpace(pURL->tScheme.sPtr, pURL->tScheme.iLen) ) return false;
	if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, pURL->tScheme.sPtr, pURL->tScheme.iLen) ) return false;
	if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, "://", 3u) ) return false;
	if ( !xrtUrlBuildAuthority(pURL, sOut + iOff, iOutCap - iOff, &iTargetLen) ) return false;
	iOff += iTargetLen;
	if ( !xrtUrlBuildTarget(pURL, sTarget, sizeof(sTarget), &iTargetLen) ) return false;
	if ( !__xrtUrlAppendBytes(sOut, iOutCap, &iOff, sTarget, iTargetLen) ) return false;
	if ( pOutLen ) *pOutLen = iOff;
	return true;
}

XXAPI bool xrtUrlResolveTo(const xrturlview* pBase, const char* sRef, size_t iRefLen, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	xrturlview tRef;
	xrturlview tOut;
	char sMergedPath[4096];
	char sNormalizedPath[4096];
	size_t iNormLen = 0u;
	size_t iMergeLen = 0u;
	if ( pOutLen ) *pOutLen = 0u;
	if ( pBase == NULL || sOut == NULL || iOutCap == 0u || sRef == NULL ) return false;
	if ( (pBase->iFlags & XRT_URL_F_ABSOLUTE) == 0u || xrtStrViewIsEmpty(pBase->tScheme) || xrtStrViewIsEmpty(pBase->tHost) ) return false;
	if ( iRefLen == 0u ) return xrtUrlBuild(pBase, sOut, iOutCap, pOutLen);
	if ( iRefLen >= 2u && sRef[0] == '/' && sRef[1] == '/' ) {
		size_t iAuthEnd = 2u;
		xrturlview tAuthority;
		memset(&tOut, 0, sizeof(tOut));
		while ( iAuthEnd < iRefLen && sRef[iAuthEnd] != '/' && sRef[iAuthEnd] != '?' && sRef[iAuthEnd] != '#' ) iAuthEnd++;
		if ( !xrtUrlParseAuthorityN(sRef + 2u, iAuthEnd - 2u, &tAuthority) ) return false;
		tOut = *pBase;
		tOut.tAuthority = tAuthority.tAuthority;
		tOut.tUserInfo = tAuthority.tUserInfo;
		tOut.tHost = tAuthority.tHost;
		tOut.iPort = (tAuthority.iFlags & XRT_URL_F_HAS_PORT) ? tAuthority.iPort : xrtUrlDefaultPort(pBase->tScheme);
		tOut.iFlags &= ~(XRT_URL_F_HAS_USERINFO | XRT_URL_F_HAS_PORT | XRT_URL_F_HAS_PATH | XRT_URL_F_HAS_QUERY | XRT_URL_F_HAS_FRAGMENT);
		tOut.iFlags |= XRT_URL_F_ABSOLUTE | XRT_URL_F_HAS_AUTHORITY | XRT_URL_F_HAS_HOST | (pBase->iFlags & XRT_URL_F_SECURE);
		tOut.iFlags |= tAuthority.iFlags & (XRT_URL_F_HAS_USERINFO | XRT_URL_F_HAS_PORT);
		if ( iAuthEnd < iRefLen ) {
			if ( !xrtUrlParseTargetN(sRef + iAuthEnd, iRefLen - iAuthEnd, &tRef) ) return false;
			tOut.tPath = tRef.tPath;
			tOut.tQuery = tRef.tQuery;
			tOut.tFragment = tRef.tFragment;
			tOut.tTarget = tRef.tTarget;
			tOut.iFlags |= tRef.iFlags & (XRT_URL_F_HAS_PATH | XRT_URL_F_HAS_QUERY | XRT_URL_F_HAS_FRAGMENT);
		}
		return xrtUrlBuild(&tOut, sOut, iOutCap, pOutLen);
	}
	if ( !xrtUrlParseViewN(sRef, iRefLen, &tRef) ) return false;
	if ( (tRef.iFlags & XRT_URL_F_ABSOLUTE) != 0u ) return xrtUrlBuild(&tRef, sOut, iOutCap, pOutLen);
	tOut = *pBase;
	tOut.iFlags &= ~(XRT_URL_F_HAS_PATH | XRT_URL_F_HAS_QUERY | XRT_URL_F_HAS_FRAGMENT);
	tOut.tQuery = xrtStrView(NULL, 0u);
	tOut.tFragment = xrtStrView(NULL, 0u);
	if ( (tRef.iFlags & XRT_URL_F_HAS_PATH) != 0u && !xrtStrViewIsEmpty(tRef.tPath) ) {
		if ( tRef.tPath.sPtr[0] == '/' ) {
			if ( !xrtUrlNormalizePathTo(tRef.tPath.sPtr, tRef.tPath.iLen, sNormalizedPath, sizeof(sNormalizedPath), &iNormLen) ) return false;
		} else {
			size_t iBaseDirLen = 0u;
			if ( !xrtStrViewIsEmpty(pBase->tPath) ) {
				size_t i;
				for ( i = pBase->tPath.iLen; i > 0u; --i ) {
					if ( pBase->tPath.sPtr[i - 1u] == '/' ) {
						iBaseDirLen = i;
						break;
					}
				}
				if ( iBaseDirLen == 0u ) iBaseDirLen = 1u;
			} else {
				iBaseDirLen = 1u;
			}
			if ( iBaseDirLen == 1u && (xrtStrViewIsEmpty(pBase->tPath) || pBase->tPath.sPtr[0] != '/') ) {
				if ( !__xrtUrlAppendBytes(sMergedPath, sizeof(sMergedPath), &iMergeLen, "/", 1u) ) return false;
			} else {
				if ( !__xrtUrlAppendBytes(sMergedPath, sizeof(sMergedPath), &iMergeLen, pBase->tPath.sPtr, iBaseDirLen) ) return false;
			}
			if ( !__xrtUrlAppendBytes(sMergedPath, sizeof(sMergedPath), &iMergeLen, tRef.tPath.sPtr, tRef.tPath.iLen) ) return false;
			if ( !xrtUrlNormalizePathTo(sMergedPath, iMergeLen, sNormalizedPath, sizeof(sNormalizedPath), &iNormLen) ) return false;
		}
		tOut.tPath = xrtStrView(sNormalizedPath, iNormLen);
		tOut.iFlags |= XRT_URL_F_HAS_PATH;
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
	tOut.tPath = pBase->tPath;
	if ( !xrtStrViewIsEmpty(pBase->tPath) ) tOut.iFlags |= XRT_URL_F_HAS_PATH;
	if ( (tRef.iFlags & XRT_URL_F_HAS_QUERY) != 0u ) {
		tOut.tQuery = tRef.tQuery;
		tOut.iFlags |= XRT_URL_F_HAS_QUERY;
	} else if ( (pBase->iFlags & XRT_URL_F_HAS_QUERY) != 0u ) {
		tOut.tQuery = pBase->tQuery;
		tOut.iFlags |= XRT_URL_F_HAS_QUERY;
	}
	if ( (tRef.iFlags & XRT_URL_F_HAS_FRAGMENT) != 0u ) {
		tOut.tFragment = tRef.tFragment;
		tOut.iFlags |= XRT_URL_F_HAS_FRAGMENT;
	}
	return xrtUrlBuild(&tOut, sOut, iOutCap, pOutLen);
}

XXAPI bool xrtUrlResolve(const xrturlview* pBase, const char* sRef, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	if ( sRef == NULL ) return false;
	return xrtUrlResolveTo(pBase, sRef, strlen(sRef), sOut, iOutCap, pOutLen);
}

XXAPI bool xrtQueryNextN(const char* sQuery, size_t iLen, size_t* pOffset, xrtquerypair* pOut)
{
	size_t iCur;
	size_t iAmp;
	size_t iEq = (size_t)-1;
	size_t i;
	if ( sQuery == NULL || pOffset == NULL || pOut == NULL ) return false;
	iCur = *pOffset;
	if ( iCur == 0u && iLen > 0u && sQuery[0] == '?' ) iCur = 1u;
	while ( iCur < iLen && sQuery[iCur] == '&' ) iCur++;
	if ( iCur >= iLen ) {
		*pOffset = iCur;
		return false;
	}
	iAmp = iCur;
	while ( iAmp < iLen && sQuery[iAmp] != '&' ) {
		if ( sQuery[iAmp] == '=' && iEq == (size_t)-1 ) iEq = iAmp;
		iAmp++;
	}
	memset(pOut, 0, sizeof(xrtquerypair));
	if ( iEq != (size_t)-1 ) {
		pOut->tKey = xrtStrView(sQuery + iCur, iEq - iCur);
		pOut->tValue = xrtStrView(sQuery + iEq + 1u, iAmp - iEq - 1u);
		pOut->iFlags |= XRT_QUERY_F_HAS_VALUE;
	} else {
		pOut->tKey = xrtStrView(sQuery + iCur, iAmp - iCur);
	}
	for ( i = 0u; i < pOut->tKey.iLen; ++i ) {
		if ( __xrtUrlIsCtlOrSpace(pOut->tKey.sPtr[i]) ) return false;
	}
	*pOffset = (iAmp < iLen) ? (iAmp + 1u) : iAmp;
	return true;
}

XXAPI bool xrtQueryNext(const char* sQuery, size_t* pOffset, xrtquerypair* pOut)
{
	if ( sQuery == NULL ) return false;
	return xrtQueryNextN(sQuery, strlen(sQuery), pOffset, pOut);
}

XXAPI size_t xrtQueryCountN(const char* sQuery, size_t iLen)
{
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtquerypair tPair;
	if ( sQuery == NULL ) return 0u;
	while ( xrtQueryNextN(sQuery, iLen, &iOffset, &tPair) ) iCount++;
	return iCount;
}

XXAPI size_t xrtQueryCount(const char* sQuery)
{
	if ( sQuery == NULL ) return 0u;
	return xrtQueryCountN(sQuery, strlen(sQuery));
}

XXAPI bool xrtQueryFindN(const char* sQuery, size_t iLen, const char* sKey, size_t iKeyLen, xrtquerypair* pOut)
{
	size_t iOffset = 0u;
	xrtquerypair tPair;
	if ( sQuery == NULL || sKey == NULL ) return false;
	while ( xrtQueryNextN(sQuery, iLen, &iOffset, &tPair) ) {
		if ( tPair.tKey.iLen == iKeyLen && memcmp(tPair.tKey.sPtr, sKey, iKeyLen) == 0 ) {
			if ( pOut ) *pOut = tPair;
			return true;
		}
	}
	return false;
}

XXAPI bool xrtQueryFind(const char* sQuery, const char* sKey, xrtquerypair* pOut)
{
	if ( sQuery == NULL || sKey == NULL ) return false;
	return xrtQueryFindN(sQuery, strlen(sQuery), sKey, strlen(sKey), pOut);
}

XXAPI bool xrtQueryParseToN(const char* sQuery, size_t iLen, xrtquerypair* pOut, size_t iCap, size_t* pCount)
{
	size_t iOffset = 0u;
	size_t iCount = 0u;
	xrtquerypair tPair;
	if ( pCount ) *pCount = 0u;
	if ( sQuery == NULL || (pOut == NULL && iCap != 0u) ) return false;
	while ( xrtQueryNextN(sQuery, iLen, &iOffset, &tPair) ) {
		if ( iCount >= iCap ) return false;
		if ( pOut ) pOut[iCount] = tPair;
		++iCount;
	}
	if ( pCount ) *pCount = iCount;
	return true;
}

XXAPI bool xrtQueryParseTo(const char* sQuery, xrtquerypair* pOut, size_t iCap, size_t* pCount)
{
	if ( sQuery == NULL ) return false;
	return xrtQueryParseToN(sQuery, strlen(sQuery), pOut, iCap, pCount);
}

static int __xrtUrlHexValue(char ch)
{
	if ( ch >= '0' && ch <= '9' ) return (int)(ch - '0');
	if ( ch >= 'a' && ch <= 'f' ) return 10 + (int)(ch - 'a');
	if ( ch >= 'A' && ch <= 'F' ) return 10 + (int)(ch - 'A');
	return -1;
}

XXAPI bool xrtPercentDecodeTo(const char* sText, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen, bool bPlusAsSpace)
{
	size_t iOut = 0u;
	size_t i;
	if ( sText == NULL || sOut == NULL || iOutCap == 0u ) return false;
	for ( i = 0u; i < iLen; ++i ) {
		char ch = sText[i];
		if ( ch == '%' ) {
			int iHi;
			int iLo;
			if ( i + 2u >= iLen ) return false;
			iHi = __xrtUrlHexValue(sText[i + 1u]);
			iLo = __xrtUrlHexValue(sText[i + 2u]);
			if ( iHi < 0 || iLo < 0 ) return false;
			if ( iOut + 1u >= iOutCap ) return false;
			sOut[iOut++] = (char)((iHi << 4) | iLo);
			i += 2u;
			continue;
		}
		if ( ch == '+' && bPlusAsSpace ) ch = ' ';
		if ( iOut + 1u >= iOutCap ) return false;
		sOut[iOut++] = ch;
	}
	sOut[iOut] = '\0';
	if ( pOutLen ) *pOutLen = iOut;
	return true;
}

XXAPI bool xrtUrlParseFixedTo(const char* sURL, const char* sSchemeA, const char* sSchemeB, bool* pSchemeB, char* sHost, size_t iHostCap, uint16* pPort, char* sTarget, size_t iTargetCap)
{
	xrturlview tURL;
	if ( sURL == NULL || sSchemeA == NULL || sHost == NULL || iHostCap == 0u || sTarget == NULL || iTargetCap == 0u ) return false;
	if ( !xrtUrlParseView(sURL, &tURL) ) return false;
	if ( (tURL.iFlags & XRT_URL_F_ABSOLUTE) == 0u ) return false;
	if ( sSchemeB != NULL ) {
		if ( !xrtUrlViewMatchesScheme2(&tURL, sSchemeA, sSchemeB) ) return false;
	} else if ( !xrtUrlViewIsScheme(&tURL, sSchemeA) ) {
		return false;
	}
	if ( !xrtUrlViewCopyHostTo(&tURL, sHost, iHostCap) ) return false;
	if ( !xrtUrlViewCopyTargetTo(&tURL, sTarget, iTargetCap) ) return false;
	if ( pPort ) *pPort = tURL.iPort;
	if ( pSchemeB ) *pSchemeB = (sSchemeB != NULL) ? xrtUrlViewIsScheme(&tURL, sSchemeB) : false;
	return true;
}

XXAPI bool xrtUrlParse(const char* sURL, xurl pOut)
{
	if ( pOut == NULL || sURL == NULL ) return false;
	memset(pOut, 0, sizeof(xurl_struct));
	if ( !xrtUrlParseFixedTo(sURL, "http", "https", &pOut->bHttps, pOut->sHost, sizeof(pOut->sHost), &pOut->iPort, pOut->sPath, sizeof(pOut->sPath)) ) return false;
	return true;
}

#endif
