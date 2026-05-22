#include <stdio.h>

#include "xrt.h"
#include "test/test_coroutine.h"
#include "test/test_coroutine_min.h"

int main(void)
{
	xrtGlobalData* pCore = NULL;
	int iRet = 0;

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	pCore = xrtInit();
	if ( pCore == NULL ) {
		fprintf(stderr, "xrtInit failed\n");
		return 100;
	}

	iRet = Test_CoroutineMin();
	printf("[RESULT] coroutine_min=%s (%d)\n", (iRet == 0) ? "PASS" : "FAIL", iRet);

	Test_Coroutine(pCore);

	xrtUnit();
	return iRet;
}
