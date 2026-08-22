#include "../test.h"



/* 验证大认证值临时堆缓冲失败时保留旧字段。 */
static void testHttpClientGenericAuthOom(
	xhttprequest* pRequest,
	const char* sLong,
	size_t iLong
)
{
	const xhttpfield* pField;
	bool bTriggered;

	testRequire(
		xrtMemDebugFailAfter(0),
		"HTTP client large authentication OOM setup failed"
	);
	testRequire(!xrtHttpRequestSetAuth(
		pRequest,
		XRT_STR_LITERAL("Bearer"),
		(xstrview){ sLong, iLong }
	) && (xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP client large authentication did not fail with OOM");
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	testRequire(
		bTriggered,
		"HTTP client large authentication did not reach injected OOM"
	);
	xrtClearError();
	pField = xrtHttpRequestHeader(
		pRequest,
		XRT_STR_LITERAL("Authorization")
	);
	testRequire((pField != NULL) &&
		(pField->Value.Size == 9u) &&
		(memcmp(pField->Value.Data, "Old token", 9u) == 0),
		"HTTP client large authentication changed old field");
}



/* 验证长 Basic 凭据的内部敏感堆缓冲失败时保留旧字段。 */
static void testHttpClientBasicAuthOom(xhttprequest* pRequest)
{
	char User[200];
	char Password[100];
	const xhttpfield* pField;
	bool bTriggered;

	memset(User, 'u', sizeof(User));
	memset(Password, 'p', sizeof(Password));
	testRequire(
		xrtMemDebugFailAfter(1),
		"HTTP client Basic authentication OOM setup failed"
	);
	testRequire(!xrtHttpRequestSetBasicAuth(
		pRequest,
		(xstrview){ User, sizeof(User) },
		(xstrview){ Password, sizeof(Password) }
	) && (xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP client Basic authentication did not fail with OOM");
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	testRequire(
		bTriggered,
		"HTTP client Basic authentication did not reach injected OOM"
	);
	xrtClearError();
	pField = xrtHttpRequestHeader(
		pRequest,
		XRT_STR_LITERAL("Authorization")
	);
	testRequire((pField != NULL) &&
		(pField->Value.Size == 9u) &&
		(memcmp(pField->Value.Data, "Old token", 9u) == 0),
		"HTTP client Basic authentication changed old field");
}



int main(void)
{
	char Long[300];
	xhttprequest* pGeneric = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://example.test/generic")
	);
	xhttprequest* pBasic = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://example.test/basic")
	);

	memset(Long, 'a', sizeof(Long));
	testRequire((pGeneric != NULL) && (pBasic != NULL) &&
		xrtHttpRequestSetHeader(
			pGeneric,
			XRT_STR_LITERAL("Authorization"),
			XRT_STR_LITERAL("Old token")
		) && xrtHttpRequestSetHeader(
			pBasic,
			XRT_STR_LITERAL("Authorization"),
			XRT_STR_LITERAL("Old token")
		),
		"HTTP client authentication OOM setup failed");
	testHttpClientGenericAuthOom(pGeneric, Long, sizeof(Long));
	testHttpClientBasicAuthOom(pBasic);
	xrtHttpRequestDestroy(pBasic);
	xrtHttpRequestDestroy(pGeneric);
	testMemoryDebugDrain(
		"HTTP client authentication OOM leaked memory"
	);
	puts("[PASS] HTTP client authentication OOM");
	return 0;
}
