#include "../test.h"

#include <xrt/http_expect.h>



/* 比较借用视图与固定文本。 */
static bool testHttpExpectText(
	xstrview Text,
	cstr sExpected
)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		(memcmp(Text.Data, sExpected, iSize) == 0);
}



/* 验证单元素、扩展值、quoted-string 和 RFC 参数语法。 */
static void testHttpExpectationParse(void)
{
	static const xstrview Invalid[] = {
		XRT_STR_INIT(""),
		XRT_STR_INIT("name =value"),
		XRT_STR_INIT("name= value"),
		XRT_STR_INIT("name=value; p =x"),
		XRT_STR_INIT("name=value; p= x"),
		XRT_STR_INIT("name=value; p"),
		XRT_STR_INIT("name=\"open"),
		XRT_STR_INIT("name=value trailing")
	};
	xhttpexpectation Expectation;
	size_t i;

	testRequire(xrtHttpExpectationParse(
		XRT_STR_LITERAL(" 100-continue\t"), &Expectation
	) && testHttpExpectText(
		Expectation.Name, "100-continue"
	) && (Expectation.Flags == XHTTP_EXPECT_BARE) &&
		(Expectation.Value.Size == 0) &&
		(Expectation.Parameters.Size == 0),
		"HTTP Expect bare expectation mismatch");
	testRequire(xrtHttpExpectationParse(
		XRT_STR_LITERAL(
			"feature=\"a,b\" ; level=2; ; mode=\"x\\\"y\";"
		),
		&Expectation
	) && testHttpExpectText(
		Expectation.Element,
		"feature=\"a,b\" ; level=2; ; mode=\"x\\\"y\";"
	) && testHttpExpectText(
		Expectation.Name, "feature"
	) && testHttpExpectText(
		Expectation.Value, "\"a,b\""
	) && testHttpExpectText(
		Expectation.Parameters,
		"; level=2; ; mode=\"x\\\"y\";"
	) && ((Expectation.Flags &
		(uint32)XHTTP_EXPECT_HAS_VALUE) != 0) &&
	((Expectation.Flags &
		(uint32)XHTTP_EXPECT_VALUE_QUOTED) != 0) &&
	((Expectation.Flags &
		(uint32)XHTTP_EXPECT_HAS_PARAMETERS) != 0),
		"HTTP Expect extension parse mismatch");

	for ( i = 0; i < (sizeof(Invalid) / sizeof(Invalid[0])); i++ ) {
		testRequire(!xrtHttpExpectationParse(
			Invalid[i], &Expectation
		), "HTTP Expect parser accepted malformed syntax");
		testRequire(
			(Expectation.Element.Data == NULL) &&
			(Expectation.Name.Data == NULL),
			"HTTP Expect parser published a partial result"
		);
		xrtClearError();
	}
}



/* 验证单字段列表在首项发布前完整校验后缀。 */
static void testHttpExpectList(void)
{
	xstrview Value = XRT_STR_LITERAL(
		", 100-continue, feature=\"a,b\"; mode=fast,,"
	);
	xhttpexpectcursor Cursor;
	xhttpexpectcursor Before;
	xhttpexpectation Expectation;
	size_t iCount;

	testRequire(xrtHttpExpectCount(Value, &iCount) &&
		(iCount == 2u) && xrtHttpExpectValid(Value),
		"HTTP Expect list count mismatch");
	xrtHttpExpectCursorInit(&Cursor);
	testRequire((xrtHttpExpectNext(
		Value, &Cursor, &Expectation
	) == XHTTP_NEXT_ITEM) && testHttpExpectText(
		Expectation.Name, "100-continue"
	), "HTTP Expect list first item mismatch");
	testRequire((xrtHttpExpectNext(
		Value, &Cursor, &Expectation
	) == XHTTP_NEXT_ITEM) && testHttpExpectText(
		Expectation.Name, "feature"
	) && testHttpExpectText(
		Expectation.Value, "\"a,b\""
	), "HTTP Expect list extension mismatch");
	testRequire(xrtHttpExpectNext(
		Value, &Cursor, &Expectation
	) == XHTTP_NEXT_END,
		"HTTP Expect list did not end exactly");

	xrtHttpExpectCursorInit(&Cursor);
	Before = Cursor;
	memset(&Expectation, 0xA5, sizeof(Expectation));
	testRequire((xrtHttpExpectNext(
		XRT_STR_LITERAL("100-continue, bad expectation"),
		&Cursor,
		&Expectation
	) == XHTTP_NEXT_ERROR) &&
		(memcmp(&Cursor, &Before, sizeof(Cursor)) == 0) &&
		(Expectation.Element.Data == NULL) &&
		(Expectation.Name.Data == NULL),
		"HTTP Expect list published before tail validation");
	xrtClearError();
	testRequire(xrtHttpExpectCount(
		XRT_STR_LITERAL(",,\t,"), &iCount
	) && (iCount == 0),
		"HTTP Expect list rejected empty members");
}



/* 验证重复字段迭代、分类和未知扩展保留。 */
static void testHttpExpectFields(void)
{
	static const xhttpfield ContinueFields[] = {
		{ XRT_STR_INIT("Expect"), XRT_STR_INIT("100-continue") },
		{ XRT_STR_INIT("X-Other"), XRT_STR_INIT("ignored") },
		{ XRT_STR_INIT("expect"), XRT_STR_INIT(",100-CONTINUE,") }
	};
	static const xhttpfield ExtensionFields[] = {
		{ XRT_STR_INIT("Expect"), XRT_STR_INIT("100-continue") },
		{ XRT_STR_INIT("Expect"), XRT_STR_INIT("feature=on") }
	};
	static const xhttpfield InvalidFields[] = {
		{ XRT_STR_INIT("Expect"), XRT_STR_INIT("100-continue") },
		{ XRT_STR_INIT("Expect"), XRT_STR_INIT("bad value") }
	};
	xhttpexpectfieldcursor Cursor;
	xhttpexpectfieldcursor Before;
	xhttpexpectation Expectation;

	testRequire(xrtHttpExpectFields(
		NULL, 0
	) == XHTTP_EXPECT_NONE,
		"HTTP Expect missing field classification mismatch");
	testRequire(xrtHttpExpectFields(
		ContinueFields, 3u
	) == XHTTP_EXPECT_CONTINUE,
		"HTTP Expect repeated continue classification mismatch");
	testRequire(xrtHttpExpectFields(
		ExtensionFields, 2u
	) == XHTTP_EXPECT_UNSUPPORTED,
		"HTTP Expect extension classification mismatch");
	testRequire(xrtHttpExpectFields(
		InvalidFields, 2u
	) == XHTTP_EXPECT_ERROR,
		"HTTP Expect malformed field classification mismatch");
	xrtClearError();

	xrtHttpExpectFieldCursorInit(&Cursor);
	Before = Cursor;
	memset(&Expectation, 0xA5, sizeof(Expectation));
	testRequire((xrtHttpExpectFieldNext(
		InvalidFields, 2u, &Cursor, &Expectation
	) == XHTTP_NEXT_ERROR) &&
		(memcmp(&Cursor, &Before, sizeof(Cursor)) == 0) &&
		(Expectation.Element.Data == NULL),
		"HTTP Expect repeated fields published before validation");
	xrtClearError();
}



/* 验证固定描述符、游标和输出支持未对齐存储。 */
static void testHttpExpectUnaligned(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Expect"), XRT_STR_INIT("100-continue") }
	};
	uint8 FieldsStorage[sizeof(Fields) + 1u];
	uint8 CursorStorage[sizeof(xhttpexpectfieldcursor) + 1u];
	uint8 OutputStorage[sizeof(xhttpexpectation) + 1u];
	xhttpexpectation Expectation;

	memcpy(FieldsStorage + 1u, Fields, sizeof(Fields));
	xrtHttpExpectFieldCursorInit(
		(xhttpexpectfieldcursor*)(void*)(CursorStorage + 1u)
	);
	testRequire(xrtHttpExpectFieldNext(
		(const xhttpfield*)(const void*)(FieldsStorage + 1u),
		1u,
		(xhttpexpectfieldcursor*)(void*)(CursorStorage + 1u),
		(xhttpexpectation*)(void*)(OutputStorage + 1u)
	) == XHTTP_NEXT_ITEM,
		"HTTP Expect rejected unaligned storage");
	memcpy(&Expectation, OutputStorage + 1u, sizeof(Expectation));
	testRequire(testHttpExpectText(
		Expectation.Name, "100-continue"
	), "HTTP Expect unaligned output mismatch");
}



/* 验证所有公开入口都会拒绝发生地址回绕的借用输入。 */
static void testHttpExpectWrappedRange(void)
{
	xstrview Wrapped = {
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
	};
	xhttpexpectcursor Cursor;
	xhttpexpectation Expectation;
	size_t iCount = 77u;

	testRequire(!xrtHttpExpectationParse(
		Wrapped, &Expectation
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Expect parser accepted a wrapped input range");
	xrtClearError();
	testRequire(!xrtHttpExpectValid(Wrapped) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Expect validator accepted a wrapped input range");
	xrtClearError();
	testRequire(!xrtHttpExpectCount(Wrapped, &iCount) &&
		(iCount == 77u) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Expect counter accepted a wrapped input range");
	xrtClearError();
	xrtHttpExpectCursorInit(&Cursor);
	testRequire((xrtHttpExpectNext(
		Wrapped, &Cursor, &Expectation
	) == XHTTP_NEXT_ERROR) &&
		(Cursor.Offset == 0) && (Cursor.Validated == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Expect iterator accepted a wrapped input range");
	xrtClearError();
}



/* 执行传输无关 Expect 协议测试。 */
int main(void)
{
	testHttpExpectationParse();
	testHttpExpectList();
	testHttpExpectFields();
	testHttpExpectUnaligned();
	testHttpExpectWrappedRange();
	puts("[PASS] http_expect");
	return 0;
}
