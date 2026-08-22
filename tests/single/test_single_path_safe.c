#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件安全路径裁剪必须拒绝目录穿越和设备名。 */
int main(void)
{
	xpathsafesegment Segment;

	xrtPathSafeSegmentInit(&Segment);
	if ( !xrtPathSafeSegmentFeed(&Segment, (uint8)'a') ||
		!xrtPathSafeSegmentFinish(&Segment) ) {
		return 1;
	}
	return xrtPathIsSafeEntry(XRT_STR_LITERAL("assets/app.png"), false) &&
		!xrtPathIsSafeEntry(XRT_STR_LITERAL("../app.png"), false) &&
		!xrtPathIsSafeEntry(XRT_STR_LITERAL("NUL.txt"), false) ? 0 : 1;
}
