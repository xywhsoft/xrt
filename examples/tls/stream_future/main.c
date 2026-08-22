#include <stdio.h>

#include <xrt.h>



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
