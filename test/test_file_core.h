#ifndef XRT_TEST_FILE_CORE_H
#define XRT_TEST_FILE_CORE_H

static void __Test_FileCoreRequire(bool bCond, const char* sMsg)
{
	if ( !bCond ) {
		fprintf(stderr, "[file_core] FAIL: %s\n", sMsg ? sMsg : "(no message)");
		exit(1);
	}
}

static void Test_FileCore(xrtGlobalData* xCore)
{
	str sDir;
	str sPath;
	const uint8 tData[] = { 0x10u, 0x20u, 0x30u, 0x40u, 0x50u, 0x60u };
	xfile objFile = NULL;
	uint8 tBuff[16];
	size_t iRet;

	printf("[file_core] start\n");

	__Test_FileCoreRequire(xCore != NULL, "runtime core should not be null");

	sDir = xrtPathJoin(2, xCore->AppPath, "test");
	__Test_FileCoreRequire(sDir != NULL, "test directory path build failed");
	__Test_FileCoreRequire(xrtDirCreateAll(sDir) == TRUE, "test directory create failed");

	sPath = xrtPathJoin(3, xCore->AppPath, "test", "xrt_getbuffer.bin");
	__Test_FileCoreRequire(sPath != NULL, "fixture path build failed");

	xrtFileDelete(sPath);
	__Test_FileCoreRequire(
		xrtFilePutAll(sPath, (ptr)tData, sizeof(tData)) == (int)sizeof(tData),
		"fixture write failed");

	objFile = xrtOpen(sPath, TRUE, XRT_CP_BINARY);
	__Test_FileCoreRequire(objFile != NULL, "fixture open failed");

	memset(tBuff, 0xCC, sizeof(tBuff));
	iRet = xrtGetBuffer(objFile, tBuff, sizeof(tData));
	__Test_FileCoreRequire(iRet == sizeof(tData), "full read size mismatch");
	__Test_FileCoreRequire(memcmp(tBuff, tData, sizeof(tData)) == 0, "full read payload mismatch");
	__Test_FileCoreRequire(xrtTell(objFile) == sizeof(tData), "full read should advance to EOF");
	__Test_FileCoreRequire(xrtEOF(objFile) == TRUE, "full read should reach EOF");

	__Test_FileCoreRequire(xrtSeek(objFile, 4, XRT_SEEK_SET) == 4u, "seek to tail failed");
	memset(tBuff, 0xCC, sizeof(tBuff));
	iRet = xrtGetBuffer(objFile, tBuff, sizeof(tData));
	__Test_FileCoreRequire(iRet == 2u, "tail read should only return remaining bytes");
	__Test_FileCoreRequire(memcmp(tBuff, tData + 4, 2) == 0, "tail read payload mismatch");
	__Test_FileCoreRequire(tBuff[2] == 0xCCu && tBuff[3] == 0xCCu, "tail read should not overwrite extra bytes");
	__Test_FileCoreRequire(xrtTell(objFile) == sizeof(tData), "tail read should stop at EOF");

	xrtClearError();
	memset(tBuff, 0xAB, sizeof(tBuff));
	iRet = xrtGetBuffer(objFile, tBuff, 1);
	__Test_FileCoreRequire(iRet == 0u, "EOF read should return zero");
	__Test_FileCoreRequire(tBuff[0] == 0xABu, "EOF read should not modify destination buffer");
	__Test_FileCoreRequire(xrtTell(objFile) == sizeof(tData), "EOF read should not move cursor");
	__Test_FileCoreRequire(
		(xrtGetError() == NULL) || (xrtGetError() == xCore->sNull),
		"EOF read should not set an error");

	__Test_FileCoreRequire(xrtSeek(objFile, 1, XRT_SEEK_SET) == 1u, "seek for offset buffer read failed");
	memset(tBuff, 0x7A, sizeof(tBuff));
	iRet = xrtGetBuffer(objFile, tBuff + 2, 3);
	__Test_FileCoreRequire(iRet == 3u, "offset buffer read size mismatch");
	__Test_FileCoreRequire(tBuff[0] == 0x7Au && tBuff[1] == 0x7Au, "offset buffer read should preserve prefix");
	__Test_FileCoreRequire(memcmp(tBuff + 2, tData + 1, 3) == 0, "offset buffer read payload mismatch");
	__Test_FileCoreRequire(tBuff[5] == 0x7Au, "offset buffer read should preserve suffix");
	__Test_FileCoreRequire(xrtTell(objFile) == 4u, "offset buffer read should advance cursor");

	__Test_FileCoreRequire(xrtSeek(objFile, 2, XRT_SEEK_SET) == 2u, "seek for zero-size read failed");
	memset(tBuff, 0x5A, sizeof(tBuff));
	iRet = xrtGetBuffer(objFile, tBuff + 4, 0);
	__Test_FileCoreRequire(iRet == 0u, "zero-size read should return zero");
	__Test_FileCoreRequire(tBuff[3] == 0x5Au && tBuff[4] == 0x5Au && tBuff[5] == 0x5Au, "zero-size read should not touch buffer");
	__Test_FileCoreRequire(xrtTell(objFile) == 2u, "zero-size read should not move cursor");

	xrtClose(objFile);
	objFile = NULL;
	__Test_FileCoreRequire(xrtFileDelete(sPath) == TRUE, "fixture cleanup failed");

	printf("[file_core] PASS\n");
}

#endif
