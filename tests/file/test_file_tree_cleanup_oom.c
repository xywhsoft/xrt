#if !defined(_WIN32) && !defined(_WIN64) && !defined(_POSIX_C_SOURCE)
	#define _POSIX_C_SOURCE 200809L
#endif

#include "../test.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#else
	#include <dirent.h>
	#include <errno.h>
	#include <sys/stat.h>
#endif



#define TEST_TREE_OOM_DEPTH		80u
#define TEST_TREE_OOM_PATH_SIZE	512u



typedef struct test_tree_oom_allocator {
	cstr Root;
	bool Armed;
	bool Failed;
} test_tree_oom_allocator;



/* 使用系统目录接口判断测试根目录内是否仍有目录树暂存对象。 */
static bool testTreeOomHasStage(cstr sRoot)
{
	#if defined(_WIN32) || defined(_WIN64)
		char sPattern[TEST_TREE_OOM_PATH_SIZE];
		WIN32_FIND_DATAA Data;
		HANDLE hFind;

		(void)snprintf(sPattern, sizeof(sPattern),
			"%s/.xrt-tree-*.tmp", sRoot);
		hFind = FindFirstFileA(sPattern, &Data);
		if ( hFind == INVALID_HANDLE_VALUE ) {
			return false;
		}
		(void)FindClose(hFind);
		return true;
	#else
		DIR* pDirectory = opendir(sRoot);
		struct dirent* pEntry;
		bool bFound = false;

		if ( pDirectory == NULL ) {
			return false;
		}
		while ( (pEntry = readdir(pDirectory)) != NULL ) {
			if ( strncmp(pEntry->d_name, ".xrt-tree-", 10u) == 0 ) {
				bFound = true;
				break;
			}
		}
		(void)closedir(pDirectory);
		return bFound;
	#endif
}



/* 暂存树建立后只拒绝第一次底层内存请求。 */
static ptr testTreeOomAlloc(ptr pContext, size_t iSize)
{
	test_tree_oom_allocator* pState =
		(test_tree_oom_allocator*)pContext;

	if ( pState->Armed && !pState->Failed &&
		 testTreeOomHasStage(pState->Root) ) {
		pState->Failed = true;
		return NULL;
	}
	return malloc(iSize);
}



/* 重分配与普通分配共享同一个确定性故障点。 */
static ptr testTreeOomRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	test_tree_oom_allocator* pState =
		(test_tree_oom_allocator*)pContext;

	if ( pState->Armed && !pState->Failed &&
		 testTreeOomHasStage(pState->Root) ) {
		pState->Failed = true;
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 释放测试分配器成功取得的底层内存。 */
static void testTreeOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 使用系统接口创建目录，避免在安装测试分配器前锁定 XRT 分配器。 */
static bool testTreeOomMkdir(cstr sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		return CreateDirectoryA(sPath, NULL) != 0;
	#else
		return mkdir(sPath, 0700) == 0;
	#endif
}



/* 使用系统接口判断目标是否已经可见。 */
static bool testTreeOomExists(cstr sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		return GetFileAttributesA(sPath) != INVALID_FILE_ATTRIBUTES;
	#else
		struct stat Info;

		return lstat(sPath, &Info) == 0;
	#endif
}



/* 构造足够深的源树，确保复制开始后还会请求新的堆尺寸类。 */
static void testTreeOomFixture(cstr sRoot, cstr sSource)
{
	char sPath[TEST_TREE_OOM_PATH_SIZE];
	FILE* pFile;
	size_t iSize;
	uint32 i;

	testRequire(testTreeOomMkdir(sRoot), "tree OOM root creation failed");
	testRequire(testTreeOomMkdir(sSource),
		"tree OOM source creation failed");
	iSize = strlen(sSource);
	memcpy(sPath, sSource, iSize + 1u);
	for ( i = 0u; i < TEST_TREE_OOM_DEPTH; i++ ) {
		testRequire((iSize + 2u) < sizeof(sPath),
			"tree OOM fixture path overflowed");
		sPath[iSize++] = '/';
		sPath[iSize++] = 'd';
		sPath[iSize] = '\0';
		testRequire(testTreeOomMkdir(sPath),
			"tree OOM nested directory creation failed");
	}
	testRequire((iSize + 9u) < sizeof(sPath),
		"tree OOM fixture file path overflowed");
	memcpy(sPath + iSize, "/item.txt", 10u);
	pFile = fopen(sPath, "wb");
	testRequire((pFile != NULL) &&
		(fwrite("data", 1u, 4u, pFile) == 4u) &&
		(fclose(pFile) == 0), "tree OOM fixture file creation failed");
}



/* 复制中途 OOM 必须清理私有暂存树并保留原始错误。 */
int main(void)
{
	static const char sRoot[] = "xrt-file-tree-cleanup-oom";
	static const char sSource[] = "xrt-file-tree-cleanup-oom/source";
	static const char sTarget[] = "xrt-file-tree-cleanup-oom/target";
	test_tree_oom_allocator State;
	xallocator Allocator;

	testTreeOomFixture(sRoot, sSource);
	State.Root = sRoot;
	State.Armed = true;
	State.Failed = false;
	Allocator.Context = &State;
	Allocator.Alloc = testTreeOomAlloc;
	Allocator.Realloc = testTreeOomRealloc;
	Allocator.Free = testTreeOomFree;
	testRequire(xrtSetAllocator(&Allocator),
		"tree OOM allocator install failed");

	testRequire(!xrtFileTreeCopy(sSource, sTarget, NULL, NULL),
		"tree copy survived staged OOM");
	testRequire(State.Failed,
		"tree copy did not reach the staged OOM point");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"staged tree OOM did not preserve the memory error");
	testRequire(!testTreeOomExists(sTarget),
		"staged tree OOM exposed a final target");
	testRequire(!testTreeOomHasStage(sRoot),
		"staged tree OOM left a private staging directory");

	xrtClearError();
	testRequire(xrtDirRemoveAll(sRoot),
		"tree OOM fixture cleanup failed");
	return 0;
}
