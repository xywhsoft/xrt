#include "../lib/xcodec_http1.h"
#include "../lib/xcodec_ws.h"


void Test_XNet2_Codec(void)
{
	printf("\n\n\n------------------------------------\n\n XNet2 Codec Scaffolding Test:\n\n");

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
}
