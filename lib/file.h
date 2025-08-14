


// 打开文件
XXAPI xfile xrtOpen(str sPath, int bReadOnly, int iCharset)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		wstr sPathW = xrtUTF8to16(sPath, 0);
		xfile objFile = xrtOpenW(sPathW, bReadOnly, iCharset);
		xrtFree(sPathW);
		return objFile;
	#else
		// 其他平台方案
		
	#endif
}
XXAPI xfile xrtOpenW(wstr sPath, int bReadOnly, int iCharset)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		HANDLE hFile = CreateFileW(sPath, bReadOnly ? GENERIC_READ : GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, bReadOnly ? OPEN_EXISTING : OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
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
				objFile->Charset = XRT_CP_UTF8;
			} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF16 ) {
				objFile->BOM = 2;
				objFile->Charset = XRT_CP_UTF16;
			} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF16_BE ) {
				objFile->BOM = 2;
				objFile->Charset = XRT_CP_UTF16_BE;
			} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF32 ) {
				objFile->BOM = 4;
				objFile->Charset = XRT_CP_UTF32;
			} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF32_BE ) {
				objFile->BOM = 4;
				objFile->Charset = XRT_CP_UTF32_BE;
			}
		}
		// 游标跳过 BOM
		SetFilePointer(hFile, objFile->BOM, NULL, FILE_BEGIN);
		return objFile;
	#else
		// 其他平台方案
		str sConvPath = xrtUTF16to8(sPath, 0);
		xfile objFile = xrtOpenW(sConvPath, bReadOnly, iCharset);
		xrtFree(sConvPath);
		return objFile;
	#endif
}



// 关闭文件
XXAPI int xrtClose(xfile objFile)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile ) {
			if ( objFile->obj != INVALID_HANDLE_VALUE ) {
				CloseHandle(objFile->obj);
				objFile->obj = NULL;
			}
			free(objFile);
			return TRUE;
		}
		return FALSE;
	#else
		// 其他平台方案
	#endif
}



// 设置游标位置
static const str sErrorFile_Handle = "Incorrect file handle !";
XXAPI size_t xrtSeek(xfile objFile, long iOffset, int iMoveMethod)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile && (objFile->obj != INVALID_HANDLE_VALUE) ) {
			LARGE_INTEGER iPos_stu;
			iPos_stu.QuadPart = iOffset;
			if ( iMoveMethod == XRT_SEEK_BEGIN ) {
				SetFilePointerEx(objFile->obj, iPos_stu, NULL, FILE_BEGIN);
				return iPos_stu.QuadPart;
			} else if ( iMoveMethod == XRT_SEEK_CUR ) {
				SetFilePointerEx(objFile->obj, iPos_stu, NULL, FILE_CURRENT);
				return iPos_stu.QuadPart;
			} else if ( iMoveMethod == XRT_SEEK_END ) {
				SetFilePointerEx(objFile->obj, iPos_stu, NULL, FILE_END);
				return iPos_stu.QuadPart;
			} else {
				xrtSetError("Incorrect seek method !", FALSE);
				return 0;
			}
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return 0;
		}
	#else
		// 其他平台方案
	#endif
}



// 获取游标位置
XXAPI size_t xrtTell(xfile objFile)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile && (objFile->obj != INVALID_HANDLE_VALUE) ) {
			LARGE_INTEGER iPos_stu;
			iPos_stu.QuadPart = 0;
			SetFilePointerEx(objFile->obj, iPos_stu, &iPos_stu, FILE_CURRENT);
			return iPos_stu.QuadPart;
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return 0;
		}
	#else
		// 其他平台方案
	#endif
}



// 获取文件末尾位置 ( 获取一打开文件的动态大小 )
XXAPI size_t xrtGetEOF(xfile objFile)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile && (objFile->obj != INVALID_HANDLE_VALUE) ) {
			size_t iPos = xrtTell(objFile);
			xrtSeek(objFile, 0, XRT_SEEK_END);
			size_t iSize = xrtTell(objFile);
			xrtSeek(objFile, iPos, XRT_SEEK_BEGIN);
			return iSize;
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return 0;
		}
	#else
		// 其他平台方案
	#endif
}



// 是否已经读取到文件末尾
XXAPI int xrtEOF(xfile objFile)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile && (objFile->obj != INVALID_HANDLE_VALUE) ) {
			size_t iPos = xrtTell(objFile);
			xrtSeek(objFile, 0, XRT_SEEK_END);
			size_t iEnd = xrtTell(objFile);
			xrtSeek(objFile, iPos, XRT_SEEK_BEGIN);
			return (iPos >= iEnd);
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return TRUE;
		}
	#else
		// 其他平台方案
	#endif
}



// 设置文件末尾
XXAPI int xrtSetEOF(xfile objFile)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile && (objFile->obj != INVALID_HANDLE_VALUE) ) {
			SetEndOfFile(objFile->obj);
			return TRUE;
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return FALSE;
		}
	#else
		// 其他平台方案
		// ftruncate
	#endif
}



// 从已打开的文件读取数据
static const str sErrorFile_Read = "File read failure !";
XXAPI str xrtRead(xfile objFile, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile && (objFile->obj != INVALID_HANDLE_VALUE) ) {
			if ( iSize == 0 ) { xCore.iRet = 0; return xCore.sNull; }
			str sBuff = xrtMalloc(iSize + 4);
			if ( sBuff == NULL ) {
				xrtSetError(xCore.ERROR_DESC.MALLOC, FALSE);
				xCore.iRet = 0;
				return xCore.sNull;
			}
			// 读取数据
			DWORD iRetSize;
			if ( ReadFile(objFile->obj, sBuff, iSize, &iRetSize, NULL) ) {
				// 读取到的文件可能是 utf-32 编码，留 4 个空字节做结尾
				sBuff[iRetSize] = 0;
				sBuff[iRetSize + 1] = 0;
				sBuff[iRetSize + 2] = 0;
				sBuff[iRetSize + 3] = 0;
				// 转换编码 (将读取到的数据转换为 utf-8 编码)
				if ( (objFile->Charset >= 0) && (objFile->Charset != XRT_CP_UTF8) ) {
					str sRet = xrtConvCharset(sBuff, iRetSize, objFile->Charset, XRT_CP_UTF8);
					xrtFree(sBuff);
					return sRet;
				} else {
					xCore.iRet = iRetSize;
					return sBuff;
				}
			} else {
				xrtSetError(sErrorFile_Read, FALSE);
				xrtFree(sBuff);
				xCore.iRet = 0;
				return xCore.sNull;
			}
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			xCore.iRet = 0;
			return xCore.sNull;
		}
	#else
		// 其他平台方案
	#endif
	return NULL;
}



// 向已打开的文件写入数据
static const str sErrorFile_Write = "File write failure !";
XXAPI size_t xrtWrite(xfile objFile, str sText, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile && (objFile->obj != INVALID_HANDLE_VALUE) ) {
			if ( sText == NULL ) { return 0; }
			if ( iSize == 0 ) { iSize = strlen(sText); }
			if ( iSize == 0 ) { return 0; }
			DWORD iRetSize;
			if ( (objFile->Charset >= 0) && (objFile->Charset != XRT_CP_UTF8) ) {
				// 需要转换为目标文件的编码再写入
				str sBuff = xrtConvCharset(sText, iSize, XRT_CP_UTF8, objFile->Charset);
				if ( WriteFile(objFile->obj, sBuff, xCore.iRet, &iRetSize, NULL) ) {
					return iRetSize;
				} else {
					xrtSetError(sErrorFile_Write, FALSE);
					return 0;
				}
			} else {
				// 二进制或 utf-8 编码可以直接写入
				if ( WriteFile(objFile->obj, sText, iSize, &iRetSize, NULL) ) {
					return iRetSize;
				} else {
					xrtSetError(sErrorFile_Write, FALSE);
					return 0;
				}
			}
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return 0;
		}
	#else
		// 其他平台方案
	#endif
}



// 从已打开的文件读取二进制数据
XXAPI ptr xrtGet(xfile objFile, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile && (objFile->obj != INVALID_HANDLE_VALUE) ) {
			if ( iSize == 0 ) { xCore.iRet = 0; return xCore.sNull; }
			str sBuff = xrtMalloc(iSize + 4);
			if ( sBuff == NULL ) {
				xrtSetError(xCore.ERROR_DESC.MALLOC, FALSE);
				xCore.iRet = 0;
				return xCore.sNull;
			}
			// 读取数据
			DWORD iRetSize;
			if ( ReadFile(objFile->obj, sBuff, iSize, &iRetSize, NULL) ) {
				// 读取到的文件可能是 utf-32 编码，留 4 个空字节做结尾
				sBuff[iRetSize] = 0;
				sBuff[iRetSize + 1] = 0;
				sBuff[iRetSize + 2] = 0;
				sBuff[iRetSize + 3] = 0;
				xCore.iRet = iRetSize;
				return sBuff;
			} else {
				xrtSetError(sErrorFile_Read, FALSE);
				xrtFree(sBuff);
				xCore.iRet = 0;
				return xCore.sNull;
			}
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return 0;
		}
	#else
		// 其他平台方案
	#endif
}



// 向已打开的文件写入二进制数据
XXAPI int xrtPut(xfile objFile, ptr pBuff, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile && (objFile->obj != INVALID_HANDLE_VALUE) ) {
			if ( pBuff == NULL ) { return 0; }
			if ( iSize == 0 ) { iSize = strlen(pBuff); }
			if ( iSize == 0 ) { return 0; }
			DWORD iRetSize;
			if ( WriteFile(objFile->obj, pBuff, iSize, &iRetSize, NULL) ) {
				return iRetSize;
			} else {
				xrtSetError(sErrorFile_Write, FALSE);
				return 0;
			}
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return 0;
		}
	#else
		// 其他平台方案
	#endif
}



// 判断路径是否存在
XXAPI int xrtPathExists(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		wstr sPathW = xrtUTF8to16(sPath, 0);
		int bRet = GetFileAttributesW(sPathW) != INVALID_FILE_ATTRIBUTES;
		xrtFree(sPathW);
		return bRet;
	#else
		// 其他平台方案
	#endif
}
XXAPI int xrtPathExistsW(wstr sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		return GetFileAttributesW(sPath) != INVALID_FILE_ATTRIBUTES;
	#else
		// 其他平台方案
	#endif
}



// 判断文件是否存在
XXAPI int xrtFileExists(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		wstr sPathW = xrtUTF8to16(sPath, 0);
		WIN32_FILE_ATTRIBUTE_DATA wfad = { 0 };
		DWORD iRet = GetFileAttributesExW(sPathW, GetFileExInfoStandard, &wfad);
		xrtFree(sPathW);
		if ( iRet == 0 ) { return FALSE; }
		if ( wfad.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE ) {
			return TRUE;
		} else {
			return FALSE;
		}
	#else
		// 其他平台方案
	#endif
}
XXAPI int xrtFileExistsW(wstr sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		WIN32_FILE_ATTRIBUTE_DATA wfad = { 0 };
		DWORD iRet = GetFileAttributesExW(sPath, GetFileExInfoStandard, &wfad);
		if ( iRet == 0 ) { return FALSE; }
		if ( wfad.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE ) {
			return TRUE;
		} else {
			return FALSE;
		}
	#else
		// 其他平台方案
	#endif
}



// 判断目录是否存在
XXAPI int xrtDirExists(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		wstr sPathW = xrtUTF8to16(sPath, 0);
		WIN32_FILE_ATTRIBUTE_DATA wfad = { 0 };
		DWORD iRet = GetFileAttributesExW(sPathW, GetFileExInfoStandard, &wfad);
		xrtFree(sPathW);
		if ( iRet == 0 ) { return FALSE; }
		if ( wfad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) {
			return TRUE;
		} else {
			return FALSE;
		}
	#else
		// 其他平台方案
	#endif
}
XXAPI int xrtDirExistsW(wstr sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		WIN32_FILE_ATTRIBUTE_DATA wfad = { 0 };
		DWORD iRet = GetFileAttributesExW(sPath, GetFileExInfoStandard, &wfad);
		if ( iRet == 0 ) { return FALSE; }
		if ( wfad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) {
			return TRUE;
		} else {
			return FALSE;
		}
	#else
		// 其他平台方案
	#endif
}



// 获取文件长度
XXAPI size_t xrtFileGetSize(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		wstr sPathW = xrtUTF8to16(sPath, 0);
		HANDLE hFile = CreateFileW(sPathW, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		xrtFree(sPathW);
		if ( hFile && hFile != INVALID_HANDLE_VALUE ) {
			LARGE_INTEGER iSize;
			GetFileSizeEx(hFile, &iSize);
			CloseHandle(hFile);
			return iSize.QuadPart;
		}
		return 0;
	#else
		// 其他平台方案
	#endif
}



// 设置文件长度
XXAPI int xrtFileSetSize(str sPath, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		wstr sPathW = xrtUTF8to16(sPath, 0);
		HANDLE hFile = CreateFileW(sPathW, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		xrtFree(sPathW);
		if ( hFile && hFile != INVALID_HANDLE_VALUE ) {
			LARGE_INTEGER iPos_stu;
			iPos_stu.QuadPart = iSize;
			SetFilePointerEx(hFile, iPos_stu, NULL, FILE_BEGIN);
			SetEndOfFile(hFile);
			CloseHandle(hFile);
			return TRUE;
		}
		return FALSE;
	#else
		// 其他平台方案
	#endif
}



// 向文件追加写入数据
XXAPI int xrtFileAppend(str sPath, str sText, size_t iSize, int iCharset)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( sText == NULL ) { return 0; }
		xfile objFile = xrtOpen(sPath, FALSE, iCharset);
		if ( objFile ) {
			if ( iSize == 0 ) { iSize = strlen(sText); }
			if ( iSize == 0 ) { xrtClose(objFile); return 0; }
			xrtSeek(objFile, 0, FILE_END);
			ulong iRet = xrtWrite(objFile, sText, iSize);
			xrtClose(objFile);
			return iRet;
		}
		return 0;
	#else
		// 其他平台方案
	#endif
}



// 写入并覆盖文件内容
XXAPI int xrtFileWriteAll(str sPath, str sText, size_t iSize, int iCharset)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( sText == NULL ) { return 0; }
		xfile objFile = xrtOpen(sPath, FALSE, iCharset);
		if ( objFile ) {
			if ( iSize == 0 ) { iSize = strlen(sText); }
			if ( iSize == 0 ) { return 0; }
			ulong iRet = xrtWrite(objFile, sText, iSize);
			xrtSetEOF(objFile);
			xrtClose(objFile);
			return iRet;
		}
		return 0;
	#else
		// 其他平台方案
	#endif
}



// 读取文件的全部内容
XXAPI str xrtFileReadAll(str sPath, int iCharset)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		xfile objFile = xrtOpen(sPath, TRUE, iCharset);
		if ( objFile ) {
			uint64 iSize = objFile->Size - objFile->BOM;
			str sRet = xCore.sNull;
			if ( iSize > 0 ) { sRet = xrtRead(objFile, iSize - objFile->BOM); }
			xrtClose(objFile);
			return sRet;
		}
		xCore.iRet = 0;
		return xCore.sNull;
	#else
		// 其他平台方案
	#endif
}



// 写入并覆盖文件内容（二进制）
XXAPI int xrtFilePutAll(str sPath, ptr pBuff, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		wstr sPathW = xrtUTF8to16(sPath, 0);
		HANDLE hFile = CreateFileW(sPathW, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		xrtFree(sPathW);
		if ( hFile == INVALID_HANDLE_VALUE ) { return 0; }
		SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
		DWORD iRetSize;
		WriteFile(hFile, pBuff, iSize, &iRetSize, NULL);
		SetEndOfFile(hFile);
		CloseHandle(hFile);
		return iRetSize;
	#else
		// 其他平台方案
	#endif
	return 0;
}



// 读取文件的全部内容（二进制）
XXAPI ptr xrtFileGetAll(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		wstr sPathW = xrtUTF8to16(sPath, 0);
		HANDLE hFile = CreateFileW(sPathW, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		xrtFree(sPathW);
		if ( hFile == INVALID_HANDLE_VALUE ) { xCore.iRet = 0; return xCore.sNull; }
		SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
		DWORD iRetSize = GetFileSize(hFile, NULL);
		ptr pBuff = xrtMalloc(iRetSize + 1);
		if ( pBuff == NULL ) {
			xrtSetError(xCore.ERROR_DESC.MALLOC, FALSE);
			CloseHandle(hFile);
			xCore.iRet = 0;
			return xCore.sNull;
		}
		ReadFile(hFile, pBuff, iRetSize, &iRetSize, NULL);
		CloseHandle(hFile);
		xCore.iRet = iRetSize;
		return pBuff;
	#else
		// 其他平台方案
	#endif
	return 0;
}



// 获取文件属性
XXAPI int xrtFileGetAttr(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		wstr sPathW = xrtUTF8to16(sPath, 0);
		int iRet = GetFileAttributesW(sPathW);
		xrtFree(sPathW);
		return iRet;
	#else
		// 其他平台方案
	#endif
	return 0;
}



// 设置文件属性
XXAPI int xrtFileSetAttr(str sPath, int iAttr)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		wstr sPathW = xrtUTF8to16(sPath, 0);
		int iRet = SetFileAttributesW(sPathW, iAttr);
		xrtFree(sPathW);
		return iRet;
	#else
		// 其他平台方案
	#endif
	return 0;
}



// 复制文件
XXAPI int xrtFileCopy(str sSrc, str sDst, int bReWrite)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		wstr sSrcW = xrtUTF8to16(sSrc, 0);
		wstr sDstW = xrtUTF8to16(sDst, 0);
		int iRet = CopyFileW(sSrcW, sDstW, bReWrite ? FALSE : TRUE);
		xrtFree(sSrcW);
		xrtFree(sDstW);
		return iRet;
	#else
		// 其他平台方案
	#endif
	return 0;
}



// 移动文件（重命名）
XXAPI int xrtFileMove(str sSrc, str sDst, int bReWrite)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		wstr sSrcW = xrtUTF8to16(sSrc, 0);
		wstr sDstW = xrtUTF8to16(sDst, 0);
		// 如果源文件和目标文件都存在，并且 bReWrite 为 TRUE，将目标文件删除
		int bExistSrc = xrtFileExistsW(sSrcW);
		int bExistDst = xrtFileExistsW(sDstW);
		if ( bExistSrc && bExistDst && bReWrite ) {
			DeleteFileW(sDstW);
		}
		// 移动文件
		int iRet = MoveFileW(sSrcW, sDstW);
		if ( iRet == FALSE ) {
			// MoveFile 不支持跨盘移动，因此移动失败后，尝试复制再删除
			iRet = CopyFileW(sSrcW, sDstW, bReWrite ? FALSE : TRUE);
			if ( iRet ) {
				iRet = DeleteFileW(sSrcW);
			}
		}
		xrtFree(sSrcW);
		xrtFree(sDstW);
		return iRet;
	#else
		// 其他平台方案
	#endif
	return 0;
}



// 删除文件
XXAPI int xrtFileDelete(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		wstr sPathW = xrtUTF8to16(sPath, 0);
		int iRet = DeleteFileW(sPathW);
		xrtFree(sPathW);
		return iRet;
	#else
		// 其他平台方案
	#endif
	return 0;
}



// 遍历文件夹
XXAPI int xrtDirScan(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#else
		// 其他平台方案
	#endif
	return 0;
}



// 获取文件夹内容列表
XXAPI int xrtDirList(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#else
		// 其他平台方案
	#endif
	return 0;
}



// 创建文件夹
XXAPI int xrtDirCreate(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#else
		// 其他平台方案
	#endif
	return 0;
}



// 复制文件夹
XXAPI int xrtDirCopy(str sSrc, str sDst, int bReWrite)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#else
		// 其他平台方案
	#endif
	return 0;
}



// 移动文件夹
XXAPI int xrtDirMove(str sSrc, str sDst, int bReWrite)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#else
		// 其他平台方案
	#endif
	return 0;
}



// 删除文件夹
XXAPI int xrtDirDelete(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
	#else
		// 其他平台方案
	#endif
	return 0;
}


