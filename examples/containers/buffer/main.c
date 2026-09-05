/*
 * 范例：containers/buffer —— 分段缓冲：追加、稀疏写入与无复制移交
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtBufferInit    初始化空缓冲（后续按需分配段）
 *   xrtBufferAppend  追加字节到末尾（自动扩容）
 *   xrtBufferWrite   定点写入：可越过当前末尾，空洞补零
 *   xrtBufferTake    移交所有权：拼接为连续块后"取走"，缓冲变空
 *   tBuffer.Size / Data  公开字段：当前逻辑长度与数据指针
 * 模块宏：XRT_MODULE_BUFFER
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/containers/buffer/main.c -lws2_32 -liphlpapi
 * 预期输出（十六进制转储）：
 *   61 62 63 00 00 7a
 *
 * 读数解释："abc" 占 0..2；Write 在偏移 5 写 'z'（0x7a），
 *   偏移 3..4 越过了当时的末尾，被自动补零。
 * Take 的语义：缓冲让出全部数据（零拷贝移交缓冲区所有权），
 *   之后的 Unit 不再重复释放——这正是网络收包"拼好即取"的用法。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	xbuffer tBuffer;
	bytes pResult;
	size_t iSize;

	if ( !xrtBufferInit(&tBuffer) ) {
		return 1;
	}

	/*
	 * 两步构造内容：
	 *   Append "abc"        → 长度 3；
	 *   Write(5, "z")       → 长度跳到 6，中间 2 字节零填充。
	 * 定长协议头（如"第 5 字节是标志位"）的场景常这样用。
	 */
	if (
		!xrtBufferAppend(&tBuffer, XRT_BYTES_LITERAL("abc")) ||
		!xrtBufferWrite(&tBuffer, 5, XRT_BYTES_LITERAL("z"))
	) {
		xrtBufferUnit(&tBuffer);
		return 2;
	}

	/* 逐字节十六进制转储：61 62 63 00 00 7a。 */
	for ( size_t i = 0; i < tBuffer.Size; i++ ) {
		printf("%02x%s", tBuffer.Data[i],
			i + 1u == tBuffer.Size ? "\n" : " ");
	}

	/*
	 * 无复制取走：拿到连续缓冲的所有权（由 xrtFree 释放），
	 * 原缓冲归零可继续复用。iSize 应为 6。
	 */
	pResult = xrtBufferTake(&tBuffer, &iSize, NULL);
	if ( (pResult == NULL) || (iSize != 6) ) {
		xrtBufferUnit(&tBuffer);
		return 3;
	}
	xrtFree(pResult);
	xrtBufferUnit(&tBuffer);
	return 0;
}
