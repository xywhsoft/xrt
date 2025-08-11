


// 申请内存
XXAPI ptr xrtMalloc(size_t iSize)
{
	return xCore.malloc(iSize);
}



// 申请类内存
XXAPI ptr xrtCalloc(size_t iNum, size_t iSize)
{
	return xCore.calloc(iNum, iSize);
}



// 重新申请内存
XXAPI ptr xrtRealloc(ptr pMem, size_t iSize)
{
	return xCore.realloc(pMem, iSize);
}



// 释放内存（ 会先判断是否为 null ）
XXAPI void xrtFree(ptr pmem)
{
	if ( pmem && (pmem != xCore.sNull) ) { xCore.free(pmem); }
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



// 清除错误
XXAPI void xrtClearError()
{
	if ( xCore.__pri_FreeError && xCore.LastError ) {
		xrtFree(xCore.LastError);
	}
	xCore.LastError = xCore.sNull;
	xCore.__pri_FreeError = FALSE;
}


