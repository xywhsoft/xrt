#include <stdio.h>
#include <xrt.h>



/* 配置高层 HTTP Client 使用共享缓存，并展示调用级分区与只读模式。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xhttpclientconfig ClientConfig;
	xhttpcalloptions CallOptions;
	xhttpcache* pCache;
	xnetengine* pEngine;
	xhttpclient* pClient;

	pCache = xrtHttpCacheCreate(NULL);
	if ( pCache == NULL ) {
		return 1;
	}
	xrtNetEngineConfigInit(&EngineConfig);
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		xrtNetEngineDestroy(pEngine);
		xrtHttpCacheRelease(pCache);
		return 2;
	}

	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Cache.Store = pCache;
	ClientConfig.Cache.MaxBody = 16u * 1024u * 1024u;
	ClientConfig.Cache.Strict = false;
	pClient = xrtHttpClientCreate(pEngine, &ClientConfig);
	xrtHttpCacheRelease(pCache);
	if ( pClient == NULL ) {
		xrtNetEngineDestroy(pEngine);
		return 3;
	}

	xrtHttpCallOptionsInit(&CallOptions);
	CallOptions.Cache.Mode = XHTTP_CLIENT_CACHE_ONLY;
	CallOptions.Cache.PartitionKey =
		XRT_STR_LITERAL("tenant:example");
	printf(
		"cache=%s mode=%u partition=%.*s\n",
		xrtHttpClientCache(pClient) != NULL ?
			"enabled" : "disabled",
		(unsigned int)CallOptions.Cache.Mode,
		(int)CallOptions.Cache.PartitionKey.Size,
		CallOptions.Cache.PartitionKey.Data
	);

	xrtHttpClientDestroy(pClient);
	if ( !xrtNetEngineDestroy(pEngine) ) {
		return 4;
	}
	return 0;
}
