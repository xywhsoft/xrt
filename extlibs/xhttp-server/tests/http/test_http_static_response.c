#include "../test.h"



/* 创建带弱 ETag 和固定 Last-Modified 的当前表示。 */
static xhttprepresentation testHttpStaticRepresentation(void)
{
	xhttprepresentation Current;

	memset(&Current, 0, sizeof(Current));
	Current.Exists = true;
	Current.HasETag = true;
	Current.ETag.Weak = true;
	Current.ETag.Opaque = XRT_STR_LITERAL("asset-17");
	Current.HasLastModified = true;
	testRequire(xrtTimeParseHTTPDate(
		XRT_STR_LITERAL("Sun, 06 Nov 1994 08:49:37 GMT"),
		&Current.LastModified
	), "HTTP static response date parse failed");
	return Current;
}



/* 返回响应中的首个同名字段。 */
static const xhttpfield* testHttpStaticResponseField(
	const xhttpstaticresponse* pResponse,
	xstrview Name
)
{
	size_t i;

	for ( i = 0; i < pResponse->FieldCount; i++ ) {
		if ( xrtHttpFieldNameEqual(
			pResponse->Fields[i].Name,
			Name
		) ) {
			return &pResponse->Fields[i];
		}
	}
	return NULL;
}



/* 比较一个响应字段的完整值。 */
static bool testHttpStaticResponseValue(
	const xhttpstaticresponse* pResponse,
	xstrview Name,
	cstr sExpected
)
{
	const xhttpfield* pField =
		testHttpStaticResponseField(pResponse, Name);
	size_t iSize = strlen(sExpected);

	return (pField != NULL) &&
		(pField->Value.Size == iSize) &&
		(memcmp(pField->Value.Data, sExpected, iSize) == 0);
}



/* 使用精确工作区构建一个静态响应。 */
static xhttpstaticresponse testHttpStaticResponseBuild(
	const xhttpstaticplan* pPlan,
	const xhttpbyterange* pRanges,
	const xhttprepresentation* pCurrent,
	const xhttpstaticresponseconfig* pConfig,
	char* sWorkspace,
	size_t iCapacity,
	size_t* pUsed
)
{
	xhttpstaticresponse Response;
	size_t iRequired;

	testRequire(xrtHttpStaticResponseBuild(
		pPlan,
		pRanges,
		pCurrent,
		pConfig,
		NULL,
		0,
		&iRequired,
		NULL
	), "HTTP static response measure failed");
	testRequire(iRequired <= iCapacity,
		"HTTP static response test workspace too small");
	testRequire(xrtHttpStaticResponseBuild(
		pPlan,
		pRanges,
		pCurrent,
		pConfig,
		sWorkspace,
		iCapacity,
		pUsed,
		&Response
	) && (*pUsed == iRequired),
		"HTTP static response build failed");
	return Response;
}



/* 验证完整 GET 与 HEAD 的共享元数据和不同正文语义。 */
static void testHttpStaticResponseFull(void)
{
	xhttprepresentation Current =
		testHttpStaticRepresentation();
	xhttpstaticresponseconfig Config;
	xhttpstaticplan Plan;
	xhttpbyterange Unused[1];
	xhttpstaticresponse Response;
	char Workspace[256];
	size_t iUsed;

	xrtHttpStaticResponseConfigInit(&Config);
	Config.ContentType =
		XRT_STR_LITERAL("text/plain; charset=utf-8");
	Config.CacheControl =
		XRT_STR_LITERAL("public, max-age=60");
	memset(&Plan, 0, sizeof(Plan));
	Plan.Status = XHTTP_STATUS_OK;
	Plan.SendBody = true;
	Plan.AcceptRanges = true;
	Plan.CompleteLength = 123;
	Plan.SelectedLength = 123;

	Response = testHttpStaticResponseBuild(
		&Plan,
		Unused,
		&Current,
		&Config,
		Workspace,
		sizeof(Workspace),
		&iUsed
	);
	testRequire((Response.Status == XHTTP_STATUS_OK) &&
		Response.SendBody && !Response.Multipart &&
		(Response.BodyLength == 123) &&
		(Response.FieldCount == 6),
		"HTTP static full response metadata mismatch");
	testRequire(testHttpStaticResponseValue(
		&Response,
		XRT_STR_LITERAL("Content-Type"),
		"text/plain; charset=utf-8"
	) && testHttpStaticResponseValue(
		&Response,
		XRT_STR_LITERAL("Content-Length"),
		"123"
	) && testHttpStaticResponseValue(
		&Response,
		XRT_STR_LITERAL("Accept-Ranges"),
		"bytes"
	) && testHttpStaticResponseValue(
		&Response,
		XRT_STR_LITERAL("ETag"),
		"W/\"asset-17\""
	) && testHttpStaticResponseValue(
		&Response,
		XRT_STR_LITERAL("Last-Modified"),
		"Sun, 06 Nov 1994 08:49:37 GMT"
	) && testHttpStaticResponseValue(
		&Response,
		XRT_STR_LITERAL("Cache-Control"),
		"public, max-age=60"
	), "HTTP static full response fields mismatch");

	Plan.SendBody = false;
	Response = testHttpStaticResponseBuild(
		&Plan,
		Unused,
		&Current,
		&Config,
		Workspace,
		sizeof(Workspace),
		&iUsed
	);
	testRequire(!Response.SendBody &&
		(Response.BodyLength == 123) &&
		testHttpStaticResponseValue(
			&Response,
			XRT_STR_LITERAL("Content-Length"),
			"123"
		), "HTTP static HEAD metadata mismatch");
}



/* 验证单范围和 multipart 多范围的顶层字段差异。 */
static void testHttpStaticResponseRanges(void)
{
	xhttprepresentation Current =
		testHttpStaticRepresentation();
	xhttpstaticresponseconfig Config;
	xhttpstaticplan Plan;
	xhttpbyterange Ranges[2] = {
		{ 10, 19 },
		{ 30, 39 }
	};
	xhttpstaticresponse Response;
	char Workspace[256];
	size_t iUsed;

	xrtHttpStaticResponseConfigInit(&Config);
	Config.ContentType = XRT_STR_LITERAL("image/png");
	memset(&Plan, 0, sizeof(Plan));
	Plan.Status = XHTTP_STATUS_PARTIAL_CONTENT;
	Plan.SendBody = true;
	Plan.AcceptRanges = true;
	Plan.CompleteLength = 100;
	Plan.SelectedLength = 10;
	Plan.RangeCount = 1;

	Response = testHttpStaticResponseBuild(
		&Plan,
		Ranges,
		&Current,
		&Config,
		Workspace,
		sizeof(Workspace),
		&iUsed
	);
	testRequire((Response.FieldCount == 6) &&
		(Response.BodyLength == 10) &&
		testHttpStaticResponseValue(
			&Response,
			XRT_STR_LITERAL("Content-Range"),
			"bytes 10-19/100"
		) && testHttpStaticResponseValue(
			&Response,
			XRT_STR_LITERAL("Content-Length"),
			"10"
		), "HTTP static single range fields mismatch");

	Plan.SelectedLength = 20;
	Plan.RangeCount = 2;
	Config.Boundary = XRT_STR_LITERAL("xrt-range-17");
	Response = testHttpStaticResponseBuild(
		&Plan,
		Ranges,
		&Current,
		&Config,
		Workspace,
		sizeof(Workspace),
		&iUsed
	);
	testRequire(Response.Multipart &&
		(Response.BodyLength == 192) &&
		(Response.Boundary.Size == 12) &&
		(testHttpStaticResponseField(
			&Response,
			XRT_STR_LITERAL("Content-Range")
		) == NULL) &&
		testHttpStaticResponseValue(
			&Response,
			XRT_STR_LITERAL("Content-Type"),
			"multipart/byteranges; boundary=xrt-range-17"
		) && testHttpStaticResponseValue(
			&Response,
			XRT_STR_LITERAL("Content-Length"),
			"192"
		), "HTTP static multipart fields mismatch");
}



/* 验证无正文状态的字段集合。 */
static void testHttpStaticResponseStatuses(void)
{
	xhttprepresentation Current =
		testHttpStaticRepresentation();
	xhttpstaticresponseconfig Config;
	xhttpstaticplan Plan;
	xhttpstaticresponse Response;
	char Workspace[256];
	size_t iUsed;

	xrtHttpStaticResponseConfigInit(&Config);
	Config.CacheControl = XRT_STR_LITERAL("max-age=0");
	memset(&Plan, 0, sizeof(Plan));
	Plan.CompleteLength = 100;
	Plan.AcceptRanges = true;

	Plan.Status = XHTTP_STATUS_NOT_MODIFIED;
	Response = testHttpStaticResponseBuild(
		&Plan, NULL, &Current, &Config,
		Workspace, sizeof(Workspace), &iUsed
	);
	testRequire((Response.FieldCount == 4) &&
		(testHttpStaticResponseField(
			&Response,
			XRT_STR_LITERAL("Content-Length")
		) == NULL) &&
		(testHttpStaticResponseField(
			&Response,
			XRT_STR_LITERAL("Content-Type")
		) == NULL), "HTTP static 304 fields mismatch");

	Plan.Status = XHTTP_STATUS_METHOD_NOT_ALLOWED;
	Plan.AcceptRanges = false;
	Response = testHttpStaticResponseBuild(
		&Plan, NULL, &Current, &Config,
		Workspace, sizeof(Workspace), &iUsed
	);
	testRequire((Response.FieldCount == 2) &&
		testHttpStaticResponseValue(
			&Response,
			XRT_STR_LITERAL("Content-Length"),
			"0"
		) && testHttpStaticResponseValue(
			&Response,
			XRT_STR_LITERAL("Allow"),
			"GET, HEAD"
		), "HTTP static 405 fields mismatch");

	Plan.Status = XHTTP_STATUS_PRECONDITION_FAILED;
	Plan.AcceptRanges = true;
	Response = testHttpStaticResponseBuild(
		&Plan, NULL, &Current, &Config,
		Workspace, sizeof(Workspace), &iUsed
	);
	testRequire((Response.FieldCount == 5) &&
		testHttpStaticResponseValue(
			&Response,
			XRT_STR_LITERAL("Content-Length"),
			"0"
		), "HTTP static 412 fields mismatch");

	Plan.Status = XHTTP_STATUS_RANGE_NOT_SATISFIABLE;
	Response = testHttpStaticResponseBuild(
		&Plan, NULL, &Current, &Config,
		Workspace, sizeof(Workspace), &iUsed
	);
	testRequire((Response.FieldCount == 6) &&
		testHttpStaticResponseValue(
			&Response,
			XRT_STR_LITERAL("Content-Range"),
			"bytes */100"
		) && testHttpStaticResponseValue(
			&Response,
			XRT_STR_LITERAL("Content-Length"),
			"0"
		), "HTTP static 416 fields mismatch");
}



/* 验证长验证器、容量失败原子性和输出别名拒绝。 */
static void testHttpStaticResponseEdges(void)
{
	static const char sLongTag[] =
		"0123456789abcdef0123456789abcdef"
		"0123456789abcdef0123456789abcdef"
		"0123456789abcdef0123456789abcdef"
		"0123456789abcdef0123456789abcdef";
	xhttprepresentation Current =
		testHttpStaticRepresentation();
	xhttpstaticresponseconfig Config;
	xhttpstaticplan Plan;
	xhttpstaticresponse Response;
	xhttpstaticresponse Before;
	char Workspace[512];
	size_t iRequired;
	size_t iSize;

	Current.ETag.Opaque = (xstrview){
		sLongTag,
		sizeof(sLongTag) - 1u
	};
	xrtHttpStaticResponseConfigInit(&Config);
	memset(&Plan, 0, sizeof(Plan));
	Plan.Status = XHTTP_STATUS_OK;
	Plan.SendBody = true;
	Plan.CompleteLength = UINT64_MAX;
	Plan.SelectedLength = UINT64_MAX;

	testRequire(xrtHttpStaticResponseBuild(
		&Plan, NULL, &Current, &Config,
		NULL, 0, &iRequired, NULL
	) && (iRequired > sizeof(sLongTag)),
		"HTTP static long validator measure failed");
	memset(&Response, 0xA5, sizeof(Response));
	Before = Response;
	iSize = 0;
	testRequire(!xrtHttpStaticResponseBuild(
		&Plan, NULL, &Current, &Config,
		Workspace, iRequired - 1u, &iSize, &Response
	) && (iSize == iRequired) &&
		(memcmp(&Response, &Before, sizeof(Response)) == 0),
		"HTTP static response capacity failure was not atomic");
	xrtClearError();
	testRequire(xrtHttpStaticResponseBuild(
		&Plan, NULL, &Current, &Config,
		Workspace, sizeof(Workspace), &iSize, &Response
	) && testHttpStaticResponseValue(
		&Response,
		XRT_STR_LITERAL("Content-Length"),
		"18446744073709551615"
	), "HTTP static response uint64 length mismatch");

	iSize = 0;
	testRequire(!xrtHttpStaticResponseBuild(
		&Plan, NULL, &Current, &Config,
		&Response, sizeof(Response), &iSize, &Response
	), "HTTP static response accepted aliased workspace");
	xrtClearError();
}



/* 执行静态响应字段构建测试。 */
/* 验证静态响应的全部固定输入输出支持未对齐存储并拒绝回绕区间。 */
static void testHttpStaticResponseMemoryContract(void)
{
	uint8 PlanStorage[sizeof(xhttpstaticplan) + 2u];
	uint8 RangeStorage[(sizeof(xhttpbyterange) * 2u) + 2u];
	uint8 CurrentStorage[sizeof(xhttprepresentation) + 2u];
	uint8 ConfigStorage[sizeof(xhttpstaticresponseconfig) + 2u];
	uint8 SizeStorage[sizeof(size_t) + 2u];
	uint8 ResponseStorage[sizeof(xhttpstaticresponse) + 2u];
	xhttpstaticresponseconfig Config;
	xhttprepresentation Current;
	xhttpstaticresponse Response;
	xhttpstaticplan Plan;
	xhttpbyterange Ranges[2] = {
		{ 0u, 1u },
		{ 4u, 5u }
	};
	size_t iRequired;
	char Workspace[256];

	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	xrtHttpStaticResponseConfigInit(
		(xhttpstaticresponseconfig*)(ConfigStorage + 1u)
	);
	memcpy(&Config, ConfigStorage + 1u, sizeof(Config));
	Config.ContentType = XRT_STR_LITERAL("text/plain");
	Config.Boundary = XRT_STR_LITERAL("xrt-test");
	memcpy(ConfigStorage + 1u, &Config, sizeof(Config));
	testRequire((ConfigStorage[0] == 0xA5u) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] == 0xA5u),
		"HTTP static response unaligned config initialization failed");

	memset(&Plan, 0, sizeof(Plan));
	Plan.Status = XHTTP_STATUS_PARTIAL_CONTENT;
	Plan.SendBody = true;
	Plan.AcceptRanges = true;
	Plan.CompleteLength = 10u;
	Plan.SelectedLength = 4u;
	Plan.RangeCount = 2u;
	memset(&Current, 0, sizeof(Current));
	Current.Exists = true;
	memcpy(PlanStorage + 1u, &Plan, sizeof(Plan));
	memcpy(RangeStorage + 1u, Ranges, sizeof(Ranges));
	memcpy(CurrentStorage + 1u, &Current, sizeof(Current));
	memset(SizeStorage, 0xB6, sizeof(SizeStorage));
	testRequire(xrtHttpStaticResponseBuild(
		(const xhttpstaticplan*)(PlanStorage + 1u),
		(const xhttpbyterange*)(RangeStorage + 1u),
		(const xhttprepresentation*)(CurrentStorage + 1u),
		(const xhttpstaticresponseconfig*)(ConfigStorage + 1u),
		NULL,
		0,
		(size_t*)(SizeStorage + 1u),
		NULL
	), "HTTP static response rejected unaligned measure inputs");
	memcpy(&iRequired, SizeStorage + 1u, sizeof(iRequired));
	testRequire((iRequired != 0) &&
		(iRequired <= sizeof(Workspace)),
		"HTTP static response unaligned measure mismatch");

	memset(ResponseStorage, 0xC7, sizeof(ResponseStorage));
	testRequire(xrtHttpStaticResponseBuild(
		(const xhttpstaticplan*)(PlanStorage + 1u),
		(const xhttpbyterange*)(RangeStorage + 1u),
		(const xhttprepresentation*)(CurrentStorage + 1u),
		(const xhttpstaticresponseconfig*)(ConfigStorage + 1u),
		Workspace,
		sizeof(Workspace),
		(size_t*)(SizeStorage + 1u),
		(xhttpstaticresponse*)(ResponseStorage + 1u)
	), "HTTP static response rejected unaligned fixed storage");
	memcpy(&Response, ResponseStorage + 1u, sizeof(Response));
	testRequire((Response.Status == XHTTP_STATUS_PARTIAL_CONTENT) &&
		Response.SendBody && Response.Multipart &&
		(Response.BodyLength > Plan.SelectedLength) &&
		(Response.Boundary.Size == Config.Boundary.Size) &&
		(ResponseStorage[0] == 0xC7u) &&
		(ResponseStorage[sizeof(ResponseStorage) - 1u] == 0xC7u) &&
		(SizeStorage[0] == 0xB6u) &&
		(SizeStorage[sizeof(SizeStorage) - 1u] == 0xB6u),
		"HTTP static response unaligned output mismatch");

	xrtClearError();
	xrtHttpStaticResponseConfigInit(
		(xhttpstaticresponseconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(xrtGetError() != NULL,
		"HTTP static response accepted a wrapping config output");
	xrtClearError();
	testRequire(!xrtHttpStaticResponseBuild(
		(const xhttpstaticplan*)(uintptr_t)(UINTPTR_MAX - 1u),
		Ranges,
		&Current,
		&Config,
		NULL,
		0,
		&iRequired,
		NULL
	), "HTTP static response accepted a wrapping plan input");
	xrtClearError();
	testRequire(!xrtHttpStaticResponseBuild(
		&Plan,
		(const xhttpbyterange*)(uintptr_t)(UINTPTR_MAX - 1u),
		&Current,
		&Config,
		NULL,
		0,
		&iRequired,
		NULL
	), "HTTP static response accepted a wrapping range input");
	xrtClearError();
	testRequire(!xrtHttpStaticResponseBuild(
		&Plan,
		Ranges,
		(const xhttprepresentation*)(uintptr_t)(UINTPTR_MAX - 1u),
		&Config,
		NULL,
		0,
		&iRequired,
		NULL
	), "HTTP static response accepted a wrapping representation");
	xrtClearError();
	testRequire(!xrtHttpStaticResponseBuild(
		&Plan,
		Ranges,
		&Current,
		(const xhttpstaticresponseconfig*)(uintptr_t)(UINTPTR_MAX - 1u),
		NULL,
		0,
		&iRequired,
		NULL
	), "HTTP static response accepted a wrapping config input");
	xrtClearError();
	testRequire(!xrtHttpStaticResponseBuild(
		&Plan,
		Ranges,
		&Current,
		&Config,
		Workspace,
		sizeof(Workspace),
		&iRequired,
		(xhttpstaticresponse*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP static response accepted a wrapping response output");
	xrtClearError();
}



int main(void)
{
	testHttpStaticResponseFull();
	testHttpStaticResponseRanges();
	testHttpStaticResponseStatuses();
	testHttpStaticResponseEdges();
	testHttpStaticResponseMemoryContract();
	printf("[PASS] http_static_response\n");
	return 0;
}
