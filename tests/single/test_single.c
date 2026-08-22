#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>
#include <string.h>



static int __testErrorHandlerCount;
static bool __testErrorHandlerValid;



/* 在单头文件路径中制造一个嵌套错误。 */
static void testErrorHandler(const xerror* pError, ptr pUserData)
{
	xerror* pNestedError;

	(void)pUserData;
	__testErrorHandlerCount++;
	__testErrorHandlerValid = pError == xrtGetError();
	pNestedError = xrtErrorCreate(
		XERR_INTERNAL,
		"test.single.handler",
		1,
		"nested handler error"
	);
	if ( pNestedError == NULL ) {
		__testErrorHandlerValid = false;
		return;
	}
	xrtSetError(pNestedError);
	xrtErrorFree(pNestedError);
}



/* 验证发布单头文件能够独立提供声明和实现。 */
int main(void)
{
	xerror* pError;
	char* pMemory;
	volatile int32 iCount = 1;

	if ( strcmp(xrtVersion(), XRT_VERSION_TEXT) != 0 ) {
		return 1;
	}
	if ( (xrtRefRetain(&iCount) != 2) || (xrtRefRelease(&iCount) != 1) ) {
		return 5;
	}
	pMemory = (char*)xrtMalloc(16);
	if ( pMemory == NULL ) {
		return 2;
	}
	memcpy(pMemory, "single", 7);
	xrtFree(pMemory);

	pError = xrtErrorCreate(XERR_VALUE, "test.single", 1, "single-header error");
	if ( pError == NULL ) {
		return 3;
	}
	__testErrorHandlerValid = true;
	xrtSetErrorHandler(testErrorHandler, NULL);
	xrtSetError(pError);
	xrtErrorFree(pError);
	if ( strcmp(xrtErrorMessage(xrtGetError()), "single-header error") != 0 ) {
		return 4;
	}
	if ( (__testErrorHandlerCount != 1) || !__testErrorHandlerValid ) {
		return 6;
	}
	xrtSetErrorHandler(NULL, NULL);
	xrtClearError();
	printf("[PASS] single-header\n");
	return 0;
}
