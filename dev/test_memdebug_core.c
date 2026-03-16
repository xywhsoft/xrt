#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xrt.h"
#include "test/test_memdebug_core.h"

int main(void)
{
	xrtInit();
	xrtMemDebugReset();
	xrtMemDebugEnable(TRUE);
	Test_MemDebugCore();
	xrtUnit();
	return 0;
}
