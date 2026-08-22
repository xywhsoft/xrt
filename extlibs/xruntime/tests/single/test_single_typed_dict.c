#define XRUNTIME_IMPLEMENTATION
#include "../../single/xruntime.h"



/* 验证单头文件中的类型字典默认构造和事务合并路径。 */
int main(void)
{
	xtypeddict Target;
	xtypeddict Source;
	int64 iOne = 1;
	int64 iTwo = 2;
	int64 iOutput = 0;
	int64* pDefault;
	int iResult;

	if ( !xrtTypedDictInit(&Target, xrtTypeInt64()) ||
		 !xrtTypedDictInit(&Source, xrtTypeInt64()) ) {
		return 1;
	}
	pDefault = (int64*)xrtTypedDictGetOrAdd(
		&Target, XRT_STR_LITERAL("default"), NULL
	);
	if ( (pDefault == NULL) || (*pDefault != 0) ||
		 !xrtTypedDictSet(&Target, XRT_STR_LITERAL("one"), &iOne) ||
		 !xrtTypedDictSet(&Source, XRT_STR_LITERAL("two"), &iTwo) ||
		 !xrtTypedDictMerge(&Target, &Source, true) ) {
		xrtTypedDictUnit(&Source);
		xrtTypedDictUnit(&Target);
		return 2;
	}
	iResult = (!xrtTypedDictTake(
		&Target, XRT_STR_LITERAL("two"), &iOutput
	) || (iOutput != 2) ||
		(xrtTypedDictCount(&Target) != 2u)) ? 3 : 0;
	xrtTypedDictUnit(&Source);
	xrtTypedDictUnit(&Target);
	return iResult;
}
