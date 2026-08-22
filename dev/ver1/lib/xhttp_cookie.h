#ifndef XRT_XHTTP_COOKIE_H
#define XRT_XHTTP_COOKIE_H

typedef struct __xhttp_cookie_entry {
	char* sName;
	char* sValue;
	char* sDomain;
	char* sPath;
	size_t iNameLen;
	size_t iValueLen;
	size_t iDomainLen;
	size_t iPathLen;
	int64_t iExpiresUnix;
	uint64 iCreationSequence;
	uint64 iLastAccessSequence;
	uint32 iFlags;
	uint8 iSameSite;
	uint8 iPriority;
} __xhttp_cookie_entry;

struct xrt_http_cookie_jar {
	volatile long iRefCount;
	volatile long iLock;
	__xhttp_cookie_entry* pCookie;
	size_t iCount;
	size_t iCap;
	uint64 iSequence;
	xhttpcookiejarconfig tConfig;
};

typedef struct __xhttp_cookie_url {
	char* sHost;
	size_t iHostLen;
	xrtstrview tPath;
	bool bSecure;
	bool bIpHost;
} __xhttp_cookie_url;

typedef struct __xhttp_cookie_parsed {
	xrtstrview tName;
	xrtstrview tValue;
	xrtstrview tDomain;
	xrtstrview tPath;
	xrtstrview tExpires;
	int64_t iMaxAge;
	uint32 iFlags;
	uint8 iSameSite;
	uint8 iPriority;
} __xhttp_cookie_parsed;

#define __XHTTP_COOKIE_PARSED_HAS_DOMAIN  UINT32_C(0x00000001)
#define __XHTTP_COOKIE_PARSED_HAS_PATH    UINT32_C(0x00000002)
#define __XHTTP_COOKIE_PARSED_HAS_EXPIRES UINT32_C(0x00000004)
#define __XHTTP_COOKIE_PARSED_HAS_MAX_AGE UINT32_C(0x00000008)
#define __XHTTP_COOKIE_PARSED_SECURE      UINT32_C(0x00000010)
#define __XHTTP_COOKIE_PARSED_HTTP_ONLY   UINT32_C(0x00000020)
#define __XHTTP_COOKIE_PARSED_PARTITIONED UINT32_C(0x00000040)
#define __XHTTP_COOKIE_PARSED_SAME_PARTY  UINT32_C(0x00000080)


static void __xhttpCookieJarLock(xhttpcookiejar* pJar)
{
	while ( __xnetAtomicCompareExchange32(&pJar->iLock, 1, 0) != 0 ) { __xnetEngineSleepMs(1u); }
}


static void __xhttpCookieJarUnlock(xhttpcookiejar* pJar)
{
	(void)__xnetAtomicExchange32(&pJar->iLock, 0);
}


static int64_t __xhttpCookieNowUnix(int64_t iNowUnix)
{
	if ( iNowUnix > 0 ) { return iNowUnix; }
	{
		time_t iNow = time(NULL);
		return iNow > 0 ? (int64_t)iNow : 1;
	}
}


static bool __xhttpCookieTextEq(const char* sA, size_t iALen, const char* sB, size_t iBLen)
{
	return iALen == iBLen && (iALen == 0u || memcmp(sA, sB, iALen) == 0);
}


static bool __xhttpCookieStartsWith(const xrtstrview* pValue, const char* sPrefix)
{
	size_t iPrefixLen = strlen(sPrefix);
	return pValue && pValue->iLen >= iPrefixLen && memcmp(pValue->sPtr, sPrefix, iPrefixLen) == 0;
}


static bool __xhttpCookieIsIpHost(const char* sHost, size_t iLen)
{
	bool bDigitOrDot = iLen > 0u;
	if ( !sHost || iLen == 0u ) { return false; }
	for ( size_t i = 0u; i < iLen; ++i ) {
		unsigned char ch = (unsigned char)sHost[i];
		if ( ch == ':' ) { return true; }
		if ( (ch < '0' || ch > '9') && ch != '.' ) { bDigitOrDot = false; }
	}
	return bDigitOrDot;
}


static bool __xhttpCookieCanonicalUrlN(const char* sURL, size_t iURLLen, __xhttp_cookie_url* pOut)
{
	xrturlview tURL;
	size_t iHostLen;
	if ( !sURL || !pOut || !xrtUrlParseViewN(sURL, iURLLen, &tURL) ||
		(tURL.iFlags & (XRT_URL_F_ABSOLUTE | XRT_URL_F_HAS_HOST)) !=
			(XRT_URL_F_ABSOLUTE | XRT_URL_F_HAS_HOST) || tURL.tHost.iLen == 0u ) { return false; }
	memset(pOut, 0, sizeof(*pOut));
	if ( __xrtHttpUtilEqNoCaseN(tURL.tScheme.sPtr, tURL.tScheme.iLen, "https", 5u) ||
		__xrtHttpUtilEqNoCaseN(tURL.tScheme.sPtr, tURL.tScheme.iLen, "wss", 3u) ) {
		pOut->bSecure = true;
	} else if ( !__xrtHttpUtilEqNoCaseN(tURL.tScheme.sPtr, tURL.tScheme.iLen, "http", 4u) &&
		!__xrtHttpUtilEqNoCaseN(tURL.tScheme.sPtr, tURL.tScheme.iLen, "ws", 2u) ) {
		return false;
	}
	iHostLen = tURL.tHost.iLen;
	while ( iHostLen > 0u && tURL.tHost.sPtr[iHostLen - 1u] == '.' ) { iHostLen--; }
	if ( iHostLen == 0u ) { return false; }
	pOut->sHost = (char*)XNET_ALLOC(iHostLen + 1u);
	if ( !pOut->sHost ) { return false; }
	for ( size_t i = 0u; i < iHostLen; ++i ) {
		unsigned char ch = (unsigned char)tURL.tHost.sPtr[i];
		if ( ch <= 0x20u || ch >= 0x7fu || ch == '%' || ch == '/' || ch == '\\' ) {
			XNET_FREE(pOut->sHost);
			memset(pOut, 0, sizeof(*pOut));
			return false;
		}
		pOut->sHost[i] = (ch >= 'A' && ch <= 'Z') ? (char)(ch + 32u) : (char)ch;
	}
	pOut->sHost[iHostLen] = '\0';
	pOut->iHostLen = iHostLen;
	pOut->bIpHost = __xhttpCookieIsIpHost(pOut->sHost, iHostLen);
	pOut->tPath = ((tURL.iFlags & XRT_URL_F_HAS_PATH) != 0u && tURL.tPath.iLen > 0u)
		? tURL.tPath : xrtStrView("/", 1u);
	return true;
}


static void __xhttpCookieUrlUnit(__xhttp_cookie_url* pURL)
{
	if ( !pURL ) { return; }
	if ( pURL->sHost ) { XNET_FREE(pURL->sHost); }
	memset(pURL, 0, sizeof(*pURL));
}


static bool __xhttpCookieDomainMatch(const char* sHost, size_t iHostLen, const char* sDomain, size_t iDomainLen)
{
	if ( __xhttpCookieTextEq(sHost, iHostLen, sDomain, iDomainLen) ) { return true; }
	return iHostLen > iDomainLen && sHost[iHostLen - iDomainLen - 1u] == '.' &&
		memcmp(sHost + iHostLen - iDomainLen, sDomain, iDomainLen) == 0;
}


static bool __xhttpCookiePathMatch(xrtstrview tRequestPath, const char* sCookiePath, size_t iCookiePathLen)
{
	if ( tRequestPath.iLen < iCookiePathLen || memcmp(tRequestPath.sPtr, sCookiePath, iCookiePathLen) != 0 ) {
		return false;
	}
	if ( tRequestPath.iLen == iCookiePathLen || sCookiePath[iCookiePathLen - 1u] == '/' ) { return true; }
	return tRequestPath.sPtr[iCookiePathLen] == '/';
}


static xrtstrview __xhttpCookieDefaultPath(xrtstrview tRequestPath)
{
	size_t iLastSlash = 0u;
	if ( tRequestPath.iLen == 0u || tRequestPath.sPtr[0] != '/' ) { return xrtStrView("/", 1u); }
	for ( size_t i = 1u; i < tRequestPath.iLen; ++i ) {
		if ( tRequestPath.sPtr[i] == '/' ) { iLastSlash = i; }
	}
	return iLastSlash == 0u ? xrtStrView("/", 1u) : xrtStrView(tRequestPath.sPtr, iLastSlash);
}


static bool __xhttpCookieParseSigned64(xrtstrview tValue, int64_t* pOut)
{
	bool bNegative = false;
	uint64 iValue = 0u;
	size_t i = 0u;
	if ( !pOut || tValue.iLen == 0u ) { return false; }
	if ( tValue.sPtr[0] == '-' ) { bNegative = true; i = 1u; }
	if ( i == tValue.iLen ) { return false; }
	for ( ; i < tValue.iLen; ++i ) {
		unsigned char ch = (unsigned char)tValue.sPtr[i];
		if ( ch < '0' || ch > '9' ) { return false; }
		if ( iValue > (UINT64_MAX - (uint64)(ch - '0')) / 10u ) { iValue = UINT64_MAX; }
		else if ( iValue != UINT64_MAX ) { iValue = iValue * 10u + (uint64)(ch - '0'); }
	}
	if ( bNegative ) { *pOut = iValue > (uint64)INT64_MAX ? INT64_MIN : -(int64_t)iValue; }
	else { *pOut = iValue > (uint64)INT64_MAX ? INT64_MAX : (int64_t)iValue; }
	return true;
}


static bool __xhttpCookieValueValid(xrtstrview tValue)
{
	size_t iBegin = 0u;
	size_t iEnd = tValue.iLen;
	if ( iEnd >= 2u && tValue.sPtr[0] == '"' && tValue.sPtr[iEnd - 1u] == '"' ) {
		iBegin = 1u;
		iEnd--;
	}
	for ( size_t i = iBegin; i < iEnd; ++i ) {
		if ( !__xrtHttpUtilIsCookieOctet(tValue.sPtr[i]) ) { return false; }
	}
	return (iBegin != 0u) || (tValue.iLen == 0u || (tValue.sPtr[0] != '"' && tValue.sPtr[tValue.iLen - 1u] != '"'));
}


static bool __xhttpCookieParseSetCookieN(const char* sText, size_t iLen, __xhttp_cookie_parsed* pOut)
{
	size_t iPairEnd = 0u;
	size_t iEq = (size_t)-1;
	size_t iCur;
	if ( !sText || !pOut || iLen == 0u || __xrtHttpUtilContainsCtl(sText, iLen) ) { return false; }
	memset(pOut, 0, sizeof(*pOut));
	while ( iPairEnd < iLen && sText[iPairEnd] != ';' ) {
		if ( sText[iPairEnd] == '=' && iEq == (size_t)-1 ) { iEq = iPairEnd; }
		iPairEnd++;
	}
	if ( iEq == (size_t)-1 ) { return false; }
	pOut->tName = xrtStrView(sText, iEq);
	pOut->tValue = xrtStrView(sText + iEq + 1u, iPairEnd - iEq - 1u);
	__xrtHttpUtilTrimView(&pOut->tName);
	__xrtHttpUtilTrimView(&pOut->tValue);
	if ( pOut->tName.iLen == 0u || !xrtHttpIsTokenN(pOut->tName.sPtr, pOut->tName.iLen) ||
		!__xhttpCookieValueValid(pOut->tValue) ) { return false; }
	iCur = iPairEnd < iLen ? iPairEnd + 1u : iPairEnd;
	while ( iCur < iLen ) {
		size_t iEnd;
		size_t iAttrEq = (size_t)-1;
		xrtstrview tName;
		xrtstrview tValue = xrtStrView(NULL, 0u);
		while ( iCur < iLen && (sText[iCur] == ';' || sText[iCur] == ' ' || sText[iCur] == '\t') ) { iCur++; }
		if ( iCur == iLen ) { break; }
		iEnd = iCur;
		while ( iEnd < iLen && sText[iEnd] != ';' ) {
			if ( sText[iEnd] == '=' && iAttrEq == (size_t)-1 ) { iAttrEq = iEnd; }
			iEnd++;
		}
		tName = xrtStrView(sText + iCur, (iAttrEq == (size_t)-1 ? iEnd : iAttrEq) - iCur);
		__xrtHttpUtilTrimView(&tName);
		if ( iAttrEq != (size_t)-1 ) {
			tValue = xrtStrView(sText + iAttrEq + 1u, iEnd - iAttrEq - 1u);
			__xrtHttpUtilTrimView(&tValue);
		}
		if ( __xrtHttpUtilEqNoCaseN(tName.sPtr, tName.iLen, "Secure", 6u) ) {
			pOut->iFlags |= __XHTTP_COOKIE_PARSED_SECURE;
		} else if ( __xrtHttpUtilEqNoCaseN(tName.sPtr, tName.iLen, "HttpOnly", 8u) ) {
			pOut->iFlags |= __XHTTP_COOKIE_PARSED_HTTP_ONLY;
		} else if ( __xrtHttpUtilEqNoCaseN(tName.sPtr, tName.iLen, "Partitioned", 11u) ) {
			pOut->iFlags |= __XHTTP_COOKIE_PARSED_PARTITIONED;
		} else if ( __xrtHttpUtilEqNoCaseN(tName.sPtr, tName.iLen, "SameParty", 9u) ) {
			pOut->iFlags |= __XHTTP_COOKIE_PARSED_SAME_PARTY;
		} else if ( iAttrEq != (size_t)-1 && __xrtHttpUtilEqNoCaseN(tName.sPtr, tName.iLen, "Domain", 6u) ) {
			pOut->tDomain = tValue;
			pOut->iFlags |= __XHTTP_COOKIE_PARSED_HAS_DOMAIN;
		} else if ( iAttrEq != (size_t)-1 && __xrtHttpUtilEqNoCaseN(tName.sPtr, tName.iLen, "Path", 4u) ) {
			pOut->tPath = tValue;
			pOut->iFlags |= __XHTTP_COOKIE_PARSED_HAS_PATH;
		} else if ( iAttrEq != (size_t)-1 && __xrtHttpUtilEqNoCaseN(tName.sPtr, tName.iLen, "Expires", 7u) ) {
			pOut->tExpires = tValue;
			pOut->iFlags |= __XHTTP_COOKIE_PARSED_HAS_EXPIRES;
		} else if ( iAttrEq != (size_t)-1 && __xrtHttpUtilEqNoCaseN(tName.sPtr, tName.iLen, "Max-Age", 7u) ) {
			int64_t iMaxAge;
			if ( __xhttpCookieParseSigned64(tValue, &iMaxAge) ) {
				pOut->iMaxAge = iMaxAge;
				pOut->iFlags |= __XHTTP_COOKIE_PARSED_HAS_MAX_AGE;
			}
		} else if ( iAttrEq != (size_t)-1 && __xrtHttpUtilEqNoCaseN(tName.sPtr, tName.iLen, "SameSite", 8u) ) {
			if ( __xrtHttpUtilEqNoCaseN(tValue.sPtr, tValue.iLen, "Lax", 3u) ) { pOut->iSameSite = XRT_SAME_SITE_LAX; }
			else if ( __xrtHttpUtilEqNoCaseN(tValue.sPtr, tValue.iLen, "Strict", 6u) ) { pOut->iSameSite = XRT_SAME_SITE_STRICT; }
			else if ( __xrtHttpUtilEqNoCaseN(tValue.sPtr, tValue.iLen, "None", 4u) ) { pOut->iSameSite = XRT_SAME_SITE_NONE; }
			else { pOut->iSameSite = XRT_SAME_SITE_UNSPECIFIED; }
		} else if ( iAttrEq != (size_t)-1 && __xrtHttpUtilEqNoCaseN(tName.sPtr, tName.iLen, "Priority", 8u) ) {
			if ( __xrtHttpUtilEqNoCaseN(tValue.sPtr, tValue.iLen, "Low", 3u) ) { pOut->iPriority = XRT_COOKIE_PRIORITY_LOW; }
			else if ( __xrtHttpUtilEqNoCaseN(tValue.sPtr, tValue.iLen, "Medium", 6u) ) { pOut->iPriority = XRT_COOKIE_PRIORITY_MEDIUM; }
			else if ( __xrtHttpUtilEqNoCaseN(tValue.sPtr, tValue.iLen, "High", 4u) ) { pOut->iPriority = XRT_COOKIE_PRIORITY_HIGH; }
		}
		iCur = iEnd < iLen ? iEnd + 1u : iEnd;
	}
	return true;
}


static bool __xhttpCookieDateDelimiter(unsigned char ch)
{
	return ch == 0x09u || (ch >= 0x20u && ch <= 0x2fu) || (ch >= 0x3bu && ch <= 0x40u) ||
		(ch >= 0x5bu && ch <= 0x60u) || (ch >= 0x7bu && ch <= 0x7eu);
}


static bool __xhttpCookieParseDigits(const char* sText, size_t iLen, int* pValue)
{
	int iValue = 0;
	if ( !sText || !pValue || iLen == 0u || iLen > 4u ) { return false; }
	for ( size_t i = 0u; i < iLen; ++i ) {
		if ( sText[i] < '0' || sText[i] > '9' ) { return false; }
		iValue = iValue * 10 + (sText[i] - '0');
	}
	*pValue = iValue;
	return true;
}


static int __xhttpCookieMonth(const char* sText, size_t iLen)
{
	static const char* arrMonth[] = { "jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec" };
	if ( iLen < 3u ) { return 0; }
	for ( int i = 0; i < 12; ++i ) {
		if ( __xrtHttpUtilEqNoCaseN(sText, 3u, arrMonth[i], 3u) ) { return i + 1; }
	}
	return 0;
}


static int64_t __xhttpCookieDaysFromCivil(int iYear, unsigned iMonth, unsigned iDay)
{
	int iEra;
	unsigned iYearOfEra;
	unsigned iAdjustedMonth;
	unsigned iDayOfYear;
	unsigned iDayOfEra;
	iYear -= iMonth <= 2u;
	iEra = (iYear >= 0 ? iYear : iYear - 399) / 400;
	iYearOfEra = (unsigned)(iYear - iEra * 400);
	iAdjustedMonth = iMonth > 2u ? iMonth - 3u : iMonth + 9u;
	iDayOfYear = (153u * iAdjustedMonth + 2u) / 5u + iDay - 1u;
	iDayOfEra = iYearOfEra * 365u + iYearOfEra / 4u - iYearOfEra / 100u + iDayOfYear;
	return (int64_t)iEra * 146097 + (int64_t)iDayOfEra - 719468;
}


static bool __xhttpCookieParseDate(xrtstrview tDate, int64_t* pUnix)
{
	bool bTime = false, bDay = false, bMonth = false, bYear = false;
	int iHour = 0, iMinute = 0, iSecond = 0, iDay = 0, iMonth = 0, iYear = 0;
	size_t i = 0u;
	if ( !pUnix ) { return false; }
	while ( i < tDate.iLen ) {
		size_t iBegin;
		size_t iEnd;
		while ( i < tDate.iLen && __xhttpCookieDateDelimiter((unsigned char)tDate.sPtr[i]) ) { i++; }
		iBegin = i;
		while ( i < tDate.iLen && !__xhttpCookieDateDelimiter((unsigned char)tDate.sPtr[i]) ) { i++; }
		iEnd = i;
		if ( iEnd == iBegin ) { continue; }
		if ( !bTime ) {
			const char* sToken = tDate.sPtr + iBegin;
			const char* pColon1 = (const char*)memchr(sToken, ':', iEnd - iBegin);
			const char* pColon2 = pColon1 ? (const char*)memchr(pColon1 + 1, ':', (size_t)(sToken + iEnd - iBegin - pColon1 - 1)) : NULL;
			if ( pColon1 && pColon2 &&
				__xhttpCookieParseDigits(sToken, (size_t)(pColon1 - sToken), &iHour) &&
				__xhttpCookieParseDigits(pColon1 + 1, (size_t)(pColon2 - pColon1 - 1), &iMinute) &&
				__xhttpCookieParseDigits(pColon2 + 1, (size_t)(sToken + iEnd - iBegin - pColon2 - 1), &iSecond) ) {
				bTime = true;
				continue;
			}
		}
		if ( !bDay && iEnd - iBegin <= 2u && __xhttpCookieParseDigits(tDate.sPtr + iBegin, iEnd - iBegin, &iDay) ) { bDay = true; continue; }
		if ( !bMonth && (iMonth = __xhttpCookieMonth(tDate.sPtr + iBegin, iEnd - iBegin)) != 0 ) { bMonth = true; continue; }
		if ( !bYear && (iEnd - iBegin == 2u || iEnd - iBegin == 4u) &&
			__xhttpCookieParseDigits(tDate.sPtr + iBegin, iEnd - iBegin, &iYear) ) { bYear = true; }
	}
	if ( !bTime || !bDay || !bMonth || !bYear ) { return false; }
	if ( iYear >= 70 && iYear <= 99 ) { iYear += 1900; }
	else if ( iYear >= 0 && iYear <= 69 ) { iYear += 2000; }
	if ( iYear < 1601 || iDay < 1 || iDay > 31 || iHour > 23 || iMinute > 59 || iSecond > 59 ) { return false; }
	{
		static const int arrDays[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
		int iMaxDay = arrDays[iMonth - 1];
		if ( iMonth == 2 && ((iYear % 4 == 0 && iYear % 100 != 0) || iYear % 400 == 0) ) { iMaxDay = 29; }
		if ( iDay > iMaxDay ) { return false; }
	}
	*pUnix = __xhttpCookieDaysFromCivil(iYear, (unsigned)iMonth, (unsigned)iDay) * 86400 +
		(int64_t)iHour * 3600 + (int64_t)iMinute * 60 + iSecond;
	return true;
}


static xhttp_semantic_result __xhttpCookieCopyDomain(xrtstrview tValue, char** ppOut, size_t* pLen)
{
	size_t iBegin = 0u;
	size_t iEnd = tValue.iLen;
	char* sOut;
	if ( !ppOut ) { return XHTTP_SEMANTIC_INVALID_ARGUMENT; }
	*ppOut = NULL;
	if ( pLen ) { *pLen = 0u; }
	while ( iBegin < iEnd && tValue.sPtr[iBegin] == '.' ) { iBegin++; }
	while ( iEnd > iBegin && (tValue.sPtr[iEnd - 1u] == ' ' || tValue.sPtr[iEnd - 1u] == '\t') ) { iEnd--; }
	if ( iBegin == iEnd || tValue.sPtr[iEnd - 1u] == '.' ) { return XHTTP_SEMANTIC_INVALID_VALUE; }
	for ( size_t i = iBegin; i < iEnd; ++i ) {
		unsigned char ch = (unsigned char)tValue.sPtr[i];
		if ( ch <= 0x20u || ch >= 0x7fu || ch == '/' || ch == '\\' || ch == ':' ) {
			return XHTTP_SEMANTIC_INVALID_VALUE;
		}
	}
	sOut = (char*)XNET_ALLOC(iEnd - iBegin + 1u);
	if ( !sOut ) { return XHTTP_SEMANTIC_OUT_OF_MEMORY; }
	for ( size_t i = iBegin; i < iEnd; ++i ) {
		unsigned char ch = (unsigned char)tValue.sPtr[i];
		sOut[i - iBegin] = (ch >= 'A' && ch <= 'Z') ? (char)(ch + 32u) : (char)ch;
	}
	sOut[iEnd - iBegin] = '\0';
	*ppOut = sOut;
	if ( pLen ) { *pLen = iEnd - iBegin; }
	return XHTTP_SEMANTIC_OK;
}


static char* __xhttpCookieCopyView(xrtstrview tValue)
{
	char* sOut;
	if ( tValue.iLen == SIZE_MAX ) { return NULL; }
	sOut = (char*)XNET_ALLOC(tValue.iLen + 1u);
	if ( !sOut ) { return NULL; }
	if ( tValue.iLen > 0u ) { memcpy(sOut, tValue.sPtr, tValue.iLen); }
	sOut[tValue.iLen] = '\0';
	return sOut;
}


static void __xhttpCookieEntryUnit(__xhttp_cookie_entry* pCookie)
{
	if ( !pCookie ) { return; }
	XNET_FREE(pCookie->sName);
	XNET_FREE(pCookie->sValue);
	XNET_FREE(pCookie->sDomain);
	XNET_FREE(pCookie->sPath);
	memset(pCookie, 0, sizeof(*pCookie));
}


static void __xhttpCookieRemoveAt(__xhttp_cookie_entry* pCookie, size_t* pCount, size_t iIndex)
{
	if ( !pCookie || !pCount || iIndex >= *pCount ) { return; }
	__xhttpCookieEntryUnit(&pCookie[iIndex]);
	if ( iIndex + 1u < *pCount ) { memmove(&pCookie[iIndex], &pCookie[iIndex + 1u], (*pCount - iIndex - 1u) * sizeof(*pCookie)); }
	(*pCount)--;
	memset(&pCookie[*pCount], 0, sizeof(*pCookie));
}


static size_t __xhttpCookiePurgeLocked(xhttpcookiejar* pJar, int64_t iNowUnix)
{
	size_t iRemoved = 0u;
	for ( size_t i = 0u; i < pJar->iCount; ) {
		if ( (pJar->pCookie[i].iFlags & XHTTP_COOKIE_F_PERSISTENT) != 0u &&
			pJar->pCookie[i].iExpiresUnix <= iNowUnix ) {
			__xhttpCookieRemoveAt(pJar->pCookie, &pJar->iCount, i);
			iRemoved++;
		} else { i++; }
	}
	return iRemoved;
}


static size_t __xhttpCookieFindLocked(const xhttpcookiejar* pJar, const __xhttp_cookie_entry* pNeedle)
{
	for ( size_t i = 0u; i < pJar->iCount; ++i ) {
		const __xhttp_cookie_entry* pCookie = &pJar->pCookie[i];
		if ( __xhttpCookieTextEq(pCookie->sName, pCookie->iNameLen, pNeedle->sName, pNeedle->iNameLen) &&
			__xhttpCookieTextEq(pCookie->sDomain, pCookie->iDomainLen, pNeedle->sDomain, pNeedle->iDomainLen) &&
			__xhttpCookieTextEq(pCookie->sPath, pCookie->iPathLen, pNeedle->sPath, pNeedle->iPathLen) ) { return i; }
	}
	return SIZE_MAX;
}


static bool __xhttpCookieEnsureCap(xhttpcookiejar* pJar, size_t iNeed)
{
	__xhttp_cookie_entry* pNew;
	size_t iCap;
	if ( iNeed <= pJar->iCap ) { return true; }
	iCap = pJar->iCap ? pJar->iCap : 16u;
	while ( iCap < iNeed ) {
		if ( iCap > SIZE_MAX / 2u ) { return false; }
		iCap *= 2u;
	}
	if ( iCap > pJar->tConfig.iMaxCookies ) { iCap = pJar->tConfig.iMaxCookies; }
	if ( iCap < iNeed || iCap > SIZE_MAX / sizeof(*pNew) ) { return false; }
	pNew = (__xhttp_cookie_entry*)XNET_ALLOC(iCap * sizeof(*pNew));
	if ( !pNew ) { return false; }
	memset(pNew, 0, iCap * sizeof(*pNew));
	if ( pJar->iCount > 0u ) { memcpy(pNew, pJar->pCookie, pJar->iCount * sizeof(*pNew)); }
	XNET_FREE(pJar->pCookie);
	pJar->pCookie = pNew;
	pJar->iCap = iCap;
	return true;
}


static int __xhttpCookieEffectivePriority(const __xhttp_cookie_entry* pCookie)
{
	return pCookie->iPriority == XRT_COOKIE_PRIORITY_LOW ? 1 :
		(pCookie->iPriority == XRT_COOKIE_PRIORITY_HIGH ? 3 : 2);
}


static bool __xhttpCookieEvictOneLocked(xhttpcookiejar* pJar, const char* sDomain, size_t iDomainLen)
{
	size_t iCandidate = SIZE_MAX;
	for ( size_t i = 0u; i < pJar->iCount; ++i ) {
		const __xhttp_cookie_entry* pCookie = &pJar->pCookie[i];
		if ( sDomain && !__xhttpCookieTextEq(pCookie->sDomain, pCookie->iDomainLen, sDomain, iDomainLen) ) { continue; }
		if ( iCandidate == SIZE_MAX || __xhttpCookieEffectivePriority(pCookie) < __xhttpCookieEffectivePriority(&pJar->pCookie[iCandidate]) ||
			(__xhttpCookieEffectivePriority(pCookie) == __xhttpCookieEffectivePriority(&pJar->pCookie[iCandidate]) &&
			pCookie->iLastAccessSequence < pJar->pCookie[iCandidate].iLastAccessSequence) ) { iCandidate = i; }
	}
	if ( iCandidate == SIZE_MAX ) { return false; }
	__xhttpCookieRemoveAt(pJar->pCookie, &pJar->iCount, iCandidate);
	return true;
}


static bool __xhttpCookieDefaultPublicSuffix(const char* sDomain, size_t iLen)
{
	return sDomain && iLen > 0u && memchr(sDomain, '.', iLen) == NULL;
}


XXAPI void xrtHttpCookieJarConfigInit(xhttpcookiejarconfig* pConfig)
{
	if ( !pConfig ) { return; }
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->iSize = XHTTP_COOKIE_JAR_CONFIG_V1_SIZE;
	pConfig->iVersion = XHTTP_COOKIE_JAR_CONFIG_VERSION;
	pConfig->iFlags = XHTTP_COOKIE_JAR_F_ENFORCE_PREFIXES |
		XHTTP_COOKIE_JAR_F_REQUIRE_SECURE_SAMESITE_NONE |
		XHTTP_COOKIE_JAR_F_REJECT_PARTITIONED;
	pConfig->iMaxCookies = 3000u;
	pConfig->iMaxCookiesPerDomain = 180u;
	pConfig->iMaxCookieBytes = 4096u;
	pConfig->iMaxNameBytes = 1024u;
	pConfig->iMaxValueBytes = 4096u;
	pConfig->iMaxDomainBytes = 255u;
	pConfig->iMaxPathBytes = 4096u;
}


XXAPI xhttpcookiejar* xrtHttpCookieJarCreate(const xhttpcookiejarconfig* pConfig)
{
	xhttpcookiejarconfig tConfig;
	xhttpcookiejar* pJar;
	xrtHttpCookieJarConfigInit(&tConfig);
	if ( pConfig ) {
		if ( pConfig->iSize < XHTTP_COOKIE_JAR_CONFIG_V1_SIZE ||
			(pConfig->iVersion != 0u && pConfig->iVersion != XHTTP_COOKIE_JAR_CONFIG_VERSION) ||
			pConfig->iMaxCookies == 0u || pConfig->iMaxCookiesPerDomain == 0u ||
			pConfig->iMaxCookiesPerDomain > pConfig->iMaxCookies || pConfig->iMaxCookieBytes == 0u ||
			pConfig->iMaxNameBytes == 0u || pConfig->iMaxValueBytes == 0u ||
			pConfig->iMaxDomainBytes == 0u || pConfig->iMaxPathBytes == 0u ||
			(pConfig->iFlags & ~(XHTTP_COOKIE_JAR_F_ENFORCE_PREFIXES |
				XHTTP_COOKIE_JAR_F_REQUIRE_SECURE_SAMESITE_NONE | XHTTP_COOKIE_JAR_F_REJECT_PARTITIONED)) != 0u ) { return NULL; }
		tConfig = *pConfig;
	}
	pJar = (xhttpcookiejar*)XNET_ALLOC(sizeof(*pJar));
	if ( !pJar ) { return NULL; }
	memset(pJar, 0, sizeof(*pJar));
	pJar->iRefCount = 1;
	pJar->tConfig = tConfig;
	return pJar;
}


XXAPI xhttpcookiejar* xrtHttpCookieJarRetain(xhttpcookiejar* pJar)
{
	if ( pJar ) { (void)__xnetAtomicAddFetch32(&pJar->iRefCount, 1); }
	return pJar;
}


XXAPI void xrtHttpCookieJarClear(xhttpcookiejar* pJar)
{
	if ( !pJar ) { return; }
	__xhttpCookieJarLock(pJar);
	for ( size_t i = 0u; i < pJar->iCount; ++i ) { __xhttpCookieEntryUnit(&pJar->pCookie[i]); }
	pJar->iCount = 0u;
	__xhttpCookieJarUnlock(pJar);
}


XXAPI void xrtHttpCookieJarDestroy(xhttpcookiejar* pJar)
{
	if ( !pJar || __xnetAtomicAddFetch32(&pJar->iRefCount, -1) != 0 ) { return; }
	xrtHttpCookieJarClear(pJar);
	XNET_FREE(pJar->pCookie);
	memset(pJar, 0, sizeof(*pJar));
	XNET_FREE(pJar);
}


XXAPI size_t xrtHttpCookieJarCount(const xhttpcookiejar* pJar)
{
	size_t iCount;
	if ( !pJar ) { return 0u; }
	__xhttpCookieJarLock((xhttpcookiejar*)pJar);
	iCount = pJar->iCount;
	__xhttpCookieJarUnlock((xhttpcookiejar*)pJar);
	return iCount;
}


XXAPI size_t xrtHttpCookieJarPurgeExpired(xhttpcookiejar* pJar, int64_t iNowUnix)
{
	size_t iRemoved;
	if ( !pJar ) { return 0u; }
	iNowUnix = __xhttpCookieNowUnix(iNowUnix);
	__xhttpCookieJarLock(pJar);
	iRemoved = __xhttpCookiePurgeLocked(pJar, iNowUnix);
	__xhttpCookieJarUnlock(pJar);
	return iRemoved;
}


XXAPI xhttp_semantic_result xrtHttpCookieJarSetCookieN(xhttpcookiejar* pJar,
	const char* sURL, size_t iURLLen, const char* sSetCookie, size_t iSetCookieLen, int64_t iNowUnix)
{
	__xhttp_cookie_url tURL;
	__xhttp_cookie_parsed tParsed;
	__xhttp_cookie_entry tCookie;
	xrtstrview tPath;
	bool bHostOnly = true;
	bool bPublicSuffix = false;
	size_t iExisting;
	size_t iDomainCount = 0u;
	if ( !pJar || !sURL || !sSetCookie ) { return XHTTP_SEMANTIC_INVALID_ARGUMENT; }
	if ( !__xhttpCookieCanonicalUrlN(sURL, iURLLen, &tURL) ) { return XHTTP_SEMANTIC_INVALID_SYNTAX; }
	if ( !__xhttpCookieParseSetCookieN(sSetCookie, iSetCookieLen, &tParsed) ) {
		__xhttpCookieUrlUnit(&tURL);
		return XHTTP_SEMANTIC_INVALID_SYNTAX;
	}
	memset(&tCookie, 0, sizeof(tCookie));
	iNowUnix = __xhttpCookieNowUnix(iNowUnix);
	if ( tParsed.tName.iLen > pJar->tConfig.iMaxNameBytes || tParsed.tValue.iLen > pJar->tConfig.iMaxValueBytes ||
		tParsed.tName.iLen > pJar->tConfig.iMaxCookieBytes ||
		tParsed.tValue.iLen > pJar->tConfig.iMaxCookieBytes - tParsed.tName.iLen ) {
		__xhttpCookieUrlUnit(&tURL);
		return XHTTP_SEMANTIC_LIMIT;
	}
	if ( (tParsed.iFlags & __XHTTP_COOKIE_PARSED_HAS_DOMAIN) != 0u ) {
		xhttp_semantic_result eDomain = __xhttpCookieCopyDomain(tParsed.tDomain,
			&tCookie.sDomain, &tCookie.iDomainLen);
		if ( eDomain != XHTTP_SEMANTIC_OK ) {
			__xhttpCookieUrlUnit(&tURL);
			return eDomain;
		}
		if ( tCookie.iDomainLen > pJar->tConfig.iMaxDomainBytes || tURL.bIpHost ||
			!__xhttpCookieDomainMatch(tURL.sHost, tURL.iHostLen, tCookie.sDomain, tCookie.iDomainLen) ) {
			__xhttpCookieEntryUnit(&tCookie);
			__xhttpCookieUrlUnit(&tURL);
			return XHTTP_SEMANTIC_INVALID_VALUE;
		}
		bPublicSuffix = pJar->tConfig.IsPublicSuffix
			? pJar->tConfig.IsPublicSuffix(pJar->tConfig.pPublicSuffixContext, tCookie.sDomain, tCookie.iDomainLen)
			: __xhttpCookieDefaultPublicSuffix(tCookie.sDomain, tCookie.iDomainLen);
		if ( bPublicSuffix ) {
			if ( !__xhttpCookieTextEq(tURL.sHost, tURL.iHostLen, tCookie.sDomain, tCookie.iDomainLen) ) {
				__xhttpCookieEntryUnit(&tCookie);
				__xhttpCookieUrlUnit(&tURL);
				return XHTTP_SEMANTIC_INVALID_VALUE;
			}
			bHostOnly = true;
		} else { bHostOnly = false; }
	}
	if ( !tCookie.sDomain ) {
		tCookie.iDomainLen = tURL.iHostLen;
		tCookie.sDomain = __xhttpCookieCopyView(xrtStrView(tURL.sHost, tURL.iHostLen));
		if ( !tCookie.sDomain ) { __xhttpCookieUrlUnit(&tURL); return XHTTP_SEMANTIC_OUT_OF_MEMORY; }
	}
	tPath = ((tParsed.iFlags & __XHTTP_COOKIE_PARSED_HAS_PATH) != 0u &&
		tParsed.tPath.iLen > 0u && tParsed.tPath.sPtr[0] == '/') ? tParsed.tPath : __xhttpCookieDefaultPath(tURL.tPath);
	if ( tPath.iLen > pJar->tConfig.iMaxPathBytes ) {
		__xhttpCookieEntryUnit(&tCookie); __xhttpCookieUrlUnit(&tURL); return XHTTP_SEMANTIC_LIMIT;
	}
	tCookie.sName = __xhttpCookieCopyView(tParsed.tName);
	tCookie.sValue = __xhttpCookieCopyView(tParsed.tValue);
	tCookie.sPath = __xhttpCookieCopyView(tPath);
	if ( !tCookie.sName || !tCookie.sValue || !tCookie.sPath ) {
		__xhttpCookieEntryUnit(&tCookie); __xhttpCookieUrlUnit(&tURL); return XHTTP_SEMANTIC_OUT_OF_MEMORY;
	}
	tCookie.iNameLen = tParsed.tName.iLen;
	tCookie.iValueLen = tParsed.tValue.iLen;
	tCookie.iPathLen = tPath.iLen;
	if ( bHostOnly ) { tCookie.iFlags |= XHTTP_COOKIE_F_HOST_ONLY; }
	if ( (tParsed.iFlags & __XHTTP_COOKIE_PARSED_SECURE) != 0u ) { tCookie.iFlags |= XHTTP_COOKIE_F_SECURE; }
	if ( (tParsed.iFlags & __XHTTP_COOKIE_PARSED_HTTP_ONLY) != 0u ) { tCookie.iFlags |= XHTTP_COOKIE_F_HTTP_ONLY; }
	if ( (tParsed.iFlags & __XHTTP_COOKIE_PARSED_PARTITIONED) != 0u ) { tCookie.iFlags |= XHTTP_COOKIE_F_PARTITIONED; }
	if ( (tParsed.iFlags & __XHTTP_COOKIE_PARSED_SAME_PARTY) != 0u ) { tCookie.iFlags |= XHTTP_COOKIE_F_SAME_PARTY; }
	tCookie.iSameSite = tParsed.iSameSite;
	tCookie.iPriority = tParsed.iPriority;
	if ( (tCookie.iFlags & XHTTP_COOKIE_F_SECURE) != 0u && !tURL.bSecure ) {
		__xhttpCookieEntryUnit(&tCookie); __xhttpCookieUrlUnit(&tURL); return XHTTP_SEMANTIC_INVALID_VALUE;
	}
	if ( (pJar->tConfig.iFlags & XHTTP_COOKIE_JAR_F_REQUIRE_SECURE_SAMESITE_NONE) != 0u &&
		tCookie.iSameSite == XRT_SAME_SITE_NONE && (tCookie.iFlags & XHTTP_COOKIE_F_SECURE) == 0u ) {
		__xhttpCookieEntryUnit(&tCookie); __xhttpCookieUrlUnit(&tURL); return XHTTP_SEMANTIC_INVALID_VALUE;
	}
	if ( (pJar->tConfig.iFlags & XHTTP_COOKIE_JAR_F_REJECT_PARTITIONED) != 0u &&
		(tCookie.iFlags & XHTTP_COOKIE_F_PARTITIONED) != 0u ) {
		__xhttpCookieEntryUnit(&tCookie); __xhttpCookieUrlUnit(&tURL); return XHTTP_SEMANTIC_INVALID_VALUE;
	}
	if ( (pJar->tConfig.iFlags & XHTTP_COOKIE_JAR_F_ENFORCE_PREFIXES) != 0u ) {
		if ( __xhttpCookieStartsWith(&tParsed.tName, "__Secure-") &&
			((tCookie.iFlags & XHTTP_COOKIE_F_SECURE) == 0u || !tURL.bSecure) ) {
			__xhttpCookieEntryUnit(&tCookie); __xhttpCookieUrlUnit(&tURL); return XHTTP_SEMANTIC_INVALID_VALUE;
		}
		if ( __xhttpCookieStartsWith(&tParsed.tName, "__Host-") &&
			((tCookie.iFlags & XHTTP_COOKIE_F_SECURE) == 0u || !tURL.bSecure || !bHostOnly ||
			(tParsed.iFlags & __XHTTP_COOKIE_PARSED_HAS_DOMAIN) != 0u ||
			(tParsed.iFlags & __XHTTP_COOKIE_PARSED_HAS_PATH) == 0u || tCookie.iPathLen != 1u || tCookie.sPath[0] != '/') ) {
			__xhttpCookieEntryUnit(&tCookie); __xhttpCookieUrlUnit(&tURL); return XHTTP_SEMANTIC_INVALID_VALUE;
		}
	}
	if ( (tParsed.iFlags & __XHTTP_COOKIE_PARSED_HAS_MAX_AGE) != 0u ) {
		tCookie.iFlags |= XHTTP_COOKIE_F_PERSISTENT;
		tCookie.iExpiresUnix = tParsed.iMaxAge <= 0 ? 0 :
			(tParsed.iMaxAge > INT64_MAX - iNowUnix ? INT64_MAX : iNowUnix + tParsed.iMaxAge);
	} else if ( (tParsed.iFlags & __XHTTP_COOKIE_PARSED_HAS_EXPIRES) != 0u &&
		__xhttpCookieParseDate(tParsed.tExpires, &tCookie.iExpiresUnix) ) {
		tCookie.iFlags |= XHTTP_COOKIE_F_PERSISTENT;
	}
	__xhttpCookieJarLock(pJar);
	(void)__xhttpCookiePurgeLocked(pJar, iNowUnix);
	if ( !tURL.bSecure && (tCookie.iFlags & XHTTP_COOKIE_F_SECURE) == 0u ) {
		for ( size_t i = 0u; i < pJar->iCount; ++i ) {
			const __xhttp_cookie_entry* pOld = &pJar->pCookie[i];
			bool bDomainOverlap;
			bool bPathOverlap;
			if ( (pOld->iFlags & XHTTP_COOKIE_F_SECURE) == 0u ||
				!__xhttpCookieTextEq(pOld->sName, pOld->iNameLen, tCookie.sName, tCookie.iNameLen) ) { continue; }
			bDomainOverlap = __xhttpCookieDomainMatch(pOld->sDomain, pOld->iDomainLen, tCookie.sDomain, tCookie.iDomainLen) ||
				__xhttpCookieDomainMatch(tCookie.sDomain, tCookie.iDomainLen, pOld->sDomain, pOld->iDomainLen);
			bPathOverlap = __xhttpCookiePathMatch(xrtStrView(pOld->sPath, pOld->iPathLen), tCookie.sPath, tCookie.iPathLen) ||
				__xhttpCookiePathMatch(xrtStrView(tCookie.sPath, tCookie.iPathLen), pOld->sPath, pOld->iPathLen);
			if ( bDomainOverlap && bPathOverlap ) {
				__xhttpCookieJarUnlock(pJar);
				__xhttpCookieEntryUnit(&tCookie);
				__xhttpCookieUrlUnit(&tURL);
				return XHTTP_SEMANTIC_INVALID_VALUE;
			}
		}
	}
	iExisting = __xhttpCookieFindLocked(pJar, &tCookie);
	if ( (tCookie.iFlags & XHTTP_COOKIE_F_PERSISTENT) != 0u && tCookie.iExpiresUnix <= iNowUnix ) {
		if ( iExisting != SIZE_MAX ) { __xhttpCookieRemoveAt(pJar->pCookie, &pJar->iCount, iExisting); }
		__xhttpCookieJarUnlock(pJar);
		__xhttpCookieEntryUnit(&tCookie);
		__xhttpCookieUrlUnit(&tURL);
		return XHTTP_SEMANTIC_OK;
	}
	pJar->iSequence++;
	if ( pJar->iSequence == 0u ) { pJar->iSequence = 1u; }
	if ( iExisting != SIZE_MAX ) {
		tCookie.iCreationSequence = pJar->pCookie[iExisting].iCreationSequence;
		tCookie.iLastAccessSequence = pJar->iSequence;
		__xhttpCookieEntryUnit(&pJar->pCookie[iExisting]);
		pJar->pCookie[iExisting] = tCookie;
	} else {
		for ( size_t i = 0u; i < pJar->iCount; ++i ) {
			if ( __xhttpCookieTextEq(pJar->pCookie[i].sDomain, pJar->pCookie[i].iDomainLen,
				tCookie.sDomain, tCookie.iDomainLen) ) { iDomainCount++; }
		}
		while ( iDomainCount >= pJar->tConfig.iMaxCookiesPerDomain &&
			__xhttpCookieEvictOneLocked(pJar, tCookie.sDomain, tCookie.iDomainLen) ) { iDomainCount--; }
		while ( pJar->iCount >= pJar->tConfig.iMaxCookies && __xhttpCookieEvictOneLocked(pJar, NULL, 0u) ) {}
		if ( !__xhttpCookieEnsureCap(pJar, pJar->iCount + 1u) ) {
			__xhttpCookieJarUnlock(pJar);
			__xhttpCookieEntryUnit(&tCookie);
			__xhttpCookieUrlUnit(&tURL);
			return XHTTP_SEMANTIC_OUT_OF_MEMORY;
		}
		tCookie.iCreationSequence = pJar->iSequence;
		tCookie.iLastAccessSequence = pJar->iSequence;
		pJar->pCookie[pJar->iCount++] = tCookie;
	}
	__xhttpCookieJarUnlock(pJar);
	__xhttpCookieUrlUnit(&tURL);
	return XHTTP_SEMANTIC_OK;
}


XXAPI xhttp_semantic_result xrtHttpCookieJarSetCookie(xhttpcookiejar* pJar,
	const char* sURL, const char* sSetCookie, int64_t iNowUnix)
{
	return (sURL && sSetCookie) ? xrtHttpCookieJarSetCookieN(pJar, sURL, strlen(sURL),
		sSetCookie, strlen(sSetCookie), iNowUnix) : XHTTP_SEMANTIC_INVALID_ARGUMENT;
}


XXAPI size_t xrtHttpCookieJarStoreResponseHeaders(xhttpcookiejar* pJar,
	const char* sURL, const xhttpheaders* pHeaders, int64_t iNowUnix, size_t* pRejected)
{
	size_t iAccepted = 0u;
	size_t iRejected = 0u;
	if ( pRejected ) { *pRejected = 0u; }
	if ( !pJar || !sURL || !pHeaders ) { return 0u; }
	for ( size_t i = 0u; i < xrtHttpHeadersCount(pHeaders); ++i ) {
		xrtheaderpair tHeader;
		if ( xrtHttpHeadersAt(pHeaders, i, &tHeader) &&
			__xrtHttpUtilEqNoCaseN(tHeader.tName.sPtr, tHeader.tName.iLen, "Set-Cookie", 10u) ) {
			xhttp_semantic_result eResult = xrtHttpCookieJarSetCookieN(pJar, sURL, strlen(sURL),
				tHeader.tValue.sPtr, tHeader.tValue.iLen, iNowUnix);
			if ( eResult == XHTTP_SEMANTIC_OK ) { iAccepted++; } else { iRejected++; }
		}
	}
	if ( pRejected ) { *pRejected = iRejected; }
	return iAccepted;
}


static bool __xhttpCookieEligible(const __xhttp_cookie_entry* pCookie,
	const __xhttp_cookie_url* pURL, int64_t iNowUnix)
{
	if ( (pCookie->iFlags & XHTTP_COOKIE_F_PERSISTENT) != 0u && pCookie->iExpiresUnix <= iNowUnix ) { return false; }
	if ( (pCookie->iFlags & XHTTP_COOKIE_F_SECURE) != 0u && !pURL->bSecure ) { return false; }
	if ( (pCookie->iFlags & XHTTP_COOKIE_F_HOST_ONLY) != 0u ) {
		if ( !__xhttpCookieTextEq(pURL->sHost, pURL->iHostLen, pCookie->sDomain, pCookie->iDomainLen) ) { return false; }
	} else if ( !__xhttpCookieDomainMatch(pURL->sHost, pURL->iHostLen, pCookie->sDomain, pCookie->iDomainLen) ) { return false; }
	return __xhttpCookiePathMatch(pURL->tPath, pCookie->sPath, pCookie->iPathLen);
}


static bool __xhttpCookieOrderBefore(const __xhttp_cookie_entry* pA, const __xhttp_cookie_entry* pB)
{
	if ( pA->iPathLen != pB->iPathLen ) { return pA->iPathLen > pB->iPathLen; }
	return pA->iCreationSequence < pB->iCreationSequence;
}


XXAPI xhttp_semantic_result xrtHttpCookieJarBuildHeaderToN(xhttpcookiejar* pJar,
	const char* sURL, size_t iURLLen, int64_t iNowUnix, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	__xhttp_cookie_url tURL;
	size_t* arrIndex = NULL;
	size_t iSelected = 0u;
	size_t iRequired = 0u;
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( !pJar || !sURL || !pOutLen || !__xhttpCookieCanonicalUrlN(sURL, iURLLen, &tURL) ) {
		return XHTTP_SEMANTIC_INVALID_ARGUMENT;
	}
	iNowUnix = __xhttpCookieNowUnix(iNowUnix);
	__xhttpCookieJarLock(pJar);
	(void)__xhttpCookiePurgeLocked(pJar, iNowUnix);
	if ( pJar->iCount > 0u ) {
		if ( pJar->iCount > SIZE_MAX / sizeof(*arrIndex) ) {
			__xhttpCookieJarUnlock(pJar); __xhttpCookieUrlUnit(&tURL); return XHTTP_SEMANTIC_LIMIT;
		}
		arrIndex = (size_t*)XNET_ALLOC(pJar->iCount * sizeof(*arrIndex));
		if ( !arrIndex ) { __xhttpCookieJarUnlock(pJar); __xhttpCookieUrlUnit(&tURL); return XHTTP_SEMANTIC_OUT_OF_MEMORY; }
	}
	for ( size_t i = 0u; i < pJar->iCount; ++i ) {
		if ( __xhttpCookieEligible(&pJar->pCookie[i], &tURL, iNowUnix) ) {
			size_t iPos = iSelected;
			while ( iPos > 0u && __xhttpCookieOrderBefore(&pJar->pCookie[i], &pJar->pCookie[arrIndex[iPos - 1u]]) ) {
				arrIndex[iPos] = arrIndex[iPos - 1u];
				iPos--;
			}
			arrIndex[iPos] = i;
			iSelected++;
		}
	}
	for ( size_t i = 0u; i < iSelected; ++i ) {
		const __xhttp_cookie_entry* pCookie = &pJar->pCookie[arrIndex[i]];
		if ( pCookie->iNameLen > SIZE_MAX - iRequired - pCookie->iValueLen - 1u - (i > 0u ? 2u : 0u) ) {
			XNET_FREE(arrIndex); __xhttpCookieJarUnlock(pJar); __xhttpCookieUrlUnit(&tURL); return XHTTP_SEMANTIC_LIMIT;
		}
		iRequired += pCookie->iNameLen + 1u + pCookie->iValueLen + (i > 0u ? 2u : 0u);
	}
	*pOutLen = iRequired;
	if ( !sOut || iOutCap <= iRequired ) {
		XNET_FREE(arrIndex);
		__xhttpCookieJarUnlock(pJar);
		__xhttpCookieUrlUnit(&tURL);
		return XHTTP_SEMANTIC_BUFFER_TOO_SMALL;
	}
	{
		size_t iOff = 0u;
		for ( size_t i = 0u; i < iSelected; ++i ) {
			__xhttp_cookie_entry* pCookie = &pJar->pCookie[arrIndex[i]];
			if ( i > 0u ) { memcpy(sOut + iOff, "; ", 2u); iOff += 2u; }
			memcpy(sOut + iOff, pCookie->sName, pCookie->iNameLen); iOff += pCookie->iNameLen;
			sOut[iOff++] = '=';
			memcpy(sOut + iOff, pCookie->sValue, pCookie->iValueLen); iOff += pCookie->iValueLen;
			pJar->iSequence++;
			if ( pJar->iSequence == 0u ) { pJar->iSequence = 1u; }
			pCookie->iLastAccessSequence = pJar->iSequence;
		}
		sOut[iOff] = '\0';
	}
	XNET_FREE(arrIndex);
	__xhttpCookieJarUnlock(pJar);
	__xhttpCookieUrlUnit(&tURL);
	return XHTTP_SEMANTIC_OK;
}


XXAPI xhttp_semantic_result xrtHttpCookieJarBuildHeaderTo(xhttpcookiejar* pJar,
	const char* sURL, int64_t iNowUnix, char* sOut, size_t iOutCap, size_t* pOutLen)
{
	return sURL ? xrtHttpCookieJarBuildHeaderToN(pJar, sURL, strlen(sURL), iNowUnix,
		sOut, iOutCap, pOutLen) : XHTTP_SEMANTIC_INVALID_ARGUMENT;
}


XXAPI str xrtHttpCookieJarBuildHeaderN(xhttpcookiejar* pJar,
	const char* sURL, size_t iURLLen, int64_t iNowUnix, size_t* pOutLen)
{
	char* sOut = NULL;
	size_t iNeed = 0u;
	if ( pOutLen ) { *pOutLen = 0u; }
	for ( uint32 i = 0u; i < 4u; ++i ) {
		xhttp_semantic_result eResult = xrtHttpCookieJarBuildHeaderToN(pJar, sURL, iURLLen,
			iNowUnix, sOut, sOut ? iNeed + 1u : 0u, &iNeed);
		if ( eResult == XHTTP_SEMANTIC_OK ) { if ( pOutLen ) { *pOutLen = iNeed; } return (str)sOut; }
		if ( eResult != XHTTP_SEMANTIC_BUFFER_TOO_SMALL || iNeed == SIZE_MAX ) { XNET_FREE(sOut); return NULL; }
		XNET_FREE(sOut);
		sOut = (char*)XNET_ALLOC(iNeed + 1u);
		if ( !sOut ) { return NULL; }
	}
	XNET_FREE(sOut);
	return NULL;
}


XXAPI str xrtHttpCookieJarBuildHeader(xhttpcookiejar* pJar,
	const char* sURL, int64_t iNowUnix, size_t* pOutLen)
{
	return sURL ? xrtHttpCookieJarBuildHeaderN(pJar, sURL, strlen(sURL), iNowUnix, pOutLen) : NULL;
}


XXAPI bool xrtHttpCookieJarCookieAt(const xhttpcookiejar* pJar, size_t iIndex, xhttpcookieinfo* pOut)
{
	const __xhttp_cookie_entry* pCookie;
	if ( !pJar || !pOut ) { return false; }
	__xhttpCookieJarLock((xhttpcookiejar*)pJar);
	if ( iIndex >= pJar->iCount ) { __xhttpCookieJarUnlock((xhttpcookiejar*)pJar); return false; }
	pCookie = &pJar->pCookie[iIndex];
	memset(pOut, 0, sizeof(*pOut));
	pOut->iFlags = pCookie->iFlags;
	pOut->iSameSite = pCookie->iSameSite;
	pOut->iPriority = pCookie->iPriority;
	pOut->iExpiresUnix = pCookie->iExpiresUnix;
	pOut->iCreationSequence = pCookie->iCreationSequence;
	pOut->iLastAccessSequence = pCookie->iLastAccessSequence;
	pOut->tName = xrtStrView(pCookie->sName, pCookie->iNameLen);
	pOut->tValue = xrtStrView(pCookie->sValue, pCookie->iValueLen);
	pOut->tDomain = xrtStrView(pCookie->sDomain, pCookie->iDomainLen);
	pOut->tPath = xrtStrView(pCookie->sPath, pCookie->iPathLen);
	__xhttpCookieJarUnlock((xhttpcookiejar*)pJar);
	return true;
}

#endif
