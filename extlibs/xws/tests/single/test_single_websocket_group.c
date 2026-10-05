#define XWS_IMPLEMENTATION
#include "../../include/xws/features.h"
#include "../../../xhttp/include/xhttp/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#define XHTTP_IMPLEMENTATION
#include "../../../../single/extlibs/xhttp.h"
#include "../../../../single/extlibs/xws.h"



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
