/*
 * 范例：io/file —— 文件 Reader/Writer：写入、读回与自动清理
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtWriterOpen / Destroy   打开/关闭拥有式文件写入器
 *   xrtReaderOpen / Destroy   打开/关闭拥有式文件读取器
 *   xrtWriterWriteFull        写满指定字节（短写即失败）
 *   xrtReaderReadFull         读满指定字节（不足即失败）
 *   xrtFileDelete             删除临时文件（范例收尾）
 * 模块宏：XRT_MODULE_IO（依赖 FILE）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/io/file/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   xrt io
 *
 * 与 io/buffer 对照：业务调用完全相同（WriteFull/ReadFull），
 *   只有适配器创建一行不同（Open 文件 vs FromBuffer）——
 *   这就是 Reader/Writer 抽象换存储零改动的证明。
 * Destroy 返回 bool：关闭时的刷新/落盘失败会被如实报告。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	static const char sPath[] = "xrt-io-example.bin";
	unsigned char arrData[7];
	xwriter* pWriter = xrtWriterOpen(sPath);   /* 拥有式：Destroy 关文件 */
	xreader* pReader;
	int iResult = 1;

	/* 写 7 字节 "xrt io\n" 后立即关闭（Destroy 含落盘检查）。 */
	if ( (pWriter == NULL) ||
		 !xrtWriterWriteFull(pWriter, "xrt io\n", sizeof(arrData), NULL) ||
		 !xrtWriterDestroy(pWriter) ) {
		return 1;
	}

	/* 重新打开读取：ReadFull 保证读满 7 字节，否则失败。 */
	pReader = xrtReaderOpen(sPath);
	if ( (pReader != NULL) &&
		 xrtReaderReadFull(pReader, arrData, sizeof(arrData), NULL) ) {
		fwrite(arrData, 1u, sizeof(arrData), stdout);
		iResult = 0;
	}
	xrtReaderDestroy(pReader);

	/* 范例产物随用随删，不污染工作目录。 */
	(void)xrtFileDelete(sPath);
	return iResult;
}
