#define XRT_MODULE_PROCESS_PIPELINE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 子进程模式原样复制标准输入。 */
static int singlePipelineCopy(void)
{
	char pData[128];
	size_t iRead;

	while ( (iRead = fread(pData, 1u, sizeof(pData), stdin)) != 0u ) {
		if ( fwrite(pData, 1u, iRead, stdout) != iRead ) {
			return 1;
		}
	}
	fflush(stdout);
	return ferror(stdin) ? 2 : 0;
}



/* 验证 Pipeline 单头闭包和两段真实管道。 */
int main(int argc, char** argv)
{
	xprocesspipelineoptions Options;
	xprocesspipelineresult Result;
	xprocessconfig Stages[2];
	const cstr pArgs[] = { "--single-pipeline-copy" };
	static const unsigned char pInput[] = "single pipeline";

	#if !defined(XRT_FEATURE_PROCESS_PIPELINE) || \
		!defined(XRT_FEATURE_PROCESS_RUN) || \
		!defined(XRT_FEATURE_PROCESS) || \
		!defined(XRT_FEATURE_BUFFER) || \
		!defined(XRT_FEATURE_CANCEL) || \
		defined(XRT_FEATURE_FUTURE) || \
		defined(XRT_FEATURE_FILE)
		#error "XRT_MODULE_PROCESS_PIPELINE dependency closure is incorrect"
	#endif

	if ( (argc >= 2) && (strcmp(argv[1], "--single-pipeline-copy") == 0) ) {
		return singlePipelineCopy();
	}
	for ( size_t i = 0u; i < 2u; i++ ) {
		if ( !xrtProcessConfigInit(&Stages[i]) ) {
			return 3;
		}
		Stages[i].Program = argv[0];
		Stages[i].Args = pArgs;
		Stages[i].ArgCount = 1u;
	}
	if ( !xrtProcessPipelineOptionsInit(&Options) ) {
		return 4;
	}
	Options.Input.Data = pInput;
	Options.Input.Size = sizeof(pInput) - 1u;
	if ( !xrtProcessPipeline(Stages, 2u, &Options, &Result) ) {
		return 5;
	}
	if ( !xrtProcessPipelineSuccess(&Result) ||
		(Result.InputWritten != Options.Input.Size) ||
		(Result.StdoutSize != Options.Input.Size) ||
		(memcmp(Result.Stdout, pInput, Options.Input.Size) != 0) ) {
		xrtProcessPipelineResultUnit(&Result);
		return 6;
	}
	xrtProcessPipelineResultUnit(&Result);
	return 0;
}
