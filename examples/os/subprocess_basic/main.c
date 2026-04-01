/*
 * XRT Example - Subprocess Basic
 * XRT 范例 - Subprocess 基础用法
 *
 * Description / 说明:
 *   EN: Demonstrates direct exec, shell command execution, stdin piping,
 *       event timeline polling, and terminal capture.
 *   CN: 演示 direct exec、shell 命令执行、stdin 管道写入、
 *       event 时间线轮询，以及 terminal 输出捕获。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/subprocess_basic.exe -lws2_32 -lshell32 -liphlpapi
 *   gcc main.c -o ../../bin/subprocess_basic -pthread -lm
 */

#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
	#include <io.h>
#else
	#include <unistd.h>
#endif

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


static int ChildTtyState(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		printf("%d %d %d\n", _isatty(_fileno(stdin)) ? 1 : 0, _isatty(_fileno(stdout)) ? 1 : 0, _isatty(_fileno(stderr)) ? 1 : 0);
	#else
		printf("%d %d %d\n", isatty(fileno(stdin)) ? 1 : 0, isatty(fileno(stdout)) ? 1 : 0, isatty(fileno(stderr)) ? 1 : 0);
	#endif
	fflush(stdout);
	return 0;
}


static void NormalizeTerminalText(const char* sInput, char* sOutput, size_t iCap)
{
	size_t iWrite = 0u;
	size_t iRead = 0u;

	if ( sOutput == NULL || iCap == 0u ) {
		return;
	}
	if ( sInput == NULL ) {
		sOutput[0] = '\0';
		return;
	}

	while ( sInput[iRead] != '\0' && iWrite + 1u < iCap ) {
		unsigned char c = (unsigned char)sInput[iRead];

		if ( c == '\r' ) {
			iRead++;
			continue;
		}
		if ( c == 0x1B ) {
			iRead++;
			if ( sInput[iRead] == '[' ) {
				iRead++;
				while ( sInput[iRead] != '\0' ) {
					unsigned char cFinal = (unsigned char)sInput[iRead];

					iRead++;
					if ( cFinal >= 0x40u && cFinal <= 0x7Eu ) {
						break;
					}
				}
				continue;
			}
			if ( sInput[iRead] == ']' ) {
				iRead++;
				while ( sInput[iRead] != '\0' ) {
					if ( sInput[iRead] == '\a' ) {
						iRead++;
						break;
					}
					if ( sInput[iRead] == 0x1B && sInput[iRead + 1u] == '\\' ) {
						iRead += 2u;
						break;
					}
					iRead++;
				}
				continue;
			}
			if ( sInput[iRead] != '\0' ) {
				iRead++;
			}
			continue;
		}

		sOutput[iWrite++] = sInput[iRead++];
	}

	sOutput[iWrite] = '\0';
}


static void PrintEvents(xprocess* pProcess)
{
	xprocesseventreadinfo tInfo;
	xprocessevent* arrEvents = NULL;
	uint32 iEventCount = 0u;

	memset(&tInfo, 0, sizeof(tInfo));
	arrEvents = xrtProcessReadEventsSince(pProcess, 0u, 0u, &iEventCount, &tInfo);
	if ( arrEvents == NULL ) {
		printf("  events=0\n");
		return;
	}

	printf("  events=%u done=%d\n", iEventCount, tInfo.bDone ? 1 : 0);
	for ( uint32 i = 0u; i < iEventCount; i++ ) {
		printf("    seq=%llu kind=%d stream=%d offset=%llu size=%llu exit=%d\n",
			(unsigned long long)arrEvents[i].iSeq,
			arrEvents[i].iKind,
			arrEvents[i].iStream,
			(unsigned long long)arrEvents[i].iOffset,
			(unsigned long long)arrEvents[i].iSize,
			arrEvents[i].ExitInfo.iExitCode);
	}

	xrtFree(arrEvents);
}


static int ChildMain(int argc, char** argv)
{
	if ( argc > 0 && argv != NULL && strcmp(argv[0], "--child-emit") == 0 ) {
		return ChildEmit();
	}
	if ( argc > 0 && argv != NULL && strcmp(argv[0], "--child-echo") == 0 ) {
		return ChildEchoStdin();
	}
	if ( argc > 0 && argv != NULL && strcmp(argv[0], "--child-tty") == 0 ) {
		return ChildTtyState();
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


static int RunEventExample(void)
{
	xprocessconfig tConfig;
	xprocess* pProcess;
	xprocessexitinfo tExitInfo;
	str arrArgs[] = { (str)"--child-emit" };

	printf("== Event Timeline ==\n");

	xrtProcessConfigInit(&tConfig);
	tConfig.sProgram = xCore.AppFile;
	tConfig.arrArgs = arrArgs;
	tConfig.iArgCount = 1u;
	tConfig.Stdout.iMode = XPROC_STDIO_PIPE;
	tConfig.Stderr.iMode = XPROC_STDIO_PIPE;

	pProcess = xrtProcessSpawn(&tConfig);
	if ( pProcess == NULL ) {
		printf("  spawn failed\n");
		return 1;
	}

	xrtProcessWait(pProcess);
	xrtProcessGetExitInfo(pProcess, &tExitInfo);
	PrintExitInfo(&tExitInfo);
	PrintEvents(pProcess);
	xrtProcessDestroy(pProcess);
	return 0;
}


static int RunTerminalExample(void)
{
	xprocessconfig tConfig;
	xprocessresult tResult;
	str arrArgs[] = { (str)"--child-tty" };
	char sNormalized[256];

	printf("== Terminal Capture ==\n");

	if ( !xrtProcessTerminalSupported() ) {
		printf("  terminal is not supported on this platform\n");
		return 0;
	}

	xrtProcessConfigInit(&tConfig);
	tConfig.sProgram = xCore.AppFile;
	tConfig.arrArgs = arrArgs;
	tConfig.iArgCount = 1u;
	tConfig.bUseTerminal = true;
	tConfig.iTerminalCols = 100u;
	tConfig.iTerminalRows = 32u;

	if ( !xrtExecCapture(&tConfig, &tResult, 3000u) ) {
		printf("  terminal spawn failed\n");
		PrintExitInfo(&tResult.ExitInfo);
		return 1;
	}

	PrintExitInfo(&tResult.ExitInfo);
	NormalizeTerminalText((const char*)tResult.pStdout, sNormalized, sizeof(sNormalized));
	printf("  stdout-bytes=%llu\n", (unsigned long long)tResult.iStdoutSize);
	printf("  terminal-text=%s", sNormalized);
	xrtProcessResultUnit(&tResult);
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
	iRet |= RunEventExample();
	iRet |= RunTerminalExample();

	xrtUnit();
	return iRet == 0 ? 0 : 1;
}
