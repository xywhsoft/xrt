#include <xrt.h>

#include <stdio.h>
#include <string.h>



/* 直接连接真实管道，适合需要自行控制流式读取的调用方。 */
int main(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		const cstr pArgs[] = { "/C", "more" };
		cstr sProgram = "cmd.exe";
	#else
		const cstr pArgs[] = { "-c", "cat" };
		cstr sProgram = "/bin/sh";
	#endif
	static const char sInput[] = "streamed through a child process\n";
	xprocessconfig Config;
	xprocess* pProcess;
	char pOutput[128];
	int64 iRead;

	if ( !xrtProcessConfigInit(&Config) ) {
		return 1;
	}
	Config.Program = sProgram;
	Config.Args = pArgs;
	Config.ArgCount = sizeof(pArgs) / sizeof(pArgs[0]);
	Config.Stdin.Mode = XPROCESS_IO_PIPE;
	Config.Stdout.Mode = XPROCESS_IO_PIPE;
	Config.Stderr.Mode = XPROCESS_IO_NULL;
	pProcess = xrtProcessSpawn(&Config);
	if ( pProcess == NULL ) {
		const xerror* pError = xrtGetError();

		fprintf(
			stderr,
			"process spawn failed: %s: %s (system=%d)\n",
			pError != NULL ? xrtErrorOperation(pError) : "spawn",
			pError != NULL ? xrtErrorMessage(pError) : "unknown error",
			pError != NULL ? xrtErrorSystemCode(pError) : 0
		);
		return 2;
	}
	if ( xrtProcessWrite(pProcess, sInput, sizeof(sInput) - 1u) <= 0 ) {
		xrtProcessDestroy(pProcess);
		return 3;
	}
	(void)xrtProcessClose(pProcess, XPROCESS_STDIN);
	while ( (iRead = xrtProcessRead(
		pProcess,
		XPROCESS_STDOUT,
		pOutput,
		sizeof(pOutput)
	)) > 0 ) {
		(void)fwrite(pOutput, 1u, (size_t)iRead, stdout);
	}
	if ( (iRead < 0) || (xrtProcessWait(pProcess) != XWAIT_OK) ) {
		xrtProcessDestroy(pProcess);
		return 4;
	}
	xrtProcessDestroy(pProcess);
	return 0;
}
