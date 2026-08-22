#define XRUNTIME_IMPLEMENTATION
#include "../../single/xruntime.h"



/* 验证单头文件中的类型列表稀疏键与追加路径。 */
int main(void)
{
	xtypedlist List;
	int64 iValue = 41;
	int64 iOutput = 0;
	int64 iKey;
	int iResult;

	if ( !xrtTypedListInit(&List, xrtTypeInt64()) ) {
		return 1;
	}
	iResult = (!xrtTypedListSet(&List, -9, &iValue) ||
		!xrtTypedListAppend(&List, &iValue, &iKey) ||
		(iKey != -8) ||
		!xrtTypedListTake(&List, -9, &iOutput) ||
		(iOutput != 41)) ? 2 : 0;
	xrtTypedListUnit(&List);
	return iResult;
}
