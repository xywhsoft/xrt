#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xrt.h"
#include "test/test_temparena_core.h"

int main(void)
{
	xrtInit();
	#ifdef XRT_MEM_DEBUG
		xrtMemDebugReset();
		xrtMemDebugEnable(TRUE);
	#endif
	Test_TempArenaCore();
	xrtUnit();
	return 0;
}
