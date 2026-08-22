#include "../test.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <fcntl.h>
	#include <io.h>
#endif



/* Pipeline 回调状态由全部输出泵并发更新。 */
typedef struct testpipelineoutput {
	xmutex Lock;
	size_t Sizes[3][3];
	size_t Calls;
	bool Valid;
} testpipelineoutput;



/* 延迟取消线程借用外部令牌。 */
typedef struct testpipelinecancel {
	xcancel* Cancel;
	uint64 Delay;
} testpipelinecancel;



/* 把 stdin 原样复制到 stdout。 */
static int testPipelineChildCopy(cstr sStderr)
{
	unsigned char pData[4096];
	size_t iRead;

	while ( (iRead = fread(pData, 1u, sizeof(pData), stdin)) != 0u ) {
		if ( fwrite(pData, 1u, iRead, stdout) != iRead ) {
			return 91;
		}
	}
	if ( sStderr != NULL ) {
		fputs(sStderr, stderr);
	}
	fflush(stdout);
	fflush(stderr);
	return ferror(stdin) ? 92 : 0;
}



/* 子进程模式提供管道容量以上的数据和确定性状态。 */
static int testPipelineChild(int argc, char** argv)
{
	#if defined(_WIN32) || defined(_WIN64)
		(void)_setmode(_fileno(stdin), _O_BINARY);
		(void)_setmode(_fileno(stdout), _O_BINARY);
		(void)_setmode(_fileno(stderr), _O_BINARY);
	#endif
	if ( (argc < 3) || (strcmp(argv[1], "--xrt-pipeline-child") != 0) ) {
		return -1;
	}
	if ( strcmp(argv[2], "source") == 0 ) {
		unsigned char pData[1024];

		memset(pData, 'P', sizeof(pData));
		for ( size_t i = 0u; i < 256u; i++ ) {
			if ( fwrite(pData, 1u, sizeof(pData), stdout) != sizeof(pData) ) {
				return 93;
			}
		}
		fputs("source-error", stderr);
		fflush(stdout);
		fflush(stderr);
		return 0;
	}
	if ( strcmp(argv[2], "copy") == 0 ) {
		return testPipelineChildCopy(argc >= 4 ? argv[3] : NULL);
	}
	if ( strcmp(argv[2], "sequence") == 0 ) {
		fputs("0123456789", stdout);
		fputs("abcdefghij", stderr);
		fflush(stdout);
		fflush(stderr);
		return 0;
	}
	if ( strcmp(argv[2], "sleep") == 0 ) {
		xrtSleep(5000u);
		return 0;
	}
	if ( strcmp(argv[2], "exit") == 0 ) {
		return argc >= 4 ? atoi(argv[3]) : 0;
	}
	return 90;
}



/* 初始化一个运行当前测试程序的 Pipeline 阶段。 */
static void testPipelineStage(
	xprocessconfig* pConfig,
	cstr sProgram,
	const cstr* pArgs,
	size_t iArgCount
)
{
	testRequire(xrtProcessConfigInit(pConfig), "pipeline stage config init failed");
	pConfig->Program = sProgram;
	pConfig->Args = pArgs;
	pConfig->ArgCount = iArgCount;
}



/* 验证捕获区只包含一个重复字节。 */
static bool testPipelineBytes(
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



/* 验证回调阶段索引，并记录每段每条流的字节数。 */
static bool testPipelineOutput(
	size_t iStage,
	xprocessstream Stream,
	xbytesview Data,
	ptr pUserData
)
{
	testpipelineoutput* pOutput = (testpipelineoutput*)pUserData;
	bool bValid = (iStage < 3u) &&
		((Stream == XPROCESS_STDOUT) || (Stream == XPROCESS_STDERR));

	if ( bValid && (Stream == XPROCESS_STDOUT) ) {
		bValid = (iStage == 2u) &&
			testPipelineBytes(Data.Data, Data.Size, 'P');
	}
	(void)xrtMutexLock(&pOutput->Lock);
	pOutput->Valid = pOutput->Valid && bValid;
	if ( bValid ) {
		pOutput->Sizes[iStage][Stream] += Data.Size;
	}
	pOutput->Calls++;
	(void)xrtMutexUnlock(&pOutput->Lock);
	return true;
}



/* 在线程中延迟请求 Pipeline 取消。 */
static int32 testPipelineCancel(ptr pData)
{
	testpipelinecancel* pCancel = (testpipelinecancel*)pData;

	xrtSleep(pCancel->Delay);
	return xrtCancelRequest(pCancel->Cancel) ? 0 : 1;
}



/* 验证真实 Pipeline、逐段结果、限额与共享控制流。 */
int main(int argc, char** argv)
{
	xprocesspipelineoptions Options;
	xprocesspipelineresult Result;
	xprocessconfig Stages[3];
	testpipelineoutput Output;
	testpipelinecancel CancelState;
	xcancel* pCancel;
	xthread* pThread;
	const xerror* pError;
	static unsigned char pInput[256u * 1024u];
	const size_t iDataSize = sizeof(pInput);
	const cstr pSource[] = { "--xrt-pipeline-child", "source" };
	const cstr pMiddle[] = {
		"--xrt-pipeline-child", "copy", "middle-error"
	};
	const cstr pSink[] = {
		"--xrt-pipeline-child", "copy", "sink-error"
	};
	const cstr pFirst[] = {
		"--xrt-pipeline-child", "copy", "first-error"
	};
	const cstr pSequence[] = {
		"--xrt-pipeline-child", "sequence"
	};
	const cstr pSleep[] = { "--xrt-pipeline-child", "sleep" };
	const cstr pExit[] = { "--xrt-pipeline-child", "exit", "7" };

	if ( (argc >= 2) && (strcmp(argv[1], "--xrt-pipeline-child") == 0) ) {
		return testPipelineChild(argc, argv);
	}

	testPipelineStage(&Stages[0], argv[0], pSource, 2u);
	testPipelineStage(&Stages[1], argv[0], pMiddle, 3u);
	testPipelineStage(&Stages[2], argv[0], pSink, 3u);
	testRequire(xrtMutexInit(&Output.Lock), "pipeline callback mutex init failed");
	memset(Output.Sizes, 0, sizeof(Output.Sizes));
	Output.Calls = 0u;
	Output.Valid = true;
	testRequire(
		xrtProcessPipelineOptionsInit(&Options),
		"pipeline options init failed"
	);
	Options.Output = testPipelineOutput;
	Options.UserData = &Output;
	testRequire(
		xrtProcessPipeline(Stages, 3u, &Options, &Result),
		"three-stage pipeline failed"
	);
	testRequire(xrtProcessPipelineSuccess(&Result), "three-stage result failed");
	testRequire(
		(Result.StdoutSize == iDataSize) &&
		testPipelineBytes(Result.Stdout, Result.StdoutSize, 'P'),
		"three-stage stdout mismatch"
	);
	testRequire(
		(Result.StageCount == 3u) &&
		(Result.Stages[0].StderrSize == 12u) &&
		(memcmp(Result.Stages[0].Stderr, "source-error", 12u) == 0) &&
		(Result.Stages[1].StderrSize == 12u) &&
		(memcmp(Result.Stages[1].Stderr, "middle-error", 12u) == 0) &&
		(Result.Stages[2].StderrSize == 10u) &&
		(memcmp(Result.Stages[2].Stderr, "sink-error", 10u) == 0),
		"stage stderr ownership mismatch"
	);
	testRequire(
		Output.Valid && (Output.Calls != 0u) &&
		(Output.Sizes[2][XPROCESS_STDOUT] == iDataSize) &&
		(Output.Sizes[0][XPROCESS_STDERR] == 12u) &&
		(Output.Sizes[1][XPROCESS_STDERR] == 12u) &&
		(Output.Sizes[2][XPROCESS_STDERR] == 10u),
		"pipeline callback attribution mismatch"
	);
	xrtProcessPipelineResultUnit(&Result);
	testRequire(xrtMutexUnit(&Output.Lock), "pipeline callback mutex unit failed");

	memset(pInput, 'I', sizeof(pInput));
	testPipelineStage(&Stages[0], argv[0], pFirst, 3u);
	testPipelineStage(&Stages[1], argv[0], pSink, 3u);
	testRequire(xrtProcessPipelineOptionsInit(&Options), "input options init failed");
	Options.Input.Data = pInput;
	Options.Input.Size = sizeof(pInput);
	testRequire(
		xrtProcessPipeline(Stages, 2u, &Options, &Result),
		"input pipeline failed"
	);
	testRequire(xrtProcessPipelineSuccess(&Result), "input pipeline result failed");
	testRequire(Result.InputWritten == sizeof(pInput), "pipeline input size mismatch");
	testRequire(
		(Result.StdoutSize == sizeof(pInput)) &&
		testPipelineBytes(Result.Stdout, Result.StdoutSize, 'I'),
		"pipeline input output mismatch"
	);
	xrtProcessPipelineResultUnit(&Result);

	testPipelineStage(&Stages[0], argv[0], pSequence, 2u);
	testRequire(xrtProcessPipelineOptionsInit(&Options), "limit options init failed");
	Options.StdoutLimit = 4u;
	Options.StderrLimit = 4u;
	Options.Overflow = XPROCESS_OVERFLOW_KEEP_LAST;
	testRequire(
		xrtProcessPipeline(Stages, 1u, &Options, &Result),
		"keep-last pipeline failed"
	);
	testRequire(
		(Result.StdoutSize == 4u) &&
		(memcmp(Result.Stdout, "6789", 4u) == 0) &&
		Result.StdoutTruncated &&
		(Result.Stages[0].StderrSize == 4u) &&
		(memcmp(Result.Stages[0].Stderr, "ghij", 4u) == 0) &&
		Result.Stages[0].StderrTruncated,
		"pipeline keep-last result mismatch"
	);
	xrtProcessPipelineResultUnit(&Result);

	testRequire(xrtProcessPipelineOptionsInit(&Options), "overflow options init failed");
	Options.StdoutLimit = 4u;
	Options.Overflow = XPROCESS_OVERFLOW_ERROR;
	testRequire(
		!xrtProcessPipeline(Stages, 1u, &Options, &Result),
		"overflow pipeline unexpectedly succeeded"
	);
	pError = xrtGetError();
	testRequire(
		(pError != NULL) &&
		(strcmp(xrtErrorDomain(pError), "xrt.process") == 0) &&
		(xrtErrorCode(pError) == XPROCESS_ERROR_LIMIT),
		"pipeline overflow error mismatch"
	);
	xrtProcessPipelineResultUnit(&Result);

	testPipelineStage(&Stages[0], argv[0], pExit, 3u);
	testRequire(
		xrtProcessPipeline(Stages, 1u, NULL, &Result),
		"nonzero pipeline infrastructure failed"
	);
	testRequire(!xrtProcessPipelineSuccess(&Result), "nonzero pipeline succeeded");
	testRequire(
		(Result.Stages[0].Status.Kind == XPROCESS_EXIT_CODE) &&
		(Result.Stages[0].Status.Code == 7),
		"nonzero pipeline status mismatch"
	);
	xrtProcessPipelineResultUnit(&Result);

	testPipelineStage(&Stages[0], argv[0], pSleep, 2u);
	testRequire(xrtProcessPipelineOptionsInit(&Options), "deadline options init failed");
	Options.Deadline = xrtDeadlineAfter(20000u);
	Options.StopGrace = 10000u;
	testRequire(
		xrtProcessPipeline(Stages, 1u, &Options, &Result),
		"deadline pipeline failed"
	);
	testRequire(Result.Wait == XWAIT_TIMEOUT, "pipeline deadline mismatch");
	testRequire(
		Result.Stages[0].Status.Stop != XPROCESS_STOP_NONE,
		"pipeline deadline stop missing"
	);
	xrtProcessPipelineResultUnit(&Result);

	pCancel = xrtCancelCreate();
	testRequire(pCancel != NULL, "pipeline cancel create failed");
	CancelState.Cancel = pCancel;
	CancelState.Delay = 20u;
	pThread = xrtThreadCreate(testPipelineCancel, &CancelState, 0u);
	testRequire(pThread != NULL, "pipeline cancel thread create failed");
	testRequire(xrtProcessPipelineOptionsInit(&Options), "cancel options init failed");
	Options.Cancel = pCancel;
	Options.StopGrace = 10000u;
	testRequire(
		xrtProcessPipeline(Stages, 1u, &Options, &Result),
		"cancel pipeline failed"
	);
	testRequire(Result.Wait == XWAIT_CANCELLED, "pipeline cancel mismatch");
	testRequire(xrtThreadWait(pThread) == XWAIT_OK, "pipeline cancel thread failed");
	xrtThreadDestroy(pThread);
	xrtCancelDestroy(pCancel);
	xrtProcessPipelineResultUnit(&Result);

	testPipelineStage(&Stages[0], argv[0], pSleep, 2u);
	testRequire(xrtProcessConfigInit(&Stages[1]), "missing stage config init failed");
	Stages[1].Program = "__xrt_pipeline_missing_stage_7f6a__";
	{
		xerror* pOld = xrtErrorCreate(
			XERR_VALUE,
			"test.pipeline.old",
			91,
			"old pipeline error"
		);

		testRequire(pOld != NULL, "old pipeline error allocation failed");
		xrtSetError(pOld);
		xrtErrorFree(pOld);
	}
	testRequire(
		!xrtProcessPipeline(Stages, 2u, NULL, &Result),
		"missing pipeline stage unexpectedly succeeded"
	);
	testRequire(
		xrtErrorCode(xrtGetError()) == XPROCESS_ERROR_COMMAND,
		"missing pipeline stage error mismatch"
	);
	return 0;
}
