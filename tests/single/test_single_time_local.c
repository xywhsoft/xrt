#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立接通系统本地时区和显式偏移往返。 */
int main(void)
{
	xtime iNow = xrtNow();
	xtime iRoundtrip;
	xdatetime tLocal;

	return xrtTimeLocal(iNow, &tLocal) &&
		xrtTimeMake(&tLocal, &iRoundtrip) && (iRoundtrip == iNow) ? 0 : 1;
}
