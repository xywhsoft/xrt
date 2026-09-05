#include <stdio.h>
#include <string.h>

#include <xrt.h>



/*
 * 范例：network/buffer —— 网络缓冲：缓冲池 + 可变尺寸写入口
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtNetBufPoolCreate   创建缓冲池（引用计数共享）
 *   xnetbuf               网络缓冲：池内分配、零拷贝引用
 *   后端/解析器直写接口    可变尺寸追加（内部走池）
 * 模块宏：XRT_MODULE_NET
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/buffer/main.c -lws2_32 -liphlpapi
 * 预期输出：（打印写入内容，具体见运行）
 *
 * 为什么要有专用缓冲体系：收包路径的分配必须走池——
 *   引擎按突发预算预热，峰值零系统分配；
 *   xnetbuf 支持引用计数（一份数据多消费者零拷贝，
 *   见 websocket/stream_ref 的组播用法）。
 */


/* 展示后端或协议解析器直接写入可变尺寸网络缓冲。 */
int main(void)
{
	xnetbufpool* pPool = xrtNetBufPoolCreate(NULL);
	xnetbuf Buffer;
	xnetwspan Write;
	xnetspan Read;

	if ( (pPool == NULL) || !xrtNetBufInit(&Buffer, pPool) ||
		!xrtNetBufReserve(&Buffer, 6, &Write) ) {
		return 1;
	}
	memcpy(Write.Data, "packet", 6);
	if ( !xrtNetBufCommit(&Buffer, 6) ||
		!xrtNetBufFront(&Buffer, &Read) ) {
		return 1;
	}
	printf("bytes=%zu spans=%zu data=%.*s\n",
		xrtNetBufSize(&Buffer), xrtNetBufSpanCount(&Buffer),
		(int)Read.Size, (const char*)Read.Data);
	xrtNetBufClear(&Buffer);
	return xrtNetBufPoolDestroy(pPool) ? 0 : 1;
}
