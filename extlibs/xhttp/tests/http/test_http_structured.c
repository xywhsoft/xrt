#include "../test.h"

#include <xrt/http_structured.h>



/* 按字节比较两个借用视图。 */
static bool testStructuredViewEqual(xstrview Left, xstrview Right)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 严格读取一个完整裸值，测试失败时直接终止。 */
static xhttpstructuredbare testStructuredBare(xstrview Value)
{
	xhttpstructuredbare Bare;
	size_t iOffset = 0;

	testRequire(
		(xrtHttpStructuredBareNext(
			Value, &iOffset, &Bare
		) == XHTTP_NEXT_ITEM) && (iOffset == Value.Size),
		"structured bare value parse failed"
	);
	return Bare;
}



/* 验证 key 与 token 使用不同的大小写和字符集合。 */
static void testStructuredSyntax(void)
{
	testRequire(
		xrtHttpStructuredKeyValid(XRT_STR_LITERAL("a_b-2.*")) &&
		xrtHttpStructuredKeyValid(XRT_STR_LITERAL("*")) &&
		!xrtHttpStructuredKeyValid(XRT_STR_LITERAL("A")) &&
		!xrtHttpStructuredKeyValid(XRT_STR_LITERAL("2a")),
		"structured key syntax mismatch"
	);
	testRequire(
		xrtHttpStructuredTokenValid(
			XRT_STR_LITERAL("HTTP/1.1:ok")
		) && !xrtHttpStructuredTokenValid(
			XRT_STR_LITERAL("2bad")
		) && !xrtHttpStructuredTokenValid(
			XRT_STR_LITERAL("bad value")
		),
		"structured token syntax mismatch"
	);
}



/* 验证 Integer、Decimal、Boolean 与 Date 的数值模型和边界。 */
static void testStructuredNumbers(void)
{
	xhttpstructuredbare Bare;
	xhttpstructureditem Item;

	Bare = testStructuredBare(
		XRT_STR_LITERAL("999999999999999")
	);
	testRequire(
		(Bare.Type == XHTTP_STRUCTURED_INTEGER) &&
		(Bare.Number == INT64_C(999999999999999)),
		"structured integer upper boundary mismatch"
	);
	Bare = testStructuredBare(XRT_STR_LITERAL("-0"));
	testRequire(
		(Bare.Type == XHTTP_STRUCTURED_INTEGER) &&
		(Bare.Number == 0),
		"structured signed zero mismatch"
	);
	Bare = testStructuredBare(XRT_STR_LITERAL("-12.34"));
	testRequire(
		(Bare.Type == XHTTP_STRUCTURED_DECIMAL) &&
		(Bare.Number == -12340),
		"structured decimal fixed-point mismatch"
	);
	Bare = testStructuredBare(XRT_STR_LITERAL("?1"));
	testRequire(
		(Bare.Type == XHTTP_STRUCTURED_BOOLEAN) &&
		(Bare.Number == 1),
		"structured Boolean mismatch"
	);
	Bare = testStructuredBare(XRT_STR_LITERAL("@1659578233"));
	testRequire(
		(Bare.Type == XHTTP_STRUCTURED_DATE) &&
		(Bare.Number == INT64_C(1659578233)),
		"structured Date mismatch"
	);
	testRequire(
		!xrtHttpStructuredItemParse(
			XRT_STR_LITERAL("1000000000000000"), &Item
		) && !xrtHttpStructuredItemParse(
			XRT_STR_LITERAL("1234567890123.1"), &Item
		) && !xrtHttpStructuredItemParse(
			XRT_STR_LITERAL("1.0000"), &Item
		) && !xrtHttpStructuredItemParse(
			XRT_STR_LITERAL("@1.0"), &Item
		),
		"structured numeric boundary accepted invalid input"
	);
	xrtClearError();
}



/* 验证 String、Byte Sequence 和 Display String 的解码。 */
static void testStructuredDecodedValues(void)
{
	static const unsigned char ExpectedUtf8[] = {
		'T', 'h', 'i', 's', ' ', 0xC3u, 0xBCu, 's'
	};
	xhttpstructuredbare Bare;
	unsigned char arrOutput[32];
	size_t iSize;

	Bare = testStructuredBare(
		XRT_STR_LITERAL("\"a\\\\b\\\"c\"")
	);
	testRequire(
		xrtHttpStructuredStringDecode(
			&Bare, NULL, 0, &iSize
		) && (iSize == 5u) &&
		xrtHttpStructuredStringDecode(
			&Bare, (char*)arrOutput,
			sizeof(arrOutput), &iSize
		) && (memcmp(arrOutput, "a\\b\"c", 5u) == 0),
		"structured String decoding mismatch"
	);

	Bare = testStructuredBare(XRT_STR_LITERAL(":YWJjZA:"));
	testRequire(
		xrtHttpStructuredBytesDecode(
			&Bare, arrOutput, sizeof(arrOutput), &iSize
		) && (iSize == 4u) &&
		(memcmp(arrOutput, "abcd", 4u) == 0),
		"structured unpadded Byte Sequence mismatch"
	);
	Bare = testStructuredBare(XRT_STR_LITERAL(":YQ=:"));
	testRequire(
		xrtHttpStructuredBytesDecode(
			&Bare, arrOutput, sizeof(arrOutput), &iSize
		) && (iSize == 1u) && (arrOutput[0] == 'a'),
		"structured partial Base64 padding mismatch"
	);

	Bare = testStructuredBare(
		XRT_STR_LITERAL("%\"This %c3%bcs\"")
	);
	testRequire(
		xrtHttpStructuredDisplayDecode(
			&Bare, (char*)arrOutput,
			sizeof(arrOutput), &iSize
		) && (iSize == sizeof(ExpectedUtf8)) &&
		(memcmp(
			arrOutput, ExpectedUtf8, sizeof(ExpectedUtf8)
		) == 0),
		"structured Display String decoding mismatch"
	);
}



/* 验证解码入口支持未对齐描述符和长度输出，并拒绝环绕地址。 */
static void testStructuredDecodedMemory(void)
{
	union {
		uint64 Align;
		uint8 Bytes[sizeof(xhttpstructuredbare) + 1u];
	} BareStorage;
	union {
		uint64 Align;
		uint8 Bytes[sizeof(size_t) + 1u];
	} SizeStorage;
	xhttpstructuredbare Bare;
	const xhttpstructuredbare* pBare =
		(const xhttpstructuredbare*)(BareStorage.Bytes + 1u);
	size_t* pSize = (size_t*)(SizeStorage.Bytes + 1u);
	char arrOutput[16];
	size_t iSize;

	Bare = testStructuredBare(XRT_STR_LITERAL("\"a\\\\b\""));
	memcpy(BareStorage.Bytes + 1u, &Bare, sizeof(Bare));
	testRequire(
		xrtHttpStructuredStringDecode(
			pBare, arrOutput, sizeof(arrOutput), pSize
		),
		"structured String rejected unaligned memory"
	);
	memcpy(&iSize, pSize, sizeof(iSize));
	testRequire(
		(iSize == 3u) && (memcmp(arrOutput, "a\\b", 3u) == 0),
		"structured unaligned String result mismatch"
	);

	Bare = testStructuredBare(XRT_STR_LITERAL(":YWI=:"));
	memcpy(BareStorage.Bytes + 1u, &Bare, sizeof(Bare));
	testRequire(
		xrtHttpStructuredBytesDecode(
			pBare, arrOutput, sizeof(arrOutput), pSize
		),
		"structured Bytes rejected unaligned memory"
	);
	memcpy(&iSize, pSize, sizeof(iSize));
	testRequire(
		(iSize == 2u) && (memcmp(arrOutput, "ab", 2u) == 0),
		"structured unaligned Bytes result mismatch"
	);

	Bare = testStructuredBare(XRT_STR_LITERAL("%\"ok\""));
	memcpy(BareStorage.Bytes + 1u, &Bare, sizeof(Bare));
	testRequire(
		xrtHttpStructuredDisplayDecode(
			pBare, arrOutput, sizeof(arrOutput), pSize
		),
		"structured Display rejected unaligned memory"
	);
	memcpy(&iSize, pSize, sizeof(iSize));
	testRequire(
		(iSize == 2u) && (memcmp(arrOutput, "ok", 2u) == 0),
		"structured unaligned Display result mismatch"
	);

	testRequire(
		!xrtHttpStructuredDisplayDecode(
			pBare, (char*)(uintptr_t)(UINTPTR_MAX - 1u),
			4u, pSize
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"structured decoder accepted wrapped output"
	);
	xrtClearError();
	testRequire(
		!xrtHttpStructuredDisplayDecode(
			(const xhttpstructuredbare*)(uintptr_t)(
				UINTPTR_MAX - 1u
			), NULL, 0, pSize
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"structured decoder accepted wrapped descriptor"
	);
	xrtClearError();
	testRequire(
		!xrtHttpStructuredDisplayDecode(
			pBare, NULL, 0,
			(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"structured decoder accepted wrapped length output"
	);
	xrtClearError();
}



/* 验证非数值裸值描述符不能携带伪造的数值载荷。 */
static void testStructuredDecodedCoherence(void)
{
	xhttpstructuredbare Bare;
	char arrOutput[16];
	size_t iSize;

	Bare = testStructuredBare(XRT_STR_LITERAL("\"ok\""));
	Bare.Number = 1;
	iSize = 77u;
	xrtClearError();
	testRequire(
		!xrtHttpStructuredStringDecode(
			&Bare, arrOutput, sizeof(arrOutput), &iSize
		) && (xrtErrorKind(xrtGetError()) == XERR_VALUE) &&
		(iSize == 77u),
		"structured String accepted an incoherent descriptor"
	);

	Bare = testStructuredBare(XRT_STR_LITERAL(":YQ==:"));
	Bare.Number = 1;
	iSize = 77u;
	xrtClearError();
	testRequire(
		!xrtHttpStructuredBytesDecode(
			&Bare, arrOutput, sizeof(arrOutput), &iSize
		) && (xrtErrorKind(xrtGetError()) == XERR_VALUE) &&
		(iSize == 77u),
		"structured Bytes accepted an incoherent descriptor"
	);

	Bare = testStructuredBare(XRT_STR_LITERAL("%\"ok\""));
	Bare.Number = 1;
	iSize = 77u;
	xrtClearError();
	testRequire(
		!xrtHttpStructuredDisplayDecode(
			&Bare, arrOutput, sizeof(arrOutput), &iSize
		) && (xrtErrorKind(xrtGetError()) == XERR_VALUE) &&
		(iSize == 77u),
		"structured Display accepted an incoherent descriptor"
	);
	xrtClearError();
}



/* 验证参数作为有序 map 暴露，并让重复 key 的最后值生效。 */
static void testStructuredParameters(void)
{
	xhttpstructureditem Item;
	xhttpstructuredparameter Parameter;
	size_t iOffset = 0;

	testRequire(
		xrtHttpStructuredItemParse(
			XRT_STR_LITERAL("abc;a=1;b;a=2"), &Item
		),
		"structured Item with parameters failed"
	);
	testRequire(
		(xrtHttpStructuredParameterCount(
			Item.Parameters
		) == 2u) &&
		(xrtHttpStructuredParameterAt(
			Item.Parameters, 0, &Parameter
		) == XHTTP_NEXT_ITEM) &&
		testStructuredViewEqual(Parameter.Key, XRT_STR_LITERAL("a")) &&
		(Parameter.Value.Number == 2),
		"structured parameter index overwrite mismatch"
	);
	testRequire(
		(xrtHttpStructuredParameterFind(
			Item.Parameters, XRT_STR_LITERAL("b"), &Parameter
		) == XHTTP_NEXT_ITEM) &&
		(Parameter.Value.Type == XHTTP_STRUCTURED_BOOLEAN) &&
		(Parameter.Value.Number == 1),
		"structured omitted parameter value mismatch"
	);
	testRequire(
		(xrtHttpStructuredParameterNext(
			Item.Parameters, &iOffset, &Parameter
		) == XHTTP_NEXT_ITEM) &&
		(Parameter.Value.Number == 1),
		"structured wire parameter order mismatch"
	);
}



/* 验证 Inner List 的项目参数、空列表和空格分隔规则。 */
static void testStructuredInnerList(void)
{
	xhttpstructuredmember Member;
	xhttpstructureditem Item;
	size_t iOffset = 0;
	size_t iInner = 0;

	testRequire(
		(xrtHttpStructuredListNext(
			XRT_STR_LITERAL("(\"foo\";a=1 bar);lvl=5, ()"),
			&iOffset, &Member
		) == XHTTP_NEXT_ITEM) &&
		(Member.Kind == XHTTP_STRUCTURED_MEMBER_INNER_LIST),
		"structured Inner List member failed"
	);
	testRequire(
		(xrtHttpStructuredInnerNext(
			Member.Inner, &iInner, &Item
		) == XHTTP_NEXT_ITEM) &&
		(Item.Bare.Type == XHTTP_STRUCTURED_STRING) &&
		(xrtHttpStructuredInnerNext(
			Member.Inner, &iInner, &Item
		) == XHTTP_NEXT_ITEM) &&
		(Item.Bare.Type == XHTTP_STRUCTURED_TOKEN) &&
		(xrtHttpStructuredInnerNext(
			Member.Inner, &iInner, &Item
		) == XHTTP_NEXT_END),
		"structured Inner List iteration mismatch"
	);
	testRequire(
		(xrtHttpStructuredListNext(
			XRT_STR_LITERAL("(\"foo\";a=1 bar);lvl=5, ()"),
			&iOffset, &Member
		) == XHTTP_NEXT_ITEM) &&
		(Member.Inner.Size == 0) &&
		(xrtHttpStructuredListNext(
			XRT_STR_LITERAL("(\"foo\";a=1 bar);lvl=5, ()"),
			&iOffset, &Member
		) == XHTTP_NEXT_END),
		"structured empty Inner List mismatch"
	);
	testRequire(
		!xrtHttpStructuredListValid(
			XRT_STR_LITERAL("(a\tb)")
		),
		"structured Inner List accepted HTAB separator"
	);
	xrtClearError();
}



/* 验证 Dictionary 的省略 true、Inner List 和重复 key 语义。 */
static void testStructuredDictionary(void)
{
	xstrview Value = XRT_STR_LITERAL(
		"a=?0, b, c;foo=bar, a=9, d=(5 6);valid"
	);
	xhttpstructureddictionarymember Member;
	size_t iOffset = 0;

	testRequire(
		xrtHttpStructuredDictionaryValid(Value) &&
		(xrtHttpStructuredDictionaryCount(Value) == 4u),
		"structured Dictionary count mismatch"
	);
	testRequire(
		(xrtHttpStructuredDictionaryAt(
			Value, 0, &Member
		) == XHTTP_NEXT_ITEM) &&
		testStructuredViewEqual(Member.Key, XRT_STR_LITERAL("a")) &&
		(Member.Member.Bare.Number == 9),
		"structured Dictionary indexed overwrite mismatch"
	);
	testRequire(
		(xrtHttpStructuredDictionaryFind(
			Value, XRT_STR_LITERAL("c"), &Member
		) == XHTTP_NEXT_ITEM) &&
		(Member.Member.Bare.Type == XHTTP_STRUCTURED_BOOLEAN) &&
		(Member.Member.Bare.Number == 1) &&
		(Member.Member.Parameters.Size != 0),
		"structured Dictionary omitted true mismatch"
	);
	testRequire(
		(xrtHttpStructuredDictionaryFind(
			Value, XRT_STR_LITERAL("d"), &Member
		) == XHTTP_NEXT_ITEM) &&
		(Member.Member.Kind == XHTTP_STRUCTURED_MEMBER_INNER_LIST),
		"structured Dictionary Inner List mismatch"
	);
	while ( xrtHttpStructuredDictionaryNext(
		Value, &iOffset, &Member
	) == XHTTP_NEXT_ITEM ) {
	}
	testRequire(
		iOffset == Value.Size,
		"structured Dictionary wire iteration did not end"
	);
}



/* 验证有序 Map 游标集中实现重复 key 的覆盖语义和来源绑定。 */
static void testStructuredDictionaryMap(void)
{
	xstrview Value = XRT_STR_LITERAL(
		"a=?0, b, a=9, c=token"
	);
	xstrview Other = XRT_STR_LITERAL(
		"d=?0, e, d=8, f=token"
	);
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Priority"), XRT_STR_INIT("u=3") },
		{ XRT_STR_INIT("priority"), XRT_STR_INIT("i, u=1") }
	};
	union {
		uint64 Align;
		uint8 Bytes[sizeof(xhttpstructuredmapcursor) + 1u];
	} CursorStorage;
	union {
		uint64 Align;
		uint8 Bytes[
			sizeof(xhttpstructureddictionarymember) + 1u
		];
	} MemberStorage;
	xhttpstructuredmapcursor* pUnalignedCursor =
		(xhttpstructuredmapcursor*)(CursorStorage.Bytes + 1u);
	xhttpstructureddictionarymember* pUnalignedMember =
		(xhttpstructureddictionarymember*)(MemberStorage.Bytes + 1u);
	xhttpstructuredmapcursor Cursor;
	xhttpstructuredmapcursor SavedCursor;
	xhttpstructureddictionarymember Member;
	xhttpstructureddictionarymember SavedMember;
	xhttpnext Next;
	char arrPriorityName[] = "pRiOrItY";
	size_t iCount = 0;

	xrtHttpStructuredMapCursorInit(&Cursor);
	testRequire(
		(xrtHttpStructuredDictionaryMapNext(
			Value, &Cursor, &Member
		) == XHTTP_NEXT_ITEM) &&
		testStructuredViewEqual(Member.Key, XRT_STR_LITERAL("a")) &&
		(Member.Member.Bare.Number == 9),
		"structured ordered Map final value mismatch"
	);
	SavedCursor = Cursor;
	SavedMember = Member;
	xrtClearError();
	testRequire(
		(xrtHttpStructuredDictionaryMapNext(
			Other, &Cursor, &Member
		) == XHTTP_NEXT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(memcmp(&Cursor, &SavedCursor, sizeof(Cursor)) == 0) &&
		(memcmp(&Member, &SavedMember, sizeof(Member)) == 0),
		"structured ordered Map cursor switched value sources"
	);

	Cursor = SavedCursor;
	Member = SavedMember;
	xrtClearError();
	testRequire(
		(xrtHttpStructuredDictionaryMapFieldNext(
			Fields, 2u, XRT_STR_LITERAL("Priority"),
			&Cursor, &Member
		) == XHTTP_NEXT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(memcmp(&Cursor, &SavedCursor, sizeof(Cursor)) == 0) &&
		(memcmp(&Member, &SavedMember, sizeof(Member)) == 0),
		"structured ordered Map cursor switched source kinds"
	);

	xrtHttpStructuredMapCursorInit(&Cursor);
	testRequire(
		(xrtHttpStructuredDictionaryMapFieldNext(
		Fields, 2u, XRT_STR_LITERAL("Priority"),
		&Cursor, &Member
		) == XHTTP_NEXT_ITEM) &&
		testStructuredViewEqual(
			Member.Key, XRT_STR_LITERAL("u")
		) && (Member.Member.Bare.Number == 1),
		"structured field Map final value mismatch"
	);
	iCount = 1u;
	SavedCursor = Cursor;
	SavedMember = Member;
	xrtClearError();
	testRequire(
		(xrtHttpStructuredDictionaryMapFieldNext(
			Fields, 2u, XRT_STR_LITERAL("Other"),
			&Cursor, &Member
		) == XHTTP_NEXT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(memcmp(&Cursor, &SavedCursor, sizeof(Cursor)) == 0) &&
		(memcmp(&Member, &SavedMember, sizeof(Member)) == 0),
		"structured field Map cursor switched field names"
	);
	Cursor = SavedCursor;
	Member = SavedMember;
	xrtClearError();
	while ( (Next = xrtHttpStructuredDictionaryMapFieldNext(
		Fields, 2u,
		(xstrview){ arrPriorityName, sizeof(arrPriorityName) - 1u },
		&Cursor, &Member
	)) == XHTTP_NEXT_ITEM ) {
		iCount++;
	}
	testRequire(
		(Next == XHTTP_NEXT_END) && (iCount == 2u),
		"structured field Map iteration mismatch"
	);

	xrtHttpStructuredMapCursorInit(&Cursor);
	SavedCursor = Cursor;
	memset(&Member, 0xA5, sizeof(Member));
	SavedMember = Member;
	xrtClearError();
	testRequire(
		(xrtHttpStructuredDictionaryMapNext(
			XRT_STR_LITERAL("a=1, b=bad@"),
			&Cursor, &Member
		) == XHTTP_NEXT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE) &&
		(memcmp(&Cursor, &SavedCursor, sizeof(Cursor)) == 0) &&
		(memcmp(&Member, &SavedMember, sizeof(Member)) == 0),
		"structured Map published before trailing validation"
	);

	xrtHttpStructuredMapCursorInit(pUnalignedCursor);
	testRequire(
		xrtHttpStructuredDictionaryMapNext(
			Value, pUnalignedCursor, pUnalignedMember
		) == XHTTP_NEXT_ITEM,
		"structured ordered Map rejected unaligned objects"
	);
	memcpy(&Member, pUnalignedMember, sizeof(Member));
	testRequire(
		testStructuredViewEqual(
			Member.Key, XRT_STR_LITERAL("a")
		),
		"structured unaligned Map result mismatch"
	);
	xrtClearError();
	xrtHttpStructuredMapCursorInit(
		(xhttpstructuredmapcursor*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"structured Map initializer accepted wrapped storage"
	);
	xrtClearError();
}



/* 验证重复 HTTP 字段行按逗号组合，并拒绝空行边界。 */
static void testStructuredFields(void)
{
	static const xhttpfield Lists[] = {
		{ XRT_STR_INIT("Example"), XRT_STR_INIT("a, b") },
		{ XRT_STR_INIT("Other"), XRT_STR_INIT("ignored") },
		{ XRT_STR_INIT("example"), XRT_STR_INIT("(c d)") }
	};
	static const xhttpfield Dictionaries[] = {
		{ XRT_STR_INIT("Priority"), XRT_STR_INIT("u=3") },
		{ XRT_STR_INIT("priority"), XRT_STR_INIT("i, u=1") }
	};
	static const xhttpfield Invalid[] = {
		{ XRT_STR_INIT("Example"), XRT_STR_INIT("a") },
		{ XRT_STR_INIT("example"), XRT_STR_INIT("") }
	};
	static const xhttpfield OtherLists[] = {
		{ XRT_STR_INIT("Example"), XRT_STR_INIT("x, y") },
		{ XRT_STR_INIT("Other"), XRT_STR_INIT("ignored") },
		{ XRT_STR_INIT("example"), XRT_STR_INIT("z") }
	};
	static const xhttpfield ItemField[] = {
		{ XRT_STR_INIT("One"), XRT_STR_INIT(" 42;a ") }
	};
	xhttpstructuredfieldcursor Cursor;
	xhttpstructuredfieldcursor SavedCursor;
	xhttpstructureddictionarymember Dictionary;
	xhttpstructuredmember Member;
	xhttpstructuredmember SavedMember;
	xhttpstructureditem Item;
	char arrEquivalentName[] = "eXaMpLe";
	size_t iMembers = 0;

	xrtHttpStructuredFieldCursorInit(&Cursor);
	while ( xrtHttpStructuredListFieldNext(
		Lists, 3, XRT_STR_LITERAL("Example"), &Cursor, &Member
	) == XHTTP_NEXT_ITEM ) {
		iMembers++;
	}
	testRequire(
		iMembers == 3u,
		"structured repeated List fields mismatch"
	);
	xrtHttpStructuredFieldCursorInit(&Cursor);
	testRequire(
		(xrtHttpStructuredListFieldNext(
			Lists, 3u, XRT_STR_LITERAL("Example"),
			&Cursor, &Member
		) == XHTTP_NEXT_ITEM) &&
		(xrtHttpStructuredListFieldNext(
			Lists, 3u,
			(xstrview){
				arrEquivalentName,
				sizeof(arrEquivalentName) - 1u
			}, &Cursor, &Member
		) == XHTTP_NEXT_ITEM),
		"structured field cursor rejected equivalent name storage"
	);
	xrtHttpStructuredFieldCursorInit(&Cursor);
	testRequire(
		(xrtHttpStructuredDictionaryFieldNext(
			Dictionaries, 2, XRT_STR_LITERAL("Priority"),
			&Cursor, &Dictionary
		) == XHTTP_NEXT_ITEM) &&
		testStructuredViewEqual(Dictionary.Key, XRT_STR_LITERAL("u")),
		"structured repeated Dictionary fields mismatch"
	);
	testRequire(
		(xrtHttpStructuredDictionaryFieldCount(
			Dictionaries, 2, XRT_STR_LITERAL("Priority")
		) == 2u) &&
		(xrtHttpStructuredDictionaryFieldAt(
			Dictionaries, 2, XRT_STR_LITERAL("Priority"),
			0, &Dictionary
		) == XHTTP_NEXT_ITEM) &&
		(Dictionary.Member.Bare.Number == 1) &&
		(xrtHttpStructuredDictionaryFieldFind(
			Dictionaries, 2, XRT_STR_LITERAL("Priority"),
			XRT_STR_LITERAL("i"), &Dictionary
		) == XHTTP_NEXT_ITEM) &&
		(Dictionary.Member.Bare.Number == 1),
		"structured repeated Dictionary map mismatch"
	);
	testRequire(
		(xrtHttpStructuredItemField(
			ItemField, 1, XRT_STR_LITERAL("One"), &Item
		) == XHTTP_NEXT_ITEM) && (Item.Bare.Number == 42),
		"structured Item field mismatch"
	);
	xrtHttpStructuredFieldCursorInit(&Cursor);
	testRequire(
		xrtHttpStructuredListFieldNext(
			Invalid, 2, XRT_STR_LITERAL("Example"),
			&Cursor, &Member
		) == XHTTP_NEXT_ERROR,
		"structured repeated field accepted empty combination member"
	);
	xrtClearError();

	xrtHttpStructuredFieldCursorInit(&Cursor);
	testRequire(
		xrtHttpStructuredListFieldNext(
			Lists, 3u, XRT_STR_LITERAL("Example"),
			&Cursor, &Member
		) == XHTTP_NEXT_ITEM,
		"structured field cursor binding setup failed"
	);
	SavedCursor = Cursor;
	SavedMember = Member;
	xrtClearError();
	testRequire(
		(xrtHttpStructuredListFieldNext(
			Lists, 3u, XRT_STR_LITERAL("Other"),
			&Cursor, &Member
		) == XHTTP_NEXT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(memcmp(&Cursor, &SavedCursor, sizeof(Cursor)) == 0) &&
		(memcmp(&Member, &SavedMember, sizeof(Member)) == 0),
		"structured field cursor switched field names"
	);
	Cursor = SavedCursor;
	Member = SavedMember;
	xrtClearError();
	testRequire(
		(xrtHttpStructuredDictionaryFieldNext(
			Lists, 3u, XRT_STR_LITERAL("Example"),
			&Cursor, &Dictionary
		) == XHTTP_NEXT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(memcmp(&Cursor, &SavedCursor, sizeof(Cursor)) == 0),
		"structured field cursor switched top-level types"
	);
	xrtHttpStructuredFieldCursorInit(&Cursor);
	testRequire(
		xrtHttpStructuredListFieldNext(
			Lists, 3u, XRT_STR_LITERAL("Example"),
			&Cursor, &Member
		) == XHTTP_NEXT_ITEM,
		"structured field source binding setup failed"
	);
	SavedCursor = Cursor;
	SavedMember = Member;
	xrtClearError();
	testRequire(
		(xrtHttpStructuredListFieldNext(
			OtherLists, 3u, XRT_STR_LITERAL("Example"),
			&Cursor, &Member
		) == XHTTP_NEXT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(memcmp(&Cursor, &SavedCursor, sizeof(Cursor)) == 0) &&
		(memcmp(&Member, &SavedMember, sizeof(Member)) == 0),
		"structured field cursor switched field arrays"
	);
	xrtClearError();
}



/* 验证严格空白、分隔符、线路字符和失败原子性。 */
static void testStructuredInvalidAndMemory(void)
{
	static const xstrview InvalidList[] = {
		XRT_STR_INIT("a,"),
		XRT_STR_INIT(",a"),
		XRT_STR_INIT("a,,b"),
		XRT_STR_INIT("\ta"),
		XRT_STR_INIT("\"bad\\q\""),
		XRT_STR_INIT(":A=:") ,
		XRT_STR_INIT("%\"%C3%BC\"") ,
		XRT_STR_INIT("%\"%ff\"")
	};
	union {
		uint64 Align;
		uint8 Bytes[sizeof(xhttpstructuredmember) + 1u];
	} Storage;
	xhttpstructuredmember* pMember =
		(xhttpstructuredmember*)(Storage.Bytes + 1u);
	xhttpstructuredmember Saved;
	xhttpstructureddictionarymember Dictionary;
	xhttpstructureddictionarymember SavedDictionary;
	xhttpstructureditem Item;
	xhttpstructureditem SavedItem;
	size_t iOffset;
	size_t i;

	for ( i = 0; i <
		(sizeof(InvalidList) / sizeof(InvalidList[0])); i++ ) {
		testRequire(
			!xrtHttpStructuredListValid(InvalidList[i]),
			"structured List accepted malformed input"
		);
		xrtClearError();
	}
	iOffset = 0;
	testRequire(
		xrtHttpStructuredListNext(
			XRT_STR_LITERAL("a, b"), &iOffset, pMember
		) == XHTTP_NEXT_ITEM,
		"structured List rejected unaligned output"
	);
	memcpy(&Saved, pMember, sizeof(Saved));
	testRequire(
		Saved.Kind == XHTTP_STRUCTURED_MEMBER_ITEM,
		"structured unaligned output mismatch"
	);
	iOffset = 0;
	memset(pMember, 0xA5, sizeof(*pMember));
	memcpy(&Saved, pMember, sizeof(Saved));
	testRequire(
		xrtHttpStructuredListNext(
			XRT_STR_LITERAL("a,"), &iOffset, pMember
		) == XHTTP_NEXT_ERROR,
		"structured List trailing comma did not fail"
	);
	testRequire(
		(iOffset == 0) &&
		(memcmp(pMember, &Saved, sizeof(Saved)) == 0),
		"structured parser failure was not atomic"
	);
	xrtClearError();

	iOffset = 0;
	memset(pMember, 0xA5, sizeof(*pMember));
	memcpy(&Saved, pMember, sizeof(Saved));
	testRequire(
		xrtHttpStructuredListNext(
			XRT_STR_LITERAL("a, bad@"), &iOffset, pMember
		) == XHTTP_NEXT_ERROR,
		"structured List published before trailing validation"
	);
	testRequire(
		(iOffset == 0) &&
		(memcmp(pMember, &Saved, sizeof(Saved)) == 0),
		"structured List trailing failure was not atomic"
	);
	xrtClearError();

	iOffset = 0;
	memset(&Dictionary, 0xA5, sizeof(Dictionary));
	memcpy(&SavedDictionary, &Dictionary, sizeof(Dictionary));
	testRequire(
		xrtHttpStructuredDictionaryNext(
			XRT_STR_LITERAL("a=1, b=bad@"),
			&iOffset, &Dictionary
		) == XHTTP_NEXT_ERROR,
		"structured Dictionary published before trailing validation"
	);
	testRequire(
		(iOffset == 0) && (memcmp(
			&Dictionary, &SavedDictionary, sizeof(Dictionary)
		) == 0),
		"structured Dictionary trailing failure was not atomic"
	);
	xrtClearError();

	iOffset = 0;
	memset(&Item, 0xA5, sizeof(Item));
	memcpy(&SavedItem, &Item, sizeof(Item));
	testRequire(
		xrtHttpStructuredInnerNext(
			XRT_STR_LITERAL("a bad@"), &iOffset, &Item
		) == XHTTP_NEXT_ERROR,
		"structured Inner List published before trailing validation"
	);
	testRequire(
		(iOffset == 0) &&
		(memcmp(&Item, &SavedItem, sizeof(Item)) == 0),
		"structured Inner List trailing failure was not atomic"
	);
	xrtClearError();

	iOffset = 0;
	memset(pMember, 0xA5, sizeof(*pMember));
	memcpy(&Saved, pMember, sizeof(Saved));
	testRequire(
		xrtHttpStructuredListNext(
			(xstrview){
				(const char*)(uintptr_t)(UINTPTR_MAX - 1u), 4u
			},
			&iOffset, pMember
		) == XHTTP_NEXT_ERROR,
		"structured List accepted wrapped input"
	);
	testRequire(
		(iOffset == 0) &&
		(memcmp(pMember, &Saved, sizeof(Saved)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"structured wrapped input failure was not atomic"
	);
	xrtClearError();
}



/* 验证实现满足 RFC 9651 要求的至少 1024 个 List 成员。 */
static void testStructuredScale(void)
{
	char arrList[(1024u * 2u) - 1u];
	xstrview Value = { arrList, sizeof(arrList) };
	xhttpstructuredmember Member;
	xhttpnext Next;
	size_t iOffset = 0;
	size_t iCount = 0;
	size_t i;

	for ( i = 0; i < 1024u; i++ ) {
		arrList[i * 2u] = 'a';
		if ( i != 1023u ) {
			arrList[(i * 2u) + 1u] = ',';
		}
	}
	testRequire(
		xrtHttpStructuredListValid(Value),
		"structured List rejected 1024 members"
	);
	while ( (Next = xrtHttpStructuredListNext(
		Value, &iOffset, &Member
	)) == XHTTP_NEXT_ITEM ) {
		iCount++;
	}
	testRequire(
		(Next == XHTTP_NEXT_END) && (iCount == 1024u) &&
		(iOffset == Value.Size),
		"structured 1024-member iteration mismatch"
	);
}



/* 运行 RFC 9651 Structured Fields 基础测试。 */
int main(void)
{
	testStructuredSyntax();
	testStructuredNumbers();
	testStructuredDecodedValues();
	testStructuredDecodedMemory();
	testStructuredDecodedCoherence();
	testStructuredParameters();
	testStructuredInnerList();
	testStructuredDictionary();
	testStructuredDictionaryMap();
	testStructuredFields();
	testStructuredInvalidAndMemory();
	testStructuredScale();
	printf("[PASS] http_structured\n");
	return 0;
}
