#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件公开空连接组、封闭状态和稳定空快照。 */
int main(void)
{
	xwsgroup* pGroup = xrtWsGroupCreate(4u);
	xwsgroupsnapshot* pSnapshot;

	if ( (pGroup == NULL) ||
		(xrtWsGroupCount(pGroup) != 0) ||
		(xrtWsGroupLimit(pGroup) != 4u) ) {
		return 1;
	}
	pSnapshot = xrtWsGroupSnapshotCreate(pGroup);
	if ( (pSnapshot == NULL) ||
		(xrtWsGroupSnapshotCount(pSnapshot) != 0) ||
		!xrtWsGroupSeal(pGroup) ||
		!xrtWsGroupSealed(pGroup) ) {
		xrtWsGroupSnapshotDestroy(pSnapshot);
		xrtWsGroupDestroy(pGroup);
		return 1;
	}
	xrtWsGroupSnapshotDestroy(pSnapshot);
	xrtWsGroupDestroy(pGroup);
	return 0;
}
