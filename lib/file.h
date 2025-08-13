


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
		DWORD iRetSize;
		LARGE_INTEGER stuSize;
		GetFileSizeEx(hFile, &stuSize);
		objFile->obj = hFile;
		objFile->Size = stuSize.QuadPart;
		objFile->Charset = iCharset;
		objFile->BOM = 0;
		if ( iCharset == XRT_CP_AUTO ) {
			// 自动识别文件编码
			if ( stuSize.QuadPart == 0 ) {
				// 空文件默认使用 utf8 编码 ( 无 BOM )
				objFile->Charset = XRT_CP_UTF8;
			} else {
				// 最多读取 64KB 数据做编码推测（数据越多推测准确性越高，数据越少推测速度越快）
				size_t iReadSize = stuSize.QuadPart > 65536 ? 65536 : stuSize.QuadPart;
				str sText = xrtMalloc(iReadSize);
				if ( objFile == NULL ) {
					xrtSetError(xCore.ERROR_DESC.MALLOC, FALSE);
					CloseHandle(hFile);
					return NULL;
				}
				SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
				ReadFile(hFile, (ptr)sText, iReadSize, &iRetSize, NULL);
				objFile->Charset = xrtDetectCharset(sText, iReadSize, TRUE);
			}
		} else {
			// 手动指定编码时，检查 BOM 是否正确或提前写入 BOM
			if ( (iCharset & XRT_CP_BOM) == XRT_CP_BOM ) {
				int bErrorBOM = FALSE;
				if ( stuSize.QuadPart == 0 ) {
					// 0 字节文件自动添加 BOM ( 如果是只读模式直接报错 )
					if ( bReadOnly ) {
						bErrorBOM = TRUE;
					} else {
						if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF8 ) {
							uint8 sBOM[3] = { 0xEF, 0xBB, 0xBF };
							WriteFile(hFile, (ptr)sBOM, 3, &iRetSize, NULL);
						} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF16 ) {
							unsigned short iBOM = 0xFFFE;
							WriteFile(hFile, (ptr)&iBOM, 2, &iRetSize, NULL);
						} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF16_BE ) {
							unsigned short iBOM = 0xFEFF;
							WriteFile(hFile, (ptr)&iBOM, 2, &iRetSize, NULL);
						} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF32 ) {
							unsigned int iBOM = 0xFFFE0000;
							WriteFile(hFile, (ptr)&iBOM, 4, &iRetSize, NULL);
						} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF32_BE ) {
							unsigned int iBOM = 0x0000FEFF;
							WriteFile(hFile, (ptr)&iBOM, 4, &iRetSize, NULL);
						}
					}
				} else {
					// 固定编码的文件检测并跳过 BOM ( BOM 与检测结果不符直接报错 )
					uint8 sBOM[4] = { 0xA5, 0xA5, 0xA5, 0xA5 };
					SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
					ReadFile(hFile, (ptr)sBOM, 4, &iRetSize, NULL);
					if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF8 ) {
						if ( (sBOM[0] != 0xEF) || (sBOM[1] != 0xBB) || (sBOM[2] != 0xBF) ) {
							bErrorBOM = TRUE;
						}
					} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF16 ) {
						if ( (sBOM[0] != 0xFF) || (sBOM[1] != 0xFE) ) {
							bErrorBOM = TRUE;
						}
					} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF16_BE ) {
						if ( (sBOM[0] != 0xFE) || (sBOM[1] != 0xFF) ) {
							bErrorBOM = TRUE;
						}
					} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF32 ) {
						if ( (sBOM[0] != 0xFF) || (sBOM[1] != 0xFE) || (sBOM[2] != 0x00) || (sBOM[3] != 0x00) ) {
							bErrorBOM = TRUE;
						}
					} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF32_BE ) {
						if ( (sBOM[0] != 0x00) || (sBOM[1] != 0x00) || (sBOM[2] != 0xFE) || (sBOM[3] != 0xFF) ) {
							bErrorBOM = TRUE;
						}
					}
				}
				if ( bErrorBOM ) {
					xrtSetError("Incorrect BOM data !", FALSE);
					CloseHandle(hFile);
					return NULL;
				}
			}
		}
		// 计算 BOM 长度 ( 处理到这个步骤可以确保 BOM 信息是正确的 )
		if ( (iCharset & XRT_CP_BOM) == XRT_CP_BOM ) {
			if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF8 ) {
				objFile->BOM = 3;
			} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF16 ) {
				objFile->BOM = 2;
			} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF16_BE ) {
				objFile->BOM = 2;
			} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF32 ) {
				objFile->BOM = 4;
			} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF32_BE ) {
				objFile->BOM = 4;
			}
		}
		// 游标跳过 BOM
		SetFilePointer(hFile, objFile->BOM, NULL, FILE_BEGIN);
		return objFile;
	#else
		// 其他平台方案
		
	#endif
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


