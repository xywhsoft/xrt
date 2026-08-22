#include "../test.h"
#include "test_process_oom_allocator.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <fcntl.h>
	#include <io.h>
#endif



/* 子进程提供小输出和足以触发捕获扩容的大输出。 */
static int testProcessRunOomChild(int argc, char** argv)
{
	unsigned char arrData[4096];

	if ( (argc < 3) ||
		(strcmp(argv[1], "--xrt-process-run-oom-child") != 0) ) {
		return -1;
	}
	#if defined(_WIN32) || defined(_WIN64)
		(void)_setmode(_fileno(stdout), _O_BINARY);
	#endif
	if ( strcmp(argv[2], "small") == 0 ) {
		return fwrite("run-ok", 1u, 6u, stdout) == 6u ? 0 : 91;
	}
	if ( strcmp(argv[2], "flood") == 0 ) {
		memset(arrData, 'r', sizeof(arrData));
		for ( size_t i = 0u; i < 128u; i++ ) {
			if ( fwrite(arrData, 1u, sizeof(arrData), stdout) !=
				sizeof(arrData) ) {
				return 92;
			}
			fflush(stdout);
		}
		return 0;
	}
	return 90;
}



/* 首块输出成功后关闭分配器，迫使后续 Buffer 扩容失败。 */
static bool testProcessRunOomOutput(
	xprocessstream Stream,
	xbytesview Data,
	ptr pUserData
)
{
	testprocessoomallocator* pAllocator =
		(testprocessoomallocator*)pUserData;

	(void)Stream;
	(void)Data;
	testProcessOomFailStore(pAllocator, true);
	return true;
}



/* 初始化运行当前测试程序的捕获配置。 */
static void testProcessRunOomConfig(
	xprocessconfig* pConfig,
	cstr sProgram,
	cstr sMode
)
{
	static cstr pArgs[2];

	pArgs[0] = "--xrt-process-run-oom-child";
	pArgs[1] = sMode;
	testRequire(xrtProcessConfigInit(pConfig), "process run OOM config init failed");
	pConfig->Program = sProgram;
	pConfig->Args = pArgs;
	pConfig->ArgCount = 2u;
	pConfig->Stdin.Mode = XPROCESS_IO_NULL;
	pConfig->Stderr.Mode = XPROCESS_IO_MERGE;
}



/* 验证 Run 初始化与动态捕获 OOM 都能收口，并可继续运行。 */
int main(int argc, char** argv)
{
	testprocessoomallocator Allocator;
	xprocessrunoptions Options;
	xprocessconfig Config;
	xprocessresult Result;

	if ( (argc >= 2) &&
		(strcmp(argv[1], "--xrt-process-run-oom-child") == 0) ) {
		return testProcessRunOomChild(argc, argv);
	}
	testRequire(
		testProcessOomInstall(&Allocator),
		"process run OOM allocator install failed"
	);
	testProcessRunOomConfig(&Config, argv[0], "small");
	testProcessOomFailStore(&Allocator, true);
	testRequire(
		!xrtProcessRun(&Config, NULL, &Result),
		"process run setup succeeded under OOM"
	);
	testRequire(
		(testProcessOomDeniedLoad(&Allocator) != 0u) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(Result.Stdout == NULL) && (Result.Stderr == NULL),
		"process run setup OOM mismatch"
	);
	testProcessOomFailStore(&Allocator, false);
	xrtProcessResultUnit(&Result);
	xrtClearError();

	testProcessRunOomConfig(&Config, argv[0], "flood");
	testRequire(
		xrtProcessRunOptionsInit(&Options),
		"process run OOM options init failed"
	);
	Options.Output = testProcessRunOomOutput;
	Options.UserData = &Allocator;
	testRequire(
		!xrtProcessRun(&Config, &Options, &Result),
		"process run capture succeeded under OOM"
	);
	testRequire(
		(testProcessOomDeniedLoad(&Allocator) != 0u) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(Result.Wait == XWAIT_ERROR),
		"process run capture OOM mismatch"
	);
	testProcessOomFailStore(&Allocator, false);
	xrtProcessResultUnit(&Result);
	xrtClearError();

	testProcessRunOomConfig(&Config, argv[0], "small");
	for ( size_t i = 0u; i < 16u; i++ ) {
		testRequire(
			xrtProcessRun(&Config, NULL, &Result),
			"process run did not recover after OOM"
		);
		testRequire(
			(Result.Wait == XWAIT_OK) &&
			(Result.StdoutSize == 6u) &&
			(memcmp(Result.Stdout, "run-ok", 6u) == 0),
			"process run OOM recovery result mismatch"
		);
		xrtProcessResultUnit(&Result);
	}
	return 0;
}
