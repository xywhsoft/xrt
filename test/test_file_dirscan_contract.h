#ifndef XRT_TEST_FILE_DIRSCAN_CONTRACT_H
#define XRT_TEST_FILE_DIRSCAN_CONTRACT_H

typedef struct {
	void* pExpectedParam;
	int iFileCount;
	int iDirEnterCount;
	int iDirLeaveCount;
	int iBadParamCount;
	int iBadSizeCount;
	int iNullPlatformDataCount;
} xrt_test_dirscan_contract_ctx;

static void __Test_FileDirScanContract_Require(bool bCond, const char* sMsg)
{
	if ( !bCond ) {
		fprintf(stderr, "[dirscan_contract] FAIL: %s\n", sMsg ? sMsg : "(no message)");
		exit(1);
	}
}

static int __Test_FileDirScanContract_Callback(str sPath, size_t iSize, int bDir, ptr pData, ptr Param)
{
	xrt_test_dirscan_contract_ctx* pCtx = (xrt_test_dirscan_contract_ctx*)Param;

	if ( pCtx == NULL ) {
		return TRUE;
	}
	if ( Param != pCtx->pExpectedParam ) {
		pCtx->iBadParamCount++;
	}
	if ( pData == NULL ) {
		pCtx->iNullPlatformDataCount++;
	}
	if ( (sPath == NULL) || (strlen(sPath) != iSize) ) {
		pCtx->iBadSizeCount++;
	}

	if ( bDir == 0 ) {
		pCtx->iFileCount++;
	} else if ( bDir == 1 ) {
		pCtx->iDirEnterCount++;
	} else if ( bDir == 2 ) {
		pCtx->iDirLeaveCount++;
	}
	return FALSE;
}

static void Test_FileDirScanContract(xrtGlobalData* xCore)
{
	str sRootDir;
	str sSubDir;
	str sFile1;
	str sFile2;
	xrt_test_dirscan_contract_ctx tCtx;
	int iScanned;

	printf("[dirscan_contract] start\n");

	__Test_FileDirScanContract_Require(xCore != NULL, "runtime core should not be null");

	sRootDir = xrtPathJoin(3, xCore->AppPath, "test", "dirscan_contract");
	sSubDir = xrtPathJoin(4, xCore->AppPath, "test", "dirscan_contract", "sub");
	sFile1 = xrtPathJoin(4, xCore->AppPath, "test", "dirscan_contract", "root.txt");
	sFile2 = xrtPathJoin(5, xCore->AppPath, "test", "dirscan_contract", "sub", "child.txt");

	__Test_FileDirScanContract_Require(sRootDir != NULL, "root dir path build failed");
	__Test_FileDirScanContract_Require(sSubDir != NULL, "sub dir path build failed");
	__Test_FileDirScanContract_Require(sFile1 != NULL, "file1 path build failed");
	__Test_FileDirScanContract_Require(sFile2 != NULL, "file2 path build failed");

	xrtDirDelete(sRootDir);
	__Test_FileDirScanContract_Require(xrtDirCreateAll(sSubDir) == TRUE, "fixture directory create failed");
	__Test_FileDirScanContract_Require(xrtFilePutAll(sFile1, "root", 4) == 4, "root fixture write failed");
	__Test_FileDirScanContract_Require(xrtFilePutAll(sFile2, "child", 5) == 5, "child fixture write failed");

	memset(&tCtx, 0, sizeof(tCtx));
	tCtx.pExpectedParam = &tCtx;
	iScanned = xrtDirScan(sRootDir, TRUE, __Test_FileDirScanContract_Callback, &tCtx);

	__Test_FileDirScanContract_Require(iScanned == 2, "recursive scan should count exactly 2 files");
	__Test_FileDirScanContract_Require(tCtx.iFileCount == 2, "callback should see exactly 2 file events");
	__Test_FileDirScanContract_Require(tCtx.iDirEnterCount == 1, "callback should see exactly 1 dir-enter event");
	__Test_FileDirScanContract_Require(tCtx.iDirLeaveCount == 1, "callback should see exactly 1 dir-leave event");
	__Test_FileDirScanContract_Require(tCtx.iBadParamCount == 0, "user Param should arrive in the 5th callback argument");
	__Test_FileDirScanContract_Require(tCtx.iBadSizeCount == 0, "iSize should match strlen(sPath) for every callback");
	__Test_FileDirScanContract_Require(tCtx.iNullPlatformDataCount == 0, "platform-specific pData should not be null");

	__Test_FileDirScanContract_Require(xrtDirDelete(sRootDir) >= 0, "fixture cleanup failed");

	xrtFree(sRootDir);
	xrtFree(sSubDir);
	xrtFree(sFile1);
	xrtFree(sFile2);

	printf("[dirscan_contract] PASS\n");
}

#endif
