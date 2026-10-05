#define XRUNTIME_IMPLEMENTATION
#include "../../include/xruntime/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xruntime.h"



/* 验证单头文件中的 Value 类型描述、COW 复制和深克隆路径。 */
int main(void)
{
	const xrttype* pType = xrtTypeValue();
	xvalue* pSource = xrtValueArray();
	xvalue* pTarget = xrtValueNull();
	xvalue* pClone = xrtValueNull();
	bool bReady = (pSource != NULL) &&
		xrtValueArrayAppendNew(pSource, xrtValueInt(7)) &&
		xrtTypeCopyValue(pType, &pTarget, &pSource) &&
		(pTarget != pSource) && xrtValueEqual(pTarget, pSource) &&
		xrtTypeCloneValue(pType, &pClone, &pSource) &&
		(pClone != pSource) && xrtValueEqual(pClone, pSource);

	xrtTypeDropValue(pType, &pClone);
	xrtTypeDropValue(pType, &pTarget);
	xrtValueRelease(pSource);
	return bReady ? 0 : 1;
}
