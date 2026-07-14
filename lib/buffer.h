


// 创建内存缓冲区管理器
XXAPI xbuffer xrtBufferCreate(uint32 iStep)
{
	xbuffer pBuf = xrtMalloc(sizeof(xbuffer_struct));
	if ( pBuf ) {
		xrtBufferInit(pBuf, iStep);
	}
	return pBuf;
}

// 销毁内存缓冲区管理器
XXAPI void xrtBufferDestroy(xbuffer pBuf)
{
	if ( pBuf ) {
		xrtBufferUnit(pBuf);
		xrtFree(pBuf);
	}
}

// 初始化缓冲区管理器（对自维护结构体指针使用）
XXAPI void xrtBufferInit(xbuffer pBuf, uint32 iStep)
{
	pBuf->Buffer = NULL;
	pBuf->Length = 0;
	pBuf->AllocLength = 0;
	pBuf->AllocStep = iStep ? iStep : XBUFFER_ALLOC_STEP;
}

// 释放缓冲区管理器（对自维护结构体指针使用）
XXAPI void xrtBufferUnit(xbuffer pBuf)
{
	if ( pBuf->Buffer ) { xrtFree(pBuf->Buffer); pBuf->Buffer = NULL; }
	pBuf->Length = 0;
	pBuf->AllocLength = 0;
}

// 分配内存
XXAPI bool xrtBufferMalloc(xbuffer pBuf, uint32 iCount)
{
	if ( iCount == 0 ) {
		// 清空
		xrtBufferUnit(pBuf);
		return TRUE;
	} else if ( iCount > pBuf->AllocLength ) {
		// 增量
		ptr pNew = xrtRealloc(pBuf->Buffer, iCount);
		if ( pNew ) {
			pBuf->AllocLength = iCount;
			pBuf->Buffer = pNew;
			return TRUE;
		}
	} else if ( iCount < pBuf->AllocLength ) {
		// 裁剪
		ptr pNew = xrtRealloc(pBuf->Buffer, iCount);
		if ( pNew ) {
			pBuf->AllocLength = iCount;
			pBuf->Buffer = pNew;
			if ( iCount <= pBuf->Length ) {
				// 需要裁剪数据
				pBuf->Length = iCount;
			}
			return TRUE;
		}
	} else {
		// 不变
		return TRUE;
	}
	return FALSE;
}

// 中间添加数据（可以复制或者开辟新的数据区，不会自动将新开辟的数据区填充 \0）
XXAPI bool xrtBufferInsert(xbuffer pBuf, uint32 iPos, ptr pData, uint32 iSize, uint32 bStrMode)
{
	uint64 iNeedSize;
	uint64 iAllocSize;
	uint32 iOldLength;
	ptr pCopy = NULL;
	ptr pSource = pData;
	uintptr_t iBufferBegin;
	uintptr_t iBufferEnd;
	uintptr_t iSource;
	if ( pBuf == NULL || iPos > pBuf->Length ) {
		return FALSE;
	}
	// 长度为 0 时自动计算数据长度
	if ( iSize == 0 ) {
		if ( pData == NULL ) {
			return FALSE;
		}
		if ( bStrMode == XBUF_ANSI ) {
			iSize = strlen(pData);
		} else if ( bStrMode == XBUF_UTF16 ) {
			iSize = u16len(pData) * XBUF_UTF16;
		} else if ( bStrMode == XBUF_UTF32 ) {
			iSize = u32len(pData) * XBUF_UTF32;
		} else {
			return FALSE;
		}
	}
	if ( iSize == 0 ) {
		return TRUE;
	}
	if ( pData == NULL ) {
		return FALSE;
	}
	iOldLength = pBuf->Length;

	// 允许插入 Buffer 自身的数据。扩容和后缀搬移前先保存源片段。
	iBufferBegin = (uintptr_t)pBuf->Buffer;
	iBufferEnd = iBufferBegin + iOldLength;
	iSource = (uintptr_t)pData;
	if ( pBuf->Buffer != NULL && iSource >= iBufferBegin && iSource < iBufferEnd ) {
		if ( (uint64)(iSource - iBufferBegin) + iSize > iOldLength ) {
			return FALSE;
		}
		pCopy = xrtMalloc(iSize);
		if ( pCopy == NULL ) {
			return FALSE;
		}
		memcpy(pCopy, pData, iSize);
		pSource = pCopy;
	}
	// 分配内存
	iNeedSize = (uint64)iOldLength + (uint64)iSize + (uint64)bStrMode;
	if ( iNeedSize > UINT32_MAX ) {
		if ( pCopy != NULL ) { xrtFree(pCopy); }
		return FALSE;
	}
	if ( iNeedSize > pBuf->AllocLength ) {
		iAllocSize = iNeedSize + pBuf->AllocStep;
		if ( iAllocSize < iNeedSize || iAllocSize > UINT32_MAX ) {
			if ( pCopy != NULL ) { xrtFree(pCopy); }
			return FALSE;
		}
		if ( xrtBufferMalloc(pBuf, (uint32)iAllocSize) == 0 ) {
			if ( pCopy != NULL ) { xrtFree(pCopy); }
			return FALSE;
		}
	}
	// 为新数据腾出空间，再写入插入内容。
	memmove(&pBuf->Buffer[iPos + iSize], &pBuf->Buffer[iPos], iOldLength - iPos);
	memcpy(&pBuf->Buffer[iPos], pSource, iSize);
	pBuf->Length = iOldLength + iSize;
	if ( pCopy != NULL ) { xrtFree(pCopy); }
	// 字符串模式自动添加 \0
	if ( bStrMode ) {
		for ( uint32 i = 0; i < bStrMode; i++ ) {
			pBuf->Buffer[pBuf->Length + i] = 0;
		}
	}
	return TRUE;
}

// 末尾添加数据
XXAPI bool xrtBufferAppend(xbuffer pBuf, ptr pData, uint32 iSize, uint32 bStrMode)
{
	return xrtBufferInsert(pBuf, pBuf->Length, pData, iSize, bStrMode);
}


// 通过原始内存创建 Buffer
XXAPI xbuffer xrtBufferFrom(ptr pData, size_t iSize)
{
	xbuffer pBuf;
	if ( pData == NULL && iSize != 0u ) { return NULL; }
	if ( iSize > 0xFFFFFFFFu ) { return NULL; }
	pBuf = xrtBufferCreate(0u);
	if ( pBuf == NULL ) { return NULL; }
	if ( iSize != 0u && !xrtBufferAppend(pBuf, pData, (uint32)iSize, XBUF_BINARY) ) {
		xrtBufferDestroy(pBuf);
		return NULL;
	}
	return pBuf;
}


// 接管由 xrtMalloc 分配的原始内存
XXAPI xbuffer xrtBufferAdopt(ptr pData, size_t iSize)
{
	xbuffer pBuf;
	if ( (pData == NULL && iSize != 0u) || iSize > UINT32_MAX ) {
		return NULL;
	}
	pBuf = (xbuffer)xrtMalloc(sizeof(xbuffer_struct));
	if ( pBuf == NULL ) {
		return NULL;
	}
	xrtBufferInit(pBuf, 0u);
	if ( iSize == 0u ) {
		if ( pData != NULL && pData != xCore.sNull ) {
			xrtFree(pData);
		}
		return pBuf;
	}
	pBuf->Buffer = pData;
	pBuf->Length = (uint32)iSize;
	pBuf->AllocLength = (uint32)iSize;
	return pBuf;
}


// 将 HEX 文本解码为 Buffer
XXAPI xbuffer xrtBufferFromHex(str sText, size_t iSize)
{
	ptr pData;
	xbuffer pBuf;
	size_t iDataSize;
	if ( sText == NULL ) { return NULL; }
	if ( iSize == 0u ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0u ) { return xrtBufferCreate(0u); }
	if ( (iSize & 1u) != 0u ) { return NULL; }
	pData = xrtHexDecode(sText, iSize);
	if ( pData == NULL || pData == xCore.sNull ) { return NULL; }
	iDataSize = iSize / 2u;
	pBuf = xrtBufferFrom(pData, iDataSize);
	xrtFree(pData);
	return pBuf;
}


// 将 Base64 文本解码为 Buffer
XXAPI xbuffer xrtBufferFromBase64(str sText, size_t iSize, str sTable)
{
	ptr pData;
	xbuffer pBuf;
	size_t iDataSize;
	if ( sText == NULL ) { return NULL; }
	if ( iSize == 0u ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0u ) { return xrtBufferCreate(0u); }
	if ( (iSize % 4u) != 0u ) { return NULL; }
	iDataSize = (iSize / 4u) * 3u;
	if ( sText[iSize - 1u] == '=' ) { iDataSize--; }
	if ( sText[iSize - 2u] == '=' ) { iDataSize--; }
	pData = xrtBase64Decode(sText, iSize, sTable);
	if ( pData == NULL || pData == xCore.sNull ) { return NULL; }
	pBuf = xrtBufferFrom(pData, iDataSize);
	xrtFree(pData);
	return pBuf;
}


