#include "../test.h"



/* 验证 POP3 状态、STAT、LIST 和 UIDL 解析。 */
static void testPop3Replies(void)
{
	xpop3replyview Reply;
	xpop3stat Stat;
	xpop3listview List;
	xpop3uidlview Uidl;

	testRequire(xrtPop3ReplyParse(
		XRT_STR_LITERAL("+OK mailbox ready"),
		&Reply
	) && Reply.Ok &&
		testMailViewEqual(Reply.Text, XRT_STR_LITERAL("mailbox ready")),
		"POP3 success reply parse failed");
	testRequire(xrtPop3ReplyParse(
		XRT_STR_LITERAL("-ERR [AUTH] denied"),
		&Reply
	) && !Reply.Ok, "POP3 error reply parse failed");
	testRequire(xrtPop3StatParse(XRT_STR_LITERAL("+OK 12 4096"), &Stat) &&
		(Stat.Messages == 12u) && (Stat.Bytes == 4096u),
		"POP3 STAT parse failed");
	testRequire(xrtPop3ListParse(XRT_STR_LITERAL("7 1024"), &List) &&
		(List.Message == 7u) && (List.Bytes == 1024u),
		"POP3 LIST parse failed");
	testRequire(xrtPop3UidlParse(
		XRT_STR_LITERAL("7 whqtswO00Q430"),
		&Uidl
	) && (Uidl.Message == 7u) &&
		testMailViewEqual(Uidl.Id, XRT_STR_LITERAL("whqtswO00Q430")),
		"POP3 UIDL parse failed");
	testRequire(!xrtPop3UidlParse(XRT_STR_LITERAL("0 invalid"), &Uidl),
		"POP3 UIDL accepted zero message number");
}



/* 验证 CAPA 解析和未知能力可扩展路径。 */
static void testPop3Capabilities(void)
{
	xpop3capabilityview Capability;

	testRequire(xrtPop3CapabilityParse(
		XRT_STR_LITERAL("SASL PLAIN XOAUTH2"),
		&Capability
	), "POP3 capability parse failed");
	testRequire(testMailViewEqual(Capability.Name, XRT_STR_LITERAL("SASL")) &&
		testMailViewEqual(
			Capability.Parameters,
			XRT_STR_LITERAL("PLAIN XOAUTH2")
		), "POP3 capability views mismatch");
	testRequire(xrtPop3Capability(Capability.Name) == XPOP3_CAP_SASL,
		"POP3 capability lookup failed");
	testRequire(xrtPop3Capability(XRT_STR_LITERAL("X-VENDOR")) == 0,
		"unknown POP3 capability consumed a built-in bit");
}



/* 验证命令构建器和 dot 多行复用路径。 */
static void testPop3Command(void)
{
	char arrOutput[32];
	size_t iSize;
	bytes pBody;

	testRequire(xrtPop3CommandWrite(
		XRT_STR_LITERAL("RETR"),
		XRT_STR_LITERAL("42"),
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (iSize == 9u) && (memcmp(arrOutput, "RETR 42\r\n", iSize) == 0),
		"POP3 command output mismatch");
	testRequire(!xrtPop3CommandWrite(
		XRT_STR_LITERAL("USER"),
		XRT_STR_LITERAL("bad\r\nDELE 1"),
		arrOutput,
		sizeof(arrOutput),
		&iSize
	), "POP3 command accepted line injection");
	pBody = xrtMailDotDecode(
		XRT_STR_LITERAL("Header: value\r\n\r\n..body\r\n.\r\n"),
		true,
		&iSize
	);
	testRequire((pBody != NULL) &&
		(memcmp(pBody, "Header: value\r\n\r\n.body\r\n", iSize) == 0),
		"POP3 multiline body decode failed");
	xrtFree(pBody);
}



/* 运行 POP3 协议原语测试。 */
int main(void)
{
	testPop3Replies();
	testPop3Capabilities();
	testPop3Command();
	return 0;
}
