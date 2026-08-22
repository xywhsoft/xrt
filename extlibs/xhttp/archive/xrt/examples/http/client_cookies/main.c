#include <stdio.h>
#include <xrt.h>



/* 配置一个由高层 HTTP Client 持有并自动使用的共享 CookieJar。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xhttpclientconfig ClientConfig;
	xhttpcalloptions CallOptions;
	xnetengine* pEngine;
	xcookiejar* pJar;
	xhttpclient* pClient;
	xhttprequest* pRequest;

	xrtNetEngineConfigInit(&EngineConfig);
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		xrtNetEngineDestroy(pEngine);
		return 1;
	}
	pJar = xrtCookieJarCreate(NULL);
	if ( (pJar == NULL) ||
		(xrtCookieJarStoreUrl(
			pJar,
			XRT_STR_LITERAL("https://api.example.test/"),
			XRT_STR_LITERAL(
				"session=ready; Path=/; Secure; HttpOnly"
			),
			NULL
		) != XCOOKIE_STORE_STORED) ) {
		xrtCookieJarRelease(pJar);
		xrtNetEngineDestroy(pEngine);
		return 1;
	}

	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Cookies = pJar;
	pClient = xrtHttpClientCreate(pEngine, &ClientConfig);
	xrtCookieJarRelease(pJar);
	if ( pClient == NULL ) {
		xrtNetEngineDestroy(pEngine);
		return 1;
	}
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://api.example.test/items")
	);
	xrtHttpCallOptionsInit(&CallOptions);
	CallOptions.Cookies.Flags |= XHTTP_COOKIE_TOP_LEVEL;
	if ( pRequest == NULL ) {
		xrtHttpClientDestroy(pClient);
		xrtNetEngineDestroy(pEngine);
		return 1;
	}

	printf(
		"cookies=%zu same_site=%u top_level=%u\n",
		xrtCookieJarCount(xrtHttpClientCookieJar(pClient)),
		(unsigned int)(
			(CallOptions.Cookies.Flags &
			 XHTTP_COOKIE_SAME_SITE) != 0
		),
		(unsigned int)(
			(CallOptions.Cookies.Flags &
			 XHTTP_COOKIE_TOP_LEVEL) != 0
		)
	);
	xrtHttpRequestDestroy(pRequest);
	xrtHttpClientDestroy(pClient);
	if ( !xrtNetEngineDestroy(pEngine) ) {
		return 1;
	}
	return 0;
}
