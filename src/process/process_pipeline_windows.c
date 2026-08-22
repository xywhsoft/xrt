#include "../internal/xrt_process_pipeline.h"



#if defined(XRT_FEATURE_PROCESS_PIPELINE) && \
	(defined(_WIN32) || defined(_WIN64))

/* 创建只由父进程持有、交给 Spawn 精确复制的匿名管道。 */
bool __xrtProcessPipeCreate(xprocesspipe* pPipe)
{
	HANDLE hRead = INVALID_HANDLE_VALUE;
	HANDLE hWrite = INVALID_HANDLE_VALUE;
	int iError;

	pPipe->Read = -1;
	pPipe->Write = -1;
	if ( !CreatePipe(&hRead, &hWrite, NULL, 0u) ) {
		iError = (int)GetLastError();
		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_PIPE,
			"pipeline.pipe",
			"pipeline connector could not be created",
			iError
		);
		return false;
	}
	pPipe->Read = (intptr_t)hRead;
	pPipe->Write = (intptr_t)hWrite;
	return true;
}



/* 关闭 Windows 管道两端并恢复无效槽值。 */
void __xrtProcessPipeClose(xprocesspipe* pPipe)
{
	if ( pPipe->Read != -1 ) {
		(void)CloseHandle((HANDLE)pPipe->Read);
	}
	if ( pPipe->Write != -1 ) {
		(void)CloseHandle((HANDLE)pPipe->Write);
	}
	pPipe->Read = -1;
	pPipe->Write = -1;
}

#endif
