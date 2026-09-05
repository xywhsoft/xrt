/*
 * 范例：io/buffer —— 用统一 Writer 接口写内存缓冲（追加 + 定点覆盖）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtWriterFromBuffer  把 xbuffer 适配成 xwriter（借用，不拥有）
 *   xrtWriterWriteFull   写满全部字节（短写即失败）
 *   xrtWriterSeek        定位写入位置（XSEEK_START 从头起算）
 *   xrtWriterDestroy     释放适配器（不动缓冲本身）
 * 模块宏：XRT_MODULE_IO（依赖 BUFFER）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/io/buffer/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   hello xrtld
 *
 * Reader/Writer 抽象的意义：同一段业务代码
 *   （WriteFull/Seek/ReadFull）可以对接内存、文件、网络流——
 *   本例接内存缓冲，io/file 范例接文件，仅适配器一行不同。
 * WriteFull 与 Write 的区别：Full 保证写满指定字节数，
 *   部分写入（如容量不足）视为失败——省去循环重试样板。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	xbuffer Buffer;          /* 底层存储：分段字节缓冲 */
	xwriter* pWriter;        /* 适配器：借用 Buffer，Destroy 不释放它 */
	int iResult = 1;

	if ( !xrtBufferInit(&Buffer) ) {
		return 1;
	}

	/*
	 * 三步构造内容：
	 *   1) 追加 "hello world"（11 字节）；
	 *   2) Seek 回到偏移 6（'w' 的位置，从 START 起算）；
	 *   3) 覆写 "xrt"（3 字节）→ "wor" 变 "xrt"，最终 "hello xrtld"。
	 * 定点改写协议头字段就是这个姿势。
	 */
	pWriter = xrtWriterFromBuffer(&Buffer);
	if ( (pWriter != NULL) &&
		 xrtWriterWriteFull(pWriter, "hello world", 11u, NULL) &&
		 xrtWriterSeek(pWriter, 6, XSEEK_START, NULL) &&
		 xrtWriterWriteFull(pWriter, "xrt", 3u, NULL) ) {
		printf("%.*s\n", (int)Buffer.Size, (const char*)Buffer.Data);
		iResult = 0;
	}

	/* 两级清理：适配器归适配器，缓冲归缓冲。 */
	xrtWriterDestroy(pWriter);
	xrtBufferUnit(&Buffer);
	return iResult;
}
