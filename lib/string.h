


// 创建字符串副本（需使用 xrtFree 释放）
XXAPI ustr xrtCopyString(ustr sText, size_t iSize)
{
	if ( sText == NULL ) { return (ustr)xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { return (ustr)xCore.sNull; }
	ustr sRet = xrtMalloc(iSize + 1);
	if ( sRet == NULL ) {
		xrtSetError(XRT_ERROR_MALLOC, FALSE);
		return (ustr)xCore.sNull;
	}
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
	if ( sRet == NULL ) {
		xrtSetError(XRT_ERROR_MALLOC, FALSE);
		return (wstr)xCore.sNull;
	}
	memcpy(sRet, sText, iSize * sizeof(wchar_t));
	sRet[iSize] = 0;
	return sRet;
}



// 字符串转为小写（bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存）
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
		sRet[i] = towlower(sRet[i]);
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
		sRet[i] = towupper(sRet[i]);
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
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { return NULL; }
	if ( iSubSize == 0 ) { iSubSize = strlen(sSubText); }
	if ( iSubSize == 0 ) { return NULL; }
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
			i++;
		} else if ( sText[i] & 0b11100000 == 0b11100000 ) {
			// 三字节字符
			size_t iLen = iSubSize - 2;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) ) {
					return &sText[i];
				}
			}
			i += 2;
		} else if ( sText[i] & 0b11110000 == 0b11110000 ) {
			// 四字节字符
			size_t iLen = iSubSize - 3;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) ) {
					return &sText[i];
				}
			}
			i += 3;
		} else if ( sText[i] & 0b11111000 == 0b11111000 ) {
			// 五字节字符
			size_t iLen = iSubSize - 4;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) && (sSubText[j+4] == sText[i+4]) ) {
					return &sText[i];
				}
			}
			i += 4;
		} else if ( sText[i] & 0b11111100 == 0b11111100 ) {
			// 六字节字符
			size_t iLen = iSubSize - 5;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) && (sSubText[j+4] == sText[i+4]) && (sSubText[j+5] == sText[i+5]) ) {
					return &sText[i];
				}
			}
			i += 5;
		} else {
			// 跳过异常字符（FE、FF）
		}
	}
	return NULL;
}
XXAPI wstr xrtCheckStrW(wstr sText, size_t iSize, wstr sSubText, size_t iSubSize)
{
	if ( iSize == 0 ) { iSize = wcslen(sText); }
	if ( iSize == 0 ) { return NULL; }
	if ( iSubSize == 0 ) { iSubSize = wcslen(sSubText); }
	if ( iSubSize == 0 ) { return NULL; }
	wchar_t sFindStr[2] = { 0, 0 };
	for ( int i = 0; i < iSize; i++ ) {
		for ( int j = 0; j < iSubSize; j++ ) {
			if ( sText[i] == sSubText[j] ) {
				return &sText[i];
			}
		}
	}
	return NULL;
}



// 裁剪字符串（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
XXAPI ustr xrtLTrim(ustr sText, size_t iSize, ustr sSubText, size_t iSubSize, int bSrcRevise)
{
	if ( sText == NULL ) { xCore.iRet = 0; return (ustr)xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { xCore.iRet = 0; return (ustr)xCore.sNull; }
	if ( sSubText == NULL ) { sSubText = " \t\r\n"; iSubSize = 4; }
	if ( iSubSize == 0 ) { iSubSize = strlen(sSubText); }
	if ( iSubSize == 0 ) { sSubText = " \t\r\n"; iSubSize = 4; }
	int iCount = 0;
	for ( int i = 0; i < iSize; i++ ) {
		int bBreak = TRUE;
		if ( (sText[i] & 0b10000000) == 0 ) {
			// ASCII 兼容字符
			for ( int j = 0; j < iSubSize; j++ ) {
				if ( sSubText[j] == sText[i] ) {
					iCount++;
					bBreak = FALSE;
					break;
				}
			}
		} else if ( sText[i] & 0b11000000 == 0b11000000 ) {
			// 双字节字符
			size_t iLen = iSubSize - 1;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) ) {
					iCount += 2;
					bBreak = FALSE;
					break;
				}
			}
			i++;
		} else if ( sText[i] & 0b11100000 == 0b11100000 ) {
			// 三字节字符
			size_t iLen = iSubSize - 2;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) ) {
					iCount += 3;
					bBreak = FALSE;
					break;
				}
			}
			i += 2;
		} else if ( sText[i] & 0b11110000 == 0b11110000 ) {
			// 四字节字符
			size_t iLen = iSubSize - 3;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) ) {
					iCount += 4;
					bBreak = FALSE;
					break;
				}
			}
			i += 3;
		} else if ( sText[i] & 0b11111000 == 0b11111000 ) {
			// 五字节字符
			size_t iLen = iSubSize - 4;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) && (sSubText[j+4] == sText[i+4]) ) {
					iCount += 5;
					bBreak = FALSE;
					break;
				}
			}
			i += 4;
		} else if ( sText[i] & 0b11111100 == 0b11111100 ) {
			// 六字节字符
			size_t iLen = iSubSize - 5;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) && (sSubText[j+4] == sText[i+4]) && (sSubText[j+5] == sText[i+5]) ) {
					iCount += 6;
					bBreak = FALSE;
					break;
				}
			}
			i += 5;
		} else {
			// 跳过异常字符（FE、FF）
		}
		if ( bBreak ) {
			break;
		}
	}
	xCore.iRet = iCount;
	if ( bSrcRevise ) {
		if ( iCount > 0 ) {
			memmove(sText, &sText[iCount], iSize - iCount);
			sText[iSize - iCount] = 0;
		}
		return sText;
	} else {
		return xrtCopyString(&sText[iCount], iSize - iCount);
	}
}
XXAPI ustr xrtRTrim(ustr sText, size_t iSize, ustr sSubText, size_t iSubSize, int bSrcRevise)
{
	if ( sText == NULL ) { xCore.iRet = 0; return (ustr)xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { xCore.iRet = 0; return (ustr)xCore.sNull; }
	if ( sSubText == NULL ) { sSubText = " \t\r\n"; iSubSize = 4; }
	if ( iSubSize == 0 ) { iSubSize = strlen(sSubText); }
	if ( iSubSize == 0 ) { sSubText = " \t\r\n"; iSubSize = 4; }
	int iCount = 0;
	for ( int i = iSize - 1; i >= 0; i-- ) {
		int bBreak = TRUE;
		if ( (sText[i] & 0b10000000) == 0 ) {
			// ASCII 兼容字符
			for ( int j = 0; j < iSubSize; j++ ) {
				if ( sSubText[j] == sText[i] ) {
					iCount++;
					bBreak = FALSE;
					break;
				}
			}
		} else if ( sText[i] & 0b11000000 == 0b11000000 ) {
			// 双字节字符
			size_t iLen = iSubSize - 1;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) ) {
					iCount += 2;
					bBreak = FALSE;
					break;
				}
			}
			i++;
		} else if ( sText[i] & 0b11100000 == 0b11100000 ) {
			// 三字节字符
			size_t iLen = iSubSize - 2;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) ) {
					iCount += 3;
					bBreak = FALSE;
					break;
				}
			}
			i += 2;
		} else if ( sText[i] & 0b11110000 == 0b11110000 ) {
			// 四字节字符
			size_t iLen = iSubSize - 3;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) ) {
					iCount += 4;
					bBreak = FALSE;
					break;
				}
			}
			i += 3;
		} else if ( sText[i] & 0b11111000 == 0b11111000 ) {
			// 五字节字符
			size_t iLen = iSubSize - 4;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) && (sSubText[j+4] == sText[i+4]) ) {
					iCount += 5;
					bBreak = FALSE;
					break;
				}
			}
			i += 4;
		} else if ( sText[i] & 0b11111100 == 0b11111100 ) {
			// 六字节字符
			size_t iLen = iSubSize - 5;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) && (sSubText[j+4] == sText[i+4]) && (sSubText[j+5] == sText[i+5]) ) {
					iCount += 6;
					bBreak = FALSE;
					break;
				}
			}
			i += 5;
		} else {
			// 跳过异常字符（FE、FF）
		}
		if ( bBreak ) {
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
		return xrtCopyString(sText, iSize - iCount);
	}
}
XXAPI ustr xrtTrim(ustr sText, size_t iSize, ustr sSubText, size_t iSubSize, int bSrcRevise)
{
	if ( sText == NULL ) { xCore.iRet = 0; return (ustr)xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { xCore.iRet = 0; return (ustr)xCore.sNull; }
	if ( sSubText == NULL ) { sSubText = " \t\r\n"; iSubSize = 4; }
	if ( iSubSize == 0 ) { iSubSize = strlen(sSubText); }
	if ( iSubSize == 0 ) { sSubText = " \t\r\n"; iSubSize = 4; }
	int iCountL = 0;
	int iCountR = 0;
	// 裁剪左侧
	for ( int i = 0; i < iSize; i++ ) {
		int bBreak = TRUE;
		if ( (sText[i] & 0b10000000) == 0 ) {
			// ASCII 兼容字符
			for ( int j = 0; j < iSubSize; j++ ) {
				if ( sSubText[j] == sText[i] ) {
					iCountL++;
					bBreak = FALSE;
					break;
				}
			}
		} else if ( sText[i] & 0b11000000 == 0b11000000 ) {
			// 双字节字符
			size_t iLen = iSubSize - 1;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) ) {
					iCountL += 2;
					bBreak = FALSE;
					break;
				}
			}
			i++;
		} else if ( sText[i] & 0b11100000 == 0b11100000 ) {
			// 三字节字符
			size_t iLen = iSubSize - 2;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) ) {
					iCountL += 3;
					bBreak = FALSE;
					break;
				}
			}
			i += 2;
		} else if ( sText[i] & 0b11110000 == 0b11110000 ) {
			// 四字节字符
			size_t iLen = iSubSize - 3;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) ) {
					iCountL += 4;
					bBreak = FALSE;
					break;
				}
			}
			i += 3;
		} else if ( sText[i] & 0b11111000 == 0b11111000 ) {
			// 五字节字符
			size_t iLen = iSubSize - 4;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) && (sSubText[j+4] == sText[i+4]) ) {
					iCountL += 5;
					bBreak = FALSE;
					break;
				}
			}
			i += 4;
		} else if ( sText[i] & 0b11111100 == 0b11111100 ) {
			// 六字节字符
			size_t iLen = iSubSize - 5;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) && (sSubText[j+4] == sText[i+4]) && (sSubText[j+5] == sText[i+5]) ) {
					iCountL += 6;
					bBreak = FALSE;
					break;
				}
			}
			i += 5;
		} else {
			// 跳过异常字符（FE、FF）
		}
		if ( bBreak ) {
			break;
		}
	}
	// 全部裁剪需要特殊处理
	if ( iCountL >= iSize ) {
		xCore.iRet = iSize;
		return (ustr)xCore.sNull;
	}
	// 裁剪右侧
	for ( int i = iSize - 1; i >= 0; i-- ) {
		int bBreak = TRUE;
		if ( (sText[i] & 0b10000000) == 0 ) {
			// ASCII 兼容字符
			for ( int j = 0; j < iSubSize; j++ ) {
				if ( sSubText[j] == sText[i] ) {
					iCountR++;
					bBreak = FALSE;
					break;
				}
			}
		} else if ( sText[i] & 0b11000000 == 0b11000000 ) {
			// 双字节字符
			size_t iLen = iSubSize - 1;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) ) {
					iCountR += 2;
					bBreak = FALSE;
					break;
				}
			}
			i++;
		} else if ( sText[i] & 0b11100000 == 0b11100000 ) {
			// 三字节字符
			size_t iLen = iSubSize - 2;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) ) {
					iCountR += 3;
					bBreak = FALSE;
					break;
				}
			}
			i += 2;
		} else if ( sText[i] & 0b11110000 == 0b11110000 ) {
			// 四字节字符
			size_t iLen = iSubSize - 3;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) ) {
					iCountR += 4;
					bBreak = FALSE;
					break;
				}
			}
			i += 3;
		} else if ( sText[i] & 0b11111000 == 0b11111000 ) {
			// 五字节字符
			size_t iLen = iSubSize - 4;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) && (sSubText[j+4] == sText[i+4]) ) {
					iCountR += 5;
					bBreak = FALSE;
					break;
				}
			}
			i += 4;
		} else if ( sText[i] & 0b11111100 == 0b11111100 ) {
			// 六字节字符
			size_t iLen = iSubSize - 5;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) && (sSubText[j+4] == sText[i+4]) && (sSubText[j+5] == sText[i+5]) ) {
					iCountR += 6;
					bBreak = FALSE;
					break;
				}
			}
			i += 5;
		} else {
			// 跳过异常字符（FE、FF）
		}
		if ( bBreak ) {
			break;
		}
	}
	int iCount = iCountL + iCountR;
	xCore.iRet = iCount;
	if ( bSrcRevise ) {
		if ( iCount > 0 ) {
			memmove(sText, &sText[iCountL], iSize - iCount);
			sText[iSize - iCount] = 0;
		}
		return sText;
	} else {
		return xrtCopyString(&sText[iCountL], iSize - iCount);
	}
}
XXAPI wstr xrtLTrimW(wstr sText, size_t iSize, wstr sSubText, size_t iSubSize, int bSrcRevise)
{
	if ( sText == NULL ) { xCore.iRet = 0; return (wstr)xCore.sNull; }
	if ( iSize == 0 ) { iSize = wcslen(sText); }
	if ( iSize == 0 ) { xCore.iRet = 0; return (wstr)xCore.sNull; }
	if ( sSubText == NULL ) { sSubText = L" \t\r\n"; iSubSize = 4; }
	if ( iSubSize == 0 ) { iSubSize = wcslen(sSubText); }
	if ( iSubSize == 0 ) { sSubText = L" \t\r\n"; iSubSize = 4; }
	int iCount = 0;
	for ( int i = 0; i < iSize; i++ ) {
		int bBreak = TRUE;
		for ( int j = 0; j < iSubSize; j++ ) {
			if ( sText[i] == sSubText[j] ) {
				iCount++;
				bBreak = FALSE;
				break;
			}
		}
		if ( bBreak ) {
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
XXAPI wstr xrtRTrimW(wstr sText, size_t iSize, wstr sSubText, size_t iSubSize, int bSrcRevise)
{
	if ( sText == NULL ) { xCore.iRet = 0; return (wstr)xCore.sNull; }
	if ( iSize == 0 ) { iSize = wcslen(sText); }
	if ( iSize == 0 ) { xCore.iRet = 0; return (wstr)xCore.sNull; }
	if ( sSubText == NULL ) { sSubText = L" \t\r\n"; iSubSize = 4; }
	if ( iSubSize == 0 ) { iSubSize = wcslen(sSubText); }
	if ( iSubSize == 0 ) { sSubText = L" \t\r\n"; iSubSize = 4; }
	int iCount = 0;
	for ( int i = iSize - 1; i >= 0; i-- ) {
		int bBreak = TRUE;
		for ( int j = 0; j < iSubSize; j++ ) {
			if ( sText[i] == sSubText[j] ) {
				iCount++;
				bBreak = FALSE;
				break;
			}
		}
		if ( bBreak ) {
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
XXAPI wstr xrtTrimW(wstr sText, size_t iSize, wstr sSubText, size_t iSubSize, int bSrcRevise)
{
	if ( sText == NULL ) { xCore.iRet = 0; return (wstr)xCore.sNull; }
	if ( iSize == 0 ) { iSize = wcslen(sText); }
	if ( iSize == 0 ) { xCore.iRet = 0; return (wstr)xCore.sNull; }
	if ( sSubText == NULL ) { sSubText = L" \t\r\n"; iSubSize = 4; }
	if ( iSubSize == 0 ) { iSubSize = wcslen(sSubText); }
	if ( iSubSize == 0 ) { sSubText = L" \t\r\n"; iSubSize = 4; }
	int iCountL = 0;
	int iCountR = 0;
	// 裁剪左侧
	for ( int i = 0; i < iSize; i++ ) {
		int bBreak = TRUE;
		for ( int j = 0; j < iSubSize; j++ ) {
			if ( sText[i] == sSubText[j] ) {
				iCountL++;
				bBreak = FALSE;
				break;
			}
		}
		if ( bBreak ) {
			break;
		}
	}
	// 全部裁剪需要特殊处理
	if ( iCountL >= iSize ) {
		xCore.iRet = iSize;
		return (wstr)xCore.sNull;
	}
	// 裁剪右侧
	for ( int i = iSize - 1; i >= 0; i-- ) {
		int bBreak = TRUE;
		for ( int j = 0; j < iSubSize; j++ ) {
			if ( sText[i] == sSubText[j] ) {
				iCountR++;
				bBreak = FALSE;
				break;
			}
		}
		if ( bBreak ) {
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



// 过滤字符串（ bSrcRevise 为 FALSE 时，需使用 xrtFree 释放内存 ）
XXAPI ustr xrtFilterStr(ustr sText, size_t iSize, ustr sSubText, size_t iSubSize, int bSrcRevise)
{
	if ( sText == NULL ) { xCore.iRet = 0; return (ustr)xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { xCore.iRet = 0; return (ustr)xCore.sNull; }
	if ( sSubText == NULL ) { if ( bSrcRevise ) { return sText; } else { return xrtCopyString(sText, iSize); } }
	if ( iSubSize == 0 ) { iSubSize = strlen(sSubText); }
	if ( iSubSize == 0 ) { if ( bSrcRevise ) { return sText; } else { return xrtCopyString(sText, iSize); } }
	// 不改动源数据时，直接创建副本
	if ( bSrcRevise == FALSE ) {
		sText = xrtCopyString(sText, iSize);
	}
	int iCount = 0;
	for ( int i = 0; i < iSize; i++ ) {
		if ( (sText[i] & 0b10000000) == 0 ) {
			// ASCII 兼容字符
			int bCopy = TRUE;
			for ( int j = 0; j < iSubSize; j++ ) {
				if ( sSubText[j] == sText[i] ) {
					iCount++;
					bCopy = FALSE;
					break;
				}
			}
			if ( bCopy ) {
				sText[i - iCount] = sText[i];
			}
		} else if ( sText[i] & 0b11000000 == 0b11000000 ) {
			// 双字节字符
			int bCopy = TRUE;
			size_t iLen = iSubSize - 1;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) ) {
					iCount += 2;
					bCopy = FALSE;
					break;
				}
			}
			if ( bCopy ) {
				sText[i - iCount] = sText[i];
				sText[i - iCount + 1] = sText[i + 1];
			}
			i++;
		} else if ( sText[i] & 0b11100000 == 0b11100000 ) {
			// 三字节字符
			int bCopy = TRUE;
			size_t iLen = iSubSize - 2;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) ) {
					iCount += 3;
					bCopy = FALSE;
					break;
				}
			}
			if ( bCopy ) {
				sText[i - iCount] = sText[i];
				sText[i - iCount + 1] = sText[i + 1];
				sText[i - iCount + 2] = sText[i + 2];
			}
			i += 2;
		} else if ( sText[i] & 0b11110000 == 0b11110000 ) {
			// 四字节字符
			int bCopy = TRUE;
			size_t iLen = iSubSize - 3;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) ) {
					iCount += 4;
					bCopy = FALSE;
					break;
				}
			}
			if ( bCopy ) {
				sText[i - iCount] = sText[i];
				sText[i - iCount + 1] = sText[i + 1];
				sText[i - iCount + 2] = sText[i + 2];
				sText[i - iCount + 3] = sText[i + 3];
			}
			i += 3;
		} else if ( sText[i] & 0b11111000 == 0b11111000 ) {
			// 五字节字符
			int bCopy = TRUE;
			size_t iLen = iSubSize - 4;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) && (sSubText[j+4] == sText[i+4]) ) {
					iCount += 5;
					bCopy = FALSE;
					break;
				}
			}
			if ( bCopy ) {
				sText[i - iCount] = sText[i];
				sText[i - iCount + 1] = sText[i + 1];
				sText[i - iCount + 2] = sText[i + 2];
				sText[i - iCount + 3] = sText[i + 3];
				sText[i - iCount + 4] = sText[i + 4];
			}
			i += 4;
		} else if ( sText[i] & 0b11111100 == 0b11111100 ) {
			// 六字节字符
			int bCopy = TRUE;
			size_t iLen = iSubSize - 5;
			for ( int j = 0; j < iLen; j++ ) {
				if ( (sSubText[j] == sText[i]) && (sSubText[j+1] == sText[i+1]) && (sSubText[j+2] == sText[i+2]) && (sSubText[j+3] == sText[i+3]) && (sSubText[j+4] == sText[i+4]) && (sSubText[j+5] == sText[i+5]) ) {
					iCount += 6;
					bCopy = FALSE;
					break;
				}
			}
			if ( bCopy ) {
				sText[i - iCount] = sText[i];
				sText[i - iCount + 1] = sText[i + 1];
				sText[i - iCount + 2] = sText[i + 2];
				sText[i - iCount + 3] = sText[i + 3];
				sText[i - iCount + 4] = sText[i + 4];
				sText[i - iCount + 5] = sText[i + 5];
			}
			i += 5;
		} else {
			// 跳过异常字符（FE、FF）
		}
	}
	sText[iSize - iCount] = 0;
	xCore.iRet = iCount;
	return sText;
}
XXAPI wstr xrtFilterStrW(wstr sText, size_t iSize, wstr sSubText, size_t iSubSize, int bSrcRevise)
{
	if ( sText == NULL ) { xCore.iRet = 0; return (wstr)xCore.sNull; }
	if ( iSize == 0 ) { iSize = wcslen(sText); }
	if ( iSize == 0 ) { xCore.iRet = 0; return (wstr)xCore.sNull; }
	if ( sSubText == NULL ) { if ( bSrcRevise ) { return sText; } else { return xrtCopyStringW(sText, iSize); } }
	if ( iSubSize == 0 ) { iSubSize = wcslen(sSubText); }
	if ( iSubSize == 0 ) { if ( bSrcRevise ) { return sText; } else { return xrtCopyStringW(sText, iSize); } }
	// 不改动源数据时，直接创建副本
	if ( bSrcRevise == FALSE ) {
		sText = xrtCopyStringW(sText, iSize);
	}
	int iCount = 0;
	for ( int i = 0; i < iSize; i++ ) {
		int bCopy = TRUE;
		for ( int j = 0; j < iSubSize; j++ ) {
			if ( sText[i] == sSubText[j] ) {
				iCount++;
				bCopy = FALSE;
				break;
			}
		}
		if ( bCopy ) {
			sText[i - iCount] = sText[i];
		}
	}
	sText[iSize - iCount] = 0;
	xCore.iRet = iCount;
	return sText;
}



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
		if ( sRet == NULL ) {
			xrtSetError(XRT_ERROR_MALLOC, FALSE);
			xCore.iRet = 0;
			return (ustr)xCore.sNull;
		}
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
		if ( sRet == NULL ) {
			xrtSetError(XRT_ERROR_MALLOC, FALSE);
			xCore.iRet = 0;
			return (wstr)xCore.sNull;
		}
		va_start(ip, sFormat);
		iSize = vsnwprintf(sRet, iSize + 1, sFormat, ip);
		va_end(ip);
		sRet[iSize] = 0;
		xCore.iRet = iSize;
		return sRet;
	} else {
		return (wstr)xCore.sNull;
	}
}



// 字符串替换（需使用 xrtFree 释放）
XXAPI ustr xrtReplace(ustr sText, size_t iSize, ustr sSubText, size_t iSubSize, ustr sRepText, size_t iRepSize)
{
	if ( sText == NULL ) { xCore.iRet = 0; return (ustr)xCore.sNull; }
	if ( iSize == 0 ) { iSize = strlen(sText); }
	if ( iSize == 0 ) { xCore.iRet = 0; return (ustr)xCore.sNull; }
	if ( sSubText == NULL ) { xCore.iRet = iSize; return xrtCopyString(sText, iSize); }
	if ( iSubSize == 0 ) { iSubSize = strlen(sSubText); }
	if ( iSubSize == 0 ) { xCore.iRet = iSize; return xrtCopyString(sText, iSize); }
	if ( sRepText == NULL ) { iRepSize = 0; } else { if ( iRepSize == 0 ) { iRepSize = strlen(sRepText); } }
	// 计算 sSubText 在 sText 中出现的次数
	size_t iFindCount = 0;
	ustr sTextPtr;
	ustr sSubPos;
	for ( sTextPtr = sText; (sSubPos = memmem(sTextPtr, iSize - (sTextPtr - sText) + 1, sSubText, iSubSize)); sTextPtr = sSubPos + iSubSize ) {
		iFindCount++;
	}
	// 为新字符串分配内存
	size_t iRetSize = iSize + iFindCount * (iRepSize - iSubSize);
	ustr sRet = (ustr)xrtMalloc(iRetSize + 1);
	if ( sRet == NULL ) {
		xrtSetError(XRT_ERROR_MALLOC, FALSE);
		xCore.iRet = 0;
		return (ustr)xCore.sNull;
	}
	// 复制原始字符串, 替换需要改变的部分
	ustr sRetPtr = sRet;
	for ( sTextPtr = sText; (sSubPos = memmem(sTextPtr, iSize - (sTextPtr - sText) + 1, sSubText, iSubSize)); sTextPtr = sSubPos + iSubSize ) {
		size_t iSkipSize = sSubPos - sTextPtr;
		// 复制前面的部分，直到出现要查找的字符串
		strncpy(sRetPtr, sTextPtr, iSkipSize);
		sRetPtr += iSkipSize;
		// 复制要替换的字符串
		strncpy(sRetPtr, sRepText, iRepSize);
		sRetPtr += iRepSize;
	}
	// 复制最后一段剩下的字符串
	if ( &sText[iSize] > sTextPtr ) {
		memcpy(sRetPtr, sTextPtr, &sText[iSize] - sTextPtr);
	}
	sRet[iRetSize] = 0;
	xCore.iRet = iRetSize;
	return sRet;
}
XXAPI wstr xrtReplaceW(wstr sText, size_t iSize, wstr sSubText, size_t iSubSize, wstr sRepText, size_t iRepSize)
{
	if ( sText == NULL ) { xCore.iRet = 0; return (wstr)xCore.sNull; }
	if ( iSize == 0 ) { iSize = wcslen(sText); }
	if ( iSize == 0 ) { xCore.iRet = 0; return (wstr)xCore.sNull; }
	if ( sSubText == NULL ) { xCore.iRet = iSize; return xrtCopyStringW(sText, iSize); }
	if ( iSubSize == 0 ) { iSubSize = wcslen(sSubText); }
	if ( iSubSize == 0 ) { xCore.iRet = iSize; return xrtCopyStringW(sText, iSize); }
	if ( sRepText == NULL ) { iRepSize = 0; } else { if ( iRepSize == 0 ) { iRepSize = wcslen(sRepText); } }
	// 计算 sSubText 在 sText 中出现的次数
	size_t iFindCount = 0;
	wstr sTextPtr;
	wstr sSubPos;
	for ( sTextPtr = sText; (sSubPos = memmem(sTextPtr, (iSize - (sTextPtr - sText) + 1) * sizeof(wchar_t), sSubText, iSubSize * sizeof(wchar_t))); sTextPtr = sSubPos + iSubSize ) {
		iFindCount++;
	}
	// 为新字符串分配内存
	size_t iRetSize = iSize + iFindCount * (iRepSize - iSubSize);
	wstr sRet = (wstr)xrtMalloc( (iRetSize + 1) * sizeof(wchar_t) );
	if ( sRet == NULL ) {
		xrtSetError(XRT_ERROR_MALLOC, FALSE);
		xCore.iRet = 0;
		return (wstr)xCore.sNull;
	}
	// 复制原始字符串, 替换需要改变的部分
	wstr sRetPtr = sRet;
	for ( sTextPtr = sText; (sSubPos = memmem(sTextPtr, (iSize - (sTextPtr - sText) + 1) * sizeof(wchar_t), sSubText, iSubSize * sizeof(wchar_t))); sTextPtr = sSubPos + iSubSize ) {
		size_t iSkipSize = sSubPos - sTextPtr;
		// 复制前面的部分，直到出现要查找的字符串
		wcsncpy(sRetPtr, sTextPtr, iSkipSize);
		sRetPtr += iSkipSize;
		// 复制要替换的字符串
		wcsncpy(sRetPtr, sRepText, iRepSize);
		sRetPtr += iRepSize;
	}
	// 复制最后一段剩下的字符串
	if ( &sText[iSize] > sTextPtr ) {
		memcpy(sRetPtr, sTextPtr, (&sText[iSize] - sTextPtr) * sizeof(wchar_t));
	}
	sRet[iRetSize] = 0;
	xCore.iRet = iRetSize;
	return sRet;
}



// 字符串分割（需使用 xrtFree 释放）
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


