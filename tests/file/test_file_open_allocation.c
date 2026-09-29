#include "../test.h"



/* 在逻辑分配边界注入失败，不受小块池复用或 backing allocator 重试影响。 */
static void testOpenFailures(cstr sDirectory, cstr sExisting, bool bTemporary)
{
	size_t i, iFailures = 0, iSuccesses = 0;
	for ( i = 0; i < 64 && iSuccesses < 2; i++ ) {
		xmemdebugsnapshot Before, After;
		str sPath = NULL;
		xfile File;
		bool bHit, bEmpty;

		if ( !bTemporary ) {
			testRequire(xrtFileWriteAll(sExisting, XRT_BYTES_LITERAL("keep")),
				"fixture write failed");
		}
		xrtMemDebugSnapshot(&Before);
		testRequire(xrtMemDebugFailAfter(i), "fault injection failed");
		File = bTemporary ? xrtFileTemp(sDirectory, "file-", ".tmp", &sPath) :
			xrtOpen(sExisting, XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE);
		bHit = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( bHit ) {
			testRequire(File == NULL && sPath == NULL && xrtGetError() &&
				xrtErrorKind(xrtGetError()) == XERR_MEMORY, "file OOM result is incorrect");
			iFailures++;
		} else {
			testRequire(File && xrtClose(File), "file open failed without OOM");
			if ( bTemporary ) testRequire(xrtFileDelete(sPath), "temporary cleanup failed");
			xrtFree(sPath);
			iSuccesses++;
		}
		xrtClearError();
		xrtMemDebugSnapshot(&After);
		testRequire(Before.LiveCount == After.LiveCount && Before.LiveBytes == After.LiveBytes,
			"file OOM leaked an allocation");
		if ( !bTemporary ) {
			if ( bHit ) {
				size_t iSize = 0;
				bytes Data = xrtFileReadAll(sExisting, &iSize);
				testRequire(Data && iSize == 4 && memcmp(Data, "keep", 4) == 0,
					"truncate OOM modified an existing file");
				xrtFree(Data);
			}
			testRequire(xrtFileDelete(sExisting), "fixture cleanup failed");
		}
		testRequire(xrtDirEmpty(sDirectory, &bEmpty) && bEmpty,
			"file OOM left an orphan file");
	}
	testRequire(iFailures && iSuccesses == 2, "file fault sweep was incomplete");
	printf("file allocation: %s %zu failure points passed\n",
		bTemporary ? "temporary" : "truncate", iFailures);
}



int main(void)
{
	str sDirectory, sExisting;
	testRequire(xrtMemDebugEnable(true), "memory debug enable failed");
	sDirectory = xrtDirTemp(NULL, "xrt-open-allocation-", NULL);
	testRequire(sDirectory != NULL, "fixture directory create failed");
	sExisting = xrtPathJoin(sDirectory, "existing");
	testRequire(sExisting != NULL, "fixture path failed");
	testOpenFailures(sDirectory, sExisting, true);
	testOpenFailures(sDirectory, sExisting, false);
	testRequire(xrtDirRemove(sDirectory), "fixture directory remove failed");
	xrtFree(sExisting);
	xrtFree(sDirectory);
	testMemoryDebugDrain("file allocation tests leaked a live block");
	return 0;
}
