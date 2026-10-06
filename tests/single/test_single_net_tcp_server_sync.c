#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头聚合 Server 的阻塞 Accept 复用 Future 和统一等待桥。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetserverconfig ServerConfig;
	xnetengine* pEngine;
	xnetserver* pServer;
	xnetstream* pClient;
	xnetstream* pAccepted;
	xnetaddr Address;
	double Deadline;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2;
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
	pServer = xrtNetServerStart(
		pEngine,
		&ServerConfig,
		NULL,
		NULL,
		NULL
	);
	if ( (pServer == NULL) ||
		!xrtNetServerLocal(pServer, 0, &Address) ) {
		return 2;
	}
	pClient = xrtNetStreamConnect(
		pEngine,
		&Address,
		1,
		NULL,
		NULL,
		NULL
	);
	pAccepted = __xrtNetServerAcceptWait(
		pServer,
		__xrtWaitAfter(3000),
		NULL
	);
	if ( (pClient == NULL) || (pAccepted == NULL) ) {
		return 3;
	}
	(void)xrtNetStreamAbort(pClient);
	(void)xrtNetStreamAbort(pAccepted);
	(void)xrtNetServerClose(pServer);
	Deadline = __xrtWaitAfter(3000);
	while ( (xrtNetStreamState(pClient) != XNET_STREAM_CLOSED) ||
		(xrtNetStreamState(pAccepted) != XNET_STREAM_CLOSED) ||
		(xrtNetServerState(pServer) != XNET_SERVER_CLOSED) ) {
		if ( __xrtWaitExpired(Deadline) ) {
			return 4;
		}
		xrtThreadYield();
	}
	xrtNetStreamDestroy(pClient);
	xrtNetStreamDestroy(pAccepted);
	xrtNetServerDestroy(pServer);
	return xrtNetEngineDestroy(pEngine) ? 0 : 5;
}
