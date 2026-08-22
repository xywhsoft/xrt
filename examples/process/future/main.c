#include <stdio.h>

#include <xrt.h>



/* 异步等待一个外部命令，并读取 Future 拥有的退出状态。 */
int main(void)
{
	const xprocessstatus* pStatus;
	xprocessconfig Config;
	xprocess* pProcess;
	xfuture* pFuture;
	bool bSuccess;

	#if defined(_WIN32) || defined(_WIN64)
		if ( !xrtProcessShellConfigInit(&Config, "exit /B 0") ) {
			return 1;
		}
	#else
		if ( !xrtProcessShellConfigInit(&Config, "exit 0") ) {
			return 1;
		}
	#endif
	Config.Stdin.Mode = XPROCESS_IO_NULL;
	Config.Stdout.Mode = XPROCESS_IO_NULL;
	Config.Stderr.Mode = XPROCESS_IO_NULL;
	pProcess = xrtProcessSpawn(&Config);
	if ( pProcess == NULL ) {
		return 2;
	}
	pFuture = xrtProcessWaitAsync(pProcess);
	xrtProcessDestroy(pProcess);
	if ( (pFuture == NULL) || (xrtFutureWait(pFuture) != XWAIT_OK) ) {
		xrtFutureDestroy(pFuture);
		return 3;
	}
	pStatus = (const xprocessstatus*)xrtFutureValue(pFuture);
	printf("exit code: %d\n", pStatus != NULL ? pStatus->Code : -1);
	bSuccess = (pStatus != NULL) && (pStatus->Code == 0);
	xrtFutureDestroy(pFuture);
	return bSuccess ? 0 : 4;
}
