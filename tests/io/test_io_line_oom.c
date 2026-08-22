#include "../test.h"



/* 验证构造和首次动态缓冲扩展的 OOM 所有权契约。 */
int main(void)
{
	xreader* pReader;
	xlinereader* pLines;
	xlineview Line;
	xlineview Saved;

	/* 借用构造失败不得改变底层 Reader。 */
	pReader = xrtReaderFromMemory(XRT_BYTES_LITERAL("alpha\n"));
	testRequire(pReader != NULL, "Line Reader OOM source setup failed");
	testRequire(xrtMemDebugFailAfter(0u), "Line Reader create OOM setup failed");
	pLines = xrtLineReaderCreate(pReader, 32u);
	testRequire(
		(pLines == NULL) && xrtMemDebugFailTriggered(),
		"borrowed Line Reader construction OOM was not observed"
	);
	xrtMemDebugFailClear();
	xrtClearError();
	xrtReaderDestroy(pReader);

	/* 接管构造失败必须保留来源槽。 */
	pReader = xrtReaderFromMemory(XRT_BYTES_LITERAL("beta\n"));
	testRequire(pReader != NULL, "Line Reader take OOM source setup failed");
	testRequire(xrtMemDebugFailAfter(0u), "Line Reader take OOM setup failed");
	pLines = xrtLineReaderTake(&pReader, 32u);
	testRequire(
		(pLines == NULL) && (pReader != NULL) &&
		xrtMemDebugFailTriggered(),
		"Line Reader take OOM consumed source ownership"
	);
	xrtMemDebugFailClear();
	xrtClearError();
	xrtReaderDestroy(pReader);

	/* 缓冲扩展失败不发布部分行，并使已消费语义的迭代器终止。 */
	pReader = xrtReaderFromMemory(XRT_BYTES_LITERAL("gamma\n"));
	pLines = xrtLineReaderTake(&pReader, 32u);
	testRequire(pLines != NULL, "Line Reader growth OOM setup failed");
	Line.Text = XRT_STR_LITERAL("unchanged");
	Line.End = XLINE_END_CRLF;
	Saved = Line;
	testRequire(xrtMemDebugFailAfter(0u), "Line Reader growth failure setup failed");
	testRequire(
		(xrtLineReaderNext(pLines, &Line) == XLINE_NEXT_ERROR) &&
		xrtMemDebugFailTriggered() &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(memcmp(&Line, &Saved, sizeof(Line)) == 0),
		"Line Reader growth OOM published partial output"
	);
	xrtMemDebugFailClear();
	xrtClearError();
	testRequire(
		(xrtLineReaderNext(pLines, &Line) == XLINE_NEXT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"Line Reader growth OOM did not poison the iterator"
	);
	xrtClearError();
	xrtLineReaderDestroy(pLines);

	testMemoryDebugDrain("IO Line Reader OOM test leaked memory");
	printf("[PASS] IO line reader OOM\n");
	return 0;
}
