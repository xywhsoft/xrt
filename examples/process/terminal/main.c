#include <stdio.h>

#include <xrt.h>



/* 在伪终端中执行命令并流式转发合并输出。 */
int main(void)
{
	xprocessconfig Config;
	xprocess* pProcess;
	char sOutput[256];
	int64 iRead;

	if ( !xrtProcessTerminalSupported() ) {
		return 0;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( !xrtProcessShellConfigInit(&Config, "echo terminal output") ) {
			return 1;
		}
	#else
		if ( !xrtProcessShellConfigInit(&Config, "printf 'terminal output\\n'") ) {
			return 1;
		}
	#endif
	Config.Terminal = true;
	pProcess = xrtProcessSpawn(&Config);
	if ( pProcess == NULL ) {
		return 2;
	}
	while ( (iRead = xrtProcessRead(
		pProcess,
		XPROCESS_STDOUT,
		sOutput,
		sizeof(sOutput)
	)) > 0 ) {
		(void)fwrite(sOutput, 1u, (size_t)iRead, stdout);
	}
	if ( (iRead < 0) || (xrtProcessWait(pProcess) != XWAIT_OK) ) {
		xrtProcessDestroy(pProcess);
		return 3;
	}
	xrtProcessDestroy(pProcess);
	return 0;
}
