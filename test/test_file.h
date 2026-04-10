


// 扫描文件回调函数
int FileScanProc(str sPath, size_t iSize, int bDir, ptr pData, ptr Param)
{
	if ( bDir == 1 ) {
		printf("\tdir+ : %s\n", sPath);
	} else if ( bDir == 2 ) {
		printf("\tdir- : %s\n", sPath);
	} else {
		printf("\tfile : %s\n", sPath);
	}
	return FALSE;
}



// File 库测试
void Test_File(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n File 库测试 :\n\n");
	
	//*
	printf("---------------- 编码自动识别\n\n");
	str sPath = xrtPathJoin(3, xCore->AppPath, "test", "ascii.txt");
	printf("test : %s\n", sPath);
	xfile f_a = xrtOpen(sPath, TRUE, XRT_CP_AUTO);
	size_t iFileSize = xrtGetEOF(f_a);
	str sText = xrtRead(f_a, iFileSize - f_a->BOM, NULL);
	printf("Charset (65501) : %d\n", f_a->Charset);
	printf("Size : %d\n", iFileSize);
	printf("BOM : %d\n", f_a->BOM);
	printf("Text : %s\n\n", sText);
	
	sPath = xrtPathJoin(3, xCore->AppPath, "test", "gb2312.txt");
	printf("test : %s\n", sPath);
	xfile f_b = xrtOpen(sPath, TRUE, XRT_CP_AUTO);
	iFileSize = xrtGetEOF(f_b);
	sText = xrtRead(f_b, iFileSize - f_b->BOM, NULL);
	printf("Charset (0) : %d\n", f_b->Charset);
	printf("Size : %d\n", iFileSize);
	printf("BOM : %d\n", f_b->BOM);
	printf("Text : %s\n\n", sText);
	
	sPath = xrtPathJoin(3, xCore->AppPath, "test", "utf8.txt");
	printf("test : %s\n", sPath);
	xfile f_c = xrtOpen(sPath, TRUE, XRT_CP_AUTO);
	iFileSize = xrtGetEOF(f_c);
	sText = xrtRead(f_c, iFileSize - f_c->BOM, NULL);
	printf("Charset (65501) : %d\n", f_c->Charset);
	printf("Size : %d\n", iFileSize);
	printf("BOM : %d\n", f_c->BOM);
	printf("Text : %s\n\n", sText);
	
	sPath = xrtPathJoin(3, xCore->AppPath, "test", "utf8_bom.txt");
	printf("test : %s\n", sPath);
	xfile f_d = xrtOpen(sPath, TRUE, XRT_CP_AUTO);
	iFileSize = xrtGetEOF(f_d);
	sText = xrtRead(f_d, iFileSize - f_d->BOM, NULL);
	printf("Charset (65501) : %d\n", f_d->Charset);
	printf("Size : %d\n", iFileSize);
	printf("BOM : %d\n", f_d->BOM);
	printf("Text : %s\n\n", sText);
	
	sPath = xrtPathJoin(3, xCore->AppPath, "test", "utf16.txt");
	printf("test : %s\n", sPath);
	xfile f_e = xrtOpen(sPath, TRUE, XRT_CP_AUTO);
	iFileSize = xrtGetEOF(f_e);
	sText = xrtRead(f_e, iFileSize - f_e->BOM, NULL);
	printf("Charset (1200) : %d\n", f_e->Charset);
	printf("Size : %d\n", iFileSize);
	printf("BOM : %d\n", f_e->BOM);
	printf("Text : %s\n\n", sText);
	
	sPath = xrtPathJoin(3, xCore->AppPath, "test", "utf16_be.txt");
	printf("test : %s\n", sPath);
	xfile f_f = xrtOpen(sPath, TRUE, XRT_CP_AUTO);
	iFileSize = xrtGetEOF(f_f);
	sText = xrtRead(f_f, iFileSize - f_f->BOM, NULL);
	printf("Charset (1201) : %d\n", f_f->Charset);
	printf("Size : %d\n", iFileSize);
	printf("BOM : %d\n", f_f->BOM);
	printf("Text : %s\n\n", sText);
	
	sPath = xrtPathJoin(3, xCore->AppPath, "test", "utf16_bom.txt");
	printf("test : %s\n", sPath);
	xfile f_g = xrtOpen(sPath, TRUE, XRT_CP_AUTO);
	iFileSize = xrtGetEOF(f_g);
	sText = xrtRead(f_g, iFileSize - f_g->BOM, NULL);
	printf("Charset (1200) : %d\n", f_g->Charset);
	printf("Size : %d\n", iFileSize);
	printf("BOM : %d\n", f_g->BOM);
	printf("Text : %s\n\n", sText);
	
	sPath = xrtPathJoin(3, xCore->AppPath, "test", "utf16_be_bom.txt");
	printf("test : %s\n", sPath);
	xfile f_h = xrtOpen(sPath, TRUE, XRT_CP_AUTO);
	iFileSize = xrtGetEOF(f_h);
	sText = xrtRead(f_h, iFileSize - f_h->BOM, NULL);
	printf("Charset (1201) : %d\n", f_h->Charset);
	printf("Size : %d\n", iFileSize);
	printf("BOM : %d\n", f_h->BOM);
	printf("Text : %s\n\n", sText);
	
	sPath = xrtPathJoin(3, xCore->AppPath, "test", "utf32.txt");
	printf("test : %s\n", sPath);
	xfile f_i = xrtOpen(sPath, TRUE, XRT_CP_AUTO);
	iFileSize = xrtGetEOF(f_i);
	sText = xrtRead(f_i, iFileSize - f_i->BOM, NULL);
	printf("Charset (65005) : %d\n", f_i->Charset);
	printf("Size : %d\n", iFileSize);
	printf("BOM : %d\n", f_i->BOM);
	printf("Text : %s\n\n", sText);
	
	sPath = xrtPathJoin(3, xCore->AppPath, "test", "utf32_be.txt");
	printf("test : %s\n", sPath);
	xfile f_j = xrtOpen(sPath, TRUE, XRT_CP_AUTO);
	iFileSize = xrtGetEOF(f_j);
	sText = xrtRead(f_j, iFileSize - f_j->BOM, NULL);
	printf("Charset (65006) : %d\n", f_j->Charset);
	printf("Size : %d\n", iFileSize);
	printf("BOM : %d\n", f_j->BOM);
	printf("Text : %s\n\n", sText);
	
	sPath = xrtPathJoin(3, xCore->AppPath, "test", "utf32_bom.txt");
	printf("test : %s\n", sPath);
	xfile f_k = xrtOpen(sPath, TRUE, XRT_CP_AUTO);
	iFileSize = xrtGetEOF(f_k);
	sText = xrtRead(f_k, iFileSize - f_k->BOM, NULL);
	printf("Charset (65005) : %d\n", f_k->Charset);
	printf("Size : %d\n", iFileSize);
	printf("BOM : %d\n", f_k->BOM);
	printf("Text : %s\n\n", sText);
	
	sPath = xrtPathJoin(3, xCore->AppPath, "test", "utf32_be_bom.txt");
	printf("test : %s\n", sPath);
	xfile f_l = xrtOpen(sPath, TRUE, XRT_CP_AUTO);
	iFileSize = xrtGetEOF(f_l);
	sText = xrtRead(f_l, iFileSize - f_l->BOM, NULL);
	printf("Charset (65006) : %d\n", f_l->Charset);
	printf("Size : %d\n", iFileSize);
	printf("BOM : %d\n", f_l->BOM);
	printf("Text : %s\n\n", sText);
	//*/
	
	//*
	printf("---------------- 文件读取和写入\n");
	str sPath2 = xrtPathJoin(3, xCore->AppPath, "test", "test.txt");
	printf("test : %s\n", sPath2);
	xfile objFile = xrtOpen(sPath2, TRUE, XRT_CP_AUTO);
	str sText2 = xrtRead(objFile, xrtGetEOF(objFile) - objFile->BOM, NULL);
	printf("Text : %s\n\n", sText2);
	
	str sPath3 = xrtPathJoin(3, xCore->AppPath, "test", "test_write_u16be.txt");
	xrtFileDelete(sPath3);
	xfile objFile2 = xrtOpen(sPath3, FALSE, XRT_CP_UTF16_BE | XRT_CP_BOM);
	xrtWrite(objFile2, sText2, 0);
	xrtWrite(objFile2, "\n1234567", 0);
	xrtClose(objFile2);
	
	str sPath4 = xrtPathJoin(3, xCore->AppPath, "test", "test_write_u16.txt");
	xrtFileDelete(sPath4);
	xfile objFile3 = xrtOpen(sPath4, FALSE, XRT_CP_UTF16 | XRT_CP_BOM);
	xrtWrite(objFile3, sText2, 0);
	xrtWrite(objFile3, "\n7654321", 0);
	xrtClose(objFile3);
	
	str sPath5 = xrtPathJoin(3, xCore->AppPath, "test", "test_write_u32.txt");
	xrtFileWriteAll(sPath5, "xxrpa.com 清浅池塘边，重生破土的冲动", 0, XRT_CP_UTF32 | XRT_CP_BOM);
	xrtFileAppend(sPath5, "\nxdoc.online 天地正玲珑，殡葬了飞虫", 0, XRT_CP_UTF32 | XRT_CP_BOM);
	str sRetPath5 = xrtFileReadAll(sPath5, XRT_CP_AUTO, NULL);
	printf("xrtFileReadAll : %s\n", sRetPath5);
	//*/
	
	//*
	printf("---------------- 文件操作测试\n");
	str sPathA1 = xrtPathJoin(3, xCore->AppPath, "test", "test.txt");
	str sPathA2 = xrtPathJoin(3, xCore->AppPath, "test", "test_copy.txt");
	str sPathA3 = xrtPathJoin(3, xCore->AppPath, "test", "test_movesrc.txt");
	str sPathA4 = xrtPathJoin(3, xCore->AppPath, "test", "test_move.txt");
	str sPathA5 = xrtPathJoin(3, xCore->AppPath, "test", "test_delete.txt");
	str sPathA6 = xrtPathJoin(3, xCore->AppPath, "test", "test222.txt");
	str sPathA7 = xrtPathJoin(3, xCore->AppPath, "test", "test_size.txt");
	str sPathAX = xrtPathJoin(2, xCore->AppPath, "test");
	xrtFileCopy(sPathA1, sPathA2, TRUE);
	xrtFileCopy(sPathA1, sPathA3, TRUE);
	xrtFileCopy(sPathA1, sPathA7, TRUE);
	xrtFileMove(sPathA3, sPathA4, TRUE);
	xrtFileDelete(sPathA5);
	xrtFileSetSize(sPathA7, 10 * 1024 * 1024);
	printf("xrtPathExists : %s = %d\n", sPathA1, xrtPathExists(sPathA1));
	printf("xrtPathExists : %s = %d\n", sPathA6, xrtPathExists(sPathA6));
	printf("xrtPathExists : %s = %d\n", sPathAX, xrtPathExists(sPathAX));
	printf("xrtFileExists : %s = %d\n", sPathA1, xrtFileExists(sPathA1));
	printf("xrtFileExists : %s = %d\n", sPathA6, xrtFileExists(sPathA6));
	printf("xrtFileExists : %s = %d\n", sPathAX, xrtFileExists(sPathAX));
	printf("xrtDirExists : %s = %d\n", sPathA1, xrtDirExists(sPathA1));
	printf("xrtDirExists : %s = %d\n", sPathA6, xrtDirExists(sPathA6));
	printf("xrtDirExists : %s = %d\n", sPathAX, xrtDirExists(sPathAX));
	printf("xrtFileGetSize : %s = %d\n", sPathA1, xrtFileGetSize(sPathA1));
	printf("xrtFileSetSize : %s = 10MB\n", sPathA7);
	printf("xrtFileGetSize : %s = %d\n", sPathA7, xrtFileGetSize(sPathA7));
	printf("xrtFileGetAttr : %s = %d\n", sPathA1, xrtFileGetAttr(sPathA1));
	printf("xrtFileCopy : %s\n", sPathA2);
	printf("xrtFileMove : %s\n", sPathA4);
	printf("xrtFileDelete : %s\n", sPathA5);
	//*/

	//*
	printf("---------------- 文件指针操作测试\n");
	str sPathPtr1 = xrtPathJoin(3, xCore->AppPath, "test", "test_ptr.txt");
	xfile fPtr1 = xrtOpen(sPathPtr1, FALSE, XRT_CP_UTF8);
	xrtWrite(fPtr1, "Hello World!", 0);
	xrtWrite(fPtr1, "\nXRT File Test", 0);
	printf("xrtSeek : seek to position 6\n");
	xrtSeek(fPtr1, 6, XRT_SEEK_SET);
	printf("xrtTell : current position = %d\n", xrtTell(fPtr1));
	int64 iLargeSeek = ((int64)5 * 1024 * 1024 * 1024) + 123;
	xrtSeek(fPtr1, iLargeSeek, XRT_SEEK_SET);
	printf("xrtSeek : seek to >4GB position = %lld\n", iLargeSeek);
	printf("xrtTell : large position = %llu\n", (unsigned long long)xrtTell(fPtr1));
	printf("xrtSeek large : %s\n", ((uint64)xrtTell(fPtr1) == (uint64)iLargeSeek) ? "PASS" : "FAIL");
	xrtSeek(fPtr1, 6, XRT_SEEK_SET);
	printf("xrtEOF : EOF = %d\n", xrtEOF(fPtr1));
	printf("xrtGetEOF : file size = %d\n", xrtGetEOF(fPtr1));
	xrtClose(fPtr1);

	printf("xrtGet : read from file\n");
	xfile fPtr2 = xrtOpen(sPathPtr1, TRUE, XRT_CP_UTF8);
	size_t iReadSize = 0;
	ptr pRead = xrtGet(fPtr2, 100, &iReadSize);
	printf("Read %d bytes\n", (int)iReadSize);
	if ( pRead != NULL && iReadSize > 0 ) {
		printf("Content : %s\n", (str)pRead);
		xrtFree(pRead);
	}
	xrtClose(fPtr2);

	printf("xrtPut : write buffer to file\n");
	str sPathPtr2 = xrtPathJoin(3, xCore->AppPath, "test", "test_put.txt");
	xfile fPtr3 = xrtOpen(sPathPtr2, FALSE, XRT_CP_UTF8);
	str sWrite = "Test Buffer Write";
	xrtPut(fPtr3, sWrite, strlen(sWrite));
	xrtClose(fPtr3);

	printf("xrtFilePutAll : write binary data\n");
	str sPathPtr3 = xrtPathJoin(3, xCore->AppPath, "test", "test_putall.txt");
	uint8 u8Data[] = { 0x01, 0x02, 0x03, 0x04, 0x05 };
	xrtFilePutAll(sPathPtr3, u8Data, sizeof(u8Data));

	printf("xrtFileGetAll : read binary data\n");
	ptr pReadData = xrtFileGetAll(sPathPtr3, NULL);
	if ( pReadData ) {
		uint8* pU8 = (uint8*)pReadData;
		printf("Read data: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n", pU8[0], pU8[1], pU8[2], pU8[3], pU8[4]);
		xrtFree(pReadData);
	}

	printf("xrtFileGetAccessTime : get file access time\n");
	xtime tAccess = xrtFileGetAccessTime(sPathA1);
	printf("Access time: %lld\n", tAccess);

	printf("xrtFileGetChangeTime : get file change time\n");
	xtime tChange = xrtFileGetChangeTime(sPathA1);
	printf("Change time: %lld\n", tChange);

	printf("xrtFileSetAttr : set file attributes\n");
	int iOldAttr = xrtFileGetAttr(sPathA7);
	printf("Old attr: %d\n", iOldAttr);
	xrtFileSetAttr(sPathA7, 32);
	int iNewAttr = xrtFileGetAttr(sPathA7);
	printf("New attr: %d\n", iNewAttr);

	printf("xrtDirCreate : create directory\n");
	str sPathDir1 = xrtPathJoin(3, xCore->AppPath, "test", "newdir");
	xrtDirCreate(sPathDir1);
	printf("xrtDirCreate : %s\n", sPathDir1);
	printf("xrtDirExists : %d\n", xrtDirExists(sPathDir1));

	printf("xrtDirCreateAll : create nested directories\n");
	str sPathDir2 = xrtPathJoin(3, xCore->AppPath, "test", "nested/dir/path");
	xrtDirCreateAll(sPathDir2);
	printf("xrtDirCreateAll : %s\n", sPathDir2);
	printf("xrtDirExists : %d\n", xrtDirExists(sPathDir2));

	xrtDirDelete(sPathDir1);
	xrtDirDelete(sPathDir2);
	//*/
	
	//*
	printf("---------------- 遍历文件测试\n");
	printf("xrtDirScan : %s\n", xCore->AppPath);
	printf("FileCount : %d\n\n", xrtDirScan(xCore->AppPath, FALSE, FileScanProc, NULL));
	printf("xrtDirScan [Recu] : %s\n", xCore->AppPath);
	printf("FileCount : %d\n\n", xrtDirScan(xCore->AppPath, TRUE, FileScanProc, NULL));
	//*/
	
	//*
	printf("---------------- 目录操作测试\n");
	#if defined(_WIN32) || defined(_WIN64)
		printf("xrtDirCopy : %s -> c:\\123\n", xCore->AppPath);
		printf("FileCount : %d\n\n", xrtDirCopy(xCore->AppPath, "c:\\123", FALSE));
		#if defined(_WIN32) || defined(_WIN64)
			system("pause");
		#else
			printf("Press Enter to continue...");
			getchar();
		#endif
		printf("xrtDirMove : c:\\123 -> c:\\456\n");
		printf("FileCount : %d\n\n", xrtDirMove("c:\\123", "c:\\456", TRUE));
		#if defined(_WIN32) || defined(_WIN64)
			system("pause");
		#else
			printf("Press Enter to continue...");
			getchar();
		#endif
		printf("xrtDirDelete : c:\\456\n");
		printf("FileCount : %d\n\n", xrtDirDelete("c:\\456"));
	#else
		printf("xrtDirCopy : %s -> /home/123\n", xCore->AppPath);
		printf("FileCount : %d\n\n", xrtDirCopy(xCore->AppPath, "/home/123", FALSE));
		printf("Press any key to continue...\n");
		getchar();
		printf("xrtDirMove : /home/123 -> /home/456\n");
		printf("FileCount : %d\n\n", xrtDirMove("/home/123", "/home/456", TRUE));
		printf("Press any key to continue...\n");
		getchar();
		printf("xrtDirDelete : /home/456\n");
		printf("FileCount : %d\n\n", xrtDirDelete("/home/456"));
	#endif
	//*/
}


