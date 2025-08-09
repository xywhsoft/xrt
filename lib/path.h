


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


/*
// 获取随机不存在的路径
XXAPI wstr Path_RandomW(wstr sHead, wstr sFoot, int iRand)
{
	int iSizeHead = wcslen(sHead);
	int iSize = iSizeHead + wcslen(sFoot) + iRand + 1;
	wstr sRet = malloc(iSize * sizeof(wchar_t));
	wcscpy(sRet, sHead);
	for ( int i = 0; i < iRand; i++ ) {
		int iChar = xCore_Rand(0x30, 0x5A - 7);
		if ( iChar > 0x39 ) { iChar += 7; }
		sRet[iSizeHead + i] = iChar;
	}
	wcscpy(&sRet[iSizeHead + iRand], sFoot);
	sRet[iSize - 1] = 0;
	return sRet;
}
XXAPI astr Path_RandomA(astr sHead, astr sFoot, int iRand)
{
	int iSizeHead = strlen(sHead);
	int iSize = iSizeHead + strlen(sFoot) + iRand + 1;
	astr sRet = malloc(iSize);
	strcpy(sRet, sHead);
	for ( int i = 0; i < iRand; i++ ) {
		int iChar = xCore_Rand(0x30, 0x5A - 7);
		if ( iChar > 0x39 ) { iChar += 7; }
		sRet[iSizeHead + i] = iChar;
	}
	strcpy(&sRet[iSizeHead + iRand], sFoot);
	sRet[iSize - 1] = 0;
	return sRet;
}



// 文件路径合法性检查（返回 TRUE 代表路径有问题）
XXAPI int Path_SafeCheckW(wstr sPath, int bRepair)
{
	if ( sPath == NULL ) { return TRUE; }
	wstr sNewPath = xCore_ReplaceW(sPath, L"/", L"\\");
	wstr* arrText = xCore_SplitCharW(sNewPath, L'\\', TRUE);
	int iCount = xCore.iRetSize;
	int bRet = FALSE;
	if ( bRepair ) { sPath[0] = 0; }
	for ( int i = 0; i < iCount; i++ ) {
		xxTrimW(arrText[i], L" .", TRUE);					// 过滤文件开头和末尾的空格和 . 符号
		int bSize = xCore.iRetSize;
		xxStringFilterW(arrText[i], L"*\"<>|", TRUE);		// 去除不允许存在的字符（ : 和 ? 不过滤）
		if ( bSize || xCore.iRetSize ) { bRet = TRUE; }
		if ( bRepair ) {
			wcscat(sPath, arrText[i]);
			if ( i + 1 != iCount ) { wcscat(sPath, L"\\"); }
		}
	}
	xCore_free(arrText);
	return bRet;
}
XXAPI int Path_SafeCheckA(astr sPath, int bRepair)
{
	wstr sPathW = xCore_M2W(sPath, CP_ACP, 0);
	int bRet = Path_SafeCheckW(sPathW, bRepair);
	if ( bRepair ) {
		astr sNewPath = xCore_W2M(sPathW, CP_ACP, 0);
		strcpy(sPath, sNewPath);
		xCore_free(sNewPath);
	}
	xCore_free(sPathW);
	return bRet;
}



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
	int bRet = -1;
	for ( int i = 0; i < iSize; i++ ) {
		if ( (sPath[i] == '/') || (sPath[i] == '\\') ) {
			if ( (sFile[i] != '/') && (sFile[i] != '\\') ) {
				bRet = 0;
				break;
			}
		} else if ( tolower(sPath[i]) != tolower(sFile[i]) ) {
			bRet = 0;
			break;
		}
	}
	return bRet;
}
XXAPI int Path_RelLikeA(astr sPath, astr sFile)
{
	size_t iSize = strlen(sPath);
	int bRet = -1;
	for ( int i = 0; i < iSize; i++ ) {
		if ( (sPath[i] == '/') || (sPath[i] == '\\') ) {
			if ( (sFile[i] != '/') && (sFile[i] != '\\') ) {
				bRet = 0;
				break;
			}
		} else if ( tolower(sPath[i]) != tolower(sFile[i]) ) {
			bRet = 0;
			break;
		}
	}
	return bRet;
}
*/

