#include "../test.h"
#include "test_process_oom_allocator.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <fcntl.h>
	#include <io.h>
#endif



/* 子进程提供 Pipeline 源和原样复制阶段。 */
static int testProcessPipelineOomChild(int argc, char** argv)
{
	unsigned char arrData[4096];
	size_t iRead;

	if ( (argc < 3) ||
		(strcmp(argv[1], "--xrt-process-pipeline-oom-child") != 0) ) {
		return -1;
	}
	#if defined(_WIN32) || defined(_WIN64)
		(void)_setmode(_fileno(stdin), _O_BINARY);
		(void)_setmode(_fileno(stdout), _O_BINARY);
	#endif
	if ( strcmp(argv[2], "small") == 0 ) {
		return fwrite("pipe-ok", 1u, 7u, stdout) == 7u ? 0 : 91;
	}
	if ( strcmp(argv[2], "flood") == 0 ) {
		memset(arrData, 'p', sizeof(arrData));
		for ( size_t i = 0u; i < 128u; i++ ) {
			if ( fwrite(arrData, 1u, sizeof(arrData), stdout) !=
				sizeof(arrData) ) {
				return 92;
			}
			fflush(stdout);
		}
		return 0;
	}
	if ( strcmp(argv[2], "copy") == 0 ) {
		while ( (iRead = fread(arrData, 1u, sizeof(arrData), stdin)) != 0u ) {
			if ( fwrite(arrData, 1u, iRead, stdout) != iRead ) {
				return 93;
			}
			fflush(stdout);
		}
		return ferror(stdin) ? 94 : 0;
	}
	return 90;
}



/* 首块末段 stdout 成功后关闭分配器。 */
static bool testProcessPipelineOomOutput(
	size_t iStage,
	xprocessstream Stream,
	xbytesview Data,
	ptr pUserData
)
{
	testprocessoomallocator* pAllocator =
		(testprocessoomallocator*)pUserData;

	(void)iStage;
	(void)Stream;
	(void)Data;
	testProcessOomFailStore(pAllocator, true);
	return true;
}



/* 初始化两段当前程序 Pipeline。 */
static void testProcessPipelineOomStages(
	xprocessconfig* pStages,
	cstr sProgram,
	cstr sSource
)
{
	static cstr pSourceArgs[2];
	static const cstr pCopyArgs[] = {
		"--xrt-process-pipeline-oom-child", "copy"
	};

	pSourceArgs[0] = "--xrt-process-pipeline-oom-child";
	pSourceArgs[1] = sSource;
	for ( size_t i = 0u; i < 2u; i++ ) {
		testRequire(
			xrtProcessConfigInit(&pStages[i]),
			"process pipeline OOM stage init failed"
		);
		pStages[i].Program = sProgram;
		pStages[i].Stderr.Mode = XPROCESS_IO_MERGE;
	}
	pStages[0].Args = pSourceArgs;
	pStages[0].ArgCount = 2u;
	pStages[1].Args = pCopyArgs;
	pStages[1].ArgCount = 2u;
}



/* 验证 Pipeline OOM 不遗留阶段，并可重复恢复执行。 */
int main(int argc, char** argv)
{
	testprocessoomallocator Allocator;
	xprocesspipelineoptions Options;
	xprocesspipelineresult Result;
	xprocessconfig Stages[2];

	if ( (argc >= 2) &&
		(strcmp(argv[1], "--xrt-process-pipeline-oom-child") == 0) ) {
		return testProcessPipelineOomChild(argc, argv);
	}
	testRequire(
		testProcessOomInstall(&Allocator),
		"process pipeline OOM allocator install failed"
	);
	testProcessPipelineOomStages(Stages, argv[0], "small");
	testProcessOomFailStore(&Allocator, true);
	testRequire(
		!xrtProcessPipeline(Stages, 2u, NULL, &Result),
		"process pipeline setup succeeded under OOM"
	);
	testRequire(
		(testProcessOomDeniedLoad(&Allocator) != 0u) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(Result.Stages == NULL) && (Result.Stdout == NULL),
		"process pipeline setup OOM mismatch"
	);
	testProcessOomFailStore(&Allocator, false);
	xrtProcessPipelineResultUnit(&Result);
	xrtClearError();

	testProcessPipelineOomStages(Stages, argv[0], "flood");
	testRequire(
		xrtProcessPipelineOptionsInit(&Options),
		"process pipeline OOM options init failed"
	);
	Options.Output = testProcessPipelineOomOutput;
	Options.UserData = &Allocator;
	testRequire(
		!xrtProcessPipeline(Stages, 2u, &Options, &Result),
		"process pipeline capture succeeded under OOM"
	);
	testRequire(
		(testProcessOomDeniedLoad(&Allocator) != 0u) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(Result.Wait == XWAIT_ERROR),
		"process pipeline capture OOM mismatch"
	);
	testProcessOomFailStore(&Allocator, false);
	xrtProcessPipelineResultUnit(&Result);
	xrtClearError();

	testProcessPipelineOomStages(Stages, argv[0], "small");
	for ( size_t i = 0u; i < 8u; i++ ) {
		testRequire(
			xrtProcessPipeline(Stages, 2u, NULL, &Result),
			"process pipeline did not recover after OOM"
		);
		testRequire(
			(Result.Wait == XWAIT_OK) &&
			(Result.StageCount == 2u) &&
			(Result.StdoutSize == 7u) &&
			(memcmp(Result.Stdout, "pipe-ok", 7u) == 0),
			"process pipeline OOM recovery result mismatch"
		);
		xrtProcessPipelineResultUnit(&Result);
	}
	return 0;
}
