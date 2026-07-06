#ifndef XRT_TEST_FILE_DIRSCAN_EX_H
#define XRT_TEST_FILE_DIRSCAN_EX_H

typedef struct {
	str sRootDir;
	str sSubDir;
	str sRootFile;
	str sChildFile;
	int iFileCount;
	int iDirEnterCount;
	int iDirLeaveCount;
	int iBadParamCount;
	int iBadSizeCount;
	int iNullPlatformDataCount;
	int bSeenRootFile;
	int bSeenChildFile;
	int bSeenSubEnter;
	int bSeenSubLeave;
} xrt_test_dirscan_ex_ctx;


static void __Test_FileDirScanEx_Require(bool bCond, const char* sMsg)
{
	if ( !bCond ) {
		fprintf(stderr, "[dirscan_ex] FAIL: %s\n", sMsg ? sMsg : "(no message)");
		exit(1);
	}
}


static bool __Test_FileDirScanEx_StrEq(str sText, size_t iSize, const char* sExpected)
{
	size_t iExpectedSize;
	if ( sText == NULL || sExpected == NULL ) {
		return FALSE;
	}
	iExpectedSize = strlen(sExpected);
	return iSize == iExpectedSize && memcmp(sText, sExpected, iExpectedSize) == 0;
}


static bool __Test_FileDirScanEx_PathEq(str sText, size_t iSize, str sExpected)
{
	size_t iExpectedSize;
	if ( sText == NULL || sExpected == NULL ) {
		return FALSE;
	}
	iExpectedSize = strlen((const char*)sExpected);
	return iSize == iExpectedSize && memcmp(sText, sExpected, iExpectedSize) == 0;
}


static int __Test_FileDirScanEx_Callback(str sDir, size_t iDirSize, str sName, size_t iNameSize, str sPath, size_t iPathSize, int bDir, ptr pData, ptr Param)
{
	xrt_test_dirscan_ex_ctx* pCtx = (xrt_test_dirscan_ex_ctx*)Param;

	if ( pCtx == NULL ) {
		return FALSE;
	}
	if ( Param != pCtx ) {
		pCtx->iBadParamCount++;
	}
	if ( pData == NULL ) {
		pCtx->iNullPlatformDataCount++;
	}
	if ( (sDir == NULL) || (strlen((const char*)sDir) != iDirSize) ||
	     (sName == NULL) || (strlen((const char*)sName) != iNameSize) ||
	     (sPath == NULL) || (strlen((const char*)sPath) != iPathSize) ) {
		pCtx->iBadSizeCount++;
	}

	if ( bDir == 0 ) {
		pCtx->iFileCount++;
		if ( __Test_FileDirScanEx_StrEq(sName, iNameSize, "root.txt") ) {
			if ( __Test_FileDirScanEx_PathEq(sDir, iDirSize, pCtx->sRootDir) &&
			     __Test_FileDirScanEx_PathEq(sPath, iPathSize, pCtx->sRootFile) ) {
				pCtx->bSeenRootFile = TRUE;
			}
		} else if ( __Test_FileDirScanEx_StrEq(sName, iNameSize, "child.txt") ) {
			if ( __Test_FileDirScanEx_PathEq(sDir, iDirSize, pCtx->sSubDir) &&
			     __Test_FileDirScanEx_PathEq(sPath, iPathSize, pCtx->sChildFile) ) {
				pCtx->bSeenChildFile = TRUE;
			}
		}
	} else if ( bDir == 1 ) {
		pCtx->iDirEnterCount++;
		if ( __Test_FileDirScanEx_StrEq(sName, iNameSize, "sub") &&
		     __Test_FileDirScanEx_PathEq(sDir, iDirSize, pCtx->sRootDir) &&
		     __Test_FileDirScanEx_PathEq(sPath, iPathSize, pCtx->sSubDir) ) {
			pCtx->bSeenSubEnter = TRUE;
		}
	} else if ( bDir == 2 ) {
		pCtx->iDirLeaveCount++;
		if ( __Test_FileDirScanEx_StrEq(sName, iNameSize, "sub") &&
		     __Test_FileDirScanEx_PathEq(sDir, iDirSize, pCtx->sRootDir) &&
		     __Test_FileDirScanEx_PathEq(sPath, iPathSize, pCtx->sSubDir) ) {
			pCtx->bSeenSubLeave = TRUE;
		}
	}
	return FALSE;
}


static void __Test_FileDirScanEx_ResetCtx(xrt_test_dirscan_ex_ctx* pCtx)
{
	str sRootDir = pCtx->sRootDir;
	str sSubDir = pCtx->sSubDir;
	str sRootFile = pCtx->sRootFile;
	str sChildFile = pCtx->sChildFile;
	memset(pCtx, 0, sizeof(*pCtx));
	pCtx->sRootDir = sRootDir;
	pCtx->sSubDir = sSubDir;
	pCtx->sRootFile = sRootFile;
	pCtx->sChildFile = sChildFile;
}


static void Test_FileDirScanEx(xrtGlobalData* xCore)
{
	xrt_test_dirscan_ex_ctx tCtx;
	int iScanned;

	printf("[dirscan_ex] start\n");

	__Test_FileDirScanEx_Require(xCore != NULL, "runtime core should not be null");
	memset(&tCtx, 0, sizeof(tCtx));

	tCtx.sRootDir = xrtPathJoin(3, xCore->AppPath, "test", "dirscan_ex");
	tCtx.sSubDir = xrtPathJoin(4, xCore->AppPath, "test", "dirscan_ex", "sub");
	tCtx.sRootFile = xrtPathJoin(4, xCore->AppPath, "test", "dirscan_ex", "root.txt");
	tCtx.sChildFile = xrtPathJoin(5, xCore->AppPath, "test", "dirscan_ex", "sub", "child.txt");

	__Test_FileDirScanEx_Require(tCtx.sRootDir != NULL, "root dir path build failed");
	__Test_FileDirScanEx_Require(tCtx.sSubDir != NULL, "sub dir path build failed");
	__Test_FileDirScanEx_Require(tCtx.sRootFile != NULL, "root file path build failed");
	__Test_FileDirScanEx_Require(tCtx.sChildFile != NULL, "child file path build failed");

	xrtDirDelete(tCtx.sRootDir);
	__Test_FileDirScanEx_Require(xrtDirCreateAll(tCtx.sSubDir) == TRUE, "fixture directory create failed");
	__Test_FileDirScanEx_Require(xrtFilePutAll(tCtx.sRootFile, "root", 4) == 4, "root fixture write failed");
	__Test_FileDirScanEx_Require(xrtFilePutAll(tCtx.sChildFile, "child", 5) == 5, "child fixture write failed");

	iScanned = xrtDirScanEx(tCtx.sRootDir, TRUE, __Test_FileDirScanEx_Callback, &tCtx);
	__Test_FileDirScanEx_Require(iScanned == 2, "recursive scan should count exactly 2 files");
	__Test_FileDirScanEx_Require(tCtx.iFileCount == 2, "recursive callback should see exactly 2 files");
	__Test_FileDirScanEx_Require(tCtx.iDirEnterCount == 1, "recursive callback should see sub dir enter");
	__Test_FileDirScanEx_Require(tCtx.iDirLeaveCount == 1, "recursive callback should see sub dir leave");
	__Test_FileDirScanEx_Require(tCtx.iBadParamCount == 0, "user Param should arrive unchanged");
	__Test_FileDirScanEx_Require(tCtx.iBadSizeCount == 0, "callback sizes should match text lengths");
	__Test_FileDirScanEx_Require(tCtx.iNullPlatformDataCount == 0, "platform data should not be null");
	__Test_FileDirScanEx_Require(tCtx.bSeenRootFile && tCtx.bSeenChildFile && tCtx.bSeenSubEnter && tCtx.bSeenSubLeave, "recursive callback payload mismatch");

	__Test_FileDirScanEx_ResetCtx(&tCtx);
	iScanned = xrtDirScanEx(tCtx.sRootDir, FALSE, __Test_FileDirScanEx_Callback, &tCtx);
	__Test_FileDirScanEx_Require(iScanned == 1, "non-recursive scan should count only root file");
	__Test_FileDirScanEx_Require(tCtx.iFileCount == 1 && tCtx.bSeenRootFile && !tCtx.bSeenChildFile, "non-recursive file payload mismatch");
	__Test_FileDirScanEx_Require(tCtx.iDirEnterCount == 1 && tCtx.iDirLeaveCount == 1, "non-recursive scan should still report child dir enter/leave");
	__Test_FileDirScanEx_Require(tCtx.iBadParamCount == 0 && tCtx.iBadSizeCount == 0 && tCtx.iNullPlatformDataCount == 0, "non-recursive callback contract mismatch");

	__Test_FileDirScanEx_Require(xrtDirCount(tCtx.sRootDir, TRUE, 1) == 2, "recursive file count mismatch");
	__Test_FileDirScanEx_Require(xrtDirCount(tCtx.sRootDir, TRUE, 2) == 1, "recursive dir count mismatch");
	__Test_FileDirScanEx_Require(xrtDirCount(tCtx.sRootDir, TRUE, 0) == 3, "recursive all count mismatch");
	__Test_FileDirScanEx_Require(xrtDirCount(tCtx.sRootDir, FALSE, 0) == 2, "non-recursive all count mismatch");
	__Test_FileDirScanEx_Require(xrtDirSize(tCtx.sRootDir, TRUE) == 9, "recursive dir size mismatch");
	__Test_FileDirScanEx_Require(xrtDirSize(tCtx.sRootDir, FALSE) == 4, "non-recursive dir size mismatch");
	__Test_FileDirScanEx_Require(xrtDirIsEmpty(tCtx.sRootDir) == FALSE, "fixture dir should not be empty");
	__Test_FileDirScanEx_Require(xrtDirIsRoot((str)"") == TRUE, "empty path should be treated as root");
	__Test_FileDirScanEx_Require(xrtDirClean((str)"") == FALSE, "empty path clean should be rejected");
	__Test_FileDirScanEx_Require(xrtDirClean(tCtx.sRootDir) == TRUE, "dir clean should succeed");
	__Test_FileDirScanEx_Require(xrtDirExists(tCtx.sRootDir) == TRUE, "dir clean should keep root dir");
	__Test_FileDirScanEx_Require(xrtDirIsEmpty(tCtx.sRootDir) == TRUE, "dir clean should remove children");
	__Test_FileDirScanEx_Require(xrtDirEnsureEmpty(tCtx.sSubDir) == TRUE, "ensure empty should create missing child dir");
	__Test_FileDirScanEx_Require(xrtDirExists(tCtx.sSubDir) == TRUE, "ensure empty should leave child dir present");
	__Test_FileDirScanEx_Require(xrtDirEnsureEmpty(tCtx.sRootDir) == TRUE, "ensure empty should clean existing dir");
	__Test_FileDirScanEx_Require(xrtDirExists(tCtx.sRootDir) == TRUE && xrtDirIsEmpty(tCtx.sRootDir) == TRUE, "ensure empty should keep empty root dir");

	__Test_FileDirScanEx_Require(xrtDirDelete(tCtx.sRootDir) >= 0, "fixture cleanup failed");

	xrtFree(tCtx.sRootDir);
	xrtFree(tCtx.sSubDir);
	xrtFree(tCtx.sRootFile);
	xrtFree(tCtx.sChildFile);

	printf("[dirscan_ex] PASS\n");
}

#endif
