#include "../test.h"



/* 从明确长度构造解析器需要的字节视图。 */
static xbytesview testHttp1Bytes(const void* pData, size_t iSize)
{
	xbytesview View;

	View.Data = (cbytes)pData;
	View.Size = iSize;
	return View;
}



/* 返回完整 Header 的线路字节数。 */
static size_t testHttp1HeadBytes(cstr sMessage)
{
	cstr sEnd = strstr(sMessage, "\r\n\r\n");

	testRequire(sEnd != NULL, "HTTP/1 test fixture has no Header terminator");
	return (size_t)(sEnd - sMessage) + 4u;
}



/* 验证公开 request-target 校验器覆盖 HTTP/1 常用形式和非法字节。 */
static void testHttp1Target(void)
{
	static const char ControlTarget[] = { '/', 'a', '\x1F', 'b' };

	testRequire(xrtHttp1TargetValid(XRT_STR_LITERAL("/path?q=1")),
		"HTTP/1 origin-form target was rejected");
	testRequire(xrtHttp1TargetValid(XRT_STR_LITERAL("*")),
		"HTTP/1 asterisk-form target was rejected");
	testRequire(xrtHttp1TargetValid(
		XRT_STR_LITERAL("https://example.test/path?q=1")
	), "HTTP/1 absolute-form target was rejected");
	testRequire(!xrtHttp1TargetValid((xstrview){ NULL, 0 }) &&
		!xrtHttp1TargetValid(XRT_STR_LITERAL("/bad target")) &&
		!xrtHttp1TargetValid(XRT_STR_LITERAL("/path#fragment")) &&
		!xrtHttp1TargetValid((xstrview){ ControlTarget, sizeof(ControlTarget) }),
		"HTTP/1 invalid request-target was accepted");
}



/* 验证请求解析、OWS、连接语义和借用字段视图。 */
static void testHttp1Request(void)
{
	static const char Message[] =
		"POST /submit?q=1 HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Content-Length: 5, 5\r\n"
		"Connection: keep-alive, Upgrade\r\n"
		"Connection:\r\n"
		"Upgrade: websocket\r\n"
		"X-Trim:\t value \t\r\n"
		"\r\nhelloNEXT";
	xhttpfield Fields[8];
	xhttp1errorinfo Error;
	xhttp1head Head;
	const xhttpfield* pField;
	size_t iHeadBytes = testHttp1HeadBytes(Message);

	xrtHttp1HeadInit(&Head, Fields, 8);
	testRequire(xrtHttp1RequestParse(
		testHttp1Bytes(Message, sizeof(Message) - 1u),
		&Head, NULL, &Error
	) == XHTTP1_READY, "HTTP/1 request parse failed");
	testRequire((Head.Kind == XHTTP_REQUEST) &&
		(Head.Version == XHTTP_VERSION_1_1) &&
		(Head.Bytes == iHeadBytes) &&
		(Head.FieldCount == 6) &&
		(Head.ContentLength == 5) &&
		((Head.Flags & (uint32)XHTTP1_CONTENT_LENGTH) != 0) &&
		((Head.Flags & (uint32)XHTTP1_KEEP_ALIVE) != 0) &&
		((Head.Flags & (uint32)XHTTP1_UPGRADE) != 0),
		"HTTP/1 request metadata mismatch");
	testRequire((Head.Method.Size == 4) &&
		(memcmp(Head.Method.Data, "POST", 4) == 0) &&
		(Head.Target.Size == 11) &&
		(memcmp(Head.Target.Data, "/submit?q=1", 11) == 0),
		"HTTP/1 request line views mismatch");
	pField = xrtHttp1Field(&Head, XRT_STR_LITERAL("x-trim"));
	testRequire((pField != NULL) && (pField->Value.Size == 5) &&
		(memcmp(pField->Value.Data, "value", 5) == 0),
		"HTTP/1 field OWS was not trimmed");
	testRequire(memcmp(Message + Head.Bytes, "helloNEXT", 9) == 0,
		"HTTP/1 parser consumed or changed body bytes");
}



/* 验证响应状态、空 Reason 和 HTTP/1.0 keep-alive 规则。 */
static void testHttp1Response(void)
{
	static const char Response[] =
		"HTTP/1.0 204 \r\nConnection: keep-alive\r\n\r\n";
	xhttpfield Fields[2];
	xhttp1head Head;

	xrtHttp1HeadInit(&Head, Fields, 2);
	testRequire(xrtHttp1ResponseParse(
		testHttp1Bytes(Response, sizeof(Response) - 1u),
		&Head, NULL, NULL
	) == XHTTP1_READY, "HTTP/1 response parse failed");
	testRequire((Head.Kind == XHTTP_RESPONSE) &&
		(Head.Version == XHTTP_VERSION_1_0) &&
		(Head.Status == 204) && (Head.Reason.Size == 0) &&
		((Head.Flags & (uint32)XHTTP1_KEEP_ALIVE) != 0),
		"HTTP/1 response metadata mismatch");
}



/* 验证每一个网络分片点都返回 MORE，完整 Header 后立即发布结果。 */
static void testHttp1Incremental(void)
{
	static const char Message[] =
		"GET / HTTP/1.1\r\nHost: example.test\r\n\r\nbody";
	xhttpfield Fields[2];
	xhttp1head Head;
	size_t iHeadBytes = testHttp1HeadBytes(Message);
	size_t i;

	for ( i = 0; i < iHeadBytes; i++ ) {
		xrtHttp1HeadInit(&Head, Fields, 2);
		testRequire(xrtHttp1RequestParse(
			testHttp1Bytes(Message, i), &Head, NULL, NULL
		) == XHTTP1_MORE, "HTTP/1 prefix did not request more data");
		testRequire((Head.Fields == Fields) &&
			(Head.FieldCapacity == 2) && (Head.FieldCount == 0),
			"HTTP/1 MORE state published partial metadata");
	}
	xrtHttp1HeadInit(&Head, Fields, 2);
	testRequire(xrtHttp1RequestParse(
		testHttp1Bytes(Message, iHeadBytes), &Head, NULL, NULL
	) == XHTTP1_READY, "HTTP/1 complete prefix did not parse");
}



/* 字段描述符不足必须报告精确需求，并允许用新数组无损重试。 */
static void testHttp1FieldCapacity(void)
{
	static const char Message[] =
		"GET / HTTP/1.1\r\nA: 1\r\nB: 2\r\nC: 3\r\n\r\n";
	xhttpfield Small[1];
	xhttpfield Fields[3];
	xhttp1head Head;

	memset(Small, 0xA5, sizeof(Small));
	xrtHttp1HeadInit(&Head, Small, 1);
	testRequire(xrtHttp1RequestParse(
		testHttp1Bytes(Message, sizeof(Message) - 1u),
		&Head, NULL, NULL
	) == XHTTP1_FIELDS, "HTTP/1 small field array was not reported");
	testRequire((Head.FieldCount == 3) &&
		(Head.FieldCapacity == 1) &&
		((const unsigned char*)Small)[0] == UINT8_C(0xA5),
		"HTTP/1 field shortage was not failure atomic");
	xrtClearError();
	testRequire((xrtHttp1Field(
		&Head, XRT_STR_LITERAL("A")
	) == NULL) && (xrtErrorKind(xrtGetError()) == XERR_STATE),
		"HTTP/1 field shortage exposed an unpublished descriptor");
	xrtHttp1HeadInit(&Head, Fields, 3);
	testRequire(xrtHttp1RequestParse(
		testHttp1Bytes(Message, sizeof(Message) - 1u),
		&Head, NULL, NULL
	) == XHTTP1_READY, "HTTP/1 field capacity retry failed");
	testRequire((Fields[2].Name.Size == 1) &&
		(Fields[2].Name.Data[0] == 'C'),
		"HTTP/1 retried fields mismatch");
}



/* 验证字段长度只受显式限额约束，不再存在旧固定数组截断。 */
static void testHttp1DynamicFieldLength(void)
{
	char* sMessage;
	xhttpfield Field;
	xhttp1limits Limits;
	xhttp1head Head;
	size_t iName = 300;
	size_t iValue = 12000;
	size_t iPrefix = sizeof("GET / HTTP/1.1\r\n") - 1u;
	size_t iSize = iPrefix + iName + 2u + iValue + 4u;

	sMessage = (char*)malloc(iSize);
	testRequire(sMessage != NULL, "HTTP/1 long field fixture allocation failed");
	memcpy(sMessage, "GET / HTTP/1.1\r\n", iPrefix);
	memset(sMessage + iPrefix, 'X', iName);
	memcpy(sMessage + iPrefix + iName, ": ", 2);
	memset(sMessage + iPrefix + iName + 2u, 'v', iValue);
	memcpy(sMessage + iSize - 4u, "\r\n\r\n", 4);
	xrtHttp1LimitsInit(&Limits);
	Limits.MaxHead = iSize;
	Limits.MaxFieldLine = iName + iValue + 2u;
	xrtHttp1HeadInit(&Head, &Field, 1);
	testRequire(xrtHttp1RequestParse(
		testHttp1Bytes(sMessage, iSize), &Head, &Limits, NULL
	) == XHTTP1_READY, "HTTP/1 dynamic long field parse failed");
	testRequire((Field.Name.Size == iName) &&
		(Field.Value.Size == iValue),
		"HTTP/1 dynamic long field was truncated");
	free(sMessage);
}



/* 验证重复且一致的 Content-Length 是规范化输入。 */
static void testHttp1RepeatedLength(void)
{
	static const char Message[] =
		"POST / HTTP/1.1\r\n"
		"Content-Length: 42, 42\r\n"
		"Content-Length: 42\r\n\r\n";
	xhttpfield Fields[2];
	xhttp1head Head;

	xrtHttp1HeadInit(&Head, Fields, 2);
	testRequire(xrtHttp1RequestParse(
		testHttp1Bytes(Message, sizeof(Message) - 1u),
		&Head, NULL, NULL
	) == XHTTP1_READY, "HTTP/1 identical Content-Length was rejected");
	testRequire((Head.ContentLength == 42) &&
		((Head.Flags & (uint32)XHTTP1_CONTENT_LENGTH) != 0),
		"HTTP/1 repeated Content-Length metadata mismatch");
}



/* 检查一个恶意 Header 被稳定错误码拒绝。 */
static void testHttp1Reject(
	cstr sMessage,
	bool bResponse,
	xhttp1error Code
)
{
	xhttpfield Fields[16];
	xhttp1errorinfo Error;
	xhttp1head Head;
	xhttp1status Status;

	xrtClearError();
	xrtHttp1HeadInit(&Head, Fields, 16);
	Status = bResponse ?
		xrtHttp1ResponseParse(
			testHttp1Bytes(sMessage, strlen(sMessage)),
			&Head, NULL, &Error
		) :
		xrtHttp1RequestParse(
			testHttp1Bytes(sMessage, strlen(sMessage)),
			&Head, NULL, &Error
		);
	if ( Status != XHTTP1_ERROR ) {
		fprintf(
			stderr,
			"[DETAIL] HTTP/1 status=%d expected-code=%d response=%d\n",
			(int)Status,
			(int)Code,
			bResponse ? 1 : 0
		);
	}
	testRequire(Status == XHTTP1_ERROR,
		"HTTP/1 malformed input status mismatch");
	testRequire(Error.Code == Code,
		"HTTP/1 malformed input protocol code mismatch");
	testRequire(xrtGetError() != NULL,
		"HTTP/1 malformed input did not publish an error");
	testRequire(xrtErrorCode(xrtGetError()) == (int32)Code,
		"HTTP/1 malformed input runtime code mismatch");
	testRequire(
		strcmp(xrtErrorDomain(xrtGetError()), "xrt.http1") == 0,
		"HTTP/1 malformed input runtime domain mismatch"
	);
}



/* 验证 Header 只发布线缆事实，把依赖方法和状态的分帧优先级留给 Body Plan。 */
static void testHttp1TransferFacts(void)
{
	static const char Conflict[] =
		"POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n"
		"Content-Length: 1\r\n\r\n";
	static const char Layered[] =
		"HTTP/1.1 200 OK\r\nTransfer-Encoding: gzip, chunked\r\n\r\n";
	static const char CloseDelimited[] =
		"HTTP/1.1 200 OK\r\nTransfer-Encoding: gzip\r\n\r\n";
	static const char ChunkedNotFinal[] =
		"HTTP/1.1 200 OK\r\n"
		"Transfer-Encoding: chunked, gzip\r\n\r\n";
	xhttpfield Fields[4];
	xhttp1errorinfo Error;
	xhttp1head Head;

	xrtHttp1HeadInit(&Head, Fields, 4);
	testRequire(xrtHttp1RequestParse(
		XRT_BYTES_LITERAL(Conflict), &Head, NULL, &Error
	) == XHTTP1_READY, "HTTP/1 head rejected framing facts too early");
	testRequire((Head.Flags & (uint32)XHTTP1_TRANSFER_ENCODING) != 0,
		"HTTP/1 head lost Transfer-Encoding presence");
	testRequire((Head.Flags & (uint32)XHTTP1_CONTENT_LENGTH) != 0,
		"HTTP/1 head lost Content-Length presence");

	xrtHttp1HeadInit(&Head, Fields, 4);
	testRequire(xrtHttp1ResponseParse(
		XRT_BYTES_LITERAL(Layered), &Head, NULL, &Error
	) == XHTTP1_READY, "HTTP/1 head rejected layered transfer coding");
	testRequire((Head.Flags & (uint32)XHTTP1_TRANSFER_ENCODING) != 0,
		"HTTP/1 layered coding lost transfer flag");
	testRequire((Head.Flags & (uint32)XHTTP1_CHUNKED) != 0,
		"HTTP/1 layered coding lost final chunked flag");
	testRequire((Head.Flags & (uint32)XHTTP1_TRANSFER_OTHER) != 0,
		"HTTP/1 layered coding lost non-chunked transfer fact");

	xrtHttp1HeadInit(&Head, Fields, 4);
	testRequire(xrtHttp1ResponseParse(
		XRT_BYTES_LITERAL(CloseDelimited), &Head, NULL, &Error
	) == XHTTP1_READY, "HTTP/1 head rejected close-delimited transfer coding");
	testRequire(((Head.Flags & (uint32)XHTTP1_TRANSFER_ENCODING) != 0) &&
		((Head.Flags & (uint32)XHTTP1_CHUNKED) == 0) &&
		((Head.Flags & (uint32)XHTTP1_TRANSFER_OTHER) != 0),
		"HTTP/1 close-delimited transfer facts mismatch");

	xrtHttp1HeadInit(&Head, Fields, 4);
	testRequire(xrtHttp1ResponseParse(
		XRT_BYTES_LITERAL(ChunkedNotFinal), &Head, NULL, &Error
	) == XHTTP1_READY &&
		((Head.Flags & (uint32)XHTTP1_TRANSFER_ENCODING) != 0) &&
		((Head.Flags & (uint32)XHTTP1_CHUNKED) == 0),
		"HTTP/1 response rejected non-final chunked framing facts");
}



/* 验证 Transfer-Encoding 逐项视图公开编码顺序与原样参数。 */
static void testHttp1TransferCodingIterator(void)
{
	static const xstrview Value = XRT_STR_INIT(
		"gzip; level=\"a,b\"; mode=fast, chunked"
	);
	xhttp1transfercoding Coding;
	size_t iOffset = 0;

	testRequire((xrtHttp1TransferCodingNext(
		Value, &iOffset, &Coding
	) == XHTTP_NEXT_ITEM) &&
		xrtHttpTokenEqual(Coding.Name, XRT_STR_LITERAL("gzip")) &&
		(Coding.Parameters.Data != NULL) &&
		(Coding.Parameters.Size != 0),
		"HTTP/1 first Transfer Coding mismatch");
	testRequire((xrtHttp1TransferCodingNext(
		Value, &iOffset, &Coding
	) == XHTTP_NEXT_ITEM) &&
		xrtHttpTokenEqual(Coding.Name, XRT_STR_LITERAL("chunked")) &&
		(Coding.Parameters.Data == NULL),
		"HTTP/1 final Transfer Coding mismatch");
	testRequire(xrtHttp1TransferCodingNext(
		Value, &iOffset, &Coding
	) == XHTTP_NEXT_END,
		"HTTP/1 Transfer Coding iterator did not end");

	iOffset = 0;
	testRequire((xrtHttp1TransferCodingNext(
		XRT_STR_LITERAL("gzip; bad"), &iOffset, &Coding
	) == XHTTP_NEXT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP/1 malformed Transfer Coding parameters were accepted");
	iOffset = 0;
	testRequire(xrtHttp1TransferCodingNext(
		XRT_STR_LITERAL("gzip, \t"), &iOffset, &Coding
	) == XHTTP_NEXT_ERROR,
		"HTTP/1 trailing empty Transfer Coding was accepted");
}



/* 验证字段注入、非法 transfer coding 和线路歧义全部被严格拒绝。 */
static void testHttp1RejectsMalformed(void)
{
	testHttp1Reject(
		"POST / HTTP/1.1\r\nContent-Length: 1\r\n"
		"Content-Length: 2\r\n\r\n",
		false, XHTTP1_ERROR_CONFLICTING_CONTENT_LENGTH
	);
	testHttp1Reject(
		"GET / HTTP/1.1\r\n Folded: no\r\n\r\n",
		false, XHTTP1_ERROR_FIELD_NAME
	);
	testHttp1Reject(
		"GET / HTTP/1.1\r\nBad Name: x\r\n\r\n",
		false, XHTTP1_ERROR_FIELD_NAME
	);
	testHttp1Reject(
		"GET / HTTP/1.1\r\nX-Test: ok\001bad\r\n\r\n",
		false, XHTTP1_ERROR_FIELD_VALUE
	);
	testHttp1Reject(
		"GET / HTTP/1.1\r\nConnection: close;bad\r\n\r\n",
		false, XHTTP1_ERROR_CONNECTION
	);
	testHttp1Reject(
		"GET / HTTP/1.1\r\nUpgrade: websocket/\r\n\r\n",
		false, XHTTP1_ERROR_UPGRADE
	);
	testHttp1Reject(
		"HTTP/1.1 101 Switching Protocols\r\n"
		"Connection: Upgrade\r\nUpgrade: bad protocol\r\n\r\n",
		true, XHTTP1_ERROR_UPGRADE
	);
	testHttp1Reject(
		"GET / HTTP/1.1\r\nTransfer-Encoding: chunked; x=1\r\n\r\n",
		false, XHTTP1_ERROR_TRANSFER_ENCODING
	);
	testHttp1Reject(
		"POST / HTTP/1.0\r\nTransfer-Encoding: chunked\r\n\r\n",
		false, XHTTP1_ERROR_UNSUPPORTED_TRANSFER_ENCODING
	);
	testHttp1Reject(
		"HTTP/1.0 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n",
		true, XHTTP1_ERROR_UNSUPPORTED_TRANSFER_ENCODING
	);
	testHttp1Reject(
		"GET / HTTP/1.1\nHost: x\n\n",
		false, XHTTP1_ERROR_LINE_END
	);
	testHttp1Reject(
		"GET /bad#fragment HTTP/1.1\r\n\r\n",
		false, XHTTP1_ERROR_TARGET
	);
	testHttp1Reject(
		"HTTP/2.0 200 OK\r\n\r\n",
		true, XHTTP1_ERROR_VERSION
	);
	testHttp1Reject(
		"HTTP/1.1 099 Bad\r\n\r\n",
		true, XHTTP1_ERROR_STATUS
	);
	testHttp1Reject(
		"HTTP/1.1 204\r\n\r\n",
		true, XHTTP1_ERROR_START_LINE
	);
	testHttp1Reject(
		"HTTP/1.1 200 bad\001reason\r\n\r\n",
		true, XHTTP1_ERROR_REASON
	);
}



/* 验证限额描述符支持未对齐存储，并在地址回绕时保持解析状态安全。 */
static void testHttp1LimitMemoryContracts(void)
{
	static const char Message[] = "GET / HTTP/1.1\r\n\r\n";
	uint8 Storage[sizeof(xhttp1limits) + 2u];
	xhttp1limits Limits;
	xhttp1head Head;

	memset(Storage, 0xA5, sizeof(Storage));
	xrtHttp1LimitsInit((xhttp1limits*)(void*)(Storage + 1u));
	memcpy(&Limits, Storage + 1u, sizeof(Limits));
	testRequire((Storage[0] == 0xA5) &&
		(Storage[sizeof(Storage) - 1u] == 0xA5) &&
		(Limits.MaxHead == 65536u) &&
		(Limits.MaxStartLine == 8192u) &&
		(Limits.MaxFieldLine == 8192u) &&
		(Limits.MaxFields == 100u),
		"HTTP/1 limits init did not support unaligned storage");
	xrtHttp1HeadInit(&Head, NULL, 0u);
	testRequire(xrtHttp1RequestParse(
		testHttp1Bytes(Message, sizeof(Message) - 1u),
		&Head,
		(const xhttp1limits*)(const void*)(Storage + 1u),
		NULL
	) == XHTTP1_READY,
		"HTTP/1 parser did not snapshot unaligned limits");

	xrtHttp1LimitsInit((xhttp1limits*)(uintptr_t)(UINTPTR_MAX - 1u));
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"HTTP/1 limits init accepted wrapping output");
	xrtClearError();
	xrtHttp1HeadInit(&Head, NULL, 0u);
	testRequire((xrtHttp1RequestParse(
		testHttp1Bytes(Message, sizeof(Message) - 1u),
		&Head,
		(const xhttp1limits*)(uintptr_t)(UINTPTR_MAX - 1u),
		NULL
	) == XHTTP1_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP/1 parser accepted wrapping limits storage");
	xrtClearError();
}



/* 验证四种独立限额都有自己的错误边界。 */
static void testHttp1Limits(void)
{
	static const char Message[] =
		"GET /long HTTP/1.1\r\nAlpha: value\r\nBeta: value\r\n\r\n";
	xhttpfield Fields[4];
	xhttp1errorinfo Error;
	xhttp1limits Limits;
	xhttp1head Head;

	xrtHttp1LimitsInit(&Limits);
	Limits.MaxStartLine = 3;
	xrtHttp1HeadInit(&Head, Fields, 4);
	testRequire((xrtHttp1RequestParse(
		testHttp1Bytes(Message, sizeof(Message) - 1u),
		&Head, &Limits, &Error
	) == XHTTP1_ERROR) &&
		(Error.Code == XHTTP1_ERROR_START_LINE_TOO_LARGE),
		"HTTP/1 start-line limit mismatch");

	xrtHttp1LimitsInit(&Limits);
	Limits.MaxFieldLine = 5;
	xrtHttp1HeadInit(&Head, Fields, 4);
	testRequire((xrtHttp1RequestParse(
		testHttp1Bytes(Message, sizeof(Message) - 1u),
		&Head, &Limits, &Error
	) == XHTTP1_ERROR) &&
		(Error.Code == XHTTP1_ERROR_FIELD_LINE_TOO_LARGE),
		"HTTP/1 field-line limit mismatch");

	xrtHttp1LimitsInit(&Limits);
	Limits.MaxFields = 1;
	xrtHttp1HeadInit(&Head, Fields, 4);
	testRequire((xrtHttp1RequestParse(
		testHttp1Bytes(Message, sizeof(Message) - 1u),
		&Head, &Limits, &Error
	) == XHTTP1_ERROR) &&
		(Error.Code == XHTTP1_ERROR_TOO_MANY_FIELDS),
		"HTTP/1 field-count limit mismatch");

	xrtHttp1LimitsInit(&Limits);
	Limits.MaxHead = 16;
	xrtHttp1HeadInit(&Head, Fields, 4);
	testRequire((xrtHttp1RequestParse(
		testHttp1Bytes(Message, sizeof(Message) - 1u),
		&Head, &Limits, &Error
	) == XHTTP1_ERROR) &&
		(Error.Code == XHTTP1_ERROR_HEAD_TOO_LARGE),
		"HTTP/1 total Header limit mismatch");
}



/* 验证请求和响应封包支持精确查询、原子容量失败与解析往返。 */
static void testHttp1Write(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Host"), XRT_STR_INIT("example.test") },
		{ XRT_STR_INIT("Accept"), XRT_STR_INIT("application/json") }
	};
	static const char Expected[] =
		"GET /v1 HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Accept: application/json\r\n\r\n";
	struct {
		char Output[8];
		unsigned char Gap[8];
		size_t Size;
	} Short;
	char Output[256];
	xhttpfield ParsedFields[2];
	xhttp1head Head;
	size_t iSize = 77;

	testRequire(xrtHttp1RequestWrite(
		XRT_STR_LITERAL("GET"), XRT_STR_LITERAL("/v1"),
		XHTTP_VERSION_1_1, Fields, 2,
		NULL, 0, &iSize
	) && (iSize == sizeof(Expected) - 1u),
		"HTTP/1 request size query mismatch");
	memset(&Short, 0xA5, sizeof(Short));
	testRequire(!xrtHttp1RequestWrite(
		XRT_STR_LITERAL("GET"), XRT_STR_LITERAL("/v1"),
		XHTTP_VERSION_1_1, Fields, 2,
		Short.Output, sizeof(Short.Output), &Short.Size
	) && (Short.Size == sizeof(Expected) - 1u) &&
		((unsigned char)Short.Output[0] == UINT8_C(0xA5)) &&
		(xrtErrorCode(xrtGetError()) == XHTTP1_ERROR_OUTPUT_SIZE),
		"HTTP/1 request capacity failure was not atomic");
	testRequire(xrtHttp1RequestWrite(
		XRT_STR_LITERAL("GET"), XRT_STR_LITERAL("/v1"),
		XHTTP_VERSION_1_1, Fields, 2,
		Output, sizeof(Output), &iSize
	) && (iSize == sizeof(Expected) - 1u) &&
		(memcmp(Output, Expected, iSize) == 0),
		"HTTP/1 request write mismatch");
	xrtHttp1HeadInit(&Head, ParsedFields, 2);
	testRequire(xrtHttp1RequestParse(
		testHttp1Bytes(Output, iSize), &Head, NULL, NULL
	) == XHTTP1_READY, "HTTP/1 written request did not parse");

	testRequire(xrtHttp1ResponseWrite(
		XHTTP_VERSION_1_1, 204, XRT_STR_LITERAL(""),
		NULL, 0, Output, sizeof(Output), &iSize
	) && (iSize == sizeof("HTTP/1.1 204 \r\n\r\n") - 1u) &&
		(memcmp(Output, "HTTP/1.1 204 \r\n\r\n", iSize) == 0),
		"HTTP/1 empty-reason response write mismatch");
	xrtHttp1HeadInit(&Head, NULL, 0);
	testRequire(xrtHttp1ResponseParse(
		testHttp1Bytes(Output, iSize), &Head, NULL, NULL
	) == XHTTP1_READY, "HTTP/1 written response did not parse");
}



/* 验证封包器拒绝字段注入和会破坏输入的重叠输出。 */
static void testHttp1WriteRejects(void)
{
	static const char BadValue[] = { 'o', 'k', '\r', '\n', 'X' };
	xhttpfield Field;
	xhttpfield Empty;
	char Buffer[128] = "GET";
	size_t iSize = 0;

	Field.Name = XRT_STR_LITERAL("X-Test");
	Field.Value.Data = BadValue;
	Field.Value.Size = sizeof(BadValue);
	testRequire(!xrtHttp1RequestWrite(
		XRT_STR_LITERAL("GET"), XRT_STR_LITERAL("/"),
		XHTTP_VERSION_1_1, &Field, 1,
		NULL, 0, &iSize
	) && (xrtErrorCode(xrtGetError()) == XHTTP1_ERROR_FIELD_VALUE),
		"HTTP/1 writer accepted field injection");
	Field.Value = XRT_STR_LITERAL("ok");
	testRequire(!xrtHttp1RequestWrite(
		(xstrview){ Buffer, 3 }, XRT_STR_LITERAL("/"),
		XHTTP_VERSION_1_1, &Field, 1,
		Buffer, sizeof(Buffer), &iSize
	) && (xrtErrorCode(xrtGetError()) == XHTTP1_ERROR_ARGUMENT),
		"HTTP/1 writer accepted overlapping output");
	Empty.Name = XRT_STR_LITERAL("X-Empty");
	Empty.Value.Data = NULL;
	Empty.Value.Size = 0;
	testRequire(xrtHttp1ResponseWrite(
		XHTTP_VERSION_1_1, 200, (xstrview){ NULL, 0 },
		&Empty, 1, Buffer, sizeof(Buffer), &iSize
	), "HTTP/1 writer rejected a valid empty field value");
}



/* 验证封包器完整处理未对齐描述符、回绕范围和全部输出别名。 */
static void testHttp1WriteMemoryContracts(void)
{
	static const char Expected[] =
		"GET / HTTP/1.1\r\n"
		"X-Test: ok\r\n\r\n";
	unsigned char FieldStorage[sizeof(xhttpfield) + 2u];
	unsigned char SizeStorage[sizeof(size_t) + 2u];
	xhttpfield Field = {
		XRT_STR_LITERAL("X-Test"),
		XRT_STR_LITERAL("ok")
	};
	const xhttpfield* pField =
		(const xhttpfield*)(const void*)(FieldStorage + 1u);
	size_t* pUnalignedSize =
		(size_t*)(void*)(SizeStorage + 1u);
	char Output[128];
	size_t iSize;

	memset(FieldStorage, 0xA5, sizeof(FieldStorage));
	memcpy(FieldStorage + 1u, &Field, sizeof(Field));
	memset(SizeStorage, 0xA5, sizeof(SizeStorage));
	testRequire(xrtHttp1RequestWrite(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/"),
		XHTTP_VERSION_1_1,
		pField,
		1,
		Output,
		sizeof(Output),
		pUnalignedSize
	), "HTTP/1 writer rejected unaligned descriptors or Size");
	memcpy(&iSize, SizeStorage + 1u, sizeof(iSize));
	testRequire(
		(iSize == (sizeof(Expected) - 1u)) &&
		(memcmp(Output, Expected, iSize) == 0) &&
		(FieldStorage[0] == 0xA5) &&
		(FieldStorage[sizeof(FieldStorage) - 1u] == 0xA5) &&
		(SizeStorage[0] == 0xA5) &&
		(SizeStorage[sizeof(SizeStorage) - 1u] == 0xA5),
		"HTTP/1 unaligned writer result damaged guards"
	);

	testRequire(!xrtHttp1RequestWrite(
		XRT_STR_LITERAL("GET"), XRT_STR_LITERAL("/"),
		XHTTP_VERSION_1_1,
		(const xhttpfield*)(uintptr_t)(UINTPTR_MAX - 1u),
		1,
		NULL,
		0,
		&iSize
	) && (xrtErrorCode(xrtGetError()) == XHTTP1_ERROR_ARGUMENT),
		"HTTP/1 writer accepted a wrapping field array");
	xrtClearError();
	testRequire(!xrtHttp1RequestWrite(
		XRT_STR_LITERAL("GET"), XRT_STR_LITERAL("/"),
		XHTTP_VERSION_1_1,
		pField,
		1,
		NULL,
		0,
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
	) && (xrtErrorCode(xrtGetError()) == XHTTP1_ERROR_ARGUMENT),
		"HTTP/1 writer accepted a wrapping Size output");
	xrtClearError();
	testRequire(!xrtHttp1RequestWrite(
		XRT_STR_LITERAL("GET"), XRT_STR_LITERAL("/"),
		XHTTP_VERSION_1_1,
		pField,
		1,
		(void*)(uintptr_t)(UINTPTR_MAX - 1u),
		sizeof(Output),
		&iSize
	) && (xrtErrorCode(xrtGetError()) == XHTTP1_ERROR_ARGUMENT),
		"HTTP/1 writer accepted a wrapping output range");
	xrtClearError();
	testRequire(!xrtHttp1RequestWrite(
		XRT_STR_LITERAL("GET"), XRT_STR_LITERAL("/"),
		XHTTP_VERSION_1_1,
		pField,
		1,
		FieldStorage + 1u,
		sizeof(Output),
		&iSize
	) && (xrtErrorCode(xrtGetError()) == XHTTP1_ERROR_ARGUMENT),
		"HTTP/1 writer accepted output over its field descriptors");
	xrtClearError();
	testRequire(!xrtHttp1RequestWrite(
		XRT_STR_LITERAL("GET"), XRT_STR_LITERAL("/"),
		XHTTP_VERSION_1_1,
		pField,
		1,
		NULL,
		0,
		(size_t*)(void*)(FieldStorage + 1u)
	) && (xrtErrorCode(xrtGetError()) == XHTTP1_ERROR_ARGUMENT),
		"HTTP/1 writer accepted Size over its field descriptors");
	xrtClearError();
	testRequire(!xrtHttp1RequestWrite(
		XRT_STR_LITERAL("GET"), XRT_STR_LITERAL("/"),
		XHTTP_VERSION_1_1,
		pField,
		1,
		Output,
		sizeof(Output),
		(size_t*)(void*)Output
	) && (xrtErrorCode(xrtGetError()) == XHTTP1_ERROR_ARGUMENT),
		"HTTP/1 writer accepted overlapping output and Size");
	xrtClearError();
	testRequire(xrtHttp1ResponseWrite(
		XHTTP_VERSION_1_1,
		200,
		XRT_STR_LITERAL("OK"),
		pField,
		1,
		Output,
		sizeof(Output),
		&iSize
	), "HTTP/1 writer did not recover after invalid ranges");
}



/* 执行 HTTP/1 Header 解析和封包完整回归。 */
int main(void)
{
	testHttp1Target();
	testHttp1Request();
	testHttp1Response();
	testHttp1Incremental();
	testHttp1FieldCapacity();
	testHttp1DynamicFieldLength();
	testHttp1RepeatedLength();
	testHttp1TransferFacts();
	testHttp1TransferCodingIterator();
	testHttp1RejectsMalformed();
	testHttp1LimitMemoryContracts();
	testHttp1Limits();
	testHttp1Write();
	testHttp1WriteRejects();
	testHttp1WriteMemoryContracts();
	printf("[PASS] http1\n");
	return 0;
}
