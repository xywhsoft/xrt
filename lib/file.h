


// 错误描述定义
static const str sErrorFile_Open = (str)"Failed to open file !";
static const str sErrorFile_OpenDir = (str)"Failed to open dir !";
static const str sErrorFile_Handle = (str)"Incorrect file handle !";
static const str sErrorFile_BOM = (str)"Incorrect BOM data !";
static const str sErrorFile_Seek = (str)"Incorrect seek method !";
static const str sErrorFile_Read = (str)"File read failure !";
static const str sErrorFile_Write = (str)"File write failure !";




// 打开文件
XXAPI xfile xrtOpen(str sPath, int bReadOnly, int iCharset)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		u16str sPathW = xrtUTF8to16(sPath, 0, NULL);
		HANDLE hFile = CreateFileW(sPathW, bReadOnly ? GENERIC_READ : GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, bReadOnly ? OPEN_EXISTING : OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		xrtFree(sPathW);
		if ( hFile == INVALID_HANDLE_VALUE ) {
			xrtSetError(sErrorFile_Open, FALSE);
			return NULL;
		}
		xfile objFile = xrtMalloc(sizeof(xfile_struct));
		if ( objFile == NULL ) {
			CloseHandle(hFile);
			return NULL;
		}
		DWORD iRetSize;
		LARGE_INTEGER stuSize;
		GetFileSizeEx(hFile, &stuSize);
		objFile->obj = hFile;
		objFile->Charset = iCharset;
		objFile->BOM = 0;
		objFile->ReadOnly = bReadOnly;
		SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
		if ( iCharset == XRT_CP_AUTO ) {
			// 自动识别文件编码
			if ( stuSize.QuadPart == 0 ) {
				// 空文件默认使用 utf8 编码 （ 无 BOM ）
				objFile->Charset = XRT_CP_UTF8;
			} else {
				// 最多读取 64KB 数据做编码推测（数据越多推测准确性越高，数据越少推测速度越快）
				size_t iReadSize = stuSize.QuadPart > 65536 ? 65536 : stuSize.QuadPart;
				str sText = xrtMalloc(iReadSize);
				if ( sText == NULL ) {
					CloseHandle(hFile);
					xrtFree(objFile);
					return NULL;
				}
				ReadFile(hFile, (ptr)sText, iReadSize, &iRetSize, NULL);
				objFile->Charset = xrtDetectCharset(sText, iReadSize, TRUE);
				xrtFree(sText);
				// 重置文件指针到开头
				SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
			}
		} else {
			// 手动指定编码时，检查 BOM 是否正确或提前写入 BOM
			if ( iCharset == XRT_CP_BINARY ) {
				// BINARY 模式不处理 BOM，确保文件指针在开头
				objFile->BOM = 0;
				SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
			} else if ( (iCharset > 0) && ((iCharset & XRT_CP_BOM) == XRT_CP_BOM) ) {
				int bErrorBOM = FALSE;
				if ( stuSize.QuadPart == 0 ) {
					// 0 字节文件自动添加 BOM （ 如果是只读模式直接报错 ）
					if ( bReadOnly ) {
						bErrorBOM = TRUE;
					} else {
						if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF8 ) {
							uint8 sBOM[3] = { 0xEF, 0xBB, 0xBF };
							WriteFile(hFile, (ptr)sBOM, 3, &iRetSize, NULL);
						} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF16 ) {
							unsigned short iBOM = 0xFEFF;
							WriteFile(hFile, (ptr)&iBOM, 2, &iRetSize, NULL);
						} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF16_BE ) {
							unsigned short iBOM = 0xFFFE;
							WriteFile(hFile, (ptr)&iBOM, 2, &iRetSize, NULL);
						} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF32 ) {
							unsigned int iBOM = 0x0000FEFF;
							WriteFile(hFile, (ptr)&iBOM, 4, &iRetSize, NULL);
						} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF32_BE ) {
							unsigned int iBOM = 0xFFFE0000;
							WriteFile(hFile, (ptr)&iBOM, 4, &iRetSize, NULL);
						}
					}
				} else {
					// 固定编码的文件检测并跳过 BOM （ BOM 与检测结果不符直接报错 ）
					uint8 sBOM[4] = { 0xA5, 0xA5, 0xA5, 0xA5 };
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
					xrtSetError(sErrorFile_BOM, FALSE);
					CloseHandle(hFile);
					xrtFree(objFile);
					return NULL;
				}
			}
		}
		// 计算 BOM 长度 （ 处理到这个步骤可以确保 BOM 信息是正确的 ）
		if ( (objFile->Charset > 0) && ((objFile->Charset & XRT_CP_BOM) == XRT_CP_BOM) ) {
			if ( (objFile->Charset & XRT_MASK_BOM) == XRT_CP_UTF8 ) {
				objFile->BOM = 3;
				objFile->Charset = XRT_CP_UTF8;
			} else if ( (objFile->Charset & XRT_MASK_BOM) == XRT_CP_UTF16 ) {
				objFile->BOM = 2;
				objFile->Charset = XRT_CP_UTF16;
			} else if ( (objFile->Charset & XRT_MASK_BOM) == XRT_CP_UTF16_BE ) {
				objFile->BOM = 2;
				objFile->Charset = XRT_CP_UTF16_BE;
			} else if ( (objFile->Charset & XRT_MASK_BOM) == XRT_CP_UTF32 ) {
				objFile->BOM = 4;
				objFile->Charset = XRT_CP_UTF32;
			} else if ( (objFile->Charset & XRT_MASK_BOM) == XRT_CP_UTF32_BE ) {
				objFile->BOM = 4;
				objFile->Charset = XRT_CP_UTF32_BE;
			}
		}
		// 游标跳过 BOM （ BINARY 模式已经在前面重置过文件指针 ）
		if (objFile->Charset != XRT_CP_BINARY) {
			SetFilePointer(hFile, objFile->BOM, NULL, FILE_BEGIN);
		}
		return objFile;
	#else
		// 其他平台方案
		int fd = open(sPath, bReadOnly ? O_RDONLY : (O_RDWR | O_CREAT), 0644);
		if ( fd == -1 ) {
			xrtSetError(sErrorFile_Open, FALSE);
			return NULL;
		}
		xfile objFile = xrtMalloc(sizeof(xfile_struct));
		if ( objFile == NULL ) {
			close(fd);
			return NULL;
		}
		size_t iRetSize;
		struct stat fileStat;
		fstat(fd, &fileStat);
		objFile->idx = fd;
		objFile->Charset = iCharset;
		objFile->BOM = 0;
		objFile->ReadOnly = bReadOnly;
		lseek(fd, 0, SEEK_SET);
		if ( iCharset == XRT_CP_AUTO ) {
			// 自动识别文件编码
			if ( fileStat.st_size == 0 ) {
				// 空文件默认使用 utf8 编码 （ 无 BOM ）
				objFile->Charset = XRT_CP_UTF8;
			} else {
				// 最多读取 64KB 数据做编码推测（数据越多推测准确性越高，数据越少推测速度越快）
				size_t iReadSize = fileStat.st_size > 65536 ? 65536 : fileStat.st_size;
				str sText = xrtMalloc(iReadSize);
				if ( sText == NULL ) {
					close(fd);
					xrtFree(objFile);
					return NULL;
				}
				read(fd, sText, iReadSize);
				objFile->Charset = xrtDetectCharset(sText, iReadSize, TRUE);
				xrtFree(sText);
				// 重置文件指针到开头
				lseek(fd, 0, SEEK_SET);
			}
		} else {
			// 手动指定编码时，检查 BOM 是否正确或提前写入 BOM
			if ( iCharset == XRT_CP_BINARY ) {
				// BINARY 模式不处理 BOM，确保文件指针在开头
				objFile->BOM = 0;
				lseek(fd, 0, SEEK_SET);
			} else if ( (iCharset > 0) && ((iCharset & XRT_CP_BOM) == XRT_CP_BOM) ) {
				int bErrorBOM = FALSE;
				if ( fileStat.st_size == 0 ) {
					// 0 字节文件自动添加 BOM （ 如果是只读模式直接报错 ）
					if ( bReadOnly ) {
						bErrorBOM = TRUE;
					} else {
						if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF8 ) {
							uint8 sBOM[3] = { 0xEF, 0xBB, 0xBF };
							write(fd, (ptr)sBOM, 3);
						} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF16 ) {
							unsigned short iBOM = 0xFEFF;
							write(fd, (ptr)&iBOM, 2);
						} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF16_BE ) {
							unsigned short iBOM = 0xFFFE;
							write(fd, (ptr)&iBOM, 2);
						} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF32 ) {
							unsigned int iBOM = 0x0000FEFF;
							write(fd, (ptr)&iBOM, 4);
						} else if ( (iCharset & XRT_MASK_BOM) == XRT_CP_UTF32_BE ) {
							unsigned int iBOM = 0xFFFE0000;
							write(fd, (ptr)&iBOM, 4);
						}
					}
				} else {
					// 固定编码的文件检测并跳过 BOM （ BOM 与检测结果不符直接报错 ）
					uint8 sBOM[4] = { 0xA5, 0xA5, 0xA5, 0xA5 };
					read(fd, (ptr)sBOM, 4);
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
					xrtSetError(sErrorFile_BOM, FALSE);
					close(fd);
					xrtFree(objFile);
					return NULL;
				}
			}
		}
		// 计算 BOM 长度 （ 处理到这个步骤可以确保 BOM 信息是正确的 ）
		if ( (objFile->Charset > 0) && ((objFile->Charset & XRT_CP_BOM) == XRT_CP_BOM) ) {
			if ( (objFile->Charset & XRT_MASK_BOM) == XRT_CP_UTF8 ) {
				objFile->BOM = 3;
				objFile->Charset = XRT_CP_UTF8;
			} else if ( (objFile->Charset & XRT_MASK_BOM) == XRT_CP_UTF16 ) {
				objFile->BOM = 2;
				objFile->Charset = XRT_CP_UTF16;
			} else if ( (objFile->Charset & XRT_MASK_BOM) == XRT_CP_UTF16_BE ) {
				objFile->BOM = 2;
				objFile->Charset = XRT_CP_UTF16_BE;
			} else if ( (objFile->Charset & XRT_MASK_BOM) == XRT_CP_UTF32 ) {
				objFile->BOM = 4;
				objFile->Charset = XRT_CP_UTF32;
			} else if ( (objFile->Charset & XRT_MASK_BOM) == XRT_CP_UTF32_BE ) {
				objFile->BOM = 4;
				objFile->Charset = XRT_CP_UTF32_BE;
			}
		}
		// 游标跳过 BOM （ BINARY 模式已经在前面重置过文件指针 ）
		if (objFile->Charset != XRT_CP_BINARY) {
			lseek(fd, objFile->BOM, SEEK_SET);
		}
		return objFile;
	#endif
}



// 关闭文件
XXAPI void xrtClose(xfile objFile)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile ) {
			if ( objFile->obj != INVALID_HANDLE_VALUE ) {
				// 非只读模式刷新缓冲区，确保数据写入磁盘
				if ( !objFile->ReadOnly ) {
					FlushFileBuffers(objFile->obj);
				}
				// 关闭文件句柄
				CloseHandle(objFile->obj);
				objFile->obj = NULL;
			}
			// 释放文件对象内存
			xrtFree(objFile);
		}
	#else
		// 其他平台方案
		if ( objFile ) {
			if ( objFile->idx != -1 ) {
				// 非只读模式刷新缓冲区，确保数据写入磁盘
				if ( !objFile->ReadOnly ) {
					fsync(objFile->idx);
				}
				// 关闭文件描述符
				close(objFile->idx);
				objFile->idx = 0;
			}
			// 释放文件对象内存
			xrtFree(objFile);
		}
	#endif
}



// 设置游标位置
XXAPI size_t xrtSeek(xfile objFile, int64 iOffset, int iMoveMethod)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile && (objFile->obj != INVALID_HANDLE_VALUE) ) {
			// 将偏移量转换为 LARGE_INTEGER 结构
			LARGE_INTEGER iOffsetStu;
			LARGE_INTEGER iPosStu;
			iOffsetStu.QuadPart = iOffset;
			// 根据定位方式移动文件指针
			if ( iMoveMethod == XRT_SEEK_SET ) {
				if ( SetFilePointerEx(objFile->obj, iOffsetStu, &iPosStu, FILE_BEGIN) ) {
					return iPosStu.QuadPart;
				}
				xrtSetError(sErrorFile_Seek, FALSE);
				return 0;
			} else if ( iMoveMethod == XRT_SEEK_CUR ) {
				if ( SetFilePointerEx(objFile->obj, iOffsetStu, &iPosStu, FILE_CURRENT) ) {
					return iPosStu.QuadPart;
				}
				xrtSetError(sErrorFile_Seek, FALSE);
				return 0;
			} else if ( iMoveMethod == XRT_SEEK_END ) {
				if ( SetFilePointerEx(objFile->obj, iOffsetStu, &iPosStu, FILE_END) ) {
					return iPosStu.QuadPart;
				}
				xrtSetError(sErrorFile_Seek, FALSE);
				return 0;
			} else {
				xrtSetError(sErrorFile_Seek, FALSE);
				return 0;
			}
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return 0;
		}
	#else
		// 其他平台方案
		if ( objFile && (objFile->idx != -1) ) {
			// 根据定位方式移动文件指针
			if ( iMoveMethod == XRT_SEEK_SET ) {
				return lseek(objFile->idx, iOffset, SEEK_SET);
			} else if ( iMoveMethod == XRT_SEEK_CUR ) {
				return lseek(objFile->idx, iOffset, SEEK_CUR);
			} else if ( iMoveMethod == XRT_SEEK_END ) {
				return lseek(objFile->idx, iOffset, SEEK_END);
			} else {
				xrtSetError(sErrorFile_Seek, FALSE);
				return 0;
			}
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return 0;
		}
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
		if ( objFile && (objFile->idx != -1) ) {
			return lseek(objFile->idx, 0, SEEK_CUR);
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return 0;
		}
	#endif
}



// 获取文件末尾位置 （ 获取一打开文件的动态大小 ）
XXAPI size_t xrtGetEOF(xfile objFile)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile && (objFile->obj != INVALID_HANDLE_VALUE) ) {
			LARGE_INTEGER stuSize;
			GetFileSizeEx(objFile->obj, &stuSize);
			return stuSize.QuadPart;
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return 0;
		}
	#else
		// 其他平台方案
		if ( objFile && (objFile->idx != -1) ) {
			struct stat fileStat;
			fstat(objFile->idx, &fileStat);
			return fileStat.st_size;
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return 0;
		}
	#endif
}



// 是否已经读取到文件末尾
XXAPI bool xrtEOF(xfile objFile)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile && (objFile->obj != INVALID_HANDLE_VALUE) ) {
			size_t iPos = xrtTell(objFile);
			size_t iEnd = xrtGetEOF(objFile);
			return (iPos >= iEnd);
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return TRUE;
		}
	#else
		// 其他平台方案
		if ( objFile && (objFile->idx != -1) ) {
			size_t iPos = xrtTell(objFile);
			size_t iEnd = xrtGetEOF(objFile);
			return (iPos >= iEnd);
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return TRUE;
		}
	#endif
}



// 设置文件末尾
XXAPI bool xrtSetEOF(xfile objFile)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile && (objFile->obj != INVALID_HANDLE_VALUE) ) {
			// 将文件末尾截断到当前游标位置
			SetEndOfFile(objFile->obj);
			return TRUE;
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return FALSE;
		}
	#else
		// 其他平台方案
		if ( objFile && (objFile->idx != -1) ) {
			// 获取当前游标位置并将文件截断到该位置
			size_t iPos = xrtTell(objFile);
			int iRet = ftruncate(objFile->idx, iPos);
			if ( iRet == 0 ) {
				return TRUE;
			} else {
				return FALSE;
			}
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return FALSE;
		}
	#endif
}



// 从已打开的文件读取数据 （ iSize 为要读取的字节数，需要使用 xrtFree 释放内存 ）
XXAPI str xrtRead(xfile objFile, size_t iSize, size_t* iRetSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile && (objFile->obj != INVALID_HANDLE_VALUE) ) {
			if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
			str sBuff = xrtMalloc(iSize + 4);
			if ( sBuff == NULL ) {
				if ( iRetSize ) { *iRetSize = 0; }
				return xCore.sNull;
			}
			// 读取数据
			DWORD iRet;
			if ( ReadFile(objFile->obj, sBuff, iSize, &iRet, NULL) ) {
				// 读取到的文件可能是 utf-32 编码，留 4 个空字节做结尾
				sBuff[iRet] = 0;
				sBuff[iRet + 1] = 0;
				sBuff[iRet + 2] = 0;
				sBuff[iRet + 3] = 0;
				// 转换编码 （ 将读取到的数据转换为 utf-8 编码 ）
				if ( (objFile->Charset >= 0) && (objFile->Charset != XRT_CP_UTF8) ) {
					int iCharSize = xrtGetCharSize(objFile->Charset);
					size_t iTextSize = 0;
					str sRet = xrtConvCharset(sBuff, iRet / iCharSize, objFile->Charset, XRT_CP_UTF8, &iTextSize);
					xrtFree(sBuff);
					if ( iRetSize ) { *iRetSize = iTextSize; }
					return sRet;
				} else {
					if ( iRetSize ) { *iRetSize = iRet; }
					return sBuff;
				}
			} else {
				xrtSetError(sErrorFile_Read, FALSE);
				xrtFree(sBuff);
				if ( iRetSize ) { *iRetSize = 0; }
				return xCore.sNull;
			}
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			if ( iRetSize ) { *iRetSize = 0; }
			return xCore.sNull;
		}
	#else
		// 其他平台方案
		if ( objFile && (objFile->idx != -1) ) {
			if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
			str sBuff = xrtMalloc(iSize + 4);
			if ( sBuff == NULL ) {
				if ( iRetSize ) { *iRetSize = 0; }
				return xCore.sNull;
			}
			// 读取数据
			size_t iRet = read(objFile->idx, sBuff, iSize);
			if ( iRet > 0 ) {
				// 读取到的文件可能是 utf-32 编码，留 4 个空字节做结尾
				sBuff[iRet] = 0;
				sBuff[iRet + 1] = 0;
				sBuff[iRet + 2] = 0;
				sBuff[iRet + 3] = 0;
				// 转换编码 （ 将读取到的数据转换为 utf-8 编码 ）
				if ( (objFile->Charset >= 0) && (objFile->Charset != XRT_CP_UTF8) ) {
					int iCharSize = xrtGetCharSize(objFile->Charset);
					size_t iTextSize = 0;
					str sRet = xrtConvCharset(sBuff, iRet / iCharSize, objFile->Charset, XRT_CP_UTF8, &iTextSize);
					xrtFree(sBuff);
					if ( iRetSize ) { *iRetSize = iTextSize; }
					return sRet;
				} else {
					if ( iRetSize ) { *iRetSize = iRet; }
					return sBuff;
				}
			} else {
				xrtSetError(sErrorFile_Read, FALSE);
				xrtFree(sBuff);
				if ( iRetSize ) { *iRetSize = 0; }
				return xCore.sNull;
			}
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			if ( iRetSize ) { *iRetSize = 0; }
			return xCore.sNull;
		}
	#endif
}



// 向已打开的文件写入数据 （ iSize 为要写入的字节数 ）
XXAPI size_t xrtWrite(xfile objFile, str sText, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile && (objFile->obj != INVALID_HANDLE_VALUE) ) {
			if ( sText == NULL ) { return 0; }
			if ( iSize == 0 ) { iSize = strlen((const char*)sText); }
			if ( iSize == 0 ) { return 0; }
			DWORD iRetSize;
			if ( (objFile->Charset >= 0) && (objFile->Charset != XRT_CP_UTF8) ) {
				// 需要转换为目标文件的编码再写入
				size_t iTextSize = 0;
				str sBuff = xrtConvCharset(sText, iSize, XRT_CP_UTF8, objFile->Charset, &iTextSize);
				int iCharSize = xrtGetCharSize(objFile->Charset);
				int iRet = WriteFile(objFile->obj, sBuff, iTextSize * iCharSize, &iRetSize, NULL);
				xrtFree(sBuff);
				if ( iRet ) {
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
		if ( objFile && (objFile->idx != -1) ) {
			if ( sText == NULL ) { return 0; }
			if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
			if ( iSize == 0 ) { return 0; }
			if ( (objFile->Charset >= 0) && (objFile->Charset != XRT_CP_UTF8) ) {
				// 需要转换为目标文件的编码再写入
				size_t iConvSize = 0;
				str sBuff = xrtConvCharset(sText, iSize, XRT_CP_UTF8, objFile->Charset, &iConvSize);
				int iCharSize = xrtGetCharSize(objFile->Charset);
				size_t iRetSize = write(objFile->idx, sBuff, iConvSize * iCharSize);
				xrtFree(sBuff);
				if ( iRetSize > 0 ) {
					return iRetSize;
				} else {
					xrtSetError(sErrorFile_Write, FALSE);
					return 0;
				}
			} else {
				// 二进制或 utf-8 编码可以直接写入
				size_t iRetSize = write(objFile->idx, sText, iSize);
				if ( iRetSize > 0 ) {
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
	#endif
}



// 从已打开的文件读取二进制数据到特定位置
XXAPI size_t xrtGetBuffer(xfile objFile, ptr sBuff, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile && (objFile->obj != INVALID_HANDLE_VALUE) ) {
			if ( iSize == 0 ) { return 0; }
			// 读取数据
			DWORD iRet;
			if ( ReadFile(objFile->obj, sBuff, iSize, &iRet, NULL) ) {
				return iRet;
			} else {
				xrtSetError(sErrorFile_Read, FALSE);
				return 0;
			}
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return 0;
		}
	#else
		// 其他平台方案
		if ( objFile && (objFile->idx != -1) ) {
			if ( iSize == 0 ) { return 0; }
			// 读取数据
			ssize_t iRet = read(objFile->idx, sBuff, iSize);
			if ( iRet >= 0 ) {
				return (size_t)iRet;
			} else {
				xrtSetError(sErrorFile_Read, FALSE);
				return 0;
			}
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return 0;
		}
	#endif
}



// 从已打开的文件读取二进制数据 （ 需要使用 xrtFree 释放内存 ）
XXAPI ptr xrtGet(xfile objFile, size_t iSize, size_t* iRetSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile && (objFile->obj != INVALID_HANDLE_VALUE) ) {
			if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
			str sBuff = xrtMalloc(iSize);
			if ( sBuff == NULL ) {
				if ( iRetSize ) { *iRetSize = 0; }
				return xCore.sNull;
			}
			// 读取数据
			DWORD iRet;
			if ( ReadFile(objFile->obj, sBuff, iSize, &iRet, NULL) ) {
				if ( iRetSize ) { *iRetSize = iRet; }
				return sBuff;
			} else {
				xrtSetError(sErrorFile_Read, FALSE);
				xrtFree(sBuff);
				if ( iRetSize ) { *iRetSize = 0; }
				return xCore.sNull;
			}
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			if ( iRetSize ) { *iRetSize = 0; }
			return xCore.sNull;
		}
	#else
		// 其他平台方案
		if ( objFile && (objFile->idx != -1) ) {
			if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
			str sBuff = xrtMalloc(iSize);
			if ( sBuff == NULL ) {
				if ( iRetSize ) { *iRetSize = 0; }
				return xCore.sNull;
			}
			// 读取数据
			size_t iRet = read(objFile->idx, sBuff, iSize);
			if ( iRet > 0 ) {
				if ( iRetSize ) { *iRetSize = iRet; }
				return sBuff;
			} else {
				xrtSetError(sErrorFile_Read, FALSE);
				xrtFree(sBuff);
				if ( iRetSize ) { *iRetSize = 0; }
				return xCore.sNull;
			}
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			if ( iRetSize ) { *iRetSize = 0; }
			return xCore.sNull;
		}
	#endif
}



// 向已打开的文件写入二进制数据
XXAPI int xrtPut(xfile objFile, ptr pBuff, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( objFile && (objFile->obj != INVALID_HANDLE_VALUE) ) {
			// 参数检查
			if ( pBuff == NULL ) { return 0; }
			if ( iSize == 0 ) { return 0; }
			// 写入二进制数据
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
		if ( objFile && (objFile->idx != -1) ) {
			// 参数检查
			if ( pBuff == NULL ) { return 0; }
			if ( iSize == 0 ) { return 0; }
			// 写入二进制数据
			size_t iRetSize = write(objFile->idx, pBuff, iSize);
			if ( iRetSize > 0 ) {
				return iRetSize;
			} else {
				xrtSetError(sErrorFile_Write, FALSE);
				return 0;
			}
		} else {
			xrtSetError(sErrorFile_Handle, FALSE);
			return 0;
		}
	#endif
}



// 向文件追加写入数据
XXAPI int xrtFileAppend(str sPath, str sText, size_t iSize, int iCharset)
{
	if ( sText == NULL ) { return 0; }
	if ( iSize == 0 ) { iSize = strlen((const char*)sText); }
	if ( iSize == 0 ) { return 0; }
	xfile objFile = xrtOpen(sPath, FALSE, iCharset);
	if ( objFile ) {
		xrtSeek(objFile, 0, XRT_SEEK_END);
		ulong iRet = xrtWrite(objFile, sText, iSize);
		xrtClose(objFile);
		return iRet;
	}
	return 0;
}



// 写入并覆盖文件内容
XXAPI int xrtFileWriteAll(str sPath, str sText, size_t iSize, int iCharset)
{
	if ( sText == NULL ) { return 0; }
	if ( iSize == 0 ) { iSize = strlen((const char*)sText); }
	if ( iSize == 0 ) { return 0; }
	xfile objFile = xrtOpen(sPath, FALSE, iCharset);
	if ( objFile ) {
		ulong iRet = xrtWrite(objFile, sText, iSize);
		xrtSetEOF(objFile);
		xrtClose(objFile);
		return iRet;
	}
	return 0;
}



// 读取文件的全部内容 （ 需要使用 xrtFree 释放内存 ）
XXAPI str xrtFileReadAll(str sPath, int iCharset, size_t* iRetSize)
{
	xfile objFile = xrtOpen(sPath, TRUE, iCharset);
	if ( objFile ) {
		uint64 iSize = xrtGetEOF(objFile) - objFile->BOM;
		if ( iSize > 0 ) {
			size_t iFileSize = 0;
			str sRet = xrtRead(objFile, iSize, &iFileSize);
			if ( iRetSize ) { *iRetSize = iFileSize; }
			xrtClose(objFile);
			return sRet;
		} else {
			xrtClose(objFile);
			if ( iRetSize ) { *iRetSize = 0; }
			return xCore.sNull;
		}
	}
	if ( iRetSize ) { *iRetSize = 0; }
	return xCore.sNull;
}



// 写入并覆盖文件内容 （ 二进制 ）
XXAPI int xrtFilePutAll(str sPath, ptr pBuff, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( iSize == 0 ) { return 0; }
		// 打开文件（ 读写模式，不存在则创建 ）
		u16str sPathW = xrtUTF8to16(sPath, 0, NULL);
		HANDLE hFile = CreateFileW(sPathW, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		xrtFree(sPathW);
		if ( hFile == INVALID_HANDLE_VALUE ) {
			xrtSetError(sErrorFile_Open, FALSE);
			return 0;
		}
		// 移动文件指针到开头并写入数据
		SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
		DWORD iRetSize;
		if ( WriteFile(hFile, pBuff, iSize, &iRetSize, NULL) ) {
			// 截断文件到当前写入位置，防止旧数据残留
			SetEndOfFile(hFile);
			CloseHandle(hFile);
			return iRetSize;
		} else {
			xrtSetError(sErrorFile_Write, FALSE);
			CloseHandle(hFile);
			return 0;
		}
	#else
		// 其他平台方案
		if ( iSize == 0 ) { return 0; }
		// 打开文件（ 读写模式，不存在则创建 ）
		int fd = open(sPath, O_RDWR | O_CREAT, 0644);
		if ( fd == -1 ) {
			xrtSetError(sErrorFile_Open, FALSE);
			return 0;
		}
		// 移动文件指针到开头并写入数据
		lseek(fd, 0, SEEK_SET);
		size_t iRetSize = write(fd, pBuff, iSize);
		if ( iRetSize > 0 ) {
			// 截断文件到当前写入位置，防止旧数据残留
			size_t iPos = lseek(fd, 0, SEEK_CUR);
			ftruncate(fd, iPos);
			close(fd);
			return iRetSize;
		} else {
			xrtSetError(sErrorFile_Write, FALSE);
			close(fd);
			return 0;
		}
	#endif
}



// 读取文件的全部内容 （ 二进制，需要使用 xrtFree 释放内存 ）
XXAPI ptr xrtFileGetAll(str sPath, size_t* iRetSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		// 以只读方式打开文件
		u16str sPathW = xrtUTF8to16(sPath, 0, NULL);
		HANDLE hFile = CreateFileW(sPathW, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		xrtFree(sPathW);
		if ( hFile == INVALID_HANDLE_VALUE ) {
			xrtSetError(sErrorFile_Open, FALSE);
			if ( iRetSize ) { *iRetSize = 0; }
			return xCore.sNull;
		}
		// 获取文件大小
		SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
		LARGE_INTEGER stuSize;
		GetFileSizeEx(hFile, &stuSize);
		if ( stuSize.QuadPart == 0 ) {
			CloseHandle(hFile);
			if ( iRetSize ) { *iRetSize = 0; }
			return xCore.sNull;
		}
		// 分配缓冲区并读取全部数据
		str pBuff = xrtMalloc(stuSize.QuadPart + 1);
		if ( pBuff == NULL ) {
			CloseHandle(hFile);
			if ( iRetSize ) { *iRetSize = 0; }
			return xCore.sNull;
		}
		DWORD iRet = 0;
		ReadFile(hFile, pBuff, stuSize.QuadPart, &iRet, NULL);
		if ( iRet == 0 ) {
			xrtSetError(sErrorFile_Read, FALSE);
			CloseHandle(hFile);
			xrtFree(pBuff);
			if ( iRetSize ) { *iRetSize = 0; }
			return xCore.sNull;
		}
		// 添加结尾空字节并关闭文件
		pBuff[iRet] = 0;
		CloseHandle(hFile);
		if ( iRetSize ) { *iRetSize = iRet; }
		return pBuff;
	#else
		// 其他平台方案
		// 以只读方式打开文件
		int fd = open(sPath, O_RDONLY);
		if ( fd == -1 ) {
			xrtSetError(sErrorFile_Open, FALSE);
			if ( iRetSize ) { *iRetSize = 0; }
			return xCore.sNull;
		}
		// 获取文件大小
		lseek(fd, 0, SEEK_SET);
		struct stat fileStat;
		fstat(fd, &fileStat);
		if ( fileStat.st_size == 0 ) {
			close(fd);
			if ( iRetSize ) { *iRetSize = 0; }
			return xCore.sNull;
		}
		// 分配缓冲区并读取全部数据
		str pBuff = xrtMalloc(fileStat.st_size + 1);
		if ( pBuff == NULL ) {
			close(fd);
			if ( iRetSize ) { *iRetSize = 0; }
			return xCore.sNull;
		}
		size_t iRet = read(fd, pBuff, fileStat.st_size);
		if ( iRet == 0 ) {
			xrtSetError(sErrorFile_Read, FALSE);
			close(fd);
			xrtFree(pBuff);
			if ( iRetSize ) { *iRetSize = 0; }
			return xCore.sNull;
		}
		// 添加结尾空字节并关闭文件
		pBuff[iRet] = 0;
		close(fd);
		if ( iRetSize ) { *iRetSize = iRet; }
		return pBuff;
	#endif
}



// 判断路径是否存在
XXAPI bool xrtPathExists(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		u16str sPathW = xrtUTF8to16(sPath, 0, NULL);
		bool bRet = GetFileAttributesW(sPathW) != INVALID_FILE_ATTRIBUTES;
		xrtFree(sPathW);
		return bRet;
	#else
		// 其他平台方案
		if ( access(sPath, F_OK) == 0 ) {
			return TRUE;
		} else {
			return FALSE;
		}
	#endif
}



// 判断文件是否存在
XXAPI bool xrtFileExists(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		// 获取文件属性
		u16str sPathW = xrtUTF8to16(sPath, 0, NULL);
		WIN32_FILE_ATTRIBUTE_DATA wfad = { 0 };
		DWORD iRet = GetFileAttributesExW(sPathW, GetFileExInfoStandard, &wfad);
		xrtFree(sPathW);
		if ( iRet == 0 ) { return FALSE; }
		// 判断是否为普通文件（ 排除目录等 ）
		if ( wfad.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE ) {
			return TRUE;
		} else {
			return FALSE;
		}
	#else
		// 其他平台方案
		// 获取文件状态信息
		struct stat fileStat;
		int iRet = stat(sPath, &fileStat);
		if ( iRet == 0 ) {
			// 判断是否为普通文件（ 排除目录等 ）
			if ( fileStat.st_mode & S_IFDIR ) {
				return FALSE;
			} else if ( fileStat.st_mode & S_IFREG ) {
				return TRUE;
			} else {
				return FALSE;
			}
		} else {
			return FALSE;
		}
	#endif
}

// 判断目录是否存在
XXAPI bool xrtDirExists(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		// 获取路径属性
		u16str sPathW = xrtUTF8to16(sPath, 0, NULL);
		WIN32_FILE_ATTRIBUTE_DATA wfad = { 0 };
		DWORD iRet = GetFileAttributesExW(sPathW, GetFileExInfoStandard, &wfad);
		xrtFree(sPathW);
		if ( iRet == 0 ) { return FALSE; }
		// 判断是否为目录
		if ( wfad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) {
			return TRUE;
		} else {
			return FALSE;
		}
	#else
		// 其他平台方案
		// 获取路径状态信息
		struct stat fileStat;
		int iRet = stat(sPath, &fileStat);
		if ( iRet == 0 ) {
			// 判断是否为目录
			if ( fileStat.st_mode & S_IFDIR ) {
				return TRUE;
			} else if ( fileStat.st_mode & S_IFREG ) {
				return FALSE;
			} else {
				return FALSE;
			}
		} else {
			return FALSE;
		}
	#endif
}

// 获取文件长度
XXAPI size_t xrtFileGetSize(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		u16str sPathW = xrtUTF8to16(sPath, 0, NULL);
		HANDLE hFile = CreateFileW(sPathW, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		xrtFree(sPathW);
		if ( hFile == INVALID_HANDLE_VALUE ) {
			xrtSetError(sErrorFile_Open, FALSE);
			return 0;
		}
		LARGE_INTEGER iSize;
		GetFileSizeEx(hFile, &iSize);
		CloseHandle(hFile);
		return iSize.QuadPart;
	#else
		// 其他平台方案
		struct stat fileStat;
		int iRet = stat(sPath, &fileStat);
		if ( iRet == 0 ) {
			return fileStat.st_size;
		} else {
			return 0;
		}
	#endif
}



// 设置文件长度
XXAPI bool xrtFileSetSize(str sPath, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		// 以写模式打开文件（ 不存在则创建 ）
		u16str sPathW = xrtUTF8to16(sPath, 0, NULL);
		HANDLE hFile = CreateFileW(sPathW, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		xrtFree(sPathW);
		if ( hFile == INVALID_HANDLE_VALUE ) {
			xrtSetError(sErrorFile_Open, FALSE);
			return FALSE;
		}
		// 移动文件指针到目标大小位置并截断
		LARGE_INTEGER iPos_stu;
		iPos_stu.QuadPart = iSize;
		SetFilePointerEx(hFile, iPos_stu, NULL, FILE_BEGIN);
		SetEndOfFile(hFile);
		CloseHandle(hFile);
		return TRUE;
	#else
		// 其他平台方案
		// 以读写模式打开文件（ 不存在则创建 ）
		int fd = open(sPath, O_RDWR | O_CREAT, 0644);
		if ( fd == -1 ) {
			xrtSetError(sErrorFile_Open, FALSE);
			return FALSE;
		}
		// 截断文件到目标大小
		ftruncate(fd, iSize);
		close(fd);
		return TRUE;
	#endif
}



// 获取文件属性
XXAPI int xrtFileGetAttr(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		u16str sPathW = xrtUTF8to16(sPath, 0, NULL);
		int iRet = GetFileAttributesW(sPathW);
		xrtFree(sPathW);
		return iRet;
	#else
		// 其他平台方案
		struct stat fileStat;
		int iRet = stat(sPath, &fileStat);
		if ( iRet == 0 ) {
			return fileStat.st_mode;
		} else {
			return 0;
		}
	#endif
}



// 设置文件属性
XXAPI bool xrtFileSetAttr(str sPath, int iAttr)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		u16str sPathW = xrtUTF8to16(sPath, 0, NULL);
		int iRet = SetFileAttributesW(sPathW, iAttr);
		xrtFree(sPathW);
		if ( iRet ) {
			return TRUE;
		} else {
			return FALSE;
		}
	#else
		// 其他平台方案
		int iRet = chmod(sPath, iAttr);
		if ( iRet == 0 ) {
			return TRUE;
		} else {
			return FALSE;
		}
	#endif
}



// 获取文件最后一次访问时间
XXAPI int64 xrtFileGetAccessTime(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		// 获取文件状态信息
		struct _stat64 fileStat;
		u16str sPathW = xrtUTF8to16(sPath, 0, NULL);
		int iRet;
		if ( sPathW == NULL ) {
			return 0;
		}
		iRet = _wstat64((const wchar_t*)sPathW, &fileStat);
		xrtFree(sPathW);
		// 转换时间戳为 xrt 时间格式（ 相对于 1899-12-30 基准 ）
		if ( iRet == 0 ) {
			return fileStat.st_atime + XRT_TIME_19700101;
		} else {
			return 0;
		}
	#else
		// 其他平台方案
		// 获取文件状态信息
		struct stat fileStat;
		int iRet = stat(sPath, &fileStat);
		// 转换时间戳为 xrt 时间格式（ 相对于 1899-12-30 基准 ）
		if ( iRet == 0 ) {
			return fileStat.st_atime + XRT_TIME_19700101;
		} else {
			return 0;
		}
	#endif
}



// 获取文件修改时间
XXAPI int64 xrtFileGetChangeTime(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		// 获取文件状态信息 
		struct _stat64 fileStat;
		u16str sPathW = xrtUTF8to16(sPath, 0, NULL);
		int iRet;
		if ( sPathW == NULL ) {
			return 0;
		}
		iRet = _wstat64((const wchar_t*)sPathW, &fileStat);
		xrtFree(sPathW);
		// 转换时间戳为 xrt 时间格式（ 相对于 1899-12-30 基准 ）
		if ( iRet == 0 ) {
			return fileStat.st_mtime + XRT_TIME_19700101;
		} else {
			return 0;
		}
	#else
		// 其他平台方案
		// 获取文件状态信息 
		struct stat fileStat;
		int iRet = stat(sPath, &fileStat);
		// 转换时间戳为 xrt 时间格式（ 相对于 1899-12-30 基准 ）
		if ( iRet == 0 ) {
			return fileStat.st_mtime + XRT_TIME_19700101;
		} else {
			return 0;
		}
	#endif
}



// 复制文件
XXAPI bool xrtFileCopy(str sSrc, str sDst, bool bReWrite)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		u16str sSrcW = xrtUTF8to16(sSrc, 0, NULL);
		u16str sDstW = xrtUTF8to16(sDst, 0, NULL);
		int iRet = CopyFileW(sSrcW, sDstW, bReWrite ? FALSE : TRUE);
		xrtFree(sSrcW);
		xrtFree(sDstW);
		return iRet;
	#else
		// 其他平台方案
		if ( bReWrite == FALSE ) {
			if ( access(sDst, F_OK) == 0 ) {
				return FALSE;		// 文件已经存在
			}
		}
		// 打开文件
		int fsrc = open(sSrc, O_RDONLY);
		if ( fsrc == -1 ) {
			xrtSetError(sErrorFile_Open, FALSE);
			return FALSE;
		}
		int fdst = open(sDst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if ( fdst == -1 ) {
			xrtSetError(sErrorFile_Open, FALSE);
			close(fsrc);
			return FALSE;
		}
		// 申请内存
		struct stat fileStat;
		fstat(fsrc, &fileStat);
		if ( fileStat.st_size == 0 ) {
			close(fsrc);
			close(fdst);
			return TRUE;		// 0 字节大小的文件不需要复制
		}
		str sText = xrtMalloc(fileStat.st_size);
		if ( sText == NULL ) {
			close(fsrc);
			close(fdst);
			return FALSE;
		}
		// 读取原始文件内容
		ssize_t iRetSize = read(fsrc, sText, fileStat.st_size);
		if ( iRetSize < 0 || iRetSize != fileStat.st_size ) {
			xrtSetError(sErrorFile_Read, FALSE);
			close(fsrc);
			close(fdst);
			xrtFree(sText);
			return FALSE;
		}
		// 写入目标文件
		iRetSize = write(fdst, sText, fileStat.st_size);
		if ( iRetSize < 0 || iRetSize != fileStat.st_size ) {
			xrtSetError(sErrorFile_Write, FALSE);
			close(fsrc);
			close(fdst);
			xrtFree(sText);
			return FALSE;
		}
		close(fsrc);
		close(fdst);
		xrtFree(sText);
		return TRUE;
	#endif
}



// 移动文件（重命名）
XXAPI bool xrtFileMove(str sSrc, str sDst, bool bReWrite)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		u16str sSrcW = xrtUTF8to16(sSrc, 0, NULL);
		u16str sDstW = xrtUTF8to16(sDst, 0, NULL);
		// 如果源文件和目标文件都存在，并且 bReWrite 为 TRUE，将目标文件删除
		int bExistSrc = xrtFileExists(sSrc);
		int bExistDst = xrtFileExists(sDst);
		if ( bExistSrc && bExistDst && bReWrite ) {
			DeleteFileW(sDstW);
		}
		// 移动文件
		int iRet = MoveFileExW(sSrcW, sDstW, MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH | (bReWrite ? MOVEFILE_REPLACE_EXISTING : 0));
		xrtFree(sSrcW);
		xrtFree(sDstW);
		if ( iRet ) {
			return TRUE;
		} else {
			return FALSE;
		}
	#else
		// 其他平台方案
		int bExistSrc = xrtFileExists(sSrc);
		int bExistDst = xrtFileExists(sDst);
		if ( bExistSrc && bExistDst && bReWrite ) {
			remove(sDst);
		}
		int iRet = rename(sSrc, sDst);
		if ( iRet == 0 ) {
			return TRUE;
		} else {
			// 无法移动文件时，使用复制 + 删除来实现文件移动
			iRet = xrtFileCopy(sSrc, sDst, bReWrite);
			if ( iRet ) {
				remove(sSrc);
				return TRUE;
			} else {
				return FALSE;
			}
		}
	#endif
}



// 删除文件
XXAPI bool xrtFileDelete(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		u16str sPathW = xrtUTF8to16(sPath, 0, NULL);
		int iRet = DeleteFileW(sPathW);
		xrtFree(sPathW);
		if ( iRet ) {
			return TRUE;
		} else {
			return FALSE;
		}
	#else
		// 其他平台方案
		int iRet = remove(sPath);
		if ( iRet ) {
			return FALSE;
		} else {
			return TRUE;
		}
	#endif
}



// 扫描文件夹 （ 返回文件数量 ）
#if defined(_WIN32) || defined(_WIN64)
	// windows 方案
	int __pri__DirScan_Proc(str sPath, size_t iSize, int bRecu, xrtDirScanProc pProc, ptr Param)
	{
		(void)iSize;
		xrtDirScanProc pCallBack = pProc;
		int iFileCount = 0;
		WIN32_FIND_DATAW objFindData;
		str sFindPath = xrtPathJoin(2, sPath, "*");
		u16str sFindPathW = xrtUTF8to16(sFindPath, 0, NULL);
		HANDLE hFind = FindFirstFileW(sFindPathW, &objFindData);
		xrtFree(sFindPath);
		xrtFree(sFindPathW);
		if ( hFind == INVALID_HANDLE_VALUE ) {
			xrtSetError(sErrorFile_OpenDir, FALSE);
			return 0;
		}
		int bNext = TRUE;
		while ( bNext ) {
			int bExit = FALSE;
			if ( objFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) {
				// 过滤 . 和 .. 目录
				if ( (objFindData.cFileName[0] == L'.') && ((objFindData.cFileName[1] == 0) || ((objFindData.cFileName[1] == L'.') && (objFindData.cFileName[2] == 0))) ) {
				} else {
					str sFileName = xrtUTF16to8(objFindData.cFileName, 0, NULL);
					str sDir = xrtPathJoin(2, sPath, sFileName);
					size_t iDirSize = strlen((const char*)sDir);
					xrtFree(sFileName);
					// 处理文件夹 - 进入
					if ( pProc ) {
						bExit = pCallBack(sDir, iDirSize, 1, &objFindData, Param);
					}
					// 递归子目录
					if ( bRecu ) {
						iFileCount += __pri__DirScan_Proc(sDir, iDirSize, bRecu, pProc, Param);
					}
					// 处理文件夹 - 离开
					if ( pProc ) {
						bExit = pCallBack(sDir, iDirSize, 2, &objFindData, Param);
					}
					xrtFree(sDir);
				}
			} else {
				// 处理文件
				str sFileName = xrtUTF16to8(objFindData.cFileName, 0, NULL);
				str sFile = xrtPathJoin(2, sPath, sFileName);
				size_t iFileSize = strlen((const char*)sFile);
				xrtFree(sFileName);
				if ( pProc ) {
					bExit = pCallBack(sFile, iFileSize, 0, &objFindData, Param);
				}
				xrtFree(sFile);
				iFileCount++;
			}
			// 中途停止扫描
			if ( bExit ) {
				break;
			}
			bNext = FindNextFileW(hFind, &objFindData);
		}
		FindClose(hFind);
		return iFileCount;
	}
#else
	// 其他平台方案
	int __pri__DirScan_Proc(str sPath, size_t iSize, int bRecu, xrtDirScanProc pProc, ptr Param)
	{
		xrtDirScanProc pCallBack = pProc;
		int iFileCount = 0;
		DIR* dir = opendir(sPath);
		if ( dir == NULL ) {
			xrtSetError(sErrorFile_OpenDir, FALSE);
			return 0;
		}
		struct dirent* entry;
		while ( (entry = readdir(dir)) != NULL ) {
			int bExit = FALSE;
			unsigned char iType = entry->d_type;
			if ( iType == DT_UNKNOWN ) {
				str sEntry = xrtPathJoin(2, sPath, entry->d_name);
				if ( sEntry ) {
					struct stat entryStat;
					if ( stat(sEntry, &entryStat) == 0 ) {
						if ( S_ISDIR(entryStat.st_mode) ) {
							iType = DT_DIR;
						} else if ( S_ISREG(entryStat.st_mode) ) {
							iType = DT_REG;
						}
					}
					xrtFree(sEntry);
				}
			}
			if ( iType == DT_DIR ) {
				// 过滤 . 和 .. 目录
				if ( (entry->d_name[0] == '.') && ((entry->d_name[1] == 0) || ((entry->d_name[1] == '.') && (entry->d_name[2] == 0))) ) {
				} else {
					str sDir = xrtPathJoin(2, sPath, entry->d_name);
					size_t iDirSize = strlen(sDir);
					// 处理文件夹 - 进入
					if ( pProc ) {
						bExit = pCallBack(sDir, iDirSize, 1, entry, Param);
					}
					// 递归子目录
					if ( bRecu ) {
						iFileCount += __pri__DirScan_Proc(sDir, iDirSize, bRecu, pProc, Param);
					}
					// 处理文件夹 - 离开
					if ( pProc ) {
						bExit = pCallBack(sDir, iDirSize, 2, entry, Param);
					}
					xrtFree(sDir);
				}
			} else if ( iType == DT_REG ) {
				// 处理文件
				str sFile = xrtPathJoin(2, sPath, entry->d_name);
				size_t iFileSize = strlen(sFile);
				if ( pProc ) {
					bExit = pCallBack(sFile, iFileSize, 0, entry, Param);
				}
				xrtFree(sFile);
				iFileCount++;
			}
			// 中途停止扫描
			if ( bExit ) {
				break;
			}
		}
		closedir(dir);
		return iFileCount;
	}
#endif
// xrtDirScan 相关处理
XXAPI int xrtDirScan(str sPath, int bRecu, xrtDirScanProc pProc, ptr Param)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( sPath == NULL ) { return 0; }
		size_t iSize = strlen((const char*)sPath);
		if ( iSize == 0 ) { return 0; }
		int iFileCount = __pri__DirScan_Proc(sPath, iSize, bRecu, pProc, Param);
		return iFileCount;
	#else
		// 其他平台方案
		if ( sPath == NULL ) { return 0; }
		size_t iSize = strlen((const char*)sPath);
		if ( iSize == 0 ) { return 0; }
		int iFileCount = __pri__DirScan_Proc(sPath, iSize, bRecu, pProc, Param);
		return iFileCount;
	#endif
}



// 创建文件夹
XXAPI bool xrtDirCreate(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		u16str sPathW = xrtUTF8to16(sPath, 0, NULL);
		int iRet = CreateDirectoryW(sPathW, NULL);
		xrtFree(sPathW);
		return iRet;
	#else
		// 其他平台方案
		int iRet = mkdir(sPath, 0755);
		if ( iRet == 0 ) {
			return TRUE;
		} else {
			return FALSE;
		}
	#endif
}



// 创建多级文件夹
XXAPI bool xrtDirCreateAll(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( sPath == NULL ) { return FALSE; }
		// 将路径转换为宽字符
		size_t iSize = 0;
		u16str sPathW = xrtUTF8to16(sPath, 0, &iSize);
		if ( iSize == 0 ) { return FALSE; }
		// 逐级创建目录：遇到路径分隔符时截断并创建已扫描的部分路径
		u16str sCurPath = xrtMalloc((iSize + 1) * sizeof(wchar_t));
		if ( sCurPath == NULL ) {
			xrtFree(sPathW);
			return FALSE;
		}
		size_t iCurPos = 0;
		for ( size_t i = 0; i < iSize; i++ ) {
			if ( (sPathW[i] == L'/') || (sPathW[i] == L'\\') ) {
				sCurPath[iCurPos] = 0;
				CreateDirectoryW(sCurPath, NULL);
				sCurPath[iCurPos++] = L'\\';
			} else {
				sCurPath[iCurPos++] = sPathW[i];
			}
		}
		// 创建最后一段目录
		sCurPath[iSize] = 0;
		CreateDirectoryW(sCurPath, NULL);
		xrtFree(sCurPath);
		xrtFree(sPathW);
		return TRUE;
	#else
		// 其他平台方案
		if ( sPath == NULL ) { return FALSE; }
		size_t iSize = strlen(sPath);
		if ( iSize == 0 ) { return FALSE; }
		// 逐级创建目录：遇到路径分隔符时截断并创建已扫描的部分路径
		str sCurPath = xrtMalloc(iSize + 1);
		if ( sCurPath == NULL ) {
			return FALSE;
		}
		size_t iCurPos = 0;
		for ( int i = 0; i < iSize; i++ ) {
			if ( (sPath[i] == '/') || (sPath[i] == '\\') ) {
				sCurPath[iCurPos] = 0;
				mkdir(sCurPath, 0755);
				sCurPath[iCurPos++] = '/';
			} else {
				sCurPath[iCurPos++] = sPath[i];
			}
		}
		// 创建最后一段目录
		sCurPath[iSize] = 0;
		mkdir(sCurPath, 0755);
		xrtFree(sCurPath);
		return TRUE;
	#endif
}



// 复制文件夹 （ 返回操作的文件数量 ）
#if defined(_WIN32) || defined(_WIN64)
	typedef struct {
		str DstPath;
		size_t DstSize;
		size_t SrcSize;
		int ReWrite;
		int MoveMode;			// 移动模式
	} xrtCopyFolder_Info;


	// 内部函数：复制 pri 目录进程
	int __pri__DirCopyProc(str sPath, size_t iSize, int bDir, ptr pData, ptr Param)
	{
		xrtCopyFolder_Info* pInfo = (xrtCopyFolder_Info*)Param;
		(void)iSize;
		(void)pData;
		if ( bDir == 0 ) {
			str sDstPath = xrtPathJoin(2, pInfo->DstPath, &sPath[pInfo->SrcSize]);
			//printf("\tcopy file   : %S -> %S\n", sPath, sDstPath);
			if ( pInfo->MoveMode ) {
				xrtFileMove(sPath, sDstPath, pInfo->ReWrite);
			} else {
				xrtFileCopy(sPath, sDstPath, pInfo->ReWrite);
			}
			xrtFree(sDstPath);
		} else if ( bDir == 1 ) {
			str sDstPath = xrtPathJoin(2, pInfo->DstPath, &sPath[pInfo->SrcSize]);
			//printf("\tcreate dir  : %S\n", sDstPath);
			xrtDirCreate(sDstPath);
			xrtFree(sDstPath);
		} else if ( bDir == 2 ) {
			if ( pInfo->MoveMode ) {
				// 移动模式离开文件夹时删除文件夹
				//printf("\tremove dir  : %S\n", sPath);
				u16str sPathW = xrtUTF8to16(sPath, 0, NULL);
				RemoveDirectoryW(sPathW);
				xrtFree(sPathW);
			}
		}
		return FALSE;
	}
#else
	typedef struct {
		str DstPath;
		size_t DstSize;
		size_t SrcSize;
		int ReWrite;
		int MoveMode;			// 移动模式
	} xrtCopyFolder_Info;


	// 内部函数：复制 pri 目录进程
	int __pri__DirCopyProc(str sPath, size_t iSize, int bDir, ptr pData, ptr Param)
	{
		xrtCopyFolder_Info* pInfo = (xrtCopyFolder_Info*)Param;
		(void)iSize;
		(void)pData;
		if ( bDir == 0 ) {
			str sDstPath = xrtPathJoin(2, pInfo->DstPath, &sPath[pInfo->SrcSize]);
			//printf("\tcopy file   : %s -> %s\n", sPath, sDstPath);
			if ( pInfo->MoveMode ) {
				xrtFileMove(sPath, sDstPath, pInfo->ReWrite);
			} else {
				xrtFileCopy(sPath, sDstPath, pInfo->ReWrite);
			}
			xrtFree(sDstPath);
		} else if ( bDir == 1 ) {
			str sDstPath = xrtPathJoin(2, pInfo->DstPath, &sPath[pInfo->SrcSize]);
			//printf("\tcreate dir  : %s\n", sDstPath);
			xrtDirCreate(sDstPath);
			xrtFree(sDstPath);
		} else if ( bDir == 2 ) {
			if ( pInfo->MoveMode ) {
				// 移动模式离开文件夹时删除文件夹
				//printf("\tremove dir  : %s\n", sPath);
				rmdir(sPath);
			}
		}
		return FALSE;
	}
#endif
// 复制目录
XXAPI int xrtDirCopy(str sSrc, str sDst, bool bReWrite)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		// 参数检查
		if ( sSrc == NULL ) { return 0; }
		size_t iSrcSize = strlen((const char*)sSrc);
		if ( iSrcSize == 0 ) { return 0; }
		if ( sDst == NULL ) { return 0; }
		size_t iDstSize = strlen((const char*)sDst);
		if ( iDstSize == 0 ) { return 0; }
		// 构建复制信息结构体，传入源路径偏移量用于计算相对路径
		xrtCopyFolder_Info stuInfo;
		stuInfo.DstPath = sDst;
		stuInfo.DstSize = iDstSize;
		stuInfo.SrcSize = iSrcSize + 1;
		stuInfo.ReWrite = bReWrite;
		stuInfo.MoveMode = FALSE;
		// 确保目标目录存在，然后递归扫描并复制
		xrtDirCreateAll(sDst);
		return xrtDirScan(sSrc, TRUE, __pri__DirCopyProc, &stuInfo);
	#else
		// 其他平台方案
		// 参数检查
		if ( sSrc == NULL ) { return 0; }
		size_t iSrcSize = strlen((const char*)sSrc);
		if ( iSrcSize == 0 ) { return 0; }
		if ( sDst == NULL ) { return 0; }
		size_t iDstSize = strlen((const char*)sDst);
		if ( iDstSize == 0 ) { return 0; }
		// 构建复制信息结构体，传入源路径偏移量用于计算相对路径
		xrtCopyFolder_Info stuInfo;
		stuInfo.DstPath = sDst;
		stuInfo.DstSize = iDstSize;
		stuInfo.SrcSize = iSrcSize + 1;
		stuInfo.ReWrite = bReWrite;
		stuInfo.MoveMode = FALSE;
		// 确保目标目录存在，然后递归扫描并复制
		xrtDirCreateAll(sDst);
		return xrtDirScan(sSrc, TRUE, __pri__DirCopyProc, &stuInfo);
	#endif
}



// 移动文件夹 （ 返回操作的文件数量 ）
XXAPI int xrtDirMove(str sSrc, str sDst, bool bReWrite)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		// 参数检查
		if ( sSrc == NULL ) { return 0; }
		size_t iSrcSize = strlen(__xrt_cstr(sSrc));
		if ( iSrcSize == 0 ) { return 0; }
		if ( sDst == NULL ) { return 0; }
		size_t iDstSize = strlen(__xrt_cstr(sDst));
		if ( iDstSize == 0 ) { return 0; }
		// 构建移动信息结构体，MoveMode 为 TRUE 表示移动文件后删除源文件和源目录
		xrtCopyFolder_Info stuInfo;
		stuInfo.DstPath = sDst;
		stuInfo.DstSize = iDstSize;
		stuInfo.SrcSize = iSrcSize + 1;
		stuInfo.ReWrite = bReWrite;
		stuInfo.MoveMode = TRUE;
		// 确保目标目录存在，递归扫描并移动文件
		xrtDirCreateAll(sDst);
		int iRet = xrtDirScan(sSrc, TRUE, __pri__DirCopyProc, &stuInfo);
		// 删除源目录根目录
		u16str sSrcW = xrtUTF8to16(sSrc, 0, NULL);
		RemoveDirectoryW(sSrcW);
		xrtFree(sSrcW);
		return iRet;
	#else
		// 其他平台方案
		// 参数检查
		if ( sSrc == NULL ) { return 0; }
		size_t iSrcSize = strlen(__xrt_cstr(sSrc));
		if ( iSrcSize == 0 ) { return 0; }
		if ( sDst == NULL ) { return 0; }
		size_t iDstSize = strlen(__xrt_cstr(sDst));
		if ( iDstSize == 0 ) { return 0; }
		// 构建移动信息结构体，MoveMode 为 TRUE 表示移动文件后删除源文件和源目录
		xrtCopyFolder_Info stuInfo;
		stuInfo.DstPath = sDst;
		stuInfo.DstSize = iDstSize;
		stuInfo.SrcSize = iSrcSize + 1;
		stuInfo.ReWrite = bReWrite;
		stuInfo.MoveMode = TRUE;
		// 确保目标目录存在，递归扫描并移动文件
		xrtDirCreateAll(sDst);
		int iRet = xrtDirScan(sSrc, TRUE, __pri__DirCopyProc, &stuInfo);
		// 删除源目录根目录
		rmdir(sSrc);
		return iRet;
	#endif
}



// 删除文件夹 （ 返回操作的文件数量 ）
#if defined(_WIN32) || defined(_WIN64)
	// 内部函数：删除 pri 目录进程
	int __pri__DirDeleteProc(str sPath, size_t iSize, int bDir, ptr pData, ptr Param)
	{
		(void)pData;
		(void)Param;
		if ( bDir == 0 ) {
			//printf("\tremove file : %S\n", sPath);
			xrtFileDelete(sPath);
		} else if ( bDir == 2 ) {
			//printf("\tremove dir  : %S\n", sPath);
			u16str sPathW = xrtUTF8to16(sPath, iSize, NULL);
			RemoveDirectoryW(sPathW);
			xrtFree(sPathW);
		}
		return FALSE;
	}
#else
	// 内部函数：删除 pri 目录进程
	int __pri__DirDeleteProc(str sPath, size_t iSize, int bDir, ptr pData, ptr Param)
	{
		(void)iSize;
		(void)pData;
		(void)Param;
		if ( bDir == 0 ) {
			//printf("\tremove file : %s\n", sPath);
			xrtFileDelete(sPath);
		} else if ( bDir == 2 ) {
			//printf("\tremove dir  : %s\n", sPath);
			rmdir(sPath);
		}
		return FALSE;
	}
#endif
// 删除目录
XXAPI int xrtDirDelete(str sPath)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( sPath == NULL ) { return 0; }
		size_t iSize = strlen((const char*)sPath);
		if ( iSize == 0 ) { return 0; }
		int iRet = xrtDirScan(sPath, TRUE, __pri__DirDeleteProc, NULL);
		u16str sPathW = xrtUTF8to16(sPath, iSize, NULL);
		RemoveDirectoryW(sPathW);
		xrtFree(sPathW);
		return iRet;
	#else
		// 其他平台方案
		if ( sPath == NULL ) { return 0; }
		size_t iSize = strlen(sPath);
		if ( iSize == 0 ) { return 0; }
		int iRet = xrtDirScan(sPath, TRUE, __pri__DirDeleteProc, NULL);
		rmdir(sPath);
		return iRet;
	#endif
	return 0;
}


