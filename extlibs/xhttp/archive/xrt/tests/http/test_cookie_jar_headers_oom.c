#include "../test.h"



/* 可控分配器在指定调用后拒绝请求。 */
typedef struct test_cookie_allocator {
	size_t Calls;
	size_t FailAt;
} test_cookie_allocator;



/* 在失败点前转发到 C 分配器。 */
static ptr testCookieAlloc(ptr pContext, size_t iSize)
{
	test_cookie_allocator* pState = (test_cookie_allocator*)pContext;

	if ( pState->Calls++ >= pState->FailAt ) {
		return NULL;
	}
	return malloc(iSize);
}



/* 在失败点前转发重分配。 */
static ptr testCookieRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	test_cookie_allocator* pState = (test_cookie_allocator*)pContext;

	if ( pState->Calls++ >= pState->FailAt ) {
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 释放测试分配器拥有的内存。 */
static void testCookieFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* Apply 分配失败必须保留原 Header 内容和内存错误。 */
int main(void)
{
	test_cookie_allocator State = { 0, SIZE_MAX };
	xallocator Allocator;
	xcookiejar* pJar;
	xhttpheaders* pHeaders;
	xcookierequestcontext Request;
	const xhttpfield* pField;
	char Field[2050];
	size_t i;

	Allocator.Context = &State;
	Allocator.Alloc = testCookieAlloc;
	Allocator.Realloc = testCookieRealloc;
	Allocator.Free = testCookieFree;
	testRequire(xrtSetAllocator(&Allocator),
		"cookie header staged allocator install failed");
	pJar = xrtCookieJarCreate(NULL);
	pHeaders = xrtHttpHeadersCreate(NULL);
	memcpy(Field, "sid=", 4u);
	for ( i = 4u; i < sizeof(Field); i++ ) {
		Field[i] = 'x';
	}
	testRequire((pJar != NULL) && (pHeaders != NULL) &&
		(xrtCookieJarStoreUrl(
			pJar, XRT_STR_LITERAL("https://example.com/"),
			(xstrview){ Field, sizeof(Field) }, NULL
		) == XCOOKIE_STORE_STORED) && xrtHttpHeadersAdd(
			pHeaders, XRT_STR_LITERAL("Cookie"),
			XRT_STR_LITERAL("before=1")
		), "cookie header OOM fixture failed");
	memset(&Request, 0, sizeof(Request));
	Request.Flags = XCOOKIE_REQUEST_HTTP_API |
		XCOOKIE_REQUEST_SAME_SITE;
	Request.URL = XRT_STR_LITERAL("https://example.com/");
	State.FailAt = State.Calls;
	testRequire(!xrtCookieJarApply(pJar, &Request, pHeaders) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"cookie header apply did not publish OOM");
	pField = xrtHttpHeadersGet(pHeaders, XRT_STR_LITERAL("Cookie"));
	testRequire((pField != NULL) && (pField->Value.Size == 8u) &&
		(memcmp(pField->Value.Data, "before=1", 8u) == 0),
		"cookie header OOM modified existing field");
	State.FailAt = SIZE_MAX;
	xrtHttpHeadersDestroy(pHeaders);
	xrtCookieJarRelease(pJar);
	printf("[PASS] cookie_jar_headers_oom\n");
	return 0;
}
