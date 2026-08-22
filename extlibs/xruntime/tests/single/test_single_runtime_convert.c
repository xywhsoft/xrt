#define XRUNTIME_IMPLEMENTATION
#include "../../single/xruntime.h"



/* 验证单头文件中的定宽转换和失败原子性。 */
int main(void)
{
	int8 iSource = -7;
	int64 iTarget = 0;
	uint64 iUnsigned = UINT64_MAX;
	int64 iUnchanged = 11;
	bool bSource = true;
	int32 iBool32 = 0;

	if (
		!xrtTypeConvert(xrtTypeInt8(), &iSource,
			xrtTypeInt64(), &iTarget, XTYPE_CONVERT_WIDEN) ||
		(iTarget != -7) ||
		!xrtTypeConvert(xrtTypeBool(), &bSource,
			xrtTypeBool32(), &iBool32, XTYPE_CONVERT_WIDEN) ||
		(iBool32 != 1) ||
		xrtTypeConvert(xrtTypeUInt64(), &iUnsigned,
			xrtTypeInt64(), &iUnchanged, XTYPE_CONVERT_EXPLICIT) ||
		(iUnchanged != 11)
	) {
		return 1;
	}
	return 0;
}
