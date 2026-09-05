#include <stdio.h>

#include <xrt.h>



/*
 * 范例：process/capture —— Shell 一步执行 + 全量输出捕获
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtProcessShell          走系统 shell 执行整条命令并等待完成
 *   Result.Stdout / Size     捕获的标准输出（拥有式缓冲）
 *   xrtProcessResultSuccess  退出状态是否"正常退出且码为 0"
 *   xrtProcessResultUnit     释放结果（含捕获缓冲）
 * 模块宏：XRT_MODULE_PROCESS
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c ${BS}
 *       examples/process/capture/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   captured output
 *
 * Shell 族 vs Spawn 族：Shell 接受整条命令串（走 cmd/sh 解释），
 *   适合工具调用、快速脚本；参数含空格/元字符要小心注入面——
 *   不信任输入时用 Spawn + Argv（见 stream 范例）。
 * Success 判定区分"码非 0"与"被信号杀死"（结构里有两者）。
 */


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
