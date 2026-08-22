#include "../test.h"



/* 验证单行与多行 SMTP 响应契约。 */
static void testSmtpReply(void)
{
	xsmtpreplyparser Parser;
	xsmtpreplyline Reply;

	testRequire(xrtSmtpReplyParserInit(&Parser, 0),
		"SMTP reply parser init failed");
	testRequire(xrtSmtpReplyRead(
		&Parser,
		XRT_STR_LITERAL("250-example.test"),
		&Reply
	) && Reply.Continued && (Reply.Code == 250),
		"SMTP continuation reply parse failed");
	testRequire(xrtSmtpReplyRead(
		&Parser,
		XRT_STR_LITERAL("250 SIZE 1024"),
		&Reply
	) && !Reply.Continued && Parser.Done && (Parser.Lines == 2u),
		"SMTP final reply parse failed");
	testRequire(!xrtSmtpReplyRead(
		&Parser,
		XRT_STR_LITERAL("250 late"),
		&Reply
	), "SMTP reply parser accepted input after final line");

	testRequire(xrtSmtpReplyParserInit(&Parser, 2u),
		"SMTP reply parser reset failed");
	testRequire(xrtSmtpReplyRead(
		&Parser,
		XRT_STR_LITERAL("250-first"),
		&Reply
	), "SMTP reply first line failed");
	testRequire(!xrtSmtpReplyRead(
		&Parser,
		XRT_STR_LITERAL("550 changed"),
		&Reply
	), "SMTP multiline response accepted changed code");
	testRequire(!xrtSmtpReplyLineParse(XRT_STR_LITERAL("99 short"), &Reply),
		"SMTP reply accepted invalid status code");
}



/* 验证 EHLO 能力名称、AUTH 机制和 SIZE 上限。 */
static void testSmtpCapabilities(void)
{
	xsmtpcapabilityview Capability;
	uint64 iCapabilities = 0;
	uint64 iSizeLimit = 0;

	testRequire(xrtSmtpCapabilityParse(
		XRT_STR_LITERAL("AUTH PLAIN LOGIN XOAUTH2 OAUTHBEARER"),
		&Capability
	), "SMTP AUTH capability parse failed");
	testRequire(xrtSmtpCapabilityAdd(
		&Capability,
		&iCapabilities,
		&iSizeLimit
	), "SMTP AUTH capability merge failed");
	testRequire((iCapabilities & (XSMTP_CAP_AUTH_PLAIN |
		XSMTP_CAP_AUTH_LOGIN | XSMTP_CAP_AUTH_XOAUTH2 |
		XSMTP_CAP_AUTH_OAUTHBEARER)) ==
		(XSMTP_CAP_AUTH_PLAIN | XSMTP_CAP_AUTH_LOGIN |
		 XSMTP_CAP_AUTH_XOAUTH2 | XSMTP_CAP_AUTH_OAUTHBEARER),
		"SMTP AUTH mechanisms mismatch");
	testRequire(xrtSmtpCapabilityParse(
		XRT_STR_LITERAL("AUTH=PLAIN LOGIN"),
		&Capability
	) && xrtSmtpCapabilityAdd(
		&Capability,
		&iCapabilities,
		&iSizeLimit
	), "SMTP AUTH equals compatibility parse failed");

	testRequire(xrtSmtpCapabilityParse(
		XRT_STR_LITERAL("SIZE 10485760"),
		&Capability
	) && xrtSmtpCapabilityAdd(
		&Capability,
		&iCapabilities,
		&iSizeLimit
	), "SMTP SIZE capability merge failed");
	testRequire((iCapabilities & XSMTP_CAP_SIZE) != 0 &&
		(iSizeLimit == UINT64_C(10485760)), "SMTP SIZE limit mismatch");
	testRequire(xrtSmtpCapability(XRT_STR_LITERAL("starttls")) ==
		XSMTP_CAP_STARTTLS, "SMTP capability lookup mismatch");
	testRequire(!xrtSmtpCapabilityParse(
		XRT_STR_LITERAL("BAD\r\nCAP"),
		&Capability
	), "SMTP capability accepted command injection");
}



/* 验证通用命令构建器不允许行注入或超长命令。 */
static void testSmtpCommand(void)
{
	char arrOutput[64];
	size_t iSize;
	str sCommand;

	testRequire(xrtSmtpCommandWrite(
		XRT_STR_LITERAL("EHLO"),
		XRT_STR_LITERAL("client.example"),
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (iSize == 21u) &&
		(memcmp(arrOutput, "EHLO client.example\r\n", iSize) == 0),
		"SMTP command output mismatch");
	testRequire(!xrtSmtpCommandWrite(
		XRT_STR_LITERAL("MAIL"),
		XRT_STR_LITERAL("FROM:<a@b>\r\nRCPT TO:<x@y>"),
		arrOutput,
		sizeof(arrOutput),
		&iSize
	), "SMTP command accepted line injection");
	sCommand = xrtSmtpCommand(
		XRT_STR_LITERAL("QUIT"),
		XRT_STR_LITERAL(""),
		&iSize
	);
	testRequire((sCommand != NULL) && (iSize == 6u) &&
		(strcmp(sCommand, "QUIT\r\n") == 0),
		"allocated SMTP command mismatch");
	xrtFree(sCommand);
}



/* 运行 SMTP 协议原语测试。 */
int main(void)
{
	testRequire((XSMTP_CAP_AUTH_OAUTHBEARER & XSMTP_CAP_BINARYMIME) == 0,
		"SMTP OAUTHBEARER and BINARYMIME capability bits overlap");
	testSmtpReply();
	testSmtpCapabilities();
	testSmtpCommand();
	return 0;
}
