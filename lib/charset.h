


/* ------------------------------------ C 函数库 ------------------------------------ */
/*
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
*/


static char BytesExtraTableUTF8[256] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3,4,4,4,4,5,5,5,5
};



// utf-8 转 utf-16
XXAPI u16str xrtUTF8to16(u8str sText, size_t iSize)
{
	if ( sText == NULL ) { xCore.iRet = 0; return (u32str)xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { xCore.iRet = 0; return (u32str)xCore.sNull; }
}



// utf-8 转 utf-32
XXAPI u32str xrtUTF8to32(u8str sText, size_t iSize)
{
	if ( sText == NULL ) { xCore.iRet = 0; return (u32str)xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { xCore.iRet = 0; return (u32str)xCore.sNull; }
	// 计算转换长度
	size_t iPos = 0;
	for ( int i = 0; i < iSize; i++ ) {
		iPos++;
		i += BytesExtraTableUTF8[sText[i]];
	}
	// 申请所需内存
	u32str sRet = xrtMalloc((iPos + 1) * sizeof(unsigned int));
	if ( sRet == NULL ) {
		xrtSetError(xCore.ERROR_DESC.MALLOC, FALSE);
		xCore.iRet = 0;
		return (u32str)xCore.sNull;
	}
	// 开始转换编码
	iPos = 0;
	for ( int i = 0; i < iSize; i++ ) {
		char iExtraBytes = BytesExtraTableUTF8[sText[i]];
		if ( iExtraBytes == 0 ) {
			// ASCII 兼容字符
			sRet[iPos] = sText[i];
		} else if ( iExtraBytes == 1 ) {
			// 双字节字符
			sRet[iPos] = ((sText[i] & 0b00011111) << 6) | (sText[++i] & 0b00111111);
		} else if ( iExtraBytes == 2 ) {
			// 三字节字符
			sRet[iPos] = ((sText[i] & 0b00001111) << 12) | ((sText[++i] & 0b00111111) << 6) | (sText[++i] & 0b00111111);
		} else if ( iExtraBytes == 3 ) {
			// 四字节字符
			sRet[iPos] = ((sText[i] & 0b00000111) << 18) | ((sText[++i] & 0b00111111) << 12) | ((sText[++i] & 0b00111111) << 6) | (sText[++i] & 0b00111111);
		} else if ( iExtraBytes == 4 ) {
			// 五字节字符
			sRet[iPos] = ((sText[i] & 0b00000011) << 24) | ((sText[++i] & 0b00111111) << 18) | ((sText[++i] & 0b00111111) << 12) | ((sText[++i] & 0b00111111) << 6) | (sText[++i] & 0b00111111);
		} else if ( iExtraBytes == 5 ) {
			// 六字节字符
			sRet[iPos] = ((sText[i] & 0b00000001) << 30) | ((sText[++i] & 0b00111111) << 24) | ((sText[++i] & 0b00111111) << 18) | ((sText[++i] & 0b00111111) << 12) | ((sText[++i] & 0b00111111) << 6) | (sText[++i] & 0b00111111);
		}
		iPos++;
	}
	sRet[iPos] = 0;
	xCore.iRet = iPos;
	return sRet;
}



// utf-16 转 utf-8
XXAPI u8str xrtUTF16to8(u16str sText, size_t iSize)
{
}



// utf-16 转 utf-32
XXAPI u32str xrtUTF16to32(u16str sText, size_t iSize)
{
}



// utf-32 转 utf-8
XXAPI u8str xrtUTF32to8(u32str sText, size_t iSize)
{
	if ( sText == NULL ) { xCore.iRet = 0; return xCore.sNull; }
	size_t iRetSize = 0;
	// 计算数据长度和转换长度
	if ( iSize == 0 ) {
		u32str sPtr = sText;
		while ( *sPtr != 0 ) {
			if ( *sPtr <= 0x7F ) {
				iRetSize++;
			} else if ( *sPtr <= 0x7FF ) {
				iRetSize += 2;
			} else if ( *sPtr <= 0xFFFF ) {
				iRetSize += 3;
			} else if ( *sPtr <= 0x1FFFFF ) {
				iRetSize += 4;
			} else if ( *sPtr <= 0x3FFFFFF ) {
				iRetSize += 5;
			} else if ( *sPtr <= 0x7FFFFFFF ) {
				iRetSize += 6;
			}
			sPtr++;
			iSize++;
		}
	} else {
		for ( int i = 0; i < iSize; i++ ) {
			uint32 iChar = sText[i];
			if ( iChar <= 0x7F ) {
				iRetSize++;
			} else if ( iChar <= 0x7FF ) {
				iRetSize += 2;
			} else if ( iChar <= 0xFFFF ) {
				iRetSize += 3;
			} else if ( iChar <= 0x1FFFFF ) {
				iRetSize += 4;
			} else if ( iChar <= 0x3FFFFFF ) {
				iRetSize += 5;
			} else if ( iChar <= 0x7FFFFFFF ) {
				iRetSize += 6;
			}
		}
	}
	if ( iSize == 0 ) { xCore.iRet = 0; return xCore.sNull; }
	// 申请所需内存
	u8str sRet = xrtMalloc(iRetSize + 1);
	if ( sRet == NULL ) {
		xrtSetError(xCore.ERROR_DESC.MALLOC, FALSE);
		xCore.iRet = 0;
		return xCore.sNull;
	}
	// 开始转换编码
	size_t iPos = 0;
	for ( int i = 0; i < iSize; i++ ) {
		uint32 iChar = sText[i];
		if ( iChar <= 0x7F ) {
			// ASCII 兼容字符
			sRet[++iPos] = iChar;
		} else if ( iChar <= 0x7FF ) {
			// 双字节字符
			sRet[++iPos] = 0xC0 | ((iChar >> 6) & 0x1F);
			sRet[++iPos] = 0x80 | (iChar & 0x3F);
		} else if ( iChar <= 0xFFFF ) {
			// 三字节字符
			sRet[++iPos] = 0xE0 | ((iChar >> 12) & 0xF);
			sRet[++iPos] = 0x80 | ((iChar >> 6) & 0x3F);
			sRet[++iPos] = 0x80 | (iChar & 0x3F);
		} else if ( iChar <= 0x1FFFFF ) {
			// 四字节字符
			sRet[++iPos] = 0xF0 | ((iChar >> 18) & 0x7);
			sRet[++iPos] = 0x80 | ((iChar >> 12) & 0x3F);
			sRet[++iPos] = 0x80 | ((iChar >> 6) & 0x3F);
			sRet[++iPos] = 0x80 | (iChar & 0x3F);
		} else if ( iChar <= 0x3FFFFFF ) {
			// 五字节字符
			sRet[++iPos] = 0xF8 | ((iChar >> 24) & 0x3);
			sRet[++iPos] = 0x80 | ((iChar >> 18) & 0x3F);
			sRet[++iPos] = 0x80 | ((iChar >> 12) & 0x3F);
			sRet[++iPos] = 0x80 | ((iChar >> 6) & 0x3F);
			sRet[++iPos] = 0x80 | (iChar & 0x3F);
		} else if ( iChar <= 0x7FFFFFFF ) {
			// 六字节字符
			sRet[++iPos] = 0xFC | ((iChar >> 30) & 0x1);
			sRet[++iPos] = 0x80 | ((iChar >> 24) & 0x3F);
			sRet[++iPos] = 0x80 | ((iChar >> 18) & 0x3F);
			sRet[++iPos] = 0x80 | ((iChar >> 12) & 0x3F);
			sRet[++iPos] = 0x80 | ((iChar >> 6) & 0x3F);
			sRet[++iPos] = 0x80 | (iChar & 0x3F);
		}
	}
	sRet[iPos] = 0;
	xCore.iRet = iPos;
	return sRet;
}



// utf-32 转 utf-16
XXAPI u16str xrtUTF32to16(u32str sText, size_t iSize)
{
}


