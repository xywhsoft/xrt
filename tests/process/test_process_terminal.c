#include "../test.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <io.h>
#else
	#include <unistd.h>
#endif



/* 子进程在真实终端中报告三个标准流属性并完成一次交互。 */
static int testProcessTerminalChild(int argc, char** argv)
{
	char sInput[128];
	size_t iSize;
	int bStdin;
	int bStdout;
	int bStderr;

	if ( (argc < 3) ||
		(strcmp(argv[1], "--xrt-process-terminal-child") != 0) ) {
		return -1;
	}
	if ( strcmp(argv[2], "exit") == 0 ) {
		return 0;
	}
	#if defined(_WIN32) || defined(_WIN64)
		bStdin = _isatty(_fileno(stdin)) != 0;
		bStdout = _isatty(_fileno(stdout)) != 0;
		bStderr = _isatty(_fileno(stderr)) != 0;
	#else
		bStdin = isatty(STDIN_FILENO) != 0;
		bStdout = isatty(STDOUT_FILENO) != 0;
		bStderr = isatty(STDERR_FILENO) != 0;
	#endif
	fprintf(stdout, "tty:%d:%d:%d\n", bStdin, bStdout, bStderr);
	fputs("terminal-stderr\n", stderr);
	fflush(stdout);
	fflush(stderr);
	if ( fgets(sInput, sizeof(sInput), stdin) == NULL ) {
		return 91;
	}
	iSize = strlen(sInput);
	while ( (iSize != 0u) &&
		((sInput[iSize - 1u] == '\r') || (sInput[iSize - 1u] == '\n')) ) {
		sInput[--iSize] = 0;
	}
	fprintf(stdout, "reply:%s\n", sInput);
	fflush(stdout);
	return 0;
}



/* 循环处理平台允许的终端部分写入。 */
static bool testProcessTerminalWriteAll(
	xprocess* pProcess,
	const void* pData,
	size_t iSize
)
{
	const uint8* pBytes = (const uint8*)pData;
	size_t iOffset = 0u;

	while ( iOffset < iSize ) {
		int64 iWrite = xrtProcessWrite(
			pProcess,
			pBytes + iOffset,
			iSize - iOffset
		);

		if ( iWrite <= 0 ) {
			return false;
		}
		iOffset += (size_t)iWrite;
	}
	return true;
}



/* 读取合流后的终端输出直到伪终端关闭。 */
static size_t testProcessTerminalReadAll(
	xprocess* pProcess,
	char* sOutput,
	size_t iCapacity
)
{
	size_t iSize = 0u;

	while ( iSize < (iCapacity - 1u) ) {
		int64 iRead = xrtProcessRead(
			pProcess,
			XPROCESS_STDOUT,
			sOutput + iSize,
			iCapacity - iSize - 1u
		);

		testRequire(iRead >= 0, "terminal output read failed");
		if ( iRead == 0 ) {
			break;
		}
		iSize += (size_t)iRead;
	}
	sOutput[iSize] = 0;
	return iSize;
}



#if defined(XRT_FEATURE_PROCESS_RUN)
	/* 在二进制终端输出中查找一段确定性 ASCII 文本。 */
	static bool testProcessTerminalContains(
	const uint8* pData,
	size_t iSize,
	cstr sText
)
{
	size_t iTextSize = strlen(sText);

	if ( iTextSize > iSize ) {
		return false;
	}
	for ( size_t i = 0u; i <= (iSize - iTextSize); i++ ) {
		if ( memcmp(pData + i, sText, iTextSize) == 0 ) {
			return true;
		}
	}
	return false;
}
#endif



/* 初始化运行当前测试程序的终端配置。 */
static void testProcessTerminalSelf(
	xprocessconfig* pConfig,
	cstr sProgram,
	cstr sMode
)
{
	static cstr pArgs[2];

	pArgs[0] = "--xrt-process-terminal-child";
	pArgs[1] = sMode;
	testRequire(xrtProcessConfigInit(pConfig), "terminal config init failed");
	pConfig->Program = sProgram;
	pConfig->Args = pArgs;
	pConfig->ArgCount = 2u;
}



/* 验证终端能力、交互、尺寸、合流与错误契约。 */
int main(int argc, char** argv)
{
	xprocessconfig Config;
	xprocessstatus Status;
	xprocess* pProcess;
	char sOutput[4096];
	const xerror* pError;
	bool bStatus;

	if ( (argc >= 2) &&
		(strcmp(argv[1], "--xrt-process-terminal-child") == 0) ) {
		return testProcessTerminalChild(argc, argv);
	}

	testProcessTerminalSelf(&Config, argv[0], "exit");
	Config.Terminal = true;
	Config.Columns = 0u;
	testRequire(
		xrtProcessSpawn(&Config) == NULL,
		"zero terminal width was accepted"
	);
	pError = xrtGetError();
	testRequire(
		(pError != NULL) && (xrtErrorKind(pError) == XERR_RANGE) &&
		(xrtErrorCode(pError) == XPROCESS_ERROR_TERMINAL),
		"terminal dimension error mismatch"
	);

	if ( !xrtProcessTerminalSupported() ) {
		testProcessTerminalSelf(&Config, argv[0], "exit");
		Config.Terminal = true;
		testRequire(
			xrtProcessSpawn(&Config) == NULL,
			"unsupported terminal spawn succeeded"
		);
		pError = xrtGetError();
		testRequire(
			(pError != NULL) &&
			(xrtErrorKind(pError) == XERR_UNSUPPORTED),
			"unsupported terminal error mismatch"
		);
		return 0;
	}

	testProcessTerminalSelf(&Config, argv[0], "exit");
	pProcess = xrtProcessSpawn(&Config);
	testRequire(pProcess != NULL, "plain process spawn failed");
	testRequire(
		!xrtProcessResize(pProcess, 80u, 24u),
		"plain process accepted terminal resize"
	);
	testRequire(xrtProcessWait(pProcess) == XWAIT_OK, "plain process wait failed");
	xrtProcessDestroy(pProcess);

	{
		#if defined(_WIN32) || defined(_WIN64)
			static const char sInput[] = "hello\r\n";
		#else
			static const char sInput[] = "hello\n";
		#endif

		testProcessTerminalSelf(&Config, argv[0], "interact");
		Config.Terminal = true;
		Config.Stdin.Mode = XPROCESS_IO_NULL;
		Config.Stdout.Mode = XPROCESS_IO_NULL;
		Config.Stderr.Mode = XPROCESS_IO_PIPE;
		pProcess = xrtProcessSpawn(&Config);
		testRequire(pProcess != NULL, "terminal process spawn failed");
		testRequire(
			xrtProcessStreamNative(pProcess, XPROCESS_STDIN) != -1,
			"terminal stdin native handle missing"
		);
		testRequire(
			xrtProcessStreamNative(pProcess, XPROCESS_STDOUT) != -1,
			"terminal stdout native handle missing"
		);
		testRequire(
			xrtProcessStreamNative(pProcess, XPROCESS_STDERR) == -1,
			"terminal exposed a separate stderr handle"
		);
		testRequire(
			!xrtProcessResize(pProcess, 0u, 24u),
			"terminal accepted zero resize width"
		);
		testRequire(
			xrtProcessResize(pProcess, 96u, 32u),
			"terminal resize failed"
		);
		testRequire(
			testProcessTerminalWriteAll(
				pProcess,
				sInput,
				sizeof(sInput) - 1u
			),
			"terminal input write failed"
		);
		(void)testProcessTerminalReadAll(
			pProcess,
			sOutput,
			sizeof(sOutput)
		);
		testRequire(
			xrtProcessWait(pProcess) == XWAIT_OK,
			"terminal process wait failed"
		);
		bStatus = xrtProcessStatus(pProcess, &Status);
		if ( !bStatus ||
			(Status.Kind != XPROCESS_EXIT_CODE) || (Status.Code != 0) ) {
			fprintf(
				stderr,
				"[INFO] terminal status kind=%d code=%d output=%s\n",
				(int)Status.Kind,
				(int)Status.Code,
				sOutput
			);
		}
		testRequire(
			bStatus &&
			(Status.Kind == XPROCESS_EXIT_CODE) && (Status.Code == 0),
			"terminal process status mismatch"
		);
		testRequire(
			strstr(sOutput, "tty:1:1:1") != NULL,
			"terminal child did not receive TTY streams"
		);
		testRequire(
			strstr(sOutput, "terminal-stderr") != NULL,
			"terminal stderr was not merged"
		);
		testRequire(
			strstr(sOutput, "reply:hello") != NULL,
			"terminal interaction reply mismatch"
		);
		xrtProcessDestroy(pProcess);
	}

	#if defined(XRT_FEATURE_PROCESS_RUN)
		{
			#if defined(_WIN32) || defined(_WIN64)
				static const char sInput[] = "run-input\r\n";
			#else
				static const char sInput[] = "run-input\n";
			#endif
			xprocessrunoptions Options;
			xprocessresult Result;

			testProcessTerminalSelf(&Config, argv[0], "interact");
			Config.Terminal = true;
			testRequire(
				xrtProcessRunOptionsInit(&Options),
				"terminal run options init failed"
			);
			Options.Input.Data = (cbytes)sInput;
			Options.Input.Size = sizeof(sInput) - 1u;
			memset(&Result, 0, sizeof(Result));
			testRequire(
				xrtProcessRun(&Config, &Options, &Result),
				"terminal process run failed"
			);
			if ( !((Result.InputWritten == (sizeof(sInput) - 1u)) &&
				(Result.StderrSize == 0u)) ) {
				fprintf(stderr,
					"[diag] terminal run: written=%zu expect=%zu "
					"stderr=%zu\n",
					(size_t)Result.InputWritten,
					(size_t)(sizeof(sInput) - 1u),
					(size_t)Result.StderrSize);
			}
			if ( (Result.Stdout != NULL) && (Result.StdoutSize > 0u) ) {
				fprintf(stderr, "[diag] stdout(%zu): %.*s\n",
					(size_t)Result.StdoutSize,
					(int)(Result.StdoutSize > 400u ?
						400u : Result.StdoutSize),
					(const char*)Result.Stdout);
			} else {
				fprintf(stderr, "[diag] stdout empty\n");
			}
			testRequire(
				(Result.InputWritten == (sizeof(sInput) - 1u)) &&
				(Result.StderrSize == 0u) &&
				testProcessTerminalContains(
					Result.Stdout,
					Result.StdoutSize,
					"terminal-stderr"
				) &&
				testProcessTerminalContains(
					Result.Stdout,
					Result.StdoutSize,
					"reply:run-input"
				),
				"terminal process run merge mismatch"
			);
			xrtProcessResultUnit(&Result);
		}
	#endif

	return 0;
}
