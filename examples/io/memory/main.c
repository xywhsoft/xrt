/*
 * 范例：io/memory —— 固定内存读写适配器与 Reader→Writer 复制
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtReaderFromMemory  把只读内存包装成 Reader
 *   xrtWriterFromMemory  把固定缓冲包装成 Writer（容量=缓冲大小）
 *   xrtReaderCopy        从 Reader 复制到 Writer，出参字节数
 * 模块宏：XRT_MODULE_IO
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/io/memory/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   hello xrt io (12 bytes)
 *
 * 固定内存 Writer 的边界语义：容量就是缓冲大小（本例 32），
 *   写满后 Copy 停止并如实返回已复制字节数——
 *   适合"有限预览缓冲"（如日志前 N 字节）这类硬上限场景；
 *   需要自动增长时改用 io/buffer 的 Buffer 适配器。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	unsigned char arrOutput[32] = { 0 };   /* 目标：固定缓冲 */
	xreader* pReader = xrtReaderFromMemory(
		XRT_BYTES_LITERAL("hello xrt io")
	);
	xwriter* pWriter = xrtWriterFromMemory(
		arrOutput,
		sizeof(arrOutput)
	);
	uint64 iCopied = 0;
	int iResult = 1;

	/*
	 * 通用复制：不关心两端是什么存储——
	 * 这里是 内存→内存；换成 文件→缓冲 / 网络→文件
	 * 只是换两个适配器，Copy 一行不变。
	 */
	if ( (pReader != NULL) && (pWriter != NULL) &&
		 xrtReaderCopy(pReader, pWriter, &iCopied) ) {
		printf("%.*s (%llu bytes)\n",
			(int)iCopied,
			(const char*)arrOutput,
			(unsigned long long)iCopied);
		iResult = 0;
	}
	xrtReaderDestroy(pReader);
	xrtWriterDestroy(pWriter);
	return iResult;
}
