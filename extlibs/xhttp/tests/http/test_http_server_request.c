#include "../test.h"
#include "../../src/internal/xrt_http_server.h"



/* 比较借用文本视图与零结尾常量。 */
static bool testHttpServerRequestTextEqual(
	xstrview Text,
	cstr sExpected
)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		(memcmp(Text.Data, sExpected, iSize) == 0);
}



/* 从完整请求 Header 创建一个拥有型请求快照。 */
static xhttpserverrequest* testHttpServerRequestCreate(
	char* pInput,
	size_t iSize,
	xhttpfield* pFields,
	size_t iCapacity,
	uint32 iFlags
)
{
	xhttp1bodyplan Plan;
	xhttp1head Head;

	xrtHttp1HeadInit(&Head, pFields, iCapacity);
	testRequire(xrtHttp1RequestParse(
		(xbytesview){ (cbytes)pInput, iSize },
		&Head,
		NULL,
		NULL
	) == XHTTP1_READY,
		"HTTP server request fixture parse failed");
	testRequire(xrtHttp1RequestBodyPlan(&Head, &Plan),
		"HTTP server request body plan failed");
	return __xrtHttpServerRequestCreate(
		&Head, &Plan, iFlags
	);
}



/* 验证请求行与 Header 使用实际长度的稳定拥有快照。 */
static void testHttpServerRequestHead(void)
{
	char Input[] =
		"POST /items?q=1 HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Content-Length: 3\r\n"
		"X-Test: value\r\n"
		"\r\n";
	xhttpfield Fields[8];
	xhttpserverrequest* pRequest;
	const xhttpfield* pHost;
	xhttptarget Target;
	xhttpauthority Authority;
	uint8 TargetStorage[sizeof(xhttptarget) + 2u];
	uint8 AuthorityStorage[sizeof(xhttpauthority) + 2u];
	xhttptarget* pUnalignedTarget =
		(xhttptarget*)(void*)(TargetStorage + 1u);
	xhttpauthority* pUnalignedAuthority =
		(xhttpauthority*)(void*)(AuthorityStorage + 1u);

	pRequest = testHttpServerRequestCreate(
		Input,
		sizeof(Input) - 1u,
		Fields,
		sizeof(Fields) / sizeof(Fields[0]),
		XHTTP_SERVER_REQUEST_KEEP_ALIVE
	);
	testRequire(pRequest != NULL,
		"HTTP server request create failed");
	memset(Input, 'x', sizeof(Input) - 1u);
	pHost = xrtHttpServerRequestHeader(
		pRequest, XRT_STR_LITERAL("host")
	);
	testRequire(
		(xrtHttpServerRequestVersion(pRequest) ==
		 XHTTP_VERSION_1_1) &&
		testHttpServerRequestTextEqual(
			xrtHttpServerRequestMethod(pRequest), "POST"
		) && testHttpServerRequestTextEqual(
			xrtHttpServerRequestTarget(pRequest), "/items?q=1"
		) &&
		((xrtHttpServerRequestFlags(pRequest) &
		  XHTTP_SERVER_REQUEST_KEEP_ALIVE) != 0) &&
		(xrtHttpServerRequestBodyMode(pRequest) ==
		 XHTTP1_BODY_FIXED) &&
		(xrtHttpServerRequestContentLength(pRequest) == 3) &&
		(xrtHttpServerRequestHeaderCount(pRequest) == 3) &&
		(pHost != NULL) &&
		testHttpServerRequestTextEqual(
			pHost->Value, "example.test"
		) &&
		xrtHttpServerRequestParseTarget(
			pRequest, &Target
		) &&
		(Target.Form == XHTTP_TARGET_ORIGIN) &&
		testHttpServerRequestTextEqual(
			Target.Path, "/items"
		) &&
		xrtHttpServerRequestAuthority(
			pRequest, &Authority
		) &&
		testHttpServerRequestTextEqual(
			Authority.Host, "example.test"
		) &&
		(xrtHttpServerRequestHeaderData(pRequest) != NULL) &&
		(xrtHttpServerRequestHeaderAt(pRequest, 2) ==
		 &xrtHttpServerRequestHeaderData(pRequest)[2]),
		"HTTP server request owned metadata mismatch"
	);
	memset(TargetStorage, 0xA5, sizeof(TargetStorage));
	memset(AuthorityStorage, 0xA5, sizeof(AuthorityStorage));
	testRequire(
		xrtHttpServerRequestParseTarget(
			pRequest, pUnalignedTarget
		) && xrtHttpServerRequestAuthority(
			pRequest, pUnalignedAuthority
		),
		"HTTP server request rejected unaligned metadata outputs"
	);
	memcpy(&Target, pUnalignedTarget, sizeof(Target));
	memcpy(&Authority, pUnalignedAuthority, sizeof(Authority));
	testRequire(
		(Target.Form == XHTTP_TARGET_ORIGIN) &&
		testHttpServerRequestTextEqual(
			Target.Path, "/items"
		) && testHttpServerRequestTextEqual(
			Authority.Host, "example.test"
		) && (TargetStorage[0] == UINT8_C(0xA5)) &&
		(TargetStorage[sizeof(TargetStorage) - 1u] ==
		 UINT8_C(0xA5)) &&
		(AuthorityStorage[0] == UINT8_C(0xA5)) &&
		(AuthorityStorage[sizeof(AuthorityStorage) - 1u] ==
		 UINT8_C(0xA5)),
		"HTTP server request unaligned metadata output mismatch"
	);
	testRequire(!xrtHttpServerRequestParseTarget(
		pRequest,
		(xhttptarget*)(void*)pRequest->Fields
	), "HTTP server request target output overwrote request fields");
	xrtClearError();
	testRequire(xrtHttpServerRequestRef(pRequest) == pRequest,
		"HTTP server request retain failed");
	xrtHttpServerRequestDestroy(pRequest);
	xrtHttpServerRequestDestroy(pRequest);
}



/* 验证正文和 Trailer 只在实际出现时分配并独立拥有。 */
static void testHttpServerRequestBufferedBody(void)
{
	char Input[] =
		"POST /upload HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Transfer-Encoding: chunked\r\n"
		"\r\n";
	char TrailerName[] = "Digest";
	char TrailerValue[] = "sha-256=:AA==:";
	xhttpfield Fields[8];
	xhttpfield Trailers[] = {
		{
			{ TrailerName, sizeof(TrailerName) - 1u },
			{ TrailerValue, sizeof(TrailerValue) - 1u }
		}
	};
	xhttpserverrequest* pRequest;
	xbytesview Body;
	const xhttpfield* pTrailer;

	pRequest = testHttpServerRequestCreate(
		Input,
		sizeof(Input) - 1u,
		Fields,
		sizeof(Fields) / sizeof(Fields[0]),
		XHTTP_SERVER_REQUEST_KEEP_ALIVE
	);
	testRequire((pRequest != NULL) &&
		(xrtHttpServerRequestBody(pRequest).Data == NULL) &&
		__xrtHttpServerRequestAppendBody(
			pRequest,
			(xbytesview){ (cbytes)"abc", 3 }
		) && __xrtHttpServerRequestAppendBody(
			pRequest,
			(xbytesview){ (cbytes)"def", 3 }
		) && __xrtHttpServerRequestSetTrailers(
			pRequest, Trailers, 1
		), "HTTP server request buffered data setup failed");
	memset(TrailerName, 'x', sizeof(TrailerName) - 1u);
	memset(TrailerValue, 'x', sizeof(TrailerValue) - 1u);
	__xrtHttpServerRequestSetFlags(
		pRequest, XHTTP_SERVER_REQUEST_COMPLETE
	);
	Body = xrtHttpServerRequestBody(pRequest);
	pTrailer = xrtHttpServerRequestTrailer(
		pRequest, XRT_STR_LITERAL("digest")
	);
	testRequire(
		(Body.Size == 6) &&
		(memcmp(Body.Data, "abcdef", 6) == 0) &&
		(xrtHttpServerRequestBodyBytes(pRequest) == 6) &&
		(xrtHttpServerRequestTrailerCount(pRequest) == 1) &&
		(pTrailer != NULL) &&
		testHttpServerRequestTextEqual(
			pTrailer->Value, "sha-256=:AA==:"
		) &&
		(xrtHttpServerRequestTrailerAt(pRequest, 0) == pTrailer) &&
		(xrtHttpServerRequestTrailerData(pRequest) == pTrailer) &&
		((xrtHttpServerRequestFlags(pRequest) &
		  XHTTP_SERVER_REQUEST_COMPLETE) != 0),
		"HTTP server request buffered data mismatch"
	);
	xrtHttpServerRequestDestroy(pRequest);
}



/* 验证流式请求只累计字节而不建立正文副本。 */
static void testHttpServerRequestStreamedBody(void)
{
	char Input[] =
		"POST /stream HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Content-Length: 12\r\n"
		"\r\n";
	xhttpfield Fields[4];
	xhttpserverrequest* pRequest;

	pRequest = testHttpServerRequestCreate(
		Input,
		sizeof(Input) - 1u,
		Fields,
		sizeof(Fields) / sizeof(Fields[0]),
		XHTTP_SERVER_REQUEST_STREAMED
	);
	testRequire((pRequest != NULL) &&
		__xrtHttpServerRequestDeliverBody(pRequest, 5) &&
		__xrtHttpServerRequestDeliverBody(pRequest, 7),
		"HTTP server streamed request delivery failed");
	testRequire(
		(xrtHttpServerRequestBodyBytes(pRequest) == 12) &&
		(xrtHttpServerRequestBody(pRequest).Data == NULL) &&
		(xrtHttpServerRequestBody(pRequest).Size == 0),
		"HTTP server streamed request allocated a body copy"
	);
	testRequire(!__xrtHttpServerRequestAppendBody(
		pRequest,
		(xbytesview){ (cbytes)"bad", 3 }
	), "HTTP server streamed request accepted buffered append");
	xrtClearError();
	xrtHttpServerRequestDestroy(pRequest);
}



/* 验证内部计数溢出与空查询契约。 */
static void testHttpServerRequestContracts(void)
{
	char Input[] =
		"GET / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"\r\n";
	xhttpfield InvalidField = {
		XRT_STR_LITERAL("X-Invalid"),
		{ NULL, 1 }
	};
	xhttpfield Trailer = {
		XRT_STR_LITERAL("Digest"),
		XRT_STR_LITERAL("sha-256=:AA==:")
	};
	xhttpfield InvalidTrailer = {
		{ NULL, 1 },
		XRT_STR_LITERAL("value")
	};
	xhttpfield Fields[2];
	xhttp1bodyplan Plan = { 0 };
	xhttp1head Invalid = { 0 };
	xhttpserverrequest* pRequest;
	const xhttpfield* pTrailer;

	Invalid.Kind = XHTTP_REQUEST;
	Invalid.Method = (xstrview){ NULL, 1 };
	Invalid.Target = XRT_STR_LITERAL("/");
	testRequire(
		(__xrtHttpServerRequestCreate(
			&Invalid,
			&Plan,
			XHTTP_SERVER_REQUEST_NONE
		 ) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP server request accepted invalid method view"
	);
	xrtClearError();

	Invalid.Method = XRT_STR_LITERAL("GET");
	Invalid.Fields = &InvalidField;
	Invalid.FieldCount = 1;
	testRequire(
		(__xrtHttpServerRequestCreate(
			&Invalid,
			&Plan,
			XHTTP_SERVER_REQUEST_NONE
		 ) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP server request accepted invalid Header view"
	);
	xrtClearError();

	pRequest = testHttpServerRequestCreate(
		Input,
		sizeof(Input) - 1u,
		Fields,
		sizeof(Fields) / sizeof(Fields[0]),
		XHTTP_SERVER_REQUEST_STREAMED
	);
	testRequire(pRequest != NULL,
		"HTTP server request contract fixture failed");
	testRequire(
		__xrtHttpServerRequestSetTrailers(
			pRequest,
			&Trailer,
			1
		),
		"HTTP server request Trailer fixture failed"
	);
	pTrailer = xrtHttpServerRequestTrailerAt(pRequest, 0);
	testRequire(
		!__xrtHttpServerRequestSetTrailers(
			pRequest,
			&InvalidTrailer,
			1
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtHttpServerRequestTrailerCount(pRequest) == 1) &&
		(xrtHttpServerRequestTrailerAt(pRequest, 0) == pTrailer) &&
		testHttpServerRequestTextEqual(
			pTrailer->Value,
			"sha-256=:AA==:"
		),
		"HTTP server request invalid Trailer changed snapshot"
	);
	xrtClearError();
	pRequest->BodyBytes = UINT64_MAX;
	testRequire(!__xrtHttpServerRequestDeliverBody(
		pRequest, 1
	), "HTTP server request body count overflow succeeded");
	xrtClearError();
	xrtHttpServerRequestDestroy(pRequest);

	testRequire(
		(xrtHttpServerRequestVersion(NULL) == 0) &&
		(xrtHttpServerRequestHeaderCount(NULL) == 0) &&
		(xrtHttpServerRequestHeaderData(NULL) == NULL) &&
		(xrtHttpServerRequestTrailerCount(NULL) == 0) &&
		(xrtHttpServerRequestTrailerData(NULL) == NULL) &&
		(xrtHttpServerRequestBodyBytes(NULL) == 0) &&
		!xrtHttpServerRequestAcceptsTrailers(NULL) &&
		!xrtHttpServerRequestParseTarget(NULL, NULL) &&
		!xrtHttpServerRequestAuthority(NULL, NULL),
		"HTTP server request null query returned non-empty state"
	);
	xrtClearError();
	xrtHttpServerRequestDestroy(NULL);
}



/* 运行拥有型服务端请求快照测试。 */
int main(void)
{
	testHttpServerRequestHead();
	testHttpServerRequestBufferedBody();
	testHttpServerRequestStreamedBody();
	testHttpServerRequestContracts();
	printf("[PASS] HTTP server request\n");
	return 0;
}
