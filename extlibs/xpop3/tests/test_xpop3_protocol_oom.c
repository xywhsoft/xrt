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

/* 覆盖 POP3 短协议构建器在小块池路径上的原子 OOM 行为。 */
int main(void)
{
	size_t iSize;
	str sText;

	iSize = 12u;
	testProtocolFailNext();
	sText = xrtPop3Command(
		XRT_STR_LITERAL("RETR"),
		XRT_STR_LITERAL("1"),
		&iSize
	);
	testRequire((sText == NULL) && (iSize == 12u),
		"POP3 command OOM published a partial result");
	testProtocolRequireFailed("POP3 command OOM leaked storage");

	puts("[PASS] POP3 protocol allocation OOM (1 pooled path)");
	return 0;
}
