#include "../test.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <fcntl.h>
	#include <io.h>
#endif



/* 双流回调状态在两个输出泵之间共享，因此由互斥锁保护。 */
typedef struct testprocessoutput {
	xmutex Lock;
	size_t StdoutSize;
	size_t StderrSize;
	size_t Calls;
	bool Valid;
} testprocessoutput;



/* 延迟取消状态借用令牌直到测试线程结束。 */
typedef struct testprocesscancel {
	xcancel* Cancel;
	uint64 Delay;
} testprocesscancel;



/* 子进程模式提供确定性输入、输出、等待与退出行为。 */
static int testProcessRunChild(int argc, char** argv)
{
	#if defined(_WIN32) || defined(_WIN64)
		(void)_setmode(_fileno(stdin), _O_BINARY);
		(void)_setmode(_fileno(stdout), _O_BINARY);
		(void)_setmode(_fileno(stderr), _O_BINARY);
	#endif
	if ( (argc < 3) || (strcmp(argv[1], "--xrt-process-run-child") != 0) ) {
		return -1;
	}
	if ( strcmp(argv[2], "echo") == 0 ) {
		unsigned char pData[4096];
		size_t iRead;

		while ( (iRead = fread(pData, 1u, sizeof(pData), stdin)) != 0u ) {
			if ( fwrite(pData, 1u, iRead, stdout) != iRead ) {
				return 91;
			}
		}
		fputs("child-stderr", stderr);
		fflush(stdout);
		fflush(stderr);
		return ferror(stdin) ? 92 : 0;
	}
	if ( strcmp(argv[2], "sequence") == 0 ) {
		fputs("0123456789", stdout);
		fflush(stdout);
		return 0;
	}
	if ( strcmp(argv[2], "flood") == 0 ) {
		unsigned char pStdout[1024];
		unsigned char pStderr[1024];

		memset(pStdout, 'A', sizeof(pStdout));
		memset(pStderr, 'B', sizeof(pStderr));
		for ( size_t i = 0u; i < 256u; i++ ) {
			if ( fwrite(pStdout, 1u, sizeof(pStdout), stdout) != sizeof(pStdout) ) {
				return 93;
			}
		}
		fflush(stdout);
		for ( size_t i = 0u; i < 256u; i++ ) {
			if ( fwrite(pStderr, 1u, sizeof(pStderr), stderr) != sizeof(pStderr) ) {
				return 94;
			}
		}
		fflush(stderr);
		return 0;
	}
	if ( strcmp(argv[2], "sleep") == 0 ) {
		xrtSleep(5000u);
		return 0;
	}
	return 90;
}



/* 初始化运行当前测试程序的直接执行配置。 */
static void testProcessRunSelf(
	xprocessconfig* pConfig,
	cstr sProgram,
	cstr sMode
)
{
	static cstr pArgs[2];

	pArgs[0] = "--xrt-process-run-child";
	pArgs[1] = sMode;
	testRequire(xrtProcessConfigInit(pConfig), "process run config init failed");
	pConfig->Program = sProgram;
	pConfig->Args = pArgs;
	pConfig->ArgCount = 2u;
}



/* 验证捕获内容只包含指定重复字节。 */
static bool testProcessRunBytes(
	cbytes pData,
	size_t iSize,
	uint8 iExpected
)
{
	for ( size_t i = 0u; i < iSize; i++ ) {
		if ( pData[i] != iExpected ) {
			return false;
		}
	}
	return true;
}



/* 统计并验证 stdout 与 stderr 的流式回调。 */
static bool testProcessRunOutput(
	xprocessstream Stream,
	xbytesview Data,
	ptr pUserData
)
{
	testprocessoutput* pOutput = (testprocessoutput*)pUserData;
	uint8 iExpected = Stream == XPROCESS_STDOUT ? 'A' : 'B';

	(void)xrtMutexLock(&pOutput->Lock);
	pOutput->Valid = pOutput->Valid &&
		testProcessRunBytes(Data.Data, Data.Size, iExpected);
	if ( Stream == XPROCESS_STDOUT ) {
		pOutput->StdoutSize += Data.Size;
	} else {
		pOutput->StderrSize += Data.Size;
	}
	pOutput->Calls++;
	(void)xrtMutexUnlock(&pOutput->Lock);
	return true;
}



/* 拒绝首个输出块以验证回调失败的结构化错误。 */
static bool testProcessRunReject(
	xprocessstream Stream,
	xbytesview Data,
	ptr pUserData
)
{
	(void)Stream;
	(void)Data;
	(void)pUserData;
	return false;
}



/* 在线程中延迟请求外部取消。 */
static int32 testProcessRunCancel(ptr pData)
{
	testprocesscancel* pCancel = (testprocesscancel*)pData;

	xrtSleep(pCancel->Delay);
	return xrtCancelRequest(pCancel->Cancel) ? 0 : 1;
}



/* 运行 sequence 子模式并应用指定捕获策略。 */
static bool testProcessRunSequence(
	cstr sProgram,
	xprocessoverflow Overflow,
	xprocessresult* pResult
)
{
	xprocessrunoptions Options;
	xprocessconfig Config;

	testProcessRunSelf(&Config, sProgram, "sequence");
	testRequire(xrtProcessRunOptionsInit(&Options), "sequence options init failed");
	Options.StdoutLimit = 4u;
	Options.Overflow = Overflow;
	return xrtProcessRun(&Config, &Options, pResult);
}



/* 验证 Process Run 的并发排空、有界捕获和控制流契约。 */
int main(int argc, char** argv)
{
	xprocessrunoptions Options;
	xprocessresult Result;
	xprocessconfig Config;
	testprocessoutput Output;
	testprocesscancel CancelState;
	xprocessstatus Status;
	xprocess* pProcess;
	xcancel* pCancel;
	xthread* pThread;
	const xerror* pError;
	static const unsigned char pInput[] = "alpha\0beta\r\n";
	const size_t iFloodSize = 256u * 1024u;

	if ( (argc >= 2) &&
		(strcmp(argv[1], "--xrt-process-run-child") == 0) ) {
		return testProcessRunChild(argc, argv);
	}

	testProcessRunSelf(&Config, argv[0], "echo");
	testRequire(xrtProcessRunOptionsInit(&Options), "run options init failed");
	Options.Input.Data = pInput;
	Options.Input.Size = sizeof(pInput) - 1u;
	testRequire(xrtProcessRun(&Config, &Options, &Result), "input run failed");
	testRequire(xrtProcessResultSuccess(&Result), "input run did not succeed");
	testRequire(Result.InputWritten == Options.Input.Size, "input write size mismatch");
	testRequire(Result.StdoutSize == Options.Input.Size, "echo output size mismatch");
	testRequire(
		memcmp(Result.Stdout, pInput, Options.Input.Size) == 0,
		"echo output mismatch"
	);
	testRequire(
		(Result.StderrSize == 12u) &&
		(memcmp(Result.Stderr, "child-stderr", 12u) == 0),
		"echo stderr mismatch"
	);
	xrtProcessResultUnit(&Result);

	testRequire(xrtMutexInit(&Output.Lock), "output callback mutex init failed");
	Output.StdoutSize = 0u;
	Output.StderrSize = 0u;
	Output.Calls = 0u;
	Output.Valid = true;
	testProcessRunSelf(&Config, argv[0], "flood");
	testRequire(xrtProcessRunOptionsInit(&Options), "flood options init failed");
	Options.Output = testProcessRunOutput;
	Options.UserData = &Output;
	testRequire(xrtProcessRun(&Config, &Options, &Result), "flood run failed");
	testRequire(xrtProcessResultSuccess(&Result), "flood run did not succeed");
	testRequire(
		(Result.StdoutSize == iFloodSize) &&
		(Result.StderrSize == iFloodSize),
		"flood capture size mismatch"
	);
	testRequire(
		testProcessRunBytes(Result.Stdout, Result.StdoutSize, 'A') &&
		testProcessRunBytes(Result.Stderr, Result.StderrSize, 'B'),
		"flood capture bytes mismatch"
	);
	testRequire(
		Output.Valid && (Output.StdoutSize == iFloodSize) &&
		(Output.StderrSize == iFloodSize) && (Output.Calls != 0u),
		"flood callback mismatch"
	);
	xrtProcessResultUnit(&Result);
	testRequire(xrtMutexUnit(&Output.Lock), "output callback mutex unit failed");

	testRequire(
		testProcessRunSequence(
			argv[0],
			XPROCESS_OVERFLOW_KEEP_FIRST,
			&Result
		),
		"keep-first run failed"
	);
	testRequire(
		(Result.StdoutSize == 4u) &&
		(memcmp(Result.Stdout, "0123", 4u) == 0) &&
		Result.StdoutTruncated,
		"keep-first result mismatch"
	);
	xrtProcessResultUnit(&Result);

	testRequire(
		testProcessRunSequence(
			argv[0],
			XPROCESS_OVERFLOW_KEEP_LAST,
			&Result
		),
		"keep-last run failed"
	);
	testRequire(
		(Result.StdoutSize == 4u) &&
		(memcmp(Result.Stdout, "6789", 4u) == 0) &&
		Result.StdoutTruncated,
		"keep-last result mismatch"
	);
	xrtProcessResultUnit(&Result);

	testRequire(
		!testProcessRunSequence(
			argv[0],
			XPROCESS_OVERFLOW_ERROR,
			&Result
		),
		"overflow error run unexpectedly succeeded"
	);
	pError = xrtGetError();
	testRequire(
		(pError != NULL) &&
		(strcmp(xrtErrorDomain(pError), "xrt.process") == 0) &&
		(xrtErrorCode(pError) == XPROCESS_ERROR_LIMIT),
		"overflow error mismatch"
	);
	xrtProcessResultUnit(&Result);

	testProcessRunSelf(&Config, argv[0], "sequence");
	testRequire(xrtProcessRunOptionsInit(&Options), "reject options init failed");
	Options.Output = testProcessRunReject;
	testRequire(
		!xrtProcessRun(&Config, &Options, &Result),
		"rejected callback run unexpectedly succeeded"
	);
	testRequire(
		xrtErrorCode(xrtGetError()) == XPROCESS_ERROR_CALLBACK,
		"callback error mismatch"
	);
	xrtProcessResultUnit(&Result);

	testProcessRunSelf(&Config, argv[0], "sleep");
	testRequire(xrtProcessRunOptionsInit(&Options), "deadline options init failed");
	Options.Deadline = xrtDeadlineAfter(20000u);
	Options.StopGrace = 10000u;
	testRequire(xrtProcessRun(&Config, &Options, &Result), "deadline run failed");
	testRequire(Result.Wait == XWAIT_TIMEOUT, "deadline result mismatch");
	testRequire(Result.Status.Stop != XPROCESS_STOP_NONE, "deadline stop missing");
	xrtProcessResultUnit(&Result);

	pCancel = xrtCancelCreate();
	testRequire(pCancel != NULL, "run cancel create failed");
	CancelState.Cancel = pCancel;
	CancelState.Delay = 20u;
	pThread = xrtThreadCreate(testProcessRunCancel, &CancelState, 0u);
	testRequire(pThread != NULL, "run cancel thread create failed");
	testProcessRunSelf(&Config, argv[0], "sleep");
	testRequire(xrtProcessRunOptionsInit(&Options), "cancel options init failed");
	Options.Cancel = pCancel;
	Options.StopGrace = 10000u;
	testRequire(xrtProcessRun(&Config, &Options, &Result), "cancel run failed");
	testRequire(Result.Wait == XWAIT_CANCELLED, "cancel result mismatch");
	testRequire(xrtThreadWait(pThread) == XWAIT_OK, "cancel thread wait failed");
	xrtThreadDestroy(pThread);
	xrtCancelDestroy(pCancel);
	xrtProcessResultUnit(&Result);

	pCancel = xrtCancelCreate();
	testRequire(pCancel != NULL, "wait cancel create failed");
	CancelState.Cancel = pCancel;
	CancelState.Delay = 20u;
	pThread = xrtThreadCreate(testProcessRunCancel, &CancelState, 0u);
	testRequire(pThread != NULL, "wait cancel thread create failed");
	testProcessRunSelf(&Config, argv[0], "sleep");
	Config.Stdin.Mode = XPROCESS_IO_NULL;
	Config.Stdout.Mode = XPROCESS_IO_NULL;
	Config.Stderr.Mode = XPROCESS_IO_NULL;
	pProcess = xrtProcessSpawn(&Config);
	testRequire(pProcess != NULL, "wait cancel spawn failed");
	testRequire(
		xrtProcessWaitUntilCancel(
			pProcess,
			XRT_DEADLINE_NEVER,
			pCancel
		) == XWAIT_CANCELLED,
		"wait cancel result mismatch"
	);
	testRequire(xrtProcessKillTree(pProcess), "wait cancel kill failed");
	testRequire(xrtProcessWait(pProcess) == XWAIT_OK, "wait cancel reap failed");
	testRequire(xrtProcessStatus(pProcess, &Status), "wait cancel status failed");
	testRequire(Status.Stop == XPROCESS_STOP_KILL_TREE, "wait cancel stop mismatch");
	xrtProcessDestroy(pProcess);
	testRequire(xrtThreadWait(pThread) == XWAIT_OK, "wait cancel thread failed");
	xrtThreadDestroy(pThread);
	xrtCancelDestroy(pCancel);

	{
		const cstr pArgs[] = {
			"--xrt-process-run-child", "sequence"
		};

		testRequire(
			xrtProcessCapture(argv[0], pArgs, 2u, &Result),
			"capture helper failed"
		);
		testRequire(xrtProcessResultSuccess(&Result), "capture helper result failed");
		testRequire(
			(Result.StdoutSize == 10u) &&
			(memcmp(Result.Stdout, "0123456789", 10u) == 0),
			"capture helper output mismatch"
		);
		xrtProcessResultUnit(&Result);
	}

	#if defined(_WIN32) || defined(_WIN64)
		testRequire(xrtProcessShell("echo shell", &Result), "shell helper failed");
	#else
		testRequire(xrtProcessShell("printf shell", &Result), "shell helper failed");
	#endif
	testRequire(xrtProcessResultSuccess(&Result), "shell helper result failed");
	testRequire(
		(Result.StdoutSize >= 5u) &&
		(memcmp(Result.Stdout, "shell", 5u) == 0),
		"shell helper output mismatch"
	);
	xrtProcessResultUnit(&Result);
	return 0;
}
