#include "../test.h"



/* 可控短读来源用于压实跨读取边界的行终止符。 */
typedef struct test_line_source {
	cbytes Data;
	size_t Size;
	size_t Position;
	size_t Chunk;
	int CloseCount;
	bool FailClose;
} test_line_source;



/* 按配置的最大分片执行一次短读。 */
static bool testLineRead(
	ptr pContext,
	ptr pBuffer,
	size_t iRequest,
	size_t* pRead
)
{
	test_line_source* pSource = (test_line_source*)pContext;
	size_t iRemain = pSource->Size - pSource->Position;
	size_t iDone = iRequest < iRemain ? iRequest : iRemain;

	if ( iDone > pSource->Chunk ) {
		iDone = pSource->Chunk;
	}
	if ( iDone != 0u ) {
		memcpy(pBuffer, pSource->Data + pSource->Position, iDone);
		pSource->Position += iDone;
	}
	*pRead = iDone;
	return true;
}



/* 记录底层 Reader 是否只关闭一次。 */
static bool testLineClose(ptr pContext)
{
	test_line_source* pSource = (test_line_source*)pContext;
	xerror* pError;

	pSource->CloseCount++;
	if ( pSource->FailClose ) {
		pError = xrtErrorCreate(
			XERR_IO,
			"test.io.line",
			1,
			"injected line source close failure"
		);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return false;
	}
	return true;
}



/* 创建使用调用方来源状态的短读 Reader。 */
static xreader* testLineReader(test_line_source* pSource)
{
	xreaderops Ops;

	memset(&Ops, 0, sizeof(Ops));
	Ops.Read = testLineRead;
	Ops.Close = testLineClose;
	return xrtReaderCreate(&Ops, pSource);
}



/* 读取并核对一条借用行。 */
static void testLineRequire(
	xlinereader* pLines,
	const void* pText,
	size_t iSize,
	xlineend End
)
{
	xlineview Line;

	testRequire(
		xrtLineReaderNext(pLines, &Line) == XLINE_NEXT_LINE,
		"Line Reader did not return an expected line"
	);
	testRequire(
		(Line.Text.Size == iSize) && (Line.End == End) &&
		((iSize == 0u) || (memcmp(Line.Text.Data, pText, iSize) == 0)),
		"Line Reader contents or terminator mismatch"
	);
}



/* 验证短读、空行、CRLF、无终止符末行和借用所有权。 */
static void testLineBoundaries(void)
{
	static const uint8 Data[] =
		"alpha\r\n\nbeta\nlast";
	test_line_source Source;
	xreader* pReader;
	xlinereader* pLines;
	xlineview Line;

	memset(&Source, 0, sizeof(Source));
	Source.Data = Data;
	Source.Size = sizeof(Data) - 1u;
	Source.Chunk = 1u;
	pReader = testLineReader(&Source);
	testRequire(pReader != NULL, "short Line Reader source creation failed");
	pLines = xrtLineReaderCreate(pReader, 16u);
	testRequire(pLines != NULL, "borrowed Line Reader creation failed");

	testLineRequire(pLines, "alpha", 5u, XLINE_END_CRLF);
	testLineRequire(pLines, "", 0u, XLINE_END_LF);
	testLineRequire(pLines, "beta", 4u, XLINE_END_LF);
	testLineRequire(pLines, "last", 4u, XLINE_END_NONE);
	testRequire(
		xrtLineReaderNext(pLines, &Line) == XLINE_NEXT_END,
		"Line Reader did not report stable input end"
	);
	testRequire(
		xrtLineReaderNext(pLines, &Line) == XLINE_NEXT_END,
		"Line Reader end was not idempotent"
	);
	testRequire(
		xrtLineReaderDestroy(pLines) && (Source.CloseCount == 0),
		"borrowed Line Reader closed its source"
	);
	testRequire(
		xrtReaderDestroy(pReader) && (Source.CloseCount == 1),
		"borrowed Line Reader source close mismatch"
	);
}



/* 验证二进制零字节、接管语义和精确内容上限。 */
static void testLineOwnershipAndLimit(void)
{
	static const uint8 Binary[] = { 'a', 0u, 'b', '\n' };
	test_line_source Source;
	xreader* pReader;
	xlinereader* pLines;
	xlineview Line;

	memset(&Source, 0, sizeof(Source));
	Source.Data = Binary;
	Source.Size = sizeof(Binary);
	Source.Chunk = sizeof(Binary);
	pReader = testLineReader(&Source);
	testRequire(pReader != NULL, "owned Line Reader source creation failed");
	pLines = xrtLineReaderTake(&pReader, 3u);
	testRequire(
		(pLines != NULL) && (pReader == NULL),
		"Line Reader take did not consume the source slot"
	);
	testLineRequire(pLines, Binary, 3u, XLINE_END_LF);
	testRequire(
		xrtLineReaderNext(pLines, &Line) == XLINE_NEXT_END,
		"binary Line Reader did not end"
	);
	testRequire(
		xrtLineReaderDestroy(pLines) && (Source.CloseCount == 1),
		"owned Line Reader did not close its source once"
	);

	pReader = xrtReaderFromMemory(XRT_BYTES_LITERAL("abcd\r\n"));
	pLines = xrtLineReaderTake(&pReader, 4u);
	testRequire(pLines != NULL, "exact-limit Line Reader creation failed");
	testLineRequire(pLines, "abcd", 4u, XLINE_END_CRLF);
	testRequire(
		xrtLineReaderNext(pLines, &Line) == XLINE_NEXT_END,
		"exact-limit Line Reader did not end"
	);
	xrtLineReaderDestroy(pLines);
}



/* 验证接管模式传播底层关闭错误，同时仍然释放全部对象。 */
static void testLineCloseFailure(void)
{
	test_line_source Source;
	xreader* pReader;
	xlinereader* pLines;

	memset(&Source, 0, sizeof(Source));
	Source.Data = (cbytes)"";
	Source.Chunk = 1u;
	Source.FailClose = true;
	pReader = testLineReader(&Source);
	pLines = xrtLineReaderTake(&pReader, 8u);
	testRequire(pLines != NULL, "close-failure Line Reader setup failed");
	testRequire(
		!xrtLineReaderDestroy(pLines) && (Source.CloseCount == 1) &&
		(xrtErrorKind(xrtGetError()) == XERR_IO) &&
		strcmp(xrtErrorDomain(xrtGetError()), "test.io.line") == 0,
		"Line Reader did not propagate source close failure"
	);
	xrtClearError();
}



/* 验证超限、无终止符超限和失败后的终止状态。 */
static void testLineFailures(void)
{
	xreader* pReader;
	xlinereader* pLines;
	xlineview Line;
	xlineview Saved;

	Line.Text = XRT_STR_LITERAL("unchanged");
	Line.End = XLINE_END_CRLF;
	Saved = Line;
	pReader = xrtReaderFromMemory(XRT_BYTES_LITERAL("abcde\n"));
	pLines = xrtLineReaderTake(&pReader, 4u);
	testRequire(pLines != NULL, "overflow Line Reader creation failed");
	testRequire(
		(xrtLineReaderNext(pLines, &Line) == XLINE_NEXT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(xrtErrorCode(xrtGetError()) == XIO_ERROR_LIMIT) &&
		memcmp(&Line, &Saved, sizeof(Line)) == 0,
		"terminated overlong line did not fail atomically"
	);
	xrtClearError();
	testRequire(
		(xrtLineReaderNext(pLines, &Line) == XLINE_NEXT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"failed Line Reader remained usable"
	);
	xrtClearError();
	xrtLineReaderDestroy(pLines);

	pReader = xrtReaderFromMemory(XRT_BYTES_LITERAL("abcde"));
	pLines = xrtLineReaderTake(&pReader, 4u);
	testRequire(
		(pLines != NULL) &&
		(xrtLineReaderNext(pLines, &Line) == XLINE_NEXT_ERROR) &&
		(xrtErrorCode(xrtGetError()) == XIO_ERROR_LIMIT),
		"unterminated overlong line was accepted"
	);
	xrtClearError();
	xrtLineReaderDestroy(pLines);

	pReader = xrtReaderFromMemory(XRT_BYTES_LITERAL("x"));
	testRequire(
		xrtLineReaderCreate(pReader, 0u) == NULL,
		"zero Line Reader limit was accepted"
	);
	xrtClearError();
	testRequire(
		xrtLineReaderTake(&pReader, SIZE_MAX) == NULL &&
		(pReader != NULL),
		"failed Line Reader take consumed ownership"
	);
	xrtClearError();
	xrtReaderDestroy(pReader);
	testRequire(
		xrtLineReaderDestroy(NULL),
		"null Line Reader destroy was not a no-op"
	);
}



/* 运行流式逐行读取回归。 */
int main(void)
{
	testLineBoundaries();
	testLineOwnershipAndLimit();
	testLineCloseFailure();
	testLineFailures();
	printf("[PASS] IO line reader\n");
	return 0;
}
