#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供 256 槽页的分配、复用和回收。 */
int main(void)
{
	xpoolpage tPage;
	ptr pObject;

	if ( !xrtPoolPageInitLayout(&tPage, 32, 16, 4) ) {
		return 1;
	}
	if ( tPage.Capacity != 4 ) {
		xrtPoolPageUnit(&tPage);
		return 2;
	}
	pObject = xrtPoolPageCalloc(&tPage);
	if ( (pObject == NULL) || !xrtPoolPageMark(&tPage, pObject) ) {
		xrtPoolPageUnit(&tPage);
		return 3;
	}
	if ( (xrtPoolPageSweep(&tPage) != 0) || !xrtPoolPageOwns(&tPage, pObject) ) {
		xrtPoolPageUnit(&tPage);
		return 4;
	}
	xrtPoolPageUnit(&tPage);
	return 0;
}
