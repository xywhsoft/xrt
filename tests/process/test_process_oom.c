#include "../test.h"
#include "test_process_oom_allocator.h"



/* 子进程模式立即退出，避免把父进程故障分配器带入恢复目标。 */
static int testProcessOomChild(int argc, char** argv)
{
	if ( (argc >= 2) &&
		(strcmp(argv[1], "--xrt-process-oom-child") == 0) ) {
		return 0;
	}
	return -1;
}



/* 初始化不继承标准流的当前程序配置。 */
static void testProcessOomConfig(
	xprocessconfig* pConfig,
	cstr sProgram,
	const cstr* pArgs
)
{
	testRequire(xrtProcessConfigInit(pConfig), "process OOM config init failed");
	pConfig->Program = sProgram;
	pConfig->Args = pArgs;
	pConfig->ArgCount = 1u;
	pConfig->Stdin.Mode = XPROCESS_IO_NULL;
	pConfig->Stdout.Mode = XPROCESS_IO_NULL;
	pConfig->Stderr.Mode = XPROCESS_IO_NULL;
}



/* 验证 Spawn OOM 事务性、恢复能力与重复生命周期。 */
int main(int argc, char** argv)
{
	const cstr pArgs[] = { "--xrt-process-oom-child" };
	testprocessoomallocator Allocator;
	xprocessconfig Config;
	xprocess* pProcess;

	if ( (argc >= 2) &&
		(strcmp(argv[1], "--xrt-process-oom-child") == 0) ) {
		return testProcessOomChild(argc, argv);
	}
	testRequire(
		testProcessOomInstall(&Allocator),
		"process OOM allocator install failed"
	);
	testProcessOomConfig(&Config, argv[0], pArgs);
	testProcessOomFailStore(&Allocator, true);
	testRequire(
		xrtProcessSpawn(&Config) == NULL,
		"process spawn succeeded under OOM"
	);
	testRequire(
		(testProcessOomDeniedLoad(&Allocator) != 0u) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"process spawn OOM error mismatch"
	);

	testProcessOomFailStore(&Allocator, false);
	xrtClearError();
	for ( size_t i = 0u; i < 64u; i++ ) {
		pProcess = xrtProcessSpawn(&Config);
		testRequire(pProcess != NULL, "process OOM recovery spawn failed");
		testRequire(
			xrtProcessWait(pProcess) == XWAIT_OK,
			"process OOM recovery wait failed"
		);
		xrtProcessDestroy(pProcess);
	}
	return 0;
}
