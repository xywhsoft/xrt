#include <stdio.h>

#include <xrt.h>



/* 范例只统计运行时产生的线路负载。 */
static bool onCompressed(xbytesview Data, ptr pData)
{
	size_t* pSize = (size_t*)pData;

	*pSize += Data.Size;
	return true;
}



/*
 * 范例：websocket/deflater —— 流式压缩消息（同步尾部处理）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtWsDeflaterConfigInit / Create / Destroy   配置与生命周期
 *   xrtWsDeflaterBegin / Write / End   消息三段式（FIN 参数）
 *   NoContextTakeover   每条消息重置压缩上下文
 * 模块宏：XRT_MODULE_WEBSOCKET（DEFLATE 特性，依赖 COMPRESS）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/websocket/deflater/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   wire bytes=30
 *
 * "去除同步尾部"：RFC 7692 要求每条消息的 deflate 流末尾
 *   不带 4 字节空块同步标记——End 负责修剪，30 字节即修剪后
 *   的线路长度。压缩数据分片推给回调（不落中间缓冲），
 *   直接可进帧发送器。
 */


/* 流式压缩一条消息，并展示去除同步尾部后的长度。 */
int main(void)
{
	xwsdeflaterconfig Config;
	xwsdeflater* pDeflater;
	size_t iSize = 0;

	xrtWsDeflaterConfigInit(&Config);
	Config.NoContextTakeover = true;
	pDeflater = xrtWsDeflaterCreate(&Config);
	if ( (pDeflater == NULL) ||
		!xrtWsDeflaterBegin(pDeflater, true) ||
		!xrtWsDeflaterWrite(
			pDeflater,
			XRT_BYTES_LITERAL("Hello permessage-deflate"),
			onCompressed,
			&iSize
		) ||
		!xrtWsDeflaterEnd(
			pDeflater,
			onCompressed,
			&iSize
		) ) {
		xrtWsDeflaterDestroy(pDeflater);
		return 1;
	}
	printf("wire bytes=%zu\n", iSize);
	xrtWsDeflaterDestroy(pDeflater);
	return 0;
}
