#include "../test.h"



/* 构造带强实体标签和强修改时间的当前表示。 */
static xhttprepresentation testHttpRepresentation(
	xtime iModified
)
{
	xhttprepresentation Current;

	memset(&Current, 0, sizeof(Current));
	Current.Exists = true;
	Current.HasETag = true;
	Current.HasLastModified = true;
	Current.LastModifiedStrong = true;
	Current.ETag.Opaque = XRT_STR_LITERAL("v1");
	Current.ETag.Weak = false;
	Current.LastModified = iModified;
	return Current;
}



/* 解析测试使用的规范 HTTP 日期。 */
static xtime testHttpDate(cstr sText)
{
	xstrview Text = { sText, strlen(sText) };
	xtime iTime = 0;

	testRequire(xrtTimeParseHTTPDate(Text, &iTime),
		"HTTP precondition date fixture failed");
	return iTime;
}



/* 验证 If-Match 和 If-Unmodified-Since 的优先级与强比较。 */
static void testHttpIfMatch(void)
{
	const xtime iBase = testHttpDate(
		"Sun, 06 Nov 1994 08:49:37 GMT"
	);
	xhttprepresentation Current = testHttpRepresentation(iBase);
	xhttpfield Fields[] = {
		{
			XRT_STR_INIT("If-Match"),
			XRT_STR_INIT("\"v1\"")
		},
		{
			XRT_STR_INIT("If-Unmodified-Since"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:36 GMT")
		}
	};

	testRequire(xrtHttpPreconditionsEvaluate(
		XRT_STR_LITERAL("PUT"), Fields, 2, &Current
	) == XHTTP_PRECONDITION_PROCEED,
		"HTTP If-Match did not suppress If-Unmodified-Since");
	Fields[0].Value = XRT_STR_LITERAL("W/\"v1\"");
	testRequire(xrtHttpPreconditionsEvaluate(
		XRT_STR_LITERAL("PUT"), Fields, 2, &Current
	) == XHTTP_PRECONDITION_FAILED,
		"HTTP If-Match used weak comparison");
	Fields[0].Value = XRT_STR_LITERAL("*");
	testRequire(xrtHttpPreconditionsEvaluate(
		XRT_STR_LITERAL("PUT"), Fields, 2, &Current
	) == XHTTP_PRECONDITION_PROCEED,
		"HTTP If-Match wildcard rejected an existing representation");

	Fields[0] = Fields[1];
	testRequire(xrtHttpPreconditionsEvaluate(
		XRT_STR_LITERAL("PUT"), Fields, 1, &Current
	) == XHTTP_PRECONDITION_FAILED,
		"HTTP If-Unmodified-Since missed a newer representation");
	Fields[0].Value = XRT_STR_LITERAL("not-a-date");
	xrtClearError();
	testRequire(xrtHttpPreconditionsEvaluate(
		XRT_STR_LITERAL("PUT"), Fields, 1, &Current
	) == XHTTP_PRECONDITION_PROCEED &&
		(xrtGetError() == NULL),
		"HTTP invalid If-Unmodified-Since was not ignored cleanly");
	Fields[1] = Fields[0];
	xrtClearError();
	testRequire(xrtHttpPreconditionsEvaluate(
		XRT_STR_LITERAL("PUT"), Fields, 2, &Current
	) == XHTTP_PRECONDITION_PROCEED &&
		(xrtGetError() == NULL),
		"HTTP repeated If-Unmodified-Since was not ignored");
}



/* 验证 If-None-Match 的弱比较和方法相关结果。 */
static void testHttpIfNoneMatch(void)
{
	const xtime iBase = testHttpDate(
		"Sun, 06 Nov 1994 08:49:37 GMT"
	);
	xhttprepresentation Current = testHttpRepresentation(iBase);
	xhttpfield Fields[] = {
		{
			XRT_STR_INIT("If-None-Match"),
			XRT_STR_INIT("W/\"v1\"")
		},
		{
			XRT_STR_INIT("If-Modified-Since"),
			XRT_STR_INIT("invalid")
		}
	};

	testRequire(xrtHttpPreconditionsEvaluate(
		XRT_STR_LITERAL("GET"), Fields, 2, &Current
	) == XHTTP_PRECONDITION_NOT_MODIFIED,
		"HTTP GET If-None-Match did not produce 304");
	testRequire(xrtHttpPreconditionsEvaluate(
		XRT_STR_LITERAL("HEAD"), Fields, 2, &Current
	) == XHTTP_PRECONDITION_NOT_MODIFIED,
		"HTTP HEAD If-None-Match did not produce 304");
	testRequire(xrtHttpPreconditionsEvaluate(
		XRT_STR_LITERAL("PUT"), Fields, 2, &Current
	) == XHTTP_PRECONDITION_FAILED,
		"HTTP unsafe If-None-Match did not produce 412");
	testRequire(xrtHttpPreconditionsEvaluate(
		XRT_STR_LITERAL("get"), Fields, 2, &Current
	) == XHTTP_PRECONDITION_FAILED,
		"HTTP condition evaluator ignored method case");

	Fields[0].Value = XRT_STR_LITERAL("\"other\"");
	testRequire(xrtHttpPreconditionsEvaluate(
		XRT_STR_LITERAL("GET"), Fields, 2, &Current
	) == XHTTP_PRECONDITION_PROCEED,
		"HTTP If-None-Match rejected a non-matching tag");
}



/* 验证日期条件使用整秒语义并只作用于 GET/HEAD。 */
static void testHttpModifiedSince(void)
{
	const xtime iBase = testHttpDate(
		"Sun, 06 Nov 1994 08:49:37 GMT"
	);
	xhttprepresentation Current =
		testHttpRepresentation(iBase + INT64_C(999999));
	xhttpfield Field = {
		XRT_STR_INIT("If-Modified-Since"),
		XRT_STR_INIT("Sun, 06 Nov 1994 08:49:37 GMT")
	};

	testRequire(xrtHttpPreconditionsEvaluate(
		XRT_STR_LITERAL("GET"), &Field, 1, &Current
	) == XHTTP_PRECONDITION_NOT_MODIFIED,
		"HTTP If-Modified-Since did not compare at second precision");
	testRequire(xrtHttpPreconditionsEvaluate(
		XRT_STR_LITERAL("POST"), &Field, 1, &Current
	) == XHTTP_PRECONDITION_PROCEED,
		"HTTP If-Modified-Since affected a non-GET/HEAD method");
	{
		xhttpfield Repeated[2];

		Repeated[0] = Field;
		Repeated[1] = Field;

		xrtClearError();
		testRequire(xrtHttpPreconditionsEvaluate(
			XRT_STR_LITERAL("GET"), Repeated, 2, &Current
		) == XHTTP_PRECONDITION_PROCEED &&
			(xrtGetError() == NULL),
			"HTTP repeated If-Modified-Since was not ignored");
	}
	Current.LastModified += XRT_TIME_SECOND;
	testRequire(xrtHttpPreconditionsEvaluate(
		XRT_STR_LITERAL("GET"), &Field, 1, &Current
	) == XHTTP_PRECONDITION_PROCEED,
		"HTTP If-Modified-Since hid a newer representation");
}



/* 验证多字段合并、星号存在性和错误列表边界。 */
static void testHttpPreconditionFields(void)
{
	xhttprepresentation Current;
	xhttpfield Fields[] = {
		{
			XRT_STR_INIT("If-Match"),
			XRT_STR_INIT("\"other\"")
		},
		{
			XRT_STR_INIT("if-match"),
			XRT_STR_INIT("\"v1\"")
		}
	};

	Current = testHttpRepresentation(0);
	testRequire(xrtHttpPreconditionsEvaluate(
		XRT_STR_LITERAL("PUT"), Fields, 2, &Current
	) == XHTTP_PRECONDITION_PROCEED,
		"HTTP repeated If-Match fields were not combined");
	Fields[0].Value = XRT_STR_LITERAL("*");
	testRequire(xrtHttpPreconditionsEvaluate(
		XRT_STR_LITERAL("PUT"), Fields, 2, &Current
	) == XHTTP_PRECONDITION_ERROR,
		"HTTP mixed wildcard fields were accepted");
	xrtClearError();
	Fields[0].Value = XRT_STR_LITERAL("\"v1\", broken");
	testRequire(xrtHttpPreconditionsEvaluate(
		XRT_STR_LITERAL("PUT"), Fields, 1, &Current
	) == XHTTP_PRECONDITION_ERROR,
		"HTTP malformed If-Match list was accepted");
	xrtClearError();

	memset(&Current, 0, sizeof(Current));
	Fields[0].Value = XRT_STR_LITERAL("*");
	testRequire(xrtHttpPreconditionsEvaluate(
		XRT_STR_LITERAL("PUT"), Fields, 1, &Current
	) == XHTTP_PRECONDITION_FAILED,
		"HTTP If-Match wildcard matched a missing representation");
	Fields[0].Name = XRT_STR_LITERAL("If-None-Match");
	testRequire(xrtHttpPreconditionsEvaluate(
		XRT_STR_LITERAL("PUT"), Fields, 1, &Current
	) == XHTTP_PRECONDITION_PROCEED,
		"HTTP If-None-Match wildcard rejected a missing representation");
}



/* 验证不选择或修改表示的标准方法忽略前置条件。 */
static void testHttpIgnoredMethods(void)
{
	xhttprepresentation Current = testHttpRepresentation(0);
	xhttpfield Field = {
		XRT_STR_INIT("If-None-Match"),
		XRT_STR_INIT("*")
	};

	testRequire(xrtHttpPreconditionsEvaluate(
		XRT_STR_LITERAL("CONNECT"), &Field, 1, &Current
	) == XHTTP_PRECONDITION_PROCEED,
		"HTTP CONNECT evaluated representation preconditions");
	testRequire(xrtHttpPreconditionsEvaluate(
		XRT_STR_LITERAL("OPTIONS"), &Field, 1, &Current
	) == XHTTP_PRECONDITION_PROCEED,
		"HTTP OPTIONS evaluated representation preconditions");
	testRequire(xrtHttpPreconditionsEvaluate(
		XRT_STR_LITERAL("TRACE"), &Field, 1, &Current
	) == XHTTP_PRECONDITION_PROCEED,
		"HTTP TRACE evaluated representation preconditions");
}



/* 验证 If-Range 只接受匹配的强实体标签或强日期验证器。 */
static void testHttpIfRange(void)
{
	const xtime iBase = testHttpDate(
		"Sun, 06 Nov 1994 08:49:37 GMT"
	);
	xhttprepresentation Current =
		testHttpRepresentation(iBase + INT64_C(700000));

	testRequire(xrtHttpIfRangeMatch(
		XRT_STR_LITERAL("\"v1\""), &Current
	), "HTTP If-Range strong entity-tag did not match");
	testRequire(!xrtHttpIfRangeMatch(
		XRT_STR_LITERAL("W/\"v1\""), &Current
	), "HTTP If-Range accepted a weak request tag");
	Current.ETag.Weak = true;
	testRequire(!xrtHttpIfRangeMatch(
		XRT_STR_LITERAL("\"v1\""), &Current
	), "HTTP If-Range accepted a weak current tag");
	Current.ETag.Weak = false;
	testRequire(xrtHttpIfRangeMatch(
		XRT_STR_LITERAL("Sun, 06 Nov 1994 08:49:37 GMT"),
		&Current
	), "HTTP If-Range exact date did not match");
	Current.LastModifiedStrong = false;
	testRequire(!xrtHttpIfRangeMatch(
		XRT_STR_LITERAL("Sun, 06 Nov 1994 08:49:37 GMT"),
		&Current
	), "HTTP If-Range accepted a weak date validator");
	xrtClearError();
	testRequire(!xrtHttpIfRangeMatch(
		XRT_STR_LITERAL("invalid"), &Current
	) && (xrtGetError() == NULL),
		"HTTP invalid If-Range was not ignored cleanly");
	{
		xerror* pMarker = xrtErrorCreate(
			XERR_STATE,
			"test.http.precondition",
			1,
			"marker"
		);
		const xerror* pPrevious;

		testRequire(pMarker != NULL,
			"HTTP precondition marker allocation failed");
		xrtSetError(pMarker);
		xrtErrorFree(pMarker);
		pPrevious = xrtGetError();
		testRequire(!xrtHttpIfRangeMatch(
			XRT_STR_LITERAL("invalid"), &Current
		) && (xrtGetError() == pPrevious),
			"HTTP invalid If-Range changed the previous error");
		xrtClearError();
	}
}



/* 执行 HTTP 条件请求语义测试。 */
/* 验证条件请求支持未对齐描述符，并拒绝所有回绕的固定结构与借用文本。 */
static void testHttpPreconditionMemoryContract(void)
{
	uint8 FieldStorage[sizeof(xhttpfield) + 2u];
	uint8 CurrentStorage[sizeof(xhttprepresentation) + 2u];
	xhttprepresentation Current = testHttpRepresentation(0);
	xhttpfield Field = {
		XRT_STR_INIT("If-None-Match"),
		XRT_STR_INIT("\"v1\"")
	};

	memset(FieldStorage, 0xA5, sizeof(FieldStorage));
	memset(CurrentStorage, 0xB6, sizeof(CurrentStorage));
	memcpy(FieldStorage + 1u, &Field, sizeof(Field));
	memcpy(CurrentStorage + 1u, &Current, sizeof(Current));
	testRequire(xrtHttpPreconditionsEvaluate(
		XRT_STR_LITERAL("GET"),
		(const xhttpfield*)(FieldStorage + 1u),
		1u,
		(const xhttprepresentation*)(CurrentStorage + 1u)
	) == XHTTP_PRECONDITION_NOT_MODIFIED,
		"HTTP precondition rejected unaligned descriptors");
	testRequire(xrtHttpIfRangeMatch(
		XRT_STR_LITERAL("\"v1\""),
		(const xhttprepresentation*)(CurrentStorage + 1u)
	), "HTTP If-Range rejected an unaligned representation");
	testRequire((FieldStorage[0] == 0xA5u) &&
		(FieldStorage[sizeof(FieldStorage) - 1u] == 0xA5u) &&
		(CurrentStorage[0] == 0xB6u) &&
		(CurrentStorage[sizeof(CurrentStorage) - 1u] == 0xB6u),
		"HTTP precondition changed borrowed descriptor storage");

	xrtClearError();
	testRequire(xrtHttpPreconditionsEvaluate(
		XRT_STR_LITERAL("GET"),
		(const xhttpfield*)(uintptr_t)(UINTPTR_MAX - 1u),
		1u,
		&Current
	) == XHTTP_PRECONDITION_ERROR,
		"HTTP precondition accepted a wrapping field array");
	xrtClearError();
	testRequire(xrtHttpPreconditionsEvaluate(
		XRT_STR_LITERAL("GET"),
		NULL,
		0,
		(const xhttprepresentation*)(uintptr_t)(UINTPTR_MAX - 1u)
	) == XHTTP_PRECONDITION_ERROR,
		"HTTP precondition accepted a wrapping representation");
	xrtClearError();
	Current.ETag.Opaque = (xstrview){
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u),
		4u
	};
	testRequire(xrtHttpPreconditionsEvaluate(
		XRT_STR_LITERAL("GET"),
		NULL,
		0,
		&Current
	) == XHTTP_PRECONDITION_ERROR,
		"HTTP precondition accepted a wrapping validator text");
	xrtClearError();
}



int main(void)
{
	testHttpIfMatch();
	testHttpIfNoneMatch();
	testHttpModifiedSince();
	testHttpPreconditionFields();
	testHttpIgnoredMethods();
	testHttpIfRange();
	testHttpPreconditionMemoryContract();
	printf("[PASS] http_precondition\n");
	return 0;
}
