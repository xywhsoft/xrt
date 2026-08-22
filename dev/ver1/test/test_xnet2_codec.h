// XNET2编解码器测试
static bool __Test_XCodecHttpError(const char* sMessage, const xcodechttp1limits* pLimits, xcodechttp1error eExpected)
{
	xnetchain tChain;
	xcodecframe tFrame;
	xcodechttp1msg tMsg;
	xcodechttp1errorinfo tError;
	xcodecstatus eStatus;
	bool bPass;
	xrtNetChainInit(&tChain);
	if ( !xrtNetChainAppendCopy(&tChain, sMessage, strlen(sMessage)) ) { return false; }
	eStatus = xrtCodecHttp1ParseEx(&tChain, &tFrame, &tMsg, pLimits, &tError);
	bPass = eStatus == XCODEC_STATUS_ERROR && tError.eCode == eExpected && tMsg.tError.eCode == eExpected;
	xrtCodecHttp1MessageUnit(&tMsg);
	xrtNetChainClear(&tChain);
	return bPass;
}


typedef struct {
	uint8 aData[128];
	size_t iLen;
} __test_xinflate_output;


static bool __Test_XInflateOutput(ptr pUserData, const void* pData, size_t iLen)
{
	__test_xinflate_output* pOutput = (__test_xinflate_output*)pUserData;
	if ( !pOutput || (!pData && iLen > 0u) || iLen > sizeof(pOutput->aData) - pOutput->iLen ) { return false; }
	if ( iLen > 0u ) { memcpy(pOutput->aData + pOutput->iLen, pData, iLen); }
	pOutput->iLen += iLen;
	return true;
}


static bool __Test_XInflateFixture(__xinflate_format eFormat, const uint8* pCompressed,
	size_t iCompressedLen, uint64 iLimit, const char* sExpected)
{
	__xinflate tState;
	__test_xinflate_output tOutput;
	__xinflate_result eResult = __XINFLATE_OK;
	size_t iOffset = 0u;
	bool bPass;
	memset(&tOutput, 0, sizeof(tOutput));
	if ( !__xinflateInit(&tState, eFormat, iLimit) ) { return false; }
	while ( iOffset < iCompressedLen && eResult == __XINFLATE_OK ) {
		size_t iChunk = iCompressedLen - iOffset;
		if ( iChunk > 3u ) { iChunk = 3u; }
		eResult = __xinflateWrite(&tState, pCompressed + iOffset, iChunk,
			iOffset + iChunk == iCompressedLen, __Test_XInflateOutput, &tOutput);
		iOffset += iChunk;
	}
	bPass = eResult == __XINFLATE_DONE && tOutput.iLen == strlen(sExpected) &&
		memcmp(tOutput.aData, sExpected, tOutput.iLen) == 0;
	__xinflateUnit(&tState);
	return bPass;
}


void Test_XNet2_Codec(void)
{
	printf("\n\n\n------------------------------------\n\n XNet2 Codec Scaffolding Test:\n\n");

	{
		static const uint8 aZlib[] = {
			0x78,0x9c,0xcb,0x48,0xcd,0xc9,0xc9,0x57,0x48,0xce,0xcf,0x2d,0x28,0x4a,0x2d,
			0x2e,0x4e,0x4d,0x51,0x28,0xcf,0x2f,0xca,0x49,0x01,0x00,0x63,0x85,0x08,0xb2
		};
		static const uint8 aRaw[] = {
			0xcb,0x48,0xcd,0xc9,0xc9,0x57,0x48,0xce,0xcf,0x2d,0x28,0x4a,0x2d,0x2e,0x4e,
			0x4d,0x51,0x28,0xcf,0x2f,0xca,0x49,0x01,0x00
		};
		static const uint8 aGzip[] = {
			0x1f,0x8b,0x08,0x00,0x00,0x00,0x00,0x00,0x02,0xff,0xcb,0x48,0xcd,0xc9,0xc9,
			0x57,0x48,0xce,0xcf,0x2d,0x28,0x4a,0x2d,0x2e,0x4e,0x4d,0x51,0x28,0xcf,0x2f,
			0xca,0x49,0x01,0x00,0xa1,0x2d,0x94,0x53,0x16,0x00,0x00,0x00
		};
		static const uint8 aConcat[] = {
			0x1f,0x8b,0x08,0x00,0x00,0x00,0x00,0x00,0x02,0xff,0xcb,0x48,0xcd,0xc9,0xc9,
			0x57,0x00,0x00,0xf6,0xf9,0x81,0xed,0x06,0x00,0x00,0x00,0x1f,0x8b,0x08,0x00,
			0x00,0x00,0x00,0x00,0x02,0xff,0x4b,0xce,0xcf,0x2d,0x28,0x4a,0x2d,0x2e,0x4e,
			0x4d,0x51,0x28,0xcf,0x2f,0xca,0x49,0x01,0x00,0x73,0xb6,0xfe,0x4a,0x10,0x00,
			0x00,0x00
		};
		uint8 aBadGzip[sizeof(aGzip)];
		memcpy(aBadGzip, aGzip, sizeof(aGzip));
		aBadGzip[sizeof(aBadGzip) - 8u] ^= 1u;
		printf("  Inflate zlib stream : %s\n",
			__Test_XInflateFixture(__XINFLATE_FORMAT_ZLIB, aZlib, sizeof(aZlib), 128u,
				"hello compressed world") ? "PASS" : "FAIL");
		printf("  Inflate deflate zlib/raw compatibility : %s\n",
			__Test_XInflateFixture(__XINFLATE_FORMAT_DEFLATE_AUTO, aZlib, sizeof(aZlib), 128u,
				"hello compressed world") &&
			__Test_XInflateFixture(__XINFLATE_FORMAT_DEFLATE_AUTO, aRaw, sizeof(aRaw), 128u,
				"hello compressed world") ? "PASS" : "FAIL");
		printf("  Inflate gzip stream and trailer : %s\n",
			__Test_XInflateFixture(__XINFLATE_FORMAT_GZIP, aGzip, sizeof(aGzip), 128u,
				"hello compressed world") ? "PASS" : "FAIL");
		printf("  Inflate concatenated gzip members : %s\n",
			__Test_XInflateFixture(__XINFLATE_FORMAT_GZIP, aConcat, sizeof(aConcat), 128u,
				"hello compressed world") ? "PASS" : "FAIL");
		printf("  Inflate enforces output limit : %s\n",
			!__Test_XInflateFixture(__XINFLATE_FORMAT_GZIP, aGzip, sizeof(aGzip), 5u,
				"hello compressed world") ? "PASS" : "FAIL");
		printf("  Inflate rejects bad gzip checksum : %s\n",
			!__Test_XInflateFixture(__XINFLATE_FORMAT_GZIP, aBadGzip, sizeof(aBadGzip), 128u,
				"hello compressed world") ? "PASS" : "FAIL");
		printf("  Inflate rejects truncated gzip stream : %s\n",
			!__Test_XInflateFixture(__XINFLATE_FORMAT_GZIP, aGzip, sizeof(aGzip) - 1u, 128u,
				"hello compressed world") ? "PASS" : "FAIL");
	}

	{
		xnetchain tChain;
		xcodeclinecodec tLine;
		xcodecparser tParser;
		xcodecframe tFrame;
		char aLine[16] = {0};

		xrtNetChainInit(&tChain);
		xrtCodecLineConfigInit(&tLine);
		(void)xrtCodecLineSetDelimiter(&tLine, "\r\n", 2);
		xrtCodecParserInit(&tParser, xrtCodecLineOps(), &tLine);
		xrtNetChainAppendCopy(&tChain, "hello\r\nrest", 11);

		printf("  Line parser adapter frame : %s\n",
			xrtCodecParserParse(&tParser, &tChain, &tFrame) == XCODEC_STATUS_FRAME ? "PASS" : "FAIL");
		printf("  Line payload bytes : %s\n", tFrame.iPayloadBytes == 5 ? "PASS" : "FAIL");
		printf("  Line frame bytes : %s\n", tFrame.iFrameBytes == 7 ? "PASS" : "FAIL");
		printf("  Line payload peek : %s\n",
			xrtCodecFramePeek(&tChain, &tFrame, aLine, sizeof(aLine)) == 5 &&
			memcmp(aLine, "hello", 5) == 0 ? "PASS" : "FAIL");
		xrtCodecFrameConsume(&tChain, &tFrame);
		memset(aLine, 0, sizeof(aLine));
		printf("  Line consume leaves suffix : %s\n",
			xrtNetChainPeek(&tChain, aLine, sizeof(aLine)) == 4 &&
			memcmp(aLine, "rest", 4) == 0 ? "PASS" : "FAIL");
		xrtNetChainClear(&tChain);
	}

	{
		xnetchain tChain;
		xcodeclengthcodec tLen;
		xcodecparser tParser;
		xcodecframe tFrame;
		uint8 aPacket[] = { 0x00, 0x00, 0x00, 0x05, 'w', 'o', 'r', 'l', 'd' };
		char aPayload[16] = {0};

		xrtNetChainInit(&tChain);
		xrtCodecLengthConfigInit(&tLen);
		xrtCodecParserInit(&tParser, xrtCodecLengthOps(), &tLen);
		xrtNetChainAppendCopy(&tChain, aPacket, sizeof(aPacket));

		printf("  Length parser adapter frame : %s\n",
			xrtCodecParserParse(&tParser, &tChain, &tFrame) == XCODEC_STATUS_FRAME ? "PASS" : "FAIL");
		printf("  Length payload bytes : %s\n", tFrame.iPayloadBytes == 5 ? "PASS" : "FAIL");
		printf("  Length header bytes : %s\n", tFrame.iHeaderBytes == 4 ? "PASS" : "FAIL");
		printf("  Length payload peek : %s\n",
			xrtCodecFramePeek(&tChain, &tFrame, aPayload, sizeof(aPayload)) == 5 &&
			memcmp(aPayload, "world", 5) == 0 ? "PASS" : "FAIL");
		xrtNetChainClear(&tChain);
		xrtNetChainAppendCopy(&tChain, aPacket, 6);
		printf("  Length need more on partial body : %s\n",
			xrtCodecParserParse(&tParser, &tChain, &tFrame) == XCODEC_STATUS_NEED_MORE ? "PASS" : "FAIL");
		xrtNetChainClear(&tChain);
	}

	{
		xnetchain tChain;
		xcodecframe tFrame;
		xcodechttp1msg tMsg;
		char aBody[16] = {0};
		const char* sReq =
			"GET /chat HTTP/1.1\r\n"
			"Host: example.com\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Content-Length: 5\r\n"
			"\r\n"
			"hello";

		xrtNetChainInit(&tChain);
		xrtNetChainAppendCopy(&tChain, sReq, strlen(sReq));
		printf("  HTTP1 request frame : %s\n",
			xrtCodecHttp1Parse(&tChain, &tFrame, &tMsg) == XCODEC_STATUS_FRAME ? "PASS" : "FAIL");
		printf("  HTTP1 request method : %s\n", strcmp(tMsg.sMethod, "GET") == 0 ? "PASS" : "FAIL");
		printf("  HTTP1 request target : %s\n", strcmp(tMsg.sTarget, "/chat") == 0 ? "PASS" : "FAIL");
		printf("  HTTP1 request content-length : %s\n", tMsg.iContentLength == 5 ? "PASS" : "FAIL");
		printf("  HTTP1 request upgrade flag : %s\n",
			(tMsg.iFlags & XCODEC_HTTP1_F_UPGRADE) != 0 && (tFrame.iFlags & XCODEC_FRAME_F_UPGRADE) != 0 ? "PASS" : "FAIL");
		printf("  HTTP1 request keepalive flag : %s\n",
			(tMsg.iFlags & XCODEC_HTTP1_F_KEEPALIVE) != 0 ? "PASS" : "FAIL");
		printf("  HTTP1 request body peek : %s\n",
			xrtCodecFramePeek(&tChain, &tFrame, aBody, sizeof(aBody)) == 5 &&
			memcmp(aBody, "hello", 5) == 0 ? "PASS" : "FAIL");
		xrtCodecHttp1MessageUnit(&tMsg);
		xrtNetChainClear(&tChain);
		xrtNetChainAppendCopy(&tChain,
			"HTTP/1.1 101 Switching Protocols\r\n"
			"Connection: Upgrade\r\n"
			"Upgrade: websocket\r\n"
			"\r\n",
			strlen(
				"HTTP/1.1 101 Switching Protocols\r\n"
				"Connection: Upgrade\r\n"
				"Upgrade: websocket\r\n"
				"\r\n"));
		printf("  HTTP1 response skeleton frame : %s\n",
			xrtCodecHttp1Parse(&tChain, &tFrame, &tMsg) == XCODEC_STATUS_FRAME ? "PASS" : "FAIL");
		printf("  HTTP1 response status : %s\n", tMsg.iStatusCode == 101 ? "PASS" : "FAIL");
		printf("  HTTP1 response reason : %s\n", strcmp(tMsg.sReason, "Switching Protocols") == 0 ? "PASS" : "FAIL");
		xrtCodecHttp1MessageUnit(&tMsg);
		xrtNetChainClear(&tChain);
	}

	{
		xnetchain tChain;
		xcodecframe tFrame;
		xcodechttp1msg tMsg;
		char aBody[32] = {0};
		const char* sResp =
			"HTTP/1.1 200 OK\r\n"
			"Transfer-Encoding: chunked\r\n"
			"Connection: keep-alive\r\n"
			"\r\n"
			"4\r\nWiki\r\n"
			"5\r\npedia\r\n"
			"0\r\n\r\n"
			"rest";

		xrtNetChainInit(&tChain);
		xrtNetChainAppendCopy(&tChain, sResp, strlen(sResp));
		printf("  HTTP1 chunked response frame : %s\n",
			xrtCodecHttp1Parse(&tChain, &tFrame, &tMsg) == XCODEC_STATUS_FRAME ? "PASS" : "FAIL");
		printf("  HTTP1 chunked flag : %s\n",
			(tMsg.iFlags & XCODEC_HTTP1_F_CHUNKED) != 0 && (tFrame.iFlags & XCODEC_FRAME_F_CHUNKED) != 0 ? "PASS" : "FAIL");
		printf("  HTTP1 chunked decoded bytes : %s\n", xrtCodecHttp1BodyBytes(&tFrame) == 9 ? "PASS" : "FAIL");
		printf("  HTTP1 chunked body copy : %s\n",
			xrtCodecHttp1CopyBody(&tChain, &tFrame, aBody, sizeof(aBody)) == 9 &&
			memcmp(aBody, "Wikipedia", 9) == 0 ? "PASS" : "FAIL");
		xrtCodecHttp1MessageUnit(&tMsg);
		xrtCodecFrameConsume(&tChain, &tFrame);
		memset(aBody, 0, sizeof(aBody));
		printf("  HTTP1 chunked consume leaves suffix : %s\n",
			xrtNetChainPeek(&tChain, aBody, sizeof(aBody)) == 4 &&
			memcmp(aBody, "rest", 4) == 0 ? "PASS" : "FAIL");
		xrtNetChainClear(&tChain);
	}

	{
		xnetchain tChain;
		xcodecframe tFrame;
		xcodechttp1msg tMsg;
		char sLongName[321];
		char sLongValue[1025];
		char sResp[2048];
		xcodecstatus iStatus;
		const char* sCorsValue;
		const char* sLongResult;
		memset(sLongName, 'N', sizeof(sLongName) - 1u);
		memcpy(sLongName, "X-Long-", 7u);
		sLongName[sizeof(sLongName) - 1u] = '\0';
		memset(sLongValue, 'V', sizeof(sLongValue) - 1u);
		sLongValue[sizeof(sLongValue) - 1u] = '\0';
		snprintf(sResp, sizeof(sResp),
			"HTTP/1.1 200 OK\r\n"
			"Access-Control-Allow-Credentials: \ttrue \t\r\n"
			"%s: %s\r\n"
			"Content-Length: 0\r\n"
			"\r\n",
			sLongName,
			sLongValue);
		xrtNetChainInit(&tChain);
		xrtNetChainAppendCopy(&tChain, sResp, strlen(sResp));
		iStatus = xrtCodecHttp1ParseHead(&tChain, &tFrame, &tMsg);
		sCorsValue = iStatus == XCODEC_STATUS_FRAME ? xrtCodecHttp1GetHeader(&tMsg, "Access-Control-Allow-Credentials") : NULL;
		sLongResult = iStatus == XCODEC_STATUS_FRAME ? xrtCodecHttp1GetHeader(&tMsg, sLongName) : NULL;
		printf("  HTTP1 DeepSeek CORS header parse and OWS trim : %s\n",
			iStatus == XCODEC_STATUS_FRAME &&
			sCorsValue && strcmp(sCorsValue, "true") == 0 ? "PASS" : "FAIL");
		printf("  HTTP1 unbounded header name/value : %s\n",
			iStatus == XCODEC_STATUS_FRAME &&
			sLongResult && strcmp(sLongResult, sLongValue) == 0 ? "PASS" : "FAIL");
		xrtCodecHttp1MessageUnit(&tMsg);
		xrtNetChainClear(&tChain);
	}

	{
		xnetchain tChain;
		xcodecframe tFrame;
		xcodecwsframeinfo tInfo;
		uint8 aWsMasked[] = { 0x81, 0x82, 0x01, 0x02, 0x03, 0x04, 0x69, 0x6B };
		uint8 aPayload[8] = {0};
		uint8 aBig[130];
		uint8 aBigFrame[134];

		xrtNetChainInit(&tChain);
		xrtNetChainAppendCopy(&tChain, aWsMasked, sizeof(aWsMasked));
		printf("  WS masked frame parse : %s\n",
			xrtCodecWsParseFrame(&tChain, &tFrame, &tInfo) == XCODEC_STATUS_FRAME ? "PASS" : "FAIL");
		printf("  WS opcode text : %s\n", tInfo.iOpcode == XCODEC_WS_OPCODE_TEXT ? "PASS" : "FAIL");
		printf("  WS masked flag : %s\n",
			(tInfo.iFlags & XCODEC_WS_F_MASKED) != 0 && (tFrame.iFlags & XCODEC_FRAME_F_MASKED) != 0 ? "PASS" : "FAIL");
		printf("  WS payload peek masked bytes : %s\n",
			xrtCodecFramePeek(&tChain, &tFrame, aPayload, sizeof(aPayload)) == 2 ? "PASS" : "FAIL");
		xrtCodecWsUnmask(aPayload, 2, tInfo.aMask, 0);
		printf("  WS unmask payload : %s\n", memcmp(aPayload, "hi", 2) == 0 ? "PASS" : "FAIL");
		xrtNetChainClear(&tChain);

		memset(aBig, 'A', sizeof(aBig));
		aBigFrame[0] = 0x82;
		aBigFrame[1] = 126;
		aBigFrame[2] = 0x00;
		aBigFrame[3] = 130;
		memcpy(aBigFrame + 4, aBig, sizeof(aBig));
		xrtNetChainAppendCopy(&tChain, aBigFrame, sizeof(aBigFrame));
		printf("  WS extended length frame : %s\n",
			xrtCodecWsParseFrame(&tChain, &tFrame, &tInfo) == XCODEC_STATUS_FRAME ? "PASS" : "FAIL");
		printf("  WS extended payload length : %s\n", tInfo.iPayloadLen == 130 ? "PASS" : "FAIL");
		printf("  WS binary flag : %s\n", (tFrame.iFlags & XCODEC_FRAME_F_BINARY) != 0 ? "PASS" : "FAIL");
		xrtNetChainClear(&tChain);
	}

	{
		const char* sTeCl = "POST / HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\nContent-Length: 0\r\n\r\n0\r\n\r\n";
		const char* sConflictCl = "POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 4\r\nContent-Length: 5\r\n\r\nhello";
		const char* sObsFold = "GET / HTTP/1.1\r\nHost: x\r\n folded\r\n\r\n";
		const char* sBadName = "GET / HTTP/1.1\r\nBad Name: x\r\n\r\n";
		const char sBadValue[] = "GET / HTTP/1.1\r\nHost: x\r\nX-Test: a\x01" "b\r\n\r\n";
		const char* sBadVersion = "GET / HTTP/2.0\r\nHost: x\r\n\r\n";
		printf("  HTTP1 rejects Transfer-Encoding plus Content-Length : %s\n",
			__Test_XCodecHttpError(sTeCl, NULL, XCODEC_HTTP1_ERROR_TRANSFER_ENCODING_CONTENT_LENGTH) ? "PASS" : "FAIL");
		printf("  HTTP1 rejects conflicting Content-Length : %s\n",
			__Test_XCodecHttpError(sConflictCl, NULL, XCODEC_HTTP1_ERROR_CONFLICTING_CONTENT_LENGTH) ? "PASS" : "FAIL");
		printf("  HTTP1 rejects obs-fold : %s\n",
			__Test_XCodecHttpError(sObsFold, NULL, XCODEC_HTTP1_ERROR_INVALID_HEADER_NAME) ? "PASS" : "FAIL");
		printf("  HTTP1 rejects invalid header name : %s\n",
			__Test_XCodecHttpError(sBadName, NULL, XCODEC_HTTP1_ERROR_INVALID_HEADER_NAME) ? "PASS" : "FAIL");
		printf("  HTTP1 rejects control byte in header value : %s\n",
			__Test_XCodecHttpError(sBadValue, NULL, XCODEC_HTTP1_ERROR_INVALID_HEADER_VALUE) ? "PASS" : "FAIL");
		printf("  HTTP1 rejects unsupported version : %s\n",
			__Test_XCodecHttpError(sBadVersion, NULL, XCODEC_HTTP1_ERROR_INVALID_VERSION) ? "PASS" : "FAIL");
	}

	{
		xnetchain tChain;
		xcodecframe tFrame;
		xcodechttp1msg tMsg;
		xcodechttp1errorinfo tError;
		const char* sRepeatedCl = "POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 5, 5\r\nContent-Length: 5\r\n\r\nhello";
		xrtNetChainInit(&tChain);
		xrtNetChainAppendCopy(&tChain, sRepeatedCl, strlen(sRepeatedCl));
		printf("  HTTP1 accepts identical repeated Content-Length : %s\n",
			xrtCodecHttp1ParseEx(&tChain, &tFrame, &tMsg, NULL, &tError) == XCODEC_STATUS_FRAME && tMsg.iContentLength == 5 ? "PASS" : "FAIL");
		xrtCodecHttp1MessageUnit(&tMsg);
		xrtNetChainClear(&tChain);
	}

	{
		xcodechttp1limits tLimits;
		xrtCodecHttp1LimitsInit(&tLimits);
		tLimits.iMaxStartLineBytes = 8u;
		printf("  HTTP1 start-line limit : %s\n",
			__Test_XCodecHttpError("GET /long HTTP/1.1\r\nHost: x\r\n\r\n", &tLimits, XCODEC_HTTP1_ERROR_START_LINE_TOO_LARGE) ? "PASS" : "FAIL");
		xrtCodecHttp1LimitsInit(&tLimits);
		tLimits.iMaxHeaderLineBytes = 5u;
		printf("  HTTP1 header-line limit : %s\n",
			__Test_XCodecHttpError("GET / HTTP/1.1\r\nHeader: value\r\n\r\n", &tLimits, XCODEC_HTTP1_ERROR_HEADER_LINE_TOO_LARGE) ? "PASS" : "FAIL");
		xrtCodecHttp1LimitsInit(&tLimits);
		tLimits.iMaxHeaderCount = 1u;
		printf("  HTTP1 header-count limit : %s\n",
			__Test_XCodecHttpError("GET / HTTP/1.1\r\nA: 1\r\nB: 2\r\n\r\n", &tLimits, XCODEC_HTTP1_ERROR_TOO_MANY_HEADERS) ? "PASS" : "FAIL");
		xrtCodecHttp1LimitsInit(&tLimits);
		tLimits.iMaxBodyBytes = 4u;
		printf("  HTTP1 fixed body limit : %s\n",
			__Test_XCodecHttpError("POST / HTTP/1.1\r\nContent-Length: 5\r\n\r\nhello", &tLimits, XCODEC_HTTP1_ERROR_BODY_TOO_LARGE) ? "PASS" : "FAIL");
		xrtCodecHttp1LimitsInit(&tLimits);
		tLimits.iMaxChunkLineBytes = 3u;
		printf("  HTTP1 chunk-line limit : %s\n",
			__Test_XCodecHttpError("POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n4;foo=bar\r\ntest\r\n0\r\n\r\n", &tLimits, XCODEC_HTTP1_ERROR_CHUNK_LINE_TOO_LARGE) ? "PASS" : "FAIL");
	}

	{
		const char* sBadExtension = "POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n4;=bad\r\ntest\r\n0\r\n\r\n";
		const char* sBadTerminator = "POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n4\r\ntestXX0\r\n\r\n";
		const char* sForbiddenTrailer = "POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n0\r\nContent-Length: 0\r\n\r\n";
		printf("  HTTP1 rejects malformed chunk extension : %s\n",
			__Test_XCodecHttpError(sBadExtension, NULL, XCODEC_HTTP1_ERROR_INVALID_CHUNK_SIZE) ? "PASS" : "FAIL");
		printf("  HTTP1 rejects invalid chunk terminator : %s\n",
			__Test_XCodecHttpError(sBadTerminator, NULL, XCODEC_HTTP1_ERROR_INVALID_CHUNK_TERMINATOR) ? "PASS" : "FAIL");
		printf("  HTTP1 rejects forbidden trailer : %s\n",
			__Test_XCodecHttpError(sForbiddenTrailer, NULL, XCODEC_HTTP1_ERROR_FORBIDDEN_TRAILER) ? "PASS" : "FAIL");
	}

	{
		xnetchain tChain;
		xcodecframe tFrame;
		xcodechttp1msg tMsg;
		xcodechttp1errorinfo tError;
		const char* sTrailers = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n4\r\ntest\r\n0\r\nX-Checksum: ok\r\nX-Meta: yes\r\n\r\n";
		xrtNetChainInit(&tChain);
		xrtNetChainAppendCopy(&tChain, sTrailers, strlen(sTrailers));
		printf("  HTTP1 parses dynamic trailers : %s\n",
			xrtCodecHttp1ParseEx(&tChain, &tFrame, &tMsg, NULL, &tError) == XCODEC_STATUS_FRAME &&
			tMsg.iTrailerCount == 2u && strcmp(xrtCodecHttp1GetTrailer(&tMsg, "X-Checksum"), "ok") == 0 ? "PASS" : "FAIL");
		xrtCodecHttp1MessageUnit(&tMsg);
		xrtNetChainClear(&tChain);
	}

	{
		static const uint8 aRsv[] = { 0xC1u, 0x00u };
		static const uint8 aReservedOpcode[] = { 0x83u, 0x00u };
		static const uint8 aFragmentedPing[] = { 0x09u, 0x00u };
		static const uint8 aNonMinimal16[] = { 0x82u, 126u, 0x00u, 125u };
		static const uint8 aNonMinimal64[] = { 0x82u, 127u, 0u, 0u, 0u, 0u, 0u, 0u, 0xFFu, 0xFFu };
		static const uint8 aMsb64[] = { 0x82u, 127u, 0x80u, 0u, 0u, 0u, 0u, 1u, 0u, 0u };
		static const uint8 aOverLimit[] = { 0x82u, 0x02u, 'o', 'k' };
		const struct { const uint8* pData; size_t iLen; uint64 iLimit; } aCases[] = {
			{ aRsv, sizeof(aRsv), UINT64_MAX }, { aReservedOpcode, sizeof(aReservedOpcode), UINT64_MAX },
			{ aFragmentedPing, sizeof(aFragmentedPing), UINT64_MAX }, { aNonMinimal16, sizeof(aNonMinimal16), UINT64_MAX },
			{ aNonMinimal64, sizeof(aNonMinimal64), UINT64_MAX }, { aMsb64, sizeof(aMsb64), UINT64_MAX },
			{ aOverLimit, sizeof(aOverLimit), 1u }
		};
		bool bPass = true;
		for ( size_t i = 0u; i < sizeof(aCases) / sizeof(aCases[0]); ++i ) {
			xnetchain tChain;
			xcodecframe tFrame;
			xcodecwsframeinfo tInfo;
			xrtNetChainInit(&tChain);
			xrtNetChainAppendCopy(&tChain, aCases[i].pData, aCases[i].iLen);
			bPass = bPass && xrtCodecWsParseFrameEx(&tChain, &tFrame, &tInfo, aCases[i].iLimit) == XCODEC_STATUS_ERROR;
			xrtNetChainClear(&tChain);
		}
		printf("  WS rejects RSV/opcode/control/length violations : %s\n", bPass ? "PASS" : "FAIL");
		{
			xnetchain tChain;
			xcodecframe tFrame;
			xcodecwsframeinfo tInfo;
			xrtNetChainInit(&tChain);
			xrtNetChainAppendCopy(&tChain, aRsv, sizeof(aRsv));
			printf("  WS codec explicitly allows negotiated RSV1 : %s\n",
				xrtCodecWsParseFrameEx2(&tChain, &tFrame, &tInfo, UINT64_MAX, XCODEC_WS_RSV1) == XCODEC_STATUS_FRAME &&
				(tInfo.iFlags & XCODEC_WS_F_RSV1) != 0u ? "PASS" : "FAIL");
			xrtNetChainClear(&tChain);
		}
		printf("  WS validates keys/protocols/UTF-8/close payload : %s\n",
			__xwsValidClientKey("dGhlIHNhbXBsZSBub25jZQ==") &&
			!__xwsValidClientKey("dGhlIHNhbXBsZSBub25jZQ=A") &&
			__xwsValidProtocolList("chat, superchat") && !__xwsValidProtocolList("chat,,bad") &&
			__xwsProtocolListContains("chat, superchat", "superchat") &&
			__xwsValidUtf8("\xE4\xB8\xAD", 3u) && !__xwsValidUtf8("\xC0\x80", 2u) &&
			__xwsValidateClosePayload("\x03\xE8ok", 4u, NULL) == 0u &&
			__xwsValidateClosePayload("x", 1u, NULL) == XWS_CLOSE_PROTOCOL ? "PASS" : "FAIL");
	}
}
