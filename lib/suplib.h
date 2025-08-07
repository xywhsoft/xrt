


ptr memmem(ptr pMem, size_t iMemSize, ptr pSub, size_t iSubSize)
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
	for ( int i = 0; i < iRange; i++ ) {
		char* pPos = &pMemC[i];
		int bOK = TRUE;
		for ( int j = 0; j < iSubSize; j++ ) {
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


