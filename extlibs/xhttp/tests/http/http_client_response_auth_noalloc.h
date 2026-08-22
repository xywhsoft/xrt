#ifndef TEST_HTTP_CLIENT_RESPONSE_AUTH_NOALLOC_H
#define TEST_HTTP_CLIENT_RESPONSE_AUTH_NOALLOC_H

#include "http_client_response_fixture.h"



/* 响应认证测试分配器可在夹具建立后拒绝任何意外分配。 */
typedef struct test_http_response_auth_allocator {
	bool Reject;
	size_t Denied;
} test_http_response_auth_allocator;



/* 正常阶段交给 C 运行库，拒绝阶段只记录分配尝试。 */
static ptr testHttpResponseAuthAlloc(ptr pContext, size_t iSize)
{
	test_http_response_auth_allocator* pAllocator =
		(test_http_response_auth_allocator*)pContext;

	if ( pAllocator->Reject ) {
		pAllocator->Denied++;
		return NULL;
	}
	return malloc(iSize);
}



/* 重分配与分配使用同一个拒绝开关。 */
static ptr testHttpResponseAuthRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_http_response_auth_allocator* pAllocator =
		(test_http_response_auth_allocator*)pContext;

	if ( pAllocator->Reject ) {
		pAllocator->Denied++;
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 释放始终允许，保证夹具能在拒绝阶段完整回收。 */
static void testHttpResponseAuthFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 在 XRT 首次分配前安装可切换的测试分配器。 */
static void testHttpResponseAuthAllocatorInstall(
	test_http_response_auth_allocator* pState
)
{
	xallocator Allocator;

	memset(pState, 0, sizeof(*pState));
	Allocator.Context = pState;
	Allocator.Alloc = testHttpResponseAuthAlloc;
	Allocator.Realloc = testHttpResponseAuthRealloc;
	Allocator.Free = testHttpResponseAuthFree;
	testRequire(
		xrtSetAllocator(&Allocator),
		"HTTP response authentication allocator install failed"
	);
}

#endif
