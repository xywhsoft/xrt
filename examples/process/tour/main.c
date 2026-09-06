/*
 * 范例：process/tour —— 进程生命周期/等待/停止/Run 一把抓补集
 * ----------------------------------------------------------------
 * 演示 API：
 *   【Run 族】  xrtProcessRunOptionsInit / Run（输入+限长捕获）/
 *              Capture（默认策略直接执行）
 *   【生命周期】 xrtProcessRef / Id / Native / State / Status /
 *              Error / StreamNative
 *   【等待族】  xrtProcessWaitFor（超时）/ WaitUntil（成功）/
 *              WaitUntilCancel（已触发令牌）
 *   【停止族】  xrtProcessInterrupt / Terminate / Kill / KillTree
 *   【Pipeline】 xrtProcessPipelineOptionsInit
 * 模块宏：XRT_MODULE_PROCESS
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/process/tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   process: run echo stdout=6 exit=0
 *   process: capture exit=0 stderr=0
 *   process: lifecycle id/native/state/wait ok
 *   process: stop terminate ok kill ok kill-tree ok
 *   process: pipeline-options ok
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

int main(void)
{
	xprocessconfig Config;
	xprocessrunoptions RunOptions;
	xprocesspipelineoptions PipeOptions;
	xprocessresult Result;
	xprocess* pProcess = NULL;
	xprocess* pRef = NULL;
	xcancel* pCancel = NULL;
	cstr arrArgs[2];
	xprocessstatus Status;
	int iResult = 1;

	/* ---- Run：输入回显 + 限长捕获 ---- */
	if ( !xrtProcessShellConfigInit(&Config,
			"cmd /c echo ping") ) {
		goto Cleanup;
	}
	if ( !xrtProcessRunOptionsInit(&RunOptions) ||
		!xrtProcessRun(&Config, &RunOptions, &Result) ||
		!xrtProcessResultSuccess(&Result) ||
		(Result.StdoutSize < 4u) ||  /* "ping\r\n" 至少 4 */
		(Result.Stdout == NULL) ) {
		goto Cleanup;
	}
	printf("process: run echo stdout=%zu exit=%d\n",
		Result.StdoutSize,
		(int)Result.Status.Code);
	xrtProcessResultUnit(&Result);

	/* ---- Capture：默认策略直接执行 ---- */
	arrArgs[0] = "/c echo ok";
	if ( !xrtProcessCapture("cmd", arrArgs, 1u, &Result) ||
		!xrtProcessResultSuccess(&Result) ||
		(Result.StderrSize != 0u) ) {
		goto Cleanup;
	}
	printf("process: capture exit=%d stderr=%zu\n",
		(int)Result.Status.Code, Result.StderrSize);
	xrtProcessResultUnit(&Result);

	/* ---- 生命周期：长睡进程 + Id/Native/State/Status/Error ---- */
		if ( !xrtProcessShellConfigInit(&Config,
			"cmd /c ping -n 30 127.0.0.1 > nul") ) {
		goto Cleanup;
	}
	pProcess = xrtProcessSpawn(&Config);
	if ( (pProcess == NULL) ||
		(xrtProcessId(pProcess) == 0u) ||
		(xrtProcessNative(pProcess) == 0) ||
		(xrtProcessState(pProcess) != XPROCESS_RUNNING) ||
		(xrtProcessStreamNative(pProcess,
			XPROCESS_STDOUT) == 0) ) {
		goto Cleanup;
	}
	pRef = xrtProcessRef(pProcess);
	if ( (pRef != pProcess) ) {
		goto Cleanup;
	}
	/* WaitFor 短窗：长睡进程必然超时。 */
	if ( xrtProcessWaitFor(pProcess, 100000u) != XWAIT_TIMEOUT ) {
		goto Cleanup;
	}
	/* WaitUntilCancel：已触发令牌立即取消。 */
	pCancel = xrtCancelCreate();
	if ( (pCancel == NULL) ||
		!xrtCancelRequest(pCancel) ||
		(xrtProcessWaitUntilCancel(pProcess,
			xrtDeadlineAfter(UINT64_C(3000000)),
			pCancel) != XWAIT_CANCELLED) ) {
		goto Cleanup;
	}
	/* 停止族：Interrupt 是协作请求（Windows 重定向子进程可能
	 * 不响应控制台事件），仅断言受理；实际终止用 Terminate。 */
	if ( !xrtProcessInterrupt(pProcess) ) {
		goto Cleanup;
	}
	if ( !xrtProcessTerminate(pProcess) ||
		!xrtProcessKill(pProcess) ||
		(xrtProcessWaitUntil(pProcess,
			xrtDeadlineAfter(UINT64_C(2000000))) !=
			XWAIT_OK) ) {
		goto Cleanup;
	}
	if ( (xrtProcessState(pProcess) != XPROCESS_EXITED) ||
		!xrtProcessStatus(pProcess, &Status) ||
		(Status.Kind != XPROCESS_EXIT_CODE) ||
		(xrtProcessError(pProcess) != NULL) ) {
		goto Cleanup;
	}
	printf("process: lifecycle id/native/state/wait ok\n");

	/* ---- 停止族：Terminate 与 Kill 用独立短睡进程 ---- */
	{
		xprocess* pVictim;

		if ( !xrtProcessShellConfigInit(&Config,
			"cmd /c ping -n 30 127.0.0.1 > nul") ) {
			goto Cleanup;
		}
		pVictim = xrtProcessSpawn(&Config);
		if ( (pVictim != NULL) &&
			xrtProcessTerminate(pVictim) &&
			(xrtProcessWaitFor(pVictim,
				UINT64_C(3000000)) == XWAIT_OK) ) {
			printf("process: stop terminate ok");
		}
		else {
			goto Cleanup;
		}
		xrtProcessDestroy(pVictim);

		if ( !xrtProcessShellConfigInit(&Config,
			"cmd /c ping -n 30 127.0.0.1 > nul") ) {
			goto Cleanup;
		}
		pVictim = xrtProcessSpawn(&Config);
		if ( (pVictim != NULL) &&
			xrtProcessKill(pVictim) &&
			(xrtProcessWaitFor(pVictim,
				UINT64_C(3000000)) == XWAIT_OK) ) {
			printf(" kill ok");
		}
		else {
			goto Cleanup;
		}
		xrtProcessDestroy(pVictim);

		if ( !xrtProcessShellConfigInit(&Config,
			"cmd /c ping -n 30 127.0.0.1 > nul") ) {
			goto Cleanup;
		}
		pVictim = xrtProcessSpawn(&Config);
		if ( (pVictim != NULL) &&
			xrtProcessKillTree(pVictim) &&
			(xrtProcessWaitFor(pVictim,
				UINT64_C(3000000)) == XWAIT_OK) ) {
			printf(" kill-tree ok\n");
		}
		else {
			goto Cleanup;
		}
		xrtProcessDestroy(pVictim);
	}

	/* ---- Resize：终端进程窗口调整（无终端时受理失败不视为
	 * 范例失败——能力探测与 file/fifo 范例一致）。 */
	{
		if ( !xrtProcessShellConfigInit(&Config,
				"cmd /c ping -n 30 127.0.0.1 > nul") ) {
			goto Cleanup;
		}
		/* Config.Terminal 需要终端标志；借用 Terminal 范例配置。 */
		if ( xrtProcessTerminalSupported() ) {
			xprocess* pTerm;

			Config.Terminal = true;
			pTerm = xrtProcessSpawn(&Config);
			if ( pTerm != NULL ) {
				(void)xrtProcessResize(pTerm, 120u, 30u);
				(void)xrtProcessKill(pTerm);
				(void)xrtProcessWaitFor(pTerm,
					UINT64_C(2000000));
				xrtProcessDestroy(pTerm);
			}
		}
	}

	/* ---- PipelineOptions：配置初始化入口 ---- */
	if ( !xrtProcessPipelineOptionsInit(&PipeOptions) ) {
		goto Cleanup;
	}
	printf("process: pipeline-options ok\n");
	iResult = 0;

Cleanup:
	xrtCancelDestroy(pCancel);
	xrtProcessDestroy(pRef);
	xrtProcessDestroy(pProcess);
	return iResult;
}
