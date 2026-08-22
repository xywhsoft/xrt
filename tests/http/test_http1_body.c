#include "../test.h"

#include <xrt/http_trailer.h>



/* 把字符串视图与字面量按长度比较。 */
static bool testHttp1TextEqual(xstrview Text, cstr sExpected)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		(memcmp(Text.Data, sExpected, iSize) == 0);
}



/* 解析一份请求 Header，测试失败时立即终止。 */
static void testHttp1ParseRequest(
	cstr sMessage,
	xhttp1head* pHead,
	xhttpfield* pFields,
	size_t iCapacity
)
{
	xhttp1errorinfo Error;
	xbytesview Input = { (cbytes)sMessage, strlen(sMessage) };

	xrtHttp1HeadInit(pHead, pFields, iCapacity);
	testRequire(xrtHttp1RequestParse(
		Input, pHead, NULL, &Error
	) == XHTTP1_READY, "HTTP/1 request fixture did not parse");
}



/* 解析一份响应 Header，测试失败时立即终止。 */
static void testHttp1ParseResponse(
	cstr sMessage,
	xhttp1head* pHead,
	xhttpfield* pFields,
	size_t iCapacity
)
{
	xhttp1errorinfo Error;
	xbytesview Input = { (cbytes)sMessage, strlen(sMessage) };

	xrtHttp1HeadInit(pHead, pFields, iCapacity);
	testRequire(xrtHttp1ResponseParse(
		Input, pHead, NULL, &Error
	) == XHTTP1_READY, "HTTP/1 response fixture did not parse");
}



/* 按固定窗口逐步暴露输入，模拟网络分片并收集零拷贝正文片段。 */
static xhttp1bodystatus testHttp1BodyRun(
	xhttp1body* pBody,
	const unsigned char* pWire,
	size_t iWireSize,
	size_t iSplit,
	bool bEnd,
	unsigned char* pOutput,
	size_t iOutputCapacity,
	size_t* pOutputSize,
	size_t* pWireConsumed,
	xhttp1errorinfo* pError
)
{
	size_t iOffset = 0;
	size_t iVisible = 0;
	size_t iOutput = 0;
	size_t iGuard = 0;
	xhttp1bodystatus Status = XHTTP1_BODY_MORE;

	while ( iGuard++ < (iWireSize * 8u + 128u) ) {
		xbytesview Input;
		xbytesview Data;
		size_t iConsumed = 0;

		if ( (iVisible < iWireSize) &&
			((iVisible == iOffset) || (Status == XHTTP1_BODY_MORE)) ) {
			size_t iNext = iVisible + iSplit;

			if ( (iNext < iVisible) || (iNext > iWireSize) ) {
				iNext = iWireSize;
			}
			iVisible = iNext;
		}
		Input.Data = pWire + iOffset;
		Input.Size = iVisible - iOffset;
		Status = xrtHttp1BodyRead(
			pBody,
			Input,
			bEnd && (iVisible == iWireSize),
			&iConsumed,
			&Data,
			pError
		);
		testRequire(iConsumed <= Input.Size,
			"HTTP/1 Body consumed beyond visible input");
		if ( Status == XHTTP1_BODY_DATA ) {
			testRequire((Data.Data != NULL) &&
				(Data.Size != 0) &&
				(Data.Size <= (iOutputCapacity - iOutput)),
				"HTTP/1 Body published an invalid data view");
			memcpy(pOutput + iOutput, Data.Data, Data.Size);
			iOutput += Data.Size;
		}
		iOffset += iConsumed;
		if ( (Status == XHTTP1_BODY_DONE) ||
			(Status == XHTTP1_BODY_ERROR) ||
			(Status == XHTTP1_BODY_FIELDS) ) {
			break;
		}
		if ( (Status == XHTTP1_BODY_MORE) &&
			(iConsumed == 0) && (iVisible == iWireSize) && !bEnd ) {
			break;
		}
	}
	testRequire(iGuard < (iWireSize * 8u + 128u),
		"HTTP/1 Body Reader made no bounded progress");
	*pOutputSize = iOutput;
	*pWireConsumed = iOffset;
	return Status;
}



/* 验证请求和响应分帧严格遵守 RFC 9112 的优先级。 */
static void testHttp1BodyPlans(void)
{
	xhttpfield Fields[4];
	xhttp1bodyplan Plan;
	xhttp1head Head;

	testHttp1ParseRequest(
		"GET / HTTP/1.1\r\nHost: x\r\n\r\n",
		&Head, Fields, 4
	);
	testRequire(xrtHttp1RequestBodyPlan(&Head, &Plan) &&
		(Plan.Mode == XHTTP1_BODY_NONE),
		"HTTP/1 request without framing did not become NONE");

	testHttp1ParseRequest(
		"POST / HTTP/1.1\r\nContent-Length: 3\r\n\r\n",
		&Head, Fields, 4
	);
	testRequire(xrtHttp1RequestBodyPlan(&Head, &Plan) &&
		(Plan.Mode == XHTTP1_BODY_FIXED) && (Plan.Length == 3),
		"HTTP/1 Content-Length request plan mismatch");

	testHttp1ParseRequest(
		"POST / HTTP/1.1\r\nTransfer-Encoding: gzip, chunked\r\n\r\n",
		&Head, Fields, 4
	);
	testRequire(xrtHttp1RequestBodyPlan(&Head, &Plan) &&
		(Plan.Mode == XHTTP1_BODY_CHUNKED),
		"HTTP/1 layered chunked request plan mismatch");

	testHttp1ParseRequest(
		"POST / HTTP/1.1\r\n"
		"Transfer-Encoding: gzip; level=\"a,b\"; mode=fast\r\n"
		"Transfer-Encoding: chunked\r\n\r\n",
		&Head, Fields, 4
	);
	testRequire(xrtHttp1RequestBodyPlan(&Head, &Plan) &&
		(Plan.Mode == XHTTP1_BODY_CHUNKED),
		"HTTP/1 ordered transfer coding fields or parameters were rejected");

	testHttp1ParseRequest(
		"POST / HTTP/1.1\r\nTransfer-Encoding: gzip\r\n\r\n",
		&Head, Fields, 4
	);
	testRequire(!xrtHttp1RequestBodyPlan(&Head, &Plan) &&
		(xrtErrorCode(xrtGetError()) ==
		 (int32)XHTTP1_ERROR_REQUEST_TRANSFER_ENCODING),
		"HTTP/1 unframed request transfer coding was accepted");
	xrtClearError();

	testHttp1ParseRequest(
		"POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n"
		"Content-Length: 1\r\n\r\n",
		&Head, Fields, 4
	);
	testRequire(!xrtHttp1RequestBodyPlan(&Head, &Plan) &&
		(xrtErrorCode(xrtGetError()) ==
		 (int32)XHTTP1_ERROR_TRANSFER_LENGTH),
		"HTTP/1 ambiguous request framing was accepted");
	xrtClearError();

	testHttp1ParseResponse(
		"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n"
		"Content-Length: 9\r\n\r\n",
		&Head, Fields, 4
	);
	testRequire(xrtHttp1ResponseBodyPlan(
		&Head, XRT_STR_LITERAL("HEAD"), &Plan
	) && (Plan.Mode == XHTTP1_BODY_NONE),
		"HTTP/1 HEAD response did not ignore framing fields");
	testRequire(xrtHttp1ResponseBodyPlan(
		&Head, XRT_STR_LITERAL("CONNECT"), &Plan
	) && (Plan.Mode == XHTTP1_BODY_TUNNEL),
		"HTTP/1 successful CONNECT response did not become tunnel");
	testRequire(!xrtHttp1ResponseBodyPlan(
		&Head, XRT_STR_LITERAL("GET"), &Plan
	) && (xrtErrorCode(xrtGetError()) ==
		(int32)XHTTP1_ERROR_TRANSFER_LENGTH),
		"HTTP/1 ordinary ambiguous response framing was accepted");
	xrtClearError();
	testRequire(!xrtHttp1ResponseBodyPlan(
		&Head, XRT_STR_LITERAL("head"), &Plan
	) && (xrtErrorCode(xrtGetError()) ==
		(int32)XHTTP1_ERROR_TRANSFER_LENGTH),
		"HTTP/1 lowercase head method changed response framing");
	xrtClearError();
	testRequire(!xrtHttp1ResponseBodyPlan(
		&Head, XRT_STR_LITERAL("connect"), &Plan
	) && (xrtErrorCode(xrtGetError()) ==
		(int32)XHTTP1_ERROR_TRANSFER_LENGTH),
		"HTTP/1 lowercase connect method changed response framing");
	xrtClearError();

	testHttp1ParseResponse(
		"HTTP/1.1 204 No Content\r\nTransfer-Encoding: chunked\r\n"
		"Content-Length: 9\r\n\r\n",
		&Head, Fields, 4
	);
	testRequire(xrtHttp1ResponseBodyPlan(
		&Head, XRT_STR_LITERAL("GET"), &Plan
	) && (Plan.Mode == XHTTP1_BODY_NONE),
		"HTTP/1 204 response did not take no-body precedence");
	testRequire(xrtHttp1ResponseBodyPlan(
		&Head, XRT_STR_LITERAL("CONNECT"), &Plan
	) && (Plan.Mode == XHTTP1_BODY_TUNNEL),
		"HTTP/1 successful CONNECT 204 response did not become tunnel");

	testHttp1ParseResponse(
		"HTTP/1.1 101 Switching Protocols\r\n"
		"Connection: Upgrade\r\nUpgrade: websocket\r\n"
		"Content-Length: 9\r\n\r\n",
		&Head, Fields, 4
	);
	testRequire(xrtHttp1ResponseBodyPlan(
		&Head, XRT_STR_LITERAL("GET"), &Plan
	) && (Plan.Mode == XHTTP1_BODY_TUNNEL),
		"HTTP/1 101 response did not hand off upgraded bytes");
	testRequire(xrtHttp1ResponseBodyPlan(
		&Head, XRT_STR_LITERAL("HEAD"), &Plan
	) && (Plan.Mode == XHTTP1_BODY_TUNNEL),
		"HTTP/1 101 response to HEAD did not hand off upgraded bytes");

	testHttp1ParseResponse(
		"HTTP/1.1 101 Switching Protocols\r\n\r\n",
		&Head, Fields, 4
	);
	testRequire(!xrtHttp1ResponseBodyPlan(
		&Head, XRT_STR_LITERAL("GET"), &Plan
	) && (xrtErrorCode(xrtGetError()) ==
		(int32)XHTTP1_ERROR_UPGRADE),
		"HTTP/1 101 response without Upgrade was accepted");
	xrtClearError();
	testRequire(!xrtHttp1ResponseBodyPlan(
		&Head, XRT_STR_LITERAL("HEAD"), &Plan
	) && (xrtErrorCode(xrtGetError()) ==
		(int32)XHTTP1_ERROR_UPGRADE),
		"HTTP/1 101 response to HEAD bypassed Upgrade validation");
	xrtClearError();
	testRequire(!xrtHttp1ResponseBodyPlan(
		&Head, (xstrview){ NULL, 0 }, &Plan
	) && (xrtErrorCode(xrtGetError()) ==
		(int32)XHTTP1_ERROR_ARGUMENT),
		"HTTP/1 response plan accepted a missing request method");
	xrtClearError();

	testHttp1ParseResponse(
		"HTTP/1.1 407 Proxy Authentication Required\r\n"
		"Content-Length: 2\r\n\r\n",
		&Head, Fields, 4
	);
	testRequire(xrtHttp1ResponseBodyPlan(
		&Head, XRT_STR_LITERAL("CONNECT"), &Plan
	) && (Plan.Mode == XHTTP1_BODY_FIXED) && (Plan.Length == 2),
		"HTTP/1 failed CONNECT response lost normal framing");

	testHttp1ParseResponse(
		"HTTP/1.1 200 OK\r\nTransfer-Encoding: gzip\r\n\r\n",
		&Head, Fields, 4
	);
	testRequire(xrtHttp1ResponseBodyPlan(
		&Head, XRT_STR_LITERAL("GET"), &Plan
	) && (Plan.Mode == XHTTP1_BODY_CLOSE),
		"HTTP/1 non-final response transfer coding was not close-delimited");

	testHttp1ParseResponse(
		"HTTP/1.1 200 OK\r\n"
		"Transfer-Encoding: chunked, gzip\r\n\r\n",
		&Head, Fields, 4
	);
	testRequire(xrtHttp1ResponseBodyPlan(
		&Head, XRT_STR_LITERAL("GET"), &Plan
	) && (Plan.Mode == XHTTP1_BODY_CLOSE),
		"HTTP/1 response with non-final chunked was not close-delimited");

	testHttp1ParseRequest(
		"POST / HTTP/1.1\r\n"
		"Transfer-Encoding: chunked, gzip\r\n\r\n",
		&Head, Fields, 4
	);
	testRequire(!xrtHttp1RequestBodyPlan(&Head, &Plan) &&
		(xrtErrorCode(xrtGetError()) ==
		 (int32)XHTTP1_ERROR_REQUEST_TRANSFER_ENCODING),
		"HTTP/1 request with non-final chunked was accepted");
	xrtClearError();

	testHttp1ParseResponse(
		"HTTP/1.1 200 OK\r\n\r\n",
		&Head, Fields, 4
	);
	testRequire(xrtHttp1ResponseBodyPlan(
		&Head, XRT_STR_LITERAL("GET"), &Plan
	) && (Plan.Mode == XHTTP1_BODY_CLOSE),
		"HTTP/1 response without framing was not close-delimited");
}



/* 验证定长正文只消费声明长度并保留流水线后缀。 */
static void testHttp1FixedBody(void)
{
	static const unsigned char First[] = "he";
	static const unsigned char Second[] = "lloNEXT";
	xhttp1bodylimits Limits;
	xhttp1errorinfo Error;
	xhttp1bodyplan Plan = { XHTTP1_BODY_FIXED, 5 };
	xhttp1body Body;
	xbytesview Data;
	size_t iConsumed;

	xrtHttp1BodyLimitsInit(&Limits);
	testRequire(xrtHttp1BodyInit(
		&Body, &Plan, NULL, 0, &Limits
	), "HTTP/1 fixed Body init failed");
	testRequire(xrtHttp1BodyRead(
		&Body, (xbytesview){ First, sizeof(First) - 1u }, false,
		&iConsumed, &Data, &Error
	) == XHTTP1_BODY_DATA, "HTTP/1 fixed first fragment failed");
	testRequire((iConsumed == 2) && (Data.Size == 2) &&
		(memcmp(Data.Data, "he", 2) == 0),
		"HTTP/1 fixed first fragment mismatch");
	testRequire(xrtHttp1BodyRead(
		&Body, (xbytesview){ Second, sizeof(Second) - 1u }, false,
		&iConsumed, &Data, &Error
	) == XHTTP1_BODY_DATA, "HTTP/1 fixed final fragment failed");
	testRequire((iConsumed == 3) && (Data.Size == 3) &&
		(memcmp(Data.Data, "llo", 3) == 0) &&
		xrtHttp1BodyDone(&Body) &&
		(Body.Received == 5) && (Body.WireBytes == 5),
		"HTTP/1 fixed completion or suffix boundary mismatch");
	testRequire(xrtHttp1BodyRead(
		&Body, (xbytesview){ Second + 3, 4 }, false,
		&iConsumed, &Data, &Error
	) == XHTTP1_BODY_DONE && (iConsumed == 0),
		"HTTP/1 fixed Body consumed pipelined suffix");

	Plan.Length = 4;
	testRequire(xrtHttp1BodyInit(
		&Body, &Plan, NULL, 0, &Limits
	), "HTTP/1 incomplete fixed Body init failed");
	testRequire(xrtHttp1BodyRead(
		&Body, (xbytesview){ First, sizeof(First) - 1u }, true,
		&iConsumed, &Data, &Error
	) == XHTTP1_BODY_DATA, "HTTP/1 fixed data was not published before EOF");
	testRequire(xrtHttp1BodyRead(
		&Body, (xbytesview){ NULL, 0 }, true,
		&iConsumed, &Data, &Error
	) == XHTTP1_BODY_ERROR &&
		(Error.Code == XHTTP1_ERROR_BODY_INCOMPLETE),
		"HTTP/1 incomplete fixed Body was accepted");

	Limits.MaxBody = 0;
	Plan.Length = 0;
	testRequire(xrtHttp1BodyInit(
		&Body, &Plan, NULL, 0, &Limits
	) && xrtHttp1BodyDone(&Body),
		"HTTP/1 zero body policy rejected an empty fixed Body");
	Plan.Length = 1;
	testRequire(!xrtHttp1BodyInit(
		&Body, &Plan, NULL, 0, &Limits
	) && (xrtErrorCode(xrtGetError()) ==
		(int32)XHTTP1_ERROR_BODY_TOO_LARGE),
		"HTTP/1 zero body policy accepted a nonempty fixed Body");
	xrtClearError();
}



/* 验证关闭定界正文只在可靠 EOF 到达后完成。 */
static void testHttp1CloseBody(void)
{
	static const unsigned char Payload[] = "close-body";
	xhttp1bodylimits Limits;
	xhttp1errorinfo Error;
	xhttp1bodyplan Plan = { XHTTP1_BODY_CLOSE, 0 };
	xhttp1body Body;
	xbytesview Data;
	size_t iConsumed;

	xrtHttp1BodyLimitsInit(&Limits);
	testRequire(xrtHttp1BodyInit(
		&Body, &Plan, NULL, 0, &Limits
	), "HTTP/1 close Body init failed");
	testRequire(xrtHttp1BodyRead(
		&Body, (xbytesview){ Payload, sizeof(Payload) - 1u }, true,
		&iConsumed, &Data, &Error
	) == XHTTP1_BODY_DATA &&
		(iConsumed == (sizeof(Payload) - 1u)),
		"HTTP/1 close Body data mismatch");
	testRequire(xrtHttp1BodyRead(
		&Body, (xbytesview){ NULL, 0 }, true,
		&iConsumed, &Data, &Error
	) == XHTTP1_BODY_DONE && xrtHttp1BodyDone(&Body),
		"HTTP/1 close Body did not finish at EOF");

	Limits.MaxBody = 4;
	testRequire(xrtHttp1BodyInit(
		&Body, &Plan, NULL, 0, &Limits
	), "HTTP/1 limited close Body init failed");
	testRequire(xrtHttp1BodyRead(
		&Body, (xbytesview){ Payload, sizeof(Payload) - 1u }, false,
		&iConsumed, &Data, &Error
	) == XHTTP1_BODY_ERROR &&
		(Error.Code == XHTTP1_ERROR_BODY_TOO_LARGE) &&
		(iConsumed == 0),
		"HTTP/1 oversized close Body was partly published");
}



/* 验证 chunk 扩展、逐字节分片、trailer 与流水线后缀。 */
static void testHttp1ChunkedBody(void)
{
	static const unsigned char Wire[] =
		"4 ; foo = \"a\\\"b\"\r\nWiki\r\n"
		"5;bar\r\npedia\r\n"
		"0;end=yes\r\n"
		"X-Checksum: ok\r\nX-Meta: yes\r\n\r\nNEXT";
	xhttpfield Trailers[2];
	xhttp1bodylimits Limits;
	xhttp1errorinfo Error;
	xhttp1bodyplan Plan = { XHTTP1_BODY_CHUNKED, 0 };
	xhttp1body Body;
	unsigned char Output[32];
	size_t iOutput = 0;
	size_t iConsumed = 0;
	xhttp1bodystatus Status;

	xrtHttp1BodyLimitsInit(&Limits);
	testRequire(xrtHttp1BodyInit(
		&Body, &Plan, Trailers, 2, &Limits
	), "HTTP/1 chunked Body init failed");
	Status = testHttp1BodyRun(
		&Body, Wire, sizeof(Wire) - 1u, 1, false,
		Output, sizeof(Output), &iOutput, &iConsumed, &Error
	);
	testRequire(Status == XHTTP1_BODY_DONE,
		"HTTP/1 one-byte chunked decode did not finish");
	testRequire((iOutput == 9) &&
		(memcmp(Output, "Wikipedia", 9) == 0),
		"HTTP/1 chunked decoded payload mismatch");
	testRequire((iConsumed == (sizeof(Wire) - 1u - 4u)) &&
		(Body.WireBytes == (uint64)iConsumed) &&
		(Body.Received == 9),
		"HTTP/1 chunked wire or pipeline boundary mismatch");
	testRequire((Body.TrailerCount == 2) &&
		testHttp1TextEqual(Trailers[0].Name, "X-Checksum") &&
		testHttp1TextEqual(Trailers[0].Value, "ok") &&
		testHttp1TextEqual(Trailers[1].Name, "X-Meta") &&
		testHttp1TextEqual(Trailers[1].Value, "yes"),
		"HTTP/1 trailer fields mismatch");
}



/* 验证 trailer 描述符不足可以原地扩容后重试。 */
static void testHttp1TrailerRebind(void)
{
	static const unsigned char Wire[] =
		"0\r\nAlpha: one\r\nBeta: two\r\n\r\nNEXT";
	xhttpfield Small[1];
	xhttpfield Full[2];
	xhttp1bodylimits Limits;
	xhttp1errorinfo Error;
	xhttp1bodyplan Plan = { XHTTP1_BODY_CHUNKED, 0 };
	xhttp1body Body;
	xbytesview Data;
	size_t iConsumed;
	xhttp1bodystatus Status;

	xrtHttp1BodyLimitsInit(&Limits);
	testRequire(xrtHttp1BodyInit(
		&Body, &Plan, Small, 1, &Limits
	), "HTTP/1 trailer rebind Body init failed");
	Status = xrtHttp1BodyRead(
		&Body, (xbytesview){ Wire, sizeof(Wire) - 1u }, false,
		&iConsumed, &Data, &Error
	);
	testRequire((Status == XHTTP1_BODY_FIELDS) &&
		(iConsumed == 3) && (Body.TrailerCount == 2),
		"HTTP/1 trailer descriptor shortage mismatch");
	testRequire(xrtHttp1BodyTrailers(&Body, Full, 2),
		"HTTP/1 trailer descriptor rebind failed");
	Status = xrtHttp1BodyRead(
		&Body,
		(xbytesview){ Wire + iConsumed, sizeof(Wire) - 1u - iConsumed },
		false, &iConsumed, &Data, &Error
	);
	testRequire((Status == XHTTP1_BODY_DONE) &&
		(iConsumed == (sizeof(Wire) - 1u - 3u - 4u)) &&
		(Body.TrailerCount == 2) &&
		testHttp1TextEqual(Full[1].Value, "two"),
		"HTTP/1 trailer retry or suffix boundary mismatch");
}



/* 运行一份预期失败的 chunked 输入并核对稳定错误码。 */
static void testHttp1ChunkReject(
	cstr sWire,
	const xhttp1bodylimits* pLimits,
	xhttp1error Code
)
{
	xhttpfield Fields[4];
	xhttp1errorinfo Error;
	xhttp1bodyplan Plan = { XHTTP1_BODY_CHUNKED, 0 };
	xhttp1body Body;
	unsigned char Output[64];
	size_t iOutput;
	size_t iConsumed;
	xhttp1bodystatus Status;

	testRequire(xrtHttp1BodyInit(
		&Body, &Plan, Fields, 4, pLimits
	), "HTTP/1 malformed chunk fixture init failed");
	Status = testHttp1BodyRun(
		&Body, (const unsigned char*)sWire, strlen(sWire),
		strlen(sWire) + 1u, true,
		Output, sizeof(Output), &iOutput, &iConsumed, &Error
	);
	testRequire((Status == XHTTP1_BODY_ERROR) &&
		(Error.Code == Code) &&
		(xrtGetError() != NULL) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.http1") == 0),
		"HTTP/1 malformed chunk error mismatch");
	xrtClearError();
}



/* 错误前已经推进的 chunk 终止符前缀必须仍可从输入移除。 */
static void testHttp1ChunkErrorConsumed(void)
{
	static const unsigned char HeadAndData[] = "1\r\na";
	static const unsigned char InvalidEnd[] = "\rX";
	xhttp1bodylimits Limits;
	xhttp1bodyplan Plan = { XHTTP1_BODY_CHUNKED, 0 };
	xhttp1errorinfo Error;
	xhttp1body Body;
	xbytesview Data;
	size_t iConsumed;

	xrtHttp1BodyLimitsInit(&Limits);
	testRequire(xrtHttp1BodyInit(
		&Body, &Plan, NULL, 0, &Limits
	), "HTTP/1 chunk consumed-error fixture init failed");
	testRequire((xrtHttp1BodyRead(
		&Body,
		(xbytesview){ HeadAndData, sizeof(HeadAndData) - 1u },
		false,
		&iConsumed,
		&Data,
		&Error
	) == XHTTP1_BODY_DATA) &&
		(iConsumed == (sizeof(HeadAndData) - 1u)) &&
		(Data.Size == 1u),
		"HTTP/1 chunk consumed-error fixture data mismatch");
	testRequire((xrtHttp1BodyRead(
		&Body,
		(xbytesview){ InvalidEnd, sizeof(InvalidEnd) - 1u },
		false,
		&iConsumed,
		&Data,
		&Error
	) == XHTTP1_BODY_ERROR) &&
		(iConsumed == 1u) &&
		(Error.Code == XHTTP1_ERROR_CHUNK_TERMINATOR),
		"HTTP/1 chunk error lost its consumed wire prefix");
}



/* 验证 chunk 数值、扩展、终止符、限额和 EOF 边界。 */
static void testHttp1ChunkRejects(void)
{
	xhttp1bodylimits Limits;

	xrtHttp1BodyLimitsInit(&Limits);
	testHttp1ChunkReject(
		"\r\n", &Limits, XHTTP1_ERROR_CHUNK_SIZE
	);
	testHttp1ChunkReject(
		"10000000000000000\r\n", &Limits, XHTTP1_ERROR_CHUNK_SIZE
	);
	testHttp1ChunkReject(
		"1;=bad\r\na\r\n0\r\n\r\n",
		&Limits, XHTTP1_ERROR_CHUNK_EXTENSION
	);
	testHttp1ChunkReject(
		"1;x=\"bad\r\na\r\n0\r\n\r\n",
		&Limits, XHTTP1_ERROR_CHUNK_EXTENSION
	);
	testHttp1ChunkReject(
		"1 \r\na\r\n0\r\n\r\n",
		&Limits, XHTTP1_ERROR_CHUNK_SIZE
	);
	testHttp1ChunkReject(
		"1;x \r\na\r\n0\r\n\r\n",
		&Limits, XHTTP1_ERROR_CHUNK_EXTENSION
	);
	testHttp1ChunkReject(
		"1;x=y \r\na\r\n0\r\n\r\n",
		&Limits, XHTTP1_ERROR_CHUNK_EXTENSION
	);
	testHttp1ChunkReject(
		"1\r\naXX0\r\n\r\n",
		&Limits, XHTTP1_ERROR_CHUNK_TERMINATOR
	);
	testHttp1ChunkReject(
		"1\na\r\n0\r\n\r\n",
		&Limits, XHTTP1_ERROR_CHUNK_SIZE
	);
	testHttp1ChunkReject(
		"1\r\na\r\n", &Limits, XHTTP1_ERROR_BODY_INCOMPLETE
	);

	Limits.MaxChunkLine = 3;
	testHttp1ChunkReject(
		"4;xx\r\ntest\r\n0\r\n\r\n",
		&Limits, XHTTP1_ERROR_CHUNK_LINE_TOO_LARGE
	);
	xrtHttp1BodyLimitsInit(&Limits);
	Limits.MaxBody = 3;
	testHttp1ChunkReject(
		"4\r\ntest\r\n0\r\n\r\n",
		&Limits, XHTTP1_ERROR_BODY_TOO_LARGE
	);
	xrtHttp1BodyLimitsInit(&Limits);
	Limits.MaxBody = 0;
	testHttp1ChunkReject(
		"1\r\na\r\n0\r\n\r\n",
		&Limits, XHTTP1_ERROR_BODY_TOO_LARGE
	);
	xrtHttp1BodyLimitsInit(&Limits);
	Limits.MaxTrailers = 1;
	testHttp1ChunkReject(
		"0\r\nA: 1\r\nB: 2\r\n\r\n",
		&Limits, XHTTP1_ERROR_TOO_MANY_TRAILERS
	);
	xrtHttp1BodyLimitsInit(&Limits);
	Limits.MaxTrailerLine = 3;
	testHttp1ChunkReject(
		"0\r\nLong: value\r\n\r\n",
		&Limits, XHTTP1_ERROR_TRAILER_LINE_TOO_LARGE
	);
}



/* 验证 Body 限额描述符支持未对齐存储，并原子拒绝回绕地址。 */
static void testHttp1BodyLimitMemoryContracts(void)
{
	uint8 Storage[sizeof(xhttp1bodylimits) + 2u];
	xhttp1bodylimits Limits;
	xhttp1bodyplan Plan = { XHTTP1_BODY_FIXED, 0 };
	xhttp1body Before;
	xhttp1body Body;

	memset(Storage, 0xA5, sizeof(Storage));
	xrtHttp1BodyLimitsInit(
		(xhttp1bodylimits*)(void*)(Storage + 1u)
	);
	memcpy(&Limits, Storage + 1u, sizeof(Limits));
	testRequire((Storage[0] == 0xA5) &&
		(Storage[sizeof(Storage) - 1u] == 0xA5) &&
		(Limits.MaxBody == UINT64_MAX) &&
		(Limits.MaxChunkLine == 8192u) &&
		(Limits.MaxTrailer == 16384u) &&
		(Limits.MaxTrailerLine == 8192u) &&
		(Limits.MaxTrailers == 100u),
		"HTTP/1 Body limits init did not support unaligned storage");
	testRequire(xrtHttp1BodyInit(
		&Body,
		&Plan,
		NULL,
		0u,
		(const xhttp1bodylimits*)(const void*)(Storage + 1u)
	) && xrtHttp1BodyDone(&Body),
		"HTTP/1 Body did not snapshot unaligned limits");

	xrtHttp1BodyLimitsInit((xhttp1bodylimits*)(uintptr_t)(
		UINTPTR_MAX - 1u
	));
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"HTTP/1 Body limits init accepted wrapping output");
	xrtClearError();
	memset(&Body, 0xA5, sizeof(Body));
	memcpy(&Before, &Body, sizeof(Before));
	testRequire(!xrtHttp1BodyInit(
		&Body,
		&Plan,
		NULL,
		0u,
		(const xhttp1bodylimits*)(uintptr_t)(UINTPTR_MAX - 1u)
	) && (memcmp(&Body, &Before, sizeof(Body)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP/1 Body accepted wrapping limits or changed state");
	xrtClearError();
}



/* 验证零数量限额可明确禁止 chunk 与 Trailer，而不是形成无效配置。 */
static void testHttp1ZeroLimits(void)
{
	xhttp1bodylimits Limits;
	xhttp1bodyplan Fixed = { XHTTP1_BODY_FIXED, 0 };
	xhttp1bodyplan Chunked = { XHTTP1_BODY_CHUNKED, 0 };
	xhttp1errorinfo Error;
	xhttp1body Body;
	xbytesview Data;
	size_t iConsumed = 0;

	memset(&Limits, 0, sizeof(Limits));
	testRequire(xrtHttp1BodyInit(
		&Body, &Fixed, NULL, 0, &Limits
	) && xrtHttp1BodyDone(&Body),
		"HTTP/1 zero optional limits rejected a fixed empty body");
	testRequire(xrtHttp1BodyInit(
		&Body, &Chunked, NULL, 0, &Limits
	), "HTTP/1 zero optional limits rejected Body init");
	testRequire((xrtHttp1BodyRead(
		&Body,
		XRT_BYTES_LITERAL("0\r\n\r\n"),
		false,
		&iConsumed,
		&Data,
		&Error
	) == XHTTP1_BODY_ERROR) &&
		(Error.Code == XHTTP1_ERROR_CHUNK_LINE_TOO_LARGE),
		"HTTP/1 zero chunk-line limit did not reject chunked input");

	xrtHttp1BodyLimitsInit(&Limits);
	Limits.MaxTrailerLine = 0;
	Limits.MaxTrailers = 0;
	testRequire(xrtHttp1BodyInit(
		&Body, &Chunked, NULL, 0, &Limits
	), "HTTP/1 zero Trailer count fixture init failed");
	testRequire((xrtHttp1BodyRead(
		&Body,
		XRT_BYTES_LITERAL("0\r\n\r\n"),
		false,
		&iConsumed,
		&Data,
		&Error
	) == XHTTP1_BODY_DONE) &&
		(iConsumed == 5) &&
		(Body.TrailerCount == 0),
		"HTTP/1 zero Trailer count rejected an empty Trailer section");
}



/* 验证独立 trailer 和单行字段 API 可以被协议上层直接复用。 */
static void testHttp1TrailerApi(void)
{
	static const unsigned char Input[] =
		"Digest: sha-256=:abc: \t\r\nX-Meta: yes\r\n\r\nNEXT";
	static const unsigned char Framing[] =
		"Content-Length: 999\r\n\r\nNEXT";
	xhttpfield Fields[2];
	xhttpfield Field;
	xhttp1bodylimits Limits;
	xhttp1errorinfo Error;
	size_t iBytes;
	size_t iCount;

	xrtHttp1BodyLimitsInit(&Limits);
	testRequire(xrtHttp1TrailersParse(
		(xbytesview){ Input, sizeof(Input) - 1u }, Fields, 2, NULL,
		&iBytes, &iCount, &Error
	) == XHTTP1_READY && (iCount == 2),
		"HTTP/1 standalone trailer default limits mismatch");
	testRequire(xrtHttp1TrailersParse(
		(xbytesview){ Input, 8 }, Fields, 2, &Limits,
		&iBytes, &iCount, &Error
	) == XHTTP1_MORE, "HTTP/1 partial trailer did not need more");
	testRequire(xrtHttp1TrailersParse(
		(xbytesview){ Input, sizeof(Input) - 1u }, Fields, 1, &Limits,
		&iBytes, &iCount, &Error
	) == XHTTP1_FIELDS && (iCount == 2),
		"HTTP/1 trailer descriptor shortage mismatch");
	testRequire(xrtHttp1TrailersParse(
		(xbytesview){ Input, sizeof(Input) - 1u }, Fields, 2, &Limits,
		&iBytes, &iCount, &Error
	) == XHTTP1_READY &&
		(iCount == 2) &&
		(iBytes == (sizeof(Input) - 1u - 4u)) &&
		testHttp1TextEqual(Fields[0].Name, "Digest") &&
		testHttp1TextEqual(Fields[0].Value, "sha-256=:abc:"),
		"HTTP/1 standalone trailer parse mismatch");
	testRequire(xrtHttpFieldParse(
		XRT_STR_LITERAL("Content-Type: application/json \t"), &Field
	) && testHttp1TextEqual(Field.Name, "Content-Type") &&
		testHttp1TextEqual(Field.Value, "application/json"),
		"HTTP field line parse or OWS trim mismatch");
	testRequire(xrtHttp1TrailersParse(
		(xbytesview){ Framing, sizeof(Framing) - 1u },
		Fields, 2, &Limits, &iBytes, &iCount, &Error
	) == XHTTP1_READY && (iCount == 1) &&
		!xrtHttpTrailerNameValid(Fields[0].Name),
		"HTTP/1 framing trailer was not kept separate for policy handling");
}



/* 验证所有通用禁止字段都不能作为 trailer 写出。 */
static void testHttp1ForbiddenTrailers(void)
{
	static const cstr sForbidden[] = {
		"age",
		"authorization",
		"cache-control",
		"connection",
		"content-encoding",
		"content-language",
		"content-length",
		"content-location",
		"content-range",
		"content-type",
		"cookie",
		"date",
		"expect",
		"expires",
		"host",
		"if-match",
		"if-modified-since",
		"if-none-match",
		"if-range",
		"if-unmodified-since",
		"keep-alive",
		"location",
		"max-forwards",
		"pragma",
		"proxy-authenticate",
		"proxy-authorization",
		"proxy-connection",
		"range",
		"retry-after",
		"set-cookie",
		"te",
		"trailer",
		"transfer-encoding",
		"upgrade",
		"vary",
		"warning",
		"www-authenticate"
	};
	size_t i;

	for ( i = 0;
		i < (sizeof(sForbidden) / sizeof(sForbidden[0]));
		i++ ) {
	testRequire(!xrtHttpTrailerNameValid((xstrview){
			sForbidden[i], strlen(sForbidden[i])
		}), "HTTP/1 forbidden trailer field was accepted");
	}
	testRequire(!xrtHttpTrailerNameValid(XRT_STR_LITERAL("Content-Type")),
		"HTTP/1 forbidden trailer comparison became case-sensitive");
	testRequire(!xrtHttpTrailerNameValid(XRT_STR_LITERAL("bad field")),
		"HTTP/1 invalid trailer field name was accepted");
	testRequire(xrtHttpTrailerNameValid(XRT_STR_LITERAL("Digest")) &&
		xrtHttpTrailerNameValid(XRT_STR_LITERAL("X-Checksum")),
		"HTTP/1 valid trailer field was rejected");
}



/* 验证 chunk-size 行保留零复制路径，并严格检查原样扩展语法和输出原子性。 */
static void testHttp1ChunkLineWriter(void)
{
	static const char sExpected[] =
		"ffffffffffffffff;foo=bar;quoted=\"a\\\"b\"\r\n";
	unsigned char Output[96];
	size_t iSize;

	testRequire(xrtHttp1ChunkLineWrite(
		UINT64_MAX,
		XRT_STR_LITERAL(";foo=bar;quoted=\"a\\\"b\""),
		NULL, 0, &iSize
	) && (iSize == (sizeof(sExpected) - 1u)),
		"HTTP/1 chunk line size query mismatch");
	memset(Output, 0xA5, sizeof(Output));
	testRequire(xrtHttp1ChunkLineWrite(
		UINT64_MAX,
		XRT_STR_LITERAL(";foo=bar;quoted=\"a\\\"b\""),
		Output, sizeof(Output), &iSize
	) && (iSize == (sizeof(sExpected) - 1u)) &&
		(memcmp(Output, sExpected, iSize) == 0),
		"HTTP/1 chunk line output mismatch");

	memset(Output, 0xA5, sizeof(Output));
	testRequire(!xrtHttp1ChunkLineWrite(
		4, XRT_STR_LITERAL(";foo=bar"), Output, 2, &iSize
	) && (iSize == 11) && (Output[0] == 0xA5),
		"HTTP/1 short chunk line output was not atomic");
	xrtClearError();
	testRequire(!xrtHttp1ChunkLineWrite(
		4, XRT_STR_LITERAL("foo=bar"), Output, sizeof(Output), &iSize
	) && (xrtErrorCode(xrtGetError()) ==
		(int32)XHTTP1_ERROR_CHUNK_EXTENSION),
		"HTTP/1 chunk extension without semicolon was accepted");
	xrtClearError();
	testRequire(!xrtHttp1ChunkLineWrite(
		4, XRT_STR_LITERAL(";foo=\"unterminated"),
		Output, sizeof(Output), &iSize
	), "HTTP/1 unterminated chunk extension was accepted");
	xrtClearError();
}



/* 验证完整 chunk、last-chunk 和 trailer 写出，并把结果交给读取端回环解析。 */
static void testHttp1ChunkWriterRoundTrip(void)
{
	xhttpfield Trailers[2] = {
		{ XRT_STR_LITERAL("Digest"), XRT_STR_LITERAL("sha-256=:abc:") },
		{ XRT_STR_LITERAL("X-Meta"), XRT_STR_LITERAL("yes") }
	};
	xhttpfield Parsed[2];
	xhttp1bodylimits Limits;
	xhttp1errorinfo Error;
	xhttp1bodyplan Plan = { XHTTP1_BODY_CHUNKED, 0 };
	xhttp1body Body;
	unsigned char Wire[160];
	unsigned char Decoded[32];
	size_t iFirst;
	size_t iSecond;
	size_t iEnd;
	size_t iDecoded;
	size_t iConsumed;
	xhttp1bodystatus Status;

	testRequire(xrtHttp1ChunkWrite(
		(xbytesview){ (cbytes)"Wiki", 4 },
		Wire, sizeof(Wire), &iFirst
	) && (iFirst == 9) &&
		(memcmp(Wire, "4\r\nWiki\r\n", 9) == 0),
		"HTTP/1 first complete chunk output mismatch");
	testRequire(xrtHttp1ChunkWrite(
		(xbytesview){ (cbytes)"pedia", 5 },
		Wire + iFirst, sizeof(Wire) - iFirst, &iSecond
	) && (iSecond == 10),
		"HTTP/1 second complete chunk output mismatch");
	testRequire(xrtHttp1ChunkEndWrite(
		Trailers, 2, Wire + iFirst + iSecond,
		sizeof(Wire) - iFirst - iSecond, &iEnd
	), "HTTP/1 last chunk output failed");

	xrtHttp1BodyLimitsInit(&Limits);
	testRequire(xrtHttp1BodyInit(
		&Body, &Plan, Parsed, 2, &Limits
	), "HTTP/1 chunk writer round-trip init failed");
	Status = testHttp1BodyRun(
		&Body, Wire, iFirst + iSecond + iEnd, 2, false,
		Decoded, sizeof(Decoded), &iDecoded, &iConsumed, &Error
	);
	testRequire((Status == XHTTP1_BODY_DONE) &&
		(iDecoded == 9) && (memcmp(Decoded, "Wikipedia", 9) == 0) &&
		(iConsumed == (iFirst + iSecond + iEnd)) &&
		(Body.TrailerCount == 2) &&
		testHttp1TextEqual(Parsed[0].Name, "Digest"),
		"HTTP/1 chunk writer round-trip mismatch");

	memset(Wire, 0xA5, sizeof(Wire));
	testRequire(xrtHttp1ChunkWrite(
		(xbytesview){ NULL, 0 }, Wire, sizeof(Wire), &iFirst
	) && (iFirst == 0) && (Wire[0] == 0xA5),
		"HTTP/1 empty chunk write terminated the message");
	testRequire(xrtHttpTrailerNameValid(XRT_STR_LITERAL("Digest")) &&
		!xrtHttpTrailerNameValid(XRT_STR_LITERAL("Content-Length")) &&
		!xrtHttpTrailerNameValid(XRT_STR_LITERAL("Transfer-Encoding")),
		"HTTP/1 trailer field policy mismatch");
	Trailers[0].Name = XRT_STR_LITERAL("Content-Length");
	testRequire(!xrtHttp1ChunkEndWrite(
		Trailers, 1, Wire, sizeof(Wire), &iEnd
	) && (xrtErrorCode(xrtGetError()) ==
		(int32)XHTTP1_ERROR_FORBIDDEN_TRAILER) &&
		(Wire[0] == 0xA5),
		"HTTP/1 forbidden trailer was generated");
	xrtClearError();
}



/* 执行 HTTP/1 Body Plan、流式解码和 trailer 边界回归。 */
int main(void)
{
	testHttp1BodyPlans();
	testHttp1FixedBody();
	testHttp1CloseBody();
	testHttp1ChunkedBody();
	testHttp1TrailerRebind();
	testHttp1ChunkRejects();
	testHttp1ChunkErrorConsumed();
	testHttp1BodyLimitMemoryContracts();
	testHttp1ZeroLimits();
	testHttp1TrailerApi();
	testHttp1ForbiddenTrailers();
	testHttp1ChunkLineWriter();
	testHttp1ChunkWriterRoundTrip();
	printf("[PASS] http1_body\n");
	return 0;
}
