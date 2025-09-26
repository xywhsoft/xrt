


// 创建内存缓冲区管理器
XXAPI xbuffer xrtBufferCreate(unsigned int iAllocLength, unsigned int iStep)
{
	xbuffer pBuf = xrtMalloc(sizeof(xbuffer_struct));
	if ( pBuf == NULL ) {
		xrtSetError(xCore.ERROR_DESC.MALLOC, FALSE);
		return NULL;
	}
	xrtBufferInit(pBuf, iAllocLength, iStep);
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
XXAPI void xrtBufferInit(xbuffer pBuf, unsigned int iAllocLength, unsigned int iStep)
{
	pBuf->Buffer = NULL;
	pBuf->Length = 0;
	pBuf->AllocLength = 0;
	pBuf->AllocStep = iStep ? iStep : XBUFFER_ALLOC_STEP;
	if ( iAllocLength ) {
		xrtBufferMalloc(pBuf, iAllocLength);
	}
}

// 释放缓冲区管理器（对自维护结构体指针使用）
XXAPI void xrtBufferUnit(xbuffer pBuf)
{
	if ( pBuf->Buffer ) { xrtFree(pBuf->Buffer); pBuf->Buffer = NULL; }
	pBuf->Length = 0;
	pBuf->AllocLength = 0;
}

// 分配内存
XXAPI int xrtBufferMalloc(xbuffer pBuf, unsigned int iCount)
{
	if ( iCount > pBuf->AllocLength ) {
		// 增量
		void* pNew = xrtRealloc(pBuf->Buffer, iCount);
		if ( pNew ) {
			pBuf->AllocLength = iCount;
			pBuf->Buffer = pNew;
			return -1;
		}
	} else if ( iCount < pBuf->AllocLength ) {
		// 裁剪
		void* pNew = xrtRealloc(pBuf->Buffer, iCount);
		if ( pNew ) {
			pBuf->AllocLength = iCount;
			pBuf->Buffer = pNew;
			if ( iCount <= pBuf->Length ) {
				// 需要裁剪数据
				pBuf->Length = iCount;
			}
			return -1;
		}
	} else if ( iCount = 0 ) {
		// 清空
		xrtBufferUnit(pBuf);
		return -1;
	} else {
		// 不变
		return -1;
	}
	return 0;
}

// 中间添加数据（可以复制或者开辟新的数据区，不会自动将新开辟的数据区填充 \0）
XXAPI int xrtBufferInsert(xbuffer pBuf, unsigned int iPos, void* pData, unsigned int iSize, unsigned int bStrMode)
{
	// 长度为 0 时自动计算数据长度
	if ( iSize == 0 ) {
		if ( bStrMode == XBUFFER_ANSI ) {
			iSize = strlen(pData);
		} else if ( bStrMode == XBUFFER_UTF16 ) {
			iSize = u16len(pData) * XBUFFER_UTF16;
		} else if ( bStrMode == XBUFFER_UTF32 ) {
			iSize = u32len(pData) * XBUFFER_UTF32;
		}
	}
	// 分配内存
	if ( (iPos + iSize + bStrMode) > pBuf->AllocLength ) {
		if ( xrtBufferMalloc(pBuf, iPos + iSize + bStrMode + pBuf->AllocStep) == 0 ) {
			return 0;
		}
	}
	// 复制数据
	if ( iSize ) {
		memcpy(&pBuf->Buffer[iPos], pData, iSize);
		pBuf->Length = iPos + iSize;
	}
	// 字符串模式自动添加 \0
	if ( bStrMode ) {
		for ( int i = 0; i < bStrMode; i++ ) {
			pBuf->Buffer[pBuf->Length + i] = 0;
		}
	}
	return -1;
}

// 末尾添加数据
XXAPI int xrtBufferAppend(xbuffer pBuf, void* pData, unsigned int iSize, unsigned int bStrMode)
{
	return xrtBufferInsert(pBuf, pBuf->Length, pData, iSize, bStrMode);
}


