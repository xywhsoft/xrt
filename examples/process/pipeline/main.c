#include <stdio.h>

#include <xrt.h>



/* 用两个 Shell 阶段演示真实管道连接与末段捕获。 */
int main(void)
{
	xprocesspipelineresult Result;
	xprocessconfig Stages[2];
	bool bOk;

	#if defined(_WIN32) || defined(_WIN64)
		if ( !xrtProcessShellConfigInit(&Stages[0], "echo pipeline output") ||
			!xrtProcessShellConfigInit(&Stages[1], "findstr pipeline") ) {
			return 1;
		}
	#else
		if ( !xrtProcessShellConfigInit(&Stages[0], "printf 'pipeline output\n'") ||
			!xrtProcessShellConfigInit(&Stages[1], "tr a-z A-Z") ) {
			return 1;
		}
	#endif
	bOk = xrtProcessPipeline(Stages, 2u, NULL, &Result);
	if ( !bOk ) {
		const xerror* pError = xrtGetError();

		fprintf(
			stderr,
			"pipeline failed: %s\n",
			pError != NULL ? xrtErrorMessage(pError) : "unknown error"
		);
		return 2;
	}
	fwrite(Result.Stdout, 1u, Result.StdoutSize, stdout);
	bOk = xrtProcessPipelineSuccess(&Result);
	xrtProcessPipelineResultUnit(&Result);
	return bOk ? 0 : 3;
}
