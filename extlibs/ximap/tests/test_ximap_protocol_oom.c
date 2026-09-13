#include "test.h"



/* 为下一次逻辑分配安装故障点。 */
static void testProtocolFailNext(void)
{
	testRequire(xrtMemDebugFailAfter(0),
		"protocol OOM setup failed");
}



/* 验证失败已经命中，且没有留下活动分配。 */
static void testProtocolRequireFailed(cstr sMessage)
{
	testRequire(xrtMemDebugFailTriggered() &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY), sMessage);
	xrtMemDebugFailClear();
	xrtClearError();
	testMemoryDebugDrain(sMessage);
}

/* 覆盖 IMAP 短协议构建器在小块池路径上的原子 OOM 行为。 */
int main(void)
{
	size_t iSize;
	str sText;

	iSize = 13u;
	testProtocolFailNext();
	sText = xrtImapQuote(XRT_STR_LITERAL("INBOX"), &iSize);
	testRequire((sText == NULL) && (iSize == 13u),
		"IMAP quote OOM published a partial result");
	testProtocolRequireFailed("IMAP quote OOM leaked storage");

	iSize = 14u;
	testProtocolFailNext();
	sText = xrtImapCommand(
		XRT_STR_LITERAL("A001"),
		XRT_STR_LITERAL("SELECT"),
		XRT_STR_LITERAL("\"INBOX\""),
		0,
		&iSize
	);
	testRequire((sText == NULL) && (iSize == 14u),
		"IMAP command OOM published a partial result");
	testProtocolRequireFailed("IMAP command OOM leaked storage");

	puts("[PASS] IMAP protocol allocation OOM (2 pooled paths)");
	return 0;
}
