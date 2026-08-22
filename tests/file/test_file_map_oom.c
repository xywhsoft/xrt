#include "../test.h"



typedef struct test_file_map_allocator {
	bool Armed;
	bool Failed;
} test_file_map_allocator;



/* 武装后只拒绝映射对象的第一次底层内存请求。 */
static ptr testFileMapAlloc(ptr pContext, size_t iSize)
{
	test_file_map_allocator* pState =
		(test_file_map_allocator*)pContext;

	if ( pState->Armed && !pState->Failed ) {
		pState->Failed = true;
		return NULL;
	}
	return malloc(iSize);
}



/* 重分配与普通分配共享同一故障点。 */
static ptr testFileMapRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_file_map_allocator* pState =
		(test_file_map_allocator*)pContext;

	if ( pState->Armed && !pState->Failed ) {
		pState->Failed = true;
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 释放测试分配器成功取得的底层内存。 */
static void testFileMapFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 映射对象分配失败时不得创建半初始化映射。 */
int main(void)
{
	static const char sPath[] = "xrt-file-map-oom.tmp";
	test_file_map_allocator State;
	xallocator Allocator;
	xfile File;

	State.Armed = false;
	State.Failed = false;
	Allocator.Context = &State;
	Allocator.Alloc = testFileMapAlloc;
	Allocator.Realloc = testFileMapRealloc;
	Allocator.Free = testFileMapFree;
	testRequire(xrtSetAllocator(&Allocator),
		"file map OOM allocator install failed");
	File = xrtOpen(sPath, XFILE_READ | XFILE_WRITE |
		XFILE_CREATE | XFILE_TRUNCATE);
	testRequire((File != NULL) &&
		xrtWriteFull(File, "map", 3u, NULL),
		"file map OOM fixture creation failed");

	State.Armed = true;
	testRequire(xrtFileMap(File, 0u, 0u,
		XFILE_MAP_READ) == NULL,
		"file mapping survived object OOM");
	testRequire(State.Failed && (xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"file mapping OOM reported the wrong error");
	xrtClearError();
	testRequire(xrtClose(File), "file map OOM fixture close failed");
	testRequire(remove(sPath) == 0,
		"file map OOM fixture delete failed");
	return 0;
}
