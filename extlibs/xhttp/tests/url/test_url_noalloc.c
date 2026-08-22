#include "../test.h"



/* 无分配测试分配器记录 URL 热路径是否触碰动态内存。 */
typedef struct test_url_allocator {
	size_t Calls;
} test_url_allocator;



/* 所有分配都失败；基础 URL 解析和写出不应调用这里。 */
static ptr testUrlNoAlloc(ptr pContext, size_t iSize)
{
	test_url_allocator* pState = (test_url_allocator*)pContext;

	(void)iSize;
	pState->Calls++;
	return NULL;
}



/* 所有重分配都失败；基础 URL 解析和写出不应调用这里。 */
static ptr testUrlNoRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	test_url_allocator* pState = (test_url_allocator*)pContext;

	(void)pMemory;
	(void)iSize;
	pState->Calls++;
	return NULL;
}



/* 失败分配器没有可释放的内存。 */
static void testUrlNoFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	(void)pMemory;
}



/* 解析、authority 解析和四种直接写出必须保持零分配。 */
int main(void)
{
	test_url_allocator State = { 0 };
	xallocator Allocator = {
		&State, testUrlNoAlloc, testUrlNoRealloc, testUrlNoFree
	};
	xurl Url;
	xurl Base;
	char Output[256];
	char Path[64] = "/a/b/../c";
	size_t iSize;
	uint16 iPort;

	testRequire(xrtSetAllocator(&Allocator),
		"URL no-allocation allocator install failed");
	testRequire(xrtUrlParse(
		XRT_STR_LITERAL("https://user@[::1]:443/a/b?q=#f"), &Url
	), "URL no-allocation parse failed");
	testRequire(xrtUrlWrite(
		&Url, Output, sizeof(Output), &iSize
	) && xrtUrlAuthorityWrite(
		&Url, Output, sizeof(Output), &iSize
	) && xrtUrlHostWrite(
		&Url, Output, sizeof(Output), &iSize
	) && xrtUrlTargetWrite(
		&Url, Output, sizeof(Output), &iSize
	) && xrtUrlPort(&Url, &iPort) && (iPort == 443u),
		"URL no-allocation direct operation failed");
	testRequire(xrtUrlAuthorityParse(
		XRT_STR_LITERAL("user@[2001:db8::1]:"), &Url
	), "URL no-allocation authority parse failed");
	testRequire(xrtUrlPathNormalize(
		(xstrview){ Path, strlen(Path) },
		Path,
		sizeof(Path),
		&iSize
	) && (iSize == strlen("/a/c")) &&
		(memcmp(Path, "/a/c", iSize) == 0),
		"URL no-allocation path normalize failed");
	testRequire(xrtUrlPathNormalize(
		XRT_STR_LITERAL("/a/b/../../c"),
		Output, sizeof(Output), &iSize
	) && (iSize == strlen("/c")) &&
		(memcmp(Output, "/c", iSize) == 0),
		"URL separate-output normalize allocated memory");
	testRequire(xrtUrlParse(
		XRT_STR_LITERAL("http://a/b/c/d;p?q"), &Base
	), "URL no-allocation resolve base parse failed");
	testRequire(xrtUrlResolve(
		&Base, XRT_STR_LITERAL("../g?y#s"),
		NULL, 0, &iSize
	) && (iSize == strlen("http://a/b/g?y#s")),
		"URL no-allocation resolve size query failed");
	testRequire(xrtUrlResolve(
		&Base, XRT_STR_LITERAL("../g?y#s"),
		Output, sizeof(Output), &iSize
	) && (iSize == strlen("http://a/b/g?y#s")) &&
		(memcmp(Output, "http://a/b/g?y#s", iSize) == 0),
		"URL no-allocation resolve write failed");
	testRequire(State.Calls == 0,
		"URL direct path allocated memory");
	printf("[PASS] url_noalloc\n");
	return 0;
}
