#include "../test.h"

#include <xrt/http_auth.h>



/* 按字节比较借用文本与零结尾常量。 */
static bool testHttpAuthTextEqual(xstrview Text, cstr sExpected)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		(memcmp(Text.Data, sExpected, iSize) == 0);
}



/* 验证 challenge 迭代能消除参数逗号与下一认证方案的歧义。 */
static void testHttpAuthChallenges(void)
{
	xstrview Text = XRT_STR_LITERAL(
		", Digest realm=\"apps\", nonce=\"n,1\", "
		"algorithm=SHA-256, , Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==, "
		"Custom"
	);
	xhttpauth Auth;
	xhttpparam Param;
	size_t iOffset = 0;
	size_t iParam = 0;

	testRequire((xrtHttpChallengeNext(
		Text, &iOffset, &Auth
	) == XHTTP_NEXT_ITEM) &&
		testHttpAuthTextEqual(Auth.Scheme, "Digest") &&
		(Auth.Kind == XHTTP_AUTH_PARAMS),
		"HTTP auth Digest challenge mismatch");
	testRequire((xrtHttpAuthParamNext(
		Auth.Data, &iParam, &Param
	) == XHTTP_NEXT_ITEM) &&
		testHttpAuthTextEqual(Param.Name, "realm") &&
		((Param.Flags & XHTTP_PARAM_QUOTED) != 0),
		"HTTP auth realm parameter mismatch");
	testRequire((xrtHttpAuthParamNext(
		Auth.Data, &iParam, &Param
	) == XHTTP_NEXT_ITEM) &&
		testHttpAuthTextEqual(Param.Name, "nonce") &&
		testHttpAuthTextEqual(Param.Value, "n,1"),
		"HTTP auth quoted comma parameter mismatch");
	testRequire((xrtHttpAuthParamNext(
		Auth.Data, &iParam, &Param
	) == XHTTP_NEXT_ITEM) &&
		testHttpAuthTextEqual(Param.Name, "algorithm") &&
		testHttpAuthTextEqual(Param.Value, "SHA-256") &&
		(xrtHttpAuthParamNext(
			Auth.Data, &iParam, &Param
		) == XHTTP_NEXT_END),
		"HTTP auth parameter suffix mismatch");
	testRequire((xrtHttpChallengeNext(
		Text, &iOffset, &Auth
	) == XHTTP_NEXT_ITEM) &&
		testHttpAuthTextEqual(Auth.Scheme, "Basic") &&
		(Auth.Kind == XHTTP_AUTH_TOKEN68) &&
		testHttpAuthTextEqual(
			Auth.Data,
			"QWxhZGRpbjpvcGVuIHNlc2FtZQ=="
		),
		"HTTP auth Basic challenge mismatch");
	testRequire((xrtHttpChallengeNext(
		Text, &iOffset, &Auth
	) == XHTTP_NEXT_ITEM) &&
		testHttpAuthTextEqual(Auth.Scheme, "Custom") &&
		(Auth.Kind == XHTTP_AUTH_NONE) &&
		(xrtHttpChallengeNext(
			Text, &iOffset, &Auth
		) == XHTTP_NEXT_END),
		"HTTP auth empty challenge mismatch");
}



/* 验证 challenge 游标跨重复字段并跳过无关字段。 */
static void testHttpAuthFields(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Date"),
			XRT_STR_INIT("Sun, 02 Aug 2026 00:00:00 GMT")
		},
		{
			XRT_STR_INIT("WWW-Authenticate"),
			XRT_STR_INIT("Digest realm=\"api\", Basic abc==")
		},
		{
			XRT_STR_INIT("WWW-Authenticate"),
			XRT_STR_INIT("Bearer token")
		}
	};
	xhttpauthcursor Cursor;
	xhttpauth Auth;

	xrtHttpAuthCursorInit(&Cursor);
	testRequire((xrtHttpFieldChallengeNext(
		Fields,
		sizeof(Fields) / sizeof(Fields[0]),
		XRT_STR_LITERAL("WWW-Authenticate"),
		&Cursor,
		&Auth
	) == XHTTP_NEXT_ITEM) &&
		testHttpAuthTextEqual(Auth.Scheme, "Digest") &&
		(xrtHttpFieldChallengeNext(
			Fields,
			sizeof(Fields) / sizeof(Fields[0]),
			XRT_STR_LITERAL("WWW-Authenticate"),
			&Cursor,
			&Auth
		) == XHTTP_NEXT_ITEM) &&
		testHttpAuthTextEqual(Auth.Scheme, "Basic") &&
		(xrtHttpFieldChallengeNext(
			Fields,
			sizeof(Fields) / sizeof(Fields[0]),
			XRT_STR_LITERAL("WWW-Authenticate"),
			&Cursor,
			&Auth
		) == XHTTP_NEXT_ITEM) &&
		testHttpAuthTextEqual(Auth.Scheme, "Bearer") &&
		(xrtHttpFieldChallengeNext(
			Fields,
			sizeof(Fields) / sizeof(Fields[0]),
			XRT_STR_LITERAL("WWW-Authenticate"),
			&Cursor,
			&Auth
		) == XHTTP_NEXT_END),
		"HTTP auth field cursor mismatch");
}



/* 验证单份凭据拒绝 challenge 列表并保留认证数据类别。 */
static void testHttpAuthCredentials(void)
{
	static const xstrview Invalid[] = {
		XRT_STR_INIT(""),
		XRT_STR_INIT(", Basic abc"),
		XRT_STR_INIT("Basic abc,"),
		XRT_STR_INIT("Basic\tabc"),
		XRT_STR_INIT("Digest realm ="),
		XRT_STR_INIT("Digest realm=\"unterminated"),
		XRT_STR_INIT("Basic abc, Bearer def")
	};
	xhttpauth Auth;
	size_t i;

	testRequire(xrtHttpAuthParse(
		XRT_STR_LITERAL("Custom a=b, title=\"x,y\""),
		&Auth
	) && testHttpAuthTextEqual(Auth.Scheme, "Custom") &&
		(Auth.Kind == XHTTP_AUTH_PARAMS),
		"HTTP auth parameter credentials mismatch");
	testRequire(xrtHttpAuthParse(
		XRT_STR_LITERAL("Scheme"), &Auth
	) && (Auth.Kind == XHTTP_AUTH_NONE),
		"HTTP auth empty credentials mismatch");
	for ( i = 0; i < (sizeof(Invalid) / sizeof(Invalid[0])); i++ ) {
		testRequire(!xrtHttpAuthParse(
			Invalid[i], &Auth
		), "HTTP auth parser accepted malformed credentials");
		xrtClearError();
	}
}



/* 验证认证字段写出查询、短缓冲、重叠和拥有型便利层。 */
static void testHttpAuthWrite(void)
{
	unsigned char Output[96];
	unsigned char Before[96];
	xhttpauth Auth;
	str sBuilt;
	size_t iSize;

	testRequire(xrtHttpAuthWrite(
		XRT_STR_LITERAL("Bearer"),
		XRT_STR_LITERAL("mF_9.B5f-4.1JqM"),
		NULL, 0, &iSize
	) && (iSize == 22u),
		"HTTP auth writer query mismatch");
	memset(Output, 0xA5, sizeof(Output));
	testRequire(xrtHttpAuthWrite(
		XRT_STR_LITERAL("Bearer"),
		XRT_STR_LITERAL("mF_9.B5f-4.1JqM"),
		Output, sizeof(Output), &iSize
	) && (iSize == 22u) &&
		(memcmp(Output, "Bearer mF_9.B5f-4.1JqM", 22u) == 0),
		"HTTP auth writer output mismatch");
	testRequire(xrtHttpAuthParse(
		(xstrview){ (cstr)Output, iSize }, &Auth
	) && (Auth.Kind == XHTTP_AUTH_TOKEN68),
		"HTTP auth writer round trip mismatch");
	testRequire(xrtHttpAuthWrite(
		XRT_STR_LITERAL("Digest"),
		XRT_STR_LITERAL("realm=\"api\", algorithm=SHA-256"),
		Output, sizeof(Output), &iSize
	) && xrtHttpAuthParse(
		(xstrview){ (cstr)Output, iSize }, &Auth
	) && (Auth.Kind == XHTTP_AUTH_PARAMS),
		"HTTP auth parameter writer mismatch");

	memset(Output, 0xA5, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	testRequire(!xrtHttpAuthWrite(
		XRT_STR_LITERAL("Bearer"),
		XRT_STR_LITERAL("token"),
		Output, 5u, &iSize
	) && (iSize == 12u) &&
		(memcmp(Output, Before, sizeof(Output)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HTTP auth short output was not atomic");
	xrtClearError();
	testRequire(!xrtHttpAuthWrite(
		(xstrview){ (cstr)Output, 6u },
		XRT_STR_LITERAL("token"),
		Output, sizeof(Output), &iSize
	), "HTTP auth writer accepted overlapping output");
	xrtClearError();
	testRequire(!xrtHttpAuthWrite(
		XRT_STR_LITERAL("Bad Scheme"),
		XRT_STR_LITERAL("token"),
		NULL, 0, &iSize
	) && !xrtHttpAuthWrite(
		XRT_STR_LITERAL("Digest"),
		XRT_STR_LITERAL("realm=\"x\","),
		NULL, 0, &iSize
	), "HTTP auth writer accepted malformed input");
	xrtClearError();

	sBuilt = xrtHttpAuthBuild(
		XRT_STR_LITERAL("Custom"),
		XRT_STR_LITERAL("a=b"),
		&iSize
	);
	testRequire((sBuilt != NULL) && (iSize == 10u) &&
		(strcmp(sBuilt, "Custom a=b") == 0),
		"HTTP auth owned builder mismatch");
	xrtFree(sBuilt);
}



/* 验证 token68 与 auth-param 的边界字符。 */
static void testHttpAuthSyntax(void)
{
	static const xstrview Valid[] = {
		XRT_STR_INIT("a"),
		XRT_STR_INIT("abc=="),
		XRT_STR_INIT("mF_9.B5f-4.1JqM"),
		XRT_STR_INIT("-_~+/")
	};
	static const xstrview Invalid[] = {
		XRT_STR_INIT(""),
		XRT_STR_INIT("=abc"),
		XRT_STR_INIT("a=b"),
		XRT_STR_INIT("a b"),
		XRT_STR_INIT("a,b")
	};
	size_t i;

	for ( i = 0; i < (sizeof(Valid) / sizeof(Valid[0])); i++ ) {
		testRequire(xrtHttpAuthToken68Valid(Valid[i]),
			"HTTP auth rejected valid token68");
	}
	for ( i = 0; i < (sizeof(Invalid) / sizeof(Invalid[0])); i++ ) {
		testRequire(!xrtHttpAuthToken68Valid(Invalid[i]),
			"HTTP auth accepted invalid token68");
	}
}



/* 验证认证描述符支持未对齐存储，并原子拒绝回绕范围。 */
static void testHttpAuthMemoryContracts(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("WWW-Authenticate"),
			XRT_STR_INIT("Basic abc==")
		},
		{
			XRT_STR_INIT("WWW-Authenticate"),
			XRT_STR_INIT("Bearer token")
		}
	};
	static const xhttpfield FailureFields[] = {
		{
			XRT_STR_INIT("WWW-Authenticate"),
			XRT_STR_INIT("Basic abc")
		},
		{
			XRT_STR_INIT("WWW-Authenticate"),
			XRT_STR_INIT("Broken realm=\"unterminated")
		}
	};
	uint8 OffsetStorage[sizeof(size_t) + 2u];
	uint8 ParamStorage[sizeof(xhttpparam) + 2u];
	uint8 AuthStorage[sizeof(xhttpauth) + 2u];
	uint8 CursorStorage[sizeof(xhttpauthcursor) + 2u];
	uint8 FieldStorage[sizeof(Fields) + 2u];
	uint8 SizeStorage[sizeof(size_t) + 2u];
	size_t* pOffset = (size_t*)(void*)(OffsetStorage + 1u);
	xhttpparam* pParam =
		(xhttpparam*)(void*)(ParamStorage + 1u);
	xhttpauth* pAuth = (xhttpauth*)(void*)(AuthStorage + 1u);
	xhttpauthcursor* pCursor =
		(xhttpauthcursor*)(void*)(CursorStorage + 1u);
	const xhttpfield* pFields =
		(const xhttpfield*)(const void*)(FieldStorage + 1u);
	size_t* pSize = (size_t*)(void*)(SizeStorage + 1u);
	xhttpauthcursor Cursor;
	xhttpauthcursor BeforeCursor;
	xhttpparam Param;
	xhttpauth Auth;
	const xerror* pBefore;
	char Output[32];
	size_t iOffset = 0;
	size_t iSize;
	str sBuilt;

	memset(OffsetStorage, 0xA5, sizeof(OffsetStorage));
	memset(ParamStorage, 0xA5, sizeof(ParamStorage));
	memset(AuthStorage, 0xA5, sizeof(AuthStorage));
	memset(CursorStorage, 0xA5, sizeof(CursorStorage));
	memset(FieldStorage, 0xA5, sizeof(FieldStorage));
	memset(SizeStorage, 0xA5, sizeof(SizeStorage));
	memcpy(pOffset, &iOffset, sizeof(iOffset));
	memcpy(FieldStorage + 1u, Fields, sizeof(Fields));
	testRequire(xrtHttpAuthParamNext(
		XRT_STR_LITERAL("realm=\"api\""), pOffset, pParam
	) == XHTTP_NEXT_ITEM,
		"HTTP auth parameter rejected unaligned outputs");
	memcpy(&Param, pParam, sizeof(Param));
	testRequire(testHttpAuthTextEqual(Param.Name, "realm"),
		"HTTP auth parameter published wrong unaligned result");

	iOffset = 0;
	memcpy(pOffset, &iOffset, sizeof(iOffset));
	testRequire(xrtHttpChallengeNext(
		XRT_STR_LITERAL("Basic abc=="), pOffset, pAuth
	) == XHTTP_NEXT_ITEM,
		"HTTP challenge rejected unaligned outputs");
	memcpy(&Auth, pAuth, sizeof(Auth));
	testRequire((Auth.Kind == XHTTP_AUTH_TOKEN68) &&
		testHttpAuthTextEqual(Auth.Scheme, "Basic"),
		"HTTP challenge published wrong unaligned result");
	testRequire(xrtHttpAuthParse(
		XRT_STR_LITERAL("Bearer token"), pAuth
	), "HTTP auth parser rejected unaligned output");

	xrtHttpAuthCursorInit(pCursor);
	testRequire(xrtHttpFieldChallengeNext(
		pFields,
		2u,
		XRT_STR_LITERAL("WWW-Authenticate"),
		pCursor,
		pAuth
	) == XHTTP_NEXT_ITEM,
		"HTTP field challenge rejected unaligned storage");
	memcpy(&Cursor, pCursor, sizeof(Cursor));
	memcpy(&Auth, pAuth, sizeof(Auth));
	testRequire((Cursor.FieldIndex == 0u) &&
		(Auth.Kind == XHTTP_AUTH_TOKEN68),
		"HTTP field challenge published wrong unaligned state");

	testRequire(xrtHttpAuthWrite(
		XRT_STR_LITERAL("Bearer"), XRT_STR_LITERAL("token"),
		Output, sizeof(Output), pSize
	), "HTTP auth writer rejected unaligned size output");
	memcpy(&iSize, pSize, sizeof(iSize));
	testRequire((iSize == 12u) &&
		(memcmp(Output, "Bearer token", 12u) == 0),
		"HTTP auth writer published wrong unaligned result");
	sBuilt = xrtHttpAuthBuild(
		XRT_STR_LITERAL("Bearer"), XRT_STR_LITERAL("token"), pSize
	);
	testRequire(sBuilt != NULL,
		"HTTP auth builder rejected unaligned size output");
	xrtFree(sBuilt);
	testRequire(
		(OffsetStorage[0] == 0xA5) &&
		(OffsetStorage[sizeof(OffsetStorage) - 1u] == 0xA5) &&
		(ParamStorage[0] == 0xA5) &&
		(ParamStorage[sizeof(ParamStorage) - 1u] == 0xA5) &&
		(AuthStorage[0] == 0xA5) &&
		(AuthStorage[sizeof(AuthStorage) - 1u] == 0xA5) &&
		(CursorStorage[0] == 0xA5) &&
		(CursorStorage[sizeof(CursorStorage) - 1u] == 0xA5) &&
		(FieldStorage[0] == 0xA5) &&
		(FieldStorage[sizeof(FieldStorage) - 1u] == 0xA5) &&
		(SizeStorage[0] == 0xA5) &&
		(SizeStorage[sizeof(SizeStorage) - 1u] == 0xA5),
		"HTTP auth operations wrote outside unaligned storage"
	);

	iOffset = 0;
	memset(&Param, 0xA5, sizeof(Param));
	testRequire(xrtHttpAuthParamNext(
		XRT_STR_LITERAL("realm"), &iOffset, &Param
	) == XHTTP_NEXT_ERROR && (iOffset == 0u) &&
		(Param.Name.Data == NULL) && (Param.Value.Data == NULL),
		"HTTP auth valueless parameter advanced partial state");
	xrtClearError();
	xrtHttpAuthCursorInit(&Cursor);
	testRequire(xrtHttpFieldChallengeNext(
		FailureFields,
		2u,
		XRT_STR_LITERAL("WWW-Authenticate"),
		&Cursor,
		&Auth
	) == XHTTP_NEXT_ITEM,
		"HTTP auth cursor failure fixture did not yield first item");
	BeforeCursor = Cursor;
	memset(&Auth, 0xA5, sizeof(Auth));
	testRequire(xrtHttpFieldChallengeNext(
		FailureFields,
		2u,
		XRT_STR_LITERAL("WWW-Authenticate"),
		&Cursor,
		&Auth
	) == XHTTP_NEXT_ERROR &&
		(memcmp(&Cursor, &BeforeCursor, sizeof(Cursor)) == 0) &&
		(Auth.Scheme.Data == NULL) && (Auth.Data.Data == NULL) &&
		(Auth.Kind == XHTTP_AUTH_NONE),
		"HTTP auth field error advanced cursor or leaked result");
	xrtClearError();

	testRequire(xrtHttpAuthParamNext(
		XRT_STR_LITERAL("realm=x"),
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u),
		&Param
	) == XHTTP_NEXT_ERROR,
		"HTTP auth parameter accepted wrapping offset");
	xrtClearError();
	testRequire(xrtHttpChallengeNext(
		XRT_STR_LITERAL("Basic abc"),
		&iOffset,
		(xhttpauth*)(uintptr_t)(UINTPTR_MAX - 1u)
	) == XHTTP_NEXT_ERROR,
		"HTTP challenge accepted wrapping result");
	xrtClearError();
	testRequire(xrtHttpChallengeNext(
		XRT_STR_LITERAL("Basic abc"),
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u),
		&Auth
	) == XHTTP_NEXT_ERROR,
		"HTTP challenge accepted wrapping offset");
	xrtClearError();
	xrtHttpAuthCursorInit(
		(xhttpauthcursor*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"HTTP auth cursor accepted wrapping output");
	xrtClearError();
	testRequire(xrtHttpFieldChallengeNext(
		(const xhttpfield*)(uintptr_t)(UINTPTR_MAX - 1u),
		1u,
		XRT_STR_LITERAL("WWW-Authenticate"),
		&Cursor,
		&Auth
	) == XHTTP_NEXT_ERROR,
		"HTTP field challenge accepted wrapping field array");
	xrtClearError();
	testRequire(xrtHttpFieldChallengeNext(
		Fields,
		2u,
		XRT_STR_LITERAL("WWW-Authenticate"),
		(xhttpauthcursor*)(uintptr_t)(UINTPTR_MAX - 1u),
		&Auth
	) == XHTTP_NEXT_ERROR,
		"HTTP field challenge accepted wrapping cursor");
	xrtClearError();
	testRequire(xrtHttpFieldChallengeNext(
		Fields,
		2u,
		XRT_STR_LITERAL("WWW-Authenticate"),
		&Cursor,
		(xhttpauth*)(uintptr_t)(UINTPTR_MAX - 1u)
	) == XHTTP_NEXT_ERROR,
		"HTTP field challenge accepted wrapping result");
	xrtClearError();
	testRequire(!xrtHttpAuthParse(
		XRT_STR_LITERAL("Bearer token"),
		(xhttpauth*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP auth parser accepted wrapping result");
	xrtClearError();
	testRequire(!xrtHttpAuthWrite(
		XRT_STR_LITERAL("Bearer"), XRT_STR_LITERAL("token"),
		(void*)(uintptr_t)(UINTPTR_MAX - 1u),
		12u,
		&iSize
	), "HTTP auth writer accepted wrapping output");
	xrtClearError();
	testRequire(!xrtHttpAuthWrite(
		XRT_STR_LITERAL("Bearer"), XRT_STR_LITERAL("token"),
		Output,
		sizeof(Output),
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP auth writer accepted wrapping size output");
	xrtClearError();
	testRequire(xrtHttpAuthBuild(
		XRT_STR_LITERAL("Bearer"), XRT_STR_LITERAL("token"),
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
	) == NULL, "HTTP auth builder accepted wrapping size output");
	xrtClearError();

	testRequire(!xrtHttpAuthWrite(
		XRT_STR_LITERAL("Bad Scheme"), XRT_STR_LITERAL("token"),
		NULL, 0, &iSize
	), "HTTP auth predicate preservation fixture failed");
	pBefore = xrtGetError();
	testRequire((pBefore != NULL) && !xrtHttpAuthToken68Valid(
		(xstrview){ (cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u }
	) && (xrtGetError() == pBefore),
		"HTTP token68 predicate replaced the current error");
	xrtClearError();
}



int main(void)
{
	testHttpAuthChallenges();
	testHttpAuthFields();
	testHttpAuthCredentials();
	testHttpAuthWrite();
	testHttpAuthSyntax();
	testHttpAuthMemoryContracts();
	puts("[PASS] HTTP authentication syntax");
	return 0;
}
