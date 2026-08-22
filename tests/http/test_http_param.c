#include "../test.h"



/* 验证参数迭代能正确处理引号内分号、转义和省略值。 */
static void testHttpParamParse(void)
{
	static const xstrview Invalid[] = {
		XRT_STR_INIT("; a=1"),
		XRT_STR_INIT("a=1;"),
		XRT_STR_INIT("a=1;; b=2"),
		XRT_STR_INIT("a="),
		XRT_STR_INIT("a=\"unterminated"),
		XRT_STR_INIT("a=\"bad\\\r\""),
		XRT_STR_INIT("a=\"ok\" tail")
	};
	xstrview Text = XRT_STR_LITERAL(
		"charset=UTF-8; boundary=\"a;b\\\"c\"; secure"
	);
	xhttpparam Param;
	xhttpnext Next;
	char Value[16];
	size_t iOffset = 0;
	size_t iCount;
	size_t iSize;
	size_t i;

	Next = xrtHttpParamNext(Text, &iOffset, &Param);
	testRequire((Next == XHTTP_NEXT_ITEM) &&
		xrtHttpTokenEqual(Param.Name, XRT_STR_LITERAL("charset")) &&
		(Param.Flags == XHTTP_PARAM_HAS_VALUE) &&
		xrtHttpTokenEqual(Param.Value, XRT_STR_LITERAL("UTF-8")),
		"HTTP parameter token value mismatch");
	Next = xrtHttpParamNext(Text, &iOffset, &Param);
	testRequire((Next == XHTTP_NEXT_ITEM) &&
		xrtHttpTokenEqual(Param.Name, XRT_STR_LITERAL("boundary")) &&
		((Param.Flags & XHTTP_PARAM_QUOTED) != 0) &&
		xrtHttpParamValueWrite(
			&Param, Value, sizeof(Value), &iSize
		) && (iSize == 5) &&
		(memcmp(Value, "a;b\"c", 5) == 0),
		"HTTP parameter quoted value mismatch");
	Next = xrtHttpParamNext(Text, &iOffset, &Param);
	testRequire((Next == XHTTP_NEXT_ITEM) &&
		xrtHttpTokenEqual(Param.Name, XRT_STR_LITERAL("secure")) &&
		(Param.Flags == XHTTP_PARAM_NONE),
		"HTTP parameter bare item mismatch");
	testRequire(xrtHttpParamNext(
		Text, &iOffset, &Param
	) == XHTTP_NEXT_END, "HTTP parameter list did not end exactly");
	testRequire(xrtHttpParamCount(Text, &iCount) && (iCount == 3),
		"HTTP parameter count mismatch");
	testRequire(xrtHttpParamFind(
		Text, XRT_STR_LITERAL("BOUNDARY"), &Param
	) == XHTTP_NEXT_ITEM &&
		((Param.Flags & XHTTP_PARAM_QUOTED) != 0),
		"HTTP parameter lookup mismatch");
	testRequire(xrtHttpParamFind(
		Text, XRT_STR_LITERAL("missing"), &Param
	) == XHTTP_NEXT_END, "HTTP missing parameter mismatch");

	for ( i = 0; i < (sizeof(Invalid) / sizeof(Invalid[0])); i++ ) {
		iOffset = 0;
		testRequire(xrtHttpParamNext(
			Invalid[i], &iOffset, &Param
		) == XHTTP_NEXT_ERROR,
			"HTTP parameter parser accepted malformed syntax");
		xrtClearError();
	}
}



/* 验证逗号指令支持空列表项、值、引号和完整后缀校验。 */
static void testHttpDirectiveParse(void)
{
	static const xstrview Invalid[] = {
		XRT_STR_INIT("max-age="),
		XRT_STR_INIT("=value"),
		XRT_STR_INIT("name=value tail"),
		XRT_STR_INIT("title=\"unterminated"),
		XRT_STR_INIT("title=\"bad\\\r\"")
	};
	xstrview Text = XRT_STR_LITERAL(
		", max-age=60, private, title=\"a,b\\\"c\", , "
		"no-transform,"
	);
	xhttpparam Directive;
	xhttpnext Next;
	char Value[16];
	size_t iOffset = 0;
	size_t iCount;
	size_t iSize;
	size_t i;

	Next = xrtHttpDirectiveNext(Text, &iOffset, &Directive);
	testRequire((Next == XHTTP_NEXT_ITEM) &&
		xrtHttpTokenEqual(
			Directive.Name, XRT_STR_LITERAL("max-age")
		) && (Directive.Flags == XHTTP_PARAM_HAS_VALUE) &&
		xrtHttpTokenEqual(
			Directive.Value, XRT_STR_LITERAL("60")
		),
		"HTTP directive token value mismatch");
	Next = xrtHttpDirectiveNext(Text, &iOffset, &Directive);
	testRequire((Next == XHTTP_NEXT_ITEM) &&
		xrtHttpTokenEqual(
			Directive.Name, XRT_STR_LITERAL("private")
		) && (Directive.Flags == XHTTP_PARAM_NONE),
		"HTTP bare directive mismatch");
	Next = xrtHttpDirectiveNext(Text, &iOffset, &Directive);
	testRequire((Next == XHTTP_NEXT_ITEM) &&
		xrtHttpTokenEqual(
			Directive.Name, XRT_STR_LITERAL("title")
		) && ((Directive.Flags & XHTTP_PARAM_QUOTED) != 0) &&
		xrtHttpParamValueWrite(
			&Directive, Value, sizeof(Value), &iSize
		) && (iSize == 5) &&
		(memcmp(Value, "a,b\"c", 5) == 0),
		"HTTP quoted directive mismatch");
	testRequire(xrtHttpDirectiveNext(
		Text, &iOffset, &Directive
	) == XHTTP_NEXT_ITEM &&
		xrtHttpTokenEqual(
			Directive.Name,
			XRT_STR_LITERAL("no-transform")
		),
		"HTTP empty directive members hid no-transform");
	testRequire(xrtHttpDirectiveNext(
		Text, &iOffset, &Directive
	) == XHTTP_NEXT_END,
		"HTTP directive list did not end");
	testRequire(xrtHttpDirectiveCount(Text, &iCount) &&
		(iCount == 4),
		"HTTP directive count mismatch");
	testRequire(xrtHttpDirectiveFind(
		Text,
		XRT_STR_LITERAL("NO-TRANSFORM"),
		&Directive
	) == XHTTP_NEXT_ITEM &&
		(Directive.Flags == XHTTP_PARAM_NONE),
		"HTTP directive lookup mismatch");

	for ( i = 0; i < (sizeof(Invalid) / sizeof(Invalid[0])); i++ ) {
		iOffset = 0;
		testRequire(xrtHttpDirectiveNext(
			Invalid[i], &iOffset, &Directive
		) == XHTTP_NEXT_ERROR &&
			(iOffset == 0),
			"HTTP directive parser accepted malformed syntax");
		xrtClearError();
	}
	testRequire(xrtHttpDirectiveFind(
		XRT_STR_LITERAL("no-transform, broken="),
		XRT_STR_LITERAL("no-transform"),
		&Directive
	) == XHTTP_NEXT_ERROR,
		"HTTP directive lookup ignored malformed suffix");
	xrtClearError();
}



/* 验证 quoted-string 与参数写出的长度查询和失败原子性。 */
static void testHttpParamWrite(void)
{
	union {
		xhttpparam Param;
		size_t Size;
	} Shared;
	char Text[64];
	char Small[4] = { 'x', 'x', 'x', 'x' };
	size_t iSize;
	str sBuilt;

	testRequire(xrtHttpQuotedWrite(
		XRT_STR_LITERAL("a\"b\\c"), Text, sizeof(Text), &iSize
	) && (iSize == 9) &&
		(memcmp(Text, "\"a\\\"b\\\\c\"", 9) == 0),
		"HTTP quoted-string write mismatch");
	testRequire(xrtHttpQuotedRead(
		(xstrview){ Text, iSize }, Text + 16, 16, &iSize
	) && (iSize == 5) &&
		(memcmp(Text + 16, "a\"b\\c", 5) == 0),
		"HTTP quoted-string read mismatch");
	testRequire(xrtHttpQuotedValid(XRT_STR_LITERAL("\"\"")) &&
		!xrtHttpQuotedValid(XRT_STR_LITERAL("\"bad\n\"")),
		"HTTP quoted-string validation mismatch");
	xrtClearError();
	testRequire(!xrtHttpQuotedWrite(
		XRT_STR_LITERAL("value"), NULL, 1, &iSize
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP quoted-string invalid output did not publish argument error");
	xrtClearError();
	testRequire(xrtHttpParamWrite(
		XRT_STR_LITERAL("filename"),
		XRT_STR_LITERAL("a b.txt"),
		XHTTP_PARAM_HAS_VALUE | XHTTP_PARAM_QUOTED,
		Text, sizeof(Text), &iSize
	) && (iSize == 18) &&
		(memcmp(Text, "filename=\"a b.txt\"", 18) == 0),
		"HTTP quoted parameter write mismatch");
	testRequire(xrtHttpParamWrite(
		XRT_STR_LITERAL("charset"),
		XRT_STR_LITERAL("UTF-8"),
		XHTTP_PARAM_HAS_VALUE,
		Text, sizeof(Text), &iSize
	) && (iSize == 13) &&
		(memcmp(Text, "charset=UTF-8", 13) == 0),
		"HTTP token parameter write mismatch");
	iSize = 0;
	testRequire(!xrtHttpParamWrite(
		XRT_STR_LITERAL("charset"),
		XRT_STR_LITERAL("UTF-8"),
		XHTTP_PARAM_HAS_VALUE,
		Small, sizeof(Small), &iSize
	) && (iSize == 13) &&
		(memcmp(Small, "xxxx", 4) == 0),
		"HTTP parameter short output was not atomic");
	xrtClearError();
	memset(&Shared, 0, sizeof(Shared));
	Shared.Param.Value = XRT_STR_LITERAL("UTF-8");
	Shared.Param.Flags = XHTTP_PARAM_HAS_VALUE;
	testRequire(!xrtHttpParamValueWrite(
		&Shared.Param, NULL, 0, &Shared.Size
	), "HTTP parameter size output overlapped metadata");
	xrtClearError();
	sBuilt = xrtHttpParamBuild(
		XRT_STR_LITERAL("secure"),
		(xstrview){ NULL, 0 },
		XHTTP_PARAM_NONE,
		&iSize
	);
	testRequire((sBuilt != NULL) && (iSize == 6) &&
		(strcmp(sBuilt, "secure") == 0),
		"HTTP parameter build mismatch");
	xrtFree(sBuilt);
}



/* 验证参数 token 语义校验能够正确处理 quoted-string 转义。 */
static void testHttpParamToken(void)
{
	xhttpparam Param;
	char Output[16];
	size_t iSize;
	const xerror* pBefore;

	memset(&Param, 0, sizeof(Param));
	Param.Value = XRT_STR_LITERAL("websocket");
	Param.Flags = XHTTP_PARAM_HAS_VALUE;
	testRequire(xrtHttpParamTokenValid(&Param) &&
		xrtHttpParamTokenEqual(
			&Param, XRT_STR_LITERAL("WebSocket")
		),
		"HTTP token parameter was rejected");

	Param.Value = XRT_STR_LITERAL("web\\socket");
	Param.Flags = XHTTP_PARAM_HAS_VALUE | XHTTP_PARAM_QUOTED;
	testRequire(xrtHttpParamTokenValid(&Param) &&
		xrtHttpParamTokenEqual(
			&Param, XRT_STR_LITERAL("WEBSOCKET")
		),
		"HTTP escaped token parameter was rejected");

	Param.Value = XRT_STR_LITERAL("web socket");
	testRequire(!xrtHttpParamTokenValid(&Param),
		"HTTP quoted space was accepted as token");

	Param.Value = XRT_STR_LITERAL("web\\\"socket");
	testRequire(!xrtHttpParamTokenValid(&Param),
		"HTTP escaped separator was accepted as token");

	Param.Value = XRT_STR_LITERAL("");
	testRequire(!xrtHttpParamTokenValid(&Param),
		"HTTP empty quoted value was accepted as token");

	Param.Value = XRT_STR_LITERAL("web\\");
	testRequire(!xrtHttpParamTokenValid(&Param),
		"HTTP incomplete escape was accepted as token");

	Param.Value = XRT_STR_LITERAL("websocket");
	Param.Flags = XHTTP_PARAM_NONE;
	testRequire(!xrtHttpParamTokenValid(&Param) &&
		!xrtHttpParamTokenValid(NULL),
		"HTTP valueless or null parameter was accepted as token");

	testRequire(
		!xrtHttpParamWrite(
			XRT_STR_LITERAL("bad name"),
			XRT_STR_LITERAL("value"),
			XHTTP_PARAM_HAS_VALUE,
			Output,
			sizeof(Output),
			&iSize
		),
		"HTTP parameter predicate error fixture failed"
	);
	pBefore = xrtGetError();
	Param.Value = XRT_STR_LITERAL("websocket");
	Param.Flags = XHTTP_PARAM_HAS_VALUE;
	testRequire(
		(pBefore != NULL) &&
		xrtHttpParamTokenValid(&Param) &&
		(xrtGetError() == pBefore),
		"HTTP parameter token predicate replaced the current error"
	);
	xrtClearError();
}



/* 验证参数语义值游标保留原始偏移并严格绑定输入。 */
static void testHttpParamValueCursor(void)
{
	xhttpparamvaluecursor Cursor;
	xhttpparam Param;
	xhttpparam Changed;
	xhttpnext Next;
	uint8 iByte;
	char Text[8];
	size_t iCount = 0;

	memset(&Param, 0, sizeof(Param));
	Param.Value = XRT_STR_LITERAL("a\\\"b");
	Param.Flags = XHTTP_PARAM_HAS_VALUE | XHTTP_PARAM_QUOTED;
	xrtHttpParamValueCursorInit(&Cursor);
	while ( (Next = xrtHttpParamValueNext(
		&Param, &Cursor, &iByte
	)) == XHTTP_NEXT_ITEM ) {
		Text[iCount++] = (char)iByte;
		if ( iCount == 1u ) {
			testRequire(Cursor.Offset == 1u,
				"HTTP parameter cursor lost plain raw offset");
		} else if ( iCount == 2u ) {
			testRequire(Cursor.Offset == 3u,
				"HTTP parameter cursor lost quoted-pair raw offset");
		}
	}
	testRequire((Next == XHTTP_NEXT_END) && (iCount == 3u) &&
		(memcmp(Text, "a\"b", 3u) == 0) &&
		(Cursor.Offset == Param.Value.Size),
		"HTTP parameter cursor decoded the wrong semantic value");

	Changed = Param;
	Changed.Value = XRT_STR_LITERAL("other");
	iByte = 0xA5u;
	testRequire(xrtHttpParamValueNext(
		&Changed, &Cursor, &iByte
	) == XHTTP_NEXT_ERROR && (iByte == 0xA5u),
		"HTTP parameter cursor accepted a rebound descriptor");
	xrtClearError();

	memset(&Param, 0, sizeof(Param));
	Param.Value = (xstrview){ NULL, 0 };
	Param.Flags = XHTTP_PARAM_HAS_VALUE | XHTTP_PARAM_QUOTED;
	xrtHttpParamValueCursorInit(&Cursor);
	iByte = 0xA5u;
	testRequire(xrtHttpParamValueNext(
		&Param, &Cursor, &iByte
	) == XHTTP_NEXT_END && (iByte == 0u) &&
		(Cursor.Validated == 1u),
		"HTTP parameter cursor rejected an empty quoted value");

	memset(&Cursor, 0, sizeof(Cursor));
	Cursor.Offset = 1u;
	iByte = 0xA5u;
	testRequire(xrtHttpParamValueNext(
		&Param, &Cursor, &iByte
	) == XHTTP_NEXT_ERROR && (iByte == 0xA5u),
		"HTTP parameter cursor accepted dirty initial state");
	xrtClearError();
}



/* 验证固定描述符支持未对齐存储，并在访问前拒绝回绕地址。 */
static void testHttpParamMemoryContracts(void)
{
	uint8 OffsetStorage[sizeof(size_t) + 2u];
	uint8 ParamStorage[sizeof(xhttpparam) + 2u];
	uint8 SizeStorage[sizeof(size_t) + 2u];
	size_t* pOffset = (size_t*)(void*)(OffsetStorage + 1u);
	xhttpparam* pParam =
		(xhttpparam*)(void*)(ParamStorage + 1u);
	size_t* pSize = (size_t*)(void*)(SizeStorage + 1u);
	xstrview Parameters = XRT_STR_LITERAL("a=1; b=\"two\"");
	xstrview Directives = XRT_STR_LITERAL("max-age=60, private");
	xhttpparam Param;
	const xerror* pBefore;
	char Output[32];
	size_t iOffset = 0;
	size_t iSize;
	str sBuilt;

	memset(OffsetStorage, 0xA5, sizeof(OffsetStorage));
	memset(ParamStorage, 0xA5, sizeof(ParamStorage));
	memset(SizeStorage, 0xA5, sizeof(SizeStorage));
	memcpy(pOffset, &iOffset, sizeof(iOffset));
	testRequire(xrtHttpParamNext(
		Parameters, pOffset, pParam
	) == XHTTP_NEXT_ITEM,
		"HTTP parameter iterator rejected unaligned outputs");
	memcpy(&iOffset, pOffset, sizeof(iOffset));
	memcpy(&Param, pParam, sizeof(Param));
	testRequire((iOffset == 4u) && xrtHttpTokenEqual(
		Param.Name, XRT_STR_LITERAL("a")
	) && xrtHttpTokenEqual(
		Param.Value, XRT_STR_LITERAL("1")
	), "HTTP parameter iterator published wrong unaligned state");
	testRequire(xrtHttpParamCount(Parameters, pSize),
		"HTTP parameter count rejected unaligned output");
	memcpy(&iSize, pSize, sizeof(iSize));
	testRequire(iSize == 2u,
		"HTTP parameter count published wrong unaligned value");
	testRequire(xrtHttpParamFind(
		Parameters, XRT_STR_LITERAL("B"), pParam
	) == XHTTP_NEXT_ITEM,
		"HTTP parameter lookup rejected unaligned output");
	memcpy(&Param, pParam, sizeof(Param));
	testRequire((Param.Flags == (
		XHTTP_PARAM_HAS_VALUE | XHTTP_PARAM_QUOTED
	)) && xrtHttpTokenEqual(
		Param.Name, XRT_STR_LITERAL("b")
	), "HTTP parameter lookup published wrong unaligned value");

	testRequire(xrtHttpDirectiveCount(Directives, pSize),
		"HTTP directive count rejected unaligned output");
	memcpy(&iSize, pSize, sizeof(iSize));
	testRequire((iSize == 2u) && (xrtHttpDirectiveFind(
		Directives, XRT_STR_LITERAL("PRIVATE"), pParam
	) == XHTTP_NEXT_ITEM),
		"HTTP directive lookup rejected unaligned output");

	testRequire(xrtHttpQuotedWrite(
		XRT_STR_LITERAL("a\"b"), Output, sizeof(Output), pSize
	), "HTTP quoted writer rejected unaligned size output");
	memcpy(&iSize, pSize, sizeof(iSize));
	testRequire((iSize == 6u) && xrtHttpQuotedRead(
		(xstrview){ Output, iSize }, Output + 8u, 8u, pSize
	), "HTTP quoted reader rejected unaligned size output");
	memcpy(&iSize, pSize, sizeof(iSize));
	testRequire((iSize == 3u) &&
		(memcmp(Output + 8u, "a\"b", 3u) == 0),
		"HTTP quoted reader published wrong unaligned result");

	memset(&Param, 0, sizeof(Param));
	Param.Value = XRT_STR_LITERAL("web\\socket");
	Param.Flags = XHTTP_PARAM_HAS_VALUE | XHTTP_PARAM_QUOTED;
	memcpy(pParam, &Param, sizeof(Param));
	testRequire(xrtHttpParamTokenValid(pParam) &&
		xrtHttpParamTokenEqual(
			pParam, XRT_STR_LITERAL("WebSocket")
		) &&
		xrtHttpParamValueWrite(
			pParam, Output, sizeof(Output), pSize
		), "HTTP parameter descriptor rejected unaligned storage");
	memcpy(&iSize, pSize, sizeof(iSize));
	testRequire((iSize == 9u) &&
		(memcmp(Output, "websocket", 9u) == 0),
		"HTTP parameter value published wrong unaligned result");
	testRequire(xrtHttpParamWrite(
		XRT_STR_LITERAL("name"), XRT_STR_LITERAL("value"),
		XHTTP_PARAM_HAS_VALUE, Output, sizeof(Output), pSize
	), "HTTP parameter writer rejected unaligned size output");
	memcpy(&iSize, pSize, sizeof(iSize));
	testRequire((iSize == 10u) &&
		(memcmp(Output, "name=value", 10u) == 0),
		"HTTP parameter writer published wrong unaligned result");
	sBuilt = xrtHttpQuotedBuild(XRT_STR_LITERAL("value"), pSize);
	testRequire(sBuilt != NULL,
		"HTTP quoted builder rejected unaligned size output");
	xrtFree(sBuilt);
	sBuilt = xrtHttpParamBuild(
		XRT_STR_LITERAL("name"), XRT_STR_LITERAL("value"),
		XHTTP_PARAM_HAS_VALUE, pSize
	);
	testRequire(sBuilt != NULL,
		"HTTP parameter builder rejected unaligned size output");
	xrtFree(sBuilt);
	testRequire(
		(OffsetStorage[0] == 0xA5) &&
		(OffsetStorage[sizeof(OffsetStorage) - 1u] == 0xA5) &&
		(ParamStorage[0] == 0xA5) &&
		(ParamStorage[sizeof(ParamStorage) - 1u] == 0xA5) &&
		(SizeStorage[0] == 0xA5) &&
		(SizeStorage[sizeof(SizeStorage) - 1u] == 0xA5),
		"HTTP parameter operations wrote outside unaligned storage"
	);

	iOffset = 0;
	testRequire(xrtHttpParamNext(
		XRT_STR_LITERAL("broken="), &iOffset, &Param
	) == XHTTP_NEXT_ERROR && (iOffset == 0u) &&
		(Param.Name.Data == NULL) && (Param.Value.Data == NULL) &&
		(Param.Flags == XHTTP_PARAM_NONE),
		"HTTP parameter syntax failure published partial state");
	xrtClearError();
	iSize = 77u;
	testRequire(!xrtHttpParamCount(
		XRT_STR_LITERAL("broken="), &iSize
	) && (iSize == 0u),
		"HTTP parameter count failure did not publish zero");
	xrtClearError();
	iOffset = Parameters.Size + 1u;
	memset(&Param, 0xA5, sizeof(Param));
	testRequire(xrtHttpParamNext(
		Parameters, &iOffset, &Param
	) == XHTTP_NEXT_ERROR &&
		(iOffset == (Parameters.Size + 1u)) &&
		(Param.Name.Data == NULL) && (Param.Value.Data == NULL) &&
		(Param.Flags == XHTTP_PARAM_NONE),
		"HTTP parameter invalid offset published partial state");
	xrtClearError();

	iOffset = 0;
	testRequire(xrtHttpParamNext(
		Parameters,
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u),
		&Param
	) == XHTTP_NEXT_ERROR,
		"HTTP parameter iterator accepted wrapping offset");
	xrtClearError();
	testRequire(xrtHttpParamNext(
		Parameters,
		&iOffset,
		(xhttpparam*)(uintptr_t)(UINTPTR_MAX - 1u)
	) == XHTTP_NEXT_ERROR,
		"HTTP parameter iterator accepted wrapping result");
	xrtClearError();
	testRequire(!xrtHttpParamCount(
		Parameters, (size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP parameter count accepted wrapping output");
	xrtClearError();
	testRequire(xrtHttpParamFind(
		Parameters,
		XRT_STR_LITERAL("a"),
		(xhttpparam*)(uintptr_t)(UINTPTR_MAX - 1u)
	) == XHTTP_NEXT_ERROR,
		"HTTP parameter lookup accepted wrapping output");
	xrtClearError();
	testRequire(!xrtHttpDirectiveCount(
		Directives, (size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP directive count accepted wrapping output");
	xrtClearError();
	testRequire(xrtHttpDirectiveFind(
		Directives,
		XRT_STR_LITERAL("private"),
		(xhttpparam*)(uintptr_t)(UINTPTR_MAX - 1u)
	) == XHTTP_NEXT_ERROR,
		"HTTP directive lookup accepted wrapping output");
	xrtClearError();
	testRequire(!xrtHttpQuotedWrite(
		XRT_STR_LITERAL("value"),
		(void*)(uintptr_t)(UINTPTR_MAX - 1u),
		7u,
		&iSize
	), "HTTP quoted writer accepted wrapping output");
	xrtClearError();
	testRequire(!xrtHttpQuotedRead(
		XRT_STR_LITERAL("\"v\""),
		Output,
		sizeof(Output),
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP quoted reader accepted wrapping size output");
	xrtClearError();
	testRequire(xrtHttpQuotedBuild(
		XRT_STR_LITERAL("value"),
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
	) == NULL, "HTTP quoted builder accepted wrapping size output");
	xrtClearError();
	testRequire(xrtHttpParamBuild(
		XRT_STR_LITERAL("name"), XRT_STR_LITERAL("value"),
		XHTTP_PARAM_HAS_VALUE,
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
	) == NULL, "HTTP parameter builder accepted wrapping size output");
	xrtClearError();
	testRequire(!xrtHttpParamValueWrite(
		(const xhttpparam*)(uintptr_t)(UINTPTR_MAX - 1u),
		Output,
		sizeof(Output),
		&iSize
	), "HTTP parameter value accepted wrapping descriptor");
	xrtClearError();
	memset(&Param, 0, sizeof(Param));
	Param.Value = (xstrview){
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
	};
	Param.Flags = XHTTP_PARAM_HAS_VALUE;
	testRequire(!xrtHttpParamValueWrite(
		&Param, Output, sizeof(Output), &iSize
	), "HTTP parameter value accepted wrapping borrowed text");
	xrtClearError();
	Param.Value = XRT_STR_LITERAL("value");
	testRequire(!xrtHttpParamValueWrite(
		&Param,
		Output,
		sizeof(Output),
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP parameter value accepted wrapping size output");
	xrtClearError();
	testRequire(!xrtHttpParamValueWrite(
		&Param,
		(void*)(uintptr_t)(UINTPTR_MAX - 1u),
		5u,
		&iSize
	), "HTTP parameter value accepted wrapping output");
	xrtClearError();
	testRequire(!xrtHttpParamWrite(
		XRT_STR_LITERAL("name"), XRT_STR_LITERAL("value"),
		XHTTP_PARAM_HAS_VALUE,
		(void*)(uintptr_t)(UINTPTR_MAX - 1u),
		10u,
		&iSize
	), "HTTP parameter writer accepted wrapping output");
	xrtClearError();

	testRequire(!xrtHttpParamWrite(
		XRT_STR_LITERAL("bad name"), XRT_STR_LITERAL("value"),
		XHTTP_PARAM_HAS_VALUE, Output, sizeof(Output), &iSize
	), "HTTP predicate preservation fixture failed");
	pBefore = xrtGetError();
	Param.Value = (xstrview){
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
	};
	Param.Flags = XHTTP_PARAM_HAS_VALUE;
	testRequire((pBefore != NULL) &&
		!xrtHttpParamTokenValid(
			(const xhttpparam*)(uintptr_t)(UINTPTR_MAX - 1u)
		) && !xrtHttpParamTokenValid(&Param) &&
		(xrtGetError() == pBefore),
		"HTTP parameter predicate changed error for wrapping input");
	xrtClearError();
}



/* 执行 HTTP 参数和 quoted-string 契约测试。 */
int main(void)
{
	testHttpParamParse();
	testHttpDirectiveParse();
	testHttpParamWrite();
	testHttpParamToken();
	testHttpParamValueCursor();
	testHttpParamMemoryContracts();
	printf("[PASS] http_param\n");
	return 0;
}
