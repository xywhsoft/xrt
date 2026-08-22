#include <stdio.h>
#include <string.h>

#include <xrt.h>



/* 引用分片发送完成后归还业务负载。 */
static void releaseChunk(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pContext;
	(void)iSize;
	xrtFree((ptr)pData);
}



/* 分配一个由 Writer 成功接管、失败时由调用方收回的分片。 */
static xnetresult writeChunk(
	xwswriter* pWriter,
	xbytesview Chunk,
	bool bFinal
)
{
	bytes pOwned = (bytes)xrtMalloc(Chunk.Size);
	xnetref Ref;
	xnetresult Result;

	if ( pOwned == NULL ) {
		return XNET_RESULT_ERROR;
	}
	memcpy(pOwned, Chunk.Data, Chunk.Size);
	Ref = (xnetref) {
		pOwned,
		Chunk.Size,
		releaseChunk,
		NULL
	};
	Result = bFinal ?
		xrtWsWriterFinishRef(pWriter, &Ref) :
		xrtWsWriterWriteRef(pWriter, &Ref);
	if ( Result != XNET_RESULT_OK ) {
		xrtFree(pOwned);
	}
	return Result;
}



/* 在 Connection 所属 Worker 上发送一条不复制服务端明文负载的 Binary 消息。 */
static xnetresult sendReferenced(
	xwsconn* pConnection
)
{
	xwswriter* pWriter = xrtWsConnBeginBinary(
		pConnection
	);
	xnetresult Result;

	if ( pWriter == NULL ) {
		return XNET_RESULT_ERROR;
	}
	Result = writeChunk(
		pWriter,
		XRT_BYTES_LITERAL("first-"),
		false
	);
	if ( Result == XNET_RESULT_OK ) {
		Result = writeChunk(
			pWriter,
			XRT_BYTES_LITERAL("last"),
			true
		);
	}
	xrtWsWriterDestroy(pWriter);
	return Result;
}



/* 示例函数由已经建立连接的 Worker 回调调用。 */
int main(void)
{
	(void)sendReferenced;
	printf("WebSocket Writer reference example is ready\n");
	return 0;
}
