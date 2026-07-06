#ifndef XRT_TEST_XURL_H
#define XRT_TEST_XURL_H

// XURL核心测试
static bool Test_XUrlCore(void)
{
	bool bPass = true;
	xrturlview tURL;
	xrturlview tTarget;
	xrturlview tAuthority;
	xrtquerypair tPair;
	xrtheaderpair tHeader;
	xrtheaderpair arrHeaders[4];
	xrtstrview arrHeaderValues[3];
	xrtcookiepair tCookie;
	xrtcookiepair arrCookiePairs[2];
	xrtsetcookieview tSetCookie;
	xrthttpparam tParam;
	xrtmediatypeview tMediaType;
	xrtcontentdispositionview tDisposition;
	xrtmultipartpartview tPart;
	xrtmultipartstreamconfig tStreamCfg;
	xrtmultipartstream tStream;
	xrtmultipartstreamevent tStreamEvent;
	xrthttputillimits tLimits;
	xurl_struct tFixed;
	xrtquerypair arrQueryPairs[3];
	xrtquerypair arrFormPairs[2];
	xrtcookiepair arrCookieParsed[3];
	xrtmultipartpartview arrMultipartParts[2];
	xrtstrview arrTokens[3];
	size_t iOffset = 0u;
	const char sUtf8Name[] = "\xE4\xB8\xAD\xE6\x96\x87" ".txt";
	const char sQuotedRaw[] = "abc\"def\\ghi";
	char sHost[128];
	char sDecoded[64];
	char sBuilt[2048];
	str sAllocated = NULL;
	size_t iDecodedLen = 0u;
	size_t iBuiltLen = 0u;
	xrtstrview tView;
	const char sMultipartBody[] =
		"--boundary123\r\n"
		"Content-Disposition: form-data; name=\"field1\"\r\n"
		"\r\n"
		"value1\r\n"
		"--boundary123\r\n"
		"Content-Disposition: form-data; name=\"file\"; filename=\"a.txt\"\r\n"
		"Content-Type: text/plain\r\n"
		"\r\n"
		"hello file\r\n"
		"--boundary123--\r\n";
	const char sStreamMultipart[] =
		"--streamB\r\n"
		"Content-Disposition: form-data; name=\"field1\"\r\n"
		"\r\n"
		"value1\r\n"
		"--streamB\r\n"
		"Content-Disposition: form-data; name=\"file\"; filename=\"a.txt\"\r\n"
		"Content-Type: text/plain\r\n"
		"\r\n"
		"hello file\r\n"
		"--streamB--\r\n";
	const char sStreamTruncated[] =
		"--streamB\r\n"
		"Content-Disposition: form-data; name=\"field1\"\r\n"
		"\r\n"
		"value1";
	char sStreamBody1[32];
	char sStreamBody2[32];
	size_t iStreamBody1Len = 0u;
	size_t iStreamBody2Len = 0u;
	int iStreamPartIndex = 0;
	int iStreamBeginCount = 0;
	int iStreamEndCount = 0;
	bool bStreamDone = false;

	printf("Testing xurl/http util core...\n");

	memset(&tURL, 0, sizeof(tURL));
	bPass = bPass && xrtUrlParseView("https://example.com:8443/api/v1/users?id=1#frag", &tURL);
	bPass = bPass && ((tURL.iFlags & XRT_URL_F_ABSOLUTE) != 0u);
	bPass = bPass && ((tURL.iFlags & XRT_URL_F_HAS_PORT) != 0u);
	bPass = bPass && ((tURL.iFlags & XRT_URL_F_HAS_QUERY) != 0u);
	bPass = bPass && ((tURL.iFlags & XRT_URL_F_HAS_FRAGMENT) != 0u);
	bPass = bPass && tURL.iPort == 8443u;
	bPass = bPass && tURL.tHost.iLen == 11u && strncmp(tURL.tHost.sPtr, "example.com", 11u) == 0;
	bPass = bPass && tURL.tTarget.iLen == 18u && strncmp(tURL.tTarget.sPtr, "/api/v1/users?id=1", 18u) == 0;
	bPass = bPass && tURL.tQuery.iLen == 4u && strncmp(tURL.tQuery.sPtr, "id=1", 4u) == 0;
	bPass = bPass && tURL.tFragment.iLen == 4u && strncmp(tURL.tFragment.sPtr, "frag", 4u) == 0;

	memset(&tURL, 0, sizeof(tURL));
	bPass = bPass && xrtUrlParseView("wss://[::1]:9443/chat?room=1", &tURL);
	bPass = bPass && tURL.iPort == 9443u;
	bPass = bPass && tURL.tHost.iLen == 3u && strncmp(tURL.tHost.sPtr, "::1", 3u) == 0;
	bPass = bPass && xrtUrlViewMatchesScheme2(&tURL, "ws", "wss");
	bPass = bPass && xrtUrlViewIsScheme(&tURL, "wss");
	bPass = bPass && xrtUrlViewCopyHostTo(&tURL, sDecoded, sizeof(sDecoded));
	bPass = bPass && strcmp(sDecoded, "::1") == 0;
	bPass = bPass && xrtUrlViewCopyTargetTo(&tURL, sDecoded, sizeof(sDecoded));
	bPass = bPass && strcmp(sDecoded, "/chat?room=1") == 0;
	bPass = bPass && xrtUrlViewCopySchemeTo(&tURL, sDecoded, sizeof(sDecoded));
	bPass = bPass && strcmp(sDecoded, "wss") == 0;
	bPass = bPass && xrtUrlViewCopyAuthorityTo(&tURL, sDecoded, sizeof(sDecoded));
	bPass = bPass && strcmp(sDecoded, "[::1]:9443") == 0;
	bPass = bPass && xrtUrlViewCopyPathTo(&tURL, sDecoded, sizeof(sDecoded));
	bPass = bPass && strcmp(sDecoded, "/chat") == 0;
	bPass = bPass && xrtUrlViewCopyQueryTo(&tURL, sDecoded, sizeof(sDecoded));
	bPass = bPass && strcmp(sDecoded, "room=1") == 0;
	bPass = bPass && xrtStrViewCopyTo(xrtStrView("alpha", 5u), sDecoded, sizeof(sDecoded));
	bPass = bPass && strcmp(sDecoded, "alpha") == 0;
	bPass = bPass && xrtUrlMakeHostHeader(&tURL, sHost, sizeof(sHost));
	bPass = bPass && strcmp(sHost, "[::1]:9443") == 0;
	bPass = bPass && xrtUrlMakeHostHeaderFixed("wss", "::1", 9443u, sDecoded, sizeof(sDecoded));
	bPass = bPass && strcmp(sDecoded, "[::1]:9443") == 0;
	iBuiltLen = 0u;
	bPass = bPass && xrtUrlBuild(&tURL, sBuilt, sizeof(sBuilt), &iBuiltLen);
	bPass = bPass && strcmp(sBuilt, "wss://[::1]:9443/chat?room=1") == 0;

	memset(&tTarget, 0, sizeof(tTarget));
	bPass = bPass && xrtUrlParseTarget("/v1/chat/completions?model=gpt&stream=1", &tTarget);
	bPass = bPass && tTarget.tPath.iLen == 20u && strncmp(tTarget.tPath.sPtr, "/v1/chat/completions", 20u) == 0;
	bPass = bPass && tTarget.tQuery.iLen == 18u && strncmp(tTarget.tQuery.sPtr, "model=gpt&stream=1", 18u) == 0;
	bPass = bPass && (tTarget.iFlags & XRT_URL_F_HAS_FRAGMENT) == 0u;
	iBuiltLen = 0u;
	bPass = bPass && xrtUrlBuildTarget(&tTarget, sBuilt, sizeof(sBuilt), &iBuiltLen);
	bPass = bPass && strcmp(sBuilt, "/v1/chat/completions?model=gpt&stream=1") == 0;

	memset(&tAuthority, 0, sizeof(tAuthority));
	bPass = bPass && xrtUrlParseAuthority("[2001:db8::1]:8080", &tAuthority);
	bPass = bPass && tAuthority.iPort == 8080u;
	bPass = bPass && tAuthority.tHost.iLen == 11u && strncmp(tAuthority.tHost.sPtr, "2001:db8::1", 11u) == 0;
	tAuthority.tScheme = xrtStrView("http", 4u);
	iBuiltLen = 0u;
	bPass = bPass && xrtUrlBuildAuthority(&tAuthority, sBuilt, sizeof(sBuilt), &iBuiltLen);
	bPass = bPass && strcmp(sBuilt, "[2001:db8::1]:8080") == 0;
	iBuiltLen = 0u;
	bPass = bPass && xrtUrlNormalizePathTo("/a/b/../c/./d//", 15u, sBuilt, sizeof(sBuilt), &iBuiltLen);
	bPass = bPass && strcmp(sBuilt, "/a/c/d/") == 0;
	iBuiltLen = 0u;
	bPass = bPass && xrtUrlResolve(&tURL, "../rooms/list?page=2", sBuilt, sizeof(sBuilt), &iBuiltLen);
	bPass = bPass && strcmp(sBuilt, "wss://[::1]:9443/rooms/list?page=2") == 0;
	iBuiltLen = 0u;
	bPass = bPass && xrtUrlResolve(&tURL, "?room=2", sBuilt, sizeof(sBuilt), &iBuiltLen);
	bPass = bPass && strcmp(sBuilt, "wss://[::1]:9443/chat?room=2") == 0;
	iBuiltLen = 0u;
	bPass = bPass && xrtUrlResolve(&tURL, "//example.org/ws", sBuilt, sizeof(sBuilt), &iBuiltLen);
	bPass = bPass && strcmp(sBuilt, "wss://example.org/ws") == 0;
	memset(&tURL, 0, sizeof(tURL));
	bPass = bPass && xrtUrlParseView("https://user@example.com:8443/a/b?x=1#frag", &tURL);
	bPass = bPass && xrtUrlViewCopySchemeTo(&tURL, sDecoded, sizeof(sDecoded));
	bPass = bPass && strcmp(sDecoded, "https") == 0;
	bPass = bPass && xrtUrlViewCopyAuthorityTo(&tURL, sDecoded, sizeof(sDecoded));
	bPass = bPass && strcmp(sDecoded, "user@example.com:8443") == 0;
	bPass = bPass && xrtUrlViewCopyPathTo(&tURL, sDecoded, sizeof(sDecoded));
	bPass = bPass && strcmp(sDecoded, "/a/b") == 0;
	bPass = bPass && xrtUrlViewCopyQueryTo(&tURL, sDecoded, sizeof(sDecoded));
	bPass = bPass && strcmp(sDecoded, "x=1") == 0;
	bPass = bPass && xrtUrlViewCopyFragmentTo(&tURL, sDecoded, sizeof(sDecoded));
	bPass = bPass && strcmp(sDecoded, "frag") == 0;
	{
		bool bSecureUrl = false;
		char sFixedHost[64];
		char sFixedTarget[128];
		uint16 iFixedPort = 0u;
		bPass = bPass && xrtUrlParseFixedTo("wss://api.example.com:9443/chat?room=1", "ws", "wss", &bSecureUrl, sFixedHost, sizeof(sFixedHost), &iFixedPort, sFixedTarget, sizeof(sFixedTarget));
		bPass = bPass && bSecureUrl;
		bPass = bPass && iFixedPort == 9443u;
		bPass = bPass && strcmp(sFixedHost, "api.example.com") == 0;
		bPass = bPass && strcmp(sFixedTarget, "/chat?room=1") == 0;
	}

	iOffset = 0u;
	bPass = bPass && xrtQueryNext("a=1&b&c=&a=2", &iOffset, &tPair);
	bPass = bPass && tPair.tKey.iLen == 1u && strncmp(tPair.tKey.sPtr, "a", 1u) == 0;
	bPass = bPass && (tPair.iFlags & XRT_QUERY_F_HAS_VALUE) != 0u;
	bPass = bPass && tPair.tValue.iLen == 1u && strncmp(tPair.tValue.sPtr, "1", 1u) == 0;
	bPass = bPass && xrtQueryNext("a=1&b&c=&a=2", &iOffset, &tPair);
	bPass = bPass && tPair.tKey.iLen == 1u && strncmp(tPair.tKey.sPtr, "b", 1u) == 0;
	bPass = bPass && (tPair.iFlags & XRT_QUERY_F_HAS_VALUE) == 0u;
	bPass = bPass && xrtQueryNext("a=1&b&c=&a=2", &iOffset, &tPair);
	bPass = bPass && tPair.tKey.iLen == 1u && strncmp(tPair.tKey.sPtr, "c", 1u) == 0;
	bPass = bPass && (tPair.iFlags & XRT_QUERY_F_HAS_VALUE) != 0u;
	bPass = bPass && tPair.tValue.iLen == 0u;
	bPass = bPass && xrtQueryCount("a=1&b&c=&a=2") == 4u;
	bPass = bPass && xrtQueryFind("a=1&b&c=&a=2", "c", &tPair);
	bPass = bPass && tPair.tKey.iLen == 1u && strncmp(tPair.tKey.sPtr, "c", 1u) == 0;
	bPass = bPass && (tPair.iFlags & XRT_QUERY_F_HAS_VALUE) != 0u;
	bPass = bPass && tPair.tValue.iLen == 0u;

	bPass = bPass && xrtPercentDecodeTo("hello%20world%2Fok", 18u, sDecoded, sizeof(sDecoded), &iDecodedLen, false);
	bPass = bPass && iDecodedLen == 14u && strcmp(sDecoded, "hello world/ok") == 0;
	bPass = bPass && xrtPercentDecodeTo("a+b", 3u, sDecoded, sizeof(sDecoded), &iDecodedLen, true);
	bPass = bPass && iDecodedLen == 3u && strcmp(sDecoded, "a b") == 0;
	bPass = bPass && !xrtPercentDecodeTo("%GG", 3u, sDecoded, sizeof(sDecoded), &iDecodedLen, false);
	bPass = bPass && xrtPercentEncodeTo("alice bob/ok", 12u, sDecoded, sizeof(sDecoded), &iDecodedLen, true);
	bPass = bPass && strcmp(sDecoded, "alice+bob%2Fok") == 0;
	sAllocated = xrtPercentEncode("alice bob/ok", 12u, true, &iDecodedLen);
	bPass = bPass && sAllocated != NULL && iDecodedLen == 14u && strcmp((char*)sAllocated, "alice+bob%2Fok") == 0;
	xrtFree(sAllocated);
	sAllocated = xrtPercentDecode("hello%20world%2Fok", 18u, false, &iDecodedLen);
	bPass = bPass && sAllocated != NULL && iDecodedLen == 14u && strcmp((char*)sAllocated, "hello world/ok") == 0;
	xrtFree(sAllocated);
	sAllocated = xrtFormUrlEncode("alice bob/ok", 12u, &iDecodedLen);
	bPass = bPass && sAllocated != NULL && iDecodedLen == 14u && strcmp((char*)sAllocated, "alice+bob%2Fok") == 0;
	xrtFree(sAllocated);
	sAllocated = xrtFormUrlDecode("alice+bob%2Fok", 14u, &iDecodedLen);
	bPass = bPass && sAllocated != NULL && iDecodedLen == 12u && strcmp((char*)sAllocated, "alice bob/ok") == 0;
	xrtFree(sAllocated);

	bPass = bPass && xrtHttpHeaderSplitLine("Content-Type: application/json", &tHeader);
	bPass = bPass && tHeader.tName.iLen == 12u && strncmp(tHeader.tName.sPtr, "Content-Type", 12u) == 0;
	bPass = bPass && tHeader.tValue.iLen == 16u && strncmp(tHeader.tValue.sPtr, "application/json", 16u) == 0;
	bPass = bPass && xrtHttpHeaderContainsToken("close, Upgrade", "upgrade");
	bPass = bPass && !xrtHttpHeaderContainsToken("keep-alive", "upgrade");
	iOffset = 0u;
	bPass = bPass && xrtHttpTokenNext("gzip, br, deflate", &iOffset, &tView);
	bPass = bPass && tView.iLen == 4u && strncmp(tView.sPtr, "gzip", 4u) == 0;
	bPass = bPass && xrtHttpTokenNext("gzip, br, deflate", &iOffset, &tView);
	bPass = bPass && tView.iLen == 2u && strncmp(tView.sPtr, "br", 2u) == 0;
	bPass = bPass && xrtHttpTokenNext("gzip, br, deflate", &iOffset, &tView);
	bPass = bPass && tView.iLen == 7u && strncmp(tView.sPtr, "deflate", 7u) == 0;
	bPass = bPass && xrtHttpTokenCount("gzip, br, deflate") == 3u;
	bPass = bPass && xrtHttpTokenFind("gzip, br, deflate", "BR", &tView);
	bPass = bPass && tView.iLen == 2u && strncmp(tView.sPtr, "br", 2u) == 0;
	iOffset = 0u;
	bPass = bPass && xrtHttpTokenParseTo("gzip, br, deflate", arrTokens, 3u, &iOffset);
	bPass = bPass && iOffset == 3u;
	bPass = bPass && arrTokens[0].iLen == 4u && strncmp(arrTokens[0].sPtr, "gzip", 4u) == 0;
	bPass = bPass && arrTokens[1].iLen == 2u && strncmp(arrTokens[1].sPtr, "br", 2u) == 0;
	bPass = bPass && arrTokens[2].iLen == 7u && strncmp(arrTokens[2].sPtr, "deflate", 7u) == 0;
	iBuiltLen = 0u;
	bPass = bPass && xrtHttpTokenAppend(sBuilt, sizeof(sBuilt), &iBuiltLen, "gzip");
	bPass = bPass && xrtHttpTokenAppend(sBuilt, sizeof(sBuilt), &iBuiltLen, "br");
	bPass = bPass && xrtHttpTokenAppend(sBuilt, sizeof(sBuilt), &iBuiltLen, "deflate");
	bPass = bPass && strcmp(sBuilt, "gzip, br, deflate") == 0;
	xrtHttpUtilLimitsInit(&tLimits);
	tLimits.iMaxTokenBytes = 4u;
	bPass = bPass && xrtHttpTokenValidate("gzip, br", &tLimits);
	bPass = bPass && !xrtHttpTokenValidate("gzip, deflate", &tLimits);
	tLimits.iMaxTokenBytes = 256u;
	arrHeaders[0].tName = xrtStrView("Host", 4u);
	arrHeaders[0].tValue = xrtStrView("example.com", 11u);
	arrHeaders[1].tName = xrtStrView("Connection", 10u);
	arrHeaders[1].tValue = xrtStrView("close", 5u);
	bPass = bPass && xrtHttpHeaderFind(arrHeaders, 2u, "connection", &tView);
	bPass = bPass && tView.iLen == 5u && strncmp(tView.sPtr, "close", 5u) == 0;
	bPass = bPass && xrtHttpHeaderFindLine("Host: example.com\r\nConnection: Upgrade\r\n", "connection", &tHeader);
	bPass = bPass && tHeader.tValue.iLen == 7u && strncmp(tHeader.tValue.sPtr, "Upgrade", 7u) == 0;
	iOffset = 0u;
	memset(arrHeaders, 0, sizeof(arrHeaders));
	bPass = bPass && xrtHttpHeaderParseBlockTo("Host: example.com\r\nConnection: close\r\n\r\n", arrHeaders, 2u, &iOffset);
	bPass = bPass && iOffset == 2u;
	bPass = bPass && arrHeaders[0].tName.iLen == 4u && strncmp(arrHeaders[0].tName.sPtr, "Host", 4u) == 0;
	bPass = bPass && arrHeaders[1].tValue.iLen == 5u && strncmp(arrHeaders[1].tValue.sPtr, "close", 5u) == 0;
	iBuiltLen = 0u;
	bPass = bPass && xrtHttpHeaderBuildLine("Content-Type", "application/json", sBuilt, sizeof(sBuilt), &iBuiltLen);
	bPass = bPass && strcmp(sBuilt, "Content-Type: application/json\r\n") == 0;
	iBuiltLen = 0u;
	bPass = bPass && xrtHttpHeaderBuildCanonicalLineTo("content-type", "application/json", sBuilt, sizeof(sBuilt), &iBuiltLen);
	bPass = bPass && strcmp(sBuilt, "Content-Type: application/json\r\n") == 0;
	iBuiltLen = 0u;
	bPass = bPass && xrtHttpHeaderBuildBlockTo(arrHeaders, 2u, sBuilt, sizeof(sBuilt), &iBuiltLen);
	bPass = bPass && strcmp(sBuilt, "Host: example.com\r\nConnection: close\r\n\r\n") == 0;
	iBuiltLen = 0u;
	bPass = bPass && xrtHttpHeaderBuildCanonicalBlockTo(arrHeaders, 2u, sBuilt, sizeof(sBuilt), &iBuiltLen);
	bPass = bPass && strcmp(sBuilt, "Host: example.com\r\nConnection: close\r\n\r\n") == 0;
	iOffset = 2u;
	bPass = bPass && !xrtHttpHeaderAppendPair(arrHeaders, 2u, &iOffset, "X-Test", "1");
	iOffset = 0u;
	bPass = bPass && xrtHttpHeaderAppendPair(arrHeaders, 4u, &iOffset, "Host", "example.com");
	bPass = bPass && xrtHttpHeaderAppendPair(arrHeaders, 4u, &iOffset, "Connection", "close");
	bPass = bPass && xrtHttpHeaderSetPair(arrHeaders, 4u, &iOffset, "Connection", "keep-alive");
	bPass = bPass && xrtHttpHeaderFind(arrHeaders, iOffset, "connection", &tView);
	bPass = bPass && tView.iLen == 10u && strncmp(tView.sPtr, "keep-alive", 10u) == 0;
	bPass = bPass && xrtHttpHeaderSetPair(arrHeaders, 4u, &iOffset, "Upgrade", "websocket");
	bPass = bPass && iOffset == 3u;
	bPass = bPass && xrtHttpHeaderCount(arrHeaders, iOffset, "connection") == 1u;
	bPass = bPass && xrtHttpHeaderFindNth(arrHeaders, iOffset, "connection", 0u, &tView);
	bPass = bPass && tView.iLen == 10u && strncmp(tView.sPtr, "keep-alive", 10u) == 0;
	memset(arrHeaderValues, 0, sizeof(arrHeaderValues));
	bPass = bPass && xrtHttpHeaderAppendPair(arrHeaders, 4u, &iOffset, "Connection", "upgrade");
	bPass = bPass && xrtHttpHeaderCount(arrHeaders, iOffset, "connection") == 2u;
	bPass = bPass && xrtHttpHeaderFindNth(arrHeaders, iOffset, "connection", 1u, &tView);
	bPass = bPass && tView.iLen == 7u && strncmp(tView.sPtr, "upgrade", 7u) == 0;
	bPass = bPass && xrtHttpHeaderFindAllTo(arrHeaders, iOffset, "connection", arrHeaderValues, 3u) == 2u;
	bPass = bPass && arrHeaderValues[0].iLen == 10u && strncmp(arrHeaderValues[0].sPtr, "keep-alive", 10u) == 0;
	bPass = bPass && arrHeaderValues[1].iLen == 7u && strncmp(arrHeaderValues[1].sPtr, "upgrade", 7u) == 0;
	iBuiltLen = 0u;
	bPass = bPass && xrtHttpHeaderJoinValuesTo(arrHeaderValues, 2u, sBuilt, sizeof(sBuilt), &iBuiltLen);
	bPass = bPass && strcmp(sBuilt, "keep-alive, upgrade") == 0;
	iBuiltLen = 0u;
	bPass = bPass && xrtHttpHeaderCollectAndJoinTo(arrHeaders, iOffset, "connection", sBuilt, sizeof(sBuilt), &iBuiltLen);
	bPass = bPass && strcmp(sBuilt, "keep-alive, upgrade") == 0;
	iBuiltLen = 0u;
	bPass = bPass && xrtHttpHeaderCanonicalizeNameTo("content-type", sBuilt, sizeof(sBuilt), &iBuiltLen);
	bPass = bPass && strcmp(sBuilt, "Content-Type") == 0;
	bPass = bPass && xrtHttpHeaderRemove(arrHeaders, &iOffset, "host") == 1u;
	bPass = bPass && iOffset == 3u;
	bPass = bPass && !xrtHttpHeaderFind(arrHeaders, iOffset, "host", &tView);
	xrtHttpUtilLimitsInit(&tLimits);
	tLimits.iMaxHeaderCount = 1u;
	bPass = bPass && !xrtHttpHeaderBlockValidate("Host: example.com\r\nConnection: close\r\n", &tLimits);
	tLimits.iMaxHeaderCount = 8u;
	tLimits.iMaxHeaderLineBytes = 16u;
	bPass = bPass && !xrtHttpHeaderBlockValidate("X-Long: 1234567890123456\r\n", &tLimits);
	tLimits.iMaxHeaderLineBytes = 8192u;

	iOffset = 0u;
	bPass = bPass && xrtCookieNext("a=1; theme=light; empty=", &iOffset, &tCookie);
	bPass = bPass && tCookie.tName.iLen == 1u && strncmp(tCookie.tName.sPtr, "a", 1u) == 0;
	bPass = bPass && tCookie.tValue.iLen == 1u && strncmp(tCookie.tValue.sPtr, "1", 1u) == 0;
	bPass = bPass && xrtCookieNext("a=1; theme=light; empty=", &iOffset, &tCookie);
	bPass = bPass && tCookie.tName.iLen == 5u && strncmp(tCookie.tName.sPtr, "theme", 5u) == 0;
	bPass = bPass && tCookie.tValue.iLen == 5u && strncmp(tCookie.tValue.sPtr, "light", 5u) == 0;
	bPass = bPass && xrtCookieNext("a=1; theme=light; empty=", &iOffset, &tCookie);
	bPass = bPass && tCookie.tName.iLen == 5u && strncmp(tCookie.tName.sPtr, "empty", 5u) == 0;
	bPass = bPass && tCookie.tValue.iLen == 0u;
	bPass = bPass && xrtCookieFind("a=1; theme=light; empty=", "theme", &tCookie);
	bPass = bPass && tCookie.tValue.iLen == 5u && strncmp(tCookie.tValue.sPtr, "light", 5u) == 0;
	iOffset = 0u;
	bPass = bPass && xrtCookieParseTo("a=1; theme=light; empty=", arrCookieParsed, 3u, &iOffset);
	bPass = bPass && iOffset == 3u;
	bPass = bPass && arrCookieParsed[0].tName.iLen == 1u && strncmp(arrCookieParsed[0].tName.sPtr, "a", 1u) == 0;
	bPass = bPass && arrCookieParsed[1].tName.iLen == 5u && strncmp(arrCookieParsed[1].tName.sPtr, "theme", 5u) == 0;
	bPass = bPass && arrCookieParsed[2].tName.iLen == 5u && strncmp(arrCookieParsed[2].tName.sPtr, "empty", 5u) == 0;
	xrtHttpUtilLimitsInit(&tLimits);
	tLimits.iMaxNameBytes = 4u;
	bPass = bPass && !xrtCookieValidate("theme=light", &tLimits);
	tLimits.iMaxNameBytes = 256u;
	tLimits.iMaxPairs = 2u;
	bPass = bPass && !xrtCookieValidate("a=1; b=2; c=3", &tLimits);
	tLimits.iMaxPairs = 128u;

	memset(&tSetCookie, 0, sizeof(tSetCookie));
	bPass = bPass && xrtSetCookieParse("sid=abc123; Path=/; HttpOnly; Secure; SameSite=Lax; Max-Age=60", &tSetCookie);
	bPass = bPass && tSetCookie.tName.iLen == 3u && strncmp(tSetCookie.tName.sPtr, "sid", 3u) == 0;
	bPass = bPass && tSetCookie.tValue.iLen == 6u && strncmp(tSetCookie.tValue.sPtr, "abc123", 6u) == 0;
	bPass = bPass && (tSetCookie.iFlags & XRT_SET_COOKIE_F_HAS_PATH) != 0u;
	bPass = bPass && (tSetCookie.iFlags & XRT_SET_COOKIE_F_HTTP_ONLY) != 0u;
	bPass = bPass && (tSetCookie.iFlags & XRT_SET_COOKIE_F_SECURE) != 0u;
	bPass = bPass && (tSetCookie.iFlags & XRT_SET_COOKIE_F_HAS_MAX_AGE) != 0u;
	bPass = bPass && tSetCookie.iMaxAge == 60;
	bPass = bPass && tSetCookie.iSameSite == XRT_SAME_SITE_LAX;
	bPass = bPass && xrtSetCookieParseLine("Set-Cookie: theme=dark; Domain=example.com; Partitioned", &tSetCookie);
	bPass = bPass && (tSetCookie.iFlags & XRT_SET_COOKIE_F_HAS_DOMAIN) != 0u;
	bPass = bPass && (tSetCookie.iFlags & XRT_SET_COOKIE_F_PARTITIONED) != 0u;
	bPass = bPass && xrtSetCookieParse("sid=abc123; SameParty; Priority=High", &tSetCookie);
	bPass = bPass && (tSetCookie.iFlags & XRT_SET_COOKIE_F_SAME_PARTY) != 0u;
	bPass = bPass && (tSetCookie.iFlags & XRT_SET_COOKIE_F_HAS_PRIORITY) != 0u;
	xrtHttpUtilLimitsInit(&tLimits);
	tLimits.iMaxValueBytes = 4u;
	bPass = bPass && !xrtSetCookieValidate("sid=abc123", &tLimits);
	tLimits.iMaxValueBytes = 4096u;
	bPass = bPass && tSetCookie.iPriority == XRT_COOKIE_PRIORITY_HIGH;
	bPass = bPass && !xrtSetCookieParse("bad=\r\nx", &tSetCookie);
	iOffset = 0u;
	memset(&tParam, 0, sizeof(tParam));
	bPass = bPass && xrtHttpParamNext("charset=UTF-8; boundary=\"abc123\"; secure", &iOffset, &tParam);
	bPass = bPass && (tParam.iFlags & XRT_HTTP_PARAM_F_HAS_VALUE) != 0u;
	bPass = bPass && tParam.tName.iLen == 7u && strncmp(tParam.tName.sPtr, "charset", 7u) == 0;
	bPass = bPass && tParam.tValue.iLen == 5u && strncmp(tParam.tValue.sPtr, "UTF-8", 5u) == 0;
	bPass = bPass && xrtHttpParamNext("charset=UTF-8; boundary=\"abc123\"; secure", &iOffset, &tParam);
	bPass = bPass && (tParam.iFlags & XRT_HTTP_PARAM_F_HAS_VALUE) != 0u;
	bPass = bPass && tParam.tName.iLen == 8u && strncmp(tParam.tName.sPtr, "boundary", 8u) == 0;
	bPass = bPass && tParam.tValue.iLen == 6u && strncmp(tParam.tValue.sPtr, "abc123", 6u) == 0;
	bPass = bPass && xrtHttpParamNext("charset=UTF-8; boundary=\"abc123\"; secure", &iOffset, &tParam);
	bPass = bPass && (tParam.iFlags & XRT_HTTP_PARAM_F_HAS_VALUE) == 0u;
	bPass = bPass && tParam.tName.iLen == 6u && strncmp(tParam.tName.sPtr, "secure", 6u) == 0;
	bPass = bPass && xrtHttpParamCount("charset=UTF-8; boundary=\"abc123\"; secure") == 3u;
	bPass = bPass && xrtHttpParamFind("charset=UTF-8; boundary=\"abc123\"; secure", "boundary", &tParam);
	bPass = bPass && tParam.tValue.iLen == 6u && strncmp(tParam.tValue.sPtr, "abc123", 6u) == 0;
	iBuiltLen = 0u;
	bPass = bPass && xrtHttpParamAppendPair(sBuilt, sizeof(sBuilt), &iBuiltLen, "charset", "UTF-8", true, false);
	bPass = bPass && xrtHttpParamAppendPair(sBuilt, sizeof(sBuilt), &iBuiltLen, "boundary", "abc123", true, true);
	bPass = bPass && xrtHttpParamAppendPair(sBuilt, sizeof(sBuilt), &iBuiltLen, "secure", NULL, false, false);
	bPass = bPass && strcmp(sBuilt, "charset=UTF-8; boundary=\"abc123\"; secure") == 0;
	bPass = bPass && xrtHttpIsToken("gzip");
	bPass = bPass && !xrtHttpIsToken("bad token");
	iBuiltLen = 0u;
	bPass = bPass && xrtHttpQuotedStringBuildTo(sQuotedRaw, sBuilt, sizeof(sBuilt), &iBuiltLen);
	bPass = bPass && strcmp(sBuilt, "\"abc\\\"def\\\\ghi\"") == 0;
	iDecodedLen = 0u;
	bPass = bPass && xrtHttpQuotedStringDecodeTo(sBuilt, sDecoded, sizeof(sDecoded), &iDecodedLen);
	bPass = bPass && iDecodedLen == sizeof(sQuotedRaw) - 1u && memcmp(sDecoded, sQuotedRaw, sizeof(sQuotedRaw) - 1u) == 0;
	bPass = bPass && !xrtHttpQuotedStringDecodeTo("\"unterminated\\", sDecoded, sizeof(sDecoded), &iDecodedLen);
	memset(&tMediaType, 0, sizeof(tMediaType));
	bPass = bPass && xrtHttpMediaTypeParse("application/ld+json; charset=UTF-8; profile=\"https://x.example/p\"", &tMediaType);
	bPass = bPass && tMediaType.tType.iLen == 11u && strncmp(tMediaType.tType.sPtr, "application", 11u) == 0;
	bPass = bPass && tMediaType.tSubType.iLen == 2u && strncmp(tMediaType.tSubType.sPtr, "ld", 2u) == 0;
	bPass = bPass && (tMediaType.iFlags & XRT_HTTP_MEDIA_TYPE_F_HAS_SUFFIX) != 0u;
	bPass = bPass && tMediaType.tSuffix.iLen == 4u && strncmp(tMediaType.tSuffix.sPtr, "json", 4u) == 0;
	bPass = bPass && (tMediaType.iFlags & XRT_HTTP_MEDIA_TYPE_F_HAS_PARAMS) != 0u;
	bPass = bPass && xrtHttpMediaTypeFindParam(&tMediaType, "charset", &tParam);
	bPass = bPass && tParam.tValue.iLen == 5u && strncmp(tParam.tValue.sPtr, "UTF-8", 5u) == 0;
	iBuiltLen = 0u;
	bPass = bPass && xrtHttpMediaTypeBuild(&tMediaType, sBuilt, sizeof(sBuilt), &iBuiltLen);
	bPass = bPass && strcmp(sBuilt, "application/ld+json; charset=UTF-8; profile=\"https://x.example/p\"") == 0;
	memset(&tDisposition, 0, sizeof(tDisposition));
	bPass = bPass && xrtHttpContentDispositionParse("form-data; name=\"file\"; filename=\"fallback.txt\"; filename*=UTF-8''%E4%B8%AD%E6%96%87.txt", &tDisposition);
	bPass = bPass && tDisposition.tType.iLen == 9u && strncmp(tDisposition.tType.sPtr, "form-data", 9u) == 0;
	bPass = bPass && (tDisposition.iFlags & XRT_HTTP_CONTENT_DISPOSITION_F_HAS_NAME) != 0u;
	bPass = bPass && (tDisposition.iFlags & XRT_HTTP_CONTENT_DISPOSITION_F_HAS_FILENAME) != 0u;
	bPass = bPass && (tDisposition.iFlags & XRT_HTTP_CONTENT_DISPOSITION_F_HAS_FILENAME_EXT) != 0u;
	bPass = bPass && tDisposition.tName.iLen == 4u && strncmp(tDisposition.tName.sPtr, "file", 4u) == 0;
	bPass = bPass && tDisposition.tFileName.iLen == 12u && strncmp(tDisposition.tFileName.sPtr, "fallback.txt", 12u) == 0;
	bPass = bPass && tDisposition.tFileNameCharset.iLen == 5u && strncmp(tDisposition.tFileNameCharset.sPtr, "UTF-8", 5u) == 0;
	bPass = bPass && xrtHttpContentDispositionDecodeFileNameTo(&tDisposition, sDecoded, sizeof(sDecoded), &iDecodedLen);
	bPass = bPass && iDecodedLen == sizeof(sUtf8Name) - 1u && memcmp(sDecoded, sUtf8Name, sizeof(sUtf8Name) - 1u) == 0;
	iBuiltLen = 0u;
	bPass = bPass && xrtHttpContentDispositionBuild(&tDisposition, sBuilt, sizeof(sBuilt), &iBuiltLen);
	bPass = bPass && strstr(sBuilt, "form-data") == sBuilt;
	bPass = bPass && strstr(sBuilt, "name=\"file\"") != NULL;
	bPass = bPass && strstr(sBuilt, "filename=\"fallback.txt\"") != NULL;
	bPass = bPass && strstr(sBuilt, "filename*=UTF-8''%55%54%46%2D%38") == NULL;
	bPass = bPass && strstr(sBuilt, "filename*=UTF-8''%E4%B8%AD%E6%96%87.txt") != NULL;
	bPass = bPass && xrtMultipartBoundaryFromContentType("multipart/form-data; boundary=\"boundary123\"", &tView);
	bPass = bPass && tView.iLen == 11u && strncmp(tView.sPtr, "boundary123", 11u) == 0;
	iBuiltLen = 0u;
	bPass = bPass && xrtMultipartBuildContentType("boundary123", sBuilt, sizeof(sBuilt), &iBuiltLen);
	bPass = bPass && strcmp(sBuilt, "multipart/form-data; boundary=boundary123") == 0;
	iBuiltLen = 0u;
	bPass = bPass && xrtHttpBuildExtValueTo("UTF-8", "", sUtf8Name, sizeof(sUtf8Name) - 1u, sBuilt, sizeof(sBuilt), &iBuiltLen);
	bPass = bPass && strcmp(sBuilt, "UTF-8''%E4%B8%AD%E6%96%87.txt") == 0;
	bPass = bPass && xrtHttpDecodeExtValueTo(sBuilt, iBuiltLen, &tView, &tTarget.tQuery, sDecoded, sizeof(sDecoded), &iDecodedLen);
	bPass = bPass && tView.iLen == 5u && strncmp(tView.sPtr, "UTF-8", 5u) == 0;
	bPass = bPass && tTarget.tQuery.iLen == 0u;
	bPass = bPass && iDecodedLen == sizeof(sUtf8Name) - 1u && memcmp(sDecoded, sUtf8Name, sizeof(sUtf8Name) - 1u) == 0;

	iOffset = 0u;
	bPass = bPass && xrtFormUrlEncodedNext("name=alice+bob&city=shanghai", &iOffset, &tPair);
	bPass = bPass && tPair.tKey.iLen == 4u && strncmp(tPair.tKey.sPtr, "name", 4u) == 0;
	bPass = bPass && xrtFormUrlEncodedDecodeTo(tPair.tValue.sPtr, tPair.tValue.iLen, sDecoded, sizeof(sDecoded), &iDecodedLen);
	bPass = bPass && strcmp(sDecoded, "alice bob") == 0;
	iOffset = 0u;
	bPass = bPass && xrtFormUrlEncodedParseTo("name=alice+bob&city=shanghai", arrFormPairs, 2u, &iOffset);
	bPass = bPass && iOffset == 2u;
	bPass = bPass && arrFormPairs[0].tKey.iLen == 4u && strncmp(arrFormPairs[0].tKey.sPtr, "name", 4u) == 0;
	bPass = bPass && arrFormPairs[1].tKey.iLen == 4u && strncmp(arrFormPairs[1].tKey.sPtr, "city", 4u) == 0;
	iBuiltLen = 0u;
	bPass = bPass && xrtQueryParseTo("?model=gpt-5&stream=1", arrQueryPairs, 3u, &iOffset);
	bPass = bPass && iOffset == 2u;
	bPass = bPass && arrQueryPairs[0].tKey.iLen == 5u && strncmp(arrQueryPairs[0].tKey.sPtr, "model", 5u) == 0;
	bPass = bPass && arrQueryPairs[1].tKey.iLen == 6u && strncmp(arrQueryPairs[1].tKey.sPtr, "stream", 6u) == 0;
	iBuiltLen = 0u;
	bPass = bPass && xrtQueryAppendPair(sBuilt, sizeof(sBuilt), &iBuiltLen, "model", "gpt-5");
	bPass = bPass && xrtQueryAppendPairTo(sBuilt, sizeof(sBuilt), &iBuiltLen, "stream", 6u, "1", 1u, true, false);
	bPass = bPass && strcmp(sBuilt, "model=gpt-5&stream=1") == 0;
	iBuiltLen = 0u;
	bPass = bPass && xrtFormUrlEncodedAppendField(sBuilt, sizeof(sBuilt), &iBuiltLen, "name", "alice bob");
	bPass = bPass && xrtFormUrlEncodedAppendField(sBuilt, sizeof(sBuilt), &iBuiltLen, "city", "shanghai");
	bPass = bPass && strcmp(sBuilt, "name=alice+bob&city=shanghai") == 0;
	iBuiltLen = 0u;
	bPass = bPass && xrtCookieAppendPair(sBuilt, sizeof(sBuilt), &iBuiltLen, "sid", "abc123");
	bPass = bPass && xrtCookieAppendPair(sBuilt, sizeof(sBuilt), &iBuiltLen, "theme", "light");
	bPass = bPass && strcmp(sBuilt, "sid=abc123; theme=light") == 0;
	arrCookiePairs[0].tName = xrtStrView("sid", 3u);
	arrCookiePairs[0].tValue = xrtStrView("abc123", 6u);
	arrCookiePairs[1].tName = xrtStrView("theme", 5u);
	arrCookiePairs[1].tValue = xrtStrView("light", 5u);
	iBuiltLen = 0u;
	bPass = bPass && xrtCookieBuildTo(arrCookiePairs, 2u, sBuilt, sizeof(sBuilt), &iBuiltLen);
	bPass = bPass && strcmp(sBuilt, "sid=abc123; theme=light") == 0;
	arrQueryPairs[0].iFlags = XRT_QUERY_F_HAS_VALUE;
	arrQueryPairs[0].tKey = xrtStrView("model", 5u);
	arrQueryPairs[0].tValue = xrtStrView("gpt-5", 5u);
	arrQueryPairs[1].iFlags = 0u;
	arrQueryPairs[1].tKey = xrtStrView("stream", 6u);
	arrQueryPairs[1].tValue = xrtStrView(NULL, 0u);
	arrQueryPairs[2].iFlags = XRT_QUERY_F_HAS_VALUE;
	arrQueryPairs[2].tKey = xrtStrView("city", 4u);
	arrQueryPairs[2].tValue = xrtStrView("shanghai", 8u);
	iBuiltLen = 0u;
	bPass = bPass && xrtQueryBuildTo(arrQueryPairs, 3u, sBuilt, sizeof(sBuilt), &iBuiltLen);
	bPass = bPass && strcmp(sBuilt, "model=gpt-5&stream&city=shanghai") == 0;
	iBuiltLen = 0u;
	bPass = bPass && xrtFormUrlEncodedBuildTo(arrQueryPairs, 3u, sBuilt, sizeof(sBuilt), &iBuiltLen);
	bPass = bPass && strcmp(sBuilt, "model=gpt-5&stream&city=shanghai") == 0;
	memset(&tSetCookie, 0, sizeof(tSetCookie));
	tSetCookie.tName = xrtStrView("sid", 3u);
	tSetCookie.tValue = xrtStrView("abc123", 6u);
	tSetCookie.tPath = xrtStrView("/", 1u);
	tSetCookie.iPriority = XRT_COOKIE_PRIORITY_HIGH;
	tSetCookie.iFlags = XRT_SET_COOKIE_F_HAS_VALUE | XRT_SET_COOKIE_F_HAS_PATH | XRT_SET_COOKIE_F_HTTP_ONLY | XRT_SET_COOKIE_F_SECURE | XRT_SET_COOKIE_F_SAME_PARTY | XRT_SET_COOKIE_F_HAS_PRIORITY;
	iBuiltLen = 0u;
	bPass = bPass && xrtSetCookieBuildTo(&tSetCookie, sBuilt, sizeof(sBuilt), &iBuiltLen);
	bPass = bPass && strcmp(sBuilt, "sid=abc123; Path=/; Secure; HttpOnly; SameParty; Priority=High") == 0;
	iBuiltLen = 0u;
	bPass = bPass && xrtSetCookieBuildLineTo(&tSetCookie, sBuilt, sizeof(sBuilt), &iBuiltLen);
	bPass = bPass && strcmp(sBuilt, "Set-Cookie: sid=abc123; Path=/; Secure; HttpOnly; SameParty; Priority=High\r\n") == 0;
	tSetCookie.iMaxAge = 60;
	tSetCookie.iSameSite = XRT_SAME_SITE_LAX;
	tSetCookie.iFlags |= XRT_SET_COOKIE_F_HAS_MAX_AGE | XRT_SET_COOKIE_F_HAS_SAME_SITE;
	iBuiltLen = 0u;
	bPass = bPass && xrtSetCookieBuildTo(&tSetCookie, sBuilt, sizeof(sBuilt), &iBuiltLen);
	bPass = bPass && strcmp(sBuilt, "sid=abc123; Path=/; Max-Age=60; SameSite=Lax; Secure; HttpOnly; SameParty; Priority=High") == 0;
	iOffset = 0u;
	memset(&tPart, 0, sizeof(tPart));
	bPass = bPass && xrtMultipartNextN(sMultipartBody, sizeof(sMultipartBody) - 1u, "boundary123", 11u, &iOffset, &tPart);
	bPass = bPass && (tPart.iFlags & XRT_MULTIPART_F_HAS_NAME) != 0u;
	bPass = bPass && tPart.tName.iLen == 6u && strncmp(tPart.tName.sPtr, "field1", 6u) == 0;
	bPass = bPass && tPart.tBody.iLen == 6u && strncmp(tPart.tBody.sPtr, "value1", 6u) == 0;
	memset(&tPart, 0, sizeof(tPart));
	bPass = bPass && xrtMultipartNextN(sMultipartBody, sizeof(sMultipartBody) - 1u, "boundary123", 11u, &iOffset, &tPart);
	bPass = bPass && (tPart.iFlags & XRT_MULTIPART_F_HAS_FILENAME) != 0u;
	bPass = bPass && (tPart.iFlags & XRT_MULTIPART_F_HAS_CONTENT_TYPE) != 0u;
	bPass = bPass && tPart.tName.iLen == 4u && strncmp(tPart.tName.sPtr, "file", 4u) == 0;
	bPass = bPass && tPart.tFileName.iLen == 5u && strncmp(tPart.tFileName.sPtr, "a.txt", 5u) == 0;
	bPass = bPass && tPart.tContentType.iLen == 10u && strncmp(tPart.tContentType.sPtr, "text/plain", 10u) == 0;
	bPass = bPass && tPart.tBody.iLen == 10u && strncmp(tPart.tBody.sPtr, "hello file", 10u) == 0;
	memset(&tHeader, 0, sizeof(tHeader));
	bPass = bPass && xrtHttpHeaderFindLineN(tPart.tHeaders.sPtr, tPart.tHeaders.iLen, "Content-Type", &tHeader);
	bPass = bPass && tHeader.tValue.iLen == 10u && strncmp(tHeader.tValue.sPtr, "text/plain", 10u) == 0;
	memset(&tPart, 0, sizeof(tPart));
	bPass = bPass && !xrtMultipartNextN(sMultipartBody, sizeof(sMultipartBody) - 1u, "boundary123", 11u, &iOffset, &tPart);
	iOffset = 0u;
	bPass = bPass && xrtMultipartParseTo(sMultipartBody, "boundary123", arrMultipartParts, 2u, &iOffset);
	bPass = bPass && iOffset == 2u;
	bPass = bPass && (arrMultipartParts[0].iFlags & XRT_MULTIPART_F_HAS_NAME) != 0u;
	bPass = bPass && arrMultipartParts[0].tName.iLen == 6u && strncmp(arrMultipartParts[0].tName.sPtr, "field1", 6u) == 0;
	bPass = bPass && (arrMultipartParts[1].iFlags & XRT_MULTIPART_F_HAS_FILENAME) != 0u;
	bPass = bPass && arrMultipartParts[1].tFileName.iLen == 5u && strncmp(arrMultipartParts[1].tFileName.sPtr, "a.txt", 5u) == 0;
	xrtHttpUtilLimitsInit(&tLimits);
	tLimits.iMaxMultipartParts = 1u;
	bPass = bPass && !xrtMultipartValidate(sMultipartBody, "boundary123", &tLimits);
	tLimits.iMaxMultipartParts = 8u;
	tLimits.iMaxBoundaryBytes = 5u;
	bPass = bPass && !xrtMultipartValidate(sMultipartBody, "boundary123", &tLimits);
	tLimits.iMaxBoundaryBytes = 70u;
	{
		const char sBadMultipart[] =
			"--boundary123\r\n"
			"Content-Disposition: form-datax; name=\"field1\"\r\n"
			"\r\n"
			"value1\r\n"
			"--boundary123--\r\n";
		iOffset = 0u;
		memset(&tPart, 0, sizeof(tPart));
		bPass = bPass && !xrtMultipartNextN(sBadMultipart, sizeof(sBadMultipart) - 1u, "boundary123", 11u, &iOffset, &tPart);
	}
	{
		const char sMultipartExt[] =
			"--boundary123\r\n"
			"Content-Disposition: form-data; name=\"file\"; filename*=UTF-8''%E4%B8%AD%E6%96%87.txt\r\n"
			"\r\n"
			"payload\r\n"
			"--boundary123--\r\n";
		iOffset = 0u;
		memset(&tPart, 0, sizeof(tPart));
		bPass = bPass && xrtMultipartNextN(sMultipartExt, sizeof(sMultipartExt) - 1u, "boundary123", 11u, &iOffset, &tPart);
		bPass = bPass && (tPart.iFlags & XRT_MULTIPART_F_HAS_FILENAME_EXT) != 0u;
		bPass = bPass && tPart.tFileNameExt.iLen == 29u && strncmp(tPart.tFileNameExt.sPtr, "UTF-8''%E4%B8%AD%E6%96%87.txt", 29u) == 0;
		bPass = bPass && tPart.tFileNameCharset.iLen == 5u && strncmp(tPart.tFileNameCharset.sPtr, "UTF-8", 5u) == 0;
		bPass = bPass && xrtHttpDecodeExtValueTo(tPart.tFileNameExt.sPtr, tPart.tFileNameExt.iLen, NULL, NULL, sDecoded, sizeof(sDecoded), &iDecodedLen);
		bPass = bPass && iDecodedLen == sizeof(sUtf8Name) - 1u && memcmp(sDecoded, sUtf8Name, sizeof(sUtf8Name) - 1u) == 0;
		bPass = bPass && xrtMultipartDecodeFileNameTo(&tPart, sDecoded, sizeof(sDecoded), &iDecodedLen);
		bPass = bPass && iDecodedLen == sizeof(sUtf8Name) - 1u && memcmp(sDecoded, sUtf8Name, sizeof(sUtf8Name) - 1u) == 0;
	}
	xrtHttpUtilLimitsInit(&tLimits);
	tLimits.iMaxPairs = 2u;
	bPass = bPass && !xrtQueryValidate("a=1&b=2&c=3", &tLimits);
	tLimits.iMaxPairs = 8u;
	tLimits.iMaxNameBytes = 1u;
	bPass = bPass && !xrtQueryValidate("aa=1", &tLimits);
	tLimits.iMaxNameBytes = 256u;
	tLimits.iMaxValueBytes = 1u;
	bPass = bPass && !xrtFormUrlEncodedValidate("a=22", &tLimits);
	tLimits.iMaxValueBytes = 4096u;
	tLimits.iMaxPairs = 4u;
	bPass = bPass && xrtHttpParamValidate("charset=UTF-8; boundary=abc123; secure", &tLimits);
	tLimits.iMaxNameBytes = 6u;
	bPass = bPass && !xrtHttpParamValidate("charset=UTF-8; boundary=abc123", &tLimits);
	xrtHttpUtilLimitsInit(&tLimits);
	xrtMultipartStreamConfigInit(&tStreamCfg);
	xrtMultipartStreamConfigApplyLimits(&tStreamCfg, &tLimits);
	bPass = bPass && tStreamCfg.iMaxBufferedBytes == tLimits.iMaxMultipartBytes;
	bPass = bPass && tStreamCfg.iMaxHeaderBytes == tLimits.iMaxHeaderBytes;
	bPass = bPass && tStreamCfg.iMaxPartHeaders == tLimits.iMaxMultipartHeaders;
	iBuiltLen = 0u;
	bPass = bPass && xrtMultipartAppendFieldPart(sBuilt, sizeof(sBuilt), &iBuiltLen, "boundaryZ", "field1", "value1");
	bPass = bPass && xrtMultipartAppendFilePart(sBuilt, sizeof(sBuilt), &iBuiltLen, "boundaryZ", "file", "a.txt", "text/plain", "hello file", 10u);
	bPass = bPass && xrtMultipartAppendFinish(sBuilt, sizeof(sBuilt), &iBuiltLen, "boundaryZ");
	iOffset = 0u;
	memset(&tPart, 0, sizeof(tPart));
	bPass = bPass && xrtMultipartNextN(sBuilt, iBuiltLen, "boundaryZ", 9u, &iOffset, &tPart);
	bPass = bPass && (tPart.iFlags & XRT_MULTIPART_F_HAS_NAME) != 0u;
	bPass = bPass && tPart.tName.iLen == 6u && strncmp(tPart.tName.sPtr, "field1", 6u) == 0;
	bPass = bPass && tPart.tBody.iLen == 6u && strncmp(tPart.tBody.sPtr, "value1", 6u) == 0;
	memset(&tPart, 0, sizeof(tPart));
	bPass = bPass && xrtMultipartNextN(sBuilt, iBuiltLen, "boundaryZ", 9u, &iOffset, &tPart);
	bPass = bPass && (tPart.iFlags & XRT_MULTIPART_F_HAS_FILENAME) != 0u;
	bPass = bPass && tPart.tFileName.iLen == 5u && strncmp(tPart.tFileName.sPtr, "a.txt", 5u) == 0;
	bPass = bPass && tPart.tBody.iLen == 10u && strncmp(tPart.tBody.sPtr, "hello file", 10u) == 0;
	bPass = bPass && xrtMultipartDecodeFileNameTo(&tPart, sDecoded, sizeof(sDecoded), &iDecodedLen);
	bPass = bPass && iDecodedLen == 5u && memcmp(sDecoded, "a.txt", 5u) == 0;
	memset(&tPart, 0, sizeof(tPart));
	bPass = bPass && !xrtMultipartNextN(sBuilt, iBuiltLen, "boundaryZ", 9u, &iOffset, &tPart);
	iBuiltLen = 0u;
	bPass = bPass && xrtMultipartAppendFilePartExt(sBuilt, sizeof(sBuilt), &iBuiltLen, "boundaryUTF8", "file", "fallback.txt", sUtf8Name, "text/plain", "payload", 7u);
	bPass = bPass && xrtMultipartAppendFinish(sBuilt, sizeof(sBuilt), &iBuiltLen, "boundaryUTF8");
	bPass = bPass && strstr(sBuilt, "filename=\"fallback.txt\"") != NULL;
	bPass = bPass && strstr(sBuilt, "filename*=UTF-8''%E4%B8%AD%E6%96%87.txt") != NULL;
	iOffset = 0u;
	memset(&tPart, 0, sizeof(tPart));
	bPass = bPass && xrtMultipartNextN(sBuilt, iBuiltLen, "boundaryUTF8", 12u, &iOffset, &tPart);
	bPass = bPass && (tPart.iFlags & XRT_MULTIPART_F_HAS_FILENAME) != 0u;
	bPass = bPass && (tPart.iFlags & XRT_MULTIPART_F_HAS_FILENAME_EXT) != 0u;
	bPass = bPass && tPart.tFileName.iLen == 12u && strncmp(tPart.tFileName.sPtr, "fallback.txt", 12u) == 0;
	bPass = bPass && xrtMultipartDecodeFileNameTo(&tPart, sDecoded, sizeof(sDecoded), &iDecodedLen);
	bPass = bPass && iDecodedLen == sizeof(sUtf8Name) - 1u && memcmp(sDecoded, sUtf8Name, sizeof(sUtf8Name) - 1u) == 0;
	memset(&tPart, 0, sizeof(tPart));
	bPass = bPass && !xrtMultipartNextN(sBuilt, iBuiltLen, "boundaryUTF8", 12u, &iOffset, &tPart);
	{
		xrtheaderpair arrPartHeaders[2];
		arrPartHeaders[0].tName = xrtStrView("Content-Disposition", 19u);
		arrPartHeaders[0].tValue = xrtStrView("form-data; name=\"meta\"", 22u);
		arrPartHeaders[1].tName = xrtStrView("Content-Type", 12u);
		arrPartHeaders[1].tValue = xrtStrView("application/json", 16u);
		iBuiltLen = 0u;
		bPass = bPass && xrtMultipartAppendRawPart(sBuilt, sizeof(sBuilt), &iBuiltLen, "boundaryRAW", arrPartHeaders, 2u, "{\"ok\":1}", 8u);
		bPass = bPass && xrtMultipartAppendFinish(sBuilt, sizeof(sBuilt), &iBuiltLen, "boundaryRAW");
		iOffset = 0u;
		memset(&tPart, 0, sizeof(tPart));
		bPass = bPass && xrtMultipartNextN(sBuilt, iBuiltLen, "boundaryRAW", 11u, &iOffset, &tPart);
		bPass = bPass && (tPart.iFlags & XRT_MULTIPART_F_HAS_NAME) != 0u;
		bPass = bPass && (tPart.iFlags & XRT_MULTIPART_F_HAS_CONTENT_TYPE) != 0u;
		bPass = bPass && tPart.tName.iLen == 4u && strncmp(tPart.tName.sPtr, "meta", 4u) == 0;
		bPass = bPass && tPart.tContentType.iLen == 16u && strncmp(tPart.tContentType.sPtr, "application/json", 16u) == 0;
		bPass = bPass && tPart.tBody.iLen == 8u && strncmp(tPart.tBody.sPtr, "{\"ok\":1}", 8u) == 0;
		memset(&tPart, 0, sizeof(tPart));
		bPass = bPass && !xrtMultipartNextN(sBuilt, iBuiltLen, "boundaryRAW", 11u, &iOffset, &tPart);
	}
	xrtMultipartStreamConfigInit(&tStreamCfg);
	tStreamCfg.iMaxBufferedBytes = 4096u;
	bPass = bPass && xrtMultipartStreamInit(&tStream, "streamB", 7u, &tStreamCfg);
	bPass = bPass && xrtMultipartStreamFeed(&tStream, sStreamMultipart, 37u);
	for (;;) {
		xrtmultipartstreamresult iRes = xrtMultipartStreamNext(&tStream, &tStreamEvent);
		if ( iRes == XRT_MULTIPART_STREAM_RESULT_NEED_MORE ) break;
		bPass = bPass && iRes != XRT_MULTIPART_STREAM_RESULT_ERROR;
	}
	bPass = bPass && xrtMultipartStreamFeed(&tStream, sStreamMultipart + 37u, 53u);
	for (;;) {
		xrtmultipartstreamresult iRes = xrtMultipartStreamNext(&tStream, &tStreamEvent);
		if ( iRes == XRT_MULTIPART_STREAM_RESULT_NEED_MORE ) break;
		bPass = bPass && iRes != XRT_MULTIPART_STREAM_RESULT_ERROR;
		if ( iRes == XRT_MULTIPART_STREAM_RESULT_PART_BEGIN ) {
			++iStreamBeginCount;
			++iStreamPartIndex;
			if ( iStreamPartIndex == 1 ) {
				bPass = bPass && (tStreamEvent.tPart.iFlags & XRT_MULTIPART_F_HAS_NAME) != 0u;
				bPass = bPass && tStreamEvent.tPart.tName.iLen == 6u && strncmp(tStreamEvent.tPart.tName.sPtr, "field1", 6u) == 0;
			}
		} else if ( iRes == XRT_MULTIPART_STREAM_RESULT_DATA ) {
			if ( iStreamPartIndex == 1 ) {
				bPass = bPass && iStreamBody1Len + tStreamEvent.tData.iLen < sizeof(sStreamBody1);
				memcpy(sStreamBody1 + iStreamBody1Len, tStreamEvent.tData.sPtr, tStreamEvent.tData.iLen);
				iStreamBody1Len += tStreamEvent.tData.iLen;
				sStreamBody1[iStreamBody1Len] = '\0';
			}
		} else if ( iRes == XRT_MULTIPART_STREAM_RESULT_PART_END ) {
			++iStreamEndCount;
		}
	}
	bPass = bPass && xrtMultipartStreamFeed(&tStream, sStreamMultipart + 90u, (sizeof(sStreamMultipart) - 1u) - 90u);
	xrtMultipartStreamFinish(&tStream);
	for (;;) {
		xrtmultipartstreamresult iRes = xrtMultipartStreamNext(&tStream, &tStreamEvent);
		bPass = bPass && iRes != XRT_MULTIPART_STREAM_RESULT_ERROR;
		if ( iRes == XRT_MULTIPART_STREAM_RESULT_NEED_MORE ) {
			bPass = false;
			break;
		}
		if ( iRes == XRT_MULTIPART_STREAM_RESULT_PART_BEGIN ) {
			++iStreamBeginCount;
			++iStreamPartIndex;
			if ( iStreamPartIndex == 2 ) {
				bPass = bPass && (tStreamEvent.tPart.iFlags & XRT_MULTIPART_F_HAS_FILENAME) != 0u;
				bPass = bPass && (tStreamEvent.tPart.iFlags & XRT_MULTIPART_F_HAS_CONTENT_TYPE) != 0u;
				bPass = bPass && tStreamEvent.tPart.tName.iLen == 4u && strncmp(tStreamEvent.tPart.tName.sPtr, "file", 4u) == 0;
			}
		} else if ( iRes == XRT_MULTIPART_STREAM_RESULT_DATA ) {
			if ( iStreamPartIndex == 1 ) {
				bPass = bPass && iStreamBody1Len + tStreamEvent.tData.iLen < sizeof(sStreamBody1);
				memcpy(sStreamBody1 + iStreamBody1Len, tStreamEvent.tData.sPtr, tStreamEvent.tData.iLen);
				iStreamBody1Len += tStreamEvent.tData.iLen;
				sStreamBody1[iStreamBody1Len] = '\0';
			} else if ( iStreamPartIndex == 2 ) {
				bPass = bPass && iStreamBody2Len + tStreamEvent.tData.iLen < sizeof(sStreamBody2);
				memcpy(sStreamBody2 + iStreamBody2Len, tStreamEvent.tData.sPtr, tStreamEvent.tData.iLen);
				iStreamBody2Len += tStreamEvent.tData.iLen;
				sStreamBody2[iStreamBody2Len] = '\0';
			}
		} else if ( iRes == XRT_MULTIPART_STREAM_RESULT_PART_END ) {
			++iStreamEndCount;
		} else if ( iRes == XRT_MULTIPART_STREAM_RESULT_END ) {
			bStreamDone = true;
			break;
		}
	}
	bPass = bPass && bStreamDone;
	bPass = bPass && iStreamBeginCount == 2;
	bPass = bPass && iStreamEndCount == 2;
	bPass = bPass && iStreamBody1Len == 6u && memcmp(sStreamBody1, "value1", 6u) == 0;
	bPass = bPass && iStreamBody2Len == 10u && memcmp(sStreamBody2, "hello file", 10u) == 0;
	xrtMultipartStreamUnit(&tStream);
	bPass = bPass && xrtMultipartStreamInit(&tStream, "streamB", 7u, NULL);
	bPass = bPass && xrtMultipartStreamFeed(&tStream, sStreamTruncated, sizeof(sStreamTruncated) - 1u);
	xrtMultipartStreamFinish(&tStream);
	for (;;) {
		xrtmultipartstreamresult iRes = xrtMultipartStreamNext(&tStream, &tStreamEvent);
		if ( iRes == XRT_MULTIPART_STREAM_RESULT_NEED_MORE ) {
			bPass = false;
			break;
		}
		if ( iRes == XRT_MULTIPART_STREAM_RESULT_ERROR ) break;
	}
	bPass = bPass && xrtMultipartStreamError(&tStream) == XRT_MULTIPART_STREAM_ERR_TRUNCATED;
	xrtMultipartStreamUnit(&tStream);

	bPass = bPass && xrtUrlParse("https://example.com/path?q=1", &tFixed);
	bPass = bPass && tFixed.bHttps;
	bPass = bPass && tFixed.iPort == 443u;
	bPass = bPass && strcmp(tFixed.sHost, "example.com") == 0;
	bPass = bPass && strcmp(tFixed.sPath, "/path?q=1") == 0;

	printf("  xurl/http util core : %s\n", bPass ? "PASS" : "FAIL");
	return bPass;
}

#endif
