#include "../test.h"



/* 验证公共状态常量和原因短语覆盖正式注册值且允许扩展状态。 */
static void testHttpStatus(void)
{
	xstrview Text;

	Text = xrtHttpStatusText(XHTTP_STATUS_OK);
	testRequire((Text.Size == 2) &&
		(memcmp(Text.Data, "OK", 2) == 0),
		"HTTP status text did not resolve 200");
	Text = xrtHttpStatusText(
		XHTTP_STATUS_UNPROCESSABLE_CONTENT
	);
	testRequire((Text.Size == 21) &&
		(memcmp(Text.Data, "Unprocessable Content", 21) == 0),
		"HTTP status text did not use the current registered phrase");
	Text = xrtHttpStatusText(
		XHTTP_STATUS_NETWORK_AUTHENTICATION_REQUIRED
	);
	testRequire((Text.Size == 31) &&
		(memcmp(
			Text.Data,
			"Network Authentication Required",
			31
		) == 0), "HTTP status text did not resolve 511");
	testRequire(
		(xrtHttpStatusText(104).Size == 0) &&
		(xrtHttpStatusText(418).Size == 0) &&
		(xrtHttpStatusText(799).Size == 0),
		"HTTP status text guessed a temporary or unknown status"
	);
}



/* 验证 token 与字段值严格遵守线路字符边界。 */
static void testHttpValidation(void)
{
	static const char InvalidValue[] = { 'o', 'k', '\r', '\n', 'x' };
	static const char ObsValue[] = { (char)0x80, (char)0xFF };

	testRequire(xrtHttpTokenValid(XRT_STR_LITERAL("Content-Type")),
		"HTTP token rejected a valid field name");
	testRequire(xrtHttpTokenValid(XRT_STR_LITERAL("!#$%&'*+-.^_`|~")),
		"HTTP token rejected RFC punctuation");
	testRequire(!xrtHttpTokenValid(XRT_STR_LITERAL("bad name")) &&
		!xrtHttpTokenValid(XRT_STR_LITERAL("bad:name")) &&
		!xrtHttpTokenValid(XRT_STR_LITERAL("")),
		"HTTP token accepted an invalid byte");
	testRequire(xrtHttpFieldValueValid(XRT_STR_LITERAL(" text\tvalue ")) &&
		xrtHttpFieldValueValid((xstrview){ ObsValue, sizeof(ObsValue) }),
		"HTTP field value rejected valid bytes");
	testRequire(!xrtHttpFieldValueValid((xstrview){
		InvalidValue, sizeof(InvalidValue)
	}), "HTTP field value accepted CRLF injection");
	testRequire(!xrtHttpFieldValueValid((xstrview){
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
	}) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP field value accepted a wrapping borrowed view");
	xrtClearError();
}



/* 验证 OWS、token 比较与接收方 token-list 迭代共享同一套线路语义。 */
static void testHttpTokens(void)
{
	static const xstrview Invalid[] = {
		XRT_STR_INIT("close keep-alive"),
		XRT_STR_INIT("gzip;q=1")
	};
	xstrview Text = XRT_STR_LITERAL(" \t alpha \t");
	xstrview Token;
	size_t iOffset = 0;
	size_t iCount;
	size_t i;

	Text = xrtHttpOwsTrim(Text);
	testRequire((Text.Size == 5) &&
		(memcmp(Text.Data, "alpha", 5) == 0),
		"HTTP OWS trim changed the inner value");
	testRequire(xrtHttpTokenEqual(
		XRT_STR_LITERAL("Upgrade"), XRT_STR_LITERAL("upgrade")
	), "HTTP token comparison is not ASCII case insensitive");
	testRequire(xrtHttpTokenNext(
		XRT_STR_LITERAL("close, keep-alive, Upgrade"),
		&iOffset, &Token
	) == XHTTP_NEXT_ITEM &&
		xrtHttpTokenEqual(Token, XRT_STR_LITERAL("close")),
		"HTTP token-list first item mismatch");
	testRequire(xrtHttpTokenNext(
		XRT_STR_LITERAL("close, keep-alive, Upgrade"),
		&iOffset, &Token
	) == XHTTP_NEXT_ITEM &&
		xrtHttpTokenEqual(Token, XRT_STR_LITERAL("keep-alive")),
		"HTTP token-list middle item mismatch");
	testRequire(xrtHttpTokenNext(
		XRT_STR_LITERAL("close, keep-alive, Upgrade"),
		&iOffset, &Token
	) == XHTTP_NEXT_ITEM &&
		xrtHttpTokenEqual(Token, XRT_STR_LITERAL("upgrade")),
		"HTTP token-list final item mismatch");
	testRequire(xrtHttpTokenNext(
		XRT_STR_LITERAL("close, keep-alive, Upgrade"),
		&iOffset, &Token
	) == XHTTP_NEXT_END,
		"HTTP token-list did not end exactly");
	testRequire(xrtHttpTokenListHas(
		XRT_STR_LITERAL("close, Upgrade"),
		XRT_STR_LITERAL("upgrade")
	), "HTTP token-list lookup missed an item");
	testRequire(xrtHttpTokenListCount(
		XRT_STR_LITERAL("gzip, br, deflate"), &iCount
	) && (iCount == 3), "HTTP token-list count mismatch");
	testRequire(xrtHttpTokenListHas(
		XRT_STR_LITERAL(",, upgrade,"),
		XRT_STR_LITERAL("upgrade")
	), "HTTP token-list lookup rejected RFC empty elements");
	testRequire(xrtHttpTokenListCount(
		XRT_STR_LITERAL("gzip,,br,"), &iCount
	) && (iCount == 2),
		"HTTP token-list count did not ignore empty elements");

	for ( i = 0; i < (sizeof(Invalid) / sizeof(Invalid[0])); i++ ) {
		iOffset = 0;
		testRequire(xrtHttpTokenNext(
			Invalid[i], &iOffset, &Token
		) == XHTTP_NEXT_ERROR,
			"HTTP token-list accepted malformed syntax");
		xrtClearError();
	}
}



/* 验证 token-list 规范写出、构建和失败原子契约。 */
static void testHttpTokenWrite(void)
{
	static const xstrview Tokens[] = {
		XRT_STR_INIT("GET"),
		XRT_STR_INIT("HEAD"),
		XRT_STR_INIT("OPTIONS")
	};
	static const xstrview Invalid[] = {
		XRT_STR_INIT("GET"),
		XRT_STR_INIT("BAD METHOD")
	};
	static const char Expected[] = "GET, HEAD, OPTIONS";
	uint8 TokenStorage[sizeof(Tokens) + 1u];
	const xstrview* pUnaligned;
	char Output[32];
	char Before[32];
	str sBuilt;
	size_t iSize;

	testRequire(xrtHttpTokenListWrite(
		Tokens, 3u, NULL, 0, &iSize
	) && (iSize == (sizeof(Expected) - 1u)),
		"HTTP token-list size query mismatch");
	memset(Output, '#', sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	testRequire(!xrtHttpTokenListWrite(
		Tokens, 3u, Output, iSize - 1u, &iSize
	) && (iSize == (sizeof(Expected) - 1u)) &&
		(memcmp(Output, Before, sizeof(Output)) == 0),
		"HTTP token-list short write was not atomic");
	xrtClearError();
	memcpy(TokenStorage + 1u, Tokens, sizeof(Tokens));
	pUnaligned = (const xstrview*)(const void*)(
		TokenStorage + 1u
	);
	testRequire(xrtHttpTokenListWrite(
		pUnaligned, 3u, Output, sizeof(Output), &iSize
	) && (iSize == (sizeof(Expected) - 1u)) &&
		(memcmp(Output, Expected, iSize) == 0),
		"HTTP token-list writer rejected unaligned descriptors");
	memset(Output, '#', sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	iSize = 71u;
	testRequire(!xrtHttpTokenListWrite(
		Invalid, 2u, Output, sizeof(Output), &iSize
	) && (iSize == 71u) &&
		(memcmp(Output, Before, sizeof(Output)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP token-list writer accepted an invalid token");
	xrtClearError();
	iSize = 73u;
	testRequire(!xrtHttpTokenListWrite(
		Tokens,
		3u,
		(void*)Tokens[0].Data,
		Tokens[0].Size,
		&iSize
	) && (iSize == 73u) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP token-list writer accepted overlapping output");
	xrtClearError();
	testRequire(xrtHttpTokenListWrite(
		NULL, 0, Output, sizeof(Output), &iSize
	) && (iSize == 0),
		"HTTP token-list writer rejected an empty list");
	sBuilt = xrtHttpTokenListBuild(Tokens, 3u, &iSize);
	testRequire((sBuilt != NULL) &&
		(iSize == (sizeof(Expected) - 1u)) &&
		(strcmp(sBuilt, Expected) == 0),
		"HTTP token-list Build mismatch");
	xrtFree(sBuilt);
	sBuilt = xrtHttpTokenListBuild(NULL, 0, &iSize);
	testRequire((sBuilt != NULL) && (iSize == 0) &&
		(sBuilt[0] == 0),
		"HTTP token-list empty Build mismatch");
	xrtFree(sBuilt);
}



/* 验证重复字段 token 游标在首项发布前完成全量校验。 */
static void testHttpFieldTokens(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("X-List"), XRT_STR_INIT("alpha, beta") },
		{ XRT_STR_INIT("X-Other"), XRT_STR_INIT("ignored") },
		{ XRT_STR_INIT("x-list"), XRT_STR_INIT(", gamma,") }
	};
	static const xhttpfield Invalid[] = {
		{ XRT_STR_INIT("X-List"), XRT_STR_INIT("published") },
		{ XRT_STR_INIT("x-list"), XRT_STR_INIT("bad item") }
	};
	static const xhttpfield Other[] = {
		{ XRT_STR_INIT("X-List"), XRT_STR_INIT("other") },
		{ XRT_STR_INIT("X-Other"), XRT_STR_INIT("ignored") },
		{ XRT_STR_INIT("x-list"), XRT_STR_INIT("tail") }
	};
	static const char* Expected[] = {
		"alpha", "beta", "gamma"
	};
	xhttpfieldtokencursor Cursor;
	xhttpfieldtokencursor Before;
	xstrview Token;
	size_t iCount;
	size_t i;

	testRequire(xrtHttpFieldTokenCount(
		Fields, 3u, XRT_STR_LITERAL("X-LIST"), &iCount
	) && (iCount == 3u),
		"HTTP repeated field token count mismatch");
	testRequire(
		xrtHttpFieldTokenFind(
			Fields, 3u,
			XRT_STR_LITERAL("X-List"),
			XRT_STR_LITERAL("GAMMA")
		) == XHTTP_NEXT_ITEM,
		"HTTP repeated field token find missed an item"
	);
	testRequire(
		xrtHttpFieldTokenFind(
			Fields, 3u,
			XRT_STR_LITERAL("X-List"),
			XRT_STR_LITERAL("missing")
		) == XHTTP_NEXT_END,
		"HTTP repeated field token find reported a missing item"
	);
	xrtHttpFieldTokenCursorInit(&Cursor);
	for ( i = 0; i < 3u; i++ ) {
		testRequire(
			(xrtHttpFieldTokenNext(
				Fields,
				3u,
				XRT_STR_LITERAL("X-List"),
				&Cursor,
				&Token
			 ) == XHTTP_NEXT_ITEM) &&
			(Token.Size == strlen(Expected[i])) &&
			(memcmp(
				Token.Data, Expected[i], Token.Size
			) == 0),
			"HTTP repeated field token order mismatch"
		);
	}
	testRequire(xrtHttpFieldTokenNext(
		Fields,
		3u,
		XRT_STR_LITERAL("X-List"),
		&Cursor,
		&Token
	) == XHTTP_NEXT_END,
		"HTTP repeated field token cursor did not end");

	xrtHttpFieldTokenCursorInit(&Cursor);
	testRequire(
		xrtHttpFieldTokenNext(
			Fields, 3u, XRT_STR_LITERAL("X-List"),
			&Cursor, &Token
		) == XHTTP_NEXT_ITEM,
		"HTTP repeated field token source-binding setup failed"
	);
	Before = Cursor;
	Token = XRT_STR_LITERAL("unchanged");
	testRequire(
		(xrtHttpFieldTokenNext(
			Other, 3u, XRT_STR_LITERAL("X-List"),
			&Cursor, &Token
		 ) == XHTTP_NEXT_ERROR) &&
		(memcmp(&Cursor, &Before, sizeof(Cursor)) == 0) &&
		(Token.Data == NULL) && (Token.Size == 0),
		"HTTP repeated field token cursor changed source"
	);
	xrtClearError();
	testRequire(
		xrtHttpFieldTokenNext(
			Fields, 3u, XRT_STR_LITERAL("x-list"),
			&Cursor, &Token
		) == XHTTP_NEXT_ITEM,
		"HTTP repeated field token cursor rejected an equivalent name"
	);

	xrtHttpFieldTokenCursorInit(&Cursor);
	Before = Cursor;
	Token = XRT_STR_LITERAL("unchanged");
	testRequire(
		(xrtHttpFieldTokenNext(
			Invalid,
			2u,
			XRT_STR_LITERAL("X-List"),
			&Cursor,
			&Token
		 ) == XHTTP_NEXT_ERROR) &&
		(memcmp(&Cursor, &Before, sizeof(Cursor)) == 0) &&
		(Token.Data == NULL) && (Token.Size == 0),
		"HTTP repeated field token cursor published before validation"
	);
	xrtClearError();
	iCount = 99u;
	testRequire(!xrtHttpFieldTokenCount(
		Invalid, 2u, XRT_STR_LITERAL("X-List"), &iCount
	) && (iCount == 0),
		"HTTP repeated field token count accepted malformed syntax");
	xrtClearError();
	testRequire(
		xrtHttpFieldTokenFind(
			Invalid, 2u,
			XRT_STR_LITERAL("X-List"),
			XRT_STR_LITERAL("published")
		) == XHTTP_NEXT_ERROR,
		"HTTP repeated field token find published before validation"
	);
	xrtClearError();
}



/* 验证通用 qvalue 与加权 token 迭代覆盖严格线路边界。 */
static void testHttpWeights(void)
{
	static const xstrview Valid[] = {
		XRT_STR_INIT("0"),
		XRT_STR_INIT("0."),
		XRT_STR_INIT("0.1"),
		XRT_STR_INIT("0.12"),
		XRT_STR_INIT("0.123"),
		XRT_STR_INIT("1"),
		XRT_STR_INIT("1.000"),
		XRT_STR_INIT(" \t0.5\t")
	};
	static const uint16 Quality[] = {
		0, 0, 100, 120, 123, 1000, 1000, 500
	};
	static const xstrview Invalid[] = {
		XRT_STR_INIT(""),
		XRT_STR_INIT(".5"),
		XRT_STR_INIT("00"),
		XRT_STR_INIT("0.0000"),
		XRT_STR_INIT("1.001"),
		XRT_STR_INIT("2"),
		XRT_STR_INIT("-1")
	};
	xstrview List = XRT_STR_LITERAL(
		", gzip; q=1.000, deflate;Q=0.25, identity,"
	);
	xhttpweightedtoken Item;
	union {
		uint64 Align;
		uint8 Bytes[sizeof(xhttpweightedtoken) + 1u];
	} ItemStorage;
	union {
		uint64 Align;
		uint8 Bytes[sizeof(size_t) + 1u];
	} OffsetStorage;
	union {
		uint64 Align;
		uint8 Bytes[sizeof(uint16) + 1u];
	} QualityStorage;
	xhttpweightedtoken* pUnalignedItem =
		(xhttpweightedtoken*)(ItemStorage.Bytes + 1u);
	size_t* pUnalignedOffset =
		(size_t*)(OffsetStorage.Bytes + 1u);
	uint16* pUnalignedQuality =
		(uint16*)(QualityStorage.Bytes + 1u);
	xstrview Wrapped = {
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
	};
	uint16 iQuality;
	size_t iOffset = 0;
	size_t i;

	for ( i = 0; i < (sizeof(Valid) / sizeof(Valid[0])); i++ ) {
		iQuality = UINT16_MAX;
		testRequire(
			xrtHttpQualityParse(Valid[i], &iQuality) &&
			(iQuality == Quality[i]),
			"HTTP qvalue rejected a valid value"
		);
	}
	for ( i = 0; i < (sizeof(Invalid) / sizeof(Invalid[0])); i++ ) {
		iQuality = UINT16_MAX;
		testRequire(
			!xrtHttpQualityParse(Invalid[i], &iQuality) &&
			(iQuality == 0),
			"HTTP qvalue accepted malformed syntax"
		);
		xrtClearError();
	}
	testRequire(
		(xrtHttpWeightedTokenNext(
			List, &iOffset, &Item
		 ) == XHTTP_NEXT_ITEM) &&
		xrtHttpTokenEqual(
			Item.Token, XRT_STR_LITERAL("gzip")
		) && (Item.Quality == 1000),
		"HTTP weighted token first item mismatch"
	);
	testRequire(
		(xrtHttpWeightedTokenNext(
			List, &iOffset, &Item
		 ) == XHTTP_NEXT_ITEM) &&
		xrtHttpTokenEqual(
			Item.Token, XRT_STR_LITERAL("deflate")
		) && (Item.Quality == 250),
		"HTTP weighted token middle item mismatch"
	);
	testRequire(
		(xrtHttpWeightedTokenNext(
			List, &iOffset, &Item
		 ) == XHTTP_NEXT_ITEM) &&
		xrtHttpTokenEqual(
			Item.Token, XRT_STR_LITERAL("identity")
		) && (Item.Quality == 1000),
		"HTTP weighted token default quality mismatch"
	);
	testRequire(
		xrtHttpWeightedTokenNext(
			List, &iOffset, &Item
		) == XHTTP_NEXT_END,
		"HTTP weighted token list did not end"
	);
	iOffset = 0;
	testRequire(
		xrtHttpWeightedTokenNext(
			XRT_STR_LITERAL("gzip;level=9"),
			&iOffset,
			&Item
		) == XHTTP_NEXT_ERROR,
		"HTTP weighted token accepted an extension parameter"
	);
	xrtClearError();
	iOffset = 0;
	testRequire(
		xrtHttpWeightedTokenNext(
			XRT_STR_LITERAL("gzip;q=1.0000"),
			&iOffset,
			&Item
		) == XHTTP_NEXT_ERROR,
		"HTTP weighted token accepted an invalid qvalue"
	);
	xrtClearError();

	/* 公共解析器允许调用方使用打包结构中的未对齐输出。 */
	iQuality = UINT16_MAX;
	memcpy(pUnalignedQuality, &iQuality, sizeof(iQuality));
	testRequire(
		xrtHttpQualityParse(
			XRT_STR_LITERAL("0.375"), pUnalignedQuality
		),
		"HTTP qvalue rejected unaligned output"
	);
	memcpy(&iQuality, pUnalignedQuality, sizeof(iQuality));
	testRequire(
		iQuality == 375u,
		"HTTP qvalue unaligned output mismatch"
	);
	iOffset = 0;
	memcpy(pUnalignedOffset, &iOffset, sizeof(iOffset));
	testRequire(
		xrtHttpWeightedTokenNext(
			XRT_STR_LITERAL("br;q=0.6"),
			pUnalignedOffset,
			pUnalignedItem
		) == XHTTP_NEXT_ITEM,
		"HTTP weighted token rejected unaligned storage"
	);
	memcpy(&Item, pUnalignedItem, sizeof(Item));
	memcpy(&iOffset, pUnalignedOffset, sizeof(iOffset));
	testRequire(
		xrtHttpTokenEqual(
			Item.Token, XRT_STR_LITERAL("br")
		) && (Item.Quality == 600u) && (iOffset == 8u),
		"HTTP weighted token unaligned result mismatch"
	);

	/* 包装地址属于参数错误，不得访问或改写调用方输出。 */
	iQuality = UINT16_MAX;
	xrtClearError();
	testRequire(
		!xrtHttpQualityParse(Wrapped, &iQuality) &&
		(iQuality == UINT16_MAX) &&
		(xrtGetError() != NULL),
		"HTTP qvalue accepted a wrapped input range"
	);
	xrtClearError();
	iOffset = 0;
	memset(&Item, 0xA5, sizeof(Item));
	testRequire(
		xrtHttpWeightedTokenNext(
			XRT_STR_LITERAL("gzip"),
			&iOffset,
			(xhttpweightedtoken*)(uintptr_t)UINTPTR_MAX
		) == XHTTP_NEXT_ERROR,
		"HTTP weighted token accepted a wrapped output range"
	);
	testRequire(
		iOffset == 0,
		"HTTP weighted token argument error advanced the cursor"
	);
	xrtClearError();
}



/* 验证安全与幂等方法分类不把 POST 或扩展方法误判为可自动重试。 */
static void testHttpMethods(void)
{
	static const struct {
		xstrview Text;
		xhttpmethod Method;
	} Known[] = {
		{ XRT_STR_INIT("GET"), XHTTP_METHOD_GET },
		{ XRT_STR_INIT("HEAD"), XHTTP_METHOD_HEAD },
		{ XRT_STR_INIT("POST"), XHTTP_METHOD_POST },
		{ XRT_STR_INIT("PUT"), XHTTP_METHOD_PUT },
		{ XRT_STR_INIT("DELETE"), XHTTP_METHOD_DELETE },
		{ XRT_STR_INIT("CONNECT"), XHTTP_METHOD_CONNECT },
		{ XRT_STR_INIT("OPTIONS"), XHTTP_METHOD_OPTIONS },
		{ XRT_STR_INIT("TRACE"), XHTTP_METHOD_TRACE },
		{ XRT_STR_INIT("PATCH"), XHTTP_METHOD_PATCH }
	};
	static const xstrview Other[] = {
		XRT_STR_INIT("get"),
		XRT_STR_INIT("Get"),
		XRT_STR_INIT("GETX"),
		XRT_STR_INIT("P0ST"),
		XRT_STR_INIT("PURGE"),
		XRT_STR_INIT("QUERY")
	};
	size_t i;
	size_t j;

	for ( i = 0; i < sizeof(Known) / sizeof(Known[0]); i++ ) {
		testRequire(
			xrtHttpMethodParse(Known[i].Text) == Known[i].Method,
			"HTTP method parser missed a built-in method"
		);
		testRequire(
			(Known[i].Method != XHTTP_METHOD_INVALID) &&
			((Known[i].Method & (Known[i].Method - 1)) == 0),
			"HTTP method code is not a single bit"
		);
		testRequire(
			(XHTTP_METHOD_ANY & Known[i].Method) != 0,
			"HTTP any-method set missed a built-in method"
		);
		for ( j = i + 1u;
			j < sizeof(Known) / sizeof(Known[0]);
			j++ ) {
			testRequire(
				(Known[i].Method & Known[j].Method) == 0,
				"HTTP method codes share a bit"
			);
		}
	}
	testRequire(
		((XHTTP_METHOD_ANY & XHTTP_METHOD_OTHER) != 0) &&
		((XHTTP_METHOD_ANY & XHTTP_METHOD_INVALID) == 0),
		"HTTP any-method set mishandled OTHER or INVALID"
	);
	testRequire(
		((XHTTP_METHOD_CRUD & XHTTP_METHOD_GET) != 0) &&
		((XHTTP_METHOD_CRUD & XHTTP_METHOD_POST) != 0) &&
		((XHTTP_METHOD_CRUD & XHTTP_METHOD_PUT) != 0) &&
		((XHTTP_METHOD_CRUD & XHTTP_METHOD_PATCH) != 0) &&
		((XHTTP_METHOD_CRUD & XHTTP_METHOD_DELETE) != 0) &&
		((XHTTP_METHOD_CRUD & XHTTP_METHOD_HEAD) == 0) &&
		((XHTTP_METHOD_CRUD & XHTTP_METHOD_CONNECT) == 0) &&
		((XHTTP_METHOD_CRUD & XHTTP_METHOD_OPTIONS) == 0) &&
		((XHTTP_METHOD_CRUD & XHTTP_METHOD_TRACE) == 0) &&
		((XHTTP_METHOD_CRUD & XHTTP_METHOD_OTHER) == 0),
		"HTTP CRUD method set has incorrect members"
	);
	for ( i = 0; i < sizeof(Other) / sizeof(Other[0]); i++ ) {
		testRequire(
			xrtHttpMethodParse(Other[i]) == XHTTP_METHOD_OTHER,
			"HTTP method parser confused a valid extension method"
		);
	}
	testRequire(
		(xrtHttpMethodParse((xstrview){ NULL, 0 }) ==
			XHTTP_METHOD_INVALID) &&
		(xrtHttpMethodParse(XRT_STR_LITERAL("BAD METHOD")) ==
			XHTTP_METHOD_INVALID) &&
		(xrtHttpMethodParse(XRT_STR_LITERAL("BAD(METHOD")) ==
			XHTTP_METHOD_INVALID),
		"HTTP method parser accepted an empty or invalid token"
	);
	testRequire(
		xrtHttpMethodEqual(
			XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("GET")
		) &&
		!xrtHttpMethodEqual(
			XRT_STR_LITERAL("get"),
			XRT_STR_LITERAL("GET")
		) &&
		!xrtHttpMethodEqual(
			XRT_STR_LITERAL("BAD METHOD"),
			XRT_STR_LITERAL("BAD METHOD")
		),
		"HTTP method equality ignored case or token validity"
	);
	testRequire(
		xrtHttpMethodSafe(XRT_STR_LITERAL("GET")) &&
		xrtHttpMethodSafe(XRT_STR_LITERAL("HEAD")) &&
		xrtHttpMethodSafe(XRT_STR_LITERAL("OPTIONS")) &&
		xrtHttpMethodSafe(XRT_STR_LITERAL("TRACE")),
		"HTTP safe method classification missed a standard method"
	);
	testRequire(
		xrtHttpMethodIdempotent(XRT_STR_LITERAL("PUT")) &&
		xrtHttpMethodIdempotent(XRT_STR_LITERAL("DELETE")) &&
		xrtHttpMethodIdempotent(XRT_STR_LITERAL("GET")),
		"HTTP idempotent method classification missed a standard method"
	);
	testRequire(
		!xrtHttpMethodSafe(XRT_STR_LITERAL("POST")) &&
		!xrtHttpMethodIdempotent(XRT_STR_LITERAL("POST")) &&
		!xrtHttpMethodSafe(XRT_STR_LITERAL("get")) &&
		!xrtHttpMethodIdempotent(XRT_STR_LITERAL("delete")) &&
		!xrtHttpMethodSafe(XRT_STR_LITERAL("CUSTOM")) &&
		!xrtHttpMethodIdempotent(XRT_STR_LITERAL("CUSTOM")) &&
		!xrtHttpMethodSafe(XRT_STR_LITERAL("BAD METHOD")),
		"HTTP method classification accepted an unsafe or invalid method"
	);
}



/* 验证响应内容语义覆盖无正文状态、元数据响应和成功隧道。 */
static void testHttpResponseContent(void)
{
	testRequire(
		xrtHttpResponseContentAllowed(
			XRT_STR_LITERAL("GET"), 200
		) &&
		xrtHttpResponseContentAllowed(
			XRT_STR_LITERAL("POST"), 206
		) &&
		xrtHttpResponseContentAllowed(
			XRT_STR_LITERAL("CUSTOM"), 404
		),
		"HTTP response content predicate rejected an allowed response"
	);
	testRequire(
		!xrtHttpResponseContentAllowed(
			XRT_STR_LITERAL("HEAD"), 200
		) &&
		!xrtHttpResponseContentAllowed(
			XRT_STR_LITERAL("HEAD"), 404
		) &&
		!xrtHttpResponseContentAllowed(
			XRT_STR_LITERAL("GET"), 103
		) &&
		!xrtHttpResponseContentAllowed(
			XRT_STR_LITERAL("GET"), 204
		) &&
		!xrtHttpResponseContentAllowed(
			XRT_STR_LITERAL("GET"), 205
		) &&
		!xrtHttpResponseContentAllowed(
			XRT_STR_LITERAL("GET"), 304
		) &&
		!xrtHttpResponseContentAllowed(
			XRT_STR_LITERAL("CONNECT"), 200
		) &&
		!xrtHttpResponseContentAllowed(
			XRT_STR_LITERAL("CONNECT"), 299
		),
		"HTTP response content predicate missed a forbidden response"
	);
	testRequire(
		!xrtHttpResponseContentAllowed(
			XRT_STR_LITERAL("CONNECT"), 199
		) &&
		xrtHttpResponseContentAllowed(
			XRT_STR_LITERAL("CONNECT"), 300
		) &&
		xrtHttpResponseContentAllowed(
			XRT_STR_LITERAL("head"), 200
		) &&
		xrtHttpResponseContentAllowed(
			XRT_STR_LITERAL("connect"), 200
		),
		"HTTP response content predicate ignored method case or status class"
	);
	testRequire(
		!xrtHttpResponseContentAllowed(
			XRT_STR_LITERAL("BAD METHOD"), 200
		) &&
		!xrtHttpResponseContentAllowed(
			XRT_STR_LITERAL("GET"), 99
		) &&
		!xrtHttpResponseContentAllowed(
			XRT_STR_LITERAL("GET"), 1000
		),
		"HTTP response content predicate accepted invalid input"
	);
}



/* 验证 Content-Length 重复值只在数值完全一致时被接受。 */
static void testHttpContentLength(void)
{
	uint64 iLength = UINT64_MAX;

	testRequire(xrtHttpContentLengthParse(
		XRT_STR_LITERAL("42"), &iLength
	) && (iLength == 42), "HTTP Content-Length parse mismatch");
	testRequire(xrtHttpContentLengthParse(
		XRT_STR_LITERAL(" 42, 42\t"), &iLength
	) && (iLength == 42),
		"HTTP repeated Content-Length parse mismatch");
	testRequire(xrtHttpContentLengthParse(
		XRT_STR_LITERAL("18446744073709551615"), &iLength
	) && (iLength == UINT64_MAX),
		"HTTP maximum Content-Length was rejected");
	testRequire(!xrtHttpContentLengthParse(
		XRT_STR_LITERAL("42, 43"), &iLength
	) && (iLength == 0),
		"HTTP conflicting Content-Length was accepted");
	xrtClearError();
	testRequire(!xrtHttpContentLengthParse(
		XRT_STR_LITERAL("18446744073709551616"), &iLength
	) && (iLength == 0),
		"HTTP overflowing Content-Length was accepted");
	xrtClearError();
}



/* 验证字段查找不区分 ASCII 大小写，并支持重复字段遍历。 */
static void testHttpFieldLookup(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Accept"), XRT_STR_INIT("text/plain") },
		{ XRT_STR_INIT("Set-Cookie"), XRT_STR_INIT("a=1") },
		{ XRT_STR_INIT("set-cookie"), XRT_STR_INIT("b=2") }
	};
	size_t iFirst;
	size_t iSecond;
	const xhttpfield* pUnique = NULL;

	testRequire(xrtHttpFieldNameEqual(
		XRT_STR_LITERAL("CONTENT-LENGTH"),
		XRT_STR_LITERAL("content-length")
	), "HTTP field name comparison is not case insensitive");
	iFirst = xrtHttpFieldFind(
		Fields, 3, XRT_STR_LITERAL("SET-COOKIE"), 0
	);
	iSecond = xrtHttpFieldFind(
		Fields, 3, XRT_STR_LITERAL("set-cookie"), iFirst + 1u
	);
	testRequire((iFirst == 1) && (iSecond == 2) &&
		(xrtHttpFieldCount(
			Fields, 3, XRT_STR_LITERAL("Set-Cookie")
		) == 2), "HTTP duplicate field lookup mismatch");
	testRequire(xrtHttpFieldFind(
		Fields, 3, XRT_STR_LITERAL("missing"), 0
	) == XRT_NPOS, "HTTP missing field lookup mismatch");
	testRequire(xrtHttpFieldGet(
		Fields, 3, XRT_STR_LITERAL("accept")
	) == &Fields[0], "HTTP field get did not return the first match");
	testRequire(
		(xrtHttpFieldGetUnique(
			Fields,
			3,
			XRT_STR_LITERAL("Accept"),
			&pUnique
		 ) == XHTTP_NEXT_ITEM) &&
		(pUnique == &Fields[0]),
		"HTTP unique field lookup mismatch"
	);
	testRequire(
		xrtHttpFieldGetUnique(
			Fields,
			3,
			XRT_STR_LITERAL("missing"),
			&pUnique
		) == XHTTP_NEXT_END,
		"HTTP unique field missing result mismatch"
	);
	testRequire(
		xrtHttpFieldGetUnique(
			Fields,
			3,
			XRT_STR_LITERAL("Set-Cookie"),
			&pUnique
		) == XHTTP_NEXT_ERROR,
		"HTTP unique field lookup accepted duplicates"
	);
	testRequire(
		(pUnique == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP unique duplicate error mismatch"
	);
	xrtClearError();
}



/* 验证字段块迭代严格区分字段、结束和线路语法错误。 */
static void testHttpFieldBlock(void)
{
	static const xstrview Invalid[] = {
		XRT_STR_INIT("A: 1\nB: 2"),
		XRT_STR_INIT("A: 1\rB: 2"),
		XRT_STR_INIT("A: 1\r\n\r\nB: 2"),
		XRT_STR_INIT(" folded: value")
	};
	xstrview Block = XRT_STR_LITERAL(
		"Content-Type: text/plain\r\n"
		"X-Empty:\r\n"
		"X-Last: value"
	);
	xhttpfield Field;
	xhttpnext Next;
	size_t iOffset = 0;
	size_t iCount;
	size_t i;

	Next = xrtHttpFieldNext(Block, &iOffset, &Field);
	testRequire((Next == XHTTP_NEXT_ITEM) &&
		xrtHttpFieldNameEqual(
			Field.Name, XRT_STR_LITERAL("content-type")
		) && (Field.Value.Size == 10) &&
		(memcmp(Field.Value.Data, "text/plain", 10) == 0),
		"HTTP field block first item mismatch");
	Next = xrtHttpFieldNext(Block, &iOffset, &Field);
	testRequire((Next == XHTTP_NEXT_ITEM) &&
		xrtHttpFieldNameEqual(
			Field.Name, XRT_STR_LITERAL("x-empty")
		) && (Field.Value.Size == 0),
		"HTTP field block empty value mismatch");
	Next = xrtHttpFieldNext(Block, &iOffset, &Field);
	testRequire((Next == XHTTP_NEXT_ITEM) &&
		xrtHttpFieldNameEqual(
			Field.Name, XRT_STR_LITERAL("x-last")
		) && xrtHttpTokenEqual(
			Field.Value, XRT_STR_LITERAL("value")
		), "HTTP field block final item mismatch");
	testRequire(xrtHttpFieldNext(
		Block, &iOffset, &Field
	) == XHTTP_NEXT_END, "HTTP field block did not end exactly");
	testRequire(xrtHttpFieldBlockCount(Block, &iCount) &&
		(iCount == 3), "HTTP field block count mismatch");

	for ( i = 0; i < (sizeof(Invalid) / sizeof(Invalid[0])); i++ ) {
		testRequire(!xrtHttpFieldBlockCount(
			Invalid[i], &iCount
		), "HTTP field block accepted malformed syntax");
		xrtClearError();
	}
}



/* 验证借用字段写出支持精确计长、最终空行和失败原子性。 */
static void testHttpFieldWrite(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Content-Type"), XRT_STR_INIT("text/plain") },
		{ XRT_STR_INIT("X-Empty"), XRT_STR_INIT("") }
	};
	xhttpfield Invalid = {
		XRT_STR_LITERAL("X-Test"),
		XRT_STR_LITERAL("one\r\ntwo")
	};
	static const char ExpectedLine[] =
		"Content-Type: text/plain\r\n";
	static const char ExpectedBlock[] =
		"Content-Type: text/plain\r\n"
		"X-Empty: \r\n"
		"\r\n";
	struct {
		char Output[8];
		unsigned char Gap[8];
		size_t Size;
	} Short;
	char Output[96];
	char Before[96];
	size_t iRequired;
	size_t iSize;

	testRequire(xrtHttpFieldWrite(
		&Fields[0], Output, sizeof(Output), &iSize
	) && (iSize == (sizeof(ExpectedLine) - 1u)) &&
		(memcmp(Output, ExpectedLine, iSize) == 0),
		"HTTP field line write mismatch");
	testRequire(xrtHttpFieldBlockWrite(
		Fields, 2u, NULL, 0, &iRequired
	) && (iRequired == (sizeof(ExpectedBlock) - 1u)),
		"HTTP field block size query mismatch");
	testRequire(xrtHttpFieldBlockWrite(
		Fields, 2u, Output, sizeof(Output), &iSize
	) && (iSize == iRequired) &&
		(memcmp(Output, ExpectedBlock, iSize) == 0),
		"HTTP field block write mismatch");
	testRequire(xrtHttpFieldBlockWrite(
		NULL, 0, Output, sizeof(Output), &iSize
	) && (iSize == 2u) && (memcmp(Output, "\r\n", 2u) == 0),
		"HTTP empty field block write mismatch");

	memset(&Short, 0xA5, sizeof(Short));
	testRequire(!xrtHttpFieldBlockWrite(
		Fields, 2u, Short.Output, sizeof(Short.Output), &Short.Size
	) && (Short.Size == iRequired) &&
		((unsigned char)Short.Output[0] == UINT8_C(0xA5)) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HTTP short field block write was not atomic");
	xrtClearError();
	memset(Output, 0xA5, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	iSize = 123u;
	testRequire(!xrtHttpFieldWrite(
		&Invalid, Output, sizeof(Output), &iSize
	) && (iSize == 123u) &&
		(memcmp(Output, Before, sizeof(Output)) == 0),
		"HTTP invalid field write changed output");
	xrtClearError();
	iSize = 456u;
	testRequire(!xrtHttpFieldWrite(
		&Fields[0], (void*)Fields[0].Name.Data,
		Fields[0].Name.Size, &iSize
	) && (iSize == 456u) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP field writer accepted overlapping output");
	xrtClearError();
}



/* 验证字段固定描述符支持未对齐存储并拒绝地址回绕。 */
static void testHttpFieldMemoryContracts(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("X-Test"), XRT_STR_INIT("value") },
		{ XRT_STR_INIT("X-Other"), XRT_STR_INIT("two") }
	};
	static const char Expected[] =
		"X-Test: value\r\n"
		"X-Other: two\r\n"
		"\r\n";
	uint8 FieldsStorage[sizeof(Fields) + 2u];
	uint8 FieldStorage[sizeof(xhttpfield) + 2u];
	uint8 OffsetStorage[sizeof(size_t) + 2u];
	uint8 SizeStorage[sizeof(size_t) + 2u];
	uint8 PointerStorage[sizeof(const xhttpfield*) + 2u];
	const xhttpfield* pFields = (const xhttpfield*)(const void*)(
		FieldsStorage + 1u
	);
	xhttpfield Field;
	const xhttpfield* pFound;
	size_t iOffset = 0;
	size_t iSize;
	char Output[64];

	memset(FieldsStorage, 0xA5, sizeof(FieldsStorage));
	memset(FieldStorage, 0xA5, sizeof(FieldStorage));
	memset(OffsetStorage, 0xA5, sizeof(OffsetStorage));
	memset(SizeStorage, 0xA5, sizeof(SizeStorage));
	memset(PointerStorage, 0xA5, sizeof(PointerStorage));
	memcpy(FieldsStorage + 1u, Fields, sizeof(Fields));
	testRequire(
		(xrtHttpFieldFind(
			pFields, 2u, XRT_STR_LITERAL("x-other"), 0
		 ) == 1u) &&
		(xrtHttpFieldCount(
			pFields, 2u, XRT_STR_LITERAL("X-TEST")
		 ) == 1u) &&
		(xrtHttpFieldGet(
			pFields, 2u, XRT_STR_LITERAL("x-test")
		 ) == pFields),
		"HTTP field lookup rejected an unaligned descriptor array"
	);
	testRequire(xrtHttpFieldGetUnique(
		pFields,
		2u,
		XRT_STR_LITERAL("X-Other"),
		(const xhttpfield**)(void*)(PointerStorage + 1u)
	) == XHTTP_NEXT_ITEM,
		"HTTP unique field lookup rejected unaligned storage");
	memcpy(&pFound, PointerStorage + 1u, sizeof(pFound));
	testRequire(pFound == (pFields + 1u),
		"HTTP unique field lookup published the wrong descriptor");
	testRequire(xrtHttpFieldBlockWrite(
		pFields,
		2u,
		Output,
		sizeof(Output),
		(size_t*)(void*)(SizeStorage + 1u)
	), "HTTP field writer rejected unaligned descriptors");
	memcpy(&iSize, SizeStorage + 1u, sizeof(iSize));
	testRequire((iSize == (sizeof(Expected) - 1u)) &&
		(memcmp(Output, Expected, iSize) == 0),
		"HTTP field writer published the wrong unaligned result");
	testRequire(xrtHttpFieldParse(
		XRT_STR_LITERAL("X-Parsed: yes"),
		(xhttpfield*)(void*)(FieldStorage + 1u)
	), "HTTP field parser rejected an unaligned output");
	memcpy(&Field, FieldStorage + 1u, sizeof(Field));
	testRequire(xrtHttpFieldNameEqual(
		Field.Name, XRT_STR_LITERAL("x-parsed")
	) && xrtHttpTokenEqual(
		Field.Value, XRT_STR_LITERAL("yes")
	), "HTTP field parser published the wrong unaligned output");
	memcpy(OffsetStorage + 1u, &iOffset, sizeof(iOffset));
	testRequire(xrtHttpFieldNext(
		XRT_STR_LITERAL("A: one\r\nB: two"),
		(size_t*)(void*)(OffsetStorage + 1u),
		(xhttpfield*)(void*)(FieldStorage + 1u)
	) == XHTTP_NEXT_ITEM,
		"HTTP field iterator rejected unaligned outputs");
	memcpy(&iOffset, OffsetStorage + 1u, sizeof(iOffset));
	memcpy(&Field, FieldStorage + 1u, sizeof(Field));
	testRequire((iOffset == 8u) && xrtHttpTokenEqual(
		Field.Value, XRT_STR_LITERAL("one")
	), "HTTP field iterator published the wrong unaligned state");
	testRequire(
		(FieldsStorage[0] == 0xA5) &&
		(FieldsStorage[sizeof(FieldsStorage) - 1u] == 0xA5) &&
		(FieldStorage[0] == 0xA5) &&
		(FieldStorage[sizeof(FieldStorage) - 1u] == 0xA5) &&
		(OffsetStorage[0] == 0xA5) &&
		(OffsetStorage[sizeof(OffsetStorage) - 1u] == 0xA5) &&
		(SizeStorage[0] == 0xA5) &&
		(SizeStorage[sizeof(SizeStorage) - 1u] == 0xA5),
		"HTTP field operations wrote outside unaligned storage"
	);

	testRequire(!xrtHttpFieldParse(
		XRT_STR_LITERAL("A: one"),
		(xhttpfield*)(uintptr_t)(UINTPTR_MAX - 1u)
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP field parser accepted a wrapping output");
	xrtClearError();
	testRequire(xrtHttpFieldFind(
		(const xhttpfield*)(uintptr_t)(UINTPTR_MAX - 1u),
		1u,
		XRT_STR_LITERAL("A"),
		0
	) == XRT_NPOS,
		"HTTP field lookup accepted a wrapping descriptor array");
	xrtClearError();
	testRequire(xrtHttpFieldGetUnique(
		Fields,
		2u,
		XRT_STR_LITERAL("X-Test"),
		(const xhttpfield**)(uintptr_t)(UINTPTR_MAX - 1u)
	) == XHTTP_NEXT_ERROR,
		"HTTP unique field lookup accepted a wrapping output");
	xrtClearError();
	testRequire(!xrtHttpFieldBlockWrite(
		(const xhttpfield*)(uintptr_t)(UINTPTR_MAX - 1u),
		1u,
		Output,
		sizeof(Output),
		&iSize
	), "HTTP field writer accepted a wrapping descriptor array");
	xrtClearError();
	testRequire(!xrtHttpFieldBlockCount(
		XRT_STR_LITERAL("A: one"),
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP field block count accepted a wrapping output");
	xrtClearError();
}



/* 执行传输无关 HTTP 语义基础测试。 */
int main(void)
{
	testHttpStatus();
	testHttpValidation();
	testHttpTokens();
	testHttpTokenWrite();
	testHttpFieldTokens();
	testHttpWeights();
	testHttpMethods();
	testHttpResponseContent();
	testHttpContentLength();
	testHttpFieldLookup();
	testHttpFieldBlock();
	testHttpFieldWrite();
	testHttpFieldMemoryContracts();
	printf("[PASS] http\n");
	return 0;
}
