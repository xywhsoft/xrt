#include <stdio.h>

#include <xrt.h>



/*
 * 范例：tls/stream_future —— 已握手流的 Future 化收发
 * ----------------------------------------------------------------
 * 演示 API：
 *   TLS Stream 读写 Future 入口（无返回值操作）
 *   拉取一批明文结果；跨 Future 生命周期可增引用
 * 模块宏：XRT_MODULE_TLS_STREAM（依赖 FUTURE）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/tls/stream_future/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   TLS Stream Future example is ready
 *
 * Stream 同步 Read 会阻塞/回调；Future 入口把"读到一批
 *   明文"变成可等待对象——挂到事件循环或 any/all 组合。
 *   本范例演示 API 形态（真实收发需已握手流，参数注入）。
 */


/* 在普通线程等待无返回值操作成功，并释放调用方 Future 引用。 */
static bool exampleTlsFutureResolved(xfuture* pFuture)
{
	bool bResolved =
		(pFuture != NULL) &&
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(10000000)
		 ) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED);

	xrtFutureDestroy(pFuture);
	return bResolved;
}



/* 复制提交一条消息，并等待 TLS 与 TCP 用户态发送队列完全排空。 */
static bool exampleTlsFutureSend(
	xtlsstream* pStream,
	const void* pData,
	size_t iSize
)
{
	if ( !exampleTlsFutureResolved(xrtTlsStreamSendAsync(
		pStream,
		pData,
		iSize
	)) ) {
		return false;
	}
	return exampleTlsFutureResolved(xrtTlsStreamWaitAsync(
		pStream,
		XTLS_STREAM_WAIT_DRAIN
	));
}



/* 拉取当前一批明文；需要越过 Future 生命周期时可增加结果引用。 */
static bool exampleTlsFutureReceive(xtlsstream* pStream)
{
	xfuture* pFuture = xrtTlsStreamRecvAsync(
		pStream,
		64u * 1024u
	);
	const xnetbytes* pBytes;
	xbytesview View;
	bool bReceived;

	if ( (pFuture == NULL) ||
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(10000000)
		 ) != XWAIT_OK) ||
		(xrtFutureState(pFuture) != XFUTURE_RESOLVED) ) {
		xrtFutureDestroy(pFuture);
		return false;
	}
	pBytes = (const xnetbytes*)xrtFutureValue(pFuture);
	View = xrtNetBytesView(pBytes);
	bReceived = pBytes != NULL;
	if ( bReceived && (View.Size != 0) ) {
		(void)fwrite(
			View.Data,
			1u,
			View.Size,
			stdout
		);
	}
	xrtFutureDestroy(pFuture);
	return bReceived;
}



/* 示例函数接收已经完成握手的客户端或服务端 TLS Stream。 */
int main(void)
{
	(void)exampleTlsFutureSend;
	(void)exampleTlsFutureReceive;
	printf("TLS Stream Future example is ready\n");
	return 0;
}
