#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件拥有型树对象。 */
typedef struct singleitem {
	int Key;
	int Value;
} singleitem;



/* 比较单头文件测试键与对象键。 */
static int singleCompare(const void* pKey, const void* pItem, ptr pUserData)
{
	int iKey = *(const int*)pKey;
	int iItemKey = ((const singleitem*)pItem)->Key;

	(void)pUserData;
	return (iKey > iItemKey) - (iKey < iItemKey);
}



/* 单头文件必须独立提供拥有型树完整生命周期。 */
int main(void)
{
	xavltree tTree;
	xavltreeiter tIterator;
	singleitem tItem = { 5, 50 };
	singleitem* pStored;

	if ( !xrtAVLTreeInit(&tTree, sizeof(singleitem), singleCompare, NULL) ) {
		return 1;
	}
	pStored = (singleitem*)xrtAVLTreeAdd(&tTree, &tItem.Key, &tItem, NULL);
	if ( (pStored == NULL) || (pStored->Value != 50) ) {
		xrtAVLTreeUnit(&tTree);
		return 2;
	}
	if ( xrtAVLTreeFind(&tTree, &tItem.Key) != pStored ) {
		xrtAVLTreeUnit(&tTree);
		return 3;
	}
	if (
		!xrtAVLTreeIterFrom(&tTree, &tItem.Key, &tIterator) ||
		(xrtAVLTreeIterNext(&tIterator) != pStored)
	) {
		xrtAVLTreeUnit(&tTree);
		return 4;
	}
	xrtAVLTreeUnit(&tTree);
	return 0;
}
