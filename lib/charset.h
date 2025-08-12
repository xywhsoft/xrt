


// utf8 字符长度码表
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



// utf-8 转 utf-16（ 需使用 xrtFree 释放 ）
XXAPI u16str xrtUTF8to16(u8str sText, size_t iSize)
{
	if ( sText == NULL ) { xCore.iRet = 0; return (u16str)xCore.sNull; }
	// 计算数据长度和转换长度
	size_t iPos = 0;
	if ( iSize == 0 ) {
		while ( sText[iSize] != 0 ) {
			char iExtraBytes = BytesExtraTableUTF8[sText[iSize]];
			if ( iExtraBytes < 3 ) {
				// 小于等于 3 字节的 utf8 字符会被编码为 2 字节的 utf16 字符
				iPos++;
			} else if ( iExtraBytes == 3 ) {
				// 4 字节的 utf8 会被编码为 4 字节的 utf16 字符
				iPos += 2;
			} else {
				// 超过 4 字节的 utf8 会被替换为 FFFD 替换码点（ 超出 utf16 支持的范围 ）
				iPos++;
			}
			iSize += iExtraBytes + 1;
		}
	} else {
		for ( int i = 0; i < iSize; i++ ) {
			char iExtraBytes = BytesExtraTableUTF8[sText[i]];
			if ( iExtraBytes < 3 ) {
				// 小于等于 3 字节的 utf8 字符会被编码为 2 字节的 utf16 字符
				iPos++;
			} else if ( iExtraBytes == 3 ) {
				// 4 字节的 utf8 会被编码为 4 字节的 utf16 字符
				iPos += 2;
			} else {
				// 超过 4 字节的 utf8 会被替换为 FFFD 替换码点（ 超出 utf16 支持的范围 ）
				iPos++;
			}
			i += iExtraBytes;
		}
	}
	if ( iSize == 0 ) { xCore.iRet = 0; return (u16str)xCore.sNull; }
	// 申请所需内存
	u16str sRet = xrtMalloc((iPos + 1) * sizeof(unsigned short));
	if ( sRet == NULL ) {
		xrtSetError(xCore.ERROR_DESC.MALLOC, FALSE);
		xCore.iRet = 0;
		return (u16str)xCore.sNull;
	}
	// 开始转换编码
	iPos = 0;
	for ( int i = 0; i < iSize; i++ ) {
		char iExtraBytes = BytesExtraTableUTF8[sText[i]];
		if ( iExtraBytes == 0 ) {
			// ASCII 兼容字符
			sRet[iPos++] = sText[i];
		} else if ( iExtraBytes == 1 ) {
			// 双字节字符
			sRet[iPos++] = ((sText[i] & 0b00011111) << 6) | (sText[++i] & 0b00111111);
		} else if ( iExtraBytes == 2 ) {
			// 三字节字符
			sRet[iPos++] = ((sText[i] & 0b00001111) << 12) | ((sText[++i] & 0b00111111) << 6) | (sText[++i] & 0b00111111);
		} else if ( iExtraBytes == 3 ) {
			// 四字节字符
			if ( sText[i] & 0b00000100 ) {
				// 超出 utf16 支持的范围，使用替换字符 FFFD 代替
				sRet[iPos++] = 0xFFFD;
				i += iExtraBytes;
			} else {
				uint32 c = ((sText[i] & 0b00000011) << 18) | ((sText[++i] & 0b00111111) << 12) | ((sText[++i] & 0b00111111) << 6) | (sText[++i] & 0b00111111);
				if ( c < 0x10000 ) {
					// 原则上不会进入这个分支，除非遇到错误的编码
					sRet[iPos++] = c;
				} else {
					c -= 0x10000;
					sRet[iPos++] = 0b1101100000000000 | ((c & 0b11111111110000000000) >> 10);
					sRet[iPos++] = 0b1101110000000000 | (c & 0b00000000001111111111);
				}
			}
		} else if ( iExtraBytes == 4 ) {
			// 五字节字符（ 超出 utf16 支持的范围，使用替换字符 FFFD 代替 ）
			sRet[iPos++] = 0xFFFD;
			i += iExtraBytes;
		} else if ( iExtraBytes == 5 ) {
			// 五字节字符（ 超出 utf16 支持的范围，使用替换字符 FFFD 代替 ）
			sRet[iPos++] = 0xFFFD;
			i += iExtraBytes;
		}
	}
	// 返回字符数和转换后数据
	sRet[iPos] = 0;
	xCore.iRet = iPos;
	return sRet;
}



// utf-8 转 utf-32（ 需使用 xrtFree 释放 ）
XXAPI u32str xrtUTF8to32(u8str sText, size_t iSize)
{
	if ( sText == NULL ) { xCore.iRet = 0; return (u32str)xCore.sNull; }
	// 计算数据长度和转换长度
	size_t iPos = 0;
	if ( iSize == 0 ) {
		while ( sText[iSize] != 0 ) {
			iPos++;
			iSize += BytesExtraTableUTF8[sText[iSize]] + 1;
		}
	} else {
		for ( int i = 0; i < iSize; i++ ) {
			iPos++;
			i += BytesExtraTableUTF8[sText[i]];
		}
	}
	if ( iSize == 0 ) { xCore.iRet = 0; return (u32str)xCore.sNull; }
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
			sRet[iPos++] = sText[i];
		} else if ( iExtraBytes == 1 ) {
			// 双字节字符
			sRet[iPos++] = ((sText[i] & 0b00011111) << 6) | (sText[++i] & 0x3F);
		} else if ( iExtraBytes == 2 ) {
			// 三字节字符
			sRet[iPos++] = ((sText[i] & 0b00001111) << 12) | ((sText[++i] & 0x3F) << 6) | (sText[++i] & 0x3F);
		} else if ( iExtraBytes == 3 ) {
			// 四字节字符
			sRet[iPos++] = ((sText[i] & 0b00000111) << 18) | ((sText[++i] & 0x3F) << 12) | ((sText[++i] & 0x3F) << 6) | (sText[++i] & 0x3F);
		} else if ( iExtraBytes == 4 ) {
			// 五字节字符
			sRet[iPos++] = ((sText[i] & 0b00000011) << 24) | ((sText[++i] & 0x3F) << 18) | ((sText[++i] & 0x3F) << 12) | ((sText[++i] & 0x3F) << 6) | (sText[++i] & 0x3F);
		} else if ( iExtraBytes == 5 ) {
			// 六字节字符
			sRet[iPos++] = ((sText[i] & 0b00000001) << 30) | ((sText[++i] & 0x3F) << 24) | ((sText[++i] & 0x3F) << 18) | ((sText[++i] & 0x3F) << 12) | ((sText[++i] & 0x3F) << 6) | (sText[++i] & 0x3F);
		}
	}
	// 返回字符数和转换后数据
	sRet[iPos] = 0;
	xCore.iRet = iPos;
	return sRet;
}



// utf-16 转 utf-8（ 需使用 xrtFree 释放 ）
XXAPI u8str xrtUTF16to8(u16str sText, size_t iSize)
{
	if ( sText == NULL ) { xCore.iRet = 0; return xCore.sNull; }
	size_t iPos = 0;
	// 计算数据长度和转换长度
	if ( iSize == 0 ) {
		while ( sText[iSize] != 0 ) {
			uint16 iChar = sText[iSize];
			if ( (iChar & 0b1111110000000000) == 0b1101100000000000 ) {
				if ( (sText[iSize + 1] & 0b1111110000000000) == 0b1101110000000000 ) {
					iPos += 4;
				} else {
					// 错误的代理对，使用替换字符 EFBFBD 代替
					iPos += 3;
				}
				iSize += 2;
			} else if ( iChar <= 0x7F ) {
				iPos++;
				iSize++;
			} else if ( iChar <= 0x7FF ) {
				iPos += 2;
				iSize++;
			} else {
				iPos += 3;
				iSize++;
			}
		}
	} else {
		for ( int i = 0; i < iSize; i++ ) {
			uint16 iChar = sText[i];
			if ( (iChar & 0b1111110000000000) == 0b1101100000000000 ) {
				if ( (sText[++i] & 0b1111110000000000) == 0b1101110000000000 ) {
					iPos += 4;
				} else {
					// 错误的代理对，使用替换字符 EFBFBD 代替
					iPos += 3;
				}
			} else if ( iChar <= 0x7F ) {
				iPos++;
			} else if ( iChar <= 0x7FF ) {
				iPos += 2;
			} else {
				iPos += 3;
			}
		}
	}
	if ( iSize == 0 ) { xCore.iRet = 0; return xCore.sNull; }
	// 申请所需内存
	u8str sRet = xrtMalloc(iPos + 1);
	if ( sRet == NULL ) {
		xrtSetError(xCore.ERROR_DESC.MALLOC, FALSE);
		xCore.iRet = 0;
		return xCore.sNull;
	}
	// 开始转换编码
	iPos = 0;
	for ( int i = 0; i < iSize; i++ ) {
		uint16 iChar = sText[i];
		if ( (iChar & 0b1111110000000000) == 0b1101100000000000 ) {
			uint16 iNext = sText[++i];
			if ( (iNext & 0b1111110000000000) == 0b1101110000000000 ) {
				uint32 cp = (((iChar & 0x3FF) << 10) | (iNext & 0x3FF)) + 0x10000;
				sRet[iPos++] = 0xF0 | ((cp >> 18) & 0x7);
				sRet[iPos++] = 0x80 | ((cp >> 12) & 0x3F);
				sRet[iPos++] = 0x80 | ((cp >> 6) & 0x3F);
				sRet[iPos++] = 0x80 | (cp & 0x3F);
			} else {
				// 错误的代理对，使用替换字符 EFBFBD 代替
				sRet[iPos++] = 0xEF;
				sRet[iPos++] = 0xBF;
				sRet[iPos++] = 0xBD;
			}
		} else if ( iChar <= 0x7F ) {
			sRet[iPos++] = iChar;
		} else if ( iChar <= 0x7FF ) {
			sRet[iPos++] = 0xC0 | ((iChar & 0x7C0) >> 6);
			sRet[iPos++] = 0x80 | (iChar & 0x3F);
		} else {
			sRet[iPos++] = 0xE0 | ((iChar & 0xF000) >> 12);
			sRet[iPos++] = 0x80 | ((iChar & 0xFC0) >> 6);
			sRet[iPos++] = 0x80 | (iChar & 0x3F);
		}
	}
	// 返回字符数和转换后数据
	sRet[iPos] = 0;
	xCore.iRet = iPos;
	return sRet;
}



// utf-16 转 utf-32（ 需使用 xrtFree 释放 ）
XXAPI u32str xrtUTF16to32(u16str sText, size_t iSize)
{
	if ( sText == NULL ) { xCore.iRet = 0; return (u32str)xCore.sNull; }
	size_t iPos = 0;
	// 计算数据长度和转换长度
	if ( iSize == 0 ) {
		while ( sText[iSize] != 0 ) {
			if ( (sText[iSize] & 0b1111110000000000) == 0b1101100000000000 ) {
				iSize += 2;
			} else {
				iSize++;
			}
			iPos++;
		}
	} else {
		for ( int i = 0; i < iSize; i++ ) {
			uint16 iChar = sText[i];
			if ( (iChar & 0b1111110000000000) == 0b1101100000000000 ) {
				i++;
			}
			iPos++;
		}
	}
	if ( iSize == 0 ) { xCore.iRet = 0; return (u32str)xCore.sNull; }
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
		uint16 iChar = sText[i];
		if ( (iChar & 0b1111110000000000) == 0b1101100000000000 ) {
			uint16 iNext = sText[++i];
			if ( (iNext & 0b1111110000000000) == 0b1101110000000000 ) {
				sRet[iPos++] = (((iChar & 0x3FF) << 10) | (iNext & 0x3FF)) + 0x10000;
			} else {
				// 错误的代理对，使用替换字符 FFFD 代替
				sRet[iPos++] = 0xFFFD;
			}
		} else {
			sRet[iPos++] = iChar;
		}
	}
	// 返回字符数和转换后数据
	sRet[iPos] = 0;
	xCore.iRet = iPos;
	return sRet;
}



// utf-32 转 utf-8（ 需使用 xrtFree 释放 ）
XXAPI u8str xrtUTF32to8(u32str sText, size_t iSize)
{
	if ( sText == NULL ) { xCore.iRet = 0; return xCore.sNull; }
	size_t iPos = 0;
	// 计算数据长度和转换长度
	if ( iSize == 0 ) {
		while ( sText[iSize] != 0 ) {
			uint32 iChar = sText[iSize++];
			if ( iChar <= 0x7F ) {
				iPos++;
			} else if ( iChar <= 0x7FF ) {
				iPos += 2;
			} else if ( iChar <= 0xFFFF ) {
				iPos += 3;
			} else if ( iChar <= 0x1FFFFF ) {
				iPos += 4;
			} else if ( iChar <= 0x3FFFFFF ) {
				iPos += 5;
			} else if ( iChar <= 0x7FFFFFFF ) {
				iPos += 6;
			}
		}
	} else {
		for ( int i = 0; i < iSize; i++ ) {
			uint32 iChar = sText[i];
			if ( iChar <= 0x7F ) {
				iPos++;
			} else if ( iChar <= 0x7FF ) {
				iPos += 2;
			} else if ( iChar <= 0xFFFF ) {
				iPos += 3;
			} else if ( iChar <= 0x1FFFFF ) {
				iPos += 4;
			} else if ( iChar <= 0x3FFFFFF ) {
				iPos += 5;
			} else if ( iChar <= 0x7FFFFFFF ) {
				iPos += 6;
			}
		}
	}
	if ( iSize == 0 ) { xCore.iRet = 0; return xCore.sNull; }
	// 申请所需内存
	u8str sRet = xrtMalloc(iPos + 1);
	if ( sRet == NULL ) {
		xrtSetError(xCore.ERROR_DESC.MALLOC, FALSE);
		xCore.iRet = 0;
		return xCore.sNull;
	}
	// 开始转换编码
	iPos = 0;
	for ( int i = 0; i < iSize; i++ ) {
		uint32 iChar = sText[i];
		if ( iChar <= 0x7F ) {
			// ASCII 兼容字符
			sRet[iPos++] = iChar;
		} else if ( iChar <= 0x7FF ) {
			// 双字节字符
			sRet[iPos++] = 0xC0 | ((iChar >> 6) & 0x1F);
			sRet[iPos++] = 0x80 | (iChar & 0x3F);
		} else if ( iChar <= 0xFFFF ) {
			// 三字节字符
			sRet[iPos++] = 0xE0 | ((iChar >> 12) & 0xF);
			sRet[iPos++] = 0x80 | ((iChar >> 6) & 0x3F);
			sRet[iPos++] = 0x80 | (iChar & 0x3F);
		} else if ( iChar <= 0x1FFFFF ) {
			// 四字节字符
			sRet[iPos++] = 0xF0 | ((iChar >> 18) & 0x7);
			sRet[iPos++] = 0x80 | ((iChar >> 12) & 0x3F);
			sRet[iPos++] = 0x80 | ((iChar >> 6) & 0x3F);
			sRet[iPos++] = 0x80 | (iChar & 0x3F);
		} else if ( iChar <= 0x3FFFFFF ) {
			// 五字节字符
			sRet[iPos++] = 0xF8 | ((iChar >> 24) & 0x3);
			sRet[iPos++] = 0x80 | ((iChar >> 18) & 0x3F);
			sRet[iPos++] = 0x80 | ((iChar >> 12) & 0x3F);
			sRet[iPos++] = 0x80 | ((iChar >> 6) & 0x3F);
			sRet[iPos++] = 0x80 | (iChar & 0x3F);
		} else if ( iChar <= 0x7FFFFFFF ) {
			// 六字节字符
			sRet[iPos++] = 0xFC | ((iChar >> 30) & 0x1);
			sRet[iPos++] = 0x80 | ((iChar >> 24) & 0x3F);
			sRet[iPos++] = 0x80 | ((iChar >> 18) & 0x3F);
			sRet[iPos++] = 0x80 | ((iChar >> 12) & 0x3F);
			sRet[iPos++] = 0x80 | ((iChar >> 6) & 0x3F);
			sRet[iPos++] = 0x80 | (iChar & 0x3F);
		}
	}
	// 返回字符数和转换后数据
	sRet[iPos] = 0;
	xCore.iRet = iPos;
	return sRet;
}



// utf-32 转 utf-16（ 需使用 xrtFree 释放 ）
XXAPI u16str xrtUTF32to16(u32str sText, size_t iSize)
{
	if ( sText == NULL ) { xCore.iRet = 0; return (u16str)xCore.sNull; }
	size_t iPos = 0;
	// 计算数据长度和转换长度
	if ( iSize == 0 ) {
		while ( sText[iSize] != 0 ) {
			if ( sText[iSize] <= 0xFFFF ) {
				iPos++;
			} else if ( sText[iSize] <= 0x10FFFF ) {
				iPos += 2;
			} else {
				iPos++;
			}
			iSize++;
		}
	} else {
		for ( int i = 0; i < iSize; i++ ) {
			uint32 iChar = sText[i];
			if ( iChar <= 0xFFFF ) {
				iPos++;
			} else if ( iChar <= 0x10FFFF ) {
				iPos += 2;
			} else {
				iPos++;
			}
		}
	}
	if ( iSize == 0 ) { xCore.iRet = 0; return (u16str)xCore.sNull; }
	// 申请所需内存
	u16str sRet = xrtMalloc((iPos + 1) * sizeof(unsigned short));
	if ( sRet == NULL ) {
		xrtSetError(xCore.ERROR_DESC.MALLOC, FALSE);
		xCore.iRet = 0;
		return (u16str)xCore.sNull;
	}
	// 开始转换编码
	iPos = 0;
	for ( int i = 0; i < iSize; i++ ) {
		uint32 iChar = sText[i];
		if ( iChar <= 0xFFFF ) {
			sRet[iPos++] = iChar;
		} else if ( iChar <= 0x10FFFF ) {
			iChar -= 0x10000;
			sRet[iPos++] = 0b1101100000000000 | ((iChar & 0b11111111110000000000) >> 10);
			sRet[iPos++] = 0b1101110000000000 | (iChar & 0b00000000001111111111);
		} else {
			// 超出 utf16 支持的范围，使用替换字符 FFFD 代替
			sRet[iPos++] = 0xFFFD;
		}
	}
	// 返回字符数和转换后数据
	sRet[iPos] = 0;
	xCore.iRet = iPos;
	return sRet;
}



// utf-16 大端序和小端序转换



// utf-32 大端序和小端序转换



// 任意编码转换
XXAPI ptr xrtConvCharset(ptr sText, size_t iSize, int iInCP, int iOutCP)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		return NULL;
	#else
		// 其他平台方案 ( 仅支持 utf-8、utf-16、utf-32 )
		static const str ErrorCP = "Unsupported charset !";
		if ( iInCP == XRT_CP_OEM ) { iInCP = XRT_CP_UTF8; }
		if ( iOutCP == XRT_CP_OEM ) { iOutCP = XRT_CP_UTF8; }
		if ( (iInCP != XRT_CP_UTF8) && (iInCP != XRT_CP_UTF16) && (iInCP != XRT_CP_UTF16_BE) && (iInCP != XRT_CP_UTF32) && (iInCP != XRT_CP_UTF32_BE) ) {
			xrtSetError(ErrorCP, FALSE);
			return sNull;
		}
		if ( (iOutCP != XRT_CP_UTF8) && (iOutCP != XRT_CP_UTF16) && (iOutCP != XRT_CP_UTF16_BE) && (iOutCP != XRT_CP_UTF32) && (iOutCP != XRT_CP_UTF32_BE) ) {
			xrtSetError(ErrorCP, FALSE);
			return sNull;
		}
		// 如果编码相同，返回副本
		if ( iInCP == iOutCP ) {
			if ( iInCP == XRT_CP_UTF8 ) {
				return xrtCopyStr(sText, iSize);
			} else if ( (iInCP == XRT_CP_UTF16) || (iInCP == XRT_CP_UTF16_BE) ) {
				return xrtCopyStrU16(sText, iSize);
			} else if ( (iInCP == XRT_CP_UTF32) || (iInCP == XRT_CP_UTF32_BE) ) {
				return xrtCopyStrU32(sText, iSize);
			}
		}
		// 需要转换编码 - 排列组合
		if ( iInCP == XRT_CP_UTF8 ) {
			if ( iOutCP == XRT_CP_UTF16 ) {
				return xrtUTF8to16(sText, iSize);
			} else if ( iOutCP == XRT_CP_UTF16_BE ) {
			} else if ( iOutCP == XRT_CP_UTF32 ) {
				return xrtUTF8to32(sText, iSize);
			} else if ( iOutCP == XRT_CP_UTF32_BE ) {
			}
		} else if ( iInCP == XRT_CP_UTF16 ) {
			if ( iOutCP == XRT_CP_UTF8 ) {
				return xrtUTF16to8(sText, iSize);
			} else if ( iOutCP == XRT_CP_UTF16_BE ) {
			} else if ( iOutCP == XRT_CP_UTF32 ) {
				return xrtUTF16to32(sText, iSize);
			} else if ( iOutCP == XRT_CP_UTF32_BE ) {
			}
		} else if ( iInCP == XRT_CP_UTF16_BE ) {
			if ( iOutCP == XRT_CP_UTF8 ) {
			} else if ( iOutCP == XRT_CP_UTF16 ) {
			} else if ( iOutCP == XRT_CP_UTF32 ) {
			} else if ( iOutCP == XRT_CP_UTF32_BE ) {
			}
		} else if ( iInCP == XRT_CP_UTF32 ) {
			if ( iOutCP == XRT_CP_UTF8 ) {
				return xrtUTF32to8(sText, iSize);
			} else if ( iOutCP == XRT_CP_UTF16 ) {
				return xrtUTF32to16(sText, iSize);
			} else if ( iOutCP == XRT_CP_UTF16_BE ) {
			} else if ( iOutCP == XRT_CP_UTF32_BE ) {
			}
		} else if ( iInCP == XRT_CP_UTF32_BE ) {
			if ( iOutCP == XRT_CP_UTF8 ) {
			} else if ( iOutCP == XRT_CP_UTF16 ) {
			} else if ( iOutCP == XRT_CP_UTF16_BE ) {
			} else if ( iOutCP == XRT_CP_UTF32 ) {
			}
		}
		// 理论上不会执行到这里
		xrtSetError(ErrorCP, FALSE);
		return sNull;
	#endif
}



// 猜测编码 ( 先判断 BOM，再判断是否为合法的 utf8 编码，再根据 \0 的长度推测是否为 utf32 或 utf16、OEM，猜测不出来时返回 binary )
XXAPI int xrtDetCP(ptr sText, size_t iSize)
{
	return XRT_CP_BINARY;
}


