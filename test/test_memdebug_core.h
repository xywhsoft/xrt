#ifndef XRT_TEST_MEMDEBUG_CORE_H
#define XRT_TEST_MEMDEBUG_CORE_H

static void __Test_MemDebugCoreRequire(bool bCond, const char* sMsg)
{
	if ( !bCond ) {
		fprintf(stderr, "[memdebug-core] FAIL: %s\n", sMsg ? sMsg : "(no message)");
		exit(1);
	}
}

static bool __Test_MemDebugCoreFileContains(const char* sPath, const char* sNeedle)
{
	FILE* pFile;
	long iSize;
	char* sData;
	bool bRet;

	if ( sPath == NULL || sNeedle == NULL ) {
		return FALSE;
	}

	pFile = fopen(sPath, "rb");
	if ( pFile == NULL ) {
		return FALSE;
	}
	fseek(pFile, 0, SEEK_END);
	iSize = ftell(pFile);
	fseek(pFile, 0, SEEK_SET);
	if ( iSize < 0 ) {
		fclose(pFile);
		return FALSE;
	}
	sData = (char*)malloc((size_t)iSize + 1);
	if ( sData == NULL ) {
		fclose(pFile);
		return FALSE;
	}
	if ( iSize > 0 ) {
		fread(sData, 1, (size_t)iSize, pFile);
	}
	sData[iSize] = '\0';
	fclose(pFile);
	bRet = strstr(sData, sNeedle) != NULL;
	free(sData);
	return bRet;
}

static void Test_MemDebugCore(void)
{
	const char* sTextPath = "dev/memdebug_report.txt";
	const char* sJsonPath = "dev/memdebug_report.json";
	char* pOverflow;
	char* pDouble;
	char* pLeak;
	void* pRaw;
	xfsmempool objFS;
	void* pForeign;
	xarray objArrayLeak;
	xarray objArrayDouble;
	xarray_struct objArrayEmbedded;

	printf("[memdebug-core] start\n");

	__Test_MemDebugCoreRequire(xrtMemDebugIsEnabled(), "memory debug must be enabled");

	pOverflow = (char*)xrtMalloc(16);
	__Test_MemDebugCoreRequire(pOverflow != NULL, "overflow alloc failed");
	memset(pOverflow, 0x31, 16);
	pOverflow[16] = 0x7A;
	xrtFree(pOverflow);

	pDouble = (char*)xrtMalloc(24);
	__Test_MemDebugCoreRequire(pDouble != NULL, "double-free alloc failed");
	xrtFree(pDouble);
	xrtFree(pDouble);

	pRaw = malloc(32);
	__Test_MemDebugCoreRequire(pRaw != NULL, "raw alloc failed");
	xrtFree(pRaw);

	pLeak = (char*)xrtMalloc(48);
	__Test_MemDebugCoreRequire(pLeak != NULL, "leak alloc failed");

	objArrayLeak = xrtArrayCreate(sizeof(int));
	__Test_MemDebugCoreRequire(objArrayLeak != NULL, "array leak create failed");

	objArrayDouble = xrtArrayCreate(sizeof(int));
	__Test_MemDebugCoreRequire(objArrayDouble != NULL, "array double-destroy create failed");
	xrtArrayDestroy(objArrayDouble);
	xrtArrayDestroy(objArrayDouble);

	xrtArrayInit(&objArrayEmbedded, sizeof(int));
	xrtArrayUnit(&objArrayEmbedded);
	xrtArrayUnit(&objArrayEmbedded);

	objFS = xrtFSMemPoolCreate(32);
	__Test_MemDebugCoreRequire(objFS != NULL, "fsmempool create failed");
	pForeign = xrtFSMemPoolAlloc(objFS);
	__Test_MemDebugCoreRequire(pForeign != NULL, "fsmempool alloc failed");
	xrtFree(pForeign);

	__Test_MemDebugCoreRequire(xrtMemDebugDumpText((str)sTextPath), "text report dump failed");
	__Test_MemDebugCoreRequire(xrtMemDebugDumpJson((str)sJsonPath), "json report dump failed");

	__Test_MemDebugCoreRequire(__Test_MemDebugCoreFileContains(sTextPath, "DOUBLE_FREE"), "text report missing double-free");
	__Test_MemDebugCoreRequire(__Test_MemDebugCoreFileContains(sTextPath, "INVALID_FREE"), "text report missing invalid-free");
	__Test_MemDebugCoreRequire(__Test_MemDebugCoreFileContains(sTextPath, "WRONG_ALLOCATOR_FREE"), "text report missing wrong-allocator-free");
	__Test_MemDebugCoreRequire(__Test_MemDebugCoreFileContains(sTextPath, "BUFFER_OVERFLOW_SUSPECT"), "text report missing overflow");
	__Test_MemDebugCoreRequire(__Test_MemDebugCoreFileContains(sTextPath, "LEAK ptr="), "text report missing heap leak");
	__Test_MemDebugCoreRequire(__Test_MemDebugCoreFileContains(sTextPath, "POOL_LEAK"), "text report missing pool leak");
	__Test_MemDebugCoreRequire(__Test_MemDebugCoreFileContains(sTextPath, "OBJECT_LEAK"), "text report missing object leak");
	__Test_MemDebugCoreRequire(__Test_MemDebugCoreFileContains(sTextPath, "OBJECT_DOUBLE_DESTROY"), "text report missing object double-destroy");
	__Test_MemDebugCoreRequire(__Test_MemDebugCoreFileContains(sTextPath, "object=array"), "text report missing object type");

	__Test_MemDebugCoreRequire(__Test_MemDebugCoreFileContains(sJsonPath, "\"events\""), "json report missing events");
	__Test_MemDebugCoreRequire(__Test_MemDebugCoreFileContains(sJsonPath, "\"live_allocations\""), "json report missing live allocations");
	__Test_MemDebugCoreRequire(__Test_MemDebugCoreFileContains(sJsonPath, "\"foreign_live_allocations\""), "json report missing foreign live allocations");
	__Test_MemDebugCoreRequire(__Test_MemDebugCoreFileContains(sJsonPath, "\"live_objects\""), "json report missing live objects");
	__Test_MemDebugCoreRequire(__Test_MemDebugCoreFileContains(sJsonPath, "\"object\":\"array\""), "json report missing object type");

	xrtFree(pLeak);
	xrtArrayDestroy(objArrayLeak);
	xrtFSMemPoolFree(objFS, pForeign);
	xrtFSMemPoolDestroy(objFS);
	free(pRaw);
	xrtMemDebugReset();

	printf("[memdebug-core] PASS\n");
}

#endif
