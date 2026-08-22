#include "../test.h"



/* 验证消息边界、重复字段查找和默认传输编码。 */
static void testMailMessageView(void)
{
	static const char sRaw[] =
		"Received: first\r\n"
		"Received: second\r\n"
		"Subject: xmail\r\n"
		"Content-Transfer-Encoding: base64\r\n"
		"\r\n"
		"aGVsbG8=\r\n";
	xmailmessageview Message;
	xmailheaderview Header;
	xmailtransfer Transfer;

	testRequire(xrtMailMessageParse(
		testMailViewN(sRaw, sizeof(sRaw) - 1u),
		0,
		0,
		&Message
	), "mail message parse failed");
	testRequire(Message.HeaderCount == 4u, "mail message header count mismatch");
	testRequire(testMailViewEqual(Message.Body, XRT_STR_LITERAL("aGVsbG8=\r\n")),
		"mail message body view mismatch");
	testRequire(xrtMailMessageHeader(
		&Message,
		XRT_STR_LITERAL("received"),
		1,
		&Header
	) == XMAIL_NEXT_ITEM, "mail message repeated header lookup failed");
	testRequire(testMailViewEqual(Header.Value, XRT_STR_LITERAL("second")),
		"mail message repeated header mismatch");
	testRequire(xrtMailMessageTransfer(&Message, &Transfer) &&
		(Transfer == XMAIL_TRANSFER_BASE64),
		"mail message transfer encoding mismatch");

	testRequire(xrtMailMessageParse(
		XRT_STR_LITERAL("\r\nbody"),
		0,
		0,
		&Message
	) && (Message.HeaderCount == 0u), "headerless mail message failed");
	testRequire(xrtMailMessageTransfer(&Message, &Transfer) &&
		(Transfer == XMAIL_TRANSFER_7BIT),
		"mail message default transfer mismatch");
}



/* 验证正文解码、查询模式和容量失败不发布部分结果。 */
static void testMailMessageBody(void)
{
	xmailmessageview Message;
	xmailtransfer Transfer;
	unsigned char arrOutput[16];
	size_t iSize = 0;
	bytes pBody;

	testRequire(xrtMailMessageParse(
		XRT_STR_LITERAL(
			"Content-Transfer-Encoding: quoted-printable\r\n"
			"\r\n"
			"hello=20mail"
		),
		0,
		0,
		&Message
	), "quoted-printable message parse failed");
	testRequire(xrtMailMessageTransfer(&Message, &Transfer),
		"quoted-printable transfer lookup failed");
	testRequire(xrtMailMessageBodyWrite(
		&Message,
		Transfer,
		0,
		NULL,
		0,
		&iSize
	) && (iSize == 10u), "mail message body size query failed");
	memcpy(arrOutput, "keep", 5u);
	testRequire(!xrtMailMessageBodyWrite(
		&Message,
		Transfer,
		0,
		arrOutput,
		5u,
		&iSize
	) && (memcmp(arrOutput, "keep", 5u) == 0),
		"mail message body short buffer published output");
	pBody = xrtMailMessageBody(&Message, Transfer, 0, &iSize);
	testRequire((pBody != NULL) && (iSize == 10u) &&
		(memcmp(pBody, "hello mail", 10u) == 0),
		"allocated mail message body mismatch");
	xrtFree(pBody);
}



/* 验证严格线路、预算和唯一传输编码约束。 */
static void testMailMessageLimits(void)
{
	xmailmessageview Message;
	xmailtransfer Transfer;

	testRequire(!xrtMailMessageParse(
		XRT_STR_LITERAL("Subject: bad\n\nbody"),
		0,
		0,
		&Message
	), "mail message accepted bare LF");
	testRequire(!xrtMailMessageParse(
		XRT_STR_LITERAL("Subject: missing separator"),
		0,
		0,
		&Message
	), "mail message accepted missing separator");
	testRequire(!xrtMailMessageParse(
		XRT_STR_LITERAL("A: 1\r\nB: 2\r\n\r\n"),
		0,
		1u,
		&Message
	), "mail message ignored header count limit");
	testRequire(!xrtMailMessageParse(
		XRT_STR_LITERAL("Long: value\r\n\r\n"),
		4u,
		0,
		&Message
	), "mail message ignored header byte limit");
	testRequire(xrtMailMessageParse(
		XRT_STR_LITERAL(
			"Content-Transfer-Encoding: base64\r\n"
			"Content-Transfer-Encoding: 7bit\r\n\r\n"
		),
		0,
		0,
		&Message
	) && !xrtMailMessageTransfer(&Message, &Transfer),
		"mail message accepted duplicate transfer fields");
	testRequire(xrtMailTransferParse(
		XRT_STR_LITERAL(" \r\n\tQuoted-Printable \t")
	) == XMAIL_TRANSFER_QUOTED_PRINTABLE,
		"folded mail transfer encoding parse failed");
	testRequire(xrtMailTransferParse(XRT_STR_LITERAL("base 64")) ==
		XMAIL_TRANSFER_UNKNOWN, "split transfer token accepted");
}



/* 运行邮件消息视图测试。 */
int main(void)
{
	testMailMessageView();
	testMailMessageBody();
	testMailMessageLimits();
	return 0;
}
