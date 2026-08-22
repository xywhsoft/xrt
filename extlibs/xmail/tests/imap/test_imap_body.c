#include "../test.h"



/* 验证 TEXT 单部分、64 位长度和参数游标。 */
static void testImapBodyText(void)
{
	static const xstrview Text = XRT_STR_LITERAL(
		"(\"TEXT\" \"PLAIN\" (\"CHARSET\" \"US-ASCII\") NIL NIL "
		"\"7BIT\" 4294967297 48)"
	);
	ximapbodyview Body;
	ximapbodyparamcursor Cursor;
	ximapbodyparam Parameter;

	testRequire(xrtImapBodyParse(Text, &Body) &&
		(Body.Kind == XIMAP_BODY_TEXT) &&
		testMailViewEqual(Body.Type.Value, XRT_STR_LITERAL("TEXT")) &&
		testMailViewEqual(Body.Subtype.Value, XRT_STR_LITERAL("PLAIN")) &&
		(Body.Octets == UINT64_C(4294967297)) && (Body.Lines == 48u) &&
		(Body.ChildCount == 0) && (Body.Extensions.Size == 0),
		"IMAP text BODYSTRUCTURE mismatch");
	testRequire(xrtImapBodyParamCursorInit(&Cursor, &Body.Parameters) &&
		(xrtImapBodyParamNext(&Cursor, &Parameter) == XMAIL_NEXT_ITEM) &&
		testMailViewEqual(Parameter.Name.Value,
			XRT_STR_LITERAL("CHARSET")) &&
		testMailViewEqual(Parameter.Value.Value,
			XRT_STR_LITERAL("US-ASCII")) &&
		(xrtImapBodyParamNext(&Cursor, &Parameter) == XMAIL_NEXT_END),
		"IMAP body parameter cursor mismatch");
}



/* 验证单部分扩展字段和未知扩展原样保留。 */
static void testImapBodyExtensions(void)
{
	static const xstrview Text = XRT_STR_LITERAL(
		"(\"TEXT\" \"PLAIN\" (\"CHARSET\" \"UTF-8\" \"FORMAT\" "
		"\"FLOWED\") NIL NIL \"8BIT\" 12 2 \"md5\" "
		"(\"INLINE\" (\"FILENAME\" \"a.txt\")) (\"en\" \"zh\") "
		"\"/body\" 7 (\"future\" 9))"
	);
	ximapbodyview Body;

	testRequire(xrtImapBodyParse(Text, &Body) &&
		(Body.Kind == XIMAP_BODY_TEXT) &&
		testMailViewEqual(Body.Md5.Value, XRT_STR_LITERAL("md5")) &&
		(Body.Disposition.Kind == XIMAP_DATA_LIST) &&
		(Body.Language.Kind == XIMAP_DATA_LIST) &&
		testMailViewEqual(Body.Location.Value,
			XRT_STR_LITERAL("/body")) &&
		testMailViewEqual(Body.Extensions,
			XRT_STR_LITERAL("7 (\"future\" 9)")),
		"IMAP body extension projection mismatch");
}



/* 验证 MESSAGE/GLOBAL 嵌套正文和 envelope 保留。 */
static void testImapBodyMessage(void)
{
	static const xstrview Text = XRT_STR_LITERAL(
		"(\"MESSAGE\" \"GLOBAL\" NIL NIL NIL \"8BIT\" 100 "
		"(NIL NIL NIL NIL NIL NIL NIL NIL NIL NIL) "
		"(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 5 1) 5)"
	);
	ximapbodyview Body;
	ximapbodyview Nested;

	testRequire(xrtImapBodyParse(Text, &Body) &&
		(Body.Kind == XIMAP_BODY_MESSAGE) &&
		(Body.Envelope.Kind == XIMAP_DATA_LIST) &&
		(Body.Body.Kind == XIMAP_DATA_LIST) && (Body.Lines == 5u) &&
		xrtImapBodyParse(Body.Body.Source, &Nested) &&
		(Nested.Kind == XIMAP_BODY_TEXT) && (Nested.Octets == 5u),
		"IMAP message BODYSTRUCTURE mismatch");
}



/* 验证 multipart 直接子部分游标和扩展字段。 */
static void testImapBodyMultipart(void)
{
	static const xstrview Text = XRT_STR_LITERAL(
		"((\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 5 1) "
		"(\"APPLICATION\" \"OCTET-STREAM\" NIL NIL NIL \"BASE64\" 10) "
		"\"MIXED\" (\"BOUNDARY\" \"b\") "
		"(\"ATTACHMENT\" (\"FILENAME\" \"x.bin\")) "
		"(\"en\" \"zh\") NIL 99 (\"x\"))"
	);
	ximapbodyview Body;
	ximapbodyview Child;
	ximapbodycursor Cursor;

	testRequire(xrtImapBodyParse(Text, &Body) &&
		(Body.Kind == XIMAP_BODY_MULTIPART) &&
		(Body.ChildCount == 2u) &&
		testMailViewEqual(Body.Subtype.Value, XRT_STR_LITERAL("MIXED")) &&
		(Body.Parameters.Kind == XIMAP_DATA_LIST) &&
		(Body.Disposition.Kind == XIMAP_DATA_LIST) &&
		(Body.Language.Kind == XIMAP_DATA_LIST) &&
		(Body.Location.Kind == XIMAP_DATA_NIL) &&
		testMailViewEqual(Body.Extensions,
			XRT_STR_LITERAL("99 (\"x\")")),
		"IMAP multipart BODYSTRUCTURE mismatch");
	testRequire(xrtImapBodyChildCursorInit(&Cursor, &Body) &&
		(xrtImapBodyChildNext(&Cursor, &Child) == XMAIL_NEXT_ITEM) &&
		(Child.Kind == XIMAP_BODY_TEXT) &&
		(xrtImapBodyChildNext(&Cursor, &Child) == XMAIL_NEXT_ITEM) &&
		(Child.Kind == XIMAP_BODY_BASIC) &&
		testMailViewEqual(Child.Type.Value,
			XRT_STR_LITERAL("APPLICATION")) &&
		(xrtImapBodyChildNext(&Cursor, &Child) == XMAIL_NEXT_END) &&
		(xrtImapBodyChildNext(&Cursor, &Child) == XMAIL_NEXT_END),
		"IMAP multipart child cursor mismatch");
}



/* 验证 BODYSTRUCTURE 视图可直接消费 FETCH 属性值。 */
static void testImapBodyFetchComposition(void)
{
	static const xstrview Text = XRT_STR_LITERAL(
		"7 FETCH (UID 9 BODYSTRUCTURE "
		"(\"TEXT\" \"HTML\" NIL NIL NIL \"8BIT\" 15 2))"
	);
	ximapfetchview Fetch;
	ximapfetchcursor Cursor;
	ximapfetchitem Item;
	ximapbodyview Body;

	testRequire(xrtImapFetchParse(Text, &Fetch) &&
		xrtImapFetchCursorInit(&Cursor, &Fetch) &&
		(xrtImapFetchNext(&Cursor, &Item) == XMAIL_NEXT_ITEM) &&
		testMailViewEqual(Item.Attribute, XRT_STR_LITERAL("UID")) &&
		(Item.Value.Number == 9u) &&
		(xrtImapFetchNext(&Cursor, &Item) == XMAIL_NEXT_ITEM) &&
		testMailViewEqual(Item.Attribute,
			XRT_STR_LITERAL("BODYSTRUCTURE")) &&
		xrtImapBodyParse(Item.Value.Source, &Body) &&
		(Body.Kind == XIMAP_BODY_TEXT) &&
		testMailViewEqual(Body.Subtype.Value, XRT_STR_LITERAL("HTML")) &&
		(xrtImapFetchNext(&Cursor, &Item) == XMAIL_NEXT_END),
		"IMAP FETCH and BODYSTRUCTURE composition mismatch");
}



/* 构造超过固定深度预算的嵌套 multipart。 */
static xstrview testImapBodyDeep(char* sText, size_t iCapacity)
{
	static const char sLeaf[] =
		"(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 1 1)";
	static const char sTail[] = " \"MIXED\")";
	size_t iPosition = 0;

	for ( size_t i = 0; i < (XIMAP_BODY_DEPTH_MAX + 1u); i++ ) {
		testRequire(iPosition < iCapacity, "IMAP deep body buffer overflow");
		sText[iPosition++] = '(';
	}
	testRequire((iPosition + sizeof(sLeaf) - 1u) < iCapacity,
		"IMAP deep body leaf overflow");
	memcpy(sText + iPosition, sLeaf, sizeof(sLeaf) - 1u);
	iPosition += sizeof(sLeaf) - 1u;
	for ( size_t i = 0; i < (XIMAP_BODY_DEPTH_MAX + 1u); i++ ) {
		testRequire((iPosition + sizeof(sTail) - 1u) < iCapacity,
			"IMAP deep body tail overflow");
		memcpy(sText + iPosition, sTail, sizeof(sTail) - 1u);
		iPosition += sizeof(sTail) - 1u;
	}
	sText[iPosition] = 0;
	return testMailViewN(sText, iPosition);
}



/* 验证不完整字段、非法扩展、literal、尾随数据、深度和内存重叠边界。 */
static void testImapBodyRejectsMalformed(void)
{
	xstrview Invalid[7];
	static const xstrview Valid = XRT_STR_LITERAL(
		"(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 1 1)"
	);
	union {
		ximapbodyview Align;
		char Bytes[256];
	} Storage;
	ximapbodyview Body;
	char sDeep[2048];

	Invalid[0] = XRT_STR_LITERAL("(\"TEXT\" \"PLAIN\")");
	Invalid[1] = XRT_STR_LITERAL(
		"(\"TEXT\" \"PLAIN\" (\"A\") NIL NIL \"7BIT\" 1 1)"
	);
	Invalid[2] = XRT_STR_LITERAL(
		"(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 1 1 NIL "
		"(\"INLINE\") )"
	);
	Invalid[3] = XRT_STR_LITERAL("((\"TEXT\") \"MIXED\")");
	Invalid[4] = XRT_STR_LITERAL(
		"({3} \"PLAIN\" NIL NIL NIL \"7BIT\" 1)"
	);
	Invalid[5] = XRT_STR_LITERAL(
		"(\"APPLICATION\" \"DATA\" NIL NIL NIL \"BINARY\" 1 "
		"NIL NIL NIL NIL ())"
	);
	Invalid[6] = XRT_STR_LITERAL(
		"(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 1 1) EXTRA"
	);

	for ( size_t i = 0; i < (sizeof(Invalid) / sizeof(Invalid[0])); i++ ) {
		testRequire(!xrtImapBodyParse(Invalid[i], &Body),
			"IMAP BODYSTRUCTURE accepted malformed data");
	}
	testRequire(!xrtImapBodyParse(testImapBodyDeep(
		sDeep,
		sizeof(sDeep)
	), &Body), "IMAP BODYSTRUCTURE accepted excessive nesting");
	testRequire(!xrtImapBodyParse(Valid, NULL),
		"IMAP BODYSTRUCTURE accepted null output");
	memcpy(Storage.Bytes, Valid.Data, Valid.Size);
	testRequire(!xrtImapBodyParse(
		testMailViewN(Storage.Bytes, Valid.Size),
		&Storage.Align
	), "IMAP BODYSTRUCTURE accepted overlapping output");
}



/* 运行 IMAP BODYSTRUCTURE 零分配语义层测试。 */
int main(void)
{
	testImapBodyText();
	testImapBodyExtensions();
	testImapBodyMessage();
	testImapBodyMultipart();
	testImapBodyFetchComposition();
	testImapBodyRejectsMalformed();
	return 0;
}
