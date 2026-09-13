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

/* 覆盖 SMTP 短协议构建器在小块池路径上的原子 OOM 行为。 */
int main(void)
{
	size_t iSize;
	str sText;

	iSize = 11u;
	testProtocolFailNext();
	sText = xrtSmtpCommand(
		XRT_STR_LITERAL("EHLO"),
		XRT_STR_LITERAL("client.example"),
		&iSize
	);
	testRequire((sText == NULL) && (iSize == 11u),
		"SMTP command OOM published a partial result");
	testProtocolRequireFailed("SMTP command OOM leaked storage");

	puts("[PASS] SMTP protocol allocation OOM (1 pooled path)");
	return 0;
}
