


// 申请内存
XXAPI ptr xrtMalloc(size_t iSize)
{
	ptr mem = xCore.malloc(iSize);
	if ( mem == NULL ) {
		xrtSetError("memory allocate failed.", FALSE);
	}
	return mem;
}



// 申请类内存
XXAPI ptr xrtCalloc(size_t iNum, size_t iSize)
{
	ptr mem = xCore.calloc(iNum, iSize);
	if ( mem == NULL ) {
		xrtSetError("memory (class) allocate failed.", FALSE);
	}
	return mem;
}



// 重新申请内存
XXAPI ptr xrtRealloc(ptr pMem, size_t iSize)
{
	ptr mem = xCore.realloc(pMem, iSize);
	if ( mem == NULL ) {
		xrtSetError("memory reallocate failed.", FALSE);
	}
	return mem;
}



// 释放内存（ 会先判断是否为 null ）
XXAPI void xrtFree(ptr pmem)
{
	if ( pmem && (pmem != xCore.sNull) ) { xCore.free(pmem); }
}



// 申请无需主动释放的临时内存
XXAPI ptr xrtTempMemory(size_t iSize)
{
	// 申请内存
	ptr pMem = xrtMalloc(iSize);
	if ( pMem == NULL ) {
		return NULL;
	}
	// 释放过期内存
	if ( xCore.TempMem[xCore.TempMemIdx] ) {
		free(xCore.TempMem[xCore.TempMemIdx]);
		xCore.TempMem[xCore.TempMemIdx] = NULL;
	}
	// 处理环形临时内存数据
	xCore.TempMem[xCore.TempMemIdx] = pMem;
	xCore.TempMemIdx++;
	if ( xCore.TempMemIdx > 31 ) {
		xCore.TempMemIdx = 0;
	}
	// 返回内存指针
	return pMem;
}



// 设置错误
XXAPI void xrtSetError(str sError, int bFree)
{
	if ( xCore.DebugMode ) {
		printf("X Runtime Error : %s\n", sError);
	}
	if ( xCore.__pri_FreeError && xCore.LastError ) {
		xrtFree(xCore.LastError);
	}
	xCore.LastError = sError;
	xCore.__pri_FreeError = bFree;
}
XXAPI void xrtSetErrorU16(u16str sError, size_t iSize, int bFree)
{
	str sErrorU8 = xrtUTF16to8(sError, iSize);
	if ( bFree ) {
		xrtFree(sError);
	}
	xrtSetError(sErrorU8, TRUE);
}
XXAPI void xrtSetErrorU32(u32str sError, size_t iSize, int bFree)
{
	str sErrorU8 = xrtUTF32to8(sError, iSize);
	if ( bFree ) {
		xrtFree(sError);
	}
	xrtSetError(sErrorU8, TRUE);
}



// 清除错误
XXAPI void xrtClearError()
{
	if ( xCore.__pri_FreeError && xCore.LastError ) {
		xrtFree(xCore.LastError);
	}
	xCore.LastError = xCore.sNull;
	xCore.__pri_FreeError = FALSE;
}


