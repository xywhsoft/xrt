/*
 * 范例：data/buffer_base64 —— Base64 文本一步解码为拥有型缓冲
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtBufferFromBase64  Base64 视图 → 堆分配的 xbuffer*
 *   xrtBufferDestroy     释放 From* 系列返回的缓冲指针
 * 模块宏：XRT_MODULE_BUFFER（依赖 CODEC）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/data/buffer_base64/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   Hello
 *
 * From* 家族的定位：把"解码 + 装缓冲"合成一个调用——
 *   相比 xrtBase64DecodeNew（返回裸字节），
 *   产物是带 Size/Data 字段的 xbuffer，可直接继续
 *   追加/转交（xrtBufferTake）给 IO 或网络层。
 * 注意返回的是堆指针（xbuffer*），不是栈句柄，
 *   释放用 Destroy 而不是 Unit。
 */

#include <stdio.h>
#include <xrt.h>



int main(void)
{
	/* "SGVsbG8=" 即 "Hello" 的 Base64；末参 Flags 传 NULL 用默认。 */
	xbuffer* pBuffer = xrtBufferFromBase64(
		XRT_STR_LITERAL("SGVsbG8="),
		NULL
	);

	if ( pBuffer == NULL ) {
		return 1;
	}

	/* 二进制内容按显式长度打印（Base64 解码物可含零字节）。 */
	printf(
		"%.*s\n",
		(int)pBuffer->Size,
		(const char*)pBuffer->Data
	);
	xrtBufferDestroy(pBuffer);
	return 0;
}
