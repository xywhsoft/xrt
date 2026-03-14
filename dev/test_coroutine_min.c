#include "../xrt.h"

static int g_done = 0;

static void __testCoMinMain(ptr pArg)
{
	(void)pArg;
	g_done = 1;
}

int main(void)
{
	xcosched* pSched = NULL;

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	if ( !xrtInit() ) return 10;

	pSched = xrtCoSchedCreate();
	if ( !pSched ) {
		xrtUnit();
		return 11;
	}

	if ( !xrtCoSchedSpawn(pSched, __testCoMinMain, NULL, 0u) ) {
		xrtCoSchedDestroy(pSched);
		xrtUnit();
		return 12;
	}

	xrtCoSchedRun(pSched);
	xrtCoSchedDestroy(pSched);
	xrtUnit();

	return g_done ? 0 : 13;
}
