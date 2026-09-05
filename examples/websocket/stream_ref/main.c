#include <stdio.h>
#include <string.h>

#include <xrt.h>



/*
 * 范例：websocket/stream_ref —— 零拷贝引用发送：发送完成再释放负载
 * ----------------------------------------------------------------
 * 演示 API：
 *   xnetref                 引用描述：{数据, 长度, 释放回调, 上下文}
 *   xrtWsStreamBinaryRef    以引用方式入队二进制消息
 * 模块宏：XRT_MODULE_WEBSOCKET_STREAM
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/websocket/stream_ref/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   WebSocket Connection reference example is ready
 *
 * 引用发送的价值：负载在 Worker 队列里排队期间不复制——
 *   真正写完 socket 后才调 releasePayload 释放。
 *   组播场景同一个引用可挂到 N 个连接（计数释放），
 *   内存占用从 N 份降为 1 份。本范例演示 API 形态
 *   （真实发送需已建立的连接，见 xws 扩展库的组播）。
 */


/* 引用发送完成后归还业务负载。 */
static void releasePayload(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pContext;
	(void)iSize;
	xrtFree((ptr)pData);
}



/* 在 Connection 所属 Worker 上发送一个可延迟释放的业务负载。 */
static xnetresult sendPayload(
	xwsstream* pConnection,
	xbytesview Payload
)
{
	bytes pOwned;
	xnetref Ref;
	xnetresult Result;

	pOwned = (bytes)xrtMalloc(Payload.Size);
	if ( pOwned == NULL ) {
		return XNET_RESULT_ERROR;
	}
	memcpy(pOwned, Payload.Data, Payload.Size);
	Ref = (xnetref) {
		pOwned,
		Payload.Size,
		releasePayload,
		NULL
	};
	Result = xrtWsStreamBinaryRef(pConnection, &Ref);
	if ( Result != XNET_RESULT_OK ) {
		xrtFree(pOwned);
	}
	return Result;
}



/* 示例函数由已经建立连接的 Worker 回调调用。 */
int main(void)
{
	(void)sendPayload;
	printf("WebSocket Connection reference example is ready\n");
	return 0;
}
