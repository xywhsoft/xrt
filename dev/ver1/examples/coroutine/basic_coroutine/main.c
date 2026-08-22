/*
 * XRT Example - Basic Coroutine
 * XRT 范例 - 基础协程
 *
 * Description / 说明:
 *   EN: Demonstrates create, resume, yield and destroy for a single coroutine.
 *   CN: 演示单个协程的创建、恢复、挂起与销毁。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/coroutine_basic_coroutine.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/coroutine_basic_coroutine -lm -lpthread
 */
#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"


static void DemoCoroutine(ptr pParam)
{
	int* pValue = (int*)pParam;

	(*pValue)++;
	xrtCoYield();
	(*pValue)++;
}



int main()
{
	xcoro hCo = NULL;
	int iValue = 0;

	xrtInit();

	hCo = xrtCoCreate(DemoCoroutine, &iValue, 0);
	printf("state_before = %d\n", xrtCoGetState(hCo));
	xrtCoResume(hCo);
	printf("state_after_first_resume = %d, value = %d\n", xrtCoGetState(hCo), iValue);
	xrtCoResume(hCo);
	printf("state_after_second_resume = %d, value = %d\n", xrtCoGetState(hCo), iValue);

	xrtCoDestroy(hCo);
	xrtUnit();
	return 0;
}
