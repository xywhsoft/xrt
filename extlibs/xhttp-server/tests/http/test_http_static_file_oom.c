#include "../test.h"



/* 单次故障注入分配器让每个构造分配点都能独立失败。 */
typedef struct test_http_static_file_oom {
	size_t Calls;
	size_t FailAt;
} test_http_static_file_oom;



/* 在指定调用拒绝一次分配，其余请求交给系统分配器。 */
static ptr testHttpStaticFileOomAlloc(
	ptr pData,
	size_t iSize
)
{
	test_http_static_file_oom* pState =
		(test_http_static_file_oom*)pData;

	if ( pState->Calls++ == pState->FailAt ) {
		return NULL;
	}
	return malloc(iSize);
}



/* 在指定调用拒绝一次重分配，其余请求交给系统分配器。 */
static ptr testHttpStaticFileOomRealloc(
	ptr pData,
	ptr pMemory,
	size_t iSize
)
{
	test_http_static_file_oom* pState =
		(test_http_static_file_oom*)pData;

	if ( pState->Calls++ == pState->FailAt ) {
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 释放故障注入分配器创建的内存。 */
static void testHttpStaticFileOomFree(
	ptr pData,
	ptr pMemory
)
{
	(void)pData;
	free(pMemory);
}



/* 创建静态文件 OOM 测试内容。 */
static void testHttpStaticFileOomWrite(
	xroot Root
)
{
	xfileoptions Options;
	xfile File;

	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_WRITE |
		XFILE_CREATE |
		XFILE_TRUNCATE;
	File = xrtRootFileOpen(
		Root,
		"asset.txt",
		&Options
	);
	testRequire(
		(File != NULL) &&
		xrtWriteFull(
			File,
			"oom",
			3,
			NULL
		) &&
		xrtClose(File),
		"HTTP static file OOM fixture write failed"
	);
}



/* 遍历同步构造分配点并确认失败后仍可重新打开同一文件。 */
int main(void)
{
	test_http_static_file_oom State;
	xallocator Allocator;
	xtaskpoolconfig PoolConfig = { 1, 16, 0 };
	char sDirectory[96];
	xtaskpool* pPool;
	xhttpstaticfile* pFile;
	xroot Parent;
	xroot Root;
	size_t i;

	memset(&State, 0, sizeof(State));
	State.FailAt = SIZE_MAX;
	Allocator.Context = &State;
	Allocator.Alloc = testHttpStaticFileOomAlloc;
	Allocator.Realloc = testHttpStaticFileOomRealloc;
	Allocator.Free = testHttpStaticFileOomFree;
	testRequire(
		xrtSetAllocator(&Allocator),
		"HTTP static file OOM allocator install failed"
	);
	testRequire(
		(snprintf(
			sDirectory,
			sizeof(sDirectory),
			".xrt-http-static-file-oom-%lld",
			(long long)xrtNow()
		) > 0),
		"HTTP static file OOM directory name failed"
	);

	Parent = xrtRootOpen(".");
	testRequire(
		Parent != NULL,
		"HTTP static file OOM parent root failed"
	);
	if ( !xrtRootRemove(Parent, sDirectory) ) {
		xrtClearError();
	}
	testRequire(
		xrtRootDirCreate(
			Parent,
			sDirectory,
			0700u
		),
		"HTTP static file OOM directory create failed"
	);
	Root = xrtRootOpenIn(
		Parent,
		sDirectory
	);
	testRequire(
		Root != NULL,
		"HTTP static file OOM root open failed"
	);
	testHttpStaticFileOomWrite(Root);
	pPool = xrtTaskPoolCreate(&PoolConfig);
	testRequire(
		pPool != NULL,
		"HTTP static file OOM task pool failed"
	);

	for ( i = 0; i < 24u; i++ ) {
		State.Calls = 0;
		State.FailAt = i;
		pFile = xrtHttpStaticFileOpen(
			pPool,
			Root,
			"asset.txt"
		);
		State.FailAt = SIZE_MAX;
		if ( pFile != NULL ) {
			xrtHttpStaticFileDestroy(pFile);
		} else {
			testRequire(
				(xrtGetError() != NULL) &&
				(xrtErrorKind(xrtGetError()) ==
				 XERR_MEMORY),
				"HTTP static file OOM error mismatch"
			);
			xrtClearError();
		}
	}

	testRequire(
		xrtTaskPoolDestroy(pPool),
		"HTTP static file OOM traversal pool cleanup failed"
	);
	pPool = xrtTaskPoolCreate(&PoolConfig);
	testRequire(
		pPool != NULL,
		"HTTP static file OOM retry pool failed"
	);
	State.Calls = 0;
	State.FailAt = SIZE_MAX;
	pFile = xrtHttpStaticFileOpen(
		pPool,
		Root,
		"asset.txt"
	);
	testRequire(
		(pFile != NULL) &&
		(xrtHttpStaticFileSize(pFile) == 3u),
		"HTTP static file did not recover after OOM"
	);
	xrtHttpStaticFileDestroy(pFile);
	testRequire(
		xrtTaskPoolDestroy(pPool),
		"HTTP static file OOM task pool cleanup failed"
	);
	testRequire(
		xrtRootRemove(Root, "asset.txt") &&
		xrtRootClose(Root) &&
		xrtRootRemove(Parent, sDirectory) &&
		xrtRootClose(Parent),
		"HTTP static file OOM cleanup failed"
	);
	printf("[PASS] http_static_file_oom\n");
	return 0;
}
