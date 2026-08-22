#include "../test.h"



/* 验证 Buffer 适配器构造失败和增长失败保持所有权与内容。 */
int main(void)
{
	unsigned char arrLarge[256];
	xbuffer Buffer;
	xbuffer* pOwned;
	xbuffer* pCopy;
	xreader* pReader;
	xwriter* pWriter;

	memset(arrLarge, 0x5A, sizeof(arrLarge));
	testRequire(xrtBufferInit(&Buffer), "IO Buffer OOM init failed");
	testRequire(
		xrtBufferAppend(&Buffer, XRT_BYTES_LITERAL("abc")),
		"IO Buffer OOM setup failed"
	);
	testRequire(xrtMemDebugFailAfter(0u), "Buffer Reader OOM setup failed");
	pReader = xrtReaderFromBuffer(&Buffer);
	testRequire(
		(pReader == NULL) && xrtMemDebugFailTriggered() &&
		(Buffer.Size == 3u) &&
		(memcmp(Buffer.Data, "abc", 3u) == 0),
		"borrowed Buffer Reader OOM changed input"
	);
	xrtMemDebugFailClear();
	xrtClearError();

	pOwned = xrtBufferFrom(XRT_BYTES_LITERAL("owned"));
	testRequire(pOwned != NULL, "owned Buffer OOM setup failed");
	testRequire(xrtMemDebugFailAfter(0u), "owned Buffer Reader OOM setup failed");
	pReader = xrtReaderTakeBuffer(&pOwned);
	testRequire(
		(pReader == NULL) && (pOwned != NULL) &&
		xrtMemDebugFailTriggered(),
		"failed Buffer Reader take consumed ownership"
	);
	xrtMemDebugFailClear();
	xrtClearError();
	xrtBufferDestroy(pOwned);

	testRequire(xrtMemDebugFailAfter(0u), "Buffer Writer OOM setup failed");
	pWriter = xrtWriterFromBuffer(&Buffer);
	testRequire(
		(pWriter == NULL) && xrtMemDebugFailTriggered() &&
		(Buffer.Size == 3u),
		"Buffer Writer construction OOM changed Buffer"
	);
	xrtMemDebugFailClear();
	xrtClearError();

	pReader = xrtReaderFromMemory(
		(xbytesview){ arrLarge, sizeof(arrLarge) }
	);
	testRequire(pReader != NULL, "ReadAll OOM Reader setup failed");
	testRequire(xrtMemDebugFailAfter(1u), "ReadAll growth OOM setup failed");
	pCopy = xrtReaderReadAll(pReader, sizeof(arrLarge));
	testRequire(
		(pCopy == NULL) && xrtMemDebugFailTriggered() &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"ReadAll growth OOM did not propagate"
	);
	xrtMemDebugFailClear();
	xrtClearError();
	xrtReaderDestroy(pReader);

	pWriter = xrtWriterFromBuffer(&Buffer);
	testRequire(pWriter != NULL, "Buffer write OOM Writer setup failed");
	testRequire(xrtBufferTrim(&Buffer), "Buffer write OOM trim failed");
	testRequire(xrtMemDebugFailAfter(0u), "Buffer write OOM setup failed");
	testRequire(
		!xrtWriterWriteFull(pWriter, arrLarge, sizeof(arrLarge), NULL) &&
		xrtMemDebugFailTriggered() && (Buffer.Size == 3u) &&
		(memcmp(Buffer.Data, "abc", 3u) == 0),
		"Buffer Writer growth OOM changed state"
	);
	xrtMemDebugFailClear();
	xrtClearError();
	xrtWriterDestroy(pWriter);
	xrtBufferUnit(&Buffer);
	testMemoryDebugDrain("IO Buffer OOM test leaked memory");
	printf("[PASS] IO buffer OOM\n");
	return 0;
}
