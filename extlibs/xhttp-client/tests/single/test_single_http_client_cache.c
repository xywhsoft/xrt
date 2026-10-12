#define XHTTP_POLICY_IMPLEMENTATION
#define XHTTP_CLIENT_IMPLEMENTATION
#define XHTTP_CLIENT_MODULE_HTTP_CLIENT_CACHE
#define XHTTP_IMPLEMENTATION
#include "../../include/xhttp-client/features.h"
#include <xhttp-policy/features.h>
#include <xhttp/features.h>
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xhttp.h"
#include "../../../../single/extlibs/xhttp-policy.h"
#include "../../../../single/extlibs/xhttp-client.h"




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
