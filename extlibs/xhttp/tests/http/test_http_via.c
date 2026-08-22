#include "../test.h"

#include <xrt/http_via.h>



/* 按字节比较借用字符串视图。 */
static bool testViaText(xstrview Text, cstr sExpected)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		((iSize == 0) ||
		 (memcmp(Text.Data, sExpected, iSize) == 0));
}



/* 验证协议、端口、嵌套注释和注释内逗号。 */
static void testHttpViaElements(void)
{
	xstrview Value = XRT_STR_LITERAL(
		"1.0 fred, HTTP/1.1 p.example.net:8080 "
		"(edge, west), 2 _hidden (outer(inner)\\))"
	);
	xhttpviacursor Cursor;
	xhttpvia Via;
	char sComment[64];
	size_t iSize;

	testRequire(xrtHttpViaValid(Value),
		"valid Via field was rejected");
	xrtHttpViaCursorInit(&Cursor);
	testRequire(
		(xrtHttpViaNext(
			Value, &Cursor, &Via
		) == XHTTP_NEXT_ITEM) &&
		(Via.Flags == 0) &&
		testViaText(Via.ProtocolVersion, "1.0") &&
		testViaText(Via.Pseudonym, "fred"),
		"first Via element mismatch"
	);
	testRequire(
		(xrtHttpViaNext(
			Value, &Cursor, &Via
		) == XHTTP_NEXT_ITEM) &&
		(Via.Flags == (
			XHTTP_VIA_HAS_PROTOCOL_NAME |
			XHTTP_VIA_HAS_PORT |
			XHTTP_VIA_HAS_COMMENT
		)) && testViaText(Via.ProtocolName, "HTTP") &&
		testViaText(Via.ProtocolVersion, "1.1") &&
		testViaText(Via.ReceivedBy, "p.example.net:8080") &&
		testViaText(Via.Port, "8080") &&
		testViaText(Via.Comment, "(edge, west)"),
		"second Via element mismatch"
	);
	testRequire(
		(xrtHttpViaNext(
			Value, &Cursor, &Via
		) == XHTTP_NEXT_ITEM) &&
		xrtHttpViaCommentDecode(
			Via.Comment, sComment, sizeof(sComment), &iSize
		) && (iSize == 13u) &&
		(memcmp(sComment, "outer(inner))", 13u) == 0),
		"nested Via comment decode mismatch"
	);
	testRequire(
		xrtHttpViaNext(
			Value, &Cursor, &Via
		) == XHTTP_NEXT_END,
		"Via iterator did not end"
	);
}



/* 验证重复字段行保持线路顺序并忽略其他字段。 */
static void testHttpViaFields(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Via"),
			XRT_STR_INIT("1.0 first")
		},
		{
			XRT_STR_INIT("Content-Type"),
			XRT_STR_INIT("text/plain")
		},
		{
			XRT_STR_INIT("via"),
			XRT_STR_INIT("1.1 second, 2 third")
		}
	};
	xhttpviafieldcursor Cursor;
	xhttpvia Via;
	size_t iCount = 0;

	xrtHttpViaFieldCursorInit(&Cursor);
	while ( xrtHttpViaFieldNext(
		Fields, 3u, &Cursor, &Via
	) == XHTTP_NEXT_ITEM ) {
		iCount++;
	}
	testRequire(
		(iCount == 3u) && (xrtGetError() == NULL),
		"Via repeated field iteration mismatch"
	);
}



/* 验证 HTTP 列表空值和 URI 语法中的显式空端口。 */
static void testHttpViaBoundaries(void)
{
	static const xstrview Empty[] = {
		XRT_STR_INIT(""),
		XRT_STR_INIT(" \t"),
		XRT_STR_INIT(", ,")
	};
	xhttpviacursor Cursor;
	xhttpvia Via;
	size_t i;

	for ( i = 0; i < (sizeof(Empty) / sizeof(Empty[0])); i++ ) {
		testRequire(
			xrtHttpViaValid(Empty[i]),
			"empty Via list was rejected"
		);
		xrtHttpViaCursorInit(&Cursor);
		memset(&Via, 0xA5, sizeof(Via));
		testRequire(
			(xrtHttpViaNext(
				Empty[i], &Cursor, &Via
			) == XHTTP_NEXT_END) &&
			(Via.Element.Data == NULL) &&
			(Via.Element.Size == 0),
			"empty Via list did not end with cleared output"
		);
	}
	testRequire(
		xrtHttpViaElementParse(
			XRT_STR_LITERAL("1.1 edge:"), &Via
		) && ((Via.Flags & XHTTP_VIA_HAS_PORT) != 0) &&
		(Via.Port.Size == 0) &&
		testViaText(Via.ReceivedBy, "edge:"),
		"explicit empty Via port was not preserved"
	);
}



/* 验证单字段和重复字段游标不能在迭代中切换输入。 */
static void testHttpViaCursorBinding(void)
{
	static char ValueA[] = "1.0 first, 1.1 last";
	static char ValueB[] = "1.0 first, 1.1 last";
	static const xhttpfield FieldsA[] = {
		{
			XRT_STR_INIT("Via"),
			XRT_STR_INIT("1.0 first, 1.1 last")
		}
	};
	static const xhttpfield FieldsB[] = {
		{
			XRT_STR_INIT("Via"),
			XRT_STR_INIT("1.0 first, 1.1 last")
		}
	};
	xhttpviafieldcursor FieldCursor;
	xhttpviafieldcursor SavedFieldCursor;
	xhttpviacursor Cursor;
	xhttpviacursor SavedCursor;
	xhttpvia Via;

	xrtHttpViaCursorInit(&Cursor);
	testRequire(
		xrtHttpViaNext(
			XRT_STR_LITERAL(ValueA), &Cursor, &Via
		) == XHTTP_NEXT_ITEM,
		"Via source-binding setup failed"
	);
	SavedCursor = Cursor;
	memset(&Via, 0xA5, sizeof(Via));
	testRequire(
		(xrtHttpViaNext(
			XRT_STR_LITERAL(ValueB), &Cursor, &Via
		) == XHTTP_NEXT_ERROR) &&
		(memcmp(&Cursor, &SavedCursor, sizeof(Cursor)) == 0) &&
		(Via.Element.Data == NULL) &&
		(Via.Element.Size == 0),
		"Via cursor accepted an equal-size source switch"
	);
	xrtClearError();
	testRequire(
		(xrtHttpViaNext(
			(xstrview){ ValueA, sizeof(ValueA) - 2u },
			&Cursor,
			&Via
		) == XHTTP_NEXT_ERROR) &&
		(memcmp(&Cursor, &SavedCursor, sizeof(Cursor)) == 0),
		"Via cursor accepted a source-size change"
	);
	xrtClearError();

	xrtHttpViaFieldCursorInit(&FieldCursor);
	testRequire(
		xrtHttpViaFieldNext(
			FieldsA, 1u, &FieldCursor, &Via
		) == XHTTP_NEXT_ITEM,
		"Via field source-binding setup failed"
	);
	SavedFieldCursor = FieldCursor;
	memset(&Via, 0xA5, sizeof(Via));
	testRequire(
		(xrtHttpViaFieldNext(
			FieldsB, 1u, &FieldCursor, &Via
		) == XHTTP_NEXT_ERROR) &&
		(memcmp(
			&FieldCursor,
			&SavedFieldCursor,
			sizeof(FieldCursor)
		) == 0) &&
		(Via.Element.Data == NULL) &&
		(Via.Element.Size == 0),
		"Via field cursor accepted a source-array switch"
	);
	xrtClearError();
}



/* 验证畸形元素在发布任何部分结果前失败。 */
static void testHttpViaInvalid(void)
{
	static const cstr Invalid[] = {
		"HTTP/ edge",
		"1.1",
		"1.1 edge:port",
		"1.1 edge(comment)",
		"1.1 edge (open",
		"1.1 edge (bad\rcomment)",
		"1.1 edge (ok) tail"
	};
	size_t i;

	for ( i = 0; i < (sizeof(Invalid) / sizeof(Invalid[0])); i++ ) {
		testRequire(
			!xrtHttpViaValid((xstrview){
				Invalid[i], strlen(Invalid[i])
			}),
			"invalid Via field was accepted"
		);
		xrtClearError();
	}
}




/* 验证深层注释使用线性迭代扫描，不受调用栈深度限制。 */
static void testHttpViaDeepComment(void)
{
	enum { TEST_VIA_DEPTH = 4096 };
	static char sValue[12u + (TEST_VIA_DEPTH * 2u)];
	xhttpvia Via;
	size_t iSize;
	size_t i = 0;
	size_t j;

	memcpy(sValue + i, "1.1 edge ", 9u);
	i += 9u;
	for ( j = 0; j < TEST_VIA_DEPTH; j++ ) {
		sValue[i++] = '(';
	}
	sValue[i++] = 'x';
	for ( j = 0; j < TEST_VIA_DEPTH; j++ ) {
		sValue[i++] = ')';
	}
	testRequire(
		xrtHttpViaElementParse(
			(xstrview){ sValue, i }, &Via
		) && xrtHttpViaCommentDecode(
			Via.Comment, NULL, 0, &iSize
		) && (iSize == ((TEST_VIA_DEPTH * 2u) - 1u)),
		"deep Via comment validation or length query failed"
	);
}



int main(void)
{
	testHttpViaElements();
	testHttpViaFields();
	testHttpViaBoundaries();
	testHttpViaCursorBinding();
	testHttpViaInvalid();
	testHttpViaDeepComment();
	printf("[PASS] http_via\n");
	return 0;
}
