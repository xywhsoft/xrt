#include "../test.h"



typedef struct testlistallocator {
	size_t Calls;
} testlistallocator;



/* 记录并拒绝原始分配。 */
static ptr testListAlloc(ptr pContext, size_t iSize)
{
	testlistallocator* pAllocator = (testlistallocator*)pContext;

	(void)iSize;
	pAllocator->Calls++;
	return NULL;
}



/* 记录并拒绝原始重分配。 */
static ptr testListRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	testlistallocator* pAllocator = (testlistallocator*)pContext;

	(void)pMemory;
	(void)iSize;
	pAllocator->Calls++;
	return NULL;
}



/* 全失败分配器没有实际内存需要释放。 */
static void testListFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	(void)pMemory;
}



/* 侵入式链表成功路径在底层分配器完全失败时仍不得申请内存。 */
int main(void)
{
	testlistallocator Allocator = { 0 };
	xallocator Hooks;
	xlist List = XRT_LIST_INIT;
	xlistnode Nodes[] = {
		XRT_LIST_NODE_INIT,
		XRT_LIST_NODE_INIT,
		XRT_LIST_NODE_INIT
	};
	xlistiter Iterator;
	size_t iCount = 0;

	Hooks.Context = &Allocator;
	Hooks.Alloc = testListAlloc;
	Hooks.Realloc = testListRealloc;
	Hooks.Free = testListFree;
	testRequire(xrtSetAllocator(&Hooks), "list allocator install failed");
	testRequire(xrtListPushBack(&List, &Nodes[0]), "noalloc first push failed");
	testRequire(xrtListPushBack(&List, &Nodes[1]), "noalloc second push failed");
	testRequire(xrtListPushFront(&List, &Nodes[2]), "noalloc front push failed");
	testRequire(xrtListMoveBack(&List, &Nodes[2]), "noalloc move failed");
	testRequire(xrtListIterBegin(&List, &Iterator), "noalloc iterator begin failed");
	while ( xrtListIterNext(&Iterator) != NULL ) {
		iCount++;
	}
	testRequire(iCount == 3u, "noalloc iterator count mismatch");
	testRequire(xrtListClear(&List), "noalloc clear failed");
	testRequire(Allocator.Calls == 0u, "list success path allocated memory");
	printf("[PASS] list-noalloc\n");
	return 0;
}
