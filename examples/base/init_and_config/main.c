/*
 * XRT Example - Initialization and Configuration
 * XRT 范例 - 初始化与配置
 *
 * Description / 说明:
 *   EN: Demonstrates xrtInit/xrtUnit lifecycle and public global configuration.
 *   CN: 演示 xrtInit/xrtUnit 生命周期以及公开的全局配置项。
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"


void procMyErrorHandler(str sError)
{
	printf("[hook] %s\n", sError);
	printf("[回调] %s\n", sError);
}


void procTestBasicInit(void)
{
	printf("=== Basic Init ===\n");
	printf("=== 基本初始化 ===\n");
	printf("xCore.bInit = %d\n", xCore.bInit);
	printf("xCore.bInit = %d\n\n", xCore.bInit);
}


void procTestGlobalInfo(void)
{
	printf("=== Global Info ===\n");
	printf("=== 全局信息 ===\n");
	printf("AppFile = %s\n", xCore.AppFile);
	printf("AppPath = %s\n", xCore.AppPath);
	printf("sNull  = %p\n", (void*)xCore.sNull);
	printf("OnError = %p\n\n", (void*)xCore.OnError);
}


void procTestRefCount(void)
{
	xrtGlobalData* pCore1 = NULL;
	xrtGlobalData* pCore2 = NULL;
	xrtGlobalData* pCore3 = NULL;

	printf("=== Ref Count ===\n");
	printf("=== 引用计数 ===\n");

	pCore1 = xrtInit();
	pCore2 = xrtInit();
	pCore3 = xrtInit();

	printf("call #1 = %p\n", (void*)pCore1);
	printf("call #2 = %p\n", (void*)pCore2);
	printf("call #3 = %p\n", (void*)pCore3);
	printf("same pointer = %s\n\n", ((pCore1 == pCore2) && (pCore2 == pCore3)) ? "TRUE" : "FALSE");

	xrtUnit();
	xrtUnit();
	xrtUnit();
}


void procTestErrorHook(void)
{
	str sError = NULL;

	printf("=== Error Hook ===\n");
	printf("=== 错误回调 ===\n");

	xCore.OnError = procMyErrorHandler;
	xrtClearError();
	xrtSetError("test error message", FALSE);

	sError = xrtGetError();
	printf("xrtGetError() = %s\n", (sError && sError[0]) ? sError : "(empty)");
	printf("xrtGetError() = %s\n\n", (sError && sError[0]) ? sError : "(empty)");

	xrtClearError();
	xCore.OnError = NULL;
}


void procTestApproxConfig(void)
{
	printf("=== Approx Config ===\n");
	printf("=== 近似比较配置 ===\n");
	printf("iApproxIntMode = %d\n", xCore.iApproxIntMode);
	printf("fApproxIntTol  = %.6f\n", xCore.fApproxIntTol);
	printf("iApproxNumMode = %d\n", xCore.iApproxNumMode);
	printf("fApproxNumTol  = %.6f\n", xCore.fApproxNumTol);
	printf("iApproxTimeTol = %lld\n", (long long)xCore.iApproxTimeTol);
	printf("iApproxStrMode = %d\n", xCore.iApproxStrMode);
	printf("fApproxStrTol  = %.6f\n", xCore.fApproxStrTol);
	printf("bApproxStrCase = %d\n\n", xCore.bApproxStrCase);
}


void procTestMemoryHooks(void)
{
	printf("=== Memory Hooks ===\n");
	printf("=== 内存函数指针 ===\n");
	printf("malloc  = %p\n", (void*)xCore.malloc);
	printf("calloc  = %p\n", (void*)xCore.calloc);
	printf("realloc = %p\n", (void*)xCore.realloc);
	printf("free    = %p\n\n", (void*)xCore.free);
}


int main(void)
{
	xrtInit();

	printf("XRT Base - Initialization and Configuration Demo\n");
	printf("XRT 基础 - 初始化与配置演示\n");
	printf("=================================================\n\n");

	procTestBasicInit();
	procTestGlobalInfo();
	procTestRefCount();
	procTestErrorHook();
	procTestApproxConfig();
	procTestMemoryHooks();

	xrtUnit();
	return 0;
}
