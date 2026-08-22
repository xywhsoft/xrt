#include "../test.h"
#include "test_process_oom_allocator.h"



/* 子进程短暂等待，给父进程留下尺寸类耗尽窗口。 */
static int testProcessFutureOomChild(int argc, char** argv)
{
	if ( (argc >= 2) &&
		(strcmp(argv[1], "--xrt-process-future-oom-child") == 0) ) {
		xrtSleep(200u);
		return 5;
	}
	return -1;
}



/* 启动当前程序的延迟退出子进程。 */
static xprocess* testProcessFutureOomSpawn(cstr sProgram)
{
	const cstr pArgs[] = { "--xrt-process-future-oom-child" };
	xprocessconfig Config;

	testRequire(
		xrtProcessConfigInit(&Config),
		"process future OOM config init failed"
	);
	Config.Program = sProgram;
	Config.Args = pArgs;
	Config.ArgCount = 1u;
	Config.Stdin.Mode = XPROCESS_IO_NULL;
	Config.Stdout.Mode = XPROCESS_IO_NULL;
	Config.Stderr.Mode = XPROCESS_IO_NULL;
	return xrtProcessSpawn(&Config);
}



/* 验证 WaitAsync 安装 OOM 不缓存半成品，并能恢复为唯一 Future。 */
int main(int argc, char** argv)
{
	testprocessoomallocator Allocator;
	ptr pHeld[4096];
	size_t iHeld = 0u;
	xprocess* pProcess;
	xfuture* pFuture;
	const xprocessstatus* pStatus;

	if ( (argc >= 2) &&
		(strcmp(argv[1], "--xrt-process-future-oom-child") == 0) ) {
		return testProcessFutureOomChild(argc, argv);
	}
	testRequire(
		testProcessOomInstall(&Allocator),
		"process future OOM allocator install failed"
	);
	pProcess = testProcessFutureOomSpawn(argv[0]);
	testRequire(pProcess != NULL, "process future OOM spawn failed");

	testProcessOomFailStore(&Allocator, true);
	while ( iHeld < (sizeof(pHeld) / sizeof(pHeld[0])) ) {
		ptr pMemory = xrtMalloc(sizeof(xprocessstatus));

		if ( pMemory == NULL ) {
			break;
		}
		pHeld[iHeld++] = pMemory;
	}
	testRequire(
		(iHeld != (sizeof(pHeld) / sizeof(pHeld[0]))) &&
		(testProcessOomDeniedLoad(&Allocator) != 0u),
		"process future status size class was not exhausted"
	);
	xrtClearError();
	testRequire(
		xrtProcessWaitAsync(pProcess) == NULL,
		"process wait Future succeeded under OOM"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"process wait Future OOM error mismatch"
	);

	testProcessOomFailStore(&Allocator, false);
	for ( size_t i = 0u; i < iHeld; i++ ) {
		xrtFree(pHeld[i]);
	}
	xrtClearError();
	pFuture = xrtProcessWaitAsync(pProcess);
	testRequire(pFuture != NULL, "process wait Future did not recover after OOM");
	for ( size_t i = 0u; i < 64u; i++ ) {
		xfuture* pShared = xrtProcessWaitAsync(pProcess);

		testRequire(pShared == pFuture, "process wait Future cache changed");
		xrtFutureDestroy(pShared);
	}
	testRequire(
		xrtFutureWaitFor(pFuture, UINT64_C(2000000)) == XWAIT_OK,
		"process wait Future OOM recovery wait failed"
	);
	pStatus = (const xprocessstatus*)xrtFutureValue(pFuture);
	testRequire(
		(pStatus != NULL) &&
		(pStatus->Kind == XPROCESS_EXIT_CODE) && (pStatus->Code == 5),
		"process wait Future OOM recovery status mismatch"
	);
	xrtFutureDestroy(pFuture);
	xrtProcessDestroy(pProcess);
	return 0;
}
