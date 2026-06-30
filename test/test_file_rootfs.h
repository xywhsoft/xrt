#ifndef XRT_TEST_FILE_ROOTFS_H
#define XRT_TEST_FILE_ROOTFS_H

typedef struct {
	int iCount;
	int iBadParamCount;
	int iBadSizeCount;
	int iBadKindCount;
	int iBadExDirCount;
} xrt_test_file_rootfs_ctx;


static void __Test_FileRootFS_Require(bool bCond, const char* sMsg)
{
	if ( !bCond ) {
		fprintf(stderr, "[file_rootfs] FAIL: %s\n", sMsg ? sMsg : "(no message)");
		exit(1);
	}
}


static int __Test_FileRootFS_Callback(str sPath, size_t iSize, int bDir, ptr pData, ptr Param)
{
	xrt_test_file_rootfs_ctx* pCtx = (xrt_test_file_rootfs_ctx*)Param;
	(void)pData;

	if ( pCtx == NULL ) {
		return TRUE;
	}
	pCtx->iCount++;
	if ( Param != pCtx ) {
		pCtx->iBadParamCount++;
	}
	if ( sPath == NULL || strlen((const char*)sPath) != iSize || iSize == 0 ) {
		pCtx->iBadSizeCount++;
	}
	if ( bDir != 1 ) {
		pCtx->iBadKindCount++;
	}
	return TRUE;
}


static int __Test_FileRootFS_ExCallback(str sDir, size_t iDirSize, str sName, size_t iNameSize, str sPath, size_t iPathSize, int bDir, ptr pData, ptr Param)
{
	xrt_test_file_rootfs_ctx* pCtx = (xrt_test_file_rootfs_ctx*)Param;
	(void)pData;

	if ( pCtx == NULL ) {
		return TRUE;
	}
	pCtx->iCount++;
	if ( Param != pCtx ) {
		pCtx->iBadParamCount++;
	}
	if ( sDir == NULL || iDirSize != 0 || sDir[0] != 0 ) {
		pCtx->iBadExDirCount++;
	}
	if ( sName == NULL || strlen((const char*)sName) != iNameSize || iNameSize == 0 ||
	     sPath == NULL || strlen((const char*)sPath) != iPathSize || iPathSize == 0 ||
	     iNameSize != iPathSize || memcmp(sName, sPath, iNameSize) != 0 ) {
		pCtx->iBadSizeCount++;
	}
	if ( bDir != 1 ) {
		pCtx->iBadKindCount++;
	}
	return TRUE;
}


static void Test_FileRootFS(xrtGlobalData* xCore)
{
	xrt_test_file_rootfs_ctx tCtx;
	int iRet;

	printf("[file_rootfs] start\n");
	__Test_FileRootFS_Require(xCore != NULL, "runtime core should not be null");

	memset(&tCtx, 0, sizeof(tCtx));
	iRet = xrtDirScan((str)"", FALSE, __Test_FileRootFS_Callback, &tCtx);
	__Test_FileRootFS_Require(iRet >= 1, "root scan should expose at least one root entry");
	__Test_FileRootFS_Require(tCtx.iCount == 1, "root scan callback should stop after first entry");
	__Test_FileRootFS_Require(tCtx.iBadParamCount == 0 && tCtx.iBadSizeCount == 0 && tCtx.iBadKindCount == 0, "root scan callback contract mismatch");

	memset(&tCtx, 0, sizeof(tCtx));
	iRet = xrtDirScanEx((str)"", FALSE, __Test_FileRootFS_ExCallback, &tCtx);
	__Test_FileRootFS_Require(iRet >= 1, "root scan ex should expose at least one root entry");
	__Test_FileRootFS_Require(tCtx.iCount == 1, "root scan ex callback should stop after first entry");
	__Test_FileRootFS_Require(tCtx.iBadParamCount == 0 && tCtx.iBadSizeCount == 0 && tCtx.iBadKindCount == 0 && tCtx.iBadExDirCount == 0, "root scan ex callback contract mismatch");

	printf("[file_rootfs] PASS\n");
}

#endif
