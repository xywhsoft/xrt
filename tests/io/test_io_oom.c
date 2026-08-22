#include "../test.h"



/* 自定义关闭过程验证构造失败不会消费调用方上下文。 */
static bool testIoOomClose(ptr pContext)
{
	(*(size_t*)pContext)++;
	return true;
}



/* 最小 Reader 回调只报告 EOF。 */
static bool testIoOomRead(
	ptr pContext,
	ptr pBuffer,
	size_t iRequest,
	size_t* pRead
)
{
	(void)pContext;
	(void)pBuffer;
	(void)iRequest;
	*pRead = 0u;
	return true;
}



/* 验证每一种核心 IO 对象都原样传播单次对象分配失败。 */
int main(void)
{
	xreaderops Ops;
	xreader* pReader;
	xwriter* pWriter;
	size_t iCloseCount = 0;
	unsigned char arrOutput[8];

	memset(&Ops, 0, sizeof(Ops));
	Ops.Read = testIoOomRead;
	Ops.Close = testIoOomClose;
	testRequire(xrtMemDebugFailAfter(0u), "IO OOM injection setup failed");
	pReader = xrtReaderCreate(&Ops, &iCloseCount);
	testRequire(
		(pReader == NULL) && xrtMemDebugFailTriggered() &&
		(iCloseCount == 0u) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"custom Reader OOM consumed context or lost error"
	);
	xrtMemDebugFailClear();
	xrtClearError();

	testRequire(xrtMemDebugFailAfter(0u), "memory Reader OOM setup failed");
	pReader = xrtReaderFromMemory(XRT_BYTES_LITERAL("abc"));
	testRequire(
		(pReader == NULL) && xrtMemDebugFailTriggered() &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"memory Reader OOM mismatch"
	);
	xrtMemDebugFailClear();
	xrtClearError();

	testRequire(xrtMemDebugFailAfter(0u), "memory Writer OOM setup failed");
	pWriter = xrtWriterFromMemory(arrOutput, sizeof(arrOutput));
	testRequire(
		(pWriter == NULL) && xrtMemDebugFailTriggered() &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"memory Writer OOM mismatch"
	);
	xrtMemDebugFailClear();
	xrtClearError();

	testRequire(xrtMemDebugFailAfter(0u), "discard Writer OOM setup failed");
	pWriter = xrtWriterDiscard();
	testRequire(
		(pWriter == NULL) && xrtMemDebugFailTriggered() &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"discard Writer OOM mismatch"
	);
	xrtMemDebugFailClear();
	xrtClearError();

	pReader = xrtReaderCreate(&Ops, &iCloseCount);
	testRequire(
		(pReader != NULL) && xrtReaderDestroy(pReader) &&
		(iCloseCount == 1u),
		"IO did not recover after injected OOM"
	);
	testMemoryDebugDrain("IO OOM test leaked memory");
	printf("[PASS] IO OOM\n");
	return 0;
}
