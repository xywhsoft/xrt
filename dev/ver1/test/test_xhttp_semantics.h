#ifndef XRT_TEST_XHTTP_SEMANTICS_H
#define XRT_TEST_XHTTP_SEMANTICS_H

static bool __Test_HttpSemViewEq(xrtstrview tView, const char* sExpected)
{
	size_t iLen = strlen(sExpected);
	return tView.iLen == iLen && (iLen == 0u || memcmp(tView.sPtr, sExpected, iLen) == 0);
}


static int __g_iHttpSemContextValueFreeCount = 0;
static int __g_iHttpSemBodyDataFreeCount = 0;


static void __Test_HttpSemFreeContextValue(ptr pValue)
{
	__g_iHttpSemContextValueFreeCount++;
	xrtFree(pValue);
}


static void __Test_HttpSemFreeBodyData(ptr pContext, const void* pData, size_t iLen)
{
	(void)pContext;
	(void)pData;
	(void)iLen;
	__g_iHttpSemBodyDataFreeCount++;
}


static bool __Test_HttpSemReadBody(xhttpbody* pBody, char* pOut, size_t iOutCap, size_t* pOutLen)
{
	xhttpbodyreader* pReader = NULL;
	size_t iTotal = 0u;
	if ( pOutLen ) { *pOutLen = 0u; }
	if ( !pBody || !pOut || iOutCap == 0u || !pOutLen ||
		xrtHttpBodyOpen(pBody, NULL, &pReader) != XHTTP_SEMANTIC_OK ) { return false; }
	for ( ;; ) {
		size_t iRead = 0u;
		size_t iStep = iOutCap - 1u - iTotal;
		xhttp_body_read_result eRead;
		if ( iStep > 7u ) { iStep = 7u; }
		if ( iStep == 0u ) { xrtHttpBodyReaderClose(pReader); return false; }
		eRead = xrtHttpBodyReaderRead(pReader, pOut + iTotal, iStep, &iRead, NULL);
		if ( eRead == XHTTP_BODY_READ_EOF ) { break; }
		if ( eRead != XHTTP_BODY_READ_DATA ) { xrtHttpBodyReaderClose(pReader); return false; }
		iTotal += iRead;
	}
	pOut[iTotal] = '\0';
	*pOutLen = iTotal;
	xrtHttpBodyReaderClose(pReader);
	return true;
}


static bool Test_XHttpSemantics(void)
{
	bool bPass = true;
	xhttpheaders* pHeaders = NULL;
	xhttpheaders* pClone = NULL;
	xhttpheaders* pLimited = NULL;
	xhttpsearchparams* pParams = NULL;
	xhttpsearchparams* pParamsClone = NULL;
	xhttpsearchparams* pParamsLenient = NULL;
	xhttpcontext* pRootContext = NULL;
	xhttpcontext* pValueContext = NULL;
	xhttpcontext* pChildContext = NULL;
	xhttpcontext* pBodyContext = NULL;
	xhttpbody* pBody = NULL;
	xhttpbody* pRefBody = NULL;
	xhttpbody* pEmptyBody = NULL;
	xhttpbodyreader* pBodyReader1 = NULL;
	xhttpbodyreader* pBodyReader2 = NULL;
	xhttpformdata* pForm = NULL;
	xhttpformdata* pFormClone = NULL;
	xhttpformdata* pParsedForm = NULL;
	xhttpbody* pFormBody = NULL;
	xhttpcookiejar* pCookieJar = NULL;
	xnetcancel* pExternalCancel = NULL;
	xhttpheadersconfig tConfig;
	xhttpsearchparamsconfig tParamsConfig;
	xhttpcookiejarconfig tCookieJarConfig;
	xhttpformdataconfig tFormConfig;
	xhttpformdatapartinfo tFormPart;
	xhttpcookieinfo tCookieInfo;
	xrtheaderpair tPair;
	xrtstrview tValue;
	xrtstrview arrValues[3];
	xhttp_semantic_result eResult;
	char* sBuilt = NULL;
	str sAllocated = NULL;
	str sCookieHeader = NULL;
	size_t iBuiltLen = 0u;
	size_t iErrorOffset = 0u;
	size_t iBeforeCount;
	bool bHasValue = false;
	char sBodySource[] = "abcdef";
	char sBodyOut[16];
	char sFormContentType[128];
	char sFormEncoded[2048];
	size_t iFormEncodedLen = 0u;
	size_t iBodyRead = 0u;
	xnetcontext tNetContext;

	printf("Testing HTTP semantic values...\n");
	xrtHttpHeadersConfigInit(&tConfig);
	bPass = bPass && tConfig.iSize == XHTTP_HEADERS_CONFIG_V1_SIZE;
	bPass = bPass && tConfig.iVersion == XHTTP_HEADERS_CONFIG_VERSION;
	bPass = bPass && strcmp(xrtHttpSemanticResultName(XHTTP_SEMANTIC_LIMIT), "limit") == 0;

	pHeaders = xrtHttpHeadersCreate();
	bPass = bPass && pHeaders != NULL;
	if ( pHeaders == NULL ) { goto done; }

	bPass = bPass && xrtHttpHeadersAppend(pHeaders, "Content-Type", "text/plain") == XHTTP_SEMANTIC_OK;
	bPass = bPass && xrtHttpHeadersAppend(pHeaders, "Set-Cookie", "a=1; Path=/") == XHTTP_SEMANTIC_OK;
	bPass = bPass && xrtHttpHeadersAppend(pHeaders, "set-cookie", "b=2; Path=/") == XHTTP_SEMANTIC_OK;
	bPass = bPass && xrtHttpHeadersAppendN(pHeaders, "X-Empty", 7u, NULL, 0u) == XHTTP_SEMANTIC_OK;
	bPass = bPass && xrtHttpHeadersAppend(pHeaders, "X-Tab", "a\tb") == XHTTP_SEMANTIC_OK;
	bPass = bPass && xrtHttpHeadersCount(pHeaders) == 5u;
	bPass = bPass && xrtHttpHeadersCountName(pHeaders, "SET-COOKIE") == 2u;
	bPass = bPass && xrtHttpHeadersContains(pHeaders, "content-type");
	bPass = bPass && xrtHttpHeadersGet(pHeaders, "CONTENT-TYPE", &tValue);
	bPass = bPass && __Test_HttpSemViewEq(tValue, "text/plain");
	bPass = bPass && xrtHttpHeadersGetAllTo(pHeaders, "Set-Cookie", arrValues, 3u) == 2u;
	bPass = bPass && __Test_HttpSemViewEq(arrValues[0], "a=1; Path=/");
	bPass = bPass && __Test_HttpSemViewEq(arrValues[1], "b=2; Path=/");
	bPass = bPass && xrtHttpHeadersAt(pHeaders, 2u, &tPair);
	bPass = bPass && __Test_HttpSemViewEq(tPair.tName, "set-cookie");

	/* Mutation must remain safe when both inputs are views into this container. */
	bPass = bPass && xrtHttpHeadersAt(pHeaders, 0u, &tPair);
	bPass = bPass && xrtHttpHeadersAppendN(pHeaders,
		tPair.tName.sPtr, tPair.tName.iLen, tPair.tValue.sPtr, tPair.tValue.iLen) == XHTTP_SEMANTIC_OK;
	bPass = bPass && xrtHttpHeadersCountName(pHeaders, "Content-Type") == 2u;

	bPass = bPass && xrtHttpHeadersSet(pHeaders, "content-type", "application/json") == XHTTP_SEMANTIC_OK;
	bPass = bPass && xrtHttpHeadersCountName(pHeaders, "Content-Type") == 1u;
	bPass = bPass && xrtHttpHeadersGet(pHeaders, "Content-Type", &tValue);
	bPass = bPass && __Test_HttpSemViewEq(tValue, "application/json");
	bPass = bPass && xrtHttpHeadersRemove(pHeaders, "x-empty") == 1u;
	bPass = bPass && !xrtHttpHeadersContains(pHeaders, "X-Empty");

	bPass = bPass && xrtHttpHeadersAppend(pHeaders, "Bad Name", "x") == XHTTP_SEMANTIC_INVALID_NAME;
	bPass = bPass && xrtHttpHeadersAppend(pHeaders, "X-Test", "ok\r\nInjected: yes") == XHTTP_SEMANTIC_INVALID_VALUE;

	eResult = xrtHttpHeadersParseAppend(pHeaders,
		"Cache-Control: no-cache\r\n"
		"Warning: 199 first\r\n"
		"Warning: 299 second\r\n\r\n",
		&iErrorOffset);
	bPass = bPass && eResult == XHTTP_SEMANTIC_OK;
	bPass = bPass && xrtHttpHeadersCountName(pHeaders, "Warning") == 2u;
	iBeforeCount = xrtHttpHeadersCount(pHeaders);
	eResult = xrtHttpHeadersParseAppend(pHeaders, "Good: yes\r\nBad: no\n", &iErrorOffset);
	bPass = bPass && eResult == XHTTP_SEMANTIC_INVALID_SYNTAX;
	bPass = bPass && iErrorOffset == 18u;
	bPass = bPass && xrtHttpHeadersCount(pHeaders) == iBeforeCount;
	bPass = bPass && !xrtHttpHeadersContains(pHeaders, "Good");

	eResult = xrtHttpHeadersBuildBlockTo(pHeaders, NULL, 0u, &iBuiltLen);
	bPass = bPass && eResult == XHTTP_SEMANTIC_BUFFER_TOO_SMALL && iBuiltLen >= 2u;
	sBuilt = (char*)xrtMalloc(iBuiltLen + 1u);
	bPass = bPass && sBuilt != NULL;
	if ( sBuilt != NULL ) {
		bPass = bPass && xrtHttpHeadersBuildBlockTo(pHeaders, sBuilt, iBuiltLen + 1u, &iBuiltLen) == XHTTP_SEMANTIC_OK;
		bPass = bPass && strstr(sBuilt, "Set-Cookie: a=1; Path=/\r\n") != NULL;
		bPass = bPass && strstr(sBuilt, "set-cookie: b=2; Path=/\r\n") != NULL;
		bPass = bPass && iBuiltLen >= 4u && memcmp(sBuilt + iBuiltLen - 4u, "\r\n\r\n", 4u) == 0;
	}
	sAllocated = xrtHttpHeadersBuildBlock(pHeaders, &iBuiltLen);
	bPass = bPass && sAllocated != NULL;
	bPass = bPass && sAllocated != NULL && strlen((const char*)sAllocated) == iBuiltLen;

	pClone = xrtHttpHeadersClone(pHeaders);
	bPass = bPass && pClone != NULL;
	if ( pClone != NULL ) {
		bPass = bPass && xrtHttpHeadersSet(pClone, "Cache-Control", "max-age=60") == XHTTP_SEMANTIC_OK;
		bPass = bPass && xrtHttpHeadersGet(pHeaders, "Cache-Control", &tValue);
		bPass = bPass && __Test_HttpSemViewEq(tValue, "no-cache");
		bPass = bPass && xrtHttpHeadersCompact(pClone) == XHTTP_SEMANTIC_OK;
		bPass = bPass && xrtHttpHeadersGet(pClone, "Cache-Control", &tValue);
		bPass = bPass && __Test_HttpSemViewEq(tValue, "max-age=60");
	}

	xrtHttpHeadersConfigInit(&tConfig);
	tConfig.iInitialCount = 1u;
	tConfig.iMaxCount = 2u;
	tConfig.iInitialBytes = 4u;
	tConfig.iMaxNameBytes = 8u;
	tConfig.iMaxValueBytes = 8u;
	tConfig.iMaxBytes = 16u;
	pLimited = xrtHttpHeadersCreateEx(&tConfig);
	bPass = bPass && pLimited != NULL;
	if ( pLimited != NULL ) {
		bPass = bPass && xrtHttpHeadersAppend(pLimited, "A", "1") == XHTTP_SEMANTIC_OK;
		bPass = bPass && xrtHttpHeadersAppend(pLimited, "B", "2") == XHTTP_SEMANTIC_OK;
		bPass = bPass && xrtHttpHeadersAppend(pLimited, "C", "3") == XHTTP_SEMANTIC_LIMIT;
		bPass = bPass && xrtHttpHeadersSet(pLimited, "A", "123456789") == XHTTP_SEMANTIC_LIMIT;
		bPass = bPass && xrtHttpHeadersCount(pLimited) == 2u;
	}

	xrtHttpCookieJarConfigInit(&tCookieJarConfig);
	bPass = bPass && tCookieJarConfig.iSize == XHTTP_COOKIE_JAR_CONFIG_V1_SIZE;
	bPass = bPass && (tCookieJarConfig.iFlags & XHTTP_COOKIE_JAR_F_ENFORCE_PREFIXES) != 0u;
	pCookieJar = xrtHttpCookieJarCreate(&tCookieJarConfig);
	bPass = bPass && pCookieJar != NULL;
	if ( pCookieJar != NULL ) {
		const int64_t iCookieNow = INT64_C(1600000000);
		bPass = bPass && xrtHttpCookieJarSetCookie(pCookieJar,
			"https://sub.example.com/app/login", "sid=host; HttpOnly", iCookieNow) == XHTTP_SEMANTIC_OK;
		bPass = bPass && xrtHttpCookieJarSetCookie(pCookieJar,
			"https://sub.example.com/app/login", "wide=1; Domain=.example.com; Path=/; Secure; Max-Age=60", iCookieNow) == XHTTP_SEMANTIC_OK;
		bPass = bPass && xrtHttpCookieJarSetCookie(pCookieJar,
			"https://sub.example.com/app/login", "pref=dark; Path=/app", iCookieNow) == XHTTP_SEMANTIC_OK;
		/* Invalid attributes are ignored without dropping an otherwise valid cookie. */
		bPass = bPass && xrtHttpCookieJarSetCookie(pCookieJar,
			"https://sub.example.com/app/login", "tolerant=ok; Max-Age=oops; SameSite=Unknown", iCookieNow) == XHTTP_SEMANTIC_OK;
		bPass = bPass && xrtHttpCookieJarSetCookie(pCookieJar,
			"https://sub.example.com/app/login", "quoted=\"hello\"", iCookieNow) == XHTTP_SEMANTIC_OK;
		bPass = bPass && xrtHttpCookieJarSetCookie(pCookieJar,
			"https://sub.example.com/", "bad=1; Domain=attacker.example", iCookieNow) == XHTTP_SEMANTIC_INVALID_VALUE;
		bPass = bPass && xrtHttpCookieJarSetCookie(pCookieJar,
			"https://foo.com/", "bad=1; Domain=com", iCookieNow) == XHTTP_SEMANTIC_INVALID_VALUE;
		bPass = bPass && xrtHttpCookieJarSetCookie(pCookieJar,
			"https://sub.example.com/", "none=1; SameSite=None", iCookieNow) == XHTTP_SEMANTIC_INVALID_VALUE;
		bPass = bPass && xrtHttpCookieJarSetCookie(pCookieJar,
			"https://sub.example.com/", "part=1; Secure; Partitioned", iCookieNow) == XHTTP_SEMANTIC_INVALID_VALUE;
		bPass = bPass && xrtHttpCookieJarSetCookie(pCookieJar,
			"https://sub.example.com/", "__Secure-bad=1", iCookieNow) == XHTTP_SEMANTIC_INVALID_VALUE;
		bPass = bPass && xrtHttpCookieJarSetCookie(pCookieJar,
			"https://sub.example.com/", "__Host-good=1; Secure; Path=/", iCookieNow) == XHTTP_SEMANTIC_OK;
		bPass = bPass && xrtHttpCookieJarSetCookie(pCookieJar,
			"https://sub.example.com/", "__Host-bad=1; Secure; Path=/; Domain=sub.example.com", iCookieNow) == XHTTP_SEMANTIC_INVALID_VALUE;
		bPass = bPass && xrtHttpCookieJarSetCookie(pCookieJar,
			"https://sub.example.com/", "shadow=secure; Secure; Path=/", iCookieNow) == XHTTP_SEMANTIC_OK;
		bPass = bPass && xrtHttpCookieJarSetCookie(pCookieJar,
			"http://sub.example.com/", "shadow=insecure; Path=/", iCookieNow) == XHTTP_SEMANTIC_INVALID_VALUE;
		bPass = bPass && xrtHttpCookieJarCount(pCookieJar) == 7u;

		sCookieHeader = xrtHttpCookieJarBuildHeader(pCookieJar,
			"https://sub.example.com/app/page", iCookieNow, &iBuiltLen);
		bPass = bPass && sCookieHeader != NULL;
		bPass = bPass && sCookieHeader != NULL &&
			strncmp((const char*)sCookieHeader, "sid=host; ", 10u) == 0;
		bPass = bPass && sCookieHeader != NULL && strstr((const char*)sCookieHeader, "sid=host") != NULL;
		bPass = bPass && sCookieHeader != NULL && strstr((const char*)sCookieHeader, "wide=1") != NULL;
		bPass = bPass && sCookieHeader != NULL && strstr((const char*)sCookieHeader, "quoted=\"hello\"") != NULL;
		xrtFree(sCookieHeader);
		sCookieHeader = xrtHttpCookieJarBuildHeader(pCookieJar,
			"http://sub.example.com/app/page", iCookieNow, &iBuiltLen);
		bPass = bPass && sCookieHeader != NULL && strstr((const char*)sCookieHeader, "wide=1") == NULL;
		bPass = bPass && sCookieHeader != NULL && strstr((const char*)sCookieHeader, "__Host-good=1") == NULL;
		xrtFree(sCookieHeader);
		sCookieHeader = xrtHttpCookieJarBuildHeader(pCookieJar,
			"https://child.example.com/app/page", iCookieNow, &iBuiltLen);
		bPass = bPass && sCookieHeader != NULL && strcmp((const char*)sCookieHeader, "wide=1") == 0;
		xrtFree(sCookieHeader);
		sCookieHeader = NULL;

		bPass = bPass && xrtHttpCookieJarCookieAt(pCookieJar, 0u, &tCookieInfo);
		bPass = bPass && (tCookieInfo.iFlags & XHTTP_COOKIE_F_HOST_ONLY) != 0u;
		bPass = bPass && __Test_HttpSemViewEq(tCookieInfo.tDomain, "sub.example.com");
		bPass = bPass && xrtHttpCookieJarSetCookie(pCookieJar,
			"https://sub.example.com/app", "sid=gone; Path=/app; Max-Age=0", iCookieNow) == XHTTP_SEMANTIC_OK;
		bPass = bPass && xrtHttpCookieJarCount(pCookieJar) == 6u;
		bPass = bPass && xrtHttpCookieJarPurgeExpired(pCookieJar, iCookieNow + 61) == 1u;
	}

	xrtFree(sBuilt);
	sBuilt = NULL;
	xrtFree(sAllocated);
	sAllocated = NULL;

	xrtHttpSearchParamsConfigInit(&tParamsConfig);
	bPass = bPass && tParamsConfig.iSize == XHTTP_SEARCH_PARAMS_CONFIG_V1_SIZE;
	bPass = bPass && (tParamsConfig.iFlags & XHTTP_SEARCH_PARAMS_F_STRICT_PERCENT) != 0u;
	pParams = xrtHttpSearchParamsParse(
		"?q=hello+world&q=two&flag&empty=&tilde=~&star=*%2A&nul=%00",
		&tParamsConfig, &eResult, &iErrorOffset);
	bPass = bPass && pParams != NULL && eResult == XHTTP_SEMANTIC_OK;
	if ( pParams != NULL ) {
		xrtquerypair tQueryPair;
		bPass = bPass && xrtHttpSearchParamsCount(pParams) == 7u;
		bPass = bPass && xrtHttpSearchParamsCountName(pParams, "q") == 2u;
		bPass = bPass && !xrtHttpSearchParamsHas(pParams, "Q");
		bPass = bPass && xrtHttpSearchParamsGet(pParams, "q", &tValue, &bHasValue);
		bPass = bPass && bHasValue && __Test_HttpSemViewEq(tValue, "hello world");
		bPass = bPass && xrtHttpSearchParamsGetNth(pParams, "q", 1u, &tValue, &bHasValue);
		bPass = bPass && bHasValue && __Test_HttpSemViewEq(tValue, "two");
		bPass = bPass && xrtHttpSearchParamsGet(pParams, "flag", &tValue, &bHasValue);
		bPass = bPass && !bHasValue && tValue.iLen == 0u;
		bPass = bPass && xrtHttpSearchParamsGet(pParams, "empty", &tValue, &bHasValue);
		bPass = bPass && bHasValue && tValue.iLen == 0u;
		bPass = bPass && xrtHttpSearchParamsGet(pParams, "nul", &tValue, &bHasValue);
		bPass = bPass && bHasValue && tValue.iLen == 1u && tValue.sPtr[0] == '\0';
		bPass = bPass && xrtHttpSearchParamsAt(pParams, 2u, &tQueryPair);
		bPass = bPass && (tQueryPair.iFlags & XRT_QUERY_F_HAS_VALUE) == 0u;

		eResult = xrtHttpSearchParamsBuildTo(pParams, NULL, 0u, &iBuiltLen);
		bPass = bPass && eResult == XHTTP_SEMANTIC_BUFFER_TOO_SMALL;
		sBuilt = (char*)xrtMalloc(iBuiltLen + 1u);
		bPass = bPass && sBuilt != NULL;
		if ( sBuilt != NULL ) {
			bPass = bPass && xrtHttpSearchParamsBuildTo(pParams, sBuilt, iBuiltLen + 1u, &iBuiltLen) == XHTTP_SEMANTIC_OK;
			bPass = bPass && strcmp(sBuilt,
				"q=hello+world&q=two&flag&empty=&tilde=%7E&star=**&nul=%00") == 0;
		}

		iBeforeCount = xrtHttpSearchParamsCount(pParams);
		eResult = xrtHttpSearchParamsParseAppend(pParams, "ok=1&bad=%GG", &iErrorOffset);
		bPass = bPass && eResult == XHTTP_SEMANTIC_INVALID_SYNTAX;
		bPass = bPass && iErrorOffset == 9u;
		bPass = bPass && xrtHttpSearchParamsCount(pParams) == iBeforeCount;
		bPass = bPass && !xrtHttpSearchParamsHas(pParams, "ok");

		/* A value view remains valid as input even if the append grows storage. */
		bPass = bPass && xrtHttpSearchParamsGet(pParams, "q", &tValue, &bHasValue);
		bPass = bPass && xrtHttpSearchParamsAppendN(pParams, "copy", 4u,
			tValue.sPtr, tValue.iLen, true) == XHTTP_SEMANTIC_OK;
		bPass = bPass && xrtHttpSearchParamsGet(pParams, "copy", &tValue, &bHasValue);
		bPass = bPass && bHasValue && __Test_HttpSemViewEq(tValue, "hello world");

		pParamsClone = xrtHttpSearchParamsClone(pParams);
		bPass = bPass && pParamsClone != NULL;
		if ( pParamsClone != NULL ) {
			bPass = bPass && xrtHttpSearchParamsAppend(pParamsClone, "a", "first") == XHTTP_SEMANTIC_OK;
			bPass = bPass && xrtHttpSearchParamsAppend(pParamsClone, "a", "second") == XHTTP_SEMANTIC_OK;
			bPass = bPass && xrtHttpSearchParamsAppend(pParamsClone, "B", "upper") == XHTTP_SEMANTIC_OK;
			bPass = bPass && xrtHttpSearchParamsSort(pParamsClone) == XHTTP_SEMANTIC_OK;
			bPass = bPass && xrtHttpSearchParamsAt(pParamsClone, 0u, &tQueryPair);
			bPass = bPass && __Test_HttpSemViewEq(tQueryPair.tKey, "B");
			bPass = bPass && xrtHttpSearchParamsGetNth(pParamsClone, "a", 0u, &tValue, &bHasValue);
			bPass = bPass && __Test_HttpSemViewEq(tValue, "first");
			bPass = bPass && xrtHttpSearchParamsGetNth(pParamsClone, "a", 1u, &tValue, &bHasValue);
			bPass = bPass && __Test_HttpSemViewEq(tValue, "second");
		}

		bPass = bPass && xrtHttpSearchParamsSet(pParams, "q", "final") == XHTTP_SEMANTIC_OK;
		bPass = bPass && xrtHttpSearchParamsCountName(pParams, "q") == 1u;
		bPass = bPass && xrtHttpSearchParamsDelete(pParams, "flag") == 1u;
		bPass = bPass && xrtHttpSearchParamsCompact(pParams) == XHTTP_SEMANTIC_OK;
	}

	tParamsConfig.iFlags = XHTTP_SEARCH_PARAMS_F_NONE;
	pParamsLenient = xrtHttpSearchParamsParse("bad=%GG", &tParamsConfig, &eResult, &iErrorOffset);
	bPass = bPass && pParamsLenient != NULL && eResult == XHTTP_SEMANTIC_OK;
	if ( pParamsLenient != NULL ) {
		bPass = bPass && xrtHttpSearchParamsGet(pParamsLenient, "bad", &tValue, &bHasValue);
		bPass = bPass && __Test_HttpSemViewEq(tValue, "%GG");
		sAllocated = xrtHttpSearchParamsBuild(pParamsLenient, &iBuiltLen);
		bPass = bPass && sAllocated != NULL;
		bPass = bPass && sAllocated != NULL && strcmp((const char*)sAllocated, "bad=%25GG") == 0;
	}

	__g_iHttpSemContextValueFreeCount = 0;
	pRootContext = xrtHttpContextCreate();
	bPass = bPass && pRootContext != NULL;
	if ( pRootContext != NULL ) {
		int* pOwnedValue = (int*)xrtMalloc(sizeof(int));
		bPass = bPass && pOwnedValue != NULL;
		if ( pOwnedValue != NULL ) {
			*pOwnedValue = 42;
			pValueContext = xrtHttpContextWithValue(pRootContext, "trace.id",
				pOwnedValue, __Test_HttpSemFreeContextValue);
			if ( pValueContext == NULL ) { xrtFree(pOwnedValue); }
		}
		bPass = bPass && pValueContext != NULL;
		if ( pValueContext != NULL ) {
			pChildContext = xrtHttpContextWithTimeout(pValueContext, 10000u);
			bPass = bPass && pChildContext != NULL;
			bPass = bPass && xrtHttpContextDeadline(pChildContext) > 0;
			bPass = bPass && xrtHttpContextValue(pChildContext, "trace.id") != NULL;
			bPass = bPass && *(int*)xrtHttpContextValue(pChildContext, "trace.id") == 42;
			bPass = bPass && xrtHttpContextToNet(pChildContext, &tNetContext);
			bPass = bPass && tNetContext.pCancel != NULL;
			bPass = bPass && xrtHttpContextCancel(pRootContext);
			bPass = bPass && xrtHttpContextIsCancelled(pChildContext);
			bPass = bPass && xrtHttpContextIsDone(pChildContext);
		}
	}

	pExternalCancel = xrtNetCancelCreate();
	xrtNetContextInit(&tNetContext);
	xrtNetContextSetCancel(&tNetContext, pExternalCancel);
	pBodyContext = xrtHttpContextFromNet(&tNetContext);
	bPass = bPass && pBodyContext != NULL;
	bPass = bPass && pBodyContext != NULL && !xrtHttpContextIsCancelled(pBodyContext);
	bPass = bPass && xrtNetCancelRequest(pExternalCancel);
	bPass = bPass && pBodyContext != NULL && xrtHttpContextIsCancelled(pBodyContext);
	xrtHttpContextRelease(pBodyContext);
	pBodyContext = xrtHttpContextCreate();
	bPass = bPass && pBodyContext != NULL;

	pBody = xrtHttpBodyCreateBytesCopy(sBodySource, 6u);
	bPass = bPass && pBody != NULL;
	if ( pBody != NULL ) {
		memset(sBodySource, 'X', 6u);
		bPass = bPass && xrtHttpBodyContentLength(pBody) == 6;
		bPass = bPass && xrtHttpBodyIsReplayable(pBody);
		bPass = bPass && xrtHttpBodyOpen(pBody, pBodyContext, &pBodyReader1) == XHTTP_SEMANTIC_OK;
		bPass = bPass && xrtHttpBodyOpen(pBody, NULL, &pBodyReader2) == XHTTP_SEMANTIC_OK;
		xrtHttpBodyRelease(pBody);
		pBody = NULL;
		if ( pBodyReader1 != NULL ) {
			bPass = bPass && xrtHttpBodyReaderRead(pBodyReader1, sBodyOut, 2u,
				&iBodyRead, pBodyContext) == XHTTP_BODY_READ_DATA;
			bPass = bPass && iBodyRead == 2u && memcmp(sBodyOut, "ab", 2u) == 0;
			bPass = bPass && xrtHttpContextCancel(pBodyContext);
			bPass = bPass && xrtHttpBodyReaderRead(pBodyReader1, sBodyOut, sizeof(sBodyOut),
				&iBodyRead, pBodyContext) == XHTTP_BODY_READ_CANCELLED;
		}
		if ( pBodyReader2 != NULL ) {
			bPass = bPass && xrtHttpBodyReaderRead(pBodyReader2, sBodyOut, sizeof(sBodyOut),
				&iBodyRead, NULL) == XHTTP_BODY_READ_DATA;
			bPass = bPass && iBodyRead == 6u && memcmp(sBodyOut, "abcdef", 6u) == 0;
			bPass = bPass && xrtHttpBodyReaderRead(pBodyReader2, sBodyOut, sizeof(sBodyOut),
				&iBodyRead, NULL) == XHTTP_BODY_READ_EOF;
		}
	}
	xrtHttpBodyReaderClose(pBodyReader1);
	pBodyReader1 = NULL;
	xrtHttpBodyReaderClose(pBodyReader2);
	pBodyReader2 = NULL;

	__g_iHttpSemBodyDataFreeCount = 0;
	pRefBody = xrtHttpBodyCreateBytesRef("ref", 3u, __Test_HttpSemFreeBodyData, NULL);
	bPass = bPass && pRefBody != NULL;
	if ( pRefBody != NULL ) {
		bPass = bPass && xrtHttpBodyOpen(pRefBody, NULL, &pBodyReader1) == XHTTP_SEMANTIC_OK;
		xrtHttpBodyRelease(pRefBody);
		pRefBody = NULL;
		bPass = bPass && __g_iHttpSemBodyDataFreeCount == 0;
		xrtHttpBodyReaderClose(pBodyReader1);
		pBodyReader1 = NULL;
		bPass = bPass && __g_iHttpSemBodyDataFreeCount == 1;
	}

	pEmptyBody = xrtHttpBodyCreateEmpty();
	bPass = bPass && pEmptyBody != NULL;
	if ( pEmptyBody != NULL ) {
		bPass = bPass && xrtHttpBodyOpen(pEmptyBody, NULL, &pBodyReader1) == XHTTP_SEMANTIC_OK;
		bPass = bPass && xrtHttpBodyReaderRead(pBodyReader1, sBodyOut, sizeof(sBodyOut),
			&iBodyRead, NULL) == XHTTP_BODY_READ_EOF;
		xrtHttpBodyReaderClose(pBodyReader1);
		pBodyReader1 = NULL;
	}

	xrtHttpFormDataConfigInit(&tFormConfig);
	bPass = bPass && tFormConfig.iSize == XHTTP_FORM_DATA_CONFIG_V1_SIZE;
	pForm = xrtHttpFormDataCreateEx(&tFormConfig);
	bPass = bPass && pForm != NULL;
	if ( pForm != NULL ) {
		static const char sUtf8FileName[] = "report-\xE4\xB8\xAD.txt";
		xhttpbody* pFileBody = xrtHttpBodyCreateBytesCopy("xyz", 3u);
		bPass = bPass && xrtHttpFormDataAppendText(pForm, "title", "hello") == XHTTP_SEMANTIC_OK;
		bPass = bPass && xrtHttpFormDataAppendText(pForm, "tag", "a") == XHTTP_SEMANTIC_OK;
		bPass = bPass && xrtHttpFormDataAppendText(pForm, "tag", "b") == XHTTP_SEMANTIC_OK;
		bPass = bPass && pFileBody != NULL &&
			xrtHttpFormDataAppendBody(pForm, "upload", pFileBody,
				sUtf8FileName, "text/plain; charset=utf-8") == XHTTP_SEMANTIC_OK;
		bPass = bPass && pFileBody != NULL &&
			xrtHttpFormDataAppendBody(pForm, "bad", pFileBody,
				"bad\r\nname", "text/plain") == XHTTP_SEMANTIC_INVALID_VALUE;
		xrtHttpBodyRelease(pFileBody);
		bPass = bPass && xrtHttpFormDataCount(pForm) == 4u;
		bPass = bPass && xrtHttpFormDataCountName(pForm, "tag") == 2u;
		bPass = bPass && xrtHttpFormDataGet(pForm, "upload", &tFormPart);
		bPass = bPass && (tFormPart.iFlags & XHTTP_FORM_DATA_PART_F_FILENAME) != 0u;
		bPass = bPass && tFormPart.iContentLength == 3;

		pFormClone = xrtHttpFormDataClone(pForm);
		bPass = bPass && pFormClone != NULL;
		if ( pFormClone != NULL ) {
			xhttpbody* pSetBody = xrtHttpBodyCreateBytesCopy("one", 3u);
			xhttprequest tFormRequest;
			bPass = bPass && pSetBody != NULL &&
				xrtHttpFormDataSetBodyN(pFormClone, "tag", 3u, pSetBody,
					NULL, 0u, NULL, 0u) == XHTTP_SEMANTIC_OK;
			xrtHttpBodyRelease(pSetBody);
			bPass = bPass && xrtHttpFormDataCountName(pFormClone, "tag") == 1u;
			bPass = bPass && xrtHttpFormDataCount(pFormClone) == 3u;
			xrtHttpRequestInit(&tFormRequest);
			bPass = bPass && xrtHttpRequestSetBodyCopy(&tFormRequest, "old", 3u, "text/plain");
			bPass = bPass && !xrtHttpRequestSetBodyCopy(&tFormRequest, "new", 3u,
				"text/plain\r\nX-Bad: yes");
			bPass = bPass && tFormRequest.iBodyLen == 3u &&
				memcmp(tFormRequest.pBody, "old", 3u) == 0;
			bPass = bPass && xrtHttpRequestSetFormData(&tFormRequest, pFormClone);
			bPass = bPass && xrtHttpRequestBody(&tFormRequest) != NULL;
			bPass = bPass && xrtHttpRequestHeader(&tFormRequest, "Content-Type") != NULL &&
				strstr(xrtHttpRequestHeader(&tFormRequest, "Content-Type"),
					"multipart/form-data; boundary=") == xrtHttpRequestHeader(&tFormRequest, "Content-Type");
			bPass = bPass && xrtHttpRequestHeaderCount(&tFormRequest) > 0u &&
				xrtHttpRequestHeaderNameAt(&tFormRequest, 0u) != NULL &&
				xrtHttpRequestHeaderValueAt(&tFormRequest, 0u) != NULL;
			xrtHttpRequestUnit(&tFormRequest);
		}

		eResult = xrtHttpFormDataBuildBody(pForm, "xrt-boundary", &pFormBody,
			NULL, 0u, &iBuiltLen);
		bPass = bPass && eResult == XHTTP_SEMANTIC_BUFFER_TOO_SMALL &&
			pFormBody == NULL && iBuiltLen > 30u;
		eResult = xrtHttpFormDataBuildBody(pForm, "xrt-boundary", &pFormBody,
			sFormContentType, sizeof(sFormContentType), &iBuiltLen);
		bPass = bPass && eResult == XHTTP_SEMANTIC_OK && pFormBody != NULL;
		bPass = bPass && strcmp(sFormContentType,
			"multipart/form-data; boundary=xrt-boundary") == 0;
		bPass = bPass && pFormBody != NULL && xrtHttpBodyIsReplayable(pFormBody);
		/* The body is a snapshot and remains stable after source mutation. */
		bPass = bPass && xrtHttpFormDataDelete(pForm, "tag") == 2u;
		bPass = bPass && pFormBody != NULL && __Test_HttpSemReadBody(pFormBody,
			sFormEncoded, sizeof(sFormEncoded), &iFormEncodedLen);
		bPass = bPass && pFormBody != NULL &&
			xrtHttpBodyContentLength(pFormBody) == (int64_t)iFormEncodedLen;
		bPass = bPass && strstr(sFormEncoded, "name=\"tag\"") != NULL;
		bPass = bPass && strstr(sFormEncoded, "filename*=UTF-8''report-%E4%B8%AD.txt") != NULL;
		bPass = bPass && iFormEncodedLen >= 18u &&
			memcmp(sFormEncoded + iFormEncodedLen - 18u, "--xrt-boundary--\r\n", 18u) == 0;

		pParsedForm = xrtHttpFormDataParse(sFormContentType, sFormEncoded,
			iFormEncodedLen, &tFormConfig, &eResult);
		bPass = bPass && pParsedForm != NULL && eResult == XHTTP_SEMANTIC_OK;
		bPass = bPass && pParsedForm != NULL && xrtHttpFormDataCount(pParsedForm) == 4u;
		bPass = bPass && pParsedForm != NULL && xrtHttpFormDataCountName(pParsedForm, "tag") == 2u;
		bPass = bPass && pParsedForm != NULL && xrtHttpFormDataGet(pParsedForm, "upload", &tFormPart);
		bPass = bPass && (tFormPart.iFlags & XHTTP_FORM_DATA_PART_F_FILENAME) != 0u &&
			__Test_HttpSemViewEq(tFormPart.tFileName, sUtf8FileName);
		bPass = bPass && xrtHttpFormDataParse(sFormContentType, sFormEncoded,
			iFormEncodedLen - 4u, &tFormConfig, &eResult) == NULL &&
			eResult == XHTTP_SEMANTIC_INVALID_SYNTAX;
	}
	xrtHttpContextRelease(pChildContext);
	pChildContext = NULL;
	xrtHttpContextRelease(pValueContext);
	pValueContext = NULL;
	xrtHttpContextRelease(pRootContext);
	pRootContext = NULL;
	bPass = bPass && __g_iHttpSemContextValueFreeCount == 1;

done:
	xrtFree(sCookieHeader);
	xrtFree(sBuilt);
	xrtFree(sAllocated);
	xrtHttpSearchParamsDestroy(pParamsLenient);
	xrtHttpSearchParamsDestroy(pParamsClone);
	xrtHttpSearchParamsDestroy(pParams);
	xrtHttpBodyReaderClose(pBodyReader2);
	xrtHttpBodyReaderClose(pBodyReader1);
	xrtHttpBodyRelease(pEmptyBody);
	xrtHttpBodyRelease(pRefBody);
	xrtHttpBodyRelease(pBody);
	xrtHttpBodyRelease(pFormBody);
	xrtHttpFormDataDestroy(pParsedForm);
	xrtHttpFormDataDestroy(pFormClone);
	xrtHttpFormDataDestroy(pForm);
	xrtHttpContextRelease(pBodyContext);
	xrtNetCancelRelease(pExternalCancel);
	xrtHttpContextRelease(pChildContext);
	xrtHttpContextRelease(pValueContext);
	xrtHttpContextRelease(pRootContext);
	xrtHttpCookieJarDestroy(pCookieJar);
	xrtHttpHeadersDestroy(pLimited);
	xrtHttpHeadersDestroy(pClone);
	xrtHttpHeadersDestroy(pHeaders);
	printf("  HTTP semantic values : %s\n", bPass ? "PASS" : "FAIL");
	return bPass;
}

#endif
