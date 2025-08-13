


// 打开文件
XXAPI ptr xrtOpen(str sPath, size_t iSize, int bReadOnly, int iCharset)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		wstr sPathW = xrtUTF8to16(sPath, iSize);
		HANDLE hFile = CreateFileW(sPathW, bReadOnly ? GENERIC_READ : GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, bReadOnly ? OPEN_EXISTING : OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		xrtFree(sPathW);
		if ( hFile == INVALID_HANDLE_VALUE ) { return NULL; }
		xfile objFile = xrtMalloc(sizeof(xfile_struct));
		if ( objFile == NULL ) {
			xrtSetError(xCore.ERROR_DESC.MALLOC, FALSE);
			CloseHandle(hFile);
			return NULL;
		}
		LARGE_INTEGER stuSize;
		GetFileSizeEx(hFile, &stuSize);
		objFile->obj = hFile;
		objFile->Size = stuSize.QuadPart;
		objFile->Charset = iCharset;
		objFile->BOM = 0;
		// 自动识别文件编码
		if ( iCharset == XRT_CP_AUTO ) {
			if ( stuSize.QuadPart == 0 ) {
				// 空文件设置为 utf8 编码 ( 无 BOM )
				objFile->Charset = XRT_CP_UTF8;
			} else {
				/*
				char sBOM[4] = { 0, 0, 0, 0 };
				SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
				ReadFile(hFile, sBOM, 4, &xCore.iRet, NULL);
				if ( (BOM[0] == 0xFF) && (BOM[1] == 0xFE) && (BOM[2] == 0x00) && (BOM[3] == 0x00) ) {
					// UTF-32 LE (little-endian)
					objFile->Charset = XRT_CP_UTF32 | XRT_CP_BOM;
					objFile->BOM = 4;
				} else if ( (BOM[0] == 0x00) && (BOM[1] == 0x00) && (BOM[2] == 0xFE) && (BOM[3] == 0xFF) ) {
					// UTF-32 BE (big-endian)
					objFile->Charset = XRT_CP_UTF32_BE | XRT_CP_BOM;
					objFile->BOM = 4;
				} else if ( (BOM[0] == 0xEF) && (BOM[1] == 0xBB) && (BOM[2] == 0xBF) ) {
					// UTF-8
					objFile->Charset = XRT_CP_UTF8 | XRT_CP_BOM;
					objFile->BOM = 3;
				} else if ( (BOM[0] == 0xFF) && (BOM[1] == 0xFE) ) {
					// UTF-16 LE (little-endian)
					objFile->Charset = XRT_CP_UTF16 | XRT_CP_BOM;
					objFile->BOM = 2;
				} else if ( (BOM[0] == 0xFE) && (BOM[1] == 0xFF) ) {
					// UTF-16 BE (big-endian)
					objFile->Charset = XRT_CP_UTF16_BE | XRT_CP_BOM;
					objFile->BOM = 2;
				} else {
					// 无 BOM 文件猜测编码是否为 utf8、utf16、utf32（ 仅支持 little-endian ）
				}
				*/
			}
		}
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
	return NULL;
}



// 关闭文件
XXAPI void xrtClose(ptr objFile)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
}



// 设置游标位置
XXAPI void xrtSeek(ptr objFile, int iOrigin, long iOffset)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
}



// 获取游标位置
XXAPI size_t xrtTell(ptr objFile)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
	return 0;
}



// 是否已经读取到文件末尾
XXAPI int xrtEOF(ptr objFile)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
	return FALSE;
}



// 设置文件末尾
XXAPI int xrtSetEOF(ptr objFile)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
	// ftruncate
	return FALSE;
}



// 从已打开的文件读取数据
XXAPI str xrtRead(ptr objFile, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
	return NULL;
}



// 向已打开的文件写入数据
XXAPI int xrtWrite(ptr objFile, str sText, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
	return FALSE;
}



// 从已打开的文件读取二进制数据
XXAPI ptr xrtGet(ptr objFile, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
	return NULL;
}



// 向已打开的文件写入二进制数据
XXAPI int xrtPut(ptr objFile, ptr pBuff, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
	return FALSE;
}



// 判断路径是否存在
XXAPI int xrtPathExists(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
	return FALSE;
}



// 判断文件是否存在
XXAPI int xrtFileExists(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
	return FALSE;
}



// 判断目录是否存在
XXAPI int xrtDirExists(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
	return FALSE;
}



// 获取文件长度
XXAPI size_t xrtFileGetSize(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
	return 0;
}



// 设置文件长度
XXAPI int xrtFileSetSize(str sPath, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
	return 0;
}



// 向文件追加写入数据
XXAPI int xrtFileAppend(str sPath, str sText, size_t iSize, int iCharset)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
	return 0;
}



// 写入并覆盖文件内容
XXAPI int xrtFileWriteAll(str sPath, str sText, size_t iSize, int iCharset)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
	return 0;
}



// 读取文件的全部内容
XXAPI int xrtFileReadAll(str sPath, int iCharset)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
	return 0;
}



// 写入并覆盖文件内容（二进制）
XXAPI int xrtFilePutAll(str sPath, ptr pBuff, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
	return 0;
}



// 读取文件的全部内容（二进制）
XXAPI int xrtFileGetAll(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
	return 0;
}



// 获取文件属性
XXAPI int xrtFileGetAttr(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
	return 0;
}



// 设置文件属性
XXAPI int xrtFileSetAttr(str sPath, int Attr)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
	return 0;
}



// 复制文件
XXAPI int xrtFileCopy(str sSrc, str sDst, int bReWrite)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
	return 0;
}



// 移动文件（重命名）
XXAPI int xrtFileMove(str sSrc, str sDst, int bReWrite)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
	return 0;
}



// 删除文件
XXAPI int xrtFileDelete(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
	return 0;
}



// 遍历文件夹
XXAPI int xrtDirScan(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
	return 0;
}



// 获取文件夹内容列表
XXAPI int xrtDirList(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
	return 0;
}



// 创建文件夹
XXAPI int xrtDirCreate(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
	return 0;
}



// 复制文件夹
XXAPI int xrtDirCopy(str sSrc, str sDst, int bReWrite)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
	return 0;
}



// 移动文件夹
XXAPI int xrtDirMove(str sSrc, str sDst, int bReWrite)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
	return 0;
}



// 删除文件夹
XXAPI int xrtDirDelete(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN64)
		// unix、linux 方案
	#else
		// crt 方案 - 暂未实现
	#endif
	return 0;
}


