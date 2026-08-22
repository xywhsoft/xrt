#include "../test.h"



/* 验证 addr-spec 语法、拆分视图和 SMTPUTF8 开关。 */
static void testMailAddressValid(void)
{
	xstrview Local;
	xstrview Domain;

	testRequire(xrtMailAddressValid(
		XRT_STR_LITERAL("user.name+tag@example.com"),
		XMAIL_ADDRESS_DEFAULT,
		&Local,
		&Domain
	) && testMailViewEqual(Local, XRT_STR_LITERAL("user.name+tag")) &&
		testMailViewEqual(Domain, XRT_STR_LITERAL("example.com")),
		"mail addr-spec split mismatch");
	testRequire(xrtMailAddressValid(
		XRT_STR_LITERAL("\"local value\"@[IPv6:2001:db8::1]"),
		XMAIL_ADDRESS_DEFAULT,
		NULL,
		NULL
	), "quoted local-part or domain literal rejected");
	testRequire(xrtMailAddressValid(
		XRT_STR_LITERAL("local@[tag(value)]"),
		XMAIL_ADDRESS_DEFAULT,
		NULL,
		NULL
	), "domain literal parentheses were parsed as comments");
	testRequire(!xrtMailAddressValid(
		XRT_STR_LITERAL("a..b@example.com"),
		XMAIL_ADDRESS_DEFAULT,
		NULL,
		NULL
	), "empty local-part atom accepted");
	testRequire(!xrtMailAddressValid(
		XRT_STR_LITERAL("user@-example.com"),
		XMAIL_ADDRESS_DEFAULT,
		NULL,
		NULL
	), "domain with leading hyphen accepted");
	testRequire(!xrtMailAddressValid(
		XRT_STR_LITERAL("用户@例子.测试"),
		XMAIL_ADDRESS_DEFAULT,
		NULL,
		NULL
	), "SMTPUTF8 address accepted without opt-in");
	testRequire(xrtMailAddressValid(
		XRT_STR_LITERAL("用户@例子.测试"),
		XMAIL_ADDRESS_SMTPUTF8,
		NULL,
		NULL
	), "SMTPUTF8 address rejected after opt-in");
}



/* 验证列表游标保留 mailbox 和 group 结构。 */
static void testMailAddressCursor(void)
{
	static const char sList[] =
		"Friends: \"Doe, Jane\" <jane@example.com>, "
		"=?UTF-8?B?5Lit5paH?= <zh@example.cn>;, "
		"Next (work) <next@[IPv6:2001:db8::1]>";
	xmailaddresscursor Cursor;
	xmailaddressview Address;

	testRequire(xrtMailAddressCursorInit(
		&Cursor,
		XRT_STR_LITERAL(sList),
		XMAIL_ADDRESS_DEFAULT
	), "mail address cursor init failed");
	testRequire(xrtMailAddressNext(&Cursor, &Address) == XMAIL_NEXT_ITEM &&
		(Address.Kind == XMAIL_ADDRESS_GROUP_BEGIN) &&
		testMailViewEqual(Address.Name, XRT_STR_LITERAL("Friends")),
		"mail address group begin mismatch");
	testRequire(xrtMailAddressNext(&Cursor, &Address) == XMAIL_NEXT_ITEM &&
		(Address.Kind == XMAIL_ADDRESS_MAILBOX) &&
		testMailViewEqual(Address.Name, XRT_STR_LITERAL("\"Doe, Jane\"")) &&
		testMailViewEqual(Address.Local, XRT_STR_LITERAL("jane")),
		"first mail group member mismatch");
	testRequire(xrtMailAddressNext(&Cursor, &Address) == XMAIL_NEXT_ITEM &&
		(Address.Kind == XMAIL_ADDRESS_MAILBOX) &&
		testMailViewEqual(Address.Domain, XRT_STR_LITERAL("example.cn")),
		"second mail group member mismatch");
	testRequire(xrtMailAddressNext(&Cursor, &Address) == XMAIL_NEXT_ITEM &&
		(Address.Kind == XMAIL_ADDRESS_GROUP_END),
		"mail address group end mismatch");
	testRequire(xrtMailAddressNext(&Cursor, &Address) == XMAIL_NEXT_ITEM &&
		(Address.Kind == XMAIL_ADDRESS_MAILBOX) &&
		testMailViewEqual(Address.Local, XRT_STR_LITERAL("next")) &&
		testMailViewEqual(Address.Domain, XRT_STR_LITERAL("[IPv6:2001:db8::1]")),
		"mailbox after group mismatch");
	testRequire(xrtMailAddressNext(&Cursor, &Address) == XMAIL_NEXT_END,
		"mail address cursor did not reach end");
}



/* 验证常见 mailbox 输出的原样、引号和编码词路径。 */
static void testMailAddressWrite(void)
{
	char arrOutput[256];
	size_t iSize;
	str sAddress;

	testRequire(xrtMailAddressWrite(
		XRT_STR_LITERAL("John Doe"),
		XRT_STR_LITERAL("john@example.com"),
		XMAIL_WORD_BASE64,
		XMAIL_ADDRESS_DEFAULT,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(arrOutput, "John Doe <john@example.com>") == 0),
		"plain display-name formatting mismatch");
	testRequire(xrtMailAddressWrite(
		XRT_STR_LITERAL("Doe, Jane"),
		XRT_STR_LITERAL("jane@example.com"),
		XMAIL_WORD_BASE64,
		XMAIL_ADDRESS_DEFAULT,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(arrOutput, "\"Doe, Jane\" <jane@example.com>") == 0),
		"quoted display-name formatting mismatch");
	testRequire(xrtMailAddressWrite(
		XRT_STR_LITERAL("中文"),
		XRT_STR_LITERAL("zh@example.cn"),
		XMAIL_WORD_BASE64,
		XMAIL_ADDRESS_DEFAULT,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(
		arrOutput,
		"=?UTF-8?B?5Lit5paH?= <zh@example.cn>"
	) == 0), "encoded display-name formatting mismatch");
	testRequire(xrtMailAddressWrite(
		XRT_STR_LITERAL(""),
		XRT_STR_LITERAL(" bare@example.com "),
		XMAIL_WORD_BASE64,
		XMAIL_ADDRESS_DEFAULT,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(arrOutput, "bare@example.com") == 0),
		"bare address formatting mismatch");

	sAddress = xrtMailAddress(
		XRT_STR_LITERAL("A \\\"User"),
		XRT_STR_LITERAL("a@example.com"),
		XMAIL_WORD_Q,
		XMAIL_ADDRESS_DEFAULT,
		&iSize
	);
	testRequire((sAddress != NULL) &&
		(strcmp(sAddress, "\"A \\\\\\\"User\" <a@example.com>") == 0),
		"allocated escaped display-name mismatch");
	xrtFree(sAddress);

	memcpy(arrOutput, "keep", 5u);
	testRequire(!xrtMailAddressWrite(
		XRT_STR_LITERAL("中文"),
		XRT_STR_LITERAL("zh@example.cn"),
		XMAIL_WORD_BASE64,
		XMAIL_ADDRESS_DEFAULT,
		arrOutput,
		8u,
		&iSize
	) && (memcmp(arrOutput, "keep", 5u) == 0) && (iSize > 8u),
		"short mailbox buffer published partial output");
}



/* 验证地址数组的精确计量、统一编码和失败原子性。 */
static void testMailAddressListWrite(void)
{
	const xmailaddress arrAddresses[] = {
		{ XRT_STR_INIT("Doe, Jane"), XRT_STR_INIT("jane@example.com") },
		{ XRT_STR_INIT("中文"), XRT_STR_INIT("zh@example.cn") },
		{ XRT_STR_INIT(""), XRT_STR_INIT("bare@example.net") }
	};
	const xmailaddress arrInvalid[] = {
		{ XRT_STR_INIT("Valid"), XRT_STR_INIT("valid@example.com") },
		{ XRT_STR_INIT("Invalid"), XRT_STR_INIT("invalid address") }
	};
	char arrOutput[256];
	size_t iRequired;
	size_t iSize;
	str sList;

	testRequire(xrtMailAddressListWrite(
		arrAddresses,
		3u,
		XMAIL_WORD_BASE64,
		XMAIL_ADDRESS_DEFAULT,
		NULL,
		0,
		&iRequired
	), "mail address list size query failed");
	testRequire(xrtMailAddressListWrite(
		arrAddresses,
		3u,
		XMAIL_WORD_BASE64,
		XMAIL_ADDRESS_DEFAULT,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (iSize == iRequired) && (strcmp(
		arrOutput,
		"\"Doe, Jane\" <jane@example.com>, "
		"=?UTF-8?B?5Lit5paH?= <zh@example.cn>, bare@example.net"
	) == 0), "mail address list output mismatch");

	sList = xrtMailAddressList(
		arrAddresses,
		3u,
		XMAIL_WORD_BASE64,
		XMAIL_ADDRESS_DEFAULT,
		&iSize
	);
	testRequire((sList != NULL) && (iSize == iRequired) &&
		(strcmp(sList, arrOutput) == 0), "owned mail address list mismatch");
	xrtFree(sList);

	memcpy(arrOutput, "keep", 5u);
	testRequire(!xrtMailAddressListWrite(
		arrAddresses,
		3u,
		XMAIL_WORD_BASE64,
		XMAIL_ADDRESS_DEFAULT,
		arrOutput,
		12u,
		&iSize
	) && (iSize == iRequired) && (memcmp(arrOutput, "keep", 5u) == 0),
		"short address list buffer published partial output");
	testRequire(xrtMailAddressListWrite(
		NULL,
		0,
		XMAIL_WORD_Q,
		XMAIL_ADDRESS_DEFAULT,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (iSize == 0) && (arrOutput[0] == 0),
		"empty mail address list mismatch");

	sList = xrtMailAddressList(
		NULL,
		0,
		XMAIL_WORD_Q,
		XMAIL_ADDRESS_DEFAULT,
		&iSize
	);
	testRequire((sList != NULL) && (iSize == 0) && (sList[0] == 0),
		"owned empty mail address list mismatch");
	xrtFree(sList);

	memcpy(arrOutput, "keep", 5u);
	iSize = 9u;
	testRequire(!xrtMailAddressListWrite(
		arrInvalid,
		2u,
		XMAIL_WORD_Q,
		XMAIL_ADDRESS_DEFAULT,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (iSize == 9u) && (memcmp(arrOutput, "keep", 5u) == 0),
		"invalid address list item published partial output");

	memcpy(arrOutput, "keep", 5u);
	iSize = 11u;
	testRequire(!xrtMailAddressListWrite(
		arrAddresses,
		(SIZE_MAX / sizeof(*arrAddresses)) + 1u,
		XMAIL_WORD_Q,
		XMAIL_ADDRESS_DEFAULT,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (iSize == 11u) && (memcmp(arrOutput, "keep", 5u) == 0),
		"overflowing address list count was accepted");
}



/* 验证列表错误不会推进游标或修改输出。 */
static void testMailAddressErrors(void)
{
	xmailaddresscursor Cursor;
	xmailaddresscursor Before;
	xmailaddressview Address;

	testRequire(xrtMailAddressCursorInit(
		&Cursor,
		XRT_STR_LITERAL("Friends: a@example.com"),
		XMAIL_ADDRESS_DEFAULT
	), "invalid group cursor init failed");
	testRequire(xrtMailAddressNext(&Cursor, &Address) == XMAIL_NEXT_ITEM,
		"invalid group begin was not returned");
	Before = Cursor;
	Address.Kind = XMAIL_ADDRESS_GROUP_END;
	xrtClearError();
	testRequire(xrtMailAddressNext(&Cursor, &Address) == XMAIL_NEXT_ERROR,
		"unterminated mail group was accepted");
	testRequire((Cursor.Position == Before.Position) && Cursor.InGroup &&
		(Address.Kind == XMAIL_ADDRESS_GROUP_END),
		"mail address error modified cursor or output");
	testRequire(xrtErrorFind(
		xrtGetError(),
		"xrt.mail",
		XMAIL_ERROR_ADDRESS
	) != NULL, "mail address parse error metadata mismatch");

	testRequire(xrtMailAddressCursorInit(
		&Cursor,
		XRT_STR_LITERAL("a@example.com,"),
		XMAIL_ADDRESS_DEFAULT
	), "trailing comma cursor init failed");
	testRequire(xrtMailAddressNext(&Cursor, &Address) == XMAIL_NEXT_ERROR,
		"mail address trailing comma was accepted");

	testRequire(xrtMailAddressCursorInit(
		&Cursor,
		XRT_STR_LITERAL("Group: a@example.com; next@example.com"),
		XMAIL_ADDRESS_DEFAULT
	), "missing group comma cursor init failed");
	testRequire(xrtMailAddressNext(&Cursor, &Address) == XMAIL_NEXT_ITEM &&
		xrtMailAddressNext(&Cursor, &Address) == XMAIL_NEXT_ITEM,
		"missing group comma setup failed");
	testRequire(xrtMailAddressNext(&Cursor, &Address) == XMAIL_NEXT_ERROR,
		"group without following list comma was accepted");
}



/* 运行邮件地址全部契约测试。 */
int main(void)
{
	testMailAddressValid();
	testMailAddressCursor();
	testMailAddressWrite();
	testMailAddressListWrite();
	testMailAddressErrors();
	return 0;
}
