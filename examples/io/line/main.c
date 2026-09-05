/*
 * 范例：io/line —— 流式逐行扫描：CRLF/LF 通吃、零整读入内存
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtReaderFromMemory  把内存字节适配成通用 Reader
 *   xrtLineReaderTake    创建行读取器并"接管"底层 Reader
 *   xrtLineReaderNext    取下一行，三态返回 + 行视图
 *   xrtLineReaderDestroy 关闭行读取器（接管模式下连带关闭 Reader）
 * 模块宏：XRT_MODULE_IO
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/io/line/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   1: INFO server started
 *   2: WARN queue is busy
 *   3: ERROR request failed
 *
 * 三个关键语义：
 *   - 行尾自适应：\r\n、\n、\r 都识别，视图不含行尾符；
 *   - 末行无换行也照常返回（"ERROR request failed"）；
 *   - Take 是所有权移交：之后只 Destroy 行读取器一个，
 *     底层 Reader 由它统一关闭（这里 Reader 可接文件/网络）。
 * Next 三态：XLINE_NEXT_LINE 有行 / XLINE_NEXT_END 正常结束 /
 *   XLINE_NEXT_ERROR 出错（区别于正常结束，不能只判非 LINE）。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	/* 混合行尾 + 末行无换行的日志样本。 */
	static const char sLog[] =
		"INFO server started\r\n"
		"WARN queue is busy\n"
		"ERROR request failed";
	xreader* pReader;
	xlinereader* pLines;
	xlineview Line;          /* 借用：到下一次 Next 前有效 */
	xlinenext Next;
	size_t iCount = 0u;
	int iResult = 1;

	/*
	 * 内存 → Reader → 行读取器：参数 1024 是初始行容量，
	 * 超长行会自动增长（缓冲只随实际行长扩展，与输入总量无关）。
	 * Take 成功后 pReader 所有权已移交，不得再碰它。
	 */
	pReader = xrtReaderFromMemory(XRT_BYTES_LITERAL(sLog));
	pLines = xrtLineReaderTake(&pReader, 1024u);
	if ( pLines == NULL ) {
		xrtReaderDestroy(pReader);
		return 1;
	}

	/* 逐行消费：每行是借用视图，处理完再取下一行。 */
	while ( (Next = xrtLineReaderNext(pLines, &Line)) == XLINE_NEXT_LINE ) {
		printf("%zu: %.*s\n", ++iCount, (int)Line.Text.Size, Line.Text.Data);
	}
	if ( Next == XLINE_NEXT_END ) {
		iResult = 0;
	}

	/* 接管模式：一个 Destroy 连底层 Reader 一起关闭。 */
	if ( !xrtLineReaderDestroy(pLines) ) {
		iResult = 1;
	}
	return iResult;
}
