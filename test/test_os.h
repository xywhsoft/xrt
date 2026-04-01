typedef struct {
	char sStdout[256];
	size_t iStdoutSize;
	char sStderr[256];
	size_t iStderrSize;
	int iStdoutCalls;
	int iStderrCalls;
	int iExitCalls;
	int iExitCode;
	int iExitKind;
} xrt_test_os_stream_ctx;


// 内部函数：__xrtTestOSAppendText
static void __xrtTestOSAppendText(char* sBuf, size_t* piBufSize, size_t iCap, const void* pData, size_t iSize)
{
	size_t iCopy;

	if ( sBuf == NULL || piBufSize == NULL || pData == NULL || iCap == 0u ) {
		return;
	}
	if ( *piBufSize >= (iCap - 1u) ) {
		return;
	}

	iCopy = iSize;
	if ( iCopy > ((iCap - 1u) - *piBufSize) ) {
		iCopy = (iCap - 1u) - *piBufSize;
	}
	memcpy(sBuf + *piBufSize, pData, iCopy);
	*piBufSize += iCopy;
	sBuf[*piBufSize] = '\0';
}


// 内部函数：__xrtTestOSNormalizeText
static void __xrtTestOSNormalizeText(const char* sInput, char* sOutput, size_t iCap)
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
		if ( sInput[iRead] != '\r' ) {
			sOutput[iWrite++] = sInput[iRead];
		}
		iRead++;
	}
	sOutput[iWrite] = '\0';
}


// 内部函数：__xrtTestOSOnStdout
static void __xrtTestOSOnStdout(xprocess* pProcess, const void* pData, size_t iSize, ptr pUserData)
{
	xrt_test_os_stream_ctx* pCtx = (xrt_test_os_stream_ctx*)pUserData;
	(void)pProcess;

	if ( pCtx == NULL ) {
		return;
	}

	pCtx->iStdoutCalls++;
	__xrtTestOSAppendText(pCtx->sStdout, &pCtx->iStdoutSize, sizeof(pCtx->sStdout), pData, iSize);
}


// 内部函数：__xrtTestOSOnStderr
static void __xrtTestOSOnStderr(xprocess* pProcess, const void* pData, size_t iSize, ptr pUserData)
{
	xrt_test_os_stream_ctx* pCtx = (xrt_test_os_stream_ctx*)pUserData;
	(void)pProcess;

	if ( pCtx == NULL ) {
		return;
	}

	pCtx->iStderrCalls++;
	__xrtTestOSAppendText(pCtx->sStderr, &pCtx->iStderrSize, sizeof(pCtx->sStderr), pData, iSize);
}


// 内部函数：__xrtTestOSOnExit
static void __xrtTestOSOnExit(xprocess* pProcess, const xprocessexitinfo* pExitInfo, ptr pUserData)
{
	xrt_test_os_stream_ctx* pCtx = (xrt_test_os_stream_ctx*)pUserData;
	(void)pProcess;

	if ( pCtx == NULL ) {
		return;
	}

	pCtx->iExitCalls++;
	pCtx->iExitCode = pExitInfo ? pExitInfo->iExitCode : -1;
	pCtx->iExitKind = pExitInfo ? pExitInfo->iKind : XPROC_EXIT_NONE;
}


// 内部函数：__xrtTestOSSleepMs
static void __xrtTestOSSleepMs(uint32 iMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iMs);
	#else
		usleep((useconds_t)iMs * 1000u);
	#endif
}


// 内部函数：__xrtTestOSPrepareSelfConfig
static void __xrtTestOSPrepareSelfConfig(xrtGlobalData* xCore, xprocessconfig* pConfig, str* arrArgs, uint32 iArgCount)
{
	xrtProcessConfigInit(pConfig);
	pConfig->sProgram = xCore->AppFile;
	pConfig->arrArgs = arrArgs;
	pConfig->iArgCount = iArgCount;
}


// 内部函数：__xrtTestSubprocessHelperMain
static int __xrtTestSubprocessHelperMain(int argc, char** argv)
{
	if ( argc <= 0 || argv == NULL ) {
		return 120;
	}

	if ( strcmp(argv[0], "emit") == 0 ) {
		fprintf(stdout, "helper-stdout\n");
		fprintf(stderr, "helper-stderr\n");
		fflush(stdout);
		fflush(stderr);
		return 7;
	}
	if ( strcmp(argv[0], "stream") == 0 ) {
		fprintf(stdout, "stream-out-a\n");
		fflush(stdout);
		__xrtTestOSSleepMs(40u);
		fprintf(stderr, "stream-err-b\n");
		fflush(stderr);
		__xrtTestOSSleepMs(40u);
		fprintf(stdout, "stream-out-c\n");
		fflush(stdout);
		return 9;
	}
	if ( strcmp(argv[0], "stdin_echo") == 0 ) {
		char sBuf[256];
		size_t iRead;

		while ( (iRead = fread(sBuf, 1u, sizeof(sBuf), stdin)) > 0u ) {
			if ( fwrite(sBuf, 1u, iRead, stdout) != iRead ) {
				return 121;
			}
			fflush(stdout);
		}
		return ferror(stdin) ? 122 : 0;
	}
	if ( strcmp(argv[0], "sleep_exit") == 0 ) {
		uint32 iMs = (argc > 1) ? (uint32)strtoul(argv[1], NULL, 10) : 100u;
		int iCode = (argc > 2) ? atoi(argv[2]) : 0;

		__xrtTestOSSleepMs(iMs);
		return iCode;
	}
	if ( strcmp(argv[0], "print_env") == 0 ) {
		const char* sValue;

		if ( argc <= 1 || argv[1] == NULL ) {
			return 124;
		}
		sValue = getenv(argv[1]);
		if ( sValue != NULL ) {
			fputs(sValue, stdout);
			fflush(stdout);
		}
		return 0;
	}
	if ( strcmp(argv[0], "print_cwd") == 0 ) {
		char sBuf[1024];

		#if defined(_WIN32) || defined(_WIN64)
			if ( _getcwd(sBuf, (int)sizeof(sBuf)) == NULL ) {
				return 125;
			}
		#else
			if ( getcwd(sBuf, sizeof(sBuf)) == NULL ) {
				return 125;
			}
		#endif

		fputs(sBuf, stdout);
		fflush(stdout);
		return 0;
	}

	return 123;
}


// OS测试
static int Test_OS(xrtGlobalData* xCore)
{
	static xprocessevents tEvents = {
		NULL,
		__xrtTestOSOnStdout,
		__xrtTestOSOnStderr,
		__xrtTestOSOnExit
	};

	xprocessconfig tConfig;
	xprocessresult tResult;
	xprocess* pProcess = NULL;
	xprocessexitinfo tExitInfo;
	xprocessreadinfo tReadInfo;
	size_t iSize = 0u;
	ptr pData = NULL;
	char sNormalized[256];
	#if !defined(XRT_NO_NETWORK)
		xfuture* pFuture = NULL;
	#endif

	{
		str arrArgs[] = { (str)"__subprocess_helper", (str)"emit" };

		memset(&tResult, 0, sizeof(tResult));
		__xrtTestOSPrepareSelfConfig(xCore, &tConfig, arrArgs, 2u);
		if ( !xrtExecCapture(&tConfig, &tResult, 3000u) ) {
			return 6001;
		}
		if ( tResult.iExitCode != 7 ) {
			xrtProcessResultUnit(&tResult);
			return 6002;
		}
		__xrtTestOSNormalizeText((char*)tResult.pStdout, sNormalized, sizeof(sNormalized));
		if ( tResult.pStdout == NULL || strcmp(sNormalized, "helper-stdout\n") != 0 ) {
			xrtProcessResultUnit(&tResult);
			return 6003;
		}
		__xrtTestOSNormalizeText((char*)tResult.pStderr, sNormalized, sizeof(sNormalized));
		if ( tResult.pStderr == NULL || strcmp(sNormalized, "helper-stderr\n") != 0 ) {
			xrtProcessResultUnit(&tResult);
			return 6004;
		}
		xrtProcessResultUnit(&tResult);
	}

	{
		static const char sInput[] = "alpha\nbeta\n";
		str arrArgs[] = { (str)"__subprocess_helper", (str)"stdin_echo" };
		ptr pStdout;

		__xrtTestOSPrepareSelfConfig(xCore, &tConfig, arrArgs, 2u);
		tConfig.Stdin.iMode = XPROC_STDIO_PIPE;
		tConfig.Stdout.iMode = XPROC_STDIO_PIPE;
		pProcess = xrtProcessSpawn(&tConfig);
		if ( pProcess == NULL ) {
			return 6010;
		}
		if ( xrtProcessWriteText(pProcess, (str)sInput, 0) != (int64)(sizeof(sInput) - 1u) ) {
			xrtProcessDestroy(pProcess);
			return 6011;
		}
		if ( !xrtProcessCloseStdin(pProcess) ) {
			xrtProcessDestroy(pProcess);
			return 6012;
		}
		if ( !xrtProcessWait(pProcess) ) {
			xrtProcessDestroy(pProcess);
			return 6013;
		}
		memset(&tExitInfo, 0, sizeof(tExitInfo));
		if ( !xrtProcessGetExitInfo(pProcess, &tExitInfo) || tExitInfo.iKind != XPROC_EXIT_NORMAL || tExitInfo.iExitCode != 0 ) {
			xrtProcessDestroy(pProcess);
			return 6016;
		}
		if ( xrtProcessExitCode(pProcess) != 0 ) {
			xrtProcessDestroy(pProcess);
			return 6014;
		}
		pStdout = xrtProcessGetStdout(pProcess, &iSize);
		__xrtTestOSNormalizeText((char*)pStdout, sNormalized, sizeof(sNormalized));
		if ( pStdout == NULL || strcmp(sNormalized, sInput) != 0 ) {
			xrtFree(pStdout);
			xrtProcessDestroy(pProcess);
			return 6015;
		}
		xrtFree(pStdout);
		xrtProcessDestroy(pProcess);
	}

	{
		xrt_test_os_stream_ctx tCtx;
		str arrArgs[] = { (str)"__subprocess_helper", (str)"stream" };

		memset(&tCtx, 0, sizeof(tCtx));
		__xrtTestOSPrepareSelfConfig(xCore, &tConfig, arrArgs, 2u);
		tConfig.Stdout.iMode = XPROC_STDIO_PIPE;
		tConfig.Stderr.iMode = XPROC_STDIO_PIPE;
		tConfig.pEvents = &tEvents;
		tConfig.pUserData = &tCtx;
		pProcess = xrtProcessSpawn(&tConfig);
		if ( pProcess == NULL ) {
			return 6020;
		}
		if ( !xrtProcessWait(pProcess) ) {
			xrtProcessDestroy(pProcess);
			return 6021;
		}
		if ( tCtx.iStdoutCalls <= 0 || tCtx.iStderrCalls <= 0 || tCtx.iExitCalls != 1 ) {
			xrtProcessDestroy(pProcess);
			return 6022;
		}
		if ( tCtx.iExitCode != 9 ) {
			xrtProcessDestroy(pProcess);
			return 6023;
		}
		if ( tCtx.iExitKind != XPROC_EXIT_NORMAL ) {
			xrtProcessDestroy(pProcess);
			return 6024;
		}
		__xrtTestOSNormalizeText(tCtx.sStdout, sNormalized, sizeof(sNormalized));
		if ( strcmp(sNormalized, "stream-out-a\nstream-out-c\n") != 0 ) {
			xrtProcessDestroy(pProcess);
			return 6025;
		}
		__xrtTestOSNormalizeText(tCtx.sStderr, sNormalized, sizeof(sNormalized));
		if ( strcmp(sNormalized, "stream-err-b\n") != 0 ) {
			xrtProcessDestroy(pProcess);
			return 6026;
		}
		xrtProcessDestroy(pProcess);
	}

	{
		str arrArgs[] = { (str)"__subprocess_helper", (str)"stream" };

		__xrtTestOSPrepareSelfConfig(xCore, &tConfig, arrArgs, 2u);
		tConfig.Stdout.iMode = XPROC_STDIO_PIPE;
		tConfig.Stderr.iMode = XPROC_STDIO_PIPE;
		pProcess = xrtProcessSpawn(&tConfig);
		if ( pProcess == NULL ) {
			return 6027;
		}
		if ( !xrtProcessWait(pProcess) ) {
			xrtProcessDestroy(pProcess);
			return 6028;
		}

		memset(&tReadInfo, 0, sizeof(tReadInfo));
		pData = xrtProcessReadStdoutSince(pProcess, 0u, 8u, &iSize, &tReadInfo);
		if ( pData == NULL || iSize != 8u ) {
			xrtFree(pData);
			xrtProcessDestroy(pProcess);
			return 6029;
		}
		if ( tReadInfo.iBaseOffset != 0u || tReadInfo.iNextOffset != 8u || !tReadInfo.bDone ) {
			xrtFree(pData);
			xrtProcessDestroy(pProcess);
			return 6030;
		}
		__xrtTestOSNormalizeText((const char*)pData, sNormalized, sizeof(sNormalized));
		if ( strcmp(sNormalized, "stream-o") != 0 ) {
			xrtFree(pData);
			xrtProcessDestroy(pProcess);
			return 6031;
		}
		xrtFree(pData);

		pData = xrtProcessReadStdoutSince(pProcess, tReadInfo.iNextOffset, 0u, &iSize, &tReadInfo);
		if ( pData == NULL || iSize == 0u ) {
			xrtFree(pData);
			xrtProcessDestroy(pProcess);
			return 6032;
		}
		__xrtTestOSNormalizeText((const char*)pData, sNormalized, sizeof(sNormalized));
		if ( strcmp(sNormalized, "ut-a\nstream-out-c\n") != 0 ) {
			xrtFree(pData);
			xrtProcessDestroy(pProcess);
			return 6033;
		}
		xrtFree(pData);
		xrtProcessDestroy(pProcess);
	}

	{
		str arrArgs[] = { (str)"__subprocess_helper", (str)"print_env", (str)"XRT_SUBPROC_SAMPLE" };
		str arrEnv[] = { (str)"XRT_SUBPROC_SAMPLE=env-value" };

		__xrtTestOSPrepareSelfConfig(xCore, &tConfig, arrArgs, 3u);
		tConfig.arrEnv = arrEnv;
		tConfig.iEnvCount = 1u;
		if ( !xrtExecCapture(&tConfig, &tResult, 3000u) ) {
			return 6034;
		}
		__xrtTestOSNormalizeText((char*)tResult.pStdout, sNormalized, sizeof(sNormalized));
		if ( tResult.iExitCode != 0 || strcmp(sNormalized, "env-value") != 0 ) {
			xrtProcessResultUnit(&tResult);
			return 6035;
		}
		xrtProcessResultUnit(&tResult);
	}

	{
		str arrArgs[] = { (str)"__subprocess_helper", (str)"emit" };

		__xrtTestOSPrepareSelfConfig(xCore, &tConfig, arrArgs, 2u);
		#if defined(_WIN32) || defined(_WIN64)
			tConfig.sWorkDir = (str)"Z:/__xrt_subprocess_missing__/cwd";
		#else
			tConfig.sWorkDir = (str)"/__xrt_subprocess_missing__/cwd";
		#endif
		if ( xrtExecCapture(&tConfig, &tResult, 3000u) ) {
			xrtProcessResultUnit(&tResult);
			return 6036;
		}
		if ( tResult.ExitInfo.iKind != XPROC_EXIT_SPAWN_FAILED || tResult.ExitInfo.iStage != XPROC_STAGE_WORKDIR ) {
			xrtProcessResultUnit(&tResult);
			return 6037;
		}
		xrtProcessResultUnit(&tResult);
	}

	{
		str arrArgs[] = { (str)"__subprocess_helper", (str)"print_cwd" };

		__xrtTestOSPrepareSelfConfig(xCore, &tConfig, arrArgs, 2u);
		tConfig.sWorkDir = xCore->AppPath;
		if ( !xrtExecCapture(&tConfig, &tResult, 3000u) ) {
			return 6049;
		}
		__xrtTestOSNormalizeText((char*)tResult.pStdout, sNormalized, sizeof(sNormalized));
		if ( tResult.iExitCode != 0 || strcmp(sNormalized, xCore->AppPath) != 0 ) {
			xrtProcessResultUnit(&tResult);
			return 6050;
		}
		xrtProcessResultUnit(&tResult);
	}

	{
		xrtProcessConfigInit(&tConfig);
		tConfig.iTargetKind = XPROC_TARGET_SHELL;
		tConfig.sCommand = (str)"echo shell-out";
		if ( !xrtExecCapture(&tConfig, &tResult, 3000u) ) {
			return 6038;
		}
		__xrtTestOSNormalizeText((char*)tResult.pStdout, sNormalized, sizeof(sNormalized));
		if ( tResult.iExitCode != 0 || strcmp(sNormalized, "shell-out\n") != 0 ) {
			xrtProcessResultUnit(&tResult);
			return 6039;
		}
		xrtProcessResultUnit(&tResult);
	}

	{
		str arrArgs[] = { (str)"__subprocess_helper", (str)"sleep_exit", (str)"1000", (str)"5" };

		__xrtTestOSPrepareSelfConfig(xCore, &tConfig, arrArgs, 4u);
		if ( !xrtExecCapture(&tConfig, &tResult, 30u) ) {
			return 6051;
		}
		if ( !tResult.ExitInfo.bTimedOut || tResult.ExitInfo.iStopReason == XPROC_STOP_NONE || tResult.ExitInfo.bCancelled ) {
			xrtProcessResultUnit(&tResult);
			return 6052;
		}
		xrtProcessResultUnit(&tResult);
	}

	#if !defined(XRT_NO_NETWORK)
	{
		str arrArgs[] = { (str)"__subprocess_helper", (str)"sleep_exit", (str)"120", (str)"9" };

		__xrtTestOSPrepareSelfConfig(xCore, &tConfig, arrArgs, 4u);
		pProcess = xrtProcessSpawn(&tConfig);
		if ( pProcess == NULL ) {
			return 6040;
		}
		pFuture = xrtProcessWaitFuture(pProcess);
		if ( pFuture == NULL ) {
			xrtProcessDestroy(pProcess);
			return 6041;
		}
		if ( xFutureWaitValueTimeout(pFuture, 3000) != pProcess ) {
			xFutureRelease(pFuture);
			xrtProcessDestroy(pProcess);
			return 6042;
		}
		if ( xrtProcessExitCode(pProcess) != 9 ) {
			xFutureRelease(pFuture);
			xrtProcessDestroy(pProcess);
			return 6043;
		}
		xFutureRelease(pFuture);
		xrtProcessDestroy(pProcess);
	}

	{
		xprocess* pFutureProcess = NULL;
		str arrArgs[] = { (str)"__subprocess_helper", (str)"sleep_exit", (str)"120", (str)"11" };

		__xrtTestOSPrepareSelfConfig(xCore, &tConfig, arrArgs, 4u);
		pProcess = xrtProcessSpawn(&tConfig);
		if ( pProcess == NULL ) {
			return 6044;
		}
		pFuture = xrtProcessWaitFuture(pProcess);
		if ( pFuture == NULL ) {
			xrtProcessDestroy(pProcess);
			return 6045;
		}
		if ( xFutureWaitValueTimeout(pFuture, 3000) != pProcess ) {
			xFutureRelease(pFuture);
			xrtProcessDestroy(pProcess);
			return 6046;
		}
		xrtProcessDestroy(pProcess);
		pFutureProcess = (xprocess*)xFutureValue(pFuture);
		if ( pFutureProcess != pProcess ) {
			xFutureRelease(pFuture);
			return 6047;
		}
		if ( xrtProcessExitCode(pFutureProcess) != 11 ) {
			xFutureRelease(pFuture);
			return 6048;
		}
		xFutureRelease(pFuture);
	}
	#endif

	return 0;
}
