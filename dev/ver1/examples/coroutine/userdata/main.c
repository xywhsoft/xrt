/*
 * XRT Example - Coroutine Userdata
 * XRT 范例 - 协程用户数据
 *
 * Description / 说明:
 *   EN: Demonstrates xrtCoSetUserData and xrtCoGetUserData.
 *   CN: 演示 xrtCoSetUserData 与 xrtCoGetUserData。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/coroutine_userdata.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/coroutine_userdata -lm -lpthread
 */
#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"


static void UserDataTask(ptr pParam)
{
	int* pValue = (int*)xrtCoGetUserData(xrtCoGetCurrent());

	(void)pParam;
	(*pValue) += 10;
	xrtCoYield();
	(*pValue) += 5;
}



int main()
{
	xcoro hCo = NULL;
	int iValue = 1;

	xrtInit();

	hCo = xrtCoCreate(UserDataTask, NULL, 0);
	xrtCoSetUserData(hCo, &iValue);

	xrtCoResume(hCo);
	printf("value_after_first_resume = %d\n", iValue);
	xrtCoResume(hCo);
	printf("value_after_second_resume = %d\n", iValue);

	xrtCoDestroy(hCo);
	xrtUnit();
	return 0;
}
