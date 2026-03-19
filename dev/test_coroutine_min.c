#include "../xrt.h"
#include "../test/test_coroutine_min.h"

int main(void)
{
	xrtGlobalData* pCore;
	int iRet;

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	pCore = xrtInit();
	if ( !pCore ) {
		return 10;
	}

	iRet = Test_CoroutineMin();

	xrtUnit();
	return iRet;
}
