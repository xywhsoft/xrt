#include <stdio.h>
#include <string.h>
#include <xrt.h>



/* 接管 Server 交付的连接；Stream 事件表已经由 Server 统一安装。 */
static bool serverAccept(
	xnetserver* pServer,
	size_t iEndpoint,
	xnetstream* pStream,
	ptr pData
)
{
	(void)pServer;
	(void)iEndpoint;
	(void)pStream;
	(void)pData;
	return true;
}



/* 直接移动接收缓冲，避免回显路径复制载荷。 */
static void serverRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	(void)pData;
	if ( xrtNetStreamSendBuffer(pStream, pBuffer) ==
		XNET_RESULT_ERROR ) {
		(void)xrtNetStreamAbort(pStream);
	}
}



/* 对端结束写方向后排空回显并正常关闭。 */
static void serverEnd(xnetstream* pStream, ptr pData)
{
	(void)pData;
	(void)xrtNetStreamClose(pStream);
}



/* 释放 Accept 回调接管的 Stream 引用。 */
static void serverStreamClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	(void)Result;
	(void)pError;
	(void)pData;
	xrtNetStreamDestroy(pStream);
}



/* 启动一个按需缓冲、带硬背压的 TCP 回显 Server。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetserverconfig ServerConfig;
	xnetserverevents ServerEvents;
	xnetstreamevents StreamEvents;
	xnetengine* pEngine;
	xnetserver* pServer;
	xnetaddr Address;

	xrtNetEngineConfigInit(&EngineConfig);
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		return 1;
	}
	xrtNetServerConfigInit(&ServerConfig);
	(void)xrtNetAddrLoopback(
		&ServerConfig.Listen.Address,
		XNET_FAMILY_IPV4,
		0
	);
	memset(&ServerEvents, 0, sizeof(ServerEvents));
	ServerEvents.Accept = serverAccept;
	memset(&StreamEvents, 0, sizeof(StreamEvents));
	StreamEvents.Read = serverRead;
	StreamEvents.End = serverEnd;
	StreamEvents.Close = serverStreamClose;
	pServer = xrtNetServerStart(
		pEngine,
		&ServerConfig,
		&ServerEvents,
		&StreamEvents,
		NULL
	);
	if ( (pServer == NULL) ||
		!xrtNetServerLocal(pServer, 0, &Address) ) {
		return 2;
	}
	printf("TCP echo server listening on 127.0.0.1:%u\n",
		(unsigned)Address.Port);
	printf("press Enter to stop accepting\n");
	(void)getchar();
	(void)xrtNetServerClose(pServer);
	while ( xrtNetServerState(pServer) != XNET_SERVER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetServerDestroy(pServer);
	return xrtNetEngineDestroy(pEngine) ? 0 : 3;
}
