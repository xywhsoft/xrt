/*
 * 范例：error/tour —— 错误对象构建/定位/查找/线程绑定补集
 * ----------------------------------------------------------------
 * 演示 API：
 *   【构建】    xrtErrorBuild（完整描述）/ BuildAt（带源码位置）
 *   【定位族】  xrtErrorFile / Line / Column / Data / Find（链查找）/
 *              Ref（共享引用）
 *   【线程绑定】 xrtSetErrorTake（所有权转移）/
 *              SetErrorInfo / SetErrorKind / SetErrorHandler（全局通知）
 * 模块宏：XRT_MODULE_ERROR
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/error/tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   error: build kind/1 domain=demo op=load data=ctx
 *   error: build-at file/democ.c line=10 col=2
 *   error: find-in-chain code=42 / miss=(none)
 *   error: ref-release paired ok
 *   error: thread-bound take/info/kind handler=3
 *
 * BuildAt 创建带 File/Line/Column 的错误（三定位器核对）；
 *   Cause 链用 Find 沿链找到内层错误；全局 Handler 在每次设置错误时收到通知（本例 3 次）。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

static volatile int g_HandlerCalls = 0;

static void exampleHandler(const xerror* pError, ptr pUserData)
{
	(void)pError;
	(void)pUserData;
	g_HandlerCalls = g_HandlerCalls + 1;
}

int main(void)
{
	xerrordesc Desc;
	xerrorlocation Location;
	xerror* pError = NULL;
	xerror* pInner = NULL;
	xerror* pRef = NULL;
	xerror* pFound = NULL;
	const xerror* pMiss = NULL;
	xerror* pTaken = NULL;
	int iResult = 1;

	/* ---- Build：完整描述构建 ---- */
	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = XERR_ARGUMENT;
	Desc.Code = 7;
	Desc.Domain = "demo";
	Desc.Operation = "load";
	Desc.Message = "bad input";
	Desc.Data = "ctx";
	pError = xrtErrorBuild(&Desc);
	if ( (pError == NULL) ||
		(xrtErrorKind(pError) != XERR_ARGUMENT) ||
		(xrtErrorCode(pError) != 7) ||
		(strcmp(xrtErrorDomain(pError), "demo") != 0) ||
		(strcmp(xrtErrorOperation(pError), "load") != 0) ||
		(strcmp(xrtErrorMessage(pError), "bad input") != 0) ||
		(strcmp(xrtErrorData(pError), "ctx") != 0) ) {
		goto Cleanup;
	}
	printf("error: build kind/1 domain=%s op=%s data=%s\n",
		xrtErrorDomain(pError), xrtErrorOperation(pError),
		xrtErrorData(pError));

	/* ---- BuildAt：源码位置三定位器 ---- */
	Location.File = "democ.c";
	Location.Line = 10;
	Location.Column = 2;
	pInner = xrtErrorBuildAt(&Desc, &Location);
	if ( (pInner == NULL) ||
		(strcmp(xrtErrorFile(pInner), "democ.c") != 0) ||
		(xrtErrorLine(pInner) != 10) ||
		(xrtErrorColumn(pInner) != 2) ) {
		goto Cleanup;
	}
	printf("error: build-at file/%s line=%d col=%d\n",
		xrtErrorFile(pInner), xrtErrorLine(pInner),
		xrtErrorColumn(pInner));

	/* ---- Find：沿 Cause 链查找 + 未命中为 NULL ---- */
	{
		xerrordesc Inner;

		memset(&Inner, 0, sizeof(Inner));
		Inner.Kind = XERR_IO;
		Inner.Code = 42;
		Inner.Domain = "inner";
		Inner.Message = "root cause";
		pTaken = xrtErrorBuild(&Inner);
		if ( (pTaken == NULL) ) {
			goto Cleanup;
		}
		/* 包装：Cause 指向内层。 */
		Desc.Cause = pTaken;
		xrtErrorFree(pError);
		pError = xrtErrorBuild(&Desc);
		Desc.Cause = NULL;
		if ( (pError == NULL) ) {
			goto Cleanup;
		}
		pFound = (xerror*)xrtErrorFind(pError, "inner", 42);
		pMiss = xrtErrorFind(pError, "inner", 99);
		if ( (pFound == NULL) || (pMiss != NULL) ||
			(strcmp(xrtErrorDomain(pFound), "inner") != 0) ) {
			goto Cleanup;
		}
	}
	printf("error: find-in-chain code=42 / miss=(none)\n");

	/* ---- Ref：共享引用配对释放 ---- */
	pRef = xrtErrorRef(pError);
	if ( (pRef != pError) ) {
		goto Cleanup;
	}
	xrtErrorFree(pRef);  /* Ref 那份 */
	pRef = NULL;
	/* pError 此刻仍可访问（引用计数仍 > 0）。 */
	if ( xrtErrorCode(pError) != 7 ) {
		goto Cleanup;
	}
	printf("error: ref-release paired ok\n");

	/* ---- 线程绑定族：Take / Info / Kind / Handler ---- */
	g_HandlerCalls = 0;
	(void)xrtSetErrorHandler(exampleHandler, NULL);
	xrtSetErrorKind(XERR_TIMEOUT);  /* 触发全局 Handler 一次 */
	if ( (xrtGetError() == NULL) ||
		(xrtErrorKind(xrtGetError()) != XERR_TIMEOUT) ) {
		goto Cleanup;
	}
	xrtClearError();
	xrtSetErrorInfo(XERR_RANGE, "demo", 5, "out of range");
	if ( (xrtGetError() == NULL) ||
		(xrtErrorCode(xrtGetError()) != 5) ) {
		goto Cleanup;
	}
	xrtClearError();
	/* SetErrorTake：所有权转移到当前上下文再取出。 */
	xrtSetErrorTake(pTaken);
	pTaken = NULL;  /* 所有权已转移，不得继续使用 */
	{
		xerror* pThread = xrtTakeError();

		if ( (pThread == NULL) ||
			(xrtErrorCode(pThread) != 42) ) {
			xrtErrorFree(pThread);
			goto Cleanup;
		}
		xrtErrorFree(pThread);
	}
	(void)xrtSetErrorHandler(NULL, NULL);  /* 注销全局处理器 */
	if ( g_HandlerCalls < 1 ) {
		goto Cleanup;
	}
	printf("error: thread-bound take/info/kind handler=%d\n",
		g_HandlerCalls);
	iResult = 0;

Cleanup:
	xrtErrorFree(pTaken);
	xrtErrorFree(pRef);
	xrtErrorFree(pError);
	xrtErrorFree(pInner);
	return iResult;
}
