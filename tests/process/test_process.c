#include "../test.h"

#include <errno.h>

#if defined(_WIN32) || defined(_WIN64)
	#include <fcntl.h>
	#include <io.h>
#else
	#include <fcntl.h>
	#include <unistd.h>
#endif



/* 子进程模式执行测试需要的确定性小操作。 */
static int testProcessChild(int argc, char** argv)
{
	#if defined(_WIN32) || defined(_WIN64)
		(void)_setmode(_fileno(stdin), _O_BINARY);
		(void)_setmode(_fileno(stdout), _O_BINARY);
		(void)_setmode(_fileno(stderr), _O_BINARY);
	#endif
	if ( (argc < 3) || (strcmp(argv[1], "--xrt-process-child") != 0) ) {
		return -1;
	}
	if ( strcmp(argv[2], "exit") == 0 ) {
		return argc >= 4 ? atoi(argv[3]) : 0;
	}
	if ( strcmp(argv[2], "echo") == 0 ) {
		char pData[128];
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
	if ( strcmp(argv[2], "args") == 0 ) {
		for ( int i = 3; i < argc; i++ ) {
			printf("%u:", (unsigned)strlen(argv[i]));
			fwrite(argv[i], 1u, strlen(argv[i]), stdout);
			fputc('\n', stdout);
		}
		fflush(stdout);
		return 0;
	}
	if ( strcmp(argv[2], "env") == 0 ) {
		const char* sValue = argc >= 4 ? getenv(argv[3]) : NULL;

		if ( sValue != NULL ) {
			fputs(sValue, stdout);
			fflush(stdout);
		}
		return 0;
	}
	if ( strcmp(argv[2], "sleep") == 0 ) {
		uint64 iMilliseconds = argc >= 4 ?
			(uint64)strtoul(argv[3], NULL, 10) : 100u;

		xrtSleep(iMilliseconds);
		return 0;
	}
	if ( strcmp(argv[2], "both") == 0 ) {
		fputs("out", stdout);
		fflush(stdout);
		fputs("err", stderr);
		fflush(stderr);
		return 0;
	}
	return 90;
}



/* 循环处理平台允许的部分写入。 */
static bool testProcessWriteAll(
	xprocess* pProcess,
	const void* pData,
	size_t iSize
)
{
	const unsigned char* pBytes = (const unsigned char*)pData;
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



/* 读取一个小型测试流直到 EOF。 */
static size_t testProcessReadAll(
	xprocess* pProcess,
	xprocessstream Stream,
	char* pOutput,
	size_t iCapacity
)
{
	size_t iSize = 0u;

	while ( iSize < (iCapacity - 1u) ) {
		int64 iRead = xrtProcessRead(
			pProcess,
			Stream,
			pOutput + iSize,
			iCapacity - iSize - 1u
		);

		testRequire(iRead >= 0, "process output read failed");
		if ( iRead == 0 ) {
			break;
		}
		iSize += (size_t)iRead;
	}
	pOutput[iSize] = 0;
	return iSize;
}



/* 初始化运行当前测试程序的直接执行配置。 */
static void testProcessSelf(
	xprocessconfig* pConfig,
	cstr sProgram,
	const cstr* pArgs,
	size_t iArgCount
)
{
	testRequire(xrtProcessConfigInit(pConfig), "process config init failed");
	pConfig->Program = sProgram;
	pConfig->Args = pArgs;
	pConfig->ArgCount = iArgCount;
}



#if !defined(_WIN32) && !defined(_WIN64)
/* 保存一个标准流，以便关闭标准流边界测试结束后恢复测试进程。 */
static int testProcessStandardSave(int iFd)
{
	int iSaved = fcntl(iFd, F_DUPFD, STDERR_FILENO + 1);
	int iFlags;

	if ( iSaved < 0 ) {
		return -1;
	}
	iFlags = fcntl(iSaved, F_GETFD);
	if ( (iFlags < 0) ||
		(fcntl(iSaved, F_SETFD, iFlags | FD_CLOEXEC) != 0) ) {
		(void)close(iSaved);
		return -1;
	}
	return iSaved;
}



/* 验证内部管道占用 0/1/2 时仍不会破坏错误通道和重定向。 */
static void testProcessClosedStandardHandles(cstr sProgram)
{
	const cstr pArgs[] = {
		"--xrt-process-child", "exit", "0"
	};
	xprocessconfig Config;
	xprocessstatus Status;
	xprocess* pProcess;
	int pSaved[3];
	bool bRestored = true;

	for ( int i = 0; i < 3; i++ ) {
		pSaved[i] = testProcessStandardSave(i);
		testRequire(pSaved[i] >= 0, "standard stream save failed");
	}
	for ( int i = 0; i < 3; i++ ) {
		(void)close(i);
	}
	testProcessSelf(&Config, sProgram, pArgs, 3u);
	Config.Stdin.Mode = XPROCESS_IO_NULL;
	Config.Stdout.Mode = XPROCESS_IO_NULL;
	Config.Stderr.Mode = XPROCESS_IO_NULL;
	pProcess = xrtProcessSpawn(&Config);
	for ( int i = 0; i < 3; i++ ) {
		if ( dup2(pSaved[i], i) != i ) {
			bRestored = false;
		}
		(void)close(pSaved[i]);
	}
	testRequire(bRestored, "standard stream restore failed");
	testRequire(pProcess != NULL, "closed standard stream spawn failed");
	testRequire(
		xrtProcessWait(pProcess) == XWAIT_OK,
		"closed standard stream wait failed"
	);
	testRequire(
		xrtProcessStatus(pProcess, &Status) &&
		(Status.Kind == XPROCESS_EXIT_CODE) && (Status.Code == 0),
		"closed standard stream exit mismatch"
	);
	xrtProcessDestroy(pProcess);
}
#endif



/* 验证 Process 核心的启动、管道、状态、环境和停止契约。 */
int main(int argc, char** argv)
{
	xprocessconfig Config;
	xprocessstatus Status;
	xprocess* pProcess;
	xprocess* pRef;
	char pStdout[512];
	char pStderr[128];
	const xerror* pError;

	if ( (argc >= 2) &&
		(strcmp(argv[1], "--xrt-process-child") == 0) ) {
		return testProcessChild(argc, argv);
	}

	{
		const cstr pArgs[] = {
			"--xrt-process-child", "exit", "7"
		};

		testProcessSelf(&Config, argv[0], pArgs, 3u);
		pProcess = xrtProcessSpawn(&Config);
		testRequire(pProcess != NULL, "direct process spawn failed");
		testRequire(xrtProcessId(pProcess) != 0u, "process id is zero");
		testRequire(xrtProcessNative(pProcess) != -1, "process native handle missing");
		testRequire(
			xrtProcessWaitFor(pProcess, 0u) == XWAIT_TIMEOUT,
			"zero process wait did not time out"
		);
		pRef = xrtProcessRef(pProcess);
		testRequire(pRef == pProcess, "process ref failed");
		testRequire(xrtProcessWait(pProcess) == XWAIT_OK, "process wait failed");
		testRequire(xrtProcessWait(pProcess) == XWAIT_OK, "repeat process wait failed");
		testRequire(
			xrtProcessState(pProcess) == XPROCESS_EXITED,
			"process state did not become exited"
		);
		testRequire(xrtProcessStatus(pProcess, &Status), "process status failed");
		testRequire(
			(Status.Kind == XPROCESS_EXIT_CODE) && (Status.Code == 7),
			"process exit code mismatch"
		);
		xrtProcessDestroy(pRef);
		xrtProcessDestroy(pProcess);
	}

	{
		static const char pInput[] = "alpha\nbeta\n";
		const cstr pArgs[] = {
			"--xrt-process-child", "echo"
		};

		testProcessSelf(&Config, argv[0], pArgs, 2u);
		Config.Stdin.Mode = XPROCESS_IO_PIPE;
		Config.Stdout.Mode = XPROCESS_IO_PIPE;
		Config.Stderr.Mode = XPROCESS_IO_PIPE;
		pProcess = xrtProcessSpawn(&Config);
		testRequire(pProcess != NULL, "pipe process spawn failed");
		testRequire(
			xrtProcessStreamNative(pProcess, XPROCESS_STDIN) != -1,
			"stdin native pipe missing"
		);
		testRequire(
			testProcessWriteAll(pProcess, pInput, sizeof(pInput) - 1u),
			"process input write failed"
		);
		testRequire(
			xrtProcessClose(pProcess, XPROCESS_STDIN),
			"process stdin close failed"
		);
		testRequire(
			xrtProcessClose(pProcess, XPROCESS_STDIN),
			"repeated process stdin close failed"
		);
		testRequire(
			testProcessReadAll(
				pProcess,
				XPROCESS_STDOUT,
				pStdout,
				sizeof(pStdout)
			) == (sizeof(pInput) - 1u),
			"process stdout size mismatch"
		);
		(void)testProcessReadAll(
			pProcess,
			XPROCESS_STDERR,
			pStderr,
			sizeof(pStderr)
		);
		testRequire(strcmp(pStdout, pInput) == 0, "process stdout mismatch");
		testRequire(
			strcmp(pStderr, "child-stderr") == 0,
			"process stderr mismatch"
		);
		testRequire(xrtProcessWait(pProcess) == XWAIT_OK, "pipe process wait failed");
		xrtProcessDestroy(pProcess);
	}

	{
		const cstr pArgs[] = {
			"--xrt-process-child",
			"args",
			"",
			"alpha beta",
			"quote\"slash\\"
		};

		testProcessSelf(&Config, argv[0], pArgs, 5u);
		Config.Stdout.Mode = XPROCESS_IO_PIPE;
		pProcess = xrtProcessSpawn(&Config);
		testRequire(pProcess != NULL, "quoted process spawn failed");
		(void)testProcessReadAll(
			pProcess,
			XPROCESS_STDOUT,
			pStdout,
			sizeof(pStdout)
		);
		testRequire(xrtProcessWait(pProcess) == XWAIT_OK, "quoted process wait failed");
		testRequire(
			strcmp(
				pStdout,
				"0:\n10:alpha beta\n12:quote\"slash\\\n"
			) == 0,
			"process argument quoting mismatch"
		);
		xrtProcessDestroy(pProcess);
	}

	{
		const cstr pArgs[] = {
			"--xrt-process-child", "env", "XRT_PROCESS_TEST"
		};
		const xprocessenv pEnv[] = {
			{ "XRT_PROCESS_TEST", "first" },
			{ "XRT_PROCESS_TEST", "value" }
		};

		testProcessSelf(&Config, argv[0], pArgs, 3u);
		Config.Env = pEnv;
		Config.EnvCount = 2u;
		Config.Stdout.Mode = XPROCESS_IO_PIPE;
		pProcess = xrtProcessSpawn(&Config);
		testRequire(pProcess != NULL, "environment process spawn failed");
		(void)testProcessReadAll(
			pProcess,
			XPROCESS_STDOUT,
			pStdout,
			sizeof(pStdout)
		);
		testRequire(xrtProcessWait(pProcess) == XWAIT_OK, "environment process wait failed");
		testRequire(strcmp(pStdout, "value") == 0, "environment override mismatch");
		xrtProcessDestroy(pProcess);
	}

	{
		const cstr pArgs[] = {
			"--xrt-process-child", "both"
		};

		testProcessSelf(&Config, argv[0], pArgs, 2u);
		Config.Stdout.Mode = XPROCESS_IO_PIPE;
		Config.Stderr.Mode = XPROCESS_IO_MERGE;
		pProcess = xrtProcessSpawn(&Config);
		testRequire(pProcess != NULL, "merged process spawn failed");
		(void)testProcessReadAll(
			pProcess,
			XPROCESS_STDOUT,
			pStdout,
			sizeof(pStdout)
		);
		testRequire(xrtProcessWait(pProcess) == XWAIT_OK, "merged process wait failed");
		testRequire(strcmp(pStdout, "outerr") == 0, "merged output mismatch");
		testRequire(
			xrtProcessStreamNative(pProcess, XPROCESS_STDERR) == -1,
			"merged stderr exposed a second pipe"
		);
		xrtProcessDestroy(pProcess);
	}

	{
		const cstr pArgs[] = {
			"--xrt-process-child", "sleep", "5000"
		};

		testProcessSelf(&Config, argv[0], pArgs, 3u);
		Config.Stdin.Mode = XPROCESS_IO_NULL;
		Config.Stdout.Mode = XPROCESS_IO_NULL;
		Config.Stderr.Mode = XPROCESS_IO_NULL;
		pProcess = xrtProcessSpawn(&Config);
		testRequire(pProcess != NULL, "kill process spawn failed");
		testRequire(
			xrtProcessWaitFor(pProcess, 10000u) == XWAIT_TIMEOUT,
			"long process did not time out"
		);
		testRequire(xrtProcessKillTree(pProcess), "process tree kill failed");
		testRequire(xrtProcessWait(pProcess) == XWAIT_OK, "killed process wait failed");
		testRequire(xrtProcessStatus(pProcess, &Status), "killed process status failed");
		testRequire(
			Status.Stop == XPROCESS_STOP_KILL_TREE,
			"killed process stop reason mismatch"
		);
		xrtProcessDestroy(pProcess);
	}

	{
		testRequire(xrtProcessConfigInit(&Config), "invalid config init failed");
		Config.Program = "__xrt_process_missing_program_7f6a__";
		testRequire(xrtProcessSpawn(&Config) == NULL, "missing process unexpectedly spawned");
		pError = xrtGetError();
		testRequire(pError != NULL, "missing process error absent");
		testRequire(
			strcmp(xrtErrorDomain(pError), "xrt.process") == 0,
			"missing process error domain mismatch"
		);
		xrtClearError();
	}

	{
		const cstr pArgs[] = {
			"--xrt-process-child", "sleep", "20"
		};

		testProcessSelf(&Config, argv[0], pArgs, 3u);
		Config.Stdin.Mode = XPROCESS_IO_NULL;
		Config.Stdout.Mode = XPROCESS_IO_NULL;
		Config.Stderr.Mode = XPROCESS_IO_NULL;
		pProcess = xrtProcessSpawn(&Config);
		testRequire(pProcess != NULL, "detached process spawn failed");
		xrtProcessDestroy(pProcess);
		xrtSleep(50u);
	}

	#if !defined(_WIN32) && !defined(_WIN64)
		testProcessClosedStandardHandles(argv[0]);
	#endif

	printf("[PASS] process\n");
	return 0;
}
