


// 内存查找
#if defined(_WIN32) || defined(_WIN64)
	// memmem 相关处理
	XXAPI ptr memmem(ptr pMem, size_t iMemSize, ptr pSub, size_t iSubSize)
	{
		if ( (iMemSize == 0) || (iSubSize == 0) ) {
			return NULL;
		}
		if ( iMemSize  < iSubSize ) {
			return NULL;
		}
		char* pMemC = pMem;
		char* pSubC = pSub;
		size_t iRange = iMemSize - iSubSize;
		for ( size_t i = 0; i <= iRange; i++ ) {
			char* pPos = &pMemC[i];
			int bOK = TRUE;
			for ( size_t j = 0; j < iSubSize; j++ ) {
				if ( pPos[j] != pSubC[j] ) {
					bOK = FALSE;
					break;
				}
			}
			if ( bOK ) {
				return pPos;
			}
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


