#include "../test.h"



/* 把字符串视图与字面量按长度比较。 */
static bool testHttp1MessageTextEqual(xstrview Text, cstr sExpected)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		(memcmp(Text.Data, sExpected, iSize) == 0);
}



/* 验证定长请求的一次扫描、零拷贝视图、正文复制和流水线边界。 */
static void testHttp1MessageFixed(void)
{
	static const uint8 Wire[] =
		"POST /submit HTTP/1.1\r\n"
		"Host: example.test\r\nContent-Length: 5\r\n\r\n"
		"helloNEXT";
	xhttpfield Fields[4];
	xhttpfield Trailers[2];
	xhttp1errorinfo Error;
	xhttp1message Message;
	xbytesview Body;
	uint8 Output[8];
	size_t iFrame = sizeof(Wire) - 1u - 4u;
	size_t iSize;

	xrtHttp1MessageInit(&Message, Fields, 4, Trailers, 2);
	testRequire(xrtHttp1RequestMessageParse(
		(xbytesview){ Wire, sizeof(Wire) - 1u }, false,
		&Message, NULL, NULL, &Error
	) == XHTTP1_READY, "HTTP/1 fixed Message did not parse");
	testRequire((Message.Wire.Size == iFrame) &&
		(Message.BodyBytes == 5) &&
		(Message.Plan.Mode == XHTTP1_BODY_FIXED) &&
		testHttp1MessageTextEqual(Message.Head.Method, "POST") &&
		(memcmp(Wire + Message.Wire.Size, "NEXT", 4) == 0),
		"HTTP/1 fixed Message boundary mismatch");
	Body = xrtHttp1MessageBodyView(&Message);
	testRequire((Body.Size == 5) &&
		(memcmp(Body.Data, "hello", 5) == 0),
		"HTTP/1 fixed Message Body view mismatch");
	testRequire(xrtHttp1MessageBodyCopy(
		&Message, NULL, 0, &iSize
	) && (iSize == 5),
		"HTTP/1 fixed Message Body size query mismatch");
	memset(Output, 0xA5, sizeof(Output));
	testRequire(!xrtHttp1MessageBodyCopy(
		&Message, Output, 4, &iSize
	) && (iSize == 5) && (Output[0] == UINT8_C(0xA5)),
		"HTTP/1 fixed Message short output was not atomic");
	xrtClearError();
	testRequire(xrtHttp1MessageBodyCopy(
		&Message, Output, sizeof(Output), &iSize
	) && (iSize == 5) && (memcmp(Output, "hello", 5) == 0),
		"HTTP/1 fixed Message Body copy mismatch");
}



/* 验证 chunked 消息解码、动态 trailer、前缀不足与后缀保留。 */
static void testHttp1MessageChunked(void)
{
	static const uint8 Wire[] =
		"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
		"4\r\nWiki\r\n5\r\npedia\r\n"
		"0\r\nDigest: ok\r\nX-Meta: yes\r\n\r\nREST";
	xhttpfield Fields[4];
	xhttpfield Trailers[2];
	xhttp1errorinfo Error;
	xhttp1message Message;
	xbytesview Body;
	uint8 Output[16];
	size_t iFrame = sizeof(Wire) - 1u - 4u;
	size_t i;
	size_t iSize;

	for ( i = 0; i < iFrame; i++ ) {
		xrtHttp1MessageInit(&Message, Fields, 4, Trailers, 2);
		testRequire(xrtHttp1ResponseMessageParse(
			(xbytesview){ Wire, i }, false,
			XRT_STR_LITERAL("GET"),
			&Message, NULL, NULL, &Error
		) == XHTTP1_MORE,
			"HTTP/1 incomplete Message prefix was accepted");
	}

	xrtHttp1MessageInit(&Message, Fields, 4, Trailers, 2);
	testRequire(xrtHttp1ResponseMessageParse(
		(xbytesview){ Wire, sizeof(Wire) - 1u }, false,
		XRT_STR_LITERAL("GET"),
		&Message, NULL, NULL, &Error
	) == XHTTP1_READY, "HTTP/1 chunked Message did not parse");
	testRequire((Message.Wire.Size == iFrame) &&
		(Message.BodyBytes == 9) &&
		(Message.Plan.Mode == XHTTP1_BODY_CHUNKED) &&
		(Message.TrailerCount == 2) &&
		testHttp1MessageTextEqual(Trailers[0].Name, "Digest") &&
		testHttp1MessageTextEqual(Trailers[1].Value, "yes") &&
		(memcmp(Wire + Message.Wire.Size, "REST", 4) == 0),
		"HTTP/1 chunked Message metadata mismatch");
	Body = xrtHttp1MessageBodyView(&Message);
	testRequire((Body.Data == NULL) && (Body.Size == 0),
		"HTTP/1 chunked Message exposed a false contiguous view");
	testRequire(xrtHttp1MessageBodyCopy(
		&Message, Output, sizeof(Output), &iSize
	) && (iSize == 9) && (memcmp(Output, "Wikipedia", 9) == 0),
		"HTTP/1 chunked Message Body copy mismatch");
}



/* 验证 Header 与 trailer 描述符不足都能发布精确需求并原输入重试。 */
static void testHttp1MessageFields(void)
{
	static const uint8 Wire[] =
		"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n"
		"Connection: keep-alive\r\n\r\n"
		"1\r\na\r\n0\r\nA: 1\r\nB: 2\r\n\r\n";
	xhttpfield SmallFields[1];
	xhttpfield Fields[2];
	xhttpfield SmallTrailers[1];
	xhttpfield Trailers[2];
	xhttp1errorinfo Error;
	xhttp1message Message;

	xrtHttp1MessageInit(&Message, SmallFields, 1, Trailers, 2);
	testRequire(xrtHttp1ResponseMessageParse(
		(xbytesview){ Wire, sizeof(Wire) - 1u }, false,
		XRT_STR_LITERAL("GET"),
		&Message, NULL, NULL, &Error
	) == XHTTP1_FIELDS && (Message.Head.FieldCount == 2),
		"HTTP/1 Message Header descriptor requirement mismatch");

	xrtHttp1MessageInit(&Message, Fields, 2, SmallTrailers, 1);
	testRequire(xrtHttp1ResponseMessageParse(
		(xbytesview){ Wire, sizeof(Wire) - 1u }, false,
		XRT_STR_LITERAL("GET"),
		&Message, NULL, NULL, &Error
	) == XHTTP1_FIELDS &&
		(Message.Head.FieldCount == 2) &&
		(Message.TrailerCount == 2),
		"HTTP/1 Message trailer descriptor requirement mismatch");

	xrtHttp1MessageInit(&Message, Fields, 2, Trailers, 2);
	testRequire(xrtHttp1ResponseMessageParse(
		(xbytesview){ Wire, sizeof(Wire) - 1u }, false,
		XRT_STR_LITERAL("GET"),
		&Message, NULL, NULL, &Error
	) == XHTTP1_READY && (Message.TrailerCount == 2),
		"HTTP/1 Message descriptor rebind parse failed");
}



/* 验证关闭定界正文必须依赖可靠 EOF，随后可直接借用连续正文。 */
static void testHttp1MessageClose(void)
{
	static const uint8 Wire[] =
		"HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nclose-body";
	xhttpfield Fields[2];
	xhttp1errorinfo Error;
	xhttp1message Message;
	xbytesview Body;

	xrtHttp1MessageInit(&Message, Fields, 2, NULL, 0);
	testRequire(xrtHttp1ResponseMessageParse(
		(xbytesview){ Wire, sizeof(Wire) - 1u }, false,
		XRT_STR_LITERAL("GET"),
		&Message, NULL, NULL, &Error
	) == XHTTP1_MORE && (Message.Head.Bytes != 0),
		"HTTP/1 close Message completed without EOF");
	testRequire(xrtHttp1ResponseMessageParse(
		(xbytesview){ Wire, sizeof(Wire) - 1u }, true,
		XRT_STR_LITERAL("GET"),
		&Message, NULL, NULL, &Error
	) == XHTTP1_READY &&
		(Message.Plan.Mode == XHTTP1_BODY_CLOSE) &&
		(Message.BodyBytes == 10),
		"HTTP/1 close Message did not complete at EOF");
	Body = xrtHttp1MessageBodyView(&Message);
	testRequire((Body.Size == 10) &&
		(memcmp(Body.Data, "close-body", 10) == 0),
		"HTTP/1 close Message Body view mismatch");
}



/* 验证 HEAD、101 和成功 CONNECT 都只消费 HTTP Header 并交还后续字节。 */
static void testHttp1MessageHandoff(void)
{
	static const uint8 HeadWire[] =
		"HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\nNEXT";
	static const uint8 UpgradeWire[] =
		"HTTP/1.1 101 Switching Protocols\r\n"
		"Connection: Upgrade\r\nUpgrade: websocket\r\n\r\nWS";
	static const uint8 ConnectWire[] =
		"HTTP/1.1 204 No Content\r\n\r\nTLS";
	xhttpfield Fields[3];
	xhttp1errorinfo Error;
	xhttp1message Message;

	xrtHttp1MessageInit(&Message, Fields, 3, NULL, 0);
	testRequire(xrtHttp1ResponseMessageParse(
		(xbytesview){ HeadWire, sizeof(HeadWire) - 1u }, false,
		XRT_STR_LITERAL("HEAD"),
		&Message, NULL, NULL, &Error
	) == XHTTP1_READY &&
		(Message.Plan.Mode == XHTTP1_BODY_NONE) &&
		(memcmp(HeadWire + Message.Wire.Size, "NEXT", 4) == 0),
		"HTTP/1 HEAD Message consumed the next response bytes");

	testRequire(xrtHttp1ResponseMessageParse(
		(xbytesview){ UpgradeWire, sizeof(UpgradeWire) - 1u }, false,
		XRT_STR_LITERAL("GET"),
		&Message, NULL, NULL, &Error
	) == XHTTP1_READY &&
		(Message.Plan.Mode == XHTTP1_BODY_TUNNEL) &&
		(memcmp(UpgradeWire + Message.Wire.Size, "WS", 2) == 0),
		"HTTP/1 Upgrade Message consumed protocol bytes");

	testRequire(xrtHttp1ResponseMessageParse(
		(xbytesview){ ConnectWire, sizeof(ConnectWire) - 1u }, false,
		XRT_STR_LITERAL("CONNECT"),
		&Message, NULL, NULL, &Error
	) == XHTTP1_READY &&
		(Message.Plan.Mode == XHTTP1_BODY_TUNNEL) &&
		(memcmp(ConnectWire + Message.Wire.Size, "TLS", 3) == 0),
		"HTTP/1 CONNECT Message consumed tunnel bytes");
}



/* 验证完整消息层保留分帧错误并把正文偏移转换为消息绝对位置。 */
static void testHttp1MessageErrors(void)
{
	static const uint8 Ambiguous[] =
		"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n"
		"Content-Length: 1\r\n\r\n";
	static const uint8 Incomplete[] =
		"POST / HTTP/1.1\r\nContent-Length: 5\r\n\r\nhi";
	static const uint8 IncompleteHead[] =
		"GET / HTTP/1.1\r\nHost: example.test\r\n";
	xhttpfield Fields[3];
	xhttp1errorinfo Error;
	xhttp1message Message;
	size_t iHeadBytes = (size_t)(
		strstr((cstr)Incomplete, "\r\n\r\n") - (cstr)Incomplete
	) + 4u;

	xrtHttp1MessageInit(&Message, Fields, 3, NULL, 0);
	testRequire(xrtHttp1ResponseMessageParse(
		(xbytesview){ Ambiguous, sizeof(Ambiguous) - 1u }, false,
		XRT_STR_LITERAL("GET"),
		&Message, NULL, NULL, &Error
	) == XHTTP1_ERROR &&
		(Error.Code == XHTTP1_ERROR_TRANSFER_LENGTH) &&
		(Message.Head.Bytes == 0),
		"HTTP/1 Message accepted ambiguous response framing");
	xrtClearError();

	testRequire(xrtHttp1RequestMessageParse(
		(xbytesview){ Incomplete, sizeof(Incomplete) - 1u }, true,
		&Message, NULL, NULL, &Error
	) == XHTTP1_ERROR &&
		(Error.Code == XHTTP1_ERROR_BODY_INCOMPLETE) &&
		(Error.Offset == (iHeadBytes + 2u)) &&
		(Message.Head.Bytes == 0),
		"HTTP/1 Message incomplete Body error position mismatch");
	xrtClearError();

	testRequire(xrtHttp1RequestMessageParse(
		(xbytesview){ IncompleteHead, sizeof(IncompleteHead) - 1u }, true,
		&Message, NULL, NULL, &Error
	) == XHTTP1_ERROR &&
		(Error.Code == XHTTP1_ERROR_HEAD_INCOMPLETE) &&
		(Error.Offset == (sizeof(IncompleteHead) - 1u)) &&
		(Message.Head.Bytes == 0),
		"HTTP/1 Message incomplete Header was not rejected at EOF");
}



/* 运行完整消息便利层的核心契约测试。 */
int main(void)
{
	testHttp1MessageFixed();
	testHttp1MessageChunked();
	testHttp1MessageFields();
	testHttp1MessageClose();
	testHttp1MessageHandoff();
	testHttp1MessageErrors();
	printf("[PASS] http1_message\n");
	return 0;
}
