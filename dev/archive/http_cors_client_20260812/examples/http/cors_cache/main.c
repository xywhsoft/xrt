#include <stdio.h>

#include <xrt.h>



/* 演示一次预检校验、缓存更新和后续命中。 */
int main(void)
{
	static const xhttpfield Request[] = {
		{ XRT_STR_INIT("Content-Type"), XRT_STR_INIT("application/json") }
	};
	static const xhttpfield Response[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Origin"), XRT_STR_INIT("https://app.example") },
		{ XRT_STR_INIT("Access-Control-Allow-Methods"), XRT_STR_INIT("PATCH") },
		{ XRT_STR_INIT("Access-Control-Allow-Headers"), XRT_STR_INIT("content-type") },
		{ XRT_STR_INIT("Access-Control-Max-Age"), XRT_STR_INIT("600") }
	};
	xhttporigin Origin;
	xhttpcorscachekey Key;
	xhttpcorsclientresult Result;
	xhttpcorspreflightplan Plan;
	xhttpcorscache* pCache;

	if ( !xrtHttpOriginParse(
		XRT_STR_LITERAL("https://app.example"),
		&Origin
	) || !xrtHttpCorsCacheKeyInit(
		&Key,
		&Origin,
		XRT_STR_LITERAL("https://api.example/items")
	) ) {
		return 1;
	}
	pCache = xrtHttpCorsCacheCreate(NULL);
	if ( pCache == NULL ) {
		return 2;
	}
	if ( !xrtHttpCorsPreflightCheck(
		204u,
		&Origin,
		XRT_STR_LITERAL("PATCH"),
		Request,
		1u,
		false,
		Response,
		4u,
		&Result
	) || ((Result.Flags & XHTTP_CORS_CLIENT_ALLOW) == 0) ||
		!xrtHttpCorsCacheUpdate(
			pCache,
			&Key,
			XRT_STR_LITERAL("PATCH"),
			false,
			false,
			Response,
			4u,
			&Result,
			NULL
		) || !xrtHttpCorsCachePlan(
			pCache,
			&Key,
			XRT_STR_LITERAL("PATCH"),
			Request,
			1u,
			false,
			false,
			&Plan
		) ) {
		xrtHttpCorsCacheRelease(pCache);
		return 3;
	}
	printf("preflight cached: %s\n",
		(Plan.Flags & XHTTP_CORS_PREFLIGHT_CACHED) != 0 ?
			"yes" : "no");
	xrtHttpCorsCacheRelease(pCache);
	return 0;
}
