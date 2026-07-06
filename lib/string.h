

#define __xrt_cstr(sText) ((const char*)(sText))
#define __xrt_str(sText) ((char*)(sText))

// 内部函数：HEX 字符转数值
static bool __xrtHexDecodeNibble(uint8 c, uint8* pNibble)
{
	if ( pNibble == NULL ) {
		return FALSE;
	}
	if ( c >= '0' && c <= '9' ) {
		*pNibble = (uint8)(c - '0');
		return TRUE;
	}
	if ( c >= 'A' && c <= 'F' ) {
		*pNibble = (uint8)(c - 'A' + 10u);
		return TRUE;
	}
	if ( c >= 'a' && c <= 'f' ) {
		*pNibble = (uint8)(c - 'a' + 10u);
		return TRUE;
	}
	return FALSE;
}


// 内部函数：安全获取 UTF-8 字符长度
static size_t __xrtUtf8CharLenSafe(str sText, size_t iSize, size_t iPos)
{
	size_t iCharLen;
	if ( !sText || iPos >= iSize ) { return 0; }
	iCharLen = (size_t)xrtCharLenU8((unsigned char)sText[iPos]);
	if ( (iCharLen == 0) || (iCharLen > (iSize - iPos)) ) {
		return 1;
	}
	return iCharLen;
}


// 内部函数：自动计算字符串字节长度
static size_t __xrtStrByteSize(str sText, size_t iSize)
{
	if ( sText == NULL ) { return 0; }
	return iSize == 0 ? strlen(__xrt_cstr(sText)) : iSize;
}


// 内部函数：把字符下标转换为字节偏移，越界时钳制到字符串末尾
static size_t __xrtStrCharToByteOffset(str sText, size_t iSize, size_t iCharIndex)
{
	size_t iPos = 0;
	size_t iChar = 0;
	while ( iPos < iSize && iChar < iCharIndex ) {
		size_t iCharLen = __xrtUtf8CharLenSafe(sText, iSize, iPos);
		if ( iCharLen == 0 ) { break; }
		iPos += iCharLen;
		iChar++;
	}
	return iPos;
}


// 内部函数：把字节偏移转换为字符下标
static size_t __xrtStrByteToCharOffset(str sText, size_t iSize, size_t iByteOffset)
{
	size_t iPos = 0;
	size_t iChar = 0;
	if ( iByteOffset > iSize ) { iByteOffset = iSize; }
	while ( iPos < iByteOffset ) {
		size_t iCharLen = __xrtUtf8CharLenSafe(sText, iSize, iPos);
		if ( iCharLen == 0 || iPos + iCharLen > iByteOffset ) { break; }
		iPos += iCharLen;
		iChar++;
	}
	return iChar;
}


// 内部函数：规范化支持负数的字符下标
static size_t __xrtStrNormalizeCharIndex(size_t iCharCount, int64 iIndex)
{
	if ( iIndex < 0 ) {
		uint64 iBack = (uint64)(-iIndex);
		if ( iBack >= iCharCount ) { return 0; }
		return iCharCount - (size_t)iBack;
	}
	if ( (uint64)iIndex > iCharCount ) { return iCharCount; }
	return (size_t)iIndex;
}


// 内部函数：复制 UTF-8 字节片段
static str __xrtStrCopyByteRange(str sText, size_t iStart, size_t iLen, size_t* iRetSize)
{
	str sRet;
	if ( iRetSize ) { *iRetSize = iLen; }
	if ( sText == NULL || iLen == 0 ) {
		if ( iRetSize ) { *iRetSize = 0; }
		return xCore.sNull;
	}
	sRet = xrtMalloc(iLen + 1);
	if ( sRet == NULL ) {
		if ( iRetSize ) { *iRetSize = 0; }
		return xCore.sNull;
	}
	memcpy(sRet, sText + iStart, iLen);
	sRet[iLen] = 0;
	return sRet;
}


// 内部函数：统计需要填充的字节数
static size_t __xrtStrPadBytes(str sPadText, size_t iPadSize, size_t iNeedChars)
{
	size_t iBytes = 0;
	size_t iPos = 0;
	size_t iChars = 0;
	if ( iNeedChars == 0 ) { return 0; }
	if ( sPadText == NULL || iPadSize == 0 ) { return iNeedChars; }
	while ( iChars < iNeedChars ) {
		size_t iCharLen = __xrtUtf8CharLenSafe(sPadText, iPadSize, iPos);
		if ( iCharLen == 0 ) {
			iPos = 0;
			continue;
		}
		iBytes += iCharLen;
		iPos += iCharLen;
		if ( iPos >= iPadSize ) { iPos = 0; }
		iChars++;
	}
	return iBytes;
}


// 内部函数：写入指定数量的填充字符
static void __xrtStrWritePad(str sDst, size_t* iDstPos, str sPadText, size_t iPadSize, size_t iNeedChars)
{
	size_t iPos = 0;
	size_t iChars = 0;
	if ( sDst == NULL || iDstPos == NULL || iNeedChars == 0 ) { return; }
	if ( sPadText == NULL || iPadSize == 0 ) {
		memset(sDst + *iDstPos, ' ', iNeedChars);
		*iDstPos += iNeedChars;
		return;
	}
	while ( iChars < iNeedChars ) {
		size_t iCharLen = __xrtUtf8CharLenSafe(sPadText, iPadSize, iPos);
		if ( iCharLen == 0 ) {
			iPos = 0;
			continue;
		}
		memcpy(sDst + *iDstPos, sPadText + iPos, iCharLen);
		*iDstPos += iCharLen;
		iPos += iCharLen;
		if ( iPos >= iPadSize ) { iPos = 0; }
		iChars++;
	}
}


// 内部函数：检查字节序列是否在集合中
static bool __xrtStrHasToken(str sText, size_t iSize, const unsigned char* sToken, size_t iTokenSize)
{
	if ( !sText || !sToken || (iTokenSize == 0) || (iSize < iTokenSize) ) { return FALSE; }
	for ( size_t i = 0; (i + iTokenSize) <= iSize; i++ ) {
		if ( memcmp(&sText[i], sToken, iTokenSize) == 0 ) {
			return TRUE;
		}
	}
	return FALSE;
}


// 创建字符串副本（ 需使用 xrtFree 释放 ）（ 线程安全 ）
XXAPI str xrtCopyStr(str sText, size_t iSize)
{
	if ( sText == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0 ) { return xCore.sNull; }
	str sRet = xrtMalloc(iSize + 1);
	if ( sRet == NULL ) { return xCore.sNull; }
	memcpy(sRet, sText, iSize);
	sRet[iSize] = 0;
	return sRet;
}


// 连接两个字符串（需使用 xrtFree 释放）
XXAPI str xrtStrConcat(str sLeft, str sRight)
{
	size_t iLeftSize = (sLeft != NULL) ? strlen(__xrt_cstr(sLeft)) : 0;
	size_t iRightSize = (sRight != NULL) ? strlen(__xrt_cstr(sRight)) : 0;
	size_t iRetSize;
	str sRet;
	if ( iLeftSize == 0 && iRightSize == 0 ) { return xCore.sNull; }
	if ( iLeftSize > SIZE_MAX - iRightSize - 1 ) { return xCore.sNull; }
	iRetSize = iLeftSize + iRightSize;
	sRet = xrtMalloc(iRetSize + 1);
	if ( sRet == NULL ) { return xCore.sNull; }
	if ( iLeftSize > 0 ) { memcpy(sRet, sLeft, iLeftSize); }
	if ( iRightSize > 0 ) { memcpy(sRet + iLeftSize, sRight, iRightSize); }
	sRet[iRetSize] = 0;
	return sRet;
}


// 连接多个字符串（需使用 xrtFree 释放）
XXAPI str xrtStrJoin(uint32 iCount, ...)
{
	va_list ap;
	uint32 i;
	size_t iRetSize = 0;
	size_t iPos = 0;
	str sRet;
	va_start(ap, iCount);
	for ( i = 0; i < iCount; ++i ) {
		str sItem = va_arg(ap, str);
		size_t iItemSize = (sItem != NULL) ? strlen(__xrt_cstr(sItem)) : 0;
		if ( iItemSize > SIZE_MAX - iRetSize - 1 ) {
			va_end(ap);
			return xCore.sNull;
		}
		iRetSize += iItemSize;
	}
	va_end(ap);
	if ( iRetSize == 0 ) { return xCore.sNull; }
	sRet = xrtMalloc(iRetSize + 1);
	if ( sRet == NULL ) { return xCore.sNull; }
	va_start(ap, iCount);
	for ( i = 0; i < iCount; ++i ) {
		str sItem = va_arg(ap, str);
		size_t iItemSize = (sItem != NULL) ? strlen(__xrt_cstr(sItem)) : 0;
		if ( iItemSize > 0 ) {
			memcpy(sRet + iPos, sItem, iItemSize);
			iPos += iItemSize;
		}
	}
	va_end(ap);
	sRet[iRetSize] = 0;
	return sRet;
}


// 复制字符串 u 16
XXAPI u16str xrtCopyStrU16(u16str sText, size_t iSize)
{
	if ( sText == NULL ) { return (u16str)xCore.sNull; }
	if ( iSize == 0 ) { iSize = u16len(sText); }
	if ( iSize == 0 ) { return (u16str)xCore.sNull; }
	if ( iSize > ((SIZE_MAX / sizeof(unsigned short)) - 1u) ) { return (u16str)xCore.sNull; }
	u16str sRet = xrtMalloc((iSize + 1) * sizeof(unsigned short));
	if ( sRet == NULL ) { return (u16str)xCore.sNull; }
	memcpy(sRet, sText, iSize * sizeof(unsigned short));
	sRet[iSize] = 0;
	return sRet;
}


// 复制字符串 u 32
XXAPI u32str xrtCopyStrU32(u32str sText, size_t iSize)
{
	if ( sText == NULL ) { return (u32str)xCore.sNull; }
	if ( iSize == 0 ) { iSize = u32len(sText); }
	if ( iSize == 0 ) { return (u32str)xCore.sNull; }
	if ( iSize > ((SIZE_MAX / sizeof(unsigned int)) - 1u) ) { return (u32str)xCore.sNull; }
	u32str sRet = xrtMalloc((iSize + 1) * sizeof(unsigned int));
	if ( sRet == NULL ) { return (u32str)xCore.sNull; }
	memcpy(sRet, sText, iSize * sizeof(unsigned int));
	sRet[iSize] = 0;
	return sRet;
}


// 复制内存
XXAPI ptr xrtCopyMem(ptr pMem, size_t iSize)
{
	if ( pMem == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { return xCore.sNull; }
	ptr pRet = xrtMalloc(iSize);
	if ( pRet == NULL ) { return xCore.sNull; }
	memcpy(pRet, pMem, iSize);
	return pRet;
}



// 比较字符串
XXAPI int xrtStrComp(str s1, str s2, size_t iSize, bool bCase)
{
	if ( s1 == NULL || s2 == NULL ) {
		if ( s1 == s2 ) { return 0; }
		return s1 ? 1 : -1;
	}
	if ( iSize > 0 ) {
		if ( bCase ) {
			#if defined(_WIN32) || defined(_WIN64)
				// windows 方案
				return strnicmp(__xrt_cstr(s1), __xrt_cstr(s2), iSize);
			#else
				// 其他平台方案
				return strncasecmp(__xrt_cstr(s1), __xrt_cstr(s2), iSize);
			#endif
		} else {
			return strncmp(__xrt_cstr(s1), __xrt_cstr(s2), iSize);
		}
	} else {
		if ( bCase ) {
			#if defined(_WIN32) || defined(_WIN64)
				// windows 方案
				return stricmp(__xrt_cstr(s1), __xrt_cstr(s2));
			#else
				// 其他平台方案
				return strcasecmp(__xrt_cstr(s1), __xrt_cstr(s2));
			#endif
		} else {
			return strcmp(__xrt_cstr(s1), __xrt_cstr(s2));
		}
	}
}


// 字符串字节长度
XXAPI size_t xrtStrByteLen(str sText, size_t iSize)
{
	return __xrtStrByteSize(sText, iSize);
}


// 安全获取 UTF-8 字符字节长度
XXAPI size_t xrtStrUtf8CharSize(str sText, size_t iSize, size_t iPos)
{
	iSize = __xrtStrByteSize(sText, iSize);
	return __xrtUtf8CharLenSafe(sText, iSize, iPos);
}


// 字符串字符数量（按 UTF-8 codepoint 计数）
XXAPI size_t xrtStrCharLen(str sText, size_t iSize)
{
	size_t iPos = 0;
	size_t iCount = 0;
	iSize = __xrtStrByteSize(sText, iSize);
	while ( iPos < iSize ) {
		size_t iCharLen = __xrtUtf8CharLenSafe(sText, iSize, iPos);
		if ( iCharLen == 0 ) { break; }
		iPos += iCharLen;
		iCount++;
	}
	return iCount;
}



// 字符串转为小写（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
XXAPI str xrtLCase(str sText, size_t iSize, bool bSrcRevise)
{
	if ( sText == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0 ) { return xCore.sNull; }
	str sRet;
	if ( bSrcRevise ) { sRet = sText; } else { sRet = xrtCopyStr(sText, iSize); }
	for ( size_t i = 0; i < iSize; i++ ) {
		unsigned char c = (unsigned char)sRet[i];
		if ( (c & 0x80) == 0 ) {
			// ASCII 字符
			sRet[i] = tolower(sRet[i]);
		} else if ( (c & 0xE0) == 0xC0 ) {
			// UTF-8 双字节字符，跳过
			i++;
		} else if ( (c & 0xF0) == 0xE0 ) {
			// UTF-8 三字节字符，跳过
			i += 2;
		} else if ( (c & 0xF8) == 0xF0 ) {
			// UTF-8 四字节字符，跳过
			i += 3;
		} else if ( (c & 0xFC) == 0xF8 ) {
			// UTF-8 五字节字符，跳过
			i += 4;
		} else if ( (c & 0xFE) == 0xFC ) {
			// UTF-8 六字节字符，跳过
			i += 5;
		}
		// 其他情况（单独的续字节或异常字符）跳过
	}
	return sRet;
}



// 字符串转为大写（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
XXAPI str xrtUCase(str sText, size_t iSize, bool bSrcRevise)
{
	if ( sText == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0 ) { return xCore.sNull; }
	str sRet;
	if ( bSrcRevise != FALSE ) { sRet = sText; } else { sRet = xrtCopyStr(sText, iSize); }
	for ( size_t i = 0; i < iSize; i++ ) {
		unsigned char c = (unsigned char)sRet[i];
		if ( (c & 0x80) == 0 ) {
			// ASCII 字符
			sRet[i] = toupper(sRet[i]);
		} else if ( (c & 0xE0) == 0xC0 ) {
			// UTF-8 双字节字符，跳过
			i++;
		} else if ( (c & 0xF0) == 0xE0 ) {
			// UTF-8 三字节字符，跳过
			i += 2;
		} else if ( (c & 0xF8) == 0xF0 ) {
			// UTF-8 四字节字符，跳过
			i += 3;
		} else if ( (c & 0xFC) == 0xF8 ) {
			// UTF-8 五字节字符，跳过
			i += 4;
		} else if ( (c & 0xFE) == 0xFC ) {
			// UTF-8 六字节字符，跳过
			i += 5;
		}
		// 其他情况（单独的续字节或异常字符）跳过
	}
	return sRet;
}



// 搜索字符串（ 没找到字符串的情况下会返回 NULL ）（ 线程安全 ）
XXAPI str xrtFindStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase)
{
	// 参数检查
	if ( sText == NULL ) { return NULL; }
	if ( sSubText == NULL ) { return NULL; }
	// 自动计算字符串长度
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0 ) { return NULL; }
	if ( iSubSize == 0 ) { iSubSize = strlen(__xrt_cstr(sSubText)); }
	if ( iSubSize == 0 ) { return NULL; }
	str sSub;
	if ( bCase ) {
		// 忽略大小写：将源字符串和子串都转为小写副本，在副本上搜索
		str sText1 = xrtLCase(sText, 0, FALSE);
		str sText2 = xrtLCase(sSubText, 0, FALSE);
		sSub = memmem(sText1, iSize, sText2, iSubSize);
		if ( sSub ) {
			// 将匹配位置映射回原始字符串的对应偏移
			sSub = &sText[sSub - sText1];
		}
		xrtFree(sText1);
		xrtFree(sText2);
	} else {
		// 精确匹配：直接在原字符串上搜索
		sSub = memmem(sText, iSize, sSubText, iSubSize);
	}
	return sSub;
}


// 查找子串位置（按 UTF-8 字符下标返回，未找到返回 -1）
XXAPI int64 xrtStrIndexOf(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase)
{
	str sFound;
	iSize = __xrtStrByteSize(sText, iSize);
	iSubSize = __xrtStrByteSize(sSubText, iSubSize);
	if ( sText == NULL || sSubText == NULL || iSize == 0 || iSubSize == 0 ) { return -1; }
	sFound = xrtFindStr(sText, iSize, sSubText, iSubSize, bCase);
	if ( sFound == NULL ) { return -1; }
	return (int64)__xrtStrByteToCharOffset(sText, iSize, (size_t)(sFound - sText));
}


// 从右向左查找子串位置（按 UTF-8 字符下标返回，未找到返回 -1）
XXAPI int64 xrtStrLastIndexOf(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase)
{
	str sSearchText;
	str sSearchSub;
	str sFound = NULL;
	size_t i;
	iSize = __xrtStrByteSize(sText, iSize);
	iSubSize = __xrtStrByteSize(sSubText, iSubSize);
	if ( sText == NULL || sSubText == NULL || iSize == 0 || iSubSize == 0 || iSubSize > iSize ) { return -1; }
	sSearchText = sText;
	sSearchSub = sSubText;
	if ( bCase ) {
		sSearchText = xrtLCase(sText, iSize, FALSE);
		sSearchSub = xrtLCase(sSubText, iSubSize, FALSE);
		if ( sSearchText == NULL || sSearchSub == NULL ) {
			xrtFree(sSearchText);
			xrtFree(sSearchSub);
			return -1;
		}
	}
	for ( i = 0; i + iSubSize <= iSize; ++i ) {
		if ( memcmp(sSearchText + i, sSearchSub, iSubSize) == 0 ) {
			sFound = sText + i;
		}
	}
	if ( bCase ) {
		xrtFree(sSearchText);
		xrtFree(sSearchSub);
	}
	if ( sFound == NULL ) { return -1; }
	return (int64)__xrtStrByteToCharOffset(sText, iSize, (size_t)(sFound - sText));
}


// 统计子串出现次数（非重叠，空子串返回 0）
XXAPI int64 xrtStrCount(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase)
{
	str sSearchText;
	str sSearchSub;
	size_t i;
	int64 iCount = 0;
	iSize = __xrtStrByteSize(sText, iSize);
	iSubSize = __xrtStrByteSize(sSubText, iSubSize);
	if ( sText == NULL || sSubText == NULL || iSize == 0 || iSubSize == 0 || iSubSize > iSize ) { return 0; }
	sSearchText = sText;
	sSearchSub = sSubText;
	if ( bCase ) {
		sSearchText = xrtLCase(sText, iSize, FALSE);
		sSearchSub = xrtLCase(sSubText, iSubSize, FALSE);
		if ( sSearchText == NULL || sSearchSub == NULL ) {
			xrtFree(sSearchText);
			xrtFree(sSearchSub);
			return 0;
		}
	}
	for ( i = 0; i + iSubSize <= iSize; ) {
		if ( memcmp(sSearchText + i, sSearchSub, iSubSize) == 0 ) {
			iCount++;
			i += iSubSize;
		} else {
			i++;
		}
	}
	if ( bCase ) {
		xrtFree(sSearchText);
		xrtFree(sSearchSub);
	}
	return iCount;
}


// xrtInStr 相关处理
XXAPI uint xrtInStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase)
{
	// 参数检查
	if ( sText == NULL ) { return 0; }
	if ( sSubText == NULL ) { return 0; }
	// 自动计算字符串长度
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0 ) { return 0; }
	if ( iSubSize == 0 ) { iSubSize = strlen(__xrt_cstr(sSubText)); }
	if ( iSubSize == 0 ) { return 0; }
	str sSub;
	if ( bCase ) {
		// 忽略大小写：将源字符串和子串都转为小写副本，在副本上搜索
		str sText1 = xrtLCase(sText, 0, FALSE);
		str sText2 = xrtLCase(sSubText, 0, FALSE);
		sSub = memmem(sText1, iSize, sText2, iSubSize);
		if ( sSub ) {
			// 将匹配位置映射回原始字符串的对应偏移
			sSub = &sText[sSub - sText1];
		}
		xrtFree(sText1);
		xrtFree(sText2);
	} else {
		// 精确匹配：直接在原字符串上搜索
		sSub = memmem(sText, iSize, sSubText, iSubSize);
	}
	// 返回 1 起始的位置索引，未找到返回 0
	if ( sSub ) {
		return (sSub - sText) + 1;
	} else {
		return 0;
	}
}



// 字符串检查（ sText 中是否包含 sSubText 列出的字符，支持 utf-8 mb6 编码 ）
XXAPI str xrtCheckStr(str sText, size_t iSize, str sSubText, size_t iSubSize)
{
	if ( (sText == NULL) || (sSubText == NULL) ) { return NULL; }
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0 ) { return NULL; }
	if ( iSubSize == 0 ) { iSubSize = strlen(__xrt_cstr(sSubText)); }
	if ( iSubSize == 0 ) { return NULL; }
	for ( size_t i = 0; i < iSize; ) {
		size_t iCharLen = __xrtUtf8CharLenSafe(sText, iSize, i);
		if ( __xrtStrHasToken(sSubText, iSubSize, &sText[i], iCharLen) ) {
			return &sText[i];
		}
		i += iCharLen;
	}
	return NULL;
}



// 裁剪字符串（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
XXAPI str xrtLTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise, size_t* iRetSize)
{
	// 参数检查
	if ( sText == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	// 自动计算字符串长度
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	// 默认裁剪空白字符（空格、制表、回车、换行）
	if ( sSubText == NULL ) { sSubText = (str)" \t\r\n"; iSubSize = 4; }
	if ( iSubSize == 0 ) { iSubSize = strlen(__xrt_cstr(sSubText)); }
	if ( iSubSize == 0 ) { sSubText = (str)" \t\r\n"; iSubSize = 4; }
	// 从左侧逐个 UTF-8 字符扫描，统计需要裁剪的字节数
	size_t iCount = 0;
	for ( size_t i = 0; i < iSize; ) {
		size_t iCharLen = __xrtUtf8CharLenSafe(sText, iSize, i);
		if ( !__xrtStrHasToken(sSubText, iSubSize, &sText[i], iCharLen) ) {
			break;
		}
		iCount += iCharLen;
		i += iCharLen;
	}
	if ( iRetSize ) { *iRetSize = iSize - iCount; }
	// 根据模式返回结果：原地修改或创建副本
	if ( bSrcRevise ) {
		if ( iCount > 0 ) {
			// 将剩余内容前移并添加结束符
			memmove(sText, &sText[iCount], iSize - iCount);
			sText[iSize - iCount] = 0;
		}
		return sText;
	} else {
		return xrtCopyStr(&sText[iCount], iSize - iCount);
	}
}


// xrtRTrim 相关处理
XXAPI str xrtRTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise, size_t* iRetSize)
{
	if ( sText == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	if ( sSubText == NULL ) { sSubText = (str)" \t\r\n"; iSubSize = 4; }
	if ( iSubSize == 0 ) { iSubSize = strlen(__xrt_cstr(sSubText)); }
	if ( iSubSize == 0 ) { sSubText = (str)" \t\r\n"; iSubSize = 4; }
	int iCount = 0;
	for ( int i = iSize - 1; i >= 0; i-- ) {
		int bBreak = TRUE;
		if ( (sText[i] & 0b10000000) == 0 ) {
			// ASCII 兼容字符
			for ( size_t j = 0; j < iSubSize; j++ ) {
				if ( sSubText[j] == sText[i] ) {
					iCount++;
					bBreak = FALSE;
					break;
				}
			}
		} else if ( (sText[i] & 0b11000000) == 0b10000000 ) {
			// UTF-8 续字节，向前查找首字节
			int iEnd = i;
			while ( (i > 0) && ((sText[i] & 0b11000000) == 0b10000000) ) {
				i--;
			}
			// 现在 i 指向首字节
			int iCharLen = iEnd - i + 1;
			if ( (size_t)iCharLen <= iSubSize ) {
				size_t iLen = iSubSize - iCharLen + 1;
				for ( size_t j = 0; j < iLen; j++ ) {
					bool bMatch = TRUE;
					for ( int k = 0; k < iCharLen; k++ ) {
						if ( sSubText[j + k] != sText[i + k] ) {
							bMatch = FALSE;
							break;
						}
					}
					if ( bMatch ) {
						iCount += iCharLen;
						bBreak = FALSE;
						break;
					}
				}
			}
			// i 已经在首字节，循环会 i-- 移到前一个字符末尾
		} else {
			// 孤立的首字节或异常字符（FE、FF），跳过
		}
		if ( bBreak ) {
			break;
		}
	}
	if ( iRetSize ) { *iRetSize = iSize - iCount; }
	if ( bSrcRevise ) {
		if ( iCount > 0 ) {
			sText[iSize - iCount] = 0;
		}
		return sText;
	} else {
		return xrtCopyStr(sText, iSize - iCount);
	}
}


// 裁剪
XXAPI str xrtTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise, size_t* iRetSize)
{
	if ( sText == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	if ( sSubText == NULL ) { sSubText = (str)" \t\r\n"; iSubSize = 4; }
	if ( iSubSize == 0 ) { iSubSize = strlen(__xrt_cstr(sSubText)); }
	if ( iSubSize == 0 ) { sSubText = (str)" \t\r\n"; iSubSize = 4; }
	size_t iCountL = 0;
	size_t iCountR = 0;
	// 裁剪左侧
	for ( size_t i = 0; i < iSize; ) {
		size_t iCharLen = __xrtUtf8CharLenSafe(sText, iSize, i);
		if ( !__xrtStrHasToken(sSubText, iSubSize, &sText[i], iCharLen) ) {
			break;
		}
		iCountL += iCharLen;
		i += iCharLen;
	}
	// 全部裁剪需要特殊处理
	if ( iCountL >= iSize ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	// 裁剪右侧
	for ( size_t i = iSize; i > iCountL; ) {
		size_t iStart = i - 1;
		size_t iCharLen = 1;
		while ( (iStart > iCountL) && (((unsigned char)sText[iStart] & 0b11000000u) == 0b10000000u) ) {
			iStart--;
		}
		iCharLen = i - iStart;
		if ( __xrtUtf8CharLenSafe(sText, iSize, iStart) != iCharLen ) {
			iStart = i - 1;
			iCharLen = 1;
		}
		if ( !__xrtStrHasToken(sSubText, iSubSize, &sText[iStart], iCharLen) ) {
			break;
		}
		iCountR += iCharLen;
		i = iStart;
	}
	size_t iCount = iCountL + iCountR;
	if ( iRetSize ) { *iRetSize = iSize - iCount; }
	if ( bSrcRevise ) {
		if ( iCount > 0 ) {
			memmove(sText, &sText[iCountL], iSize - iCount);
			sText[iSize - iCount] = 0;
		}
		return sText;
	} else {
		return xrtCopyStr(&sText[iCountL], iSize - iCount);
	}
}



// 过滤字符串（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
XXAPI str xrtFilterStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise, size_t* iRetSize)
{
	if ( sText == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	if ( sSubText == NULL ) { if ( bSrcRevise ) { if ( iRetSize ) { *iRetSize = iSize; } return sText; } else { if ( iRetSize ) { *iRetSize = iSize; } return xrtCopyStr(sText, iSize); } }
	if ( iSubSize == 0 ) { iSubSize = strlen(__xrt_cstr(sSubText)); }
	if ( iSubSize == 0 ) { if ( bSrcRevise ) { if ( iRetSize ) { *iRetSize = iSize; } return sText; } else { if ( iRetSize ) { *iRetSize = iSize; } return xrtCopyStr(sText, iSize); } }
	// 不改动源数据时，直接创建副本
	if ( bSrcRevise == FALSE ) {
		sText = xrtCopyStr(sText, iSize);
	}
	size_t iWrite = 0;
	for ( size_t i = 0; i < iSize; ) {
		size_t iCharLen = __xrtUtf8CharLenSafe(sText, iSize, i);
		if ( __xrtStrHasToken(sSubText, iSubSize, &sText[i], iCharLen) ) {
		} else {
			if ( iWrite != i ) {
				memmove(&sText[iWrite], &sText[i], iCharLen);
			}
			iWrite += iCharLen;
		}
		i += iCharLen;
	}
	sText[iWrite] = 0;
	if ( iRetSize ) { *iRetSize = iWrite; }
	return sText;
}



// 字符串格式化（ 需使用 xrtFree 释放 ）
XXAPI str xrtFormat(const void* sFormat, ...)
{
	const char* sFormatText = __xrt_cstr(sFormat);
	if ( sFormatText == NULL ) { return xCore.sNull; }
	va_list ip;
	va_start(ip, sFormat);
	int iSize = vsnprintf(NULL, 0, sFormatText, ip);
	va_end(ip);
	if ( iSize > 0 ) {
		str sRet = xrtMalloc(iSize + 1);
		if ( sRet == NULL ) { return xCore.sNull; }
		va_start(ip, sFormat);
		iSize = vsnprintf(__xrt_str(sRet), iSize + 1, sFormatText, ip);
		va_end(ip);
		sRet[iSize] = 0;
		return sRet;
	} else {
		return xCore.sNull;
	}
}



// 字符串替换（ 需使用 xrtFree 释放 ）
XXAPI str xrtReplace(str sText, size_t iSize, str sSubText, size_t iSubSize, str sRepText, size_t iRepSize, size_t* iRetSize)
{
	if ( sText == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	if ( sSubText == NULL ) { if ( iRetSize ) { *iRetSize = iSize; } return xrtCopyStr(sText, iSize); }
	if ( iSubSize == 0 ) { iSubSize = strlen(__xrt_cstr(sSubText)); }
	if ( iSubSize == 0 ) { if ( iRetSize ) { *iRetSize = iSize; } return xrtCopyStr(sText, iSize); }
	if ( sRepText == NULL ) { sRepText = xCore.sNull; iRepSize = 0; } else { if ( iRepSize == 0 ) { iRepSize = strlen(__xrt_cstr(sRepText)); } }
	// 计算 sSubText 在 sText 中出现的次数
	size_t iFindCount = 0;
	str sTextPtr;
	str sSubPos;
	for ( sTextPtr = sText; (sSubPos = memmem(sTextPtr, iSize - (size_t)(sTextPtr - sText), sSubText, iSubSize)); sTextPtr = sSubPos + iSubSize ) {
		iFindCount++;
	}
	// 为新字符串分配内存
	size_t iRet = iSize;
	if ( iFindCount > 0 ) {
		if ( iRepSize >= iSubSize ) {
			size_t iDelta = iRepSize - iSubSize;
			if ( iDelta > 0 && iFindCount > ((SIZE_MAX - iSize) / iDelta) ) {
				if ( iRetSize ) { *iRetSize = 0; }
				return (str)xCore.sNull;
			}
			iRet = iSize + iFindCount * iDelta;
		} else {
			iRet = iSize - iFindCount * (iSubSize - iRepSize);
		}
	}
	str sRet = (str)xrtMalloc(iRet + 1);
	if ( sRet == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return (str)xCore.sNull; }
	// 复制原始字符串, 替换需要改变的部分
	str sRetPtr = sRet;
	for ( sTextPtr = sText; (sSubPos = memmem(sTextPtr, iSize - (size_t)(sTextPtr - sText), sSubText, iSubSize)); sTextPtr = sSubPos + iSubSize ) {
		size_t iSkipSize = sSubPos - sTextPtr;
		// 复制前面的部分，直到出现要查找的字符串
		strncpy(__xrt_str(sRetPtr), __xrt_cstr(sTextPtr), iSkipSize);
		sRetPtr += iSkipSize;
		// 复制要替换的字符串
		strncpy(__xrt_str(sRetPtr), __xrt_cstr(sRepText), iRepSize);
		sRetPtr += iRepSize;
	}
	// 复制最后一段剩下的字符串
	if ( &sText[iSize] > sTextPtr ) {
		memcpy(sRetPtr, sTextPtr, &sText[iSize] - sTextPtr);
	}
	sRet[iRet] = 0;
	if ( iRetSize ) { *iRetSize = iRet; }
	return sRet;
}


// 截取字符串（start/count 按 UTF-8 字符计数，count < 0 表示直到末尾）
XXAPI str xrtStrSlice(str sText, size_t iSize, int64 iStart, int64 iCount, size_t* iRetSize)
{
	size_t iCharCount;
	size_t iStartChar;
	size_t iEndChar;
	size_t iStartByte;
	size_t iEndByte;
	iSize = __xrtStrByteSize(sText, iSize);
	if ( sText == NULL || iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	iCharCount = xrtStrCharLen(sText, iSize);
	iStartChar = __xrtStrNormalizeCharIndex(iCharCount, iStart);
	if ( iCount < 0 ) {
		iEndChar = iCharCount;
	} else {
		uint64 iEnd = (uint64)iStartChar + (uint64)iCount;
		iEndChar = iEnd > iCharCount ? iCharCount : (size_t)iEnd;
	}
	if ( iEndChar < iStartChar ) { iEndChar = iStartChar; }
	iStartByte = __xrtStrCharToByteOffset(sText, iSize, iStartChar);
	iEndByte = __xrtStrCharToByteOffset(sText, iSize, iEndChar);
	return __xrtStrCopyByteRange(sText, iStartByte, iEndByte - iStartByte, iRetSize);
}


// 截取左侧字符
XXAPI str xrtStrLeft(str sText, size_t iSize, int64 iCount, size_t* iRetSize)
{
	if ( iCount <= 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	return xrtStrSlice(sText, iSize, 0, iCount, iRetSize);
}


// 截取右侧字符
XXAPI str xrtStrRight(str sText, size_t iSize, int64 iCount, size_t* iRetSize)
{
	size_t iCharCount;
	if ( iCount <= 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	iSize = __xrtStrByteSize(sText, iSize);
	iCharCount = xrtStrCharLen(sText, iSize);
	if ( (uint64)iCount >= iCharCount ) {
		if ( iRetSize ) { *iRetSize = iSize; }
		return xrtCopyStr(sText, iSize);
	}
	return xrtStrSlice(sText, iSize, (int64)(iCharCount - (size_t)iCount), iCount, iRetSize);
}


// 截取中段字符
XXAPI str xrtStrMid(str sText, size_t iSize, int64 iStart, int64 iCount, size_t* iRetSize)
{
	return xrtStrSlice(sText, iSize, iStart, iCount, iRetSize);
}


// 获取指定字符
XXAPI str xrtStrCharAt(str sText, size_t iSize, int64 iIndex, size_t* iRetSize)
{
	return xrtStrSlice(sText, iSize, iIndex, 1, iRetSize);
}


// 获取指定字节，越界返回 -1
XXAPI int xrtStrByteAt(str sText, size_t iSize, int64 iIndex)
{
	size_t iPos;
	iSize = __xrtStrByteSize(sText, iSize);
	if ( sText == NULL || iSize == 0 ) { return -1; }
	iPos = __xrtStrNormalizeCharIndex(iSize, iIndex);
	if ( iPos >= iSize ) { return -1; }
	return (int)(unsigned char)sText[iPos];
}


// 重复字符串
XXAPI str xrtStrRepeat(str sText, size_t iSize, int64 iCount, size_t* iRetSize)
{
	size_t iRetSizeLocal;
	str sRet;
	size_t i;
	iSize = __xrtStrByteSize(sText, iSize);
	if ( sText == NULL || iSize == 0 || iCount <= 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	if ( (uint64)iCount > (SIZE_MAX / iSize) ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	iRetSizeLocal = iSize * (size_t)iCount;
	sRet = xrtMalloc(iRetSizeLocal + 1);
	if ( sRet == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	for ( i = 0; i < (size_t)iCount; ++i ) {
		memcpy(sRet + i * iSize, sText, iSize);
	}
	sRet[iRetSizeLocal] = 0;
	if ( iRetSize ) { *iRetSize = iRetSizeLocal; }
	return sRet;
}


// 是否为空字符串
XXAPI bool xrtStrIsEmpty(str sText, size_t iSize)
{
	return __xrtStrByteSize(sText, iSize) == 0;
}


// 是否为空白字符串（按 ASCII 空白判断）
XXAPI bool xrtStrIsBlank(str sText, size_t iSize)
{
	size_t i;
	iSize = __xrtStrByteSize(sText, iSize);
	if ( iSize == 0 ) { return TRUE; }
	for ( i = 0; i < iSize; ++i ) {
		if ( !isspace((unsigned char)sText[i]) ) { return FALSE; }
	}
	return TRUE;
}


// 删除所有匹配子串
XXAPI str xrtStrRemove(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase, size_t* iRetSize)
{
	if ( bCase ) {
		str sResult;
		str sSearchText;
		str sSearchSub;
		size_t i;
		size_t iRet = 0;
		size_t iCap;
		iSize = __xrtStrByteSize(sText, iSize);
		iSubSize = __xrtStrByteSize(sSubText, iSubSize);
		if ( sText == NULL || iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
		if ( sSubText == NULL || iSubSize == 0 || iSubSize > iSize ) {
			if ( iRetSize ) { *iRetSize = iSize; }
			return xrtCopyStr(sText, iSize);
		}
		sSearchText = xrtLCase(sText, iSize, FALSE);
		sSearchSub = xrtLCase(sSubText, iSubSize, FALSE);
		if ( sSearchText == NULL || sSearchSub == NULL ) {
			xrtFree(sSearchText);
			xrtFree(sSearchSub);
			if ( iRetSize ) { *iRetSize = 0; }
			return xCore.sNull;
		}
		iCap = iSize + 1;
		sResult = xrtMalloc(iCap);
		if ( sResult == NULL ) {
			xrtFree(sSearchText);
			xrtFree(sSearchSub);
			if ( iRetSize ) { *iRetSize = 0; }
			return xCore.sNull;
		}
		for ( i = 0; i < iSize; ) {
			if ( i + iSubSize <= iSize && memcmp(sSearchText + i, sSearchSub, iSubSize) == 0 ) {
				i += iSubSize;
			} else {
				sResult[iRet++] = sText[i++];
			}
		}
		sResult[iRet] = 0;
		xrtFree(sSearchText);
		xrtFree(sSearchSub);
		if ( iRetSize ) { *iRetSize = iRet; }
		return sResult;
	}
	return xrtReplace(sText, iSize, sSubText, iSubSize, xCore.sNull, 0, iRetSize);
}


// 按字符范围删除
XXAPI str xrtStrRemoveAt(str sText, size_t iSize, int64 iStart, int64 iCount, size_t* iRetSize)
{
	size_t iCharCount;
	size_t iStartChar;
	size_t iEndChar;
	size_t iStartByte;
	size_t iEndByte;
	str sRet;
	iSize = __xrtStrByteSize(sText, iSize);
	if ( sText == NULL || iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	if ( iCount <= 0 ) {
		if ( iRetSize ) { *iRetSize = iSize; }
		return xrtCopyStr(sText, iSize);
	}
	iCharCount = xrtStrCharLen(sText, iSize);
	iStartChar = __xrtStrNormalizeCharIndex(iCharCount, iStart);
	iEndChar = iStartChar + (size_t)iCount;
	if ( iEndChar > iCharCount ) { iEndChar = iCharCount; }
	iStartByte = __xrtStrCharToByteOffset(sText, iSize, iStartChar);
	iEndByte = __xrtStrCharToByteOffset(sText, iSize, iEndChar);
	sRet = xrtMalloc(iSize - (iEndByte - iStartByte) + 1);
	if ( sRet == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	memcpy(sRet, sText, iStartByte);
	memcpy(sRet + iStartByte, sText + iEndByte, iSize - iEndByte);
	sRet[iSize - (iEndByte - iStartByte)] = 0;
	if ( iRetSize ) { *iRetSize = iSize - (iEndByte - iStartByte); }
	return sRet;
}


// 按字符位置插入
XXAPI str xrtStrInsert(str sText, size_t iSize, int64 iStart, str sPartText, size_t iPartSize, size_t* iRetSize)
{
	size_t iCharCount;
	size_t iStartChar;
	size_t iStartByte;
	size_t iRet;
	str sRet;
	iSize = __xrtStrByteSize(sText, iSize);
	iPartSize = __xrtStrByteSize(sPartText, iPartSize);
	if ( sText == NULL ) { sText = xCore.sNull; iSize = 0; }
	if ( sPartText == NULL || iPartSize == 0 ) {
		if ( iRetSize ) { *iRetSize = iSize; }
		return xrtCopyStr(sText, iSize);
	}
	iCharCount = xrtStrCharLen(sText, iSize);
	iStartChar = __xrtStrNormalizeCharIndex(iCharCount, iStart);
	iStartByte = __xrtStrCharToByteOffset(sText, iSize, iStartChar);
	if ( iPartSize > SIZE_MAX - iSize ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	iRet = iSize + iPartSize;
	sRet = xrtMalloc(iRet + 1);
	if ( sRet == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	memcpy(sRet, sText, iStartByte);
	memcpy(sRet + iStartByte, sPartText, iPartSize);
	memcpy(sRet + iStartByte + iPartSize, sText + iStartByte, iSize - iStartByte);
	sRet[iRet] = 0;
	if ( iRetSize ) { *iRetSize = iRet; }
	return sRet;
}


// 左侧填充
XXAPI str xrtStrPadLeft(str sText, size_t iSize, int64 iWidth, str sPadText, size_t iPadSize, size_t* iRetSize)
{
	size_t iTextChars;
	size_t iNeedChars;
	size_t iPadBytes;
	size_t iRet;
	size_t iPos = 0;
	str sRet;
	iSize = __xrtStrByteSize(sText, iSize);
	iPadSize = __xrtStrByteSize(sPadText, iPadSize);
	if ( sText == NULL ) { sText = xCore.sNull; iSize = 0; }
	iTextChars = xrtStrCharLen(sText, iSize);
	if ( iWidth <= 0 || (uint64)iWidth <= iTextChars ) {
		if ( iRetSize ) { *iRetSize = iSize; }
		return xrtCopyStr(sText, iSize);
	}
	iNeedChars = (size_t)iWidth - iTextChars;
	iPadBytes = __xrtStrPadBytes(sPadText, iPadSize, iNeedChars);
	if ( iPadBytes > SIZE_MAX - iSize ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	iRet = iPadBytes + iSize;
	sRet = xrtMalloc(iRet + 1);
	if ( sRet == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	__xrtStrWritePad(sRet, &iPos, sPadText, iPadSize, iNeedChars);
	memcpy(sRet + iPos, sText, iSize);
	sRet[iRet] = 0;
	if ( iRetSize ) { *iRetSize = iRet; }
	return sRet;
}


// 右侧填充
XXAPI str xrtStrPadRight(str sText, size_t iSize, int64 iWidth, str sPadText, size_t iPadSize, size_t* iRetSize)
{
	size_t iTextChars;
	size_t iNeedChars;
	size_t iPadBytes;
	size_t iRet;
	size_t iPos = 0;
	str sRet;
	iSize = __xrtStrByteSize(sText, iSize);
	iPadSize = __xrtStrByteSize(sPadText, iPadSize);
	if ( sText == NULL ) { sText = xCore.sNull; iSize = 0; }
	iTextChars = xrtStrCharLen(sText, iSize);
	if ( iWidth <= 0 || (uint64)iWidth <= iTextChars ) {
		if ( iRetSize ) { *iRetSize = iSize; }
		return xrtCopyStr(sText, iSize);
	}
	iNeedChars = (size_t)iWidth - iTextChars;
	iPadBytes = __xrtStrPadBytes(sPadText, iPadSize, iNeedChars);
	if ( iPadBytes > SIZE_MAX - iSize ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	iRet = iSize + iPadBytes;
	sRet = xrtMalloc(iRet + 1);
	if ( sRet == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	memcpy(sRet, sText, iSize);
	iPos = iSize;
	__xrtStrWritePad(sRet, &iPos, sPadText, iPadSize, iNeedChars);
	sRet[iRet] = 0;
	if ( iRetSize ) { *iRetSize = iRet; }
	return sRet;
}


// 两侧填充
XXAPI str xrtStrPadCenter(str sText, size_t iSize, int64 iWidth, str sPadText, size_t iPadSize, size_t* iRetSize)
{
	size_t iTextChars;
	size_t iNeedChars;
	size_t iLeftChars;
	size_t iRightChars;
	size_t iLeftBytes;
	size_t iRightBytes;
	size_t iRet;
	size_t iPos = 0;
	str sRet;
	iSize = __xrtStrByteSize(sText, iSize);
	iPadSize = __xrtStrByteSize(sPadText, iPadSize);
	if ( sText == NULL ) { sText = xCore.sNull; iSize = 0; }
	iTextChars = xrtStrCharLen(sText, iSize);
	if ( iWidth <= 0 || (uint64)iWidth <= iTextChars ) {
		if ( iRetSize ) { *iRetSize = iSize; }
		return xrtCopyStr(sText, iSize);
	}
	iNeedChars = (size_t)iWidth - iTextChars;
	iLeftChars = iNeedChars / 2;
	iRightChars = iNeedChars - iLeftChars;
	iLeftBytes = __xrtStrPadBytes(sPadText, iPadSize, iLeftChars);
	iRightBytes = __xrtStrPadBytes(sPadText, iPadSize, iRightChars);
	if ( iLeftBytes > SIZE_MAX - iSize || iRightBytes > SIZE_MAX - iSize - iLeftBytes ) {
		if ( iRetSize ) { *iRetSize = 0; }
		return xCore.sNull;
	}
	iRet = iLeftBytes + iSize + iRightBytes;
	sRet = xrtMalloc(iRet + 1);
	if ( sRet == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	__xrtStrWritePad(sRet, &iPos, sPadText, iPadSize, iLeftChars);
	memcpy(sRet + iPos, sText, iSize);
	iPos += iSize;
	__xrtStrWritePad(sRet, &iPos, sPadText, iPadSize, iRightChars);
	sRet[iRet] = 0;
	if ( iRetSize ) { *iRetSize = iRet; }
	return sRet;
}


// 按 UTF-8 字符反转字符串
XXAPI str xrtStrReverse(str sText, size_t iSize, size_t* iRetSize)
{
	str sRet;
	size_t iPos = 0;
	size_t iOutPos;
	iSize = __xrtStrByteSize(sText, iSize);
	if ( sText == NULL || iSize == 0 ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	sRet = xrtMalloc(iSize + 1);
	if ( sRet == NULL ) { if ( iRetSize ) { *iRetSize = 0; } return xCore.sNull; }
	iOutPos = iSize;
	while ( iPos < iSize ) {
		size_t iCharLen = __xrtUtf8CharLenSafe(sText, iSize, iPos);
		if ( iCharLen == 0 ) { break; }
		iOutPos -= iCharLen;
		memcpy(sRet + iOutPos, sText + iPos, iCharLen);
		iPos += iCharLen;
	}
	sRet[iSize] = 0;
	if ( iRetSize ) { *iRetSize = iSize; }
	return sRet;
}


// 按行分割字符串，支持 \n、\r\n、\r
XXAPI str* xrtStrSplitLines(str sText, size_t iSize, size_t* iRetSize)
{
	size_t iCount = 1;
	size_t i;
	size_t iDataSize;
	size_t iHeaderSize;
	str* sRet;
	str pData;
	size_t iItem = 0;
	size_t iLineStart = 0;
	size_t iOutPos = 0;
	iSize = __xrtStrByteSize(sText, iSize);
	if ( iRetSize ) { *iRetSize = 0; }
	if ( sText == NULL || iSize == 0 ) {
		sRet = xrtMalloc(2 * sizeof(void*));
		if ( sRet == NULL ) { return NULL; }
		sRet[0] = xCore.sNull;
		sRet[1] = NULL;
		if ( iRetSize ) { *iRetSize = 1; }
		return sRet;
	}
	for ( i = 0; i < iSize; ++i ) {
		if ( sText[i] == '\n' ) {
			iCount++;
		} else if ( sText[i] == '\r' ) {
			iCount++;
			if ( i + 1 < iSize && sText[i + 1] == '\n' ) { i++; }
		}
	}
	iHeaderSize = (iCount + 1) * sizeof(void*);
	iDataSize = iSize + 1;
	sRet = xrtMalloc(iHeaderSize + iDataSize);
	if ( sRet == NULL ) { return NULL; }
	pData = (str)((char*)sRet + iHeaderSize);
	for ( i = 0; i <= iSize; ++i ) {
		bool bLineEnd = i == iSize || sText[i] == '\n' || sText[i] == '\r';
		if ( bLineEnd ) {
			size_t iLineLen = i - iLineStart;
			sRet[iItem++] = pData + iOutPos;
			if ( iLineLen > 0 ) {
				memcpy(pData + iOutPos, sText + iLineStart, iLineLen);
				iOutPos += iLineLen;
			}
			pData[iOutPos++] = 0;
			if ( i < iSize && sText[i] == '\r' && i + 1 < iSize && sText[i + 1] == '\n' ) {
				i++;
			}
			iLineStart = i + 1;
		}
	}
	sRet[iItem] = NULL;
	if ( iRetSize ) { *iRetSize = iItem; }
	return sRet;
}



// 字符串分割（ 任何情况返回值都必须使用 xrtFree 释放，bSrcRevise 设置为 TRUE 时会破坏原数据 ）
XXAPI str* xrtSplit(str sText, size_t iSize, str sSepText, size_t iSepSize, bool bSrcRevise, size_t* iRetSize)
{
	if ( sText == NULL ) { goto return_nullstr; }
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0 ) { goto return_nullstr; }
	if ( sSepText == NULL ) { goto return_nullsep; }
	if ( iSepSize == 0 ) { iSepSize = strlen(__xrt_cstr(sSepText)); }
	if ( iSepSize == 0 ) { goto return_nullsep; }
	// 统计分隔符出现的次数
	int iCount = 0;
	for ( size_t i = 0; i < iSize; i++ ) {
		str pPos = &sText[i];
		int bOK = TRUE;
		for ( size_t j = 0; j < iSepSize; j++ ) {
			if ( pPos[j] != sSepText[j] ) {
				bOK = FALSE;
				break;
			}
		}
		if ( bOK ) {
			iCount++;
			i += iSepSize - 1;
		}
	}
	// 如果字符串没有被分割，按照分隔符为空处理
	if ( iCount == 0 ) {
		goto return_nullsep;
	}
	// 准备返回的数据 [分割指针 + NULL + 字符串表 + \0]
	str* sRet;
	str pData;
	if ( bSrcRevise ) {
		sRet = xrtMalloc( (iCount + 2) * sizeof(ptr) );
		if ( sRet == NULL ) {
			goto return_nullstr;
		}
		pData = sText;
	} else {
		sRet = xrtMalloc( ((iCount + 2) * sizeof(ptr)) + (iSize - ((iSepSize - 1) * iCount)) + 1 );
		if ( sRet == NULL ) {
			goto return_nullstr;
		}
		pData = (str)&sRet[iCount + 2];
	}
	// 开始分割数据
	iCount = 0;
	int iPos = 0;
	str pAddr = pData;
	for ( size_t i = 0; i < iSize; i++ ) {
		str pPos = &sText[i];
		int bOK = TRUE;
		for ( size_t j = 0; j < iSepSize; j++ ) {
			if ( pPos[j] != sSepText[j] ) {
				bOK = FALSE;
				break;
			}
		}
		if ( bOK ) {
			// 找到分隔符
			sRet[iCount] = pAddr;
			iCount++;
			if ( bSrcRevise ) {
				pData[i] = 0;
				pAddr = pData + i + iSepSize;
			} else {
				pData[iPos] = 0;
				iPos++;
				pAddr = &pData[iPos];
			}
			i += iSepSize - 1;
		} else {
			// 没找到分隔符（不修改源数据时负责数据拷贝）
			if ( bSrcRevise == FALSE ) {
				pData[iPos] = sText[i];
				iPos++;
			}
		}
	}
	if ( bSrcRevise == FALSE ) {
		pData[iPos] = 0;
	}
	sRet[iCount] = pAddr;
	iCount++;
	sRet[iCount] = NULL;
	if ( iRetSize ) { *iRetSize = iCount; }
	return sRet;
	
// 处理内容为 空字符串 或 NULL 的情况（只返回包含一个空元素的数组）
return_nullstr:
	sRet = xrtMalloc(2 * sizeof(void*));
	if ( sRet == NULL ) {
		goto return_error;
	}
	sRet[0] = xCore.sNull;
	sRet[1] = NULL;
	if ( iRetSize ) { *iRetSize = 1; }
	return sRet;
	
// 处理分隔符为 空字符串 或 NULL 的情况（只返回包含一个内容的数组）
return_nullsep:
	if ( bSrcRevise ) {
		sRet = xrtMalloc(2 * sizeof(void*));
		if ( sRet == NULL ) {
			goto return_error;
		}
		sRet[0] = sText;
	} else {
		sRet = xrtMalloc((2 * sizeof(void*)) + iSize + 1);
		if ( sRet == NULL ) {
			goto return_error;
		}
		str sTextRef = (str)&sRet[2];
		memcpy(sTextRef, sText, iSize);
		sTextRef[iSize] = 0;
		sRet[0] = sTextRef;
	}
	sRet[1] = NULL;
	if ( iRetSize ) { *iRetSize = 1; }
	return sRet;
	
// 内存申请异常返回
return_error:
	if ( iRetSize ) { *iRetSize = 0; }
	return (str*)xCore.sNull;
}



// 生成随机字符串（ 需使用 xrtFree 释放 ）
static const str RandStringDefaultTemplate = (str)"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";


// xrtRandStr 相关处理
static str __xrtRandStrEx(xrand* pRand, str sTemplate, size_t iSize, size_t iLen)
{
	if ( sTemplate == NULL ) { sTemplate = RandStringDefaultTemplate; iSize = 64; }
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sTemplate)); }
	if ( iSize == 0 ) { sTemplate = RandStringDefaultTemplate; iSize = 64; }
	if ( iLen == 0 ) { return xCore.sNull; }
	iSize--;
	str sRet = xrtMalloc(iLen + 1);
	if ( sRet == NULL ) {
		return xCore.sNull;
	}
	for ( size_t i = 0; i < iLen; i++ ) {
		int idx = pRand != NULL ? xrtRandRangeEx(pRand, 0, iSize) : xrtRandRange(0, iSize);
		sRet[i] = sTemplate[idx];
	}
	sRet[iLen] = 0;
	return sRet;
}



XXAPI str xrtRandStr(str sTemplate, size_t iSize, size_t iLen)
{
	return __xrtRandStrEx(NULL, sTemplate, iSize, iLen);
}



XXAPI str xrtRandStrObj(ptr pRand, str sTemplate, size_t iSize, size_t iLen)
{
	if ( pRand == NULL ) {
		xrtSetError("random object is null.", FALSE);
		return xCore.sNull;
	}
	return __xrtRandStrEx(&((xrtRandObj*)pRand)->rand32, sTemplate, iSize, iLen);
}



// HEX 编码（ 需使用 xrtFree 释放 ）
#define dec2hex(c) (c > 9 ? c + 55 : c + '0')
// xrtHexEncode 相关处理
XXAPI str xrtHexEncode(ptr pMem, size_t iSize)
{
	if ( pMem == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen((const char*)pMem); }
	if ( iSize == 0 ) { return xCore.sNull; }
	str sRet = xrtMalloc((iSize * 2) + 1);
	if ( sRet == NULL ) { return xCore.sNull; }
	uint8* pStr = pMem;
	int iPos = 0;
	for ( size_t i = 0; i < iSize; i++ ) {
		int i1 = (pStr[i] & 0xF0) >> 4;
		int i2 = pStr[i] & 0x0F;
		sRet[iPos++] = dec2hex(i1);
		sRet[iPos++] = dec2hex(i2);
	}
	sRet[iPos] = 0;
	return sRet;
}



// xrtHexDecode 相关处理
// xrtHexEncodeBytes 相关处理
XXAPI str xrtHexEncodeBytes(ptr pMem, size_t iSize)
{
	return xrtHexEncode(pMem, iSize);
}


XXAPI ptr xrtHexDecode(str sText, size_t iSize)
{
	// 参数检查
	if ( sText == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0 ) { return xCore.sNull; }
	// HEX 编码长度必须为偶数（每两个字符对应一个字节）
	if ( (iSize & 1u) != 0 ) { return xCore.sNull; }
	// 分配解码后的缓冲区（长度为编码长度的一半）
	str sRet = xrtMalloc((iSize / 2) + 1);
	if ( sRet == NULL ) { return xCore.sNull; }
	int iPos = 0;
	// 逐对解析：每两个 HEX 字符解码为一个字节
	for ( size_t i = 0; i < iSize; i += 2 ) {
		uint8 c0 = (uint8)sText[i];
		uint8 c1 = (uint8)sText[i + 1];
		uint8 iHigh;
		uint8 iLow;
		// 将两个 HEX 字符分别转换为 4 位数值
		if ( !__xrtHexDecodeNibble(c0, &iHigh) || !__xrtHexDecodeNibble(c1, &iLow) ) {
			xrtFree(sRet);
			return xCore.sNull;
		}
		// 算法：高 4 位左移 4 位，与低 4 位按位或，组合成一个字节
		sRet[iPos++] = (char)((iHigh << 4) | iLow);
	}
	sRet[iPos] = 0;
	return sRet;
}



// Base64 编码（ 需使用 xrtFree 释放 ）
static const str Base64EncodeTable = (str)"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


// xrtBase64Encode 相关处理
XXAPI str xrtBase64Encode(ptr pMem, size_t iSize, str sTable)
{
	if ( pMem == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen((const char*)pMem); }
	if ( iSize == 0 ) { return xCore.sNull; }
	if ( sTable == NULL ) { sTable = Base64EncodeTable; }
	// 申请返回值内存
	size_t iRet= 4 * ((iSize + 2) / 3);
	str sRet = xrtMalloc(iRet + 1);
	if ( sRet == NULL ) { return xCore.sNull; }
	// 开始编码
	uint8* pStr = pMem;
	for ( size_t i = 0, j = 0; i < iSize; ) {
		uint32_t octet_a = i < iSize ? pStr[i++] : 0;
		uint32_t octet_b = i < iSize ? pStr[i++] : 0;
		uint32_t octet_c = i < iSize ? pStr[i++] : 0;
		uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;
		sRet[j++] = sTable[(triple >> 3 * 6) & 0x3F];
		sRet[j++] = sTable[(triple >> 2 * 6) & 0x3F];
		sRet[j++] = sTable[(triple >> 1 * 6) & 0x3F];
		sRet[j++] = sTable[(triple >> 0 * 6) & 0x3F];
	}
	// 添加填充字符 '='
	for ( size_t i = 0; i < (3 - iSize % 3) % 3; i++ ) {
		sRet[iRet - 1 - i] = '=';
	}
	// 返回编码后的数据
	sRet[iRet] = 0;
	return sRet;
}



// Base64 解码（ 需使用 xrtFree 释放 ）
// xrtBase64EncodeBytes 相关处理
XXAPI str xrtBase64EncodeBytes(ptr pMem, size_t iSize, str sTable)
{
	return xrtBase64Encode(pMem, iSize, sTable);
}


static const str sErrorBase64_mul4 = (str)"Base64 input length must be multiple of 4 !";
static const str sErrorBase64_char = (str)"Base64 input contains invalid characters !";


// xrtBase64Decode 相关处理
XXAPI ptr xrtBase64Decode(str sText, size_t iSize, str sTable)
{
	if ( sText == NULL ) { return xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0 ) { return xCore.sNull; }
	int8_t Base64DecodeTable[256];
	memset(Base64DecodeTable, -1, sizeof(Base64DecodeTable));
	if ( sTable == NULL ) { sTable = Base64EncodeTable; }
	for ( int i = 0; i < 64; i++ ) {
		Base64DecodeTable[(unsigned char)sTable[i]] = i;
	}
	// 计算输出缓冲区大小
	if ( iSize % 4 != 0 ) {
		xrtSetError(sErrorBase64_mul4, FALSE);
		return xCore.sNull;
	}
	// 计算返回长度
	int iRet = iSize / 4 * 3;
	if ( sText[iSize - 1] == '=' ) { iRet--; }
	if ( sText[iSize - 2] == '=' ) { iRet--; }
	// 申请返回值内存
	str sRet = xrtMalloc(iRet + 1);
	if ( sRet == NULL ) {
		return xCore.sNull;
	}
	// 开始解码
	for ( size_t i = 0, j = 0; i < iSize; ) {
		unsigned char cA = (unsigned char)sText[i];
		int8_t sextet_a = cA == '=' ? 0 : Base64DecodeTable[cA];
		i++;
		unsigned char cB = (unsigned char)sText[i];
		int8_t sextet_b = cB == '=' ? 0 : Base64DecodeTable[cB];
		i++;
		unsigned char cC = (unsigned char)sText[i];
		int8_t sextet_c = cC == '=' ? 0 : Base64DecodeTable[cC];
		i++;
		unsigned char cD = (unsigned char)sText[i];
		int8_t sextet_d = cD == '=' ? 0 : Base64DecodeTable[cD];
		i++;
		// 发现非法字符
		if (sextet_a == -1 || sextet_b == -1 || sextet_c == -1 || sextet_d == -1) {
			xrtSetError(sErrorBase64_char, FALSE);
			xrtFree(sRet);
			return xCore.sNull;
		}
		// 组合 4 个 6 位值为 3 个 8 位字节
		uint32_t triple = (sextet_a << 3 * 6) + (sextet_b << 2 * 6) + (sextet_c << 1 * 6) + (sextet_d << 0 * 6);
		if ( j < (size_t)iRet ) { sRet[j++] = (triple >> 2 * 8) & 0xFF; }
		if ( j < (size_t)iRet ) { sRet[j++] = (triple >> 1 * 8) & 0xFF; }
		if ( j < (size_t)iRet ) { sRet[j++] = (triple >> 0 * 8) & 0xFF; }
	}
	sRet[iRet] = '\0';
	return sRet;
}



// 通配符匹配（ * 匹配任意字符序列，? 匹配单个UTF-8字符，bCase 为 TRUE 时忽略大小写 ）
// 使用贪婪匹配算法：O(n*m) 最坏时间复杂度，O(1) 空间复杂度
XXAPI bool xrtStrLike(str sText, size_t iTextSize, str sPattern, size_t iPatSize, bool bCase)
{
	// 参数检查
	if ( sPattern == NULL ) { return FALSE; }
	if ( iPatSize == 0 ) { iPatSize = strlen(__xrt_cstr(sPattern)); }
	
	// 空模式只匹配空字符串
	if ( iPatSize == 0 ) {
		if ( sText == NULL ) { return TRUE; }
		if ( iTextSize == 0 ) { iTextSize = strlen(__xrt_cstr(sText)); }
		return iTextSize == 0;
	}
	
	// 处理空文本
	if ( sText == NULL ) {
		// 空文本只能匹配全是 * 的模式
		for ( size_t i = 0; i < iPatSize; i++ ) {
			if ( sPattern[i] != '*' ) { return FALSE; }
		}
		return TRUE;
	}
	if ( iTextSize == 0 ) { iTextSize = strlen(__xrt_cstr(sText)); }
	if ( iTextSize == 0 ) {
		for ( size_t i = 0; i < iPatSize; i++ ) {
			if ( sPattern[i] != '*' ) { return FALSE; }
		}
		return TRUE;
	}
	
	// 贪婪匹配算法
	size_t t = 0;           // 文本位置
	size_t p = 0;           // 模式位置
	size_t starP = (size_t)-1;   // 最近的 * 在模式中的位置
	size_t starT = 0;       // 遇到 * 时文本的位置
	
	while ( t < iTextSize ) {
		if ( p < iPatSize && sPattern[p] == '*' ) {
			// 记录 * 的位置，先假定它匹配 0 个字符
			starP = p;
			starT = t;
			p++;
		} else if ( p < iPatSize && sPattern[p] == '?' ) {
			// ? 匹配一个完整的 UTF-8 字符
			int charLen = xrtCharLenU8((unsigned char)sText[t]);
			// 检查剩余长度是否足够
			if ( t + charLen > iTextSize ) {
				// 字符不完整，尝试回溯
				if ( starP == (size_t)-1 ) { return FALSE; }
				if ( starT >= iTextSize ) { return FALSE; }
				p = starP + 1;
				starT += xrtCharLenU8((unsigned char)sText[starT]);
				t = starT;
				if ( starT > iTextSize ) { return FALSE; }
			} else {
				t += charLen;
				p++;
			}
		} else {
			// 普通字符匹配（内联字符比较）
			unsigned char c1 = (unsigned char)sText[t];
			unsigned char c2;
			if ( p >= iPatSize ) {
				if ( starP == (size_t)-1 ) { return FALSE; }
				if ( starT >= iTextSize ) { return FALSE; }
				p = starP + 1;
				starT += xrtCharLenU8((unsigned char)sText[starT]);
				t = starT;
				if ( starT > iTextSize ) { return FALSE; }
				continue;
			}
			c2 = (unsigned char)sPattern[p];
			bool bMatch = (c1 == c2);
			if ( !bMatch && bCase ) {
				// 大小写不敏感：只对 ASCII 字母转换
				if ( c1 >= 'A' && c1 <= 'Z' ) { c1 += 32; }
				if ( c2 >= 'A' && c2 <= 'Z' ) { c2 += 32; }
				bMatch = (c1 == c2);
			}
			if ( p < iPatSize && bMatch ) {
				t++;
				p++;
			} else {
				// 匹配失败，回溯到上一个 *
				if ( starP == (size_t)-1 ) { return FALSE; }
				// 让 * 多匹配一个 UTF-8 字符
				if ( starT >= iTextSize ) { return FALSE; }
				p = starP + 1;
				starT += xrtCharLenU8((unsigned char)sText[starT]);
				t = starT;
				// 如果 starT 已超出文本范围，则匹配失败
				if ( starT > iTextSize ) { return FALSE; }
			}
		}
	}
	
	// 文本已匹配完，检查模式剩余部分是否全是 *
	while ( p < iPatSize && sPattern[p] == '*' ) {
		p++;
	}
	
	return p == iPatSize;
}



// ============================================================================
// 数值格式化函数
// ============================================================================

// 查表法: 十六进制字符表
static const char xrt_digit_table[] = "0123456789abcdef0123456789ABCDEF";

// 内部函数: 解析格式字符串
typedef struct {
	bool showSign;      // 显示正号
	bool thousands;     // 千分位
	bool percent;       // 百分比
	bool uppercase;     // 大写十六进制
	int base;           // 进制 (10, 16, 8, 2)
	int width;          // 前导零宽度
	int precision;      // 小数位数 （ -1 表示未指定 ）
} XrtNumFmtOpts;


// xrt_parse_format 相关处理
static inline void xrt_parse_format(str format, XrtNumFmtOpts* opts)
{
	opts->showSign = FALSE;
	opts->thousands = FALSE;
	opts->percent = FALSE;
	opts->uppercase = FALSE;
	opts->base = 10;
	opts->width = 0;
	opts->precision = -1;
	
	if ( format == NULL || *format == '\0' ) { return; }
	
	const char* p = __xrt_cstr(format);
	while ( *p ) {
		char c = *p++;
		switch ( c ) {
			case '+': opts->showSign = TRUE; break;
			case ',': opts->thousands = TRUE; break;
			case '%': opts->percent = TRUE; break;
			case 'x': opts->base = 16; opts->uppercase = FALSE; break;
			case 'X': opts->base = 16; opts->uppercase = TRUE; break;
			case 'o': opts->base = 8; break;
			case 'b': case 'B': opts->base = 2; break;
			case '.':
				// 解析小数位数
				opts->precision = 0;
				while ( *p >= '0' && *p <= '9' ) {
					opts->precision = opts->precision * 10 + (*p++ - '0');
				}
				break;
			case '0':
				// 解析前导零宽度
				while ( *p >= '0' && *p <= '9' ) {
					opts->width = opts->width * 10 + (*p++ - '0');
				}
				break;
			default:
				if ( c >= '1' && c <= '9' ) {
					// 数字开头也解析为宽度
					opts->width = c - '0';
					while ( *p >= '0' && *p <= '9' ) {
						opts->width = opts->width * 10 + (*p++ - '0');
					}
				}
				break;
		}
	}
}

// 内部函数: uint64 转非十进制字符串（从 buffer 末尾往前写）
static inline char* xrt_u64_to_base(char* bufEnd, uint64 value, int base, bool upper)
{
	char* p = bufEnd;
	const char* digits = upper ? &xrt_digit_table[16] : xrt_digit_table;
	
	if ( base == 16 ) {
		do {
			*--p = digits[value & 0xF];
			value >>= 4;
		} while ( value );
	} else if ( base == 8 ) {
		do {
			*--p = '0' + (char)(value & 0x7);
			value >>= 3;
		} while ( value );
	} else {
		// 二进制
		do {
			*--p = '0' + (char)(value & 0x1);
			value >>= 1;
		} while ( value );
	}
	
	return p;
}

// 内部函数: 添加千分位分隔符
static inline int xrt_add_thousands(char* dst, const char* src, int srcLen)
{
	int commas = (srcLen - 1) / 3;  // 需要插入的逗号数量
	int totalLen = srcLen + commas;
	int pos = totalLen;
	int cnt = 0;
	
	for ( int i = srcLen - 1; i >= 0; i-- ) {
		dst[--pos] = src[i];
		if ( ++cnt == 3 && i > 0 ) {
			dst[--pos] = ',';
			cnt = 0;
		}
	}
	
	return totalLen;
}

// 整数格式化
XXAPI str xrtIntFormat(int64 value, str format)
{
	// 解析格式
	XrtNumFmtOpts opts;
	xrt_parse_format(format, &opts);
	
	// 处理符号
	bool negative = (value < 0);
	uint64 absVal = negative ? (uint64)(-(value + 1)) + 1 : (uint64)value;
	
	// 转换为字符串
	char tmpBuf[96];
	char* numStart;
	int numLen;
	
	if ( opts.base == 10 ) {
		// 十进制: 使用 xrtI64ToStr
		numLen = xrtI64ToStr(negative ? value : (int64)absVal, tmpBuf);
		numStart = tmpBuf;
		if ( negative ) {
			numStart++;  // 跳过负号
			numLen--;
		}
	} else {
		// 非十进制
		char* tmpEnd = tmpBuf + sizeof(tmpBuf);
		numStart = xrt_u64_to_base(tmpEnd, absVal, opts.base, opts.uppercase);
		numLen = (int)(tmpEnd - numStart);
		opts.showSign = FALSE;
		opts.thousands = FALSE;
		negative = FALSE;
	}
	
	// 计算所需总长度
	int signLen = (negative || opts.showSign) ? 1 : 0;
	int digitLen = numLen;
	if ( opts.thousands && digitLen > 3 ) {
		digitLen += (digitLen - 1) / 3;
	}
	int padLen = (opts.width > digitLen) ? (opts.width - digitLen) : 0;
	int totalLen = signLen + padLen + digitLen;
	
	// 分配缓冲区
	str buffer = xrtMalloc(totalLen + 1);
	if ( buffer == NULL ) { return xCore.sNull; }
	
	char* out = __xrt_str(buffer);
	
	// 写入符号
	if ( signLen ) {
		*out++ = negative ? '-' : '+';
	}
	
	// 写入前导零
	while ( padLen-- > 0 ) { *out++ = '0'; }
	
	// 写入数字
	if ( opts.thousands && numLen > 3 ) {
		xrt_add_thousands(out, numStart, numLen);
		out += digitLen;
	} else {
		memcpy(out, numStart, numLen);
		out += numLen;
	}
	
	*out = '\0';
	return buffer;
}

// 浮点数格式化
XXAPI str xrtNumFormat(double value, str format)
{
	// 解析格式
	XrtNumFmtOpts opts;
	xrt_parse_format(format, &opts);
	
	// 处理百分比
	if ( opts.percent ) {
		value *= 100.0;
	}
	
	// 处理特殊值
	if ( value != value ) {  // NaN
		str ret = xrtMalloc(4);
		if ( ret == NULL ) { return xCore.sNull; }
		memcpy(ret, "NaN", 4);
		return ret;
	}
	if ( isinf(value) && value > 0.0 ) {
		str ret = xrtMalloc(4);
		if ( ret == NULL ) { return xCore.sNull; }
		memcpy(ret, "Inf", 4);
		return ret;
	}
	if ( isinf(value) && value < 0.0 ) {
		str ret = xrtMalloc(5);
		if ( ret == NULL ) { return xCore.sNull; }
		memcpy(ret, "-Inf", 5);
		return ret;
	}
	
	// 处理符号
	bool negative = (value < 0);
	if ( negative ) { value = -value; }
	
	// 确定小数位数
	int precision = (opts.precision >= 0) ? opts.precision : 6;
	if ( precision > 15 ) { precision = 15; }
	
	// 四舍五入
	static const double roundTable[] = {
		0.5, 0.05, 0.005, 0.0005, 0.00005, 0.000005, 0.0000005, 0.00000005,
		0.000000005, 0.0000000005, 0.00000000005, 0.000000000005,
		0.0000000000005, 0.00000000000005, 0.000000000000005, 0.0000000000000005
	};
	value += roundTable[precision];
	
	// 分离整数部分和小数部分
	uint64 intPart = (uint64)value;
	double fracPart = value - (double)intPart;
	
	// 转换整数部分: 使用 xrtI64ToStr
	char tmpBuf[32];
	int intLen = xrtI64ToStr((int64)intPart, tmpBuf);
	char* intStart = tmpBuf;
	
	// 计算所需总长度
	int signLen = (negative || opts.showSign) ? 1 : 0;
	int intDigitLen = intLen;
	if ( opts.thousands && intDigitLen > 3 ) {
		intDigitLen += (intDigitLen - 1) / 3;
	}
	int fracLen = (precision > 0) ? (1 + precision) : 0;
	int percentLen = opts.percent ? 1 : 0;
	int totalLen = signLen + intDigitLen + fracLen + percentLen;
	
	// 分配缓冲区
	str buffer = xrtMalloc(totalLen + 1);
	if ( buffer == NULL ) { return xCore.sNull; }
	
	char* out = __xrt_str(buffer);
	
	// 写入符号
	if ( negative ) {
		*out++ = '-';
	} else if ( opts.showSign ) {
		*out++ = '+';
	}
	
	// 写入整数部分
	if ( opts.thousands && intLen > 3 ) {
		xrt_add_thousands(out, intStart, intLen);
		out += intDigitLen;
	} else {
		memcpy(out, intStart, intLen);
		out += intLen;
	}
	
	// 写入小数部分
	if ( precision > 0 ) {
		*out++ = '.';
		for ( int i = 0; i < precision; i++ ) {
			fracPart *= 10.0;
			int digit = (int)fracPart;
			*out++ = '0' + digit;
			fracPart -= digit;
		}
	}
	
	// 写入百分号
	if ( opts.percent ) {
		*out++ = '%';
	}
	
	*out = '\0';
	return buffer;
}



// 字符串相似度（基于 Levenshtein 编辑距离，返回 0.0-1.0）
// 高性能优化：一维数组 O(min(m,n)) 空间，内联 min 计算
XXAPI double xrtStrSim(str s1, size_t len1, str s2, size_t len2)
{
	// 空指针检查
	if ( s1 == NULL ) { s1 = (str)""; len1 = 0; }
	if ( s2 == NULL ) { s2 = (str)""; len2 = 0; }
	
	// 自动计算长度
	if ( len1 == 0 ) { len1 = strlen(__xrt_cstr(s1)); }
	if ( len2 == 0 ) { len2 = strlen(__xrt_cstr(s2)); }
	
	// 快速路径：空字符串
	if ( len1 == 0 && len2 == 0 ) { return 1.0; }
	if ( len1 == 0 ) { return 0.0; }
	if ( len2 == 0 ) { return 0.0; }
	
	// 快速路径：完全相同
	if ( len1 == len2 && memcmp(s1, s2, len1) == 0 ) { return 1.0; }
	
	// 确保 s1 是较长的字符串（优化内存访问）
	if ( len1 < len2 ) {
		str ts = s1; s1 = s2; s2 = ts;
		size_t tl = len1; len1 = len2; len2 = tl;
	}
	
	// 分配一维 DP 数组（只需要较短字符串的长度+1）
	size_t dpSize = len2 + 1;
	int* dp = (int*)xrtMalloc(dpSize * sizeof(int));
	if ( dp == NULL ) { return 0.0; }
	
	// 初始化第一行：dp[j] = j
	for ( size_t j = 0; j <= len2; j++ ) {
		dp[j] = (int)j;
	}
	
	// DP 计算（行优先遍历，对缓存友好）
	for ( size_t i = 1; i <= len1; i++ ) {
		int prev = dp[0];  // 保存 dp[i-1][j-1]
		dp[0] = (int)i;    // dp[i][0] = i
		
		unsigned char c1 = (unsigned char)s1[i - 1];
		
		for ( size_t j = 1; j <= len2; j++ ) {
			int temp = dp[j];  // 保存当前值，作为下一次迭代的 prev
			
			if ( c1 == (unsigned char)s2[j - 1] ) {
				// 字符相同，无需操作
				dp[j] = prev;
			} else {
				// 字符不同，取 min（ 删除, 插入, 替换 ） + 1
				int del = dp[j];      // dp[i-1][j] + 1
				int ins = dp[j - 1];  // dp[i][j-1] + 1
				int rep = prev;       // dp[i-1][j-1] + 1
				
				// 内联 min3 计算
				int minVal = del;
				if ( ins < minVal ) { minVal = ins; }
				if ( rep < minVal ) { minVal = rep; }
				
				dp[j] = minVal + 1;
			}
			
			prev = temp;
		}
	}
	
	// 获取编辑距离
	int distance = dp[len2];
	xrtFree(dp);
	
	// 计算相似度：1 - distance / maxLen
	size_t maxLen = len1;  // len1 >= len2
	return 1.0 - (double)distance / (double)maxLen;
}



// 字符串约等于（使用 xCore 配置）
// iApproxStrMode=0: 通配符模式（使用 xrtStrLike，s2 为模式串）
// iApproxStrMode=1: 相似度模式（使用 xrtStrSim 和 fApproxStrTol 阈值）
XXAPI bool xrtStrApprox(str s1, size_t len1, str s2, size_t len2)
{
	if ( xCore.iApproxStrMode == XRT_STR_APPROX_LIKE ) {
		// 通配符模式
		return xrtStrLike(s1, len1, s2, len2, xCore.bApproxStrCase);
	} else {
		// 相似度模式
		double threshold = xCore.fApproxStrTol;
		if ( threshold <= 0.0 || threshold > 1.0 ) {
			threshold = 0.95;  // 无效阈值使用默认值
		}
		double sim = xrtStrSim(s1, len1, s2, len2);
		return sim >= threshold;
	}
}
