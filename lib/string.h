


// 创建字符串副本（需使用 xrtFree 释放）
XXAPI ustr xrtCopyString(ustr sText, size_t iSize)
{
	if ( sText == NULL ) { return (ustr)xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { return (ustr)xCore.sNull; }
	ustr sRet = xrtMalloc(iSize + 1);
	if ( sRet == NULL ) { return (ustr)xCore.sNull; }
	memcpy(sRet, sText, iSize);
	sRet[iSize] = 0;
	return sRet;
}
XXAPI wstr xrtCopyStringW(wstr sText, size_t iSize)
{
	if ( sText == NULL ) { return (wstr)xCore.sNull; }
	if ( iSize == 0 ) { iSize = wcslen(sText); }
	if ( iSize == 0 ) { return (wstr)xCore.sNull; }
	wstr sRet = xrtMalloc((iSize + 1) * sizeof(wchar_t));
	if ( sRet == 0 ) { return (wstr)xCore.sNull; }
	memcpy(sRet, sText, iSize * sizeof(wchar_t));
	sRet[iSize] = 0;
	return sRet;
}



// 字符串转为小写（bSrcRevise 为 false 时，需使用 xrtFree 释放内存）
XXAPI ustr xrtLCase(ustr sText, size_t iSize, int bSrcRevise)
{
	if ( sText == NULL ) { return (ustr)xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { return (ustr)xCore.sNull; }
	ustr sRet;
	if ( bSrcRevise ) { sRet = sText; } else { sRet = xrtCopyString(sText, iSize); }
	for ( int i = 0; i < iSize; i++ ) {
		if ( sRet[i] & 0x80 ) {
			// 跳过 OEM 编码字符
			i++;
		} else {
			sRet[i] = tolower(sRet[i]);
		}
	}
	return sRet;
}
XXAPI wstr xrtLCaseW(wstr sText, size_t iSize, int bSrcRevise)
{
	if ( sText == NULL ) { return (wstr)xCore.sNull; }
	if ( iSize == 0 ) { iSize = wcslen(sText); }
	if ( iSize == 0 ) { return (wstr)xCore.sNull; }
	wstr sRet;
	if ( bSrcRevise ) { sRet = sText; } else { sRet = xrtCopyStringW(sText, iSize); }
	for ( int i = 0; i < iSize; i++ ) {
		sRet[i] = tolower(sRet[i]);
	}
	return sRet;
}



// 字符串转为大写（bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存）
XXAPI ustr xrtUCase(ustr sText, size_t iSize, int bSrcRevise)
{
	if ( sText == NULL ) { return (ustr)xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { return (ustr)xCore.sNull; }
	ustr sRet;
	if ( bSrcRevise != FALSE ) { sRet = sText; } else { sRet = xrtCopyString(sText, iSize); }
	for ( int i = 0; i < iSize; i++ ) {
		if ( sRet[i] & 0x80 ) {
			// 跳过 OEM 编码字符
			i++;
		} else {
			sRet[i] = toupper(sRet[i]);
		}
	}
	return sRet;
}
XXAPI wstr xrtUCaseW(wstr sText, size_t iSize, int bSrcRevise)
{
	if ( sText == NULL ) { return (wstr)xCore.sNull; }
	if ( iSize == 0 ) { iSize = wcslen(sText); }
	if ( iSize == 0 ) { return (wstr)xCore.sNull; }
	wstr sRet;
	if ( bSrcRevise != FALSE ) { sRet = sText; } else { sRet = xrtCopyStringW(sText, iSize); }
	for ( int i = 0; i < iSize; i++ ) {
		sRet[i] = toupper(sRet[i]);
	}
	return sRet;
}



// 搜索字符串（没找到字符串的情况下会返回 NULL）
XXAPI ustr xrtFindStr(ustr sText, size_t iSize, ustr sSubText, size_t iSubSize, int bCase)
{
	if ( sText == NULL ) { xCore.iRet = 0; return NULL; }
	if ( sSubText == NULL ) { xCore.iRet = 0; return NULL; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { xCore.iRet = 0; return NULL; }
	if ( iSubSize == 0 ) { iSubSize = strlen(sSubText); }
	if ( iSubSize == 0 ) { xCore.iRet = 0; return NULL; }
	ustr sSub;
	if ( bCase ) {
		ustr sText1 = xrtLCase(sText, 0, FALSE);
		ustr sText2 = xrtLCase(sSubText, 0, FALSE);
		sSub = memmem(sText1, iSize, sText2, iSubSize);
		if ( sSub ) {
			sSub = &sText[sSub - sText1];
		}
		free(sText1);
		free(sText2);
	} else {
		sSub = memmem(sText, iSize, sSubText, iSubSize);
	}
	if ( sSub ) {
		xCore.iRet = (sSub - sText) + 1;
		return sSub;
	} else {
		xCore.iRet = 0;
		return NULL;
	}
}
XXAPI uint xrtInStr(ustr sText, size_t iSize, ustr sSubText, size_t iSubSize, int bCase)
{
	xrtFindStr(sText, iSize, sSubText, iSubSize, bCase);
	return xCore.iRet;
}
XXAPI wstr xrtFindStrW(wstr sText, size_t iSize, wstr sSubText, size_t iSubSize, int bCase)
{
	if ( sText == NULL ) { xCore.iRet = 0; return NULL; }
	if ( sSubText == NULL ) { xCore.iRet = 0; return NULL; }
	if ( iSize == 0 ) { iSize = wcslen(sText); }
	if ( iSize == 0 ) { xCore.iRet = 0; return NULL; }
	if ( iSubSize == 0 ) { iSubSize = wcslen(sSubText); }
	if ( iSubSize == 0 ) { xCore.iRet = 0; return NULL; }
	wstr sSub;
	if ( bCase ) {
		wstr sText1 = xrtLCaseW(sText, 0, FALSE);
		wstr sText2 = xrtLCaseW(sSubText, 0, FALSE);
		sSub = memmem(sText1, iSize * 2, sText2, iSubSize * 2);
		if ( sSub ) {
			sSub = &sText[sSub - sText1];
		}
		free(sText1);
		free(sText2);
	} else {
		sSub = memmem(sText, iSize, sSubText, iSubSize);
	}
	if ( sSub ) {
		xCore.iRet = (sSub - sText) + 1;
		return sSub;
	} else {
		xCore.iRet = 0;
		return NULL;
	}
}
XXAPI uint xrtInStrW(wstr sText, size_t iSize, wstr sSubText, size_t iSubSize, int bCase)
{
	xrtFindStrW(sText, iSize, sSubText, iSubSize, bCase);
	return xCore.iRet;
}



// 字符串检查（ sText 中是否包含 sSubText 列出的字符，支持 utf-8 mb6 编码 ）
XXAPI ustr xrtCheckStr(ustr sText, size_t iSize, ustr sSubText, size_t iSubSize)
{
	if ( iSize == 0 ) {
		iSize = strlen(sText);
	}
	if ( iSubSize == 0 ) {
		iSubSize = strlen(sSubText);
	}
	if ( iSubSize == 0 ) {
		return NULL;
	}
	for ( int i = 0; i < iSize; i++ ) {
		if ( (sText[i] & 0b10000000) == 0 ) {
			// ASCII 兼容字符
			for ( int j = 0; j < iSubSize; j++ ) {
				if ( sSubText[j] == sText[i] ) {
					return &sText[i];
				}
			}
		} else if ( sText[i] & 0b11000000 == 0b11000000 ) {
			// 双字节字符
			size_t iLen = iSubSize - 1;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) ) {
					return &sText[i];
				}
			}
		} else if ( sText[i] & 0b11100000 == 0b11100000 ) {
			// 三字节字符
			size_t iLen = iSubSize - 2;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) ) {
					return &sText[i];
				}
			}
		} else if ( sText[i] & 0b11110000 == 0b11110000 ) {
			// 四字节字符
			size_t iLen = iSubSize - 3;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) ) {
					return &sText[i];
				}
			}
		} else if ( sText[i] & 0b11111000 == 0b11111000 ) {
			// 五字节字符
			size_t iLen = iSubSize - 4;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) && (sSubText[j+4] == sText[i+4]) ) {
					return &sText[i];
				}
			}
		} else if ( sText[i] & 0b11111100 == 0b11111100 ) {
			// 六字节字符
			size_t iLen = iSubSize - 5;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) && (sSubText[j+4] == sText[i+4]) && (sSubText[j+5] == sText[i+5]) ) {
					return &sText[i];
				}
			}
		} else {
			// 跳过异常字符（FE、FF）
		}
	}
	return NULL;
}
XXAPI wstr xrtCheckStrW(wstr sText, size_t iSize, wstr sSubText, size_t iSubSize)
{
	if ( iSize == 0 ) {
		iSize = wcslen(sText);
	}
	if ( iSize == 0 ) {
		return NULL;
	}
	if ( iSubSize == 0 ) {
		iSubSize = wcslen(sSubText);
	}
	if ( iSubSize == 0 ) {
		return NULL;
	}
	wchar_t sFindStr[2] = { 0, 0 };
	for ( int i = 0; i < iSize; i++ ) {
		sFindStr[0] = sText[i];
		if ( wcsstr(sSubText, sFindStr) ) {
			return &sText[i];
		}
	}
	return NULL;
}


/*
// 裁剪字符串（bSrcRevise 为 FALSE 时，需使用 xCore.free 释放内存）
XXAPI ustr xrtLTrim(ustr sText, size_t iSize, ustr sSub, size_t iSubSize, int bSrcRevise)
{
	wstr sTextW = xCore_M2W(sText, CP_UTF8, 0);
	wstr sSubW = xCore_M2W(sSub, CP_UTF8, 0);
	wstr sRetW = xxLTrimW(sTextW, sSubW, TRUE);
	ustr sRet = xCore_W2M(sRetW, CP_UTF8, 0);
	xCore_free(sTextW);
	xCore_free(sSubW);
	return sRet;
}
XXAPI ustr xrtRTrim(ustr sText, size_t iSize, ustr sSub, size_t iSubSize, int bSrcRevise)
{
	wstr sTextW = xCore_M2W(sText, CP_UTF8, 0);
	wstr sSubW = xCore_M2W(sSub, CP_UTF8, 0);
	wstr sRetW = xxRTrimW(sTextW, sSubW, TRUE);
	ustr sRet = xCore_W2M(sRetW, CP_UTF8, 0);
	xCore_free(sTextW);
	xCore_free(sSubW);
	return sRet;
}
XXAPI ustr xrtTrim(ustr sText, size_t iSize, ustr sSub, size_t iSubSize, int bSrcRevise)
{
	wstr sTextW = xCore_M2W(sText, CP_UTF8, 0);
	wstr sSubW = xCore_M2W(sSub, CP_UTF8, 0);
	wstr sRetW = xxTrimW(sTextW, sSubW, TRUE);
	ustr sRet = xCore_W2M(sRetW, CP_UTF8, 0);
	xCore_free(sTextW);
	xCore_free(sSubW);
	return sRet;
}*/
XXAPI wstr xrtLTrimW(wstr sText, size_t iSize, wstr sSub, size_t iSubSize, int bSrcRevise)
{
	if ( sText == NULL ) { xCore.iRet = 0; return (wstr)xCore.sNull; }
	if ( sSub == NULL ) { sSub = L" \t\r\n"; iSubSize = 4; }
	if ( iSize == 0 ) { iSize = wcslen(sText); }
	if ( iSize == 0 ) { xCore.iRet = 0; return (wstr)xCore.sNull; }
	if ( iSubSize == 0 ) { iSubSize = wcslen(sSubText); }
	if ( iSubSize == 0 ) { sSub = L" \t\r\n"; iSubSize = 4; }
	int iCount = 0;
	for ( int i = 0; i < iSize; i++ ) {
		if ( wcschr(sSub, sText[i]) != NULL ) {
			iCount++;
		} else {
			break;
		}
	}
	xCore.iRet = iCount;
	if ( bSrcRevise ) {
		if ( iCount > 0 ) {
			memmove(sText, &sText[iCount], (iSize - iCount) * sizeof(wchar_t));
			sText[iSize - iCount] = 0;
		}
		return sText;
	} else {
		return xrtCopyStringW(&sText[iCount], iSize - iCount);
	}
}
XXAPI wstr xrtRTrimW(wstr sText, size_t iSize, wstr sSub, size_t iSubSize, int bSrcRevise)
{
	if ( sText == NULL ) { xCore.iRet = 0; return (wstr)xCore.sNull; }
	if ( sSub == NULL ) { sSub = L" \t\r\n"; }
	int iSize = wcslen(sText);
	int iCount = 0;
	for ( int i = iSize - 1; i >= 0; i-- ) {
		if ( wcschr(sSub, sText[i]) != NULL ) {
			iCount++;
		} else {
			break;
		}
	}
	xCore.iRet = iCount;
	if ( bSrcRevise ) {
		if ( iCount > 0 ) {
			sText[iSize - iCount] = 0;
		}
		return sText;
	} else {
		return xrtCopyStringW(sText, iSize - iCount);
	}
}
XXAPI wstr xrtTrimW(wstr sText, size_t iSize, wstr sSub, size_t iSubSize, int bSrcRevise)
{
	if ( sText == NULL ) { xCore.iRet = 0; return (wstr)xCore.sNull; }
	if ( sSub == NULL ) { sSub = L" \t\r\n"; }
	int iSize = wcslen(sText);
	int iCountL = 0;
	int iCountR = 0;
	for ( int i = 0; i < iSize; i++ ) {
		if ( wcschr(sSub, sText[i]) != NULL ) {
			iCountL++;
		} else {
			break;
		}
	}
	for ( int i = iSize - 1; i >= 0; i-- ) {
		if ( wcschr(sSub, sText[i]) != NULL ) {
			iCountR++;
		} else {
			break;
		}
	}
	int iCount = iCountL + iCountR;
	xCore.iRet = iCount;
	if ( bSrcRevise ) {
		if ( iCount > 0 ) {
			memmove(sText, &sText[iCountL], (iSize - iCount) * sizeof(wchar_t));
			sText[iSize - iCount] = 0;
		}
		return sText;
	} else {
		return xrtCopyStringW(&sText[iCountL], iSize - iCount);
	}
}



// 过滤字符串（bSrcRevise 为 FALSE 时，需使用 xCore.free 释放内存）
XXAPI wstr xxStringFilterW(wstr sText, wstr sFilter, int bSrcRevise)
{
	if ( sText == NULL ) { xCore.iRet = 0; return (wstr)xCore.sNull; }
	if ( bSrcRevise == FALSE ) { sText = xrtCopyStringW(sText, 0); }
	if ( sFilter == NULL ) { xCore.iRet = 0; return sText; }
	if ( wcslen(sFilter) == 0 ) { xCore.iRet = 0; return sText; }
	int iSize = wcslen(sText);
	int iCount = 0;
	for ( int i = 0; i < iSize; i++ ) {
		if ( wcschr(sFilter, sText[i]) != NULL ) {
			iCount++;
			continue;
		}
		sText[i - iCount] = sText[i];
	}
	sText[iSize - iCount] = 0;
	xCore.iRet = iCount;
	return sText;
}
/*
// ANSI 版本尤其 OEM 编码的缘故，按单字节处理是不安全的，因此转为 UNICODE 处理
// 因为编码转换的缘故，bSrcRevise 参数不起作用，仅为了保证与 UNICODE 函数的兼容
XXAPI ustr xxStringFilterA(ustr sText, ustr sFilter, int bSrcRevise)
{
	wstr sTextW = xCore_M2W(sText, CP_ACP, 0);
	wstr sFilterW = xCore_M2W(sFilter, CP_ACP, 0);
	wstr sRetW = xxStringFilterW(sTextW, sFilterW, TRUE);
	ustr sRet = xCore_W2M(sRetW, CP_ACP, 0);
	xCore_free(sTextW);
	xCore_free(sFilterW);
	return sRet;
}
XXAPI ustr xxStringFilterU(ustr sText, ustr sFilter, int bSrcRevise)
{
	wstr sTextW = xCore_M2W(sText, CP_UTF8, 0);
	wstr sFilterW = xCore_M2W(sFilter, CP_UTF8, 0);
	wstr sRetW = xxStringFilterW(sTextW, sFilterW, TRUE);
	ustr sRet = xCore_W2M(sRetW, CP_UTF8, 0);
	xCore_free(sTextW);
	xCore_free(sFilterW);
	return sRet;
}
*/


// 字符串格式化（需使用 xrtFree 释放）
XXAPI ustr xrtFormat(ustr sFormat, ...)
{
	if ( sFormat == NULL ) { return (ustr)xCore.sNull; }
	va_list ip;
	va_start(ip, sFormat);
	int iSize = vsnprintf(NULL, 0, sFormat, ip);
	va_end(ip);
	if ( iSize >= 0 ) {
		ustr sRet = xrtMalloc(iSize + 1);
		va_start(ip, sFormat);
		iSize = vsnprintf(sRet, iSize + 1, sFormat, ip);
		va_end(ip);
		sRet[iSize] = 0;
		xCore.iRet = iSize;
		return sRet;
	} else {
		return (ustr)xCore.sNull;
	}
}
XXAPI wstr xrtFormatW(wstr sFormat, ...)
{
	if ( sFormat == NULL ) { return (wstr)xCore.sNull; }
	va_list ip;
	va_start(ip, sFormat);
	int iSize = vsnwprintf(NULL, 0, sFormat, ip);
	va_end(ip);
	if ( iSize >= 0 ) {
		wstr sRet = xrtMalloc( (iSize + 1) * 2 );
		va_start(ip, sFormat);
		iSize = vsnwprintf(sRet, iSize + 1, sFormat, ip);
		va_end(ip);
		sRet[iSize] = 0;
		xCore.iRet = iSize * 2;
		return sRet;
	} else {
		return (wstr)xCore.sNull;
	}
}



// 字符串替换（需使用 xCore.free 释放）
XXAPI ustr xrtReplace(ustr original, ustr pattern, ustr replacement)
{
	if ( original == NULL ) { xCore.iRet = 0; return (ustr)xCore.sNull; }
	if ( pattern == NULL ) { xCore.iRet = strlen(original); return xrtCopyString(original, xCore.iRet); }
	if ( replacement == NULL ) { replacement = (ustr)xCore.sNull; }
	
	size_t replen = strlen(replacement);
	size_t patlen = strlen(pattern);
	size_t orilen = strlen(original);
	
	size_t patcnt = 0;
	ustr oriptr;
	ustr patloc;
	
	// 计算 pattern 在原始字符串中出现的次数
	for ( oriptr = original; (patloc = strstr(oriptr, pattern)); oriptr = patloc + patlen ) {
		patcnt++;
	}
	
	// 为新字符串分配内存
	xCore.iRet = orilen + patcnt * (replen - patlen);
	ustr returned = (ustr)xrtMalloc( xCore.iRet + 1 );
	
	if ( returned != NULL ) {
		// 复制原始字符串, 替换需要改变的部分
		ustr retptr = returned;
		for (oriptr = original; (patloc = strstr(oriptr, pattern)); oriptr = patloc + patlen)
		{
			size_t const skplen = patloc - oriptr;
			// 复制前面的部分，直到出现要查找的字符串
			strncpy(retptr, oriptr, skplen);
			retptr += skplen;
			// 复制要替换的字符串
			strncpy(retptr, replacement, replen);
			retptr += replen;
		}
		// 复制最后一段剩下的字符串
		strcpy(retptr, oriptr);
		return returned;
	} else {
		xCore.iRet = 0;
		return (ustr)xCore.sNull;
	}
}
XXAPI wstr xrtReplaceW(wstr original, wstr pattern, wstr replacement)
{
	if ( original == NULL ) { xCore.iRet = 0; return (wstr)xCore.sNull; }
	if ( pattern == NULL ) { xCore.iRet = wcslen(original); return xrtCopyStringW(original, xCore.iRet); }
	if ( replacement == NULL ) { replacement = (wstr)xCore.sNull; }

	size_t replen = wcslen(replacement);
	size_t patlen = wcslen(pattern);
	size_t orilen = wcslen(original);
	
	size_t patcnt = 0;
	wstr oriptr;
	wstr patloc;
	
	// 计算 pattern 在原始字符串中出现的次数
	for ( oriptr = original; (patloc = wcsstr(oriptr, pattern)); oriptr = patloc + patlen ) {
		patcnt++;
	}
	
	// 为新字符串分配内存
	xCore.iRet = orilen + patcnt * (replen - patlen);
	wstr returned = (wstr)xrtMalloc( (xCore.iRet + 1) * sizeof(wchar_t) );
	
	if ( returned != NULL ) {
		// 复制原始字符串, 替换需要改变的部分
		wstr retptr = returned;
		for (oriptr = original; (patloc = wcsstr(oriptr, pattern)); oriptr = patloc + patlen)
		{
			size_t const skplen = patloc - oriptr;
			// 复制前面的部分，直到出现要查找的字符串
			wcsncpy(retptr, oriptr, skplen);
			retptr += skplen;
			// 复制要替换的字符串
			wcsncpy(retptr, replacement, replen);
			retptr += replen;
		}
		// 复制最后一段剩下的字符串
		wcscpy(retptr, oriptr);
		return returned;
	} else {
		xCore.iRet = 0;
		return (wstr)xCore.sNull;
	}
}



// 字符串分割（需使用 xCore.free 释放）
XXAPI ustr* xrtSplit(ustr sText, ustr sSep, int bSrcRevise)
{
	if ( sText == NULL ) { sText = (ustr)xCore.sNull; }
	if ( sSep == NULL ) { sSep = ","; }
	size_t iSepSize = strlen(sSep);
	int iPos = 0;
	int iCount = 0;
	// 统计分隔符出现的次数
	while (TRUE) {
		if ( sText[iPos] == 0 ) {
			break;
		}
		if ( strncmp(&sText[iPos], sSep, iSepSize) == 0 ) {
			iCount++;
		}
		iPos++;
	}
	// 准备返回的数据 [分割指针 + NULL + 字符串表 + \0]
	ustr* sRet;
	ustr pData;
	if ( bSrcRevise ) {
		sRet = xrtMalloc( (iCount + 2) * sizeof(void*) );
		pData = sText;
	} else {
		sRet = xrtMalloc( ((iCount + 2) * sizeof(void*)) + iPos + 1);
		pData = (ustr)&sRet[iCount + 2];
		memcpy(pData, sText, iPos);
		pData[iPos] = 0;
	}
	sRet[iCount + 1] = NULL;
	ustr pAddr = pData;
	// 开始分割数据
	iCount = 0;
	for (int i = 0; i < iPos; i++) {
		if ( strncmp(&sText[i], sSep, iSepSize) == 0 ) {
			pData[i] = 0;
			sRet[iCount] = pAddr;
			pAddr = pData + i + iSepSize;
			i += iSepSize - 1;
			iCount++;
		}
	}
	sRet[iCount] = pAddr;
	xCore.iRet = iCount + 1;
	return sRet;
}
XXAPI wstr* xrtSplitW(wstr sText, wstr sSep, int bSrcRevise)
{
	if ( sText == NULL ) { sText = (wstr)xCore.sNull; }
	if ( sSep == NULL ) { sSep = L","; }
	size_t iSepSize = wcslen(sSep);
	int iPos = 0;
	int iCount = 0;
	// 统计分隔符出现的次数
	while (TRUE) {
		if ( sText[iPos] == 0 ) {
			break;
		}
		if ( wcsncmp(&sText[iPos], sSep, iSepSize) == 0 ) {
			iCount++;
		}
		iPos++;
	}
	// 准备返回的数据 [分割指针 + NULL + 字符串表 + \0]
	wstr* sRet;
	wstr pData;
	if ( bSrcRevise ) {
		sRet = xrtMalloc( (iCount + 2) * sizeof(void*) );
		pData = sText;
	} else {
		sRet = xrtMalloc( ((iCount + 2) * sizeof(void*)) + ((iPos + 1) * sizeof(wchar_t)));
		pData = (wstr)&sRet[iCount + 2];
		memcpy(pData, sText, iPos * sizeof(wchar_t));
		pData[iPos] = 0;
	}
	sRet[iCount + 1] = NULL;
	wstr pAddr = pData;
	// 开始分割数据
	iCount = 0;
	for (int i = 0; i < iPos; i++) {
		if ( wcsncmp(&sText[i], sSep, iSepSize) == 0 ) {
			pData[i] = 0;
			sRet[iCount] = pAddr;
			pAddr = pData + i + iSepSize;
			i += iSepSize - 1;
			iCount++;
		}
	}
	sRet[iCount] = pAddr;
	xCore.iRet = iCount + 1;
	return sRet;
}



int hex2dec(char c)
{
    if ('0' <= c && c <= '9') {
        return c - '0';
    } else if ('a' <= c && c <= 'f') {
        return c - 'a' + 10;
    } else if ('A' <= c && c <= 'F') {
        return c - 'A' + 10;
    } else {
        return -1;
    }
}

char dec2hex(short int c)
{
    if (0 <= c && c <= 9) {
        return c + '0';
    } else if (10 <= c && c <= 15) {
        return c + 'A' - 10;
    } else {
        return -1;
    }
}



// HEX 编码
XXAPI char* HexEncode(char* sMem, uint32 iSize)
{
	if ( iSize == 0 ) {
		iSize = strlen(sMem);
	}
    char* sRet = xrtMalloc((iSize * 3) + 1);
    int iPos = 0;
    for ( int i = 0; i < iSize; ++i ) {
        char c = sMem[i];
		int j = (short int)c;
		if (j < 0) { j += 256; }
		int i1 = j / 16;
		int i0 = j - i1 * 16;
		sRet[iPos++] = dec2hex(i1);
		sRet[iPos++] = dec2hex(i0);
    }
    sRet[iPos] = 0;
    return sRet;
}

// HEX 解码
XXAPI char* HexDecode(char* sMem, uint32 iSize)
{
	if ( iSize == 0 ) {
		iSize = strlen(sMem);
	}
    char* sRet = xrtMalloc(iSize + 1);
    int iPos = 0;
    for ( int i = 0; i < iSize; ++i ) {
		char c1 = sMem[i++];
		char c0 = sMem[i];
		int iChar = hex2dec(c1) * 16 + hex2dec(c0);
		printf("%d\n", iChar);
		sRet[iPos++] = iChar;
    }
    sRet[iPos] = 0;
    return sRet;
}



// URI 编码
XXAPI char* UriEncode(char* sURL)
{
    uint32 iSize = strlen(sURL);
    char* sRet = xrtMalloc((iSize * 3) + 1);
    int iPos = 0;
    for ( int i = 0; i < iSize; ++i ) {
        char c = sURL[i];
        // 无需编码的字符
        if ( (('0' <= c) && (c <= '9')) || (('a' <= c) && (c <= 'z')) || (('A' <= c) && (c <= 'Z')) || (c == '/') || (c == '.') || (c == '-') || (c == '_') ) {
            sRet[iPos++] = c;
        } else {
			// 处理协议头，协议头部分不进行编码 [ *://*/ ]
			if ( (c == ':') && (sURL[i+1] == '/') && (sURL[i+2] == '/') ) {
				int iStrPos = 0;
				for ( int j = i + 3; j < iSize; ++j ) {
					if ( (sURL[j] == '/') || (sURL[j] == 0) ) {
						iStrPos = j + 1;
						strncpy(&sRet[iPos], &sURL[i], iStrPos - i);
						iPos += iStrPos - i;
						i = j;
						break;
					}
				}
				if ( iStrPos ) { continue; }
			}
			// 编码其他字符
            int j = (short int)c;
            if (j < 0) { j += 256; }
            int i1 = j / 16;
            int i0 = j - i1 * 16;
            sRet[iPos++] = '%';
            sRet[iPos++] = dec2hex(i1);
            sRet[iPos++] = dec2hex(i0);
        }
    }
    sRet[iPos] = 0;
    return sRet;
}

// URI 解码
XXAPI char* UriDecode(char* sURL)
{
    uint32 iSize = strlen(sURL);
    char* sRet = xrtMalloc(iSize + 1);
    int iPos = 0;
    for ( int i = 0; i < iSize; ++i ) {
        char c = sURL[i];
        if (c != '%') {
            sRet[iPos++] = c;
        } else {
            char c1 = sURL[++i];
            char c0 = sURL[++i];
            int iChar = hex2dec(c1) * 16 + hex2dec(c0);
            sRet[iPos++] = iChar;
        }
    }
    sRet[iPos] = 0;
    return sRet;
}


