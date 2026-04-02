/*
 * XRT Example - Error Handling
 * XRT 范例 - 错误处理
 *
 * Description / 说明:
 *   EN: Demonstrates xrtSetError, xrtClearError, xrtGetError and OnError.
 *   CN: 演示 xrtSetError、xrtClearError、xrtGetError 和 OnError 回调。
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

int giErrorCount = 0;


void procLogError(str sError)
{
	giErrorCount++;
	printf("[OnError %d] %s\n", giErrorCount, sError);
	printf("[错误回调 %d] %s\n", giErrorCount, sError);
}


void procCriticalError(str sError)
{
	printf("!!! CRITICAL: %s !!!\n", sError);
	printf("!!! 严重错误: %s !!!\n", sError);
}


void procPrintErrorState(str sLabel)
{
	str sError = xrtGetError();

	printf("%s: %s\n", sLabel, (sError && sError[0]) ? sError : "(empty)");
}


int procSafeDivide(int iA, int iB, int* piResult)
{
	if ( iB == 0 ) {
		xrtSetError("division by zero", FALSE);
		return FALSE;
	}

	*piResult = iA / iB;
	return TRUE;
}


void procCheckBounds(int iIndex, int iMax)
{
	if ( (iIndex < 0) || (iIndex >= iMax) ) {
		str sMsg = xrtFormat("index %d out of bounds [0, %d)", iIndex, iMax);
		xrtSetError(sMsg, TRUE);
		return;
	}

	printf("index %d is valid\n", iIndex);
	printf("索引 %d 有效\n", iIndex);
}


void procTestBasicError(void)
{
	printf("=== Basic Error API ===\n");
	printf("=== 基础错误接口 ===\n");

	xCore.OnError = procLogError;
	giErrorCount = 0;

	xrtClearError();
	procPrintErrorState("after clear / 清除后");

	xrtSetError("static error text", FALSE);
	procPrintErrorState("after static set / 静态错误");

	xrtSetError(xrtFormat("dynamic error #%d", 2), TRUE);
	procPrintErrorState("after dynamic set / 动态错误");

	xrtClearError();
	procPrintErrorState("after clear again / 再次清除");
	printf("\n");
}


void procTestOperationError(void)
{
	int iResult = 0;

	printf("=== Operation Error ===\n");
	printf("=== 操作错误 ===\n");

	xrtClearError();

	if ( procSafeDivide(10, 2, &iResult) ) {
		printf("10 / 2 = %d\n", iResult);
		printf("10 / 2 = %d\n", iResult);
	}

	if ( !procSafeDivide(10, 0, &iResult) ) {
		procPrintErrorState("divide failed / 除法失败");
	}

	xrtClearError();
	printf("\n");
}


void procTestDynamicError(void)
{
	printf("=== Dynamic Error Text ===\n");
	printf("=== 动态错误文本 ===\n");

	xrtClearError();
	procCheckBounds(5, 10);
	procPrintErrorState("valid access / 正常访问");

	procCheckBounds(15, 10);
	procPrintErrorState("invalid access / 越界访问");

	xrtClearError();
	procPrintErrorState("after release / 释放后");
	printf("\n");
}


void procTestHandlerSwitch(void)
{
	printf("=== Handler Switch ===\n");
	printf("=== 处理器切换 ===\n");

	xCore.OnError = NULL;
	xrtSetError("silent error", FALSE);
	procPrintErrorState("no handler / 无处理器");

	xCore.OnError = procLogError;
	xrtSetError("logged error", FALSE);

	xCore.OnError = procCriticalError;
	xrtSetError("critical error", FALSE);

	xrtClearError();
	xCore.OnError = NULL;
	printf("logged count = %d\n", giErrorCount);
	printf("记录次数 = %d\n\n", giErrorCount);
}


int main(void)
{
	xrtInit();

	printf("XRT Base - Error Handling Demo\n");
	printf("XRT 基础 - 错误处理演示\n");
	printf("===============================\n\n");

	procTestBasicError();
	procTestOperationError();
	procTestDynamicError();
	procTestHandlerSwitch();

	xrtUnit();
	return 0;
}
