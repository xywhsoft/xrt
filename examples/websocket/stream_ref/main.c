#include <stdio.h>
#include <string.h>

#include <xrt.h>



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
