#include <stdio.h>

#include <xws.h>



/* 等待一个无返回值 Future 成功完成，并统一释放调用方引用。 */
static bool waitResolved(xfuture* pFuture)
{
	bool bResult =
		(pFuture != NULL) &&
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(10000000)
		 ) == XWAIT_OK) &&
		(xrtFutureState(pFuture) ==
		 XFUTURE_RESOLVED);

	xrtFutureDestroy(pFuture);
	return bResult;
}



/*
	从非 Worker 线程发送消息，等待本地排空，再完成标准关闭握手。
	真实程序可把同一批 Future 交给 xrtFutureAwait*，无需改用 WebSocket 专用协程 API。
*/
static bool sendAndClose(xwsconn* pConnection)
{
	xfuture* pClosed;
	xfuture* pClose;

	if ( !waitResolved(xrtWsConnTextAsync(
		pConnection,
		XRT_STR_LITERAL("hello")
	)) || !waitResolved(xrtWsConnPongAsync(
		pConnection,
		XRT_BYTES_LITERAL("ready")
	)) || !waitResolved(xrtWsConnWaitAsync(
		pConnection,
		XWS_CONN_WAIT_DRAIN
	)) ) {
		return false;
	}

	pClosed = xrtWsConnWaitAsync(
		pConnection,
		XWS_CONN_WAIT_CLOSE
	);
	pClose = xrtWsConnCloseAsync(
		pConnection,
		XWS_CLOSE_NORMAL,
		XRT_STR_LITERAL("done")
	);
	if ( !waitResolved(pClose) ) {
		xrtFutureDestroy(pClosed);
		return false;
	}
	return waitResolved(pClosed);
}



/* 示例函数由已建立连接的客户端或服务端业务线程调用。 */
int main(void)
{
	(void)sendAndClose;
	printf("WebSocket Connection Future example is ready\n");
	return 0;
}
