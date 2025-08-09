


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



// 判断是否为相对路径
XXAPI int xrtPathIsRel(ustr sPath, size_t iSize)
{
	if ( xrtPathIsAbs(sPath, iSize) ) {
		return FALSE;
	} else {
		return TRUE;
	}
}
XXAPI int xrtPathIsRelW(wstr sPath, size_t iSize)
{
	if ( xrtPathIsAbsW(sPath, iSize) ) {
		return FALSE;
	} else {
		return TRUE;
	}
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



// 拼接路径（ 需要使用 xrtFree 释放内存 ）
XXAPI ustr xrtPathJoin(uint iCount, ...)
{
	if ( iCount == 0 ) { return (ustr)xCore.sNull; }
	ustr sRet = xrtMalloc(4096);
	if ( sRet == NULL ) {
		xrtSetError(xCore.ERROR_DESC.MALLOC, FALSE);
		return (ustr)xCore.sNull;
	}
	va_list args;
	va_start(args, iCount);
	size_t iPos = 0;
	for ( int i = 0; i < iCount; i++ ) {
		ustr sPath = va_arg(args, ustr);
		size_t iSize = va_arg(args, size_t);
		if ( sPath == NULL ) { continue; }
		if ( iSize == 0 ) { iSize = strlen(sPath); }
		if ( iSize == 0 ) { continue; }
		if ( (iPos + iSize) > 4094 ) { xrtFree(sRet); return (ustr)xCore.sNull; }
		memcpy(&sRet[iPos], sPath, iSize);
		iPos += iSize;
		if ( i < (iCount - 1) ) {
			if ( (sRet[iPos-1] != L'\\') && (sRet[iSize-1] != L'/') ) {
				#if defined(_WIN32) || defined(_WIN64)
					sRet[iPos] = L'\\';
				#else
					sRet[iPos] = L'/';
				#endif
				iPos++;
			}
		}
	}
	va_end(args);
	ustr sRetTrim = xrtMalloc(iPos + 1);
	if ( sRetTrim == NULL ) {
		return sRet;
	}
	memcpy(sRetTrim, sRet, iPos);
	xrtFree(sRet);
	sRetTrim[iPos] = 0;
	return sRetTrim;
}
XXAPI wstr xrtPathJoinW(uint iCount, ...)
{
	if ( iCount == 0 ) { return (wstr)xCore.sNull; }
	wstr sRet = xrtMalloc(4096 * sizeof(wchar_t));
	if ( sRet == NULL ) {
		xrtSetError(xCore.ERROR_DESC.MALLOC, FALSE);
		return (wstr)xCore.sNull;
	}
	va_list args;
	va_start(args, iCount);
	size_t iPos = 0;
	for ( int i = 0; i < iCount; i++ ) {
		wstr sPath = va_arg(args, wstr);
		size_t iSize = va_arg(args, size_t);
		if ( sPath == NULL ) { continue; }
		if ( iSize == 0 ) { iSize = wcslen(sPath); }
		if ( iSize == 0 ) { continue; }
		if ( (iPos + iSize) > 4094 ) { xrtFree(sRet); return (wstr)xCore.sNull; }
		memcpy(&sRet[iPos], sPath, iSize * sizeof(wchar_t));
		iPos += iSize;
		if ( i < (iCount - 1) ) {
			if ( (sRet[iPos-1] != L'\\') && (sRet[iSize-1] != L'/') ) {
				#if defined(_WIN32) || defined(_WIN64)
					sRet[iPos] = L'\\';
				#else
					sRet[iPos] = L'/';
				#endif
				iPos++;
			}
		}
	}
	va_end(args);
	wstr sRetTrim = xrtMalloc((iPos + 1) * sizeof(wchar_t));
	if ( sRetTrim == NULL ) {
		return sRet;
	}
	memcpy(sRetTrim, sRet, iPos * sizeof(wchar_t));
	xrtFree(sRet);
	sRetTrim[iPos] = 0;
	return sRetTrim;
}


