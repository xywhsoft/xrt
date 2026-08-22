#include <stdio.h>

#include <xrt.h>



/* 展示 select fallback 的 one-shot 可读观察和显式重新观察模型。 */
int main(void)
{
	xnetportconfig Config;
	xnetportevent Event;
	xnetport* pPort = NULL;
	xnetsocket Server = NULL;
	xnetsocket Client = NULL;
	xnetaddr Address;
	char sData[16] = { 0 };
	size_t iSize;
	size_t iCount;
	int iResult = 1;

	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_SELECT;
	pPort = xrtNetPortCreate(&Config);
	Server = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, XNET_SOCKET_NONBLOCK);
	Client = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, XNET_SOCKET_NONBLOCK);
	if ( (pPort == NULL) || (Server == NULL) || (Client == NULL) ||
		 !xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) ||
		 !xrtNetSocketBind(Server, &Address) ||
		 !xrtNetSocketLocal(Server, &Address) ||
		 !xrtNetPortWatch(pPort, Server, 1, XNET_POLL_READ, NULL) ||
		 (xrtNetSocketSendTo(Client, "event", 5,
			&iSize, &Address) != XNET_RESULT_OK) ||
		 (xrtNetPortWait(pPort, &Event, 1,
			xrtDeadlineAfter(1000000), &iCount) != XNET_RESULT_OK) ||
		 (iCount != 1) ||
		 ((Event.Flags & XNET_PORT_EVENT_READ) == 0) ||
		 (xrtNetSocketRecvFrom(Server, sData, sizeof(sData) - 1,
			&iSize, NULL) != XNET_RESULT_OK) ) {
		goto Cleanup;
	}

	sData[iSize] = 0;
	printf("backend=%s data=%s\n", xrtNetPortName(pPort), sData);
	iResult = 0;

Cleanup:
	if ( Client != NULL ) { (void)xrtNetSocketClose(Client); }
	if ( Server != NULL ) { (void)xrtNetSocketClose(Server); }
	if ( pPort != NULL ) { (void)xrtNetPortDestroy(pPort); }
	return iResult;
}
