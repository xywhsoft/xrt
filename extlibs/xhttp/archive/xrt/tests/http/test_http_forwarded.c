#include "../test.h"

#include <xrt/http_forwarded.h>



/* 按字节比较借用视图。 */
static bool testForwardedViewEqual(
	xstrview Left,
	xstrview Right
)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 验证标准参数、扩展参数和 quoted-pair 解码。 */
static void testForwardedElements(void)
{
	xstrview Value = XRT_STR_LITERAL(
		"for=192.0.2.43, "
		"for=\"\\[2001:db8:cafe::17]:4711\";"
		"by=_edge;host=\"exa\\mple.com:8443\";"
		"proto=https;trace=\"a,b\""
	);
	xhttpforwardedcursor Cursor;
	xhttpforwarded Forwarded;
	xhttpparam Pair;
	char sHost[64];
	size_t iOffset;
	size_t iSize;

	testRequire(xrtHttpForwardedValid(Value),
		"valid Forwarded field was rejected");
	xrtHttpForwardedCursorInit(&Cursor);
	testRequire(
		(xrtHttpForwardedNext(
			Value, &Cursor, &Forwarded
		) == XHTTP_NEXT_ITEM) &&
		(Forwarded.Flags == XHTTP_FORWARDED_HAS_FOR) &&
		(Forwarded.PairCount == 1u) &&
		testForwardedViewEqual(
			Forwarded.For.Value,
			XRT_STR_LITERAL("192.0.2.43")
		),
		"first Forwarded element mismatch"
	);
	testRequire(
		(xrtHttpForwardedNext(
			Value, &Cursor, &Forwarded
		) == XHTTP_NEXT_ITEM) &&
		(Forwarded.Flags ==
		 (XHTTP_FORWARDED_HAS_FOR |
		  XHTTP_FORWARDED_HAS_BY |
		  XHTTP_FORWARDED_HAS_HOST |
		  XHTTP_FORWARDED_HAS_PROTO)) &&
		(Forwarded.PairCount == 5u),
		"second Forwarded element mismatch"
	);
	testRequire(
		xrtHttpParamValueWrite(
			&Forwarded.Host, sHost,
			sizeof(sHost), &iSize
		) && (iSize == 16u) &&
		(memcmp(sHost, "example.com:8443", 16u) == 0),
		"Forwarded escaped Host mismatch"
	);
	iOffset = 0;
	for ( ;; ) {
		xhttpnext Next = xrtHttpForwardedPairNext(
			Forwarded.Element, &iOffset, &Pair
		);

		testRequire(Next != XHTTP_NEXT_ERROR,
			"Forwarded pair iteration failed");
		if ( Next == XHTTP_NEXT_END ) {
			break;
		}
		if ( xrtHttpTokenEqual(
			Pair.Name, XRT_STR_LITERAL("trace")
		) ) {
			testRequire(
				(Pair.Flags & XHTTP_PARAM_QUOTED) != 0,
				"Forwarded extension quote flag missing"
			);
		}
	}
	testRequire(
		xrtHttpForwardedNext(
			Value, &Cursor, &Forwarded
		) == XHTTP_NEXT_END,
		"Forwarded iterator did not end"
	);
}



/* 验证公开节点、Host 和协议谓词的边界。 */
static void testForwardedValues(void)
{
	testRequire(
		xrtHttpForwardedNodeValid(
			XRT_STR_LITERAL("[2001:db8::1]:65535")
		) && xrtHttpForwardedNodeValid(
			XRT_STR_LITERAL("unknown:_hidden")
		) && xrtHttpForwardedNodeValid(
			XRT_STR_LITERAL("_private")
		) && !xrtHttpForwardedNodeValid(
			XRT_STR_LITERAL("999.0.0.1")
		) && !xrtHttpForwardedNodeValid(
			XRT_STR_LITERAL("unknown:123456")
		),
		"Forwarded node validation mismatch"
	);
	testRequire(
		xrtHttpForwardedHostValid(
			XRT_STR_LITERAL("example.com:443")
		) && xrtHttpForwardedHostValid(
			XRT_STR_LITERAL("[v1.future]:")
		) && xrtHttpForwardedHostValid(
			XRT_STR_LITERAL("")
		) && xrtHttpForwardedHostValid(
			XRT_STR_LITERAL(":12345678901234567890")
		) && !xrtHttpForwardedHostValid(
			XRT_STR_LITERAL("user@example.com")
		) && !xrtHttpForwardedHostValid(
			XRT_STR_LITERAL("[2001:db8::1")
		),
		"Forwarded Host validation mismatch"
	);
	testRequire(
		xrtHttpForwardedProtoValid(
			XRT_STR_LITERAL("web+tls")
		) && !xrtHttpForwardedProtoValid(
			XRT_STR_LITERAL("1http")
		),
		"Forwarded proto validation mismatch"
	);
}



/* 验证 RFC 可省略分号项、空元素和计数便利层。 */
static void testForwardedOptionalPairs(void)
{
	xstrview Value = XRT_STR_LITERAL(
		";for=192.0.2.1;;proto=https;, ;, for=_edge"
	);
	xhttpforwardedcursor Cursor;
	xhttpforwarded Forwarded;
	xhttpparam Pair;
	size_t iOffset = 0;
	size_t iCount = 77u;

	testRequire(
		xrtHttpForwardedCount(Value, &iCount) &&
		(iCount == 3u),
		"Forwarded optional-pair count mismatch"
	);
	testRequire(
		xrtHttpForwardedElementParse(
			XRT_STR_LITERAL(""), &Forwarded
		) && (Forwarded.PairCount == 0) &&
		(Forwarded.Flags == 0),
		"Forwarded rejected an empty forwarded-element"
	);
	testRequire(
		(xrtHttpForwardedPairNext(
			XRT_STR_LITERAL(";;"), &iOffset, &Pair
		) == XHTTP_NEXT_END) && (iOffset == 2u),
		"Forwarded pair iterator rejected omitted pairs"
	);
	xrtHttpForwardedCursorInit(&Cursor);
	testRequire(
		(xrtHttpForwardedNext(
			Value, &Cursor, &Forwarded
		) == XHTTP_NEXT_ITEM) &&
		(Forwarded.PairCount == 2u) &&
		(Forwarded.Flags ==
		 (XHTTP_FORWARDED_HAS_FOR |
		  XHTTP_FORWARDED_HAS_PROTO)),
		"Forwarded omitted pair changed the first element"
	);
	testRequire(
		(xrtHttpForwardedNext(
			Value, &Cursor, &Forwarded
		) == XHTTP_NEXT_ITEM) &&
		(Forwarded.PairCount == 0) &&
		(Forwarded.Flags == 0),
		"Forwarded empty element was not preserved"
	);
	testRequire(
		(xrtHttpForwardedNext(
			Value, &Cursor, &Forwarded
		) == XHTTP_NEXT_ITEM) &&
		(Forwarded.PairCount == 1u) &&
		(xrtHttpForwardedNext(
			Value, &Cursor, &Forwarded
		) == XHTTP_NEXT_END),
		"Forwarded optional-pair iteration mismatch"
	);
}



/* 验证重复字段行保持完整代理链路顺序。 */
static void testForwardedFields(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Forwarded"), XRT_STR_INIT("for=192.0.2.1") },
		{ XRT_STR_INIT("Other"), XRT_STR_INIT("ignored") },
		{ XRT_STR_INIT("forwarded"), XRT_STR_INIT("for=_second") }
	};
	xhttpforwardedfieldcursor Cursor;
	xhttpforwarded Forwarded;
	size_t iCount;

	testRequire(
		xrtHttpForwardedFieldCount(
			Fields, 3u, &iCount
		) && (iCount == 2u),
		"Forwarded repeated field count mismatch"
	);

	xrtHttpForwardedFieldCursorInit(&Cursor);
	testRequire(
		(xrtHttpForwardedFieldNext(
			Fields, 3u, &Cursor, &Forwarded
		) == XHTTP_NEXT_ITEM) &&
		testForwardedViewEqual(
			Forwarded.For.Value,
			XRT_STR_LITERAL("192.0.2.1")
		),
		"first repeated Forwarded field mismatch"
	);
	testRequire(
		(xrtHttpForwardedFieldNext(
			Fields, 3u, &Cursor, &Forwarded
		) == XHTTP_NEXT_ITEM) &&
		testForwardedViewEqual(
			Forwarded.For.Value,
			XRT_STR_LITERAL("_second")
		) && (xrtHttpForwardedFieldNext(
			Fields, 3u, &Cursor, &Forwarded
		) == XHTTP_NEXT_END),
		"second repeated Forwarded field mismatch"
	);
}



/* 验证预校验游标不能切换字段值或字段数组。 */
static void testForwardedCursorBinding(void)
{
	static const xhttpfield FirstFields[] = {
		{ XRT_STR_INIT("Forwarded"), XRT_STR_INIT("for=_first,for=_next") }
	};
	static const xhttpfield SecondFields[] = {
		{ XRT_STR_INIT("Forwarded"), XRT_STR_INIT("for=_other,for=_tail") }
	};
	xstrview First = XRT_STR_LITERAL("for=_first,for=_next");
	xstrview Second = XRT_STR_LITERAL("for=_other,for=_tail");
	xhttpforwardedcursor Cursor;
	xhttpforwardedcursor SavedCursor;
	xhttpforwardedfieldcursor FieldCursor;
	xhttpforwardedfieldcursor SavedFieldCursor;
	xhttpforwarded Forwarded;
	xhttpforwarded SavedForwarded;

	xrtHttpForwardedCursorInit(&Cursor);
	testRequire(
		xrtHttpForwardedNext(
			First, &Cursor, &Forwarded
		) == XHTTP_NEXT_ITEM,
		"Forwarded binding setup failed"
	);
	SavedCursor = Cursor;
	memset(&Forwarded, 0xA5, sizeof(Forwarded));
	SavedForwarded = Forwarded;
	xrtClearError();
	testRequire(
		(xrtHttpForwardedNext(
			Second, &Cursor, &Forwarded
		) == XHTTP_NEXT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(memcmp(&Cursor, &SavedCursor, sizeof(Cursor)) == 0) &&
		(memcmp(
			&Forwarded, &SavedForwarded, sizeof(Forwarded)
		) == 0),
		"Forwarded cursor switched value sources"
	);

	xrtClearError();
	xrtHttpForwardedFieldCursorInit(&FieldCursor);
	testRequire(
		xrtHttpForwardedFieldNext(
			FirstFields, 1u, &FieldCursor, &Forwarded
		) == XHTTP_NEXT_ITEM,
		"Forwarded field binding setup failed"
	);
	SavedFieldCursor = FieldCursor;
	memset(&Forwarded, 0x5A, sizeof(Forwarded));
	SavedForwarded = Forwarded;
	xrtClearError();
	testRequire(
		(xrtHttpForwardedFieldNext(
			SecondFields, 1u, &FieldCursor, &Forwarded
		) == XHTTP_NEXT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(memcmp(
			&FieldCursor, &SavedFieldCursor,
			sizeof(FieldCursor)
		) == 0) &&
		(memcmp(
			&Forwarded, &SavedForwarded, sizeof(Forwarded)
		) == 0),
		"Forwarded field cursor switched arrays"
	);
	xrtClearError();
}



/* 验证语法错误、重复参数和完整预校验的失败原子性。 */
static void testForwardedFailure(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Forwarded"), XRT_STR_INIT("for=_first") },
		{ XRT_STR_INIT("Forwarded"), XRT_STR_INIT("for=bad") }
	};
	xhttpforwardedcursor Cursor;
	xhttpforwardedcursor SavedCursor;
	xhttpforwarded Forwarded;
	xhttpforwarded SavedForwarded;
	size_t iCount = 77u;

	testRequire(
		!xrtHttpForwardedValid(
			XRT_STR_LITERAL("for =192.0.2.1")
		) && !xrtHttpForwardedValid(
			XRT_STR_LITERAL("for=192.0.2.1; by=_edge")
		),
		"Forwarded accepted forbidden pair whitespace"
	);
	xrtClearError();
	testRequire(
		!xrtHttpForwardedValid(
			XRT_STR_LITERAL("for=192.0.2.1;For=_duplicate")
		),
		"Forwarded accepted duplicate parameter"
	);
	xrtClearError();
	testRequire(
		!xrtHttpForwardedValid(
			XRT_STR_LITERAL("for=not-an-ip")
		) && !xrtHttpForwardedValid(
			XRT_STR_LITERAL("host=bad%GG")
		) && !xrtHttpForwardedValid(
			XRT_STR_LITERAL("proto=1http")
		),
		"Forwarded accepted invalid standard value"
	);
	xrtClearError();
	xrtHttpForwardedCursorInit(&Cursor);
	SavedCursor = Cursor;
	memset(&Forwarded, 0xA5, sizeof(Forwarded));
	SavedForwarded = Forwarded;
	testRequire(
		xrtHttpForwardedNext(
			XRT_STR_LITERAL("for=192.0.2.1, for=bad"),
			&Cursor, &Forwarded
		) == XHTTP_NEXT_ERROR,
		"Forwarded accepted invalid later element"
	);
	testRequire(
		(memcmp(&Cursor, &SavedCursor, sizeof(Cursor)) == 0) &&
		(memcmp(
			&Forwarded, &SavedForwarded, sizeof(Forwarded)
		) == 0),
		"Forwarded failure was not atomic"
	);
	xrtClearError();
	testRequire(
		!xrtHttpForwardedFieldCount(
			Fields, 2u, &iCount
		) && (iCount == 77u),
		"Forwarded field count failure was not atomic"
	);
	xrtClearError();
}



/* 验证未对齐对象、回绕范围和计数输出别名。 */
static void testForwardedMemory(void)
{
	union {
		uint64 Align;
		uint8 Bytes[sizeof(xhttpforwardedcursor) + 1u];
	} CursorStorage;
	union {
		uint64 Align;
		uint8 Bytes[sizeof(xhttpforwarded) + 1u];
	} ForwardedStorage;
	union {
		uint64 Align;
		uint8 Bytes[sizeof(size_t) + 1u];
	} CountStorage;
	xhttpforwardedcursor* pCursor =
		(xhttpforwardedcursor*)(CursorStorage.Bytes + 1u);
	xhttpforwarded* pForwarded =
		(xhttpforwarded*)(ForwardedStorage.Bytes + 1u);
	size_t* pCount = (size_t*)(CountStorage.Bytes + 1u);
	xstrview Value = XRT_STR_LITERAL("for=192.0.2.1");
	xhttpforwardedcursor Cursor;
	xhttpforwarded SavedForwarded;
	char sMutable[] = "for=192.0.2.1";
	char sBefore[sizeof(size_t)];
	size_t iCount;

	xrtHttpForwardedCursorInit(pCursor);
	testRequire(
		(xrtHttpForwardedNext(
			Value, pCursor, pForwarded
		) == XHTTP_NEXT_ITEM) &&
		xrtHttpForwardedCount(Value, pCount),
		"Forwarded rejected unaligned objects"
	);
	memcpy(&iCount, pCount, sizeof(iCount));
	testRequire(iCount == 1u,
		"Forwarded unaligned count mismatch");

	xrtHttpForwardedCursorInit(&Cursor);
	memset(pForwarded, 0xA5, sizeof(*pForwarded));
	memcpy(&SavedForwarded, pForwarded, sizeof(SavedForwarded));
	xrtClearError();
	testRequire(
		(xrtHttpForwardedNext(
			(xstrview){
				(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
			}, &Cursor, pForwarded
		) == XHTTP_NEXT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(memcmp(
			pForwarded, &SavedForwarded,
			sizeof(SavedForwarded)
		) == 0),
		"Forwarded accepted a wrapped value"
	);
	xrtClearError();
	memcpy(sBefore, sMutable, sizeof(sBefore));
	testRequire(
		!xrtHttpForwardedCount(
			(xstrview){ sMutable, sizeof(sMutable) - 1u },
			(size_t*)(void*)sMutable
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(memcmp(sMutable, sBefore, sizeof(sBefore)) == 0),
		"Forwarded count accepted source overlap"
	);
	xrtClearError();
	testRequire(
		!xrtHttpForwardedNodeValid((xstrview){
			(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
		}) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"Forwarded node validator accepted wrapped input"
	);
	xrtClearError();
	testRequire(
		!xrtHttpForwardedHostValid((xstrview){
			(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
		}) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"Forwarded Host validator accepted wrapped input"
	);
	xrtClearError();
	testRequire(
		!xrtHttpForwardedProtoValid((xstrview){
			(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
		}) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"Forwarded proto validator accepted wrapped input"
	);
	xrtClearError();
	xrtHttpForwardedCursorInit(
		(xhttpforwardedcursor*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"Forwarded cursor initializer accepted wrapped storage"
	);
	xrtClearError();
}



/* 验证长扩展链没有隐藏长度上限且仍保持完整去重语义。 */
static void testForwardedScale(void)
{
	char sValue[16384];
	xhttpforwarded Forwarded;
	size_t iOffset;
	size_t iCount;
	size_t i;
	int iWritten;

	memcpy(sValue, "for=192.0.2.1", 13u);
	iOffset = 13u;
	for ( i = 0; i < 1024u; i++ ) {
		iWritten = snprintf(
			sValue + iOffset, sizeof(sValue) - iOffset,
			";x%04u=v", (unsigned)i
		);
		testRequire(
			(iWritten > 0) &&
			((size_t)iWritten < (sizeof(sValue) - iOffset)),
			"Forwarded scale fixture overflowed"
		);
		iOffset += (size_t)iWritten;
	}
	testRequire(
		xrtHttpForwardedElementParse(
			(xstrview){ sValue, iOffset }, &Forwarded
		) && (Forwarded.PairCount == 1025u),
		"Forwarded rejected 1024 unique extensions"
	);
	testRequire(
		xrtHttpForwardedCount(
			(xstrview){ sValue, iOffset }, &iCount
		) && (iCount == 1u),
		"Forwarded scale count mismatch"
	);
}



/* 执行 RFC 7239 Forwarded 解析测试。 */
int main(void)
{
	testForwardedElements();
	testForwardedValues();
	testForwardedOptionalPairs();
	testForwardedFields();
	testForwardedCursorBinding();
	testForwardedFailure();
	testForwardedMemory();
	testForwardedScale();
	printf("[PASS] http_forwarded\n");
	return 0;
}
