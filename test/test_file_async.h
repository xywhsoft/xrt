#ifndef TEST_FILE_ASYNC_H
#define TEST_FILE_ASYNC_H

#if !defined(XRT_NO_NETWORK)

// 内部函数：__Test_FileAsyncRequire
static void __Test_FileAsyncRequire(bool bCond, const char* sMsg)
{
	if ( !bCond ) {
		fprintf(stderr, "[file_async] FAIL: %s\n", sMsg ? sMsg : "(no message)");
		exit(1);
	}
}


// 内部函数：__Test_FileAsyncWaitIo
static xasyncfileio* __Test_FileAsyncWaitIo(xfuture* pFuture, const char* sMsg)
{
	xasyncfileio* pInfo;

	__Test_FileAsyncRequire(pFuture != NULL, "future should not be null");
	pInfo = (xasyncfileio*)xFutureWaitValueTimeout(pFuture, 5000);
	__Test_FileAsyncRequire(pInfo != NULL, sMsg);
	__Test_FileAsyncRequire(xFutureStatus(pFuture) == XRT_NET_OK, "future status should be ok");
	xFutureRelease(pFuture);
	return pInfo;
}


// 内部函数：__Test_FileAsyncWaitBuf
static xasyncfilebuf* __Test_FileAsyncWaitBuf(xfuture* pFuture, const char* sMsg)
{
	xasyncfilebuf* pBuf;

	__Test_FileAsyncRequire(pFuture != NULL, "future should not be null");
	pBuf = (xasyncfilebuf*)xFutureWaitValueTimeout(pFuture, 5000);
	__Test_FileAsyncRequire(pBuf != NULL, sMsg);
	__Test_FileAsyncRequire(xFutureStatus(pFuture) == XRT_NET_OK, "future status should be ok");
	xFutureRelease(pFuture);
	return pBuf;
}


// 内部函数：__Test_FileAsyncWaitOk
static void __Test_FileAsyncWaitOk(xfuture* pFuture, const char* sMsg)
{
	__Test_FileAsyncRequire(pFuture != NULL, "future should not be null");
	__Test_FileAsyncRequire(xFutureWaitTimeout(pFuture, 5000), sMsg);
	__Test_FileAsyncRequire(xFutureStatus(pFuture) == XRT_NET_OK, "future status should be ok");
	xFutureRelease(pFuture);
}


// 内部函数：__Test_FileAsyncWaitFail
static void __Test_FileAsyncWaitFail(xfuture* pFuture, const char* sMsg)
{
	const char* sError;
	xnet_result iStatus;

	__Test_FileAsyncRequire(pFuture != NULL, "future should not be null");
	iStatus = xrtNetFutureWait((xnetfuture*)pFuture, 5000u);
	__Test_FileAsyncRequire(iStatus != XRT_NET_TIMEOUT && iStatus != XRT_NET_AGAIN, sMsg);
	__Test_FileAsyncRequire(xFutureStatus(pFuture) != XRT_NET_OK, "future status should fail");
	sError = xFutureError(pFuture);
	__Test_FileAsyncRequire(sError != NULL && sError[0] != '\0', "future error should not be empty");
	xFutureRelease(pFuture);
}


// 文件异步测试
static void Test_FileAsync(xrtGlobalData* xCore)
{
	str sRoot;
	str sObjectPath;
	str sTextPath;
	str sBinaryPath;
	str sCopyPath;
	str sMovePath;
	str sDirSrc;
	str sDirDst;
	str sDirMoved;
	str sNestedDir;
	str sNestedFile;
	str sMissingFile;
	str sMissingDir;
	str sBadParentFile;
	str sDirFailCopy;
	str sDirFailMove;
	xasyncfileconfig tConfig;
	xasyncfile* pFile = NULL;
	xfuture* pFuture = NULL;
	xfuture* arrWriteFuture[2];
	xasyncfileio* pIo = NULL;
	xasyncfilebuf* pBuf = NULL;
	const char sHello[] = "hello";
	const char sWorld[] = " world";
	const char sText[] = "async-text\nline-two";
	const uint8 tBinary[] = { 0x10u, 0x20u, 0x30u, 0x40u, 0x50u, 0x60u };

	printf("[file_async] start\n");

	__Test_FileAsyncRequire(xCore != NULL, "runtime core should not be null");

	sRoot = xrtPathJoin(3, xCore->AppPath, "test", "file_async");
	sObjectPath = xrtPathJoin(4, xCore->AppPath, "test", "file_async", "object.bin");
	sTextPath = xrtPathJoin(4, xCore->AppPath, "test", "file_async", "text.txt");
	sBinaryPath = xrtPathJoin(4, xCore->AppPath, "test", "file_async", "payload.bin");
	sCopyPath = xrtPathJoin(4, xCore->AppPath, "test", "file_async", "copy.txt");
	sMovePath = xrtPathJoin(4, xCore->AppPath, "test", "file_async", "moved.txt");
	sDirSrc = xrtPathJoin(4, xCore->AppPath, "test", "file_async", "dir_src");
	sDirDst = xrtPathJoin(4, xCore->AppPath, "test", "file_async", "dir_dst");
	sDirMoved = xrtPathJoin(4, xCore->AppPath, "test", "file_async", "dir_moved");
	sNestedDir = xrtPathJoin(5, xCore->AppPath, "test", "file_async", "dir_src", "nested");
	sNestedFile = xrtPathJoin(6, xCore->AppPath, "test", "file_async", "dir_src", "nested", "note.txt");
	sMissingFile = xrtPathJoin(4, xCore->AppPath, "test", "file_async", "missing.txt");
	sMissingDir = xrtPathJoin(4, xCore->AppPath, "test", "file_async", "dir_missing");
	sBadParentFile = xrtPathJoin(5, xCore->AppPath, "test", "file_async", "missing_parent", "bad.txt");
	sDirFailCopy = xrtPathJoin(4, xCore->AppPath, "test", "file_async", "dir_fail_copy");
	sDirFailMove = xrtPathJoin(4, xCore->AppPath, "test", "file_async", "dir_fail_move");

	__Test_FileAsyncRequire(sRoot != NULL, "root path create failed");
	__Test_FileAsyncRequire(sObjectPath != NULL, "object path create failed");
	__Test_FileAsyncRequire(sTextPath != NULL, "text path create failed");
	__Test_FileAsyncRequire(sBinaryPath != NULL, "binary path create failed");
	__Test_FileAsyncRequire(sCopyPath != NULL, "copy path create failed");
	__Test_FileAsyncRequire(sMovePath != NULL, "move path create failed");
	__Test_FileAsyncRequire(sDirSrc != NULL, "dir src path create failed");
	__Test_FileAsyncRequire(sDirDst != NULL, "dir dst path create failed");
	__Test_FileAsyncRequire(sDirMoved != NULL, "dir moved path create failed");
	__Test_FileAsyncRequire(sNestedDir != NULL, "nested dir path create failed");
	__Test_FileAsyncRequire(sNestedFile != NULL, "nested file path create failed");
	__Test_FileAsyncRequire(sMissingFile != NULL, "missing file path create failed");
	__Test_FileAsyncRequire(sMissingDir != NULL, "missing dir path create failed");
	__Test_FileAsyncRequire(sBadParentFile != NULL, "bad parent file path create failed");
	__Test_FileAsyncRequire(sDirFailCopy != NULL, "dir fail copy path create failed");
	__Test_FileAsyncRequire(sDirFailMove != NULL, "dir fail move path create failed");

	xrtDirDelete(sRoot);
	__Test_FileAsyncRequire(xrtDirCreateAll(sRoot) == TRUE, "root dir create failed");

	xrtAsyncFileConfigInit(&tConfig);
	tConfig.iFlags = XAFILE_F_READ | XAFILE_F_WRITE | XAFILE_F_CREATE | XAFILE_F_TRUNCATE;
	tConfig.sPath = sObjectPath;
	pFile = xrtAsyncFileOpen(&tConfig);
	__Test_FileAsyncRequire(pFile != NULL, "async file open failed");

	arrWriteFuture[0] = xrtAsyncFileWriteAt(pFile, 0u, sHello, sizeof(sHello) - 1u);
	arrWriteFuture[1] = xrtAsyncFileWriteAt(pFile, sizeof(sHello) - 1u, sWorld, sizeof(sWorld) - 1u);

	pIo = __Test_FileAsyncWaitIo(arrWriteFuture[0], "first async write should succeed");
	__Test_FileAsyncRequire(pIo->iValue == sizeof(sHello) - 1u, "first write size mismatch");
	__Test_FileAsyncRequire(pIo->iOffset == 0u, "first write offset mismatch");
	xrtAsyncFileIoDestroy(pIo);

	pIo = __Test_FileAsyncWaitIo(arrWriteFuture[1], "second async write should succeed");
	__Test_FileAsyncRequire(pIo->iValue == sizeof(sWorld) - 1u, "second write size mismatch");
	__Test_FileAsyncRequire(pIo->iOffset == sizeof(sHello) - 1u, "second write offset mismatch");
	xrtAsyncFileIoDestroy(pIo);

	__Test_FileAsyncWaitOk(xrtAsyncFileFlush(pFile), "flush should succeed");

	pIo = __Test_FileAsyncWaitIo(xrtAsyncFileGetSize(pFile), "size query should succeed");
	__Test_FileAsyncRequire(pIo->iValue == (sizeof(sHello) + sizeof(sWorld) - 2u), "size query mismatch");
	xrtAsyncFileIoDestroy(pIo);

	pBuf = __Test_FileAsyncWaitBuf(
		xrtAsyncFileReadAt(pFile, 0u, sizeof(sHello) + sizeof(sWorld) - 2u),
		"full read should succeed");
	__Test_FileAsyncRequire(pBuf->iSize == sizeof(sHello) + sizeof(sWorld) - 2u, "full read size mismatch");
	__Test_FileAsyncRequire(memcmp(pBuf->pData, "hello world", 11u) == 0, "full read payload mismatch");
	__Test_FileAsyncRequire(pBuf->bEOF == FALSE, "full read should not mark eof when exact size is requested");
	xrtAsyncFileBufDestroy(pBuf);

	pIo = __Test_FileAsyncWaitIo(xrtAsyncFileSetSize(pFile, 5u), "shrink should succeed");
	__Test_FileAsyncRequire(pIo->iValue == 5u, "shrink size mismatch");
	xrtAsyncFileIoDestroy(pIo);

	pFuture = xrtAsyncFileReadAt(pFile, 0u, 16u);
	xrtAsyncFileClose(pFile);
	pFile = NULL;
	pBuf = __Test_FileAsyncWaitBuf(pFuture, "in-flight read should survive close");
	__Test_FileAsyncRequire(pBuf->iSize == 5u, "post-close read size mismatch");
	__Test_FileAsyncRequire(memcmp(pBuf->pData, "hello", 5u) == 0, "post-close read payload mismatch");
	__Test_FileAsyncRequire(pBuf->bEOF == TRUE, "post-close read should detect eof");
	xrtAsyncFileBufDestroy(pBuf);

	pIo = __Test_FileAsyncWaitIo(
		xrtFileWriteAllAsync(sTextPath, (str)sText, 0u, XRT_CP_UTF8),
		"text write-all should succeed");
	__Test_FileAsyncRequire(pIo->iValue == strlen(sText), "text write-all size mismatch");
	xrtAsyncFileIoDestroy(pIo);

	pBuf = __Test_FileAsyncWaitBuf(xrtFileReadAllAsync(sTextPath, XRT_CP_UTF8), "text read-all should succeed");
	__Test_FileAsyncRequire(pBuf->iSize == strlen(sText), "text read-all size mismatch");
	__Test_FileAsyncRequire(memcmp(pBuf->pData, sText, strlen(sText)) == 0, "text read-all payload mismatch");
	xrtAsyncFileBufDestroy(pBuf);

	pIo = __Test_FileAsyncWaitIo(
		xrtFilePutAllAsync(sBinaryPath, tBinary, sizeof(tBinary)),
		"binary put-all should succeed");
	__Test_FileAsyncRequire(pIo->iValue == sizeof(tBinary), "binary put-all size mismatch");
	xrtAsyncFileIoDestroy(pIo);

	pBuf = __Test_FileAsyncWaitBuf(xrtFileGetAllAsync(sBinaryPath), "binary get-all should succeed");
	__Test_FileAsyncRequire(pBuf->iSize == sizeof(tBinary), "binary get-all size mismatch");
	__Test_FileAsyncRequire(memcmp(pBuf->pData, tBinary, sizeof(tBinary)) == 0, "binary get-all payload mismatch");
	xrtAsyncFileBufDestroy(pBuf);

	pIo = __Test_FileAsyncWaitIo(xrtFileCopyAsync(sTextPath, sCopyPath, TRUE), "file copy should succeed");
	__Test_FileAsyncRequire(pIo->iValue == 1u, "file copy result mismatch");
	xrtAsyncFileIoDestroy(pIo);
	__Test_FileAsyncRequire(xrtFileExists(sCopyPath) == TRUE, "copied file should exist");

	pIo = __Test_FileAsyncWaitIo(xrtFileMoveAsync(sCopyPath, sMovePath, TRUE), "file move should succeed");
	__Test_FileAsyncRequire(pIo->iValue == 1u, "file move result mismatch");
	xrtAsyncFileIoDestroy(pIo);
	__Test_FileAsyncRequire(xrtFileExists(sMovePath) == TRUE, "moved file should exist");
	__Test_FileAsyncRequire(xrtFileExists(sCopyPath) == FALSE, "old moved file should not exist");

	pIo = __Test_FileAsyncWaitIo(xrtFileDeleteAsync(sMovePath), "file delete should succeed");
	__Test_FileAsyncRequire(pIo->iValue == 1u, "file delete result mismatch");
	xrtAsyncFileIoDestroy(pIo);
	__Test_FileAsyncRequire(xrtFileExists(sMovePath) == FALSE, "deleted file should not exist");

	pIo = __Test_FileAsyncWaitIo(xrtDirCreateAllAsync(sNestedDir), "dir create-all should succeed");
	__Test_FileAsyncRequire(pIo->iValue == 1u, "dir create-all result mismatch");
	xrtAsyncFileIoDestroy(pIo);
	__Test_FileAsyncRequire(xrtFileWriteAll(sNestedFile, (str)"nested", 0u, XRT_CP_UTF8) > 0, "nested seed file write failed");

	pIo = __Test_FileAsyncWaitIo(xrtDirCopyAsync(sDirSrc, sDirDst, TRUE), "dir copy should succeed");
	__Test_FileAsyncRequire(xrtDirExists(sDirDst) == TRUE, "copied dir should exist");
	xrtAsyncFileIoDestroy(pIo);

	pIo = __Test_FileAsyncWaitIo(xrtDirMoveAsync(sDirDst, sDirMoved, TRUE), "dir move should succeed");
	__Test_FileAsyncRequire(xrtDirExists(sDirMoved) == TRUE, "moved dir should exist");
	__Test_FileAsyncRequire(xrtDirExists(sDirDst) == FALSE, "source dir should not exist after move");
	xrtAsyncFileIoDestroy(pIo);

	pIo = __Test_FileAsyncWaitIo(xrtDirDeleteAsync(sDirMoved), "dir delete should succeed");
	__Test_FileAsyncRequire(xrtDirExists(sDirMoved) == FALSE, "deleted dir should not exist");
	xrtAsyncFileIoDestroy(pIo);

	__Test_FileAsyncWaitFail(xrtFileReadAllAsync(sMissingFile, XRT_CP_UTF8), "missing text read should fail");
	__Test_FileAsyncWaitFail(xrtFileGetAllAsync(sMissingFile), "missing binary read should fail");
	__Test_FileAsyncWaitFail(
		xrtFileAppendAsync(sBadParentFile, (str)"fail", 4u, XRT_CP_UTF8),
		"append into missing parent should fail");
	__Test_FileAsyncWaitFail(
		xrtFileWriteAllAsync(sBadParentFile, (str)"fail", 4u, XRT_CP_UTF8),
		"write-all into missing parent should fail");
	__Test_FileAsyncWaitFail(
		xrtFilePutAllAsync(sBadParentFile, tBinary, sizeof(tBinary)),
		"put-all into missing parent should fail");
	__Test_FileAsyncWaitFail(xrtDirCopyAsync(sMissingDir, sDirFailCopy, TRUE), "missing dir copy should fail");
	__Test_FileAsyncRequire(xrtDirExists(sDirFailCopy) == FALSE, "failed dir copy should not create destination");
	__Test_FileAsyncWaitFail(xrtDirMoveAsync(sMissingDir, sDirFailMove, TRUE), "missing dir move should fail");
	__Test_FileAsyncRequire(xrtDirExists(sDirFailMove) == FALSE, "failed dir move should not create destination");
	__Test_FileAsyncWaitFail(xrtDirDeleteAsync(sMissingDir), "missing dir delete should fail");

	xrtDirDelete(sRoot);

	xrtFree(sDirFailMove);
	xrtFree(sDirFailCopy);
	xrtFree(sBadParentFile);
	xrtFree(sMissingDir);
	xrtFree(sMissingFile);
	xrtFree(sNestedFile);
	xrtFree(sNestedDir);
	xrtFree(sDirMoved);
	xrtFree(sDirDst);
	xrtFree(sDirSrc);
	xrtFree(sMovePath);
	xrtFree(sCopyPath);
	xrtFree(sBinaryPath);
	xrtFree(sTextPath);
	xrtFree(sObjectPath);
	xrtFree(sRoot);

	printf("[file_async] PASS\n");
}

#endif

#endif
