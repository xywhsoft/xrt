#include "../test.h"



/* 为下一次逻辑分配安装故障点。 */
static void testMailProtocolFailNext(void)
{
	testRequire(xrtMemDebugFailAfter(0),
		"mail protocol OOM setup failed");
}



/* 验证失败已经命中，且没有留下活动分配。 */
static void testMailProtocolRequireFailed(cstr sMessage)
{
	testRequire(xrtMemDebugFailTriggered() &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY), sMessage);
	xrtMemDebugFailClear();
	xrtClearError();
	testMemoryDebugDrain(sMessage);
}



/* 覆盖短协议构建器在小块池路径上的原子 OOM 行为。 */
int main(void)
{
	size_t iSize;
	str sText;

	iSize = 11u;
	testMailProtocolFailNext();
	sText = xrtSmtpCommand(
		XRT_STR_LITERAL("EHLO"),
		XRT_STR_LITERAL("client.example"),
		&iSize
	);
	testRequire((sText == NULL) && (iSize == 11u),
		"SMTP command OOM published a partial result");
	testMailProtocolRequireFailed("SMTP command OOM leaked storage");

	iSize = 12u;
	testMailProtocolFailNext();
	sText = xrtPop3Command(
		XRT_STR_LITERAL("RETR"),
		XRT_STR_LITERAL("1"),
		&iSize
	);
	testRequire((sText == NULL) && (iSize == 12u),
		"POP3 command OOM published a partial result");
	testMailProtocolRequireFailed("POP3 command OOM leaked storage");

	iSize = 13u;
	testMailProtocolFailNext();
	sText = xrtImapQuote(XRT_STR_LITERAL("INBOX"), &iSize);
	testRequire((sText == NULL) && (iSize == 13u),
		"IMAP quote OOM published a partial result");
	testMailProtocolRequireFailed("IMAP quote OOM leaked storage");

	iSize = 14u;
	testMailProtocolFailNext();
	sText = xrtImapCommand(
		XRT_STR_LITERAL("A001"),
		XRT_STR_LITERAL("SELECT"),
		XRT_STR_LITERAL("\"INBOX\""),
		0,
		&iSize
	);
	testRequire((sText == NULL) && (iSize == 14u),
		"IMAP command OOM published a partial result");
	testMailProtocolRequireFailed("IMAP command OOM leaked storage");

	puts("[PASS] mail protocol allocation OOM (4 pooled paths)");
	return 0;
}
