#include "../xrt.h"
#include "../test/test_thread_core.h"

int main(void)
{
	xrtGlobalData* pCore;
	int iRet;

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	pCore = xrtInit();
	if ( !pCore ) {
		return 1;
	}

	iRet = Test_ThreadCore();

	xrtUnit();
	return iRet;
}
