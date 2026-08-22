#include "../test.h"



typedef struct testdnsallocator {
	bool Fail;
	size_t Attempts;
} testdnsallocator;



/* 故障阶段拒绝 DNS 结果的全部 XRT 分配。 */
static ptr testDNSAlloc(ptr pData, size_t iSize)
{
	testdnsallocator* pContext = (testdnsallocator*)pData;

	pContext->Attempts++;
	return pContext->Fail ? NULL : malloc(iSize);
}



/* 故障阶段拒绝 DNS 地址列表扩容。 */
static ptr testDNSRealloc(ptr pData, ptr pMemory, size_t iSize)
{
	testdnsallocator* pContext = (testdnsallocator*)pData;

	pContext->Attempts++;
	return pContext->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放正常阶段已经取得的底层内存。 */
static void testDNSFree(ptr pData, ptr pMemory)
{
	(void)pData;
	free(pMemory);
}



/* 验证数字快路 OOM 不返回半构造列表，恢复后仍可正常使用。 */
int main(void)
{
	testdnsallocator Context;
	xallocator Allocator;
	xnetaddrlist* pList;

	memset(&Context, 0, sizeof(Context));
	Allocator.Context = &Context;
	Allocator.Alloc = testDNSAlloc;
	Allocator.Realloc = testDNSRealloc;
	Allocator.Free = testDNSFree;
	testRequire(xrtSetAllocator(&Allocator),
		"DNS OOM allocator install failed");

	Context.Fail = true;
	testRequire(xrtNetResolve(
		"127.0.0.1",
		80,
		XNET_FAMILY_IPV4
	) == NULL, "DNS result allocation survived OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"DNS result OOM error mismatch");
	xrtClearError();

	Context.Fail = false;
	pList = xrtNetResolve("127.0.0.1", 80, XNET_FAMILY_IPV4);
	testRequire((pList != NULL) && (xrtNetAddrListCount(pList) == 1),
		"DNS did not recover after OOM");
	xrtNetAddrListDestroy(pList);
	testRequire(Context.Attempts != 0,
		"DNS OOM allocator observed no attempts");
	printf("[PASS] network DNS OOM\n");
	return 0;
}
