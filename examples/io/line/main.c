#include <stdio.h>

#include <xrt.h>



/* 流式扫描日志行，不把完整输入读入内存。 */
int main(void)
{
	static const char sLog[] =
		"INFO server started\r\n"
		"WARN queue is busy\n"
		"ERROR request failed";
	xreader* pReader;
	xlinereader* pLines;
	xlineview Line;
	xlinenext Next;
	size_t iCount = 0u;
	int iResult = 1;

	/* Line Reader 接管通用 Reader，缓冲只随实际行长度增长。 */
	pReader = xrtReaderFromMemory(XRT_BYTES_LITERAL(sLog));
	pLines = xrtLineReaderTake(&pReader, 1024u);
	if ( pLines == NULL ) {
		xrtReaderDestroy(pReader);
		return 1;
	}

	/* 借用视图在下一次迭代前有效，空行也会作为有效结果返回。 */
	while ( (Next = xrtLineReaderNext(pLines, &Line)) == XLINE_NEXT_LINE ) {
		printf("%zu: %.*s\n", ++iCount, (int)Line.Text.Size, Line.Text.Data);
	}
	if ( Next == XLINE_NEXT_END ) {
		iResult = 0;
	}

	/* 接管模式由 Line Reader 统一关闭底层 Reader。 */
	if ( !xrtLineReaderDestroy(pLines) ) {
		iResult = 1;
	}
	return iResult;
}
