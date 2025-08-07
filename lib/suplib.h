


// windows 平台缺少的函数
#if defined(_WIN32) || defined(_WIN64)
	ptr memmem(ptr pMem, size_t iMemSize, ptr pSub, size_t iSubSize)
	{
		if ( (iMemSize == 0) || (iSubSize == 0) ) {
			return NULL;
		}
		if ( iMemSize  < iSubSize ) {
			return NULL;
		}
		size_t iRange = iMemSize - iSubSize;
		for ( int i = 0; i < iRange; i++ ) {
			char* pPos = &pMem[i];
			int bOK = TRUE;
			for ( int j = 0; j < iSubSize; j++ ) {
				if ( pPos[j] != ((char*)pSub)[j] ) {
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


