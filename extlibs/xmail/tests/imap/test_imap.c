#include "../test.h"



/* 验证 tagged、untagged 和 continuation 响应视图。 */
static void testImapResponses(void)
{
	ximapresponseview Response;

	testRequire(xrtImapResponseParse(
		XRT_STR_LITERAL("A001 OK completed"),
		&Response
	) && (Response.Kind == XIMAP_RESPONSE_TAGGED) &&
		(Response.Status == XIMAP_STATUS_OK) &&
		testMailViewEqual(Response.Tag, XRT_STR_LITERAL("A001")) &&
		testMailViewEqual(Response.Text, XRT_STR_LITERAL("completed")),
		"IMAP tagged response parse failed");
	testRequire(xrtImapResponseParse(
		XRT_STR_LITERAL("* 23 EXISTS"),
		&Response
	) && (Response.Kind == XIMAP_RESPONSE_UNTAGGED) &&
		(Response.Status == XIMAP_STATUS_NONE) &&
		testMailViewEqual(Response.Text, XRT_STR_LITERAL("23 EXISTS")),
		"IMAP numeric untagged response parse failed");
	testRequire(xrtImapResponseParse(
		XRT_STR_LITERAL("* BYE closing"),
		&Response
	) && (Response.Status == XIMAP_STATUS_BYE) &&
		testMailViewEqual(Response.Text, XRT_STR_LITERAL("closing")),
		"IMAP status untagged response parse failed");
	testRequire(xrtImapResponseParse(
		XRT_STR_LITERAL("+ continue"),
		&Response
	) && (Response.Kind == XIMAP_RESPONSE_CONTINUATION) &&
		testMailViewEqual(Response.Text, XRT_STR_LITERAL("continue")),
		"IMAP continuation response parse failed");
	testRequire(!xrtImapResponseParse(
		XRT_STR_LITERAL("A001 EXISTS invalid"),
		&Response
	), "IMAP tagged response accepted a non-status atom");
}



/* 验证同步、非同步和 binary literal 标记。 */
static void testImapLiterals(void)
{
	ximapliteralview Literal;
	xstrview Overflow = XRT_STR_LITERAL("A1 APPEND {184467440737095516160}");

	testRequire(xrtImapLiteralParse(
		XRT_STR_LITERAL("* 1 FETCH (BODY[] {12}"),
		&Literal
	) == XMAIL_NEXT_ITEM && (Literal.Size == 12u) &&
		!Literal.NonSynchronizing && !Literal.Binary &&
		testMailViewEqual(Literal.Source, XRT_STR_LITERAL("{12}")),
		"IMAP synchronizing literal parse failed");
	testRequire(xrtImapLiteralParse(
		XRT_STR_LITERAL("A1 APPEND ~{4+}"),
		&Literal
	) == XMAIL_NEXT_ITEM && (Literal.Size == 4u) &&
		Literal.NonSynchronizing && Literal.Binary &&
		testMailViewEqual(Literal.Source, XRT_STR_LITERAL("~{4+}")),
		"IMAP binary literal parse failed");
	testRequire(xrtImapLiteralParse(
		XRT_STR_LITERAL("* 2 EXISTS"),
		&Literal
	) == XMAIL_NEXT_END, "IMAP no-literal result mismatch");
	testRequire(xrtImapLiteralParse(
		XRT_STR_LITERAL("A1 APPEND {x}"),
		&Literal
	) == XMAIL_NEXT_ERROR, "IMAP literal accepted non-digits");
	testRequire(xrtImapLiteralParse(Overflow, &Literal) == XMAIL_NEXT_ERROR,
		"IMAP literal accepted an overflowing size");
}



/* 验证响应码和数字型非标记响应的零分配视图。 */
static void testImapMetadata(void)
{
	ximapcodeview Code;
	ximapnumberview Number;

	testRequire(xrtImapCodeParse(
		XRT_STR_LITERAL("[UIDVALIDITY 42] selected"),
		&Code
	) == XMAIL_NEXT_ITEM &&
		testMailViewEqual(Code.Name, XRT_STR_LITERAL("UIDVALIDITY")) &&
		testMailViewEqual(Code.Arguments, XRT_STR_LITERAL("42")) &&
		testMailViewEqual(Code.Text, XRT_STR_LITERAL("selected")),
		"IMAP response code parse failed");
	testRequire(xrtImapCodeParse(
		XRT_STR_LITERAL("plain status text"),
		&Code
	) == XMAIL_NEXT_END, "IMAP no-code result mismatch");
	testRequire(xrtImapCodeParse(
		XRT_STR_LITERAL("[UIDNEXT 4"),
		&Code
	) == XMAIL_NEXT_ERROR, "IMAP response code accepted missing bracket");
	testRequire(xrtImapNumberParse(
		XRT_STR_LITERAL("23 FETCH (FLAGS (\\Seen))"),
		&Number
	) == XMAIL_NEXT_ITEM && (Number.Number == UINT64_C(23)) &&
		testMailViewEqual(Number.Name, XRT_STR_LITERAL("FETCH")) &&
		testMailViewEqual(Number.Text, XRT_STR_LITERAL("(FLAGS (\\Seen))")),
		"IMAP numeric response parse failed");
	testRequire(xrtImapNumberParse(
		XRT_STR_LITERAL("CAPABILITY IMAP4rev1"),
		&Number
	) == XMAIL_NEXT_END, "IMAP non-numeric response mismatch");
	testRequire(xrtImapNumberParse(
		XRT_STR_LITERAL("18446744073709551616 EXISTS"),
		&Number
	) == XMAIL_NEXT_ERROR, "IMAP numeric response accepted overflow");
}



/* 验证 atom 游标与常用 capability 位。 */
static void testImapCapabilities(void)
{
	ximapatomcursor Cursor;
	xstrview Atom;

	testRequire(xrtImapAtomCursorInit(
		&Cursor,
		XRT_STR_LITERAL("IMAP4rev1 STARTTLS X-VENDOR")
	), "IMAP atom cursor init failed");
	testRequire((xrtImapAtomNext(&Cursor, &Atom) == XMAIL_NEXT_ITEM) &&
		(xrtImapCapability(Atom) == XIMAP_CAP_IMAP4REV1),
		"IMAP first capability mismatch");
	testRequire((xrtImapAtomNext(&Cursor, &Atom) == XMAIL_NEXT_ITEM) &&
		(xrtImapCapability(Atom) == XIMAP_CAP_STARTTLS),
		"IMAP STARTTLS capability mismatch");
	testRequire((xrtImapAtomNext(&Cursor, &Atom) == XMAIL_NEXT_ITEM) &&
		(xrtImapCapability(Atom) == 0),
		"unknown IMAP capability consumed a built-in bit");
	testRequire(xrtImapAtomNext(&Cursor, &Atom) == XMAIL_NEXT_END,
		"IMAP atom cursor did not finish");
	testRequire(xrtImapCapability(XRT_STR_LITERAL("AUTH=XOAUTH2")) ==
		XIMAP_CAP_AUTH_XOAUTH2,
		"IMAP XOAUTH2 capability length mismatch");
	testRequire(xrtImapCapability(XRT_STR_LITERAL("COMPRESS=DEFLATE")) ==
		XIMAP_CAP_COMPRESS_DEFLATE,
		"IMAP COMPRESS capability mismatch");
	testRequire(xrtImapCapability(XRT_STR_LITERAL("LITERAL-")) ==
		XIMAP_CAP_LITERAL_MINUS,
		"IMAP LITERAL- capability mismatch");
	testRequire(xrtImapCapability(XRT_STR_LITERAL("APPENDLIMIT=1048576")) ==
		XIMAP_CAP_APPENDLIMIT,
		"IMAP APPENDLIMIT capability mismatch");
	testRequire(!xrtImapAtomValid(XRT_STR_LITERAL("bad]atom")),
		"IMAP atom accepted atom-specials");
	testRequire(xrtImapSequenceSetValid(XRT_STR_LITERAL("1,3:9,*")) &&
		xrtImapSequenceSetValid(XRT_STR_LITERAL("$")),
		"IMAP sequence-set rejected valid syntax");
	testRequire(!xrtImapSequenceSetValid(XRT_STR_LITERAL("0:4")) &&
		!xrtImapSequenceSetValid(XRT_STR_LITERAL("1,,2")) &&
		!xrtImapSequenceSetValid(XRT_STR_LITERAL("4294967296")),
		"IMAP sequence-set accepted invalid syntax");
}



/* 验证 quoted string 与命令构建器的注入边界。 */
static void testImapCommand(void)
{
	char arrOutput[64];
	size_t iSize;
	str sQuoted;

	sQuoted = xrtImapQuote(XRT_STR_LITERAL("a\"b\\c"), &iSize);
	testRequire((sQuoted != NULL) && (iSize == 9u) &&
		(strcmp(sQuoted, "\"a\\\"b\\\\c\"") == 0),
		"IMAP quoted string output mismatch");
	xrtFree(sQuoted);
	testRequire(!xrtImapQuoteWrite(
		testMailViewN("bad\tvalue", 9u),
		arrOutput,
		sizeof(arrOutput),
		&iSize
	), "IMAP quoted string accepted control data");
	testRequire(xrtImapCommandWrite(
		XRT_STR_LITERAL("A001"),
		XRT_STR_LITERAL("SELECT"),
		XRT_STR_LITERAL("\"INBOX\""),
		0,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (iSize == 21u) &&
		(memcmp(arrOutput, "A001 SELECT \"INBOX\"\r\n", iSize) == 0),
		"IMAP command output mismatch");
	testRequire(!xrtImapCommandWrite(
		XRT_STR_LITERAL("A002"),
		XRT_STR_LITERAL("LOGIN"),
		XRT_STR_LITERAL("user\r\nA003 LOGOUT"),
		0,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	), "IMAP command accepted line injection");
	testRequire(!xrtImapCommandWrite(
		XRT_STR_LITERAL("A003"),
		XRT_STR_LITERAL("NOOP"),
		XRT_STR_LITERAL("123456"),
		12u,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	), "IMAP command ignored the configured line limit");
}



/* 运行 IMAP 协议原语测试。 */
int main(void)
{
	testImapResponses();
	testImapLiterals();
	testImapMetadata();
	testImapCapabilities();
	testImapCommand();
	return 0;
}
