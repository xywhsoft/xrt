#include "../internal/xrt_process_pipeline.h"



#if defined(XRT_FEATURE_PROCESS_PIPELINE) && \
	!defined(_WIN32) && !defined(_WIN64)

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>



/* 为管道 fd 设置 close-on-exec，避免泄露到不相关阶段。 */
static bool __xrtProcessPipeCloseOnExec(int iFd)
{
	int iFlags = fcntl(iFd, F_GETFD);

	return (iFlags >= 0) &&
		(fcntl(iFd, F_SETFD, iFlags | FD_CLOEXEC) == 0);
}



/* 创建带 close-on-exec 的 POSIX 匿名管道。 */
bool __xrtProcessPipeCreate(xprocesspipe* pPipe)
{
	int pFds[2] = { -1, -1 };
	int iError;

	pPipe->Read = -1;
	pPipe->Write = -1;
	if ( (pipe(pFds) != 0) ||
		!__xrtProcessPipeCloseOnExec(pFds[0]) ||
		!__xrtProcessPipeCloseOnExec(pFds[1]) ) {
		iError = errno;
		if ( pFds[0] >= 0 ) {
			(void)close(pFds[0]);
		}
		if ( pFds[1] >= 0 ) {
			(void)close(pFds[1]);
		}
		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_PIPE,
			"pipeline.pipe",
			"pipeline connector could not be created",
			iError
		);
		return false;
	}
	pPipe->Read = (intptr_t)pFds[0];
	pPipe->Write = (intptr_t)pFds[1];
	return true;
}



/* 关闭 POSIX 管道两端并恢复无效槽值。 */
void __xrtProcessPipeClose(xprocesspipe* pPipe)
{
	if ( pPipe->Read != -1 ) {
		(void)close((int)pPipe->Read);
	}
	if ( pPipe->Write != -1 ) {
		(void)close((int)pPipe->Write);
	}
	pPipe->Read = -1;
	pPipe->Write = -1;
}

#endif
