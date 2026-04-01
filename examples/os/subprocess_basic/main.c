/*
 * XRT Example - Subprocess Basic
 * XRT 范例 - Subprocess 基础用法
 *
 * Description / 说明:
 *   EN: Demonstrates direct exec, shell command execution, stdin piping,
 *       and safe stdout polling with ReadSince.
 *   CN: 演示 direct exec、shell 命令执行、stdin 管道写入，以及
 *       ReadSince 安全轮询 stdout。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/subprocess_basic.exe -lws2_32 -lshell32 -liphlpapi
 *   gcc main.c -o ../../bin/subprocess_basic -pthread -lm
 */

#include <stdio.h>
#include <string.h>

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"


static void PrintExitInfo(const xprocessexitinfo* pExitInfo)
{
	if ( pExitInfo == NULL ) {
		return;
	}

	printf("  kind=%d exit=%d signal=%d stage=%d os_error=%d stop=%d timed_out=%d cancelled=%d\n",
		pExitInfo->iKind,
		pExitInfo->iExitCode,
		pExitInfo->iSignal,
		pExitInfo->iStage,
		pExitInfo->iOsError,
		pExitInfo->iStopReason,
		pExitInfo->bTimedOut ? 1 : 0,
		pExitInfo->bCancelled ? 1 : 0);
}


static int ChildEmit(void)
{
	fputs("child-stdout\n", stdout);
	fputs("child-stderr\n", stderr);
	fflush(stdout);
	fflush(stderr);
	return 7;
}


static int ChildEchoStdin(void)
{
	char sBuf[256];
	size_t iRead;

	while ( (iRead = fread(sBuf, 1u, sizeof(sBuf), stdin)) > 0u ) {
		if ( fwrite(sBuf, 1u, iRead, stdout) != iRead ) {
			return 31;
		}
		fflush(stdout);
	}
	return ferror(stdin) ? 32 : 0;
}


static int ChildMain(int argc, char** argv)
{
	if ( argc > 0 && argv != NULL && strcmp(argv[0], "--child-emit") == 0 ) {
		return ChildEmit();
	}
	if ( argc > 0 && argv != NULL && strcmp(argv[0], "--child-echo") == 0 ) {
		return ChildEchoStdin();
	}
	return 33;
}


static int RunDirectExecExample(void)
{
	xprocessconfig tConfig;
	xprocessresult tResult;
	str arrArgs[] = { (str)"--child-emit" };

	printf("== Direct Exec ==\n");

	xrtProcessConfigInit(&tConfig);
	tConfig.sProgram = xCore.AppFile;
	tConfig.arrArgs = arrArgs;
	tConfig.iArgCount = 1u;

	if ( !xrtExecCapture(&tConfig, &tResult, 3000u) ) {
		printf("  spawn failed\n");
		PrintExitInfo(&tResult.ExitInfo);
		return 1;
	}

	PrintExitInfo(&tResult.ExitInfo);
	printf("  stdout=%s", tResult.pStdout ? (char*)tResult.pStdout : "");
	printf("  stderr=%s", tResult.pStderr ? (char*)tResult.pStderr : "");
	xrtProcessResultUnit(&tResult);
	return 0;
}


static int RunShellExample(void)
{
	xprocessconfig tConfig;
	xprocessresult tResult;

	printf("== Shell Command ==\n");

	xrtProcessConfigInit(&tConfig);
	tConfig.iTargetKind = XPROC_TARGET_SHELL;
	tConfig.sCommand = (str)"echo shell-example";

	if ( !xrtExecCapture(&tConfig, &tResult, 3000u) ) {
		printf("  shell failed\n");
		PrintExitInfo(&tResult.ExitInfo);
		return 1;
	}

	PrintExitInfo(&tResult.ExitInfo);
	printf("  stdout=%s", tResult.pStdout ? (char*)tResult.pStdout : "");
	xrtProcessResultUnit(&tResult);
	return 0;
}


static int RunInteractiveExample(void)
{
	xprocessconfig tConfig;
	xprocessreadinfo tReadInfo;
	xprocessexitinfo tExitInfo;
	xprocess* pProcess;
	str arrArgs[] = { (str)"--child-echo" };
	const char* sInput = "alpha\nbeta\n";
	ptr pChunk;
	size_t iSize = 0u;

	printf("== Stdin + ReadSince ==\n");

	xrtProcessConfigInit(&tConfig);
	tConfig.sProgram = xCore.AppFile;
	tConfig.arrArgs = arrArgs;
	tConfig.iArgCount = 1u;
	tConfig.Stdin.iMode = XPROC_STDIO_PIPE;
	tConfig.Stdout.iMode = XPROC_STDIO_PIPE;

	pProcess = xrtProcessSpawn(&tConfig);
	if ( pProcess == NULL ) {
		printf("  spawn failed\n");
		return 1;
	}

	if ( xrtProcessWriteText(pProcess, (str)sInput, 0u) < 0 ) {
		xrtProcessKillTree(pProcess);
		xrtProcessWait(pProcess);
		xrtProcessDestroy(pProcess);
		return 1;
	}

	xrtProcessCloseStdin(pProcess);
	xrtProcessWait(pProcess);
	xrtProcessGetExitInfo(pProcess, &tExitInfo);
	PrintExitInfo(&tExitInfo);

	memset(&tReadInfo, 0, sizeof(tReadInfo));
	pChunk = xrtProcessReadStdoutSince(pProcess, 0u, 0u, &iSize, &tReadInfo);
	if ( pChunk != NULL ) {
		printf("  stdout-chunk=%.*s", (int)iSize, (char*)pChunk);
		printf("  next_offset=%llu done=%d\n",
			(unsigned long long)tReadInfo.iNextOffset,
			tReadInfo.bDone ? 1 : 0);
		xrtFree(pChunk);
	}

	xrtProcessDestroy(pProcess);
	return 0;
}


int main(int argc, char** argv)
{
	int iRet = 0;

	if ( argc > 1 && argv != NULL ) {
		return ChildMain(argc - 1, &argv[1]);
	}

	xrtInit();

	iRet |= RunDirectExecExample();
	iRet |= RunShellExample();
	iRet |= RunInteractiveExample();

	xrtUnit();
	return iRet == 0 ? 0 : 1;
}
