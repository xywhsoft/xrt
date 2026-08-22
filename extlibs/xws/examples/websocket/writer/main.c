#include <stdio.h>

#include <xws.h>



/* 在 Connection 所属 Worker 上把多个业务分块写成一条 Text 消息。 */
static xnetresult sendChunks(
	xwsconn* pConnection,
	const xstrview* pChunks,
	size_t iCount
)
{
	xwswriter* pWriter;
	xnetresult Result;

	if ( (pChunks == NULL) || (iCount == 0) ) {
		return XNET_RESULT_ERROR;
	}
	pWriter = xrtWsConnBeginText(pConnection);
	if ( pWriter == NULL ) {
		return XNET_RESULT_ERROR;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		xbytesview Chunk = {
			(cbytes)pChunks[i].Data,
			pChunks[i].Size
		};

		Result = (i + 1u) == iCount ?
			xrtWsWriterFinish(pWriter, Chunk) :
			xrtWsWriterWrite(pWriter, Chunk);
		if ( Result != XNET_RESULT_OK ) {
			xrtWsWriterDestroy(pWriter);
			return Result;
		}
	}
	xrtWsWriterDestroy(pWriter);
	return XNET_RESULT_OK;
}



/* 示例函数由已经建立连接的 Worker 回调调用。 */
int main(void)
{
	(void)sendChunks;
	printf("WebSocket Writer example is ready\n");
	return 0;
}
