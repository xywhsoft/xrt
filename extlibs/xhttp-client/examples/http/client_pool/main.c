#include <stdio.h>
#include <xhttp-client.h>

/* CLOSED and terminal results can precede the final callback release.
 * Keep the Engine alive while retirement reports BUSY, with a finite deadline. */
static bool exampleNetEngineDestroy(xnetengine* pEngine)
{
    double Deadline = xrtTimer() + 5.0;
    for (;;) {
        xnetretireresult Result = xrtNetEngineTryDestroy(pEngine);
        if (Result == XNET_RETIRE_READY) return true;
        if (Result != XNET_RETIRE_BUSY || !(xrtTimer() < Deadline)) return false;
        xrtThreadYield();
    }
}



/* 建立一个有界连接池，并读取初始并发统计。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xhttpclientconfig ClientConfig;
	xhttpclientstats Stats;
	xnetengine* pEngine;
	xhttpclient* pClient;

	xrtNetEngineConfigInit(&EngineConfig);
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		exampleNetEngineDestroy(pEngine);
		return 1;
	}

	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Pool.MaxConnections = 256;
	ClientConfig.Pool.MaxConnectionsPerOrigin = 32;
	ClientConfig.Pool.MaxWaiting = 1024;
	ClientConfig.Pool.MaxWaitingPerOrigin = 128;
	ClientConfig.Pool.MaxIdle = 64;
	ClientConfig.Pool.MaxIdlePerOrigin = 8;
	ClientConfig.Pool.IdleTimeout = INT64_C(30000);
	pClient = xrtHttpClientCreate(pEngine, &ClientConfig);
	if ( pClient == NULL ) {
		exampleNetEngineDestroy(pEngine);
		return 1;
	}
	if ( !xrtHttpClientStats(pClient, &Stats) ) {
		xrtHttpClientDestroy(pClient);
		exampleNetEngineDestroy(pEngine);
		return 1;
	}

	printf(
		"active=%zu idle=%zu waiting=%zu opened=%llu\n",
		Stats.ActiveConnections,
		Stats.IdleConnections,
		Stats.WaitingCalls,
		(unsigned long long)Stats.ConnectionsOpened
	);
	(void)xrtHttpClientCloseIdle(pClient);
	xrtHttpClientDestroy(pClient);
	if ( !exampleNetEngineDestroy(pEngine) ) {
		return 1;
	}
	return 0;
}


