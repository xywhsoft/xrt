#include "../test.h"



typedef struct testmailwiresink {
	unsigned char Data[128];
	size_t Size;
	size_t Calls;
	size_t FailAfter;
} testmailwiresink;



/* 收集增量 dot writer 输出并支持确定性回调失败。 */
static bool testMailWireWrite(xbytesview Data, ptr pUserData)
{
	testmailwiresink* pSink = (testmailwiresink*)pUserData;

	if ( (pSink->FailAfter != 0) && (pSink->Calls >= pSink->FailAfter) ) {
		return false;
	}
	if ( Data.Size > (sizeof(pSink->Data) - pSink->Size) ) {
		return false;
	}
	memcpy(pSink->Data + pSink->Size, Data.Data, Data.Size);
	pSink->Size += Data.Size;
	pSink->Calls++;
	return true;
}



/* 验证增量行探测不消费不完整输入。 */
static void testMailLine(void)
{
	xstrview Line;
	size_t iConsumed = SIZE_MAX;

	testRequire(xrtMailLineRead(
		XRT_STR_LITERAL("250-first\r\nrest"),
		0,
		&Line,
		&iConsumed
	) == XMAIL_NEXT_ITEM, "mail wire line read failed");
	testRequire(testMailViewEqual(Line, XRT_STR_LITERAL("250-first")) &&
		(iConsumed == 11u), "mail wire line view mismatch");
	testRequire(xrtMailLineRead(
		XRT_STR_LITERAL("partial\r"),
		0,
		&Line,
		&iConsumed
	) == XMAIL_NEXT_END, "mail wire partial CR was not retained");
	testRequire(xrtMailLineRead(
		XRT_STR_LITERAL("bare\n"),
		0,
		&Line,
		&iConsumed
	) == XMAIL_NEXT_ERROR, "mail wire accepted bare LF");
	testRequire(xrtMailLineRead(
		XRT_STR_LITERAL("12345"),
		4u,
		&Line,
		&iConsumed
	) == XMAIL_NEXT_ERROR, "mail wire ignored line limit");
}



/* 验证 dot 行视图、编码终止和完整解码。 */
static void testMailDot(void)
{
	static const char sPlain[] = ".first\r\nsecond\r\n..third";
	static const char sWire[] = "..first\r\nsecond\r\n...third\r\n.\r\n";
	xstrview Data;
	unsigned char arrOutput[64];
	size_t iSize;
	bytes pDecoded;

	testRequire(xrtMailDotLine(XRT_STR_LITERAL("."), &Data) == XMAIL_NEXT_END,
		"mail dot terminator was not recognized");
	testRequire(xrtMailDotLine(XRT_STR_LITERAL("..value"), &Data) ==
		XMAIL_NEXT_ITEM, "mail dot line decode failed");
	testRequire(testMailViewEqual(Data, XRT_STR_LITERAL(".value")),
		"mail dot line view mismatch");

	testRequire(xrtMailDotWrite(
		testMailViewN(sPlain, sizeof(sPlain) - 1u),
		true,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	), "mail dot encode failed");
	testRequire((iSize == sizeof(sWire) - 1u) &&
		(memcmp(arrOutput, sWire, iSize) == 0),
		"mail dot encoded bytes mismatch");
	pDecoded = xrtMailDotDecode(
		testMailViewN(sWire, sizeof(sWire) - 1u),
		true,
		&iSize
	);
	testRequire((pDecoded != NULL) &&
		testMailViewEqual(
			testMailViewN((const char*)pDecoded, iSize),
			XRT_STR_LITERAL(".first\r\nsecond\r\n..third\r\n")
		), "mail dot decoded bytes mismatch");
	xrtFree(pDecoded);
}



/* 验证任意分块边界下的增量 dot transparency。 */
static void testMailDotWriter(void)
{
	static const char sExpected[] =
		"..first\r\nsecond\r\n...third\r\n.\r\n";
	xmaildotwriter Writer;
	testmailwiresink Sink = { 0 };

	testRequire(xrtMailDotWriterInit(&Writer) &&
		xrtMailDotWriterWrite(
			&Writer,
			XRT_BYTES_LITERAL("."),
			testMailWireWrite,
			&Sink
		) && xrtMailDotWriterWrite(
			&Writer,
			XRT_BYTES_LITERAL("first\r"),
			testMailWireWrite,
			&Sink
		) && Writer.PendingCr && xrtMailDotWriterWrite(
			&Writer,
			XRT_BYTES_LITERAL("\nsecond\r\n."),
			testMailWireWrite,
			&Sink
		) && xrtMailDotWriterWrite(
			&Writer,
			XRT_BYTES_LITERAL(".third"),
			testMailWireWrite,
			&Sink
		) && xrtMailDotWriterFinish(
			&Writer,
			testMailWireWrite,
			&Sink
		) && Writer.Finished && (Sink.Size == sizeof(sExpected) - 1u) &&
		(memcmp(Sink.Data, sExpected, Sink.Size) == 0),
		"incremental mail dot output mismatch");
}



/* 验证增量输入错误不发布当前片段，sink 失败会终止 writer。 */
static void testMailDotWriterErrors(void)
{
	xmaildotwriter Writer;
	testmailwiresink Sink = { 0 };
	size_t iBefore;

	testRequire(xrtMailDotWriterInit(&Writer) && xrtMailDotWriterWrite(
		&Writer,
		XRT_BYTES_LITERAL("ok\r"),
		testMailWireWrite,
		&Sink
	), "incremental mail dot partial CR setup failed");
	iBefore = Sink.Size;
	testRequire(!xrtMailDotWriterWrite(
		&Writer,
		XRT_BYTES_LITERAL("bad"),
		testMailWireWrite,
		&Sink
	) && (Sink.Size == iBefore) && Writer.PendingCr,
		"incremental mail dot bare CR published output");
	testRequire(xrtMailDotWriterWrite(
		&Writer,
		XRT_BYTES_LITERAL("\n"),
		testMailWireWrite,
		&Sink
	) && xrtMailDotWriterFinish(&Writer, testMailWireWrite, &Sink),
		"incremental mail dot did not recover from rejected input");

	testRequire(xrtMailDotWriterInit(&Writer),
		"incremental mail dot callback setup failed");
	memset(&Sink, 0, sizeof(Sink));
	Sink.FailAfter = 1u;
	testRequire(!xrtMailDotWriterWrite(
		&Writer,
		XRT_BYTES_LITERAL(".value"),
		testMailWireWrite,
		&Sink
	) && Writer.Finished,
		"incremental mail dot callback failure was not terminal");
}



/* 验证 dot 写入失败保持目标缓冲，异常终止结构被拒绝。 */
static void testMailDotEdges(void)
{
	unsigned char arrOutput[8];
	size_t iSize = 0;

	memcpy(arrOutput, "keep", 5u);
	testRequire(!xrtMailDotWrite(
		XRT_STR_LITERAL(".long line"),
		true,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (memcmp(arrOutput, "keep", 5u) == 0),
		"mail dot short buffer published partial output");
	testRequire(!xrtMailDotWrite(
		XRT_STR_LITERAL("bad\nline"),
		true,
		NULL,
		0,
		&iSize
	), "mail dot accepted bare LF");
	testRequire(!xrtMailDotDecodeWrite(
		XRT_STR_LITERAL("line\r\n"),
		true,
		NULL,
		0,
		&iSize
	), "mail dot decode accepted missing terminator");
	testRequire(!xrtMailDotDecodeWrite(
		XRT_STR_LITERAL(".\r\nextra\r\n"),
		true,
		NULL,
		0,
		&iSize
	), "mail dot decode accepted data after terminator");
}



/* 运行邮件线路原语测试。 */
int main(void)
{
	testMailLine();
	testMailDot();
	testMailDotWriter();
	testMailDotWriterErrors();
	testMailDotEdges();
	return 0;
}
