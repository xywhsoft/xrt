


// 内存查找
#if defined(_WIN32) || defined(_WIN64)
	// memmem 相关处理
	XXAPI ptr memmem(ptr pMem, size_t iMemSize, ptr pSub, size_t iSubSize)
	{
		size_t arrShift[256];
		if ( (iMemSize == 0) || (iSubSize == 0) ) {
			return NULL;
		}
		if ( iMemSize  < iSubSize ) {
			return NULL;
		}
		unsigned char* pMemC = (unsigned char*)pMem;
		unsigned char* pSubC = (unsigned char*)pSub;
		if ( iSubSize == 1 ) {
			return memchr(pMem, pSubC[0], iMemSize);
		}
		for ( size_t i = 0; i < 256; i++ ) {
			arrShift[i] = iSubSize;
		}
		for ( size_t i = 0; i + 1 < iSubSize; i++ ) {
			arrShift[pSubC[i]] = iSubSize - i - 1;
		}
		for ( size_t i = 0; i <= (iMemSize - iSubSize); ) {
			unsigned char iLast = pMemC[i + iSubSize - 1];
			if ( iLast == pSubC[iSubSize - 1] &&
				memcmp(&pMemC[i], pSubC, iSubSize - 1) == 0 ) {
				return &pMemC[i];
			}
			i += arrShift[iLast];
		}
		return NULL;
	}
#endif



// 获取字符串长度 ( 补充 utf16 和 utf32 支持 )
XXAPI size_t u16len(u16str sText)
{
	size_t iSize = 0;
	if ( sText == NULL ) { return 0; }
	while ( sText[iSize] != 0 ) {
		iSize++;
	}
	return iSize;
}


// u32len 相关处理
XXAPI size_t u32len(u32str sText)
{
	size_t iSize = 0;
	if ( sText == NULL ) { return 0; }
	while ( sText[iSize] != 0 ) {
		iSize++;
	}
	return iSize;
}


