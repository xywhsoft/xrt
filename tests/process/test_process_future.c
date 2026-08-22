#include "../test.h"



/* 子进程模式在可控延迟后返回指定退出码。 */
static int testProcessFutureChild(int argc, char** argv)
{
	uint64 iDelay;
	int iCode;

	if ( (argc < 5) || (strcmp(argv[1], "--xrt-process-future-child") != 0) ) {
		return -1;
	}
	iDelay = (uint64)strtoul(argv[2], NULL, 10);
	iCode = atoi(argv[3]);
	if ( strcmp(argv[4], "run") != 0 ) {
		return 90;
	}
	xrtSleep(iDelay);
	return iCode;
}



/* 启动当前测试程序的延迟退出子模式。 */
static xprocess* testProcessFutureSpawn(
	cstr sProgram,
	cstr sDelay,
	cstr sCode
)
{
	xprocessconfig Config;
	const cstr pArgs[] = {
		"--xrt-process-future-child", sDelay, sCode, "run"
	};

	testRequire(xrtProcessConfigInit(&Config), "future process config init failed");
	Config.Program = sProgram;
	Config.Args = pArgs;
	Config.ArgCount = 4u;
	Config.Stdin.Mode = XPROCESS_IO_NULL;
	Config.Stdout.Mode = XPROCESS_IO_NULL;
	Config.Stderr.Mode = XPROCESS_IO_NULL;
	return xrtProcessSpawn(&Config);
}



/* 验证共享 Future、状态所有权和提前释放生命周期。 */
int main(int argc, char** argv)
{
	const xprocessstatus* pStatus;
	xprocess* pProcess;
	xfuture* pFirst;
	xfuture* pSecond;

	if ( (argc >= 2) &&
		(strcmp(argv[1], "--xrt-process-future-child") == 0) ) {
		return testProcessFutureChild(argc, argv);
	}

	pProcess = testProcessFutureSpawn(argv[0], "50", "7");
	testRequire(pProcess != NULL, "future process spawn failed");
	pFirst = xrtProcessWaitAsync(pProcess);
	pSecond = xrtProcessWaitAsync(pProcess);
	testRequire((pFirst != NULL) && (pSecond != NULL), "process future create failed");
	testRequire(pFirst == pSecond, "process did not reuse its completion future");
	testRequire(xrtFutureCancel(pFirst), "process future cancel request failed");
	xrtProcessDestroy(pProcess);
	testRequire(
		xrtFutureWaitFor(pFirst, UINT64_C(2000000)) == XWAIT_OK,
		"process future wait failed"
	);
	testRequire(
		xrtFutureState(pFirst) == XFUTURE_RESOLVED,
		"process future did not resolve"
	);
	pStatus = (const xprocessstatus*)xrtFutureValue(pFirst);
	testRequire(
		(pStatus != NULL) &&
		(pStatus->Kind == XPROCESS_EXIT_CODE) &&
		(pStatus->Code == 7),
		"process future status mismatch"
	);
	xrtFutureDestroy(pSecond);
	xrtFutureDestroy(pFirst);

	pProcess = testProcessFutureSpawn(argv[0], "0", "3");
	testRequire(pProcess != NULL, "completed process spawn failed");
	testRequire(xrtProcessWait(pProcess) == XWAIT_OK, "completed process wait failed");
	pFirst = xrtProcessWaitAsync(pProcess);
	testRequire(pFirst != NULL, "completed process future create failed");
	testRequire(xrtFutureDone(pFirst), "completed process future remained pending");
	pStatus = (const xprocessstatus*)xrtFutureValue(pFirst);
	testRequire(
		(pStatus != NULL) && (pStatus->Code == 3),
		"completed process future status mismatch"
	);
	xrtProcessDestroy(pProcess);
	testRequire(
		((const xprocessstatus*)xrtFutureValue(pFirst))->Code == 3,
		"future status depended on process lifetime"
	);
	xrtFutureDestroy(pFirst);

	testRequire(
		xrtProcessWaitAsync(NULL) == NULL,
		"null process future unexpectedly succeeded"
	);
	testRequire(
		xrtErrorCode(xrtGetError()) == XPROCESS_ERROR_ARGUMENT,
		"null process future error mismatch"
	);
	return 0;
}
