#define XRUNTIME_IMPLEMENTATION
#include "../../single/xruntime.h"



/* 验证单头文件中的类型树排序、查询和删除入口。 */
int main(void)
{
	xtypedtree Tree;
	int32 iFirstKey = 2;
	int32 iSecondKey = 1;
	int64 iFirstValue = 20;
	int64 iSecondValue = 10;
	const void* pKey;
	int iResult = 0;

	if ( !xrtTypedTreeInit(&Tree, xrtTypeInt32(), xrtTypeInt64()) ) {
		return 1;
	}
	if ( !xrtTypedTreeSet(&Tree, &iFirstKey, &iFirstValue) ||
		 !xrtTypedTreeSet(&Tree, &iSecondKey, &iSecondValue) ||
		 (*(const int64*)xrtTypedTreeFirst(&Tree, &pKey) != 10) ||
		 (*(const int32*)pKey != 1) ||
		 !xrtTypedTreeRemove(&Tree, &iFirstKey) ) {
		iResult = 2;
	}
	xrtTypedTreeUnit(&Tree);
	return iResult;
}
