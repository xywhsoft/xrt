


/* ------------------------------------ C 函数库 ------------------------------------ */

// 多字节 转 Unicode
XXAPI wstr xrtM2W(ptr pStr, uint32 iCodePage, ulong iSize)
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
XXAPI ustr xrtW2M(wstr pStr, uint32 iCodePage, ulong iSize)
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
XXAPI wstr xCore_A2W(ustr pZStr, ulong iSize)
{
	return xrtM2W(pZStr, CP_ACP, iSize);
}

// utf-8 转 Unicode
XXAPI wstr xCore_U2W(ustr pUStr, ulong iSize)
{
    return xrtM2W(pUStr, CP_UTF8, iSize);
}

// Unicode 转 ANSI
XXAPI ustr xCore_W2A(wstr pWStr, ulong iSize)
{
    return xrtW2M(pWStr, CP_ACP, iSize);
}

// Unicode 转 utf-8
XXAPI ustr xCore_W2U(wstr pWStr, ulong iSize)
{
    return xrtW2M(pWStr, CP_UTF8, iSize);
}

// ANSI 转 utf-8
XXAPI ustr xCore_A2U(ustr pZStr, ulong iSize)
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
XXAPI ustr xCore_U2A(ustr pUStr, ulong iSize)
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


