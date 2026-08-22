#include "../test.h"

#include <xrt/http_proxy_status.h>



/* 按字节比较借用文本。 */
static bool testProxyAliasViewEqual(
	xstrview Left,
	xstrview Right
)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 验证 RFC 9532 基础列表、特殊字符和空列表。 */
static void testProxyAliasVectors(void)
{
	static const xstrview Expected[] = {
		XRT_STR_INIT("tracker.example"),
		XRT_STR_INIT("comma%2Cname.example"),
		XRT_STR_INIT("dot%5C.label.example"),
		XRT_STR_INIT("backslash%5C%5Cname.example")
	};
	xstrview Aliases = XRT_STR_LITERAL(
		"tracker.example,comma%2Cname.example,"
		"dot%5C.label.example,backslash%5C%5Cname.example"
	);
	xhttpproxyaliascursor Cursor;
	xstrview Alias;
	size_t i;

	testRequire(
		xrtHttpProxyAliasesValid(Aliases),
		"valid proxy aliases were rejected"
	);
	xrtHttpProxyAliasCursorInit(&Cursor);
	for ( i = 0; i < sizeof(Expected) / sizeof(Expected[0]); i++ ) {
		testRequire(
			(xrtHttpProxyAliasNext(
				Aliases, &Cursor, &Alias
			) == XHTTP_NEXT_ITEM) &&
			testProxyAliasViewEqual(Alias, Expected[i]),
			"proxy alias iterator value mismatch"
		);
	}
	testRequire(
		(xrtHttpProxyAliasNext(
			Aliases, &Cursor, &Alias
		) == XHTTP_NEXT_END) &&
		(Alias.Data == NULL) && (Alias.Size == 0),
		"proxy alias iterator did not end cleanly"
	);
	testRequire(
		xrtHttpProxyAliasesValid(XRT_STR_LITERAL("")),
		"empty proxy alias list was rejected"
	);
	xrtHttpProxyAliasCursorInit(&Cursor);
	testRequire(
		xrtHttpProxyAliasNext(
			XRT_STR_LITERAL(""), &Cursor, &Alias
		) == XHTTP_NEXT_END,
		"empty proxy alias list did not end"
	);
}



/* 验证解码保留 DNS 展示形式的句点与反斜杠转义。 */
static void testProxyAliasRead(void)
{
	static const struct {
		xstrview Encoded;
		xstrview Decoded;
	} Cases[] = {
		{
			XRT_STR_INIT("comma%2Cname.example"),
			XRT_STR_INIT("comma,name.example")
		},
		{
			XRT_STR_INIT("dot%5C.label.example"),
			XRT_STR_INIT("dot\\.label.example")
		},
		{
			XRT_STR_INIT("backslash%5C%5Cname.example"),
			XRT_STR_INIT("backslash\\\\name.example")
		}
	};
	char arrOutput[64];
	size_t iSize;
	size_t i;

	for ( i = 0; i < sizeof(Cases) / sizeof(Cases[0]); i++ ) {
		testRequire(
			xrtHttpProxyAliasRead(
				Cases[i].Encoded, NULL, 0, &iSize
			) && (iSize == Cases[i].Decoded.Size) &&
			xrtHttpProxyAliasRead(
				Cases[i].Encoded,
				arrOutput, sizeof(arrOutput), &iSize
			) && (iSize == Cases[i].Decoded.Size) &&
			(memcmp(
				arrOutput, Cases[i].Decoded.Data, iSize
			) == 0),
			"proxy alias decoded presentation mismatch"
		);
	}
}



/* 验证列表分隔、percent 格式和反斜杠规则。 */
static void testProxyAliasInvalid(void)
{
	static const xstrview Invalid[] = {
		XRT_STR_INIT(",alias.example"),
		XRT_STR_INIT("alias.example,"),
		XRT_STR_INIT("one.example,,two.example"),
		XRT_STR_INIT("bad value.example"),
		XRT_STR_INIT("bad/name.example"),
		XRT_STR_INIT("bad\\name.example"),
		XRT_STR_INIT("bad%"),
		XRT_STR_INIT("bad%2"),
		XRT_STR_INIT("bad%GG"),
		XRT_STR_INIT("bad%5Cx.example"),
		XRT_STR_INIT("bad%5C")
	};
	size_t i;

	for ( i = 0; i < sizeof(Invalid) / sizeof(Invalid[0]); i++ ) {
		xrtClearError();
		testRequire(
			!xrtHttpProxyAliasesValid(Invalid[i]) &&
			(xrtErrorKind(xrtGetError()) == XERR_VALUE),
			"invalid proxy alias list was accepted"
		);
	}
}



/* 验证首次完整预校验、失败原子性和未对齐状态。 */
static void testProxyAliasMemory(void)
{
	union {
		uint64 Align;
		uint8 Bytes[sizeof(xhttpproxyaliascursor) + 1u];
	} CursorStorage;
	union {
		uint64 Align;
		uint8 Bytes[sizeof(xstrview) + 1u];
	} AliasStorage;
	xhttpproxyaliascursor* pCursor =
		(xhttpproxyaliascursor*)(CursorStorage.Bytes + 1u);
	xstrview* pAlias = (xstrview*)(AliasStorage.Bytes + 1u);
	xhttpproxyaliascursor SavedCursor;
	xstrview SavedAlias;
	char arrOutput[16];
	char arrBefore[16];
	size_t iSize = 77u;

	xrtHttpProxyAliasCursorInit(pCursor);
	memset(pAlias, 0xA5, sizeof(*pAlias));
	memcpy(&SavedCursor, pCursor, sizeof(SavedCursor));
	memcpy(&SavedAlias, pAlias, sizeof(SavedAlias));
	testRequire(
		xrtHttpProxyAliasNext(
			XRT_STR_LITERAL("good.example,bad value"),
			pCursor, pAlias
		) == XHTTP_NEXT_ERROR,
		"proxy alias iterator accepted an invalid suffix"
	);
	testRequire(
		(memcmp(pCursor, &SavedCursor, sizeof(SavedCursor)) == 0) &&
		(memcmp(pAlias, &SavedAlias, sizeof(SavedAlias)) == 0),
		"proxy alias failure published partial state"
	);
	xrtClearError();
	xrtHttpProxyAliasCursorInit(pCursor);
	testRequire(
		(xrtHttpProxyAliasNext(
			XRT_STR_LITERAL("one.example,two.example"),
			pCursor, pAlias
		) == XHTTP_NEXT_ITEM),
		"proxy alias rejected unaligned cursor or output"
	);
	memset(arrOutput, 0xA5, sizeof(arrOutput));
	memcpy(arrBefore, arrOutput, sizeof(arrOutput));
	testRequire(
		!xrtHttpProxyAliasRead(
			XRT_STR_LITERAL("comma%2Cname"),
			arrOutput, 3u, &iSize
		) && (iSize == 10u) &&
		(memcmp(arrOutput, arrBefore, sizeof(arrOutput)) == 0),
		"short proxy alias read was not atomic"
	);
}



/* 验证游标来源绑定和初始化状态约束。 */
static void testProxyAliasCursorBinding(void)
{
	char arrFirst[] = "one.example,two.example";
	char arrOther[] = "six.example,ten.example";
	xhttpproxyaliascursor Cursor;
	xhttpproxyaliascursor SavedCursor;
	xstrview Alias;
	xstrview SavedAlias;

	xrtHttpProxyAliasCursorInit(&Cursor);
	testRequire(
		xrtHttpProxyAliasNext(
			(xstrview){ arrFirst, sizeof(arrFirst) - 1u },
			&Cursor, &Alias
		) == XHTTP_NEXT_ITEM,
		"proxy alias source binding setup failed"
	);
	SavedCursor = Cursor;
	SavedAlias = Alias;
	testRequire(
		xrtHttpProxyAliasNext(
			(xstrview){ arrOther, sizeof(arrOther) - 1u },
			&Cursor, &Alias
		) == XHTTP_NEXT_ERROR,
		"proxy alias cursor accepted another equal-size list"
	);
	testRequire(
		(memcmp(&Cursor, &SavedCursor, sizeof(Cursor)) == 0) &&
		(memcmp(&Alias, &SavedAlias, sizeof(Alias)) == 0),
		"proxy alias source mismatch was not atomic"
	);
	xrtClearError();
	testRequire(
		(xrtHttpProxyAliasNext(
			(xstrview){ arrFirst, sizeof(arrFirst) - 1u },
			&Cursor, &Alias
		) == XHTTP_NEXT_ITEM) &&
		testProxyAliasViewEqual(
			Alias, XRT_STR_LITERAL("two.example")
		),
		"proxy alias cursor could not resume its source"
	);

	xrtHttpProxyAliasCursorInit(&Cursor);
	Cursor.Offset = 1u;
	SavedCursor = Cursor;
	SavedAlias = Alias;
	testRequire(
		xrtHttpProxyAliasNext(
			(xstrview){ arrFirst, sizeof(arrFirst) - 1u },
			&Cursor, &Alias
		) == XHTTP_NEXT_ERROR,
		"proxy alias cursor accepted a nonzero initial offset"
	);
	testRequire(
		(memcmp(&Cursor, &SavedCursor, sizeof(Cursor)) == 0) &&
		(memcmp(&Alias, &SavedAlias, sizeof(Alias)) == 0),
		"proxy alias initial-state failure was not atomic"
	);
}



/* 运行 RFC 9532 next-hop-aliases 解析测试。 */
int main(void)
{
	testProxyAliasVectors();
	testProxyAliasRead();
	testProxyAliasInvalid();
	testProxyAliasMemory();
	testProxyAliasCursorBinding();
	printf("[PASS] http_proxy_alias\n");
	return 0;
}
