#include <stdio.h>

#include <xrt.h>



/* 执行命令、输出捕获内容，并把子进程退出状态映射为示例退出码。 */
int main(void)
{
	xprocessresult Result;
	bool bOk;

	#if defined(_WIN32) || defined(_WIN64)
		bOk = xrtProcessShell("echo captured output", &Result);
	#else
		bOk = xrtProcessShell("printf 'captured output\n'", &Result);
	#endif
	if ( !bOk ) {
		const xerror* pError = xrtGetError();

		fprintf(
			stderr,
			"process failed: %s\n",
			pError != NULL ? xrtErrorMessage(pError) : "unknown error"
		);
		return 1;
	}
	fwrite(Result.Stdout, 1u, Result.StdoutSize, stdout);
	bOk = xrtProcessResultSuccess(&Result);
	xrtProcessResultUnit(&Result);
	return bOk ? 0 : 2;
}
