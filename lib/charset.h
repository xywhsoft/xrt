


/* ------------------------------------ C 函数库 ------------------------------------ */

// 多字节 转 Unicode
XXAPI wstr xrtM2W(ptr pStr, uint32 iCodePage, size_t iSize)
{
	if ( pStr ) {
		if (iSize == 0) { iSize = strlen(pStr); }
		if (iSize == 0) { xCore.iRet = 0; return (wstr)xCore.sNull; }
		xCore.iRet = MultiByteToWideChar(iCodePage, 0, pStr, iSize, NULL, 0);
		ulong iMemSize = (xCore.iRet + 1) * sizeof(wchar_t);
		wstr pWStr = malloc(iMemSize);
		MultiByteToWideChar(iCodePage, 0, pStr, iSize, pWStr, xCore.iRet);
		pWStr[xCore.iRet] = 0;
		xCore.iRet = xCore.iRet * sizeof(wchar_t);
		return pWStr;
	}
	xCore.iRet = 0;
	return (wstr)xCore.sNull;
}

// Unicode 转 多字节
XXAPI ustr xrtW2M(wstr pStr, uint32 iCodePage, size_t iSize)
{
    if ( pStr ) {
        if (iSize == 0) { iSize = wcslen(pStr); } else { iSize = iSize / sizeof(wchar_t); }
		if (iSize == 0) { xCore.iRet = 0; return (ustr)xCore.sNull; }
		xCore.iRet = WideCharToMultiByte(iCodePage, 0, pStr, iSize, NULL, 0, NULL, NULL);
		ustr pZStr = malloc(xCore.iRet + 1);
		WideCharToMultiByte(iCodePage, 0, pStr, iSize, pZStr, xCore.iRet, NULL, NULL);
		pZStr[xCore.iRet] = 0;
		return pZStr;
    }
	xCore.iRet = 0;
	return (ustr)xCore.sNull;
}

// ANSI 转 Unicode
XXAPI wstr xrtA2W(ustr pZStr, size_t iSize)
{
	return xrtM2W(pZStr, CP_ACP, iSize);
}

// utf-8 转 Unicode
XXAPI wstr xrtU2W(ustr pUStr, size_t iSize)
{
    return xrtM2W(pUStr, CP_UTF8, iSize);
}

// Unicode 转 ANSI
XXAPI ustr xrtW2A(wstr pWStr, size_t iSize)
{
    return xrtW2M(pWStr, CP_ACP, iSize);
}

// Unicode 转 utf-8
XXAPI ustr xrtW2U(wstr pWStr, size_t iSize)
{
    return xrtW2M(pWStr, CP_UTF8, iSize);
}

// ANSI 转 utf-8
XXAPI ustr xrtA2U(ustr pZStr, size_t iSize)
{
    if ( pZStr ) {
        wstr sTmp = xrtM2W(pZStr, CP_ACP , iSize * 2);
        ustr sRet = xrtW2M(sTmp , CP_UTF8, iSize);
        free(sTmp);
        return sRet;
    }
	xCore.iRet = 0;
	return (ustr)xCore.sNull;
}

// utf-8 转 ANSI
XXAPI ustr xrtU2A(ustr pUStr, size_t iSize)
{
    if ( pUStr ) {
        wstr sTmp = xrtM2W(pUStr, CP_UTF8, iSize);
        ustr sRet = xrtW2M(sTmp , CP_ACP , iSize * 2);
        free(sTmp);
        return sRet;
    }
	xCore.iRet = 0;
	return (ustr)xCore.sNull;
}


