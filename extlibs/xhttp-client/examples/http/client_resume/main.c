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



/*
	为 HTTPS Client 配置有界 TLS 1.3 ticket 缓存。
	实际请求会自动取用和补充 ticket，调用方不接触 TLS 会话对象。
*/
int main(void)
{
	xnetengineconfig EngineConfig;
	xhttpclientconfig ClientConfig;
	xhttpresumestats Stats;
	xnetengine* pEngine;
	xhttpclient* pClient;

	xrtNetEngineConfigInit(&EngineConfig);
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		exampleNetEngineDestroy(pEngine);
		return 1;
	}

	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Resume.MaxEntries = 32;
	ClientConfig.Resume.MaxEntriesPerOrigin = 2;
	pClient = xrtHttpClientCreate(pEngine, &ClientConfig);
	if ( pClient == NULL ) {
		exampleNetEngineDestroy(pEngine);
		return 1;
	}

	if ( !xrtHttpClientResumeStats(pClient, &Stats) ) {
		xrtHttpClientDestroy(pClient);
		exampleNetEngineDestroy(pEngine);
		return 1;
	}
	printf(
		"tickets=%zu hits=%llu misses=%llu\n",
		Stats.Entries,
		(unsigned long long)Stats.Hits,
		(unsigned long long)Stats.Misses
	);

	(void)xrtHttpClientResumeClear(pClient);
	xrtHttpClientDestroy(pClient);
	if ( !exampleNetEngineDestroy(pEngine) ) {
		return 1;
	}
	return 0;
}


