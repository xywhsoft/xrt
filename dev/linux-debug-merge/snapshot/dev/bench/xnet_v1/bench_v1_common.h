#ifndef XNET_V1_BENCH_COMMON_H
#define XNET_V1_BENCH_COMMON_H

#include "../xnet2/bench_common.h"
#include "../../../xrt.h"

static void xbenchV1ConfigInit(xnetconfig* pCfg)
{
	if ( !pCfg ) return;
	xrtNetConfigInit(pCfg);
	pCfg->bNoDelay = true;
	pCfg->iConnectTimeoutMs = 5000;
	pCfg->iPollTimeoutMs = 10;
}

static void xbenchV1TlsInit(xtlsconfig* pCfg, const char* sCertFile, const char* sKeyFile, const char* sHostName)
{
	if ( !pCfg ) return;
	memset(pCfg, 0, sizeof(xtlsconfig));
	pCfg->sCertFile = sCertFile;
	pCfg->sKeyFile = sKeyFile;
	pCfg->sHostName = sHostName;
	pCfg->bVerifyPeer = false;
}

static void xbenchV1AddrInit(xnetaddr* pAddr, const char* sIP, uint16_t iPort)
{
	if ( !pAddr ) return;
	memset(pAddr, 0, sizeof(xnetaddr));
	pAddr->iPort = iPort;
	if ( sIP && sIP[0] ) {
		#if defined(_WIN32) || defined(_WIN64)
			pAddr->iAddr = inet_addr(sIP);
		#else
			pAddr->iAddr = (uint32)inet_addr(sIP);
		#endif
		strncpy(pAddr->sAddr, sIP, sizeof(pAddr->sAddr) - 1u);
		pAddr->sAddr[sizeof(pAddr->sAddr) - 1u] = '\0';
	}
}

static bool xbenchV1WaitTcpConnected(xtcpclient* pClient, uint32_t iTimeoutMs)
{
	uint32_t iLoops = (iTimeoutMs / 10u) + 1u;
	if ( !pClient ) return false;
	for ( uint32_t i = 0; i < iLoops; ++i ) {
		if ( xrtTcpClientIsConnected(pClient) ) return true;
		xbenchSleepMs(10u);
	}
	return xrtTcpClientIsConnected(pClient);
}

#endif
