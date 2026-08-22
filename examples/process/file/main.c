#include <xrt.h>



/* 把命令输出直接重定向到 XRT 文件。 */
int main(void)
{
	xprocessconfig Config;
	xprocess* pProcess;
	xfile File = xrtOpen(
		"xrt-process-output.txt",
		XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE
	);

	if ( File == NULL ) {
		return 1;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( !xrtProcessShellConfigInit(&Config, "echo redirected output") ) {
			(void)xrtClose(File);
			return 2;
		}
	#else
		if ( !xrtProcessShellConfigInit(&Config, "printf 'redirected output\n'") ) {
			(void)xrtClose(File);
			return 2;
		}
	#endif
	Config.Stdin.Mode = XPROCESS_IO_NULL;
	Config.Stdout = xrtProcessFile(File);
	Config.Stderr.Mode = XPROCESS_IO_NULL;
	pProcess = xrtProcessSpawn(&Config);
	(void)xrtClose(File);
	if ( pProcess == NULL ) {
		return 3;
	}
	if ( xrtProcessWait(pProcess) != XWAIT_OK ) {
		xrtProcessDestroy(pProcess);
		return 4;
	}
	xrtProcessDestroy(pProcess);
	return 0;
}
