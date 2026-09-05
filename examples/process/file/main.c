#include <xrt.h>



/*
 * 范例：process/file —— 子进程 stdout 重定向到 XRT 文件句柄
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtOpen / xrtClose          XRT 文件打开/关闭（FILE 模块）
 *   xrtProcessShellConfigInit   Shell 命令 + 可调配置入口
 *   xrtProcessFile              把 xfile 包装为子进程 IO 目标
 *   XPROCESS_IO_NULL            丢弃该流（stdin/stderr 常用）
 *   xrtProcessSpawn / Wait / Destroy   生成、等待、释放
 * 模块宏：XRT_MODULE_PROCESS（依赖 FILE）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c ${BS}
 *       examples/process/file/main.c -lws2_32 -liphlpapi
 * 预期输出：无 stdout；生成文件 xrt-process-output.txt 内含
 *   redirected output
 *
 * 重定向三选一：PIPE（父进程读）/FILE（直写文件，零父进程中转）/
 *   NULL（丢弃）。日志归档、命令导出场景用 FILE 最省——
 *   数据从子进程直达文件描述符，不经过父进程内存。
 */


/* 把命令输出直接重定向到 XRT 文件。 */
int main(void)
{
	xprocessconfig Config;
	xprocess* pProcess;
	xfile File = xrtOpen(
		"xrt-process-output.txt",
		XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE
	);

	if ( File == NULL ) {
		return 1;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( !xrtProcessShellConfigInit(&Config, "echo redirected output") ) {
			(void)xrtClose(File);
			return 2;
		}
	#else
		if ( !xrtProcessShellConfigInit(&Config, "printf 'redirected output\n'") ) {
			(void)xrtClose(File);
			return 2;
		}
	#endif
	Config.Stdin.Mode = XPROCESS_IO_NULL;
	Config.Stdout = xrtProcessFile(File);
	Config.Stderr.Mode = XPROCESS_IO_NULL;
	pProcess = xrtProcessSpawn(&Config);
	(void)xrtClose(File);
	if ( pProcess == NULL ) {
		return 3;
	}
	if ( xrtProcessWait(pProcess) != XWAIT_OK ) {
		xrtProcessDestroy(pProcess);
		return 4;
	}
	xrtProcessDestroy(pProcess);
	return 0;
}
