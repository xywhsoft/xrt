


// 申请内存
XXAPI ptr xrtMalloc(size_t iSize)
{
	ptr (*procMalloc)(size_t) = xCore.malloc ? xCore.malloc : malloc;
	ptr mem = procMalloc(iSize);
	if ( mem == NULL ) {
		xrtSetError("memory allocate failed.", FALSE);
		return NULL;
	}
	return mem;
}



// 申请类内存
XXAPI ptr xrtCalloc(size_t iNum, size_t iSize)
{
	ptr (*procCalloc)(size_t, size_t) = xCore.calloc ? xCore.calloc : calloc;
	ptr mem = procCalloc(iNum, iSize);
	if ( mem == NULL ) {
		xrtSetError("class memory allocate failed.", FALSE);
	}
	return mem;
}



// 重新申请内存
XXAPI ptr xrtRealloc(ptr pMem, size_t iSize)
{
	ptr (*procRealloc)(ptr, size_t) = xCore.realloc ? xCore.realloc : realloc;
	ptr mem = procRealloc(pMem, iSize);
	if ( mem == NULL ) {
		xrtSetError("memory reallocate failed.", FALSE);
	}
	return mem;
}



// 释放内存（ 会先判断是否为 null ）
XXAPI void xrtFree(ptr pmem)
{
	void (*procFree)(ptr) = xCore.free ? xCore.free : free;
	if ( pmem && (pmem != xCore.sNull) ) { procFree(pmem); }
}



// 申请无需主动释放的临时内存（线程级）
XXAPI ptr xrtTempMemory(size_t iSize)
{
	xrtThreadData* pThreadData = xrtThreadGetCurrent();

	if ( pThreadData == NULL ) {
		xrtSetError("current thread is not attached to xrt runtime.", FALSE);
		return NULL;
	}

	// 申请内存
	ptr pMem = xrtMalloc(iSize);
	if ( pMem == NULL ) {
		return NULL;
	}

	// 释放过期内存
	if ( pThreadData->TempMem[pThreadData->TempMemIdx] ) {
		xrtFree(pThreadData->TempMem[pThreadData->TempMemIdx]);
		pThreadData->TempMem[pThreadData->TempMemIdx] = NULL;
	}

	// 处理环形临时内存数据
	pThreadData->TempMem[pThreadData->TempMemIdx] = pMem;
	pThreadData->TempMemIdx++;
	if ( pThreadData->TempMemIdx >= XRT_TEMP_SLOT_COUNT ) {
		pThreadData->TempMemIdx = 0;
	}

	// 返回内存指针
	return pMem;
}



// 释放当前线程的所有临时内存
XXAPI void xrtFreeTempMemory()
{
	xrtThreadData* pThreadData = xrtThreadGetCurrent();

	if ( pThreadData == NULL ) {
		return;
	}

	for ( int i = 0; i < XRT_TEMP_SLOT_COUNT; i++ ) {
		if ( pThreadData->TempMem[i] ) {
			xrtFree(pThreadData->TempMem[i]);
			pThreadData->TempMem[i] = NULL;
		}
	}
	pThreadData->TempMemIdx = 0;
}



// 设置错误（线程级）
XXAPI void xrtSetError(str sError, bool bFree)
{
	xrtThreadData* pThreadData = xrtThreadGetCurrent();

	// 回调通知
	if ( xCore.OnError ) {
		xCore.OnError(sError);
	}

	if ( pThreadData == NULL ) {
		return;
	}

	// 释放旧的错误信息
	if ( pThreadData->bFreeLastError && pThreadData->LastError && pThreadData->LastError != xCore.sNull ) {
		xrtFree(pThreadData->LastError);
	}
	pThreadData->LastError = sError;
	pThreadData->bFreeLastError = bFree;
}
XXAPI void xrtSetErrorU16(u16str sError, size_t iSize, bool bFree)
{
	str sErrorU8 = xrtUTF16to8(sError, iSize, NULL);
	if ( bFree ) {
		xrtFree(sError);
	}
	xrtSetError(sErrorU8, TRUE);
}
XXAPI void xrtSetErrorU32(u32str sError, size_t iSize, bool bFree)
{
	str sErrorU8 = xrtUTF32to8(sError, iSize, NULL);
	if ( bFree ) {
		xrtFree(sError);
	}
	xrtSetError(sErrorU8, TRUE);
}



// 清除当前线程错误
XXAPI void xrtClearError()
{
	xrtThreadData* pThreadData = xrtThreadGetCurrent();

	if ( pThreadData == NULL ) {
		return;
	}

	if ( pThreadData->bFreeLastError && pThreadData->LastError && pThreadData->LastError != xCore.sNull ) {
		xrtFree(pThreadData->LastError);
	}
	pThreadData->LastError = xCore.sNull;
	pThreadData->bFreeLastError = FALSE;
}


