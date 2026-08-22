#define XHTTP_MODULE_HTTP_CLIENT_CACHE
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头文件发布包含高层客户端缓存配置与存储句柄。 */
int main(void)
{
	xhttpclientcacheconfig Config;
	xhttpclientcacheoptions Options;
	xhttpcache* pCache;

	xrtHttpClientCacheConfigInit(&Config);
	xrtHttpClientCacheOptionsInit(&Options);
	if ( (Config.Store != NULL) ||
		(Config.MaxBody !=
		 XHTTP_CLIENT_CACHE_BODY_DEFAULT) ||
		(Config.HeuristicMax !=
		 XHTTP_CLIENT_CACHE_HEURISTIC_MAX_DEFAULT) ||
		(Config.HeuristicPercent !=
		 XHTTP_CLIENT_CACHE_HEURISTIC_PERCENT_DEFAULT) ||
		Config.Shared ||
		!Config.Heuristic ||
		Config.Strict ||
		(Options.Mode !=
		 XHTTP_CLIENT_CACHE_DEFAULT) ||
		(Options.PartitionKey.Data != NULL) ||
		(Options.PartitionKey.Size != 0) ) {
		return 1;
	}

	pCache = xrtHttpCacheCreate(NULL);
	if ( pCache == NULL ) {
		return 2;
	}
	Config.Store = pCache;
	xrtHttpCacheRelease(pCache);
	return 0;
}
