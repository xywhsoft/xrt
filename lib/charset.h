


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
	if ( sText == NULL ) { xCore.iRet = 0; return (u16str)xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { xCore.iRet = 0; return (u16str)xCore.sNull; }
	// 计算转换长度
	size_t iPos = 0;
	for ( int i = 0; i < iSize; i++ ) {
		char iExtraBytes = BytesExtraTableUTF8[sText[i]];
		if ( iExtraBytes < 3 ) {
			// 3 字节内的 utf8 会被编码为 2 字节的 utf16 字符
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
					// 原则上不会进入这个分支
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
	sRet[iPos] = 0;
	xCore.iRet = iPos;
	return sRet;
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
			sRet[iPos++] = sText[i];
		} else if ( iExtraBytes == 1 ) {
			// 双字节字符
			sRet[iPos++] = ((sText[i] & 0b00011111) << 6) | (sText[++i] & 0b00111111);
		} else if ( iExtraBytes == 2 ) {
			// 三字节字符
			sRet[iPos++] = ((sText[i] & 0b00001111) << 12) | ((sText[++i] & 0b00111111) << 6) | (sText[++i] & 0b00111111);
		} else if ( iExtraBytes == 3 ) {
			// 四字节字符
			sRet[iPos++] = ((sText[i] & 0b00000111) << 18) | ((sText[++i] & 0b00111111) << 12) | ((sText[++i] & 0b00111111) << 6) | (sText[++i] & 0b00111111);
		} else if ( iExtraBytes == 4 ) {
			// 五字节字符
			sRet[iPos++] = ((sText[i] & 0b00000011) << 24) | ((sText[++i] & 0b00111111) << 18) | ((sText[++i] & 0b00111111) << 12) | ((sText[++i] & 0b00111111) << 6) | (sText[++i] & 0b00111111);
		} else if ( iExtraBytes == 5 ) {
			// 六字节字符
			sRet[iPos++] = ((sText[i] & 0b00000001) << 30) | ((sText[++i] & 0b00111111) << 24) | ((sText[++i] & 0b00111111) << 18) | ((sText[++i] & 0b00111111) << 12) | ((sText[++i] & 0b00111111) << 6) | (sText[++i] & 0b00111111);
		}
	}
	sRet[iPos] = 0;
	xCore.iRet = iPos;
	return sRet;
}



// utf-16 转 utf-8
XXAPI u8str xrtUTF16to8(u16str sText, size_t iSize)
{
	if ( sText == NULL ) { xCore.iRet = 0; return xCore.sNull; }
	size_t iRetSize = 0;
	// 计算数据长度和转换长度
	if ( iSize == 0 ) {
		while ( sText[iSize] != 0 ) {
			uint16 iChar = sText[iSize];
			if ( (iChar & 0b1111110000000000) == 0b1101100000000000 ) {
				if ( (sText[iSize + 1] & 0b1111110000000000) == 0b1101110000000000 ) {
					iRetSize += 4;
				} else {
					// 错误的代理对，使用替换字符 EFBFBD 代替
					iRetSize += 3;
				}
				iSize += 2;
			} else if ( iChar <= 0x7F ) {
				iRetSize++;
				iSize++;
			} else if ( iChar <= 0x7FF ) {
				iRetSize += 2;
				iSize++;
			} else {
				iRetSize += 3;
				iSize++;
			}
		}
	} else {
		for ( int i = 0; i < iSize; i++ ) {
			uint16 iChar = sText[i];
			if ( (iChar & 0b1111110000000000) == 0b1101100000000000 ) {
				iRetSize += 4;
				i++;
			} else if ( iChar <= 0x7F ) {
				iRetSize++;
			} else if ( iChar <= 0x7FF ) {
				iRetSize += 2;
			} else {
				iRetSize += 3;
			}
		}
	}
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
	sRet[iPos] = 0;
	xCore.iRet = iPos;
	return sRet;
}



// utf-16 转 utf-32
XXAPI u32str xrtUTF16to32(u16str sText, size_t iSize)
{
	if ( sText == NULL ) { xCore.iRet = 0; return (u32str)xCore.sNull; }
	size_t iRetSize = 0;
	// 计算数据长度和转换长度
	if ( iSize == 0 ) {
		while ( sText[iSize] != 0 ) {
			if ( (sText[iSize] & 0b1111110000000000) == 0b1101100000000000 ) {
				iSize += 2;
			} else {
				iSize++;
			}
			iRetSize++;
		}
	} else {
		for ( int i = 0; i < iSize; i++ ) {
			uint16 iChar = sText[i];
			if ( (iChar & 0b1111110000000000) == 0b1101100000000000 ) {
				i++;
			}
			iRetSize++;
		}
	}
	// 申请所需内存
	u32str sRet = xrtMalloc((iRetSize + 1) * sizeof(unsigned int));
	if ( sRet == NULL ) {
		xrtSetError(xCore.ERROR_DESC.MALLOC, FALSE);
		xCore.iRet = 0;
		return (u32str)xCore.sNull;
	}
	// 开始转换编码
	size_t iPos = 0;
	for ( int i = 0; i < iSize; i++ ) {
		uint16 iChar = sText[i];
		if ( (iChar & 0b1111110000000000) == 0b1101100000000000 ) {
			sRet[iPos++] = (((iChar & 0x3FF) << 10) | (sText[i] & 0x3FF)) + 0x10000;
			i++;
		} else {
			sRet[iPos++] = iChar;
		}
	}
	sRet[iPos] = 0;
	xCore.iRet = iPos;
	return sRet;
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


