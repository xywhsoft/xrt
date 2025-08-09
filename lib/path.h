


// 通过路径获取文件名 + 扩展名（ 需使用 xrtFree 释放内存 ）
XXAPI ustr xrtPathGetNameExt(ustr sPath, size_t iSize)
{
	if ( sPath == NULL ) { return (ustr)xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sPath); }
	if ( iSize == 0 ) { return (ustr)xCore.sNull; }
	for ( int i = iSize - 1; i >= 0; i-- ) {
		if ( (sPath[i] == L'/') || (sPath[i] == L'\\') ) {
			if ( i >= (iSize - 1) ) {
				return (ustr)xCore.sNull;
			} else {
				return xrtCopyStr(&sPath[i + 1], iSize - i - 1);
			}
		}
	}
	return xrtCopyStr(sPath, iSize);
}
XXAPI wstr xrtPathGetNameExtW(wstr sPath, size_t iSize)
{
	if ( sPath == NULL ) { return (wstr)xCore.sNull; }
	if ( iSize == 0 ) { iSize = wcslen(sPath); }
	if ( iSize == 0 ) { return (wstr)xCore.sNull; }
	for ( int i = iSize - 1; i >= 0; i-- ) {
		if ( (sPath[i] == L'/') || (sPath[i] == L'\\') ) {
			if ( i >= (iSize - 1) ) {
				return (wstr)xCore.sNull;
			} else {
				return xrtCopyStrW(&sPath[i + 1], iSize - i - 1);
			}
		}
	}
	return xrtCopyStrW(sPath, iSize);
}



// 通过路径获取文件名（ 需使用 xrtFree 释放内存 ）
XXAPI ustr xrtPathGetName(ustr sPath, size_t iSize)
{
	if ( sPath == NULL ) { return (ustr)xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sPath); }
	if ( iSize == 0 ) { return (ustr)xCore.sNull; }
	uint iPointPos = 0;
	for ( int i = iSize - 1; i >= 0; i-- ) {
		if ( sPath[i] == L'.' ) {
			iPointPos = iSize - i;
		} else if ( (sPath[i] == L'/') || (sPath[i] == L'\\') ) {
			if ( i >= (iSize - 1) ) {
				return (ustr)xCore.sNull;
			} else {
				return xrtCopyStr(&sPath[i + 1], iSize - i - iPointPos - 1);
			}
		}
	}
	return xrtCopyStr(sPath, iSize);
}
XXAPI wstr xrtPathGetNameW(wstr sPath, size_t iSize)
{
	if ( sPath == NULL ) { return (wstr)xCore.sNull; }
	if ( iSize == 0 ) { iSize = wcslen(sPath); }
	if ( iSize == 0 ) { return (wstr)xCore.sNull; }
	uint iPointPos = 0;
	for ( int i = iSize - 1; i >= 0; i-- ) {
		if ( sPath[i] == L'.' ) {
			iPointPos = iSize - i;
		} else if ( (sPath[i] == L'/') || (sPath[i] == L'\\') ) {
			if ( i >= (iSize - 1) ) {
				return (wstr)xCore.sNull;
			} else {
				return xrtCopyStrW(&sPath[i + 1], iSize - i - iPointPos - 1);
			}
		}
	}
	return xrtCopyStrW(sPath, iSize);
}



// 通过路径获取扩展名（ 需使用 xrtFree 释放内存 ）
XXAPI ustr xrtPathGetExt(ustr sPath, size_t iSize)
{
	if ( sPath == NULL ) { return (ustr)xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sPath); }
	if ( iSize == 0 ) { return (ustr)xCore.sNull; }
	for ( int i = iSize - 1; i >= 0; i-- ) {
		if ( sPath[i] == L'.' ) {
			return xrtCopyStr(&sPath[i + 1], iSize - i - 1);
		} else if ( (sPath[i] == L'/') || (sPath[i] == L'\\') ) {
			return (ustr)xCore.sNull;
		}
	}
	return (ustr)xCore.sNull;
}
XXAPI wstr xrtPathGetExtW(wstr sPath, size_t iSize)
{
	if ( sPath == NULL ) { return (wstr)xCore.sNull; }
	if ( iSize == 0 ) { iSize = wcslen(sPath); }
	if ( iSize == 0 ) { return (wstr)xCore.sNull; }
	for ( int i = iSize - 1; i >= 0; i-- ) {
		if ( sPath[i] == L'.' ) {
			return xrtCopyStrW(&sPath[i + 1], iSize - i - 1);
		} else if ( (sPath[i] == L'/') || (sPath[i] == L'\\') ) {
			return (wstr)xCore.sNull;
		}
	}
	return (wstr)xCore.sNull;
}



// 通过路径获取文件夹（ 需使用 xrtFree 释放内存 ）
XXAPI ustr xrtPathGetDir(ustr sPath, size_t iSize)
{
	if ( sPath == NULL ) { return (ustr)xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sPath); }
	if ( iSize == 0 ) { return (ustr)xCore.sNull; }
	for ( int i = iSize - 1; i >= 0; i-- ) {
		if ( (sPath[i] == L'/') || (sPath[i] == L'\\') ) {
			if ( i >= (iSize - 1) ) {
				return xrtCopyStr(sPath, iSize - 1);
			} else {
				return xrtCopyStr(sPath, i);
			}
		}
	}
	return (ustr)xCore.sNull;
}
XXAPI wstr xrtPathGetDirW(wstr sPath, size_t iSize)
{
	if ( sPath == NULL ) { return (wstr)xCore.sNull; }
	if ( iSize == 0 ) { iSize = wcslen(sPath); }
	if ( iSize == 0 ) { return (wstr)xCore.sNull; }
	for ( int i = iSize - 1; i >= 0; i-- ) {
		if ( (sPath[i] == L'/') || (sPath[i] == L'\\') ) {
			if ( i >= (iSize - 1) ) {
				return xrtCopyStrW(sPath, iSize - 1);
			} else {
				return xrtCopyStrW(sPath, i);
			}
		}
	}
	return (wstr)xCore.sNull;
}



// 判断是否为绝对路径（Linux 系统以 / 开头为绝对路径，Windows系统含 : 为绝对路径）
XXAPI int xrtPathIsAbs(ustr sPath, size_t iSize)
{
	if ( sPath == NULL ) { return FALSE; }
	if ( iSize == 0 ) { iSize = strlen(sPath); }
	if ( iSize == 0 ) { return FALSE; }
	if ( sPath[0] == '/' ) {
		return TRUE;
	}
	for ( int i = 0; i < iSize; i++ ) {
		if ( sPath[i] == ':' ) {
			return TRUE;
		}
	}
	return FALSE;
}
XXAPI int xrtPathIsAbsW(wstr sPath, size_t iSize)
{
	if ( sPath == NULL ) { return FALSE; }
	if ( iSize == 0 ) { iSize = wcslen(sPath); }
	if ( iSize == 0 ) { return FALSE; }
	if ( sPath[0] == L'/' ) {
		return TRUE;
	}
	for ( int i = 0; i < iSize; i++ ) {
		if ( sPath[i] == L':' ) {
			return TRUE;
		}
	}
	return FALSE;
}



// 获取随机不存在的路径（ 需使用 xrtFree 释放内存 ）
XXAPI ustr xrtPathRandom(ustr sHead, size_t iHeadSize, ustr sFoot, size_t iFootSize, int iLen)
{
	if ( sHead ) {
		if ( iHeadSize == 0 ) { iHeadSize = strlen(sHead); }
		if ( iHeadSize == 0 ) { sHead = NULL; }
	} else {
		iHeadSize = 0;
	}
	if ( sFoot ) {
		if ( iFootSize == 0 ) { iFootSize = strlen(sFoot); }
		if ( iFootSize == 0 ) { sFoot = NULL; }
	} else {
		iFootSize = 0;
	}
	int iSize = iHeadSize + iFootSize + iLen;
	if ( iSize == 0 ) { return (ustr)xCore.sNull; }
	ustr sRet = xrtMalloc(iSize + 1);
	if ( sRet == NULL ) {
		xrtSetError(xCore.ERROR_DESC.MALLOC, FALSE);
		return (ustr)xCore.sNull;
	}
	if ( sHead ) {
		memcpy(sRet, sHead, iHeadSize);
	}
	for ( int i = 0; i < iLen; i++ ) {
		int idx = xrtRand(0, 61);		// 这里写 61，可以忽略 - 和 _ 字符
		sRet[iHeadSize + i] = RandStringDefaultTemplateW[idx];
	}
	if ( sFoot ) {
		memcpy(&sRet[iHeadSize + iLen], sFoot, iFootSize);
	}
	sRet[iSize] = 0;
	return sRet;
}
XXAPI wstr xrtPathRandomW(wstr sHead, size_t iHeadSize, wstr sFoot, size_t iFootSize, int iLen)
{
	if ( sHead ) {
		if ( iHeadSize == 0 ) { iHeadSize = wcslen(sHead); }
		if ( iHeadSize == 0 ) { sHead = NULL; }
	} else {
		iHeadSize = 0;
	}
	if ( sFoot ) {
		if ( iFootSize == 0 ) { iFootSize = wcslen(sFoot); }
		if ( iFootSize == 0 ) { sFoot = NULL; }
	} else {
		iFootSize = 0;
	}
	int iSize = iHeadSize + iFootSize + iLen;
	if ( iSize == 0 ) { return (wstr)xCore.sNull; }
	wstr sRet = xrtMalloc((iSize + 1) * sizeof(wchar_t));
	if ( sRet == NULL ) {
		xrtSetError(xCore.ERROR_DESC.MALLOC, FALSE);
		return (wstr)xCore.sNull;
	}
	if ( sHead ) {
		memcpy(sRet, sHead, iHeadSize * sizeof(wchar_t));
	}
	for ( int i = 0; i < iLen; i++ ) {
		int idx = xrtRand(0, 61);		// 这里写 61，可以忽略 - 和 _ 字符
		sRet[iHeadSize + i] = RandStringDefaultTemplateW[idx];
	}
	if ( sFoot ) {
		memcpy(&sRet[iHeadSize + iLen], sFoot, iFootSize * sizeof(wchar_t));
	}
	sRet[iSize] = 0;
	return sRet;
}


/*
// 根据 文件夹 + 文件 合并完整的路径，自动补充文件夹末尾的斜杠（需要使用 xCore_free 释放内存）
XXAPI wstr Path_JoinW(wstr sPath, wstr sFile)
{
	if ( sFile == NULL ) { xCore.iRet = 0; return (wstr)xCore.nullstring; }
	wstr sRet;
	int iSize = wcslen(sPath);
	if ( (sPath == NULL) || (iSize == 0) ) {
		sPath = xCore_AppPathW();
		sRet = xCore_FormatW(L"%s%s", sPath, sFile);
		xCore_free(sPath);
	} else {
		if ( (sPath[iSize - 1] == L'/') || (sPath[iSize - 1] == L'\\') ) {
			sRet = xCore_FormatW(L"%s%s", sPath, sFile);
		} else {
			sRet = xCore_FormatW(L"%s\\%s", sPath, sFile);
		}
	}
	return sRet;
}
XXAPI astr Path_JoinA(astr sPath, astr sFile)
{
	if ( sFile == NULL ) { xCore.iRet = 0; return (astr)xCore.nullstring; }
	astr sRet;
	int iSize = (sPath == NULL) ? 0 : strlen(sPath);
	if ( iSize == 0 ) {
		sPath = xCore_AppPathA();
		sRet = xCore_FormatA("%s%s", sPath, sFile);
		xCore_free(sPath);
	} else {
		if ( (sPath[iSize - 1] == '/') || (sPath[iSize - 1] == '\\') ) {
			sRet = xCore_FormatA("%s%s", sPath, sFile);
		} else {
			sRet = xCore_FormatA("%s\\%s", sPath, sFile);
		}
	}
	return sRet;
}



// 判断两个路径是否可以形成相对路径（不区分大小写，自动适配 / 和 \ 符号）
XXAPI int Path_RelLikeW(wstr sPath, wstr sFile)
{
	size_t iSize = wcslen(sPath);
	int bRet = TRUE;
	for ( int i = 0; i < iSize; i++ ) {
		if ( (sPath[i] == '/') || (sPath[i] == '\\') ) {
			if ( (sFile[i] != '/') && (sFile[i] != '\\') ) {
				bRet = FALSE;
				break;
			}
		} else if ( tolower(sPath[i]) != tolower(sFile[i]) ) {
			bRet = FALSE;
			break;
		}
	}
	return bRet;
}
XXAPI int Path_RelLikeA(astr sPath, astr sFile)
{
	size_t iSize = strlen(sPath);
	int bRet = TRUE;
	for ( int i = 0; i < iSize; i++ ) {
		if ( (sPath[i] == '/') || (sPath[i] == '\\') ) {
			if ( (sFile[i] != '/') && (sFile[i] != '\\') ) {
				bRet = FALSE;
				break;
			}
		} else if ( tolower(sPath[i]) != tolower(sFile[i]) ) {
			bRet = FALSE;
			break;
		}
	}
	return bRet;
}
*/

