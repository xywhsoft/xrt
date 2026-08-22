#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供 Epoch 前后的 Gregorian 往返能力。 */
int main(void)
{
	xtime iTime;
	xdatetime tDateTime;

	if ( !xrtDateTime(1969, 12, 31, 23, 59, 59, 999999, &iTime) ||
		 (iTime != -1) || !xrtTimeSplit(iTime, &tDateTime) ) {
		return 1;
	}
	return (tDateTime.Year == 1969) && (tDateTime.Month == 12) &&
		(tDateTime.Day == 31) ? 0 : 1;
}
