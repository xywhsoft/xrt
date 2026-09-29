#include "../test.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#endif



/* 只统计本进程描述符，补足内存检查无法发现的 OS 句柄泄漏。 */
static uint64 testHandleCount(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		DWORD iCount = 0;
		testRequire(GetProcessHandleCount(GetCurrentProcess(), &iCount),
			"process handle count failed");
		return iCount;
	#elif defined(__linux__)
		xdir Dir = xrtDirOpen("/proc/self/fd", 0u);
		xdirentry Entry;
		xdirnext Next;
		uint64 iCount = 0;
		testRequire(Dir != NULL, "descriptor directory open failed");
		while ( (Next = xrtDirNext(Dir, &Entry)) == XDIR_NEXT_ITEM ) iCount++;
		testRequire(Next == XDIR_NEXT_END && xrtDirClose(Dir),
			"descriptor directory read failed");
		return iCount;
	#else
		return 0;
	#endif
}



/* 每个逻辑分配点都失败一次；失败不得创建文件或提前截断已有内容。 */
static void testRootOpenFailures(xroot Root, cstr sRelative, cstr sTarget,
	uint32 iFlags, bool bExisting)
{
	xfileoptions Options;
	size_t i, iFailures = 0, iSuccesses = 0;
	uint64 iHandles = testHandleCount();

	xrtFileOptionsInit(&Options);
	Options.Flags = iFlags;
	for ( i = 0; i < 128 && iSuccesses < 2; i++ ) {
		xmemdebugsnapshot Before, After;
		xfile File;
		bool bHit, bPreserved;
		size_t iSize = 0;
		bytes Data;

		if ( bExisting ) {
			testRequire(xrtFileWriteAll(sTarget, XRT_BYTES_LITERAL("keep")),
				"root allocation fixture write failed");
		}
		xrtMemDebugSnapshot(&Before);
		testRequire(xrtMemDebugFailAfter(i), "root fault injection failed");
		File = xrtRootFileOpen(Root, sRelative, &Options);
		bHit = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( bHit ) {
			testRequire(File == NULL && xrtGetError() &&
				xrtErrorKind(xrtGetError()) == XERR_MEMORY,
				"root allocation failure lost its memory error");
			iFailures++;
		} else {
			testRequire(File != NULL && xrtClose(File),
				"root open failed without allocation failure");
			iSuccesses++;
		}
		xrtClearError();
		xrtMemDebugSnapshot(&After);
		testRequire(Before.LiveCount == After.LiveCount && Before.LiveBytes == After.LiveBytes &&
			Before.InvalidFreeCount == After.InvalidFreeCount &&
			Before.DoubleFreeCount == After.DoubleFreeCount &&
			Before.UnderflowCount == After.UnderflowCount && Before.OverflowCount == After.OverflowCount,
			"root open allocation ownership is incorrect");
		if ( bHit && bExisting ) {
			Data = xrtFileReadAll(sTarget, &iSize);
			bPreserved = Data != NULL && iSize == 4 && memcmp(Data, "keep", 4) == 0;
			xrtFree(Data);
			testRequire(bPreserved, "root truncate allocation failure changed existing content");
		} else if ( bHit ) {
			testRequire(!xrtPathExists(sTarget), "root allocation failure created an orphan file");
		} else {
			Data = xrtFileReadAll(sTarget, &iSize);
			testRequire(Data != NULL && iSize == 0, "root successful open did not create/truncate");
			xrtFree(Data);
		}
		if ( bExisting || !bHit ) testRequire(xrtFileDelete(sTarget), "root fixture cleanup failed");
	}
	testRequire(iFailures && iSuccesses == 2 && testHandleCount() == iHandles,
		"root fault sweep incomplete or leaked native handles");
	printf("root allocation: %s flags=%u %zu failure points passed\n",
		sRelative, (unsigned int)iFlags, iFailures);
}



int main(void)
{
	str sDirectory, sNested, sTarget, sLink;
	xroot Root;
	uint32 iTruncate = XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE;

	testRequire(xrtMemDebugEnable(true), "memory debug enable failed");
	sDirectory = xrtDirTemp(NULL, "xrt-root-allocation-", NULL);
	testRequire(sDirectory != NULL, "root fixture directory failed");
	sNested = xrtPathJoin(sDirectory, "nested");
	sTarget = xrtPathJoin(sNested, "data.bin");
	sLink = xrtPathJoin(sDirectory, "link.bin");
	testRequire(sNested && sTarget && sLink && xrtDirCreate(sNested), "root fixture setup failed");
	Root = xrtRootOpen(sDirectory);
	testRequire(Root != NULL, "root capability creation failed");
	testRootOpenFailures(Root, "nested/data.bin", sTarget, iTruncate, true);
	testRootOpenFailures(Root, "nested/data.bin", sTarget, iTruncate | XFILE_APPEND, true);
	testRootOpenFailures(Root, "nested/data.bin", sTarget, XFILE_WRITE | XFILE_CREATE | XFILE_EXCLUSIVE, false);
	if ( xrtLinkCreate("nested/data.bin", sLink, false) ) {
		testRootOpenFailures(Root, "link.bin", sTarget, iTruncate, true);
		testRequire(xrtFileDelete(sLink), "root link cleanup failed");
	} else {
		#if defined(_WIN32) || defined(_WIN64)
			printf("root allocation: symbolic link skipped (system=%d)\n",
				(int)xrtErrorSystemCode(xrtGetError()));
			xrtClearError();
		#else
			testRequire(false, "root symbolic-link fixture failed");
		#endif
	}
	testRequire(xrtRootClose(Root) && xrtDirRemove(sNested) && xrtDirRemove(sDirectory),
		"root fixture directory cleanup failed");
	xrtFree(sLink);
	xrtFree(sTarget);
	xrtFree(sNested);
	xrtFree(sDirectory);
	testMemoryDebugDrain("root allocation tests leaked a live block");
	return 0;
}
