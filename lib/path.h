


// 通过路径获取文件名 + 扩展名（ 需使用 xrtFree 释放内存 ）
XXAPI str xrtPathGetNameExt(str sPath, size_t iSize)
{
	if ( sPath == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen((const char*)sPath); }
	if ( iSize == 0 ) { return xCore.sNull; }
	for ( size_t i = iSize; i-- > 0; ) {
		if ( (sPath[i] == L'/') || (sPath[i] == L'\\') ) {
			if ( i == (iSize - 1u) ) {
				return xCore.sNull;
			} else {
				return xrtCopyStr(&sPath[i + 1], iSize - i - 1);
			}
		}
	}
	return xrtCopyStr(sPath, iSize);
}



// 通过路径获取文件名（ 需使用 xrtFree 释放内存 ）
XXAPI str xrtPathGetName(str sPath, size_t iSize)
{
	if ( sPath == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen((const char*)sPath); }
	if ( iSize == 0 ) { return xCore.sNull; }
	size_t iPointPos = 0;
	for ( size_t i = iSize; i-- > 0; ) {
		if ( sPath[i] == L'.' ) {
			iPointPos = iSize - i;
		} else if ( (sPath[i] == L'/') || (sPath[i] == L'\\') ) {
			if ( i == (iSize - 1u) ) {
				return xCore.sNull;
			} else {
				return xrtCopyStr(&sPath[i + 1], iSize - i - iPointPos - 1);
			}
		}
	}
	return xrtCopyStr(sPath, iSize);
}



// 通过路径获取扩展名（ 需使用 xrtFree 释放内存 ）
XXAPI str xrtPathGetExt(str sPath, size_t iSize)
{
	if ( sPath == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen((const char*)sPath); }
	if ( iSize == 0 ) { return xCore.sNull; }
	for ( size_t i = iSize; i-- > 0; ) {
		if ( sPath[i] == L'.' ) {
			return xrtCopyStr(&sPath[i + 1], iSize - i - 1);
		} else if ( (sPath[i] == L'/') || (sPath[i] == L'\\') ) {
			return xCore.sNull;
		}
	}
	return xCore.sNull;
}



// 通过路径获取文件夹（ 需使用 xrtFree 释放内存 ）
XXAPI str xrtPathGetDir(str sPath, size_t iSize)
{
	if ( sPath == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen((const char*)sPath); }
	if ( iSize == 0 ) { return xCore.sNull; }
	for ( size_t i = iSize; i-- > 0; ) {
		if ( (sPath[i] == L'/') || (sPath[i] == L'\\') ) {
			if ( i == (iSize - 1u) ) {
				return xrtCopyStr(sPath, iSize - 1);
			} else {
				return xrtCopyStr(sPath, i);
			}
		}
	}
	return xCore.sNull;
}



// 判断是否为绝对路径（Linux 系统以 / 开头为绝对路径，Windows系统含 : 为绝对路径）
XXAPI bool xrtPathIsAbs(str sPath, size_t iSize)
{
	if ( sPath == NULL ) { return FALSE; }
	if ( iSize == 0 ) { iSize = strlen((const char*)sPath); }
	if ( iSize == 0 ) { return FALSE; }
	if ( sPath[0] == '/' ) {
		return TRUE;
	}
	for ( size_t i = 0; i < iSize; i++ ) {
		if ( sPath[i] == ':' ) {
			return TRUE;
		}
	}
	return FALSE;
}



// 获取随机不存在的路径（ 需使用 xrtFree 释放内存 ）
XXAPI str xrtPathRandom(str sHead, size_t iHeadSize, str sFoot, size_t iFootSize, size_t iLen)
{
	// 1. 计算并处理前后缀长度
	if ( sHead ) {
		if ( iHeadSize == 0 ) { iHeadSize = strlen((const char*)sHead); }
		if ( iHeadSize == 0 ) { sHead = NULL; }
	} else {
		iHeadSize = 0;
	}
	if ( sFoot ) {
		if ( iFootSize == 0 ) { iFootSize = strlen((const char*)sFoot); }
		if ( iFootSize == 0 ) { sFoot = NULL; }
	} else {
		iFootSize = 0;
	}
	// 2. 分配结果缓冲区
	int iSize = iHeadSize + iFootSize + iLen;
	if ( iSize == 0 ) { return xCore.sNull; }
	str sRet = xrtMalloc(iSize + 1);
	if ( sRet == NULL ) {
		return xCore.sNull;
	}
	// 3. 拼接：前缀 + 随机字符 + 后缀
	if ( sHead ) {
		memcpy(sRet, sHead, iHeadSize);
	}
	for ( size_t i = 0; i < iLen; i++ ) {
		int idx = xrtRandRange(0, 61);		// 这里写 61，可以忽略 - 和 _ 字符
		sRet[iHeadSize + i] = RandStringDefaultTemplate[idx];
	}
	if ( sFoot ) {
		memcpy(&sRet[iHeadSize + iLen], sFoot, iFootSize);
	}
	sRet[iSize] = 0;
	return sRet;
}



// 拼接路径（ 需要使用 xrtFree 释放内存 ）
XXAPI str xrtPathJoin(uint iCount, ...)
{
	if ( iCount == 0 ) { return xCore.sNull; }
	// 1. 预分配 4096 字节缓冲区
	str sRet = xrtMalloc(4096);
	if ( sRet == NULL ) {
		return xCore.sNull;
	}
	// 2. 逐个拼接路径段，自动补充路径分隔符
	va_list args;
	va_start(args, iCount);
	size_t iPos = 0;
	for ( uint i = 0; i < iCount; i++ ) {
		str sPath = va_arg(args, str);
		if ( sPath == NULL ) { continue; }
		size_t iSize = strlen((const char*)sPath);
		if ( iSize == 0 ) { continue; }
		if ( (iPos + iSize) > 4094 ) { xrtFree(sRet); return xCore.sNull; }
		memcpy(&sRet[iPos], sPath, iSize);
		iPos += iSize;
		if ( i < (iCount - 1) ) {
			if ( (sRet[iPos - 1] != '\\') && (sRet[iPos - 1] != '/') ) {
				#if defined(_WIN32) || defined(_WIN64)
					sRet[iPos] = '\\';
				#else
					sRet[iPos] = '/';
				#endif
				iPos++;
			}
		}
	}
	va_end(args);
	// 3. 缩减为实际大小的字符串
	if ( iPos > 4000 ) {
		sRet[iPos] = 0;
		return sRet;
	} else {
		str sRetTrim = xrtMalloc(iPos + 1);
		if ( sRetTrim == NULL ) {
			sRet[iPos] = 0;
			return sRet;
		} else {
			memcpy(sRetTrim, sRet, iPos);
			xrtFree(sRet);
			sRetTrim[iPos] = 0;
			return sRetTrim;
		}
	}
}



typedef struct __xrtPathPart {
	size_t iStart;
	size_t iSize;
} __xrtPathPart;



static int __xrtPathIsSep(char c)
{
	return c == '/' || c == '\\';
}



static char __xrtPathSep()
{
	#if defined(_WIN32) || defined(_WIN64)
		return '\\';
	#else
		return '/';
	#endif
}



static size_t __xrtPathRootSize(const char* sPath, size_t iSize)
{
	if ( sPath == NULL || iSize == 0 ) { return 0; }
	if ( iSize >= 2 && isalpha((unsigned char)sPath[0]) && sPath[1] == ':' ) {
		if ( iSize >= 3 && __xrtPathIsSep(sPath[2]) ) {
			return 3;
		}
		return 2;
	}
	if ( iSize >= 2 && __xrtPathIsSep(sPath[0]) && __xrtPathIsSep(sPath[1]) ) {
		return 2;
	}
	if ( __xrtPathIsSep(sPath[0]) ) {
		return 1;
	}
	return 0;
}



static int __xrtPathPartIsDotDot(const char* sPath, const __xrtPathPart* pPart)
{
	return pPart != NULL &&
		   pPart->iSize == 2 &&
		   sPath[pPart->iStart] == '.' &&
		   sPath[pPart->iStart + 1] == '.';
}



static int __xrtPathPartEquals(const char* a, const __xrtPathPart* pa, const char* b, const __xrtPathPart* pb)
{
	size_t i;
	if ( pa == NULL || pb == NULL || pa->iSize != pb->iSize ) {
		return FALSE;
	}
	for ( i = 0; i < pa->iSize; ++i ) {
		char ca = a[pa->iStart + i];
		char cb = b[pb->iStart + i];
		#if defined(_WIN32) || defined(_WIN64)
			ca = (char)tolower((unsigned char)ca);
			cb = (char)tolower((unsigned char)cb);
		#endif
		if ( ca != cb ) {
			return FALSE;
		}
	}
	return TRUE;
}



static int __xrtPathRootEquals(const char* a, size_t aSize, const char* b, size_t bSize)
{
	size_t i;
	if ( aSize != bSize ) { return FALSE; }
	for ( i = 0; i < aSize; ++i ) {
		char ca = __xrtPathIsSep(a[i]) ? __xrtPathSep() : a[i];
		char cb = __xrtPathIsSep(b[i]) ? __xrtPathSep() : b[i];
		#if defined(_WIN32) || defined(_WIN64)
			ca = (char)tolower((unsigned char)ca);
			cb = (char)tolower((unsigned char)cb);
		#endif
		if ( ca != cb ) {
			return FALSE;
		}
	}
	return TRUE;
}



static size_t __xrtPathCollectParts(const char* sPath, size_t iSize, size_t iRootSize, __xrtPathPart* pParts, size_t iPartMax)
{
	size_t i = iRootSize;
	size_t iCount = 0;
	while ( i < iSize ) {
		size_t iStart;
		size_t iLen;
		while ( i < iSize && __xrtPathIsSep(sPath[i]) ) { ++i; }
		iStart = i;
		while ( i < iSize && !__xrtPathIsSep(sPath[i]) ) { ++i; }
		iLen = i - iStart;
		if ( iLen == 0 ) { continue; }
		if ( iCount < iPartMax ) {
			pParts[iCount].iStart = iStart;
			pParts[iCount].iSize = iLen;
		}
		++iCount;
	}
	return iCount;
}



XXAPI str xrtPathSep()
{
	char sSep[2];
	sSep[0] = __xrtPathSep();
	sSep[1] = 0;
	return xrtCopyStr((str)sSep, 1);
}



XXAPI str xrtPathNormalize(str sPath, size_t iSize)
{
	size_t iRootSize;
	size_t iPartMax;
	__xrtPathPart* pParts;
	size_t iCount = 0;
	size_t i;
	size_t iRetSize;
	str sRet;
	char cSep;

	if ( sPath == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen((const char*)sPath); }
	if ( iSize == 0 ) { return xrtCopyStr((str)".", 1); }

	iRootSize = __xrtPathRootSize((const char*)sPath, iSize);
	iPartMax = iSize / 2 + 2;
	pParts = (__xrtPathPart*)xrtMalloc(sizeof(__xrtPathPart) * iPartMax);
	if ( pParts == NULL ) { return xCore.sNull; }

	i = iRootSize;
	while ( i < iSize ) {
		size_t iStart;
		size_t iLen;
		while ( i < iSize && __xrtPathIsSep(sPath[i]) ) { ++i; }
		iStart = i;
		while ( i < iSize && !__xrtPathIsSep(sPath[i]) ) { ++i; }
		iLen = i - iStart;
		if ( iLen == 0 || (iLen == 1 && sPath[iStart] == '.') ) {
			continue;
		}
		if ( iLen == 2 && sPath[iStart] == '.' && sPath[iStart + 1] == '.' ) {
			if ( iCount > 0 && !__xrtPathPartIsDotDot((const char*)sPath, &pParts[iCount - 1]) ) {
				--iCount;
				continue;
			}
			if ( iRootSize > 0 ) {
				continue;
			}
		}
		pParts[iCount].iStart = iStart;
		pParts[iCount].iSize = iLen;
		++iCount;
	}

	cSep = __xrtPathSep();
	if ( iRootSize == 0 && iCount == 0 ) {
		xrtFree(pParts);
		return xrtCopyStr((str)".", 1);
	}

	iRetSize = 0;
	if ( iRootSize > 0 ) {
		iRetSize += iRootSize;
		if ( iCount > 0 && !__xrtPathIsSep(sPath[iRootSize - 1]) ) {
			++iRetSize;
		}
	}
	for ( i = 0; i < iCount; ++i ) {
		if ( i > 0 || (iRootSize > 0 && !__xrtPathIsSep(sPath[iRootSize - 1])) ) {
			++iRetSize;
		}
		iRetSize += pParts[i].iSize;
	}

	sRet = (str)xrtMalloc(iRetSize + 1);
	if ( sRet == NULL ) {
		xrtFree(pParts);
		return xCore.sNull;
	}

	iRetSize = 0;
	if ( iRootSize > 0 ) {
		size_t j;
		for ( j = 0; j < iRootSize; ++j ) {
			sRet[iRetSize++] = __xrtPathIsSep(sPath[j]) ? cSep : sPath[j];
		}
		if ( iCount > 0 && !__xrtPathIsSep(sRet[iRetSize - 1]) ) {
			sRet[iRetSize++] = cSep;
		}
	}
	for ( i = 0; i < iCount; ++i ) {
		if ( iRetSize > 0 && !__xrtPathIsSep(sRet[iRetSize - 1]) ) {
			sRet[iRetSize++] = cSep;
		}
		memcpy(&sRet[iRetSize], &sPath[pParts[i].iStart], pParts[i].iSize);
		iRetSize += pParts[i].iSize;
	}
	sRet[iRetSize] = 0;
	xrtFree(pParts);
	return sRet;
}



XXAPI str xrtPathCwd()
{
	#if defined(_WIN32) || defined(_WIN64)
		DWORD iNeed = GetCurrentDirectoryW(0, NULL);
		wchar_t* sPathW;
		str sRet;
		if ( iNeed == 0 ) { return xCore.sNull; }
		sPathW = (wchar_t*)xrtMalloc(sizeof(wchar_t) * (iNeed + 1));
		if ( sPathW == NULL ) { return xCore.sNull; }
		if ( GetCurrentDirectoryW(iNeed + 1, sPathW) == 0 ) {
			xrtFree(sPathW);
			return xCore.sNull;
		}
		sRet = (str)xrtUTF16to8((u16str)sPathW, 0, NULL);
		xrtFree(sPathW);
		return sRet;
	#else
		char sBuff[PATH_MAX];
		if ( getcwd(sBuff, sizeof(sBuff)) == NULL ) {
			return xCore.sNull;
		}
		return xrtCopyStr(sBuff, 0);
	#endif
}



XXAPI bool xrtPathSetCwd(str sPath)
{
	if ( sPath == NULL ) { return FALSE; }
	#if defined(_WIN32) || defined(_WIN64)
		u16str sPathW = xrtUTF8to16((u8str)sPath, 0, NULL);
		BOOL bRet = SetCurrentDirectoryW((const wchar_t*)sPathW);
		xrtFree(sPathW);
		return bRet ? TRUE : FALSE;
	#else
		return chdir((const char*)sPath) == 0 ? TRUE : FALSE;
	#endif
}



XXAPI str xrtPathAbs(str sPath, size_t iSize)
{
	str sCwd;
	str sJoin;
	str sRet;
	if ( sPath == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen((const char*)sPath); }
	if ( xrtPathIsAbs(sPath, iSize) ) {
		return xrtPathNormalize(sPath, iSize);
	}
	sCwd = xrtPathCwd();
	if ( sCwd == NULL || sCwd[0] == 0 ) {
		return xrtPathNormalize(sPath, iSize);
	}
	sJoin = xrtPathJoin(2, sCwd, sPath);
	xrtFree(sCwd);
	sRet = xrtPathNormalize(sJoin, 0);
	xrtFree(sJoin);
	return sRet;
}



XXAPI str xrtPathRel(str sBase, str sTarget)
{
	str sBaseAbs;
	str sTargetAbs;
	size_t iBaseSize;
	size_t iTargetSize;
	size_t iBaseRoot;
	size_t iTargetRoot;
	size_t iPartMax;
	__xrtPathPart* pBaseParts;
	__xrtPathPart* pTargetParts;
	size_t iBaseCount;
	size_t iTargetCount;
	size_t iCommon = 0;
	size_t i;
	size_t iRetSize = 0;
	str sRet;
	char cSep;

	if ( sBase == NULL || sTarget == NULL ) { return xCore.sNull; }
	sBaseAbs = xrtPathAbs(sBase, 0);
	sTargetAbs = xrtPathAbs(sTarget, 0);
	if ( sBaseAbs == NULL || sTargetAbs == NULL ) {
		xrtFree(sBaseAbs);
		return sTargetAbs;
	}

	iBaseSize = strlen((const char*)sBaseAbs);
	iTargetSize = strlen((const char*)sTargetAbs);
	iBaseRoot = __xrtPathRootSize((const char*)sBaseAbs, iBaseSize);
	iTargetRoot = __xrtPathRootSize((const char*)sTargetAbs, iTargetSize);
	if ( !__xrtPathRootEquals((const char*)sBaseAbs, iBaseRoot, (const char*)sTargetAbs, iTargetRoot) ) {
		xrtFree(sBaseAbs);
		return sTargetAbs;
	}

	iPartMax = (iBaseSize + iTargetSize) / 2 + 4;
	pBaseParts = (__xrtPathPart*)xrtMalloc(sizeof(__xrtPathPart) * iPartMax);
	pTargetParts = (__xrtPathPart*)xrtMalloc(sizeof(__xrtPathPart) * iPartMax);
	if ( pBaseParts == NULL || pTargetParts == NULL ) {
		xrtFree(pBaseParts);
		xrtFree(pTargetParts);
		xrtFree(sBaseAbs);
		return sTargetAbs;
	}

	iBaseCount = __xrtPathCollectParts((const char*)sBaseAbs, iBaseSize, iBaseRoot, pBaseParts, iPartMax);
	iTargetCount = __xrtPathCollectParts((const char*)sTargetAbs, iTargetSize, iTargetRoot, pTargetParts, iPartMax);
	while ( iCommon < iBaseCount && iCommon < iTargetCount &&
			__xrtPathPartEquals((const char*)sBaseAbs, &pBaseParts[iCommon], (const char*)sTargetAbs, &pTargetParts[iCommon]) ) {
		++iCommon;
	}
	if ( iCommon == iBaseCount && iCommon == iTargetCount ) {
		xrtFree(pBaseParts);
		xrtFree(pTargetParts);
		xrtFree(sBaseAbs);
		xrtFree(sTargetAbs);
		return xrtCopyStr((str)".", 1);
	}

	for ( i = iCommon; i < iBaseCount; ++i ) {
		if ( iRetSize > 0 ) { ++iRetSize; }
		iRetSize += 2;
	}
	for ( i = iCommon; i < iTargetCount; ++i ) {
		if ( iRetSize > 0 ) { ++iRetSize; }
		iRetSize += pTargetParts[i].iSize;
	}

	sRet = (str)xrtMalloc(iRetSize + 1);
	if ( sRet == NULL ) {
		xrtFree(pBaseParts);
		xrtFree(pTargetParts);
		xrtFree(sBaseAbs);
		return sTargetAbs;
	}
	cSep = __xrtPathSep();
	iRetSize = 0;
	for ( i = iCommon; i < iBaseCount; ++i ) {
		if ( iRetSize > 0 ) { sRet[iRetSize++] = cSep; }
		sRet[iRetSize++] = '.';
		sRet[iRetSize++] = '.';
	}
	for ( i = iCommon; i < iTargetCount; ++i ) {
		if ( iRetSize > 0 ) { sRet[iRetSize++] = cSep; }
		memcpy(&sRet[iRetSize], &sTargetAbs[pTargetParts[i].iStart], pTargetParts[i].iSize);
		iRetSize += pTargetParts[i].iSize;
	}
	sRet[iRetSize] = 0;

	xrtFree(pBaseParts);
	xrtFree(pTargetParts);
	xrtFree(sBaseAbs);
	xrtFree(sTargetAbs);
	return sRet;
}



XXAPI str xrtPathHome()
{
	#if defined(_WIN32) || defined(_WIN64)
		DWORD iNeed = GetEnvironmentVariableW(L"USERPROFILE", NULL, 0);
		wchar_t* sPathW;
		str sRet;
		if ( iNeed > 0 ) {
			sPathW = (wchar_t*)xrtMalloc(sizeof(wchar_t) * (iNeed + 1));
			if ( sPathW != NULL && GetEnvironmentVariableW(L"USERPROFILE", sPathW, iNeed + 1) > 0 ) {
				sRet = (str)xrtUTF16to8((u16str)sPathW, 0, NULL);
				xrtFree(sPathW);
				return sRet;
			}
			xrtFree(sPathW);
		}
		return xrtPathCwd();
	#else
		const char* sHome = getenv("HOME");
		if ( sHome == NULL || sHome[0] == 0 ) {
			return xrtPathCwd();
		}
		return xrtCopyStr((str)sHome, 0);
	#endif
}



XXAPI str xrtPathTemp()
{
	#if defined(_WIN32) || defined(_WIN64)
		DWORD iNeed = GetTempPathW(0, NULL);
		wchar_t* sPathW;
		str sText;
		str sRet;
		if ( iNeed == 0 ) { return xrtPathCwd(); }
		sPathW = (wchar_t*)xrtMalloc(sizeof(wchar_t) * (iNeed + 2));
		if ( sPathW == NULL ) { return xCore.sNull; }
		if ( GetTempPathW(iNeed + 1, sPathW) == 0 ) {
			xrtFree(sPathW);
			return xrtPathCwd();
		}
		sText = (str)xrtUTF16to8((u16str)sPathW, 0, NULL);
		xrtFree(sPathW);
		sRet = xrtPathNormalize(sText, 0);
		xrtFree(sText);
		return sRet;
	#else
		const char* sTmp = getenv("TMPDIR");
		if ( sTmp == NULL || sTmp[0] == 0 ) {
			sTmp = "/tmp";
		}
		return xrtPathNormalize((str)sTmp, 0);
	#endif
}



XXAPI str xrtPathAppDir()
{
	if ( xCore.AppPath != NULL && xCore.AppPath[0] != 0 ) {
		return xrtCopyStr(xCore.AppPath, 0);
	}
	return xrtPathCwd();
}



XXAPI str xrtPathWithExt(str sPath, str sExt)
{
	size_t iSize;
	size_t iExtSize;
	size_t iDot = 0;
	size_t iLastSep = 0;
	size_t iStemSize;
	size_t iRetSize;
	str sRet;

	if ( sPath == NULL ) { return xCore.sNull; }
	iSize = strlen((const char*)sPath);
	iExtSize = sExt != NULL ? strlen((const char*)sExt) : 0;
	for ( size_t i = 0; i < iSize; ++i ) {
		if ( __xrtPathIsSep(sPath[i]) ) {
			iLastSep = i + 1;
			iDot = 0;
		} else if ( sPath[i] == '.' ) {
			iDot = i;
		}
	}
	iStemSize = iDot > iLastSep ? iDot : iSize;
	iRetSize = iStemSize + iExtSize + ((iExtSize > 0 && sExt[0] != '.') ? 1 : 0);
	sRet = (str)xrtMalloc(iRetSize + 1);
	if ( sRet == NULL ) { return xCore.sNull; }
	memcpy(sRet, sPath, iStemSize);
	iRetSize = iStemSize;
	if ( iExtSize > 0 ) {
		if ( sExt[0] != '.' ) {
			sRet[iRetSize++] = '.';
		}
		memcpy(&sRet[iRetSize], sExt, iExtSize);
		iRetSize += iExtSize;
	}
	sRet[iRetSize] = 0;
	return sRet;
}



XXAPI str xrtPathWithName(str sPath, str sNameExt)
{
	str sDir;
	str sRet;
	if ( sNameExt == NULL ) { return xCore.sNull; }
	if ( sPath == NULL || sPath[0] == 0 ) { return xrtCopyStr(sNameExt, 0); }
	sDir = xrtPathGetDir(sPath, 0);
	if ( sDir == NULL || sDir[0] == 0 ) {
		xrtFree(sDir);
		return xrtCopyStr(sNameExt, 0);
	}
	sRet = xrtPathJoin(2, sDir, sNameExt);
	xrtFree(sDir);
	return sRet;
}


